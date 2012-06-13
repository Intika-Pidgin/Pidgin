/**
 * @file gtklog.c GTK+ Log viewer
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "pidgin.h"

#include "account.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "request.h"
#include "util.h"

#include "pidginstock.h"
#include "gtkblist.h"
#include "gtkimhtml.h"
#include "gtklog.h"
#include "gtkutils.h"

static GHashTable *log_viewers = NULL;
static void populate_log_tree(PidginLogViewer *lv);
static PidginLogViewer *syslog_viewer = NULL;

struct log_viewer_hash_t {
	PurpleLogType type;
	char *buddyname;
	PurpleAccount *account;
	PurpleContact *contact;
};

static guint log_viewer_hash(gconstpointer data)
{
	const struct log_viewer_hash_t *viewer = data;

	if (viewer->contact != NULL)
		return g_direct_hash(viewer->contact);

	return g_str_hash(viewer->buddyname) +
		g_str_hash(purple_account_get_username(viewer->account));
}

static gboolean log_viewer_equal(gconstpointer y, gconstpointer z)
{
	const struct log_viewer_hash_t *a, *b;
	int ret;
	char *normal;

	a = y;
	b = z;

	if (a->contact != NULL) {
		if (b->contact != NULL)
			return (a->contact == b->contact);
		else
			return FALSE;
	} else {
		if (b->contact != NULL)
			return FALSE;
	}

	normal = g_strdup(purple_normalize(a->account, a->buddyname));
	ret = (a->account == b->account) &&
		!strcmp(normal, purple_normalize(b->account, b->buddyname));
	g_free(normal);

	return ret;
}

static void select_first_log(PidginLogViewer *lv)
{
	GtkTreeModel *model;
	GtkTreeIter iter, it;
	GtkTreePath *path;

	model = GTK_TREE_MODEL(lv->treestore);

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	path = gtk_tree_model_get_path(model, &iter);
	if (gtk_tree_model_iter_children(model, &it, &iter))
	{
		gtk_tree_view_expand_row(GTK_TREE_VIEW(lv->treeview), path, TRUE);
		path = gtk_tree_model_get_path(model, &it);
	}

	gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(lv->treeview)), path);

	gtk_tree_path_free(path);
}

static const char *log_get_date(PurpleLog *log)
{
	if (log->tm)
		return purple_date_format_full(log->tm);
	else
		return purple_date_format_full(localtime(&log->time));
}

static void search_cb(GtkWidget *button, PidginLogViewer *lv)
{
	const char *search_term = gtk_entry_get_text(GTK_ENTRY(lv->entry));
	GList *logs;

	if (!(*search_term)) {
		/* reset the tree */
		gtk_tree_store_clear(lv->treestore);
		populate_log_tree(lv);
		g_free(lv->search);
		lv->search = NULL;
		gtk_imhtml_search_clear(GTK_IMHTML(lv->imhtml));
		select_first_log(lv);
		return;
	}

	if (lv->search != NULL && !strcmp(lv->search, search_term))
	{
		/* Searching for the same term acts as "Find Next" */
		gtk_imhtml_search_find(GTK_IMHTML(lv->imhtml), lv->search);
		return;
	}

	pidgin_set_cursor(lv->window, GDK_WATCH);

	g_free(lv->search);
	lv->search = g_strdup(search_term);

	gtk_tree_store_clear(lv->treestore);
	gtk_imhtml_clear(GTK_IMHTML(lv->imhtml));

	for (logs = lv->logs; logs != NULL; logs = logs->next) {
		char *read = purple_log_read((PurpleLog*)logs->data, NULL);
		if (read && *read && purple_strcasestr(read, search_term)) {
			GtkTreeIter iter;
			PurpleLog *log = logs->data;

			gtk_tree_store_append (lv->treestore, &iter, NULL);
			gtk_tree_store_set(lv->treestore, &iter,
					   0, log_get_date(log),
					   1, log, -1);
		}
		g_free(read);
	}

	select_first_log(lv);
	pidgin_clear_cursor(lv->window);
}

static void destroy_cb(GtkWidget *w, gint resp, struct log_viewer_hash_t *ht) {
	PidginLogViewer *lv = syslog_viewer;

#ifdef _WIN32
	if (resp == GTK_RESPONSE_HELP) {
		GtkTreeSelection *sel;
		GtkTreeIter iter;
		GtkTreeModel *model;
		PurpleLog *log = NULL;
		char *logdir;

		if (ht != NULL)
			lv = g_hash_table_lookup(log_viewers, ht);
		model = GTK_TREE_MODEL(lv->treestore);

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(lv->treeview));
		if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
			GValue val;

			val.g_type = 0;
			gtk_tree_model_get_value (model, &iter, 1, &val);
			log = g_value_get_pointer(&val);
			g_value_unset(&val);
		}


		if (log == NULL)
			logdir = g_build_filename(purple_user_dir(), "logs", NULL);
		else
			logdir = purple_log_get_log_dir(log->type, log->name, log->account);

		winpidgin_shell_execute(logdir, "explore", NULL);
		g_free(logdir);
		return;
	}
#endif

	if (ht != NULL) {
		lv = g_hash_table_lookup(log_viewers, ht);
		g_hash_table_remove(log_viewers, ht);

		g_free(ht->buddyname);
		g_free(ht);
	} else
		syslog_viewer = NULL;

	purple_request_close_with_handle(lv);

	g_list_foreach(lv->logs, (GFunc)purple_log_free, NULL);
	g_list_free(lv->logs);

	g_free(lv->search);
	g_free(lv);

	gtk_widget_destroy(w);
}

static void log_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, PidginLogViewer *viewer) {
	if (gtk_tree_view_row_expanded(tv, path))
		gtk_tree_view_collapse_row(tv, path);
	else
		gtk_tree_view_expand_row(tv, path, FALSE);
}

static void delete_log_cleanup_cb(gpointer *data)
{
	g_free(data[1]); /* iter */
	g_free(data);
}

static void delete_log_cb(gpointer *data)
{
	if (!purple_log_delete((PurpleLog *)data[2]))
	{
		purple_notify_error(NULL, NULL, _("Log Deletion Failed"),
		                  _("Check permissions and try again."));
	}
	else
	{
		GtkTreeStore *treestore = data[0];
		GtkTreeIter *iter = (GtkTreeIter *)data[1];
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), iter);
		gboolean first = !gtk_tree_path_prev(path);

		if (!gtk_tree_store_remove(treestore, iter) && first)
		{
			/* iter was the last child at its level */

			if (gtk_tree_path_up(path))
			{
				gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), iter, path);
				gtk_tree_store_remove(treestore, iter);
			}
		}

		gtk_tree_path_free(path);
	}

	delete_log_cleanup_cb(data);
}

static void log_delete_log_cb(GtkWidget *menuitem, gpointer *data)
{
	PidginLogViewer *lv = data[0];
	PurpleLog *log = data[1];
	const char *time = log_get_date(log);
	const char *name;
	char *tmp;
	gpointer *data2;

	if (log->type == PURPLE_LOG_IM)
	{
		PurpleBuddy *buddy = purple_find_buddy(log->account, log->name);
		if (buddy != NULL)
			name = purple_buddy_get_contact_alias(buddy);
		else
			name = log->name;

		tmp = g_strdup_printf(_("Are you sure you want to permanently delete the log of the "
		                        "conversation with %s which started at %s?"), name, time);
	}
	else if (log->type == PURPLE_LOG_CHAT)
	{
		PurpleChat *chat = purple_blist_find_chat(log->account, log->name);
		if (chat != NULL)
			name = purple_chat_get_name(chat);
		else
			name = log->name;

		tmp = g_strdup_printf(_("Are you sure you want to permanently delete the log of the "
		                        "conversation in %s which started at %s?"), name, time);
	}
	else if (log->type == PURPLE_LOG_SYSTEM)
	{
		tmp = g_strdup_printf(_("Are you sure you want to permanently delete the system log "
		                        "which started at %s?"), time);
	}
	else
		g_return_if_reached();

	/* The only way to free data in all cases is to tie it to the menuitem with
	 * g_object_set_data_full().  But, since we need to get some data down to
	 * delete_log_cb() to delete the log from the log viewer after the file is
	 * deleted, we have to allocate a new data array and make sure it gets freed
	 * either way. */
	data2 = g_new(gpointer, 3);
	data2[0] = lv->treestore;
	data2[1] = data[3]; /* iter */
	data2[2] = log;
	purple_request_action(lv, NULL, _("Delete Log?"), tmp, 0,
						NULL, NULL, NULL,
						data2, 2,
						_("Delete"), delete_log_cb,
						_("Cancel"), delete_log_cleanup_cb);
	g_free(tmp);
}

static void log_show_popup_menu(GtkWidget *treeview, GdkEventButton *event, gpointer *data)
{
	GtkWidget *menu = gtk_menu_new();
	GtkWidget *menuitem = gtk_menu_item_new_with_label(_("Delete Log..."));

	if (!purple_log_is_deletable((PurpleLog *)data[1]))
		gtk_widget_set_sensitive(menuitem, FALSE);

	g_signal_connect(menuitem, "activate", G_CALLBACK(log_delete_log_cb), data);
	g_object_set_data_full(G_OBJECT(menuitem), "log-viewer-data", data, g_free);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)data[2], NULL,
	               (event != NULL) ? event->button : 0,
	               gdk_event_get_time((GdkEvent *)event));
}

static gboolean log_button_press_cb(GtkWidget *treeview, GdkEventButton *event, PidginLogViewer *lv)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
	{
		GtkTreePath *path;
		GtkTreeIter *iter;
		GValue val;
		PurpleLog *log;
		gpointer *data;

		if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), event->x, event->y, &path, NULL, NULL, NULL))
			return FALSE;
		iter = g_new(GtkTreeIter, 1);
		gtk_tree_model_get_iter(GTK_TREE_MODEL(lv->treestore), iter, path);
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(lv->treestore), iter, 1, &val);
		gtk_tree_path_free(path);

		log = g_value_get_pointer(&val);

		if (log == NULL)
		{
			g_free(iter);
			return FALSE;
		}

		data = g_new(gpointer, 4);
		data[0] = lv;
		data[1] = log;
		data[2] = NULL;
		data[3] = iter;

		log_show_popup_menu(treeview, event, data);
		return TRUE;
	}

	return FALSE;
}

static gboolean log_popup_menu_cb(GtkWidget *treeview, PidginLogViewer *lv)
{
	GtkTreeSelection *sel;
	GtkTreeIter *iter;
	GValue val;
	PurpleLog *log;
	gpointer *data;

	iter = g_new(GtkTreeIter, 1);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(lv->treeview));
	if (!gtk_tree_selection_get_selected(sel, NULL, iter))
	{
		return FALSE;
	}

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(lv->treestore),
	                         iter, NODE_COLUMN, &val);

	log = g_value_get_pointer(&val);

	if (log == NULL)
		return FALSE;

	data = g_new(gpointer, 4);
	data[0] = lv;
	data[1] = log;
	data[2] = pidgin_treeview_popup_menu_position_func;
	data[3] = iter;

	log_show_popup_menu(treeview, NULL, data);
	return TRUE;
}

static gboolean search_find_cb(gpointer data)
{
	PidginLogViewer *viewer = data;
	gtk_imhtml_search_find(GTK_IMHTML(viewer->imhtml), viewer->search);
	g_object_steal_data(G_OBJECT(viewer->entry), "search-find-cb");
	return FALSE;
}

static void log_select_cb(GtkTreeSelection *sel, PidginLogViewer *viewer) {
	GtkTreeIter iter;
	GValue val;
	GtkTreeModel *model = GTK_TREE_MODEL(viewer->treestore);
	PurpleLog *log = NULL;
	PurpleLogReadFlags flags;
	char *read = NULL;

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 1, &val);
	log = g_value_get_pointer(&val);
	g_value_unset(&val);

	if (log == NULL)
		return;

	pidgin_set_cursor(viewer->window, GDK_WATCH);

	if (log->type != PURPLE_LOG_SYSTEM) {
		char *title;
		if (log->type == PURPLE_LOG_CHAT)
			title = g_strdup_printf(_("<span size='larger' weight='bold'>Conversation in %s on %s</span>"),
									log->name, log_get_date(log));
		else
			title = g_strdup_printf(_("<span size='larger' weight='bold'>Conversation with %s on %s</span>"),
									log->name, log_get_date(log));

		gtk_label_set_markup(GTK_LABEL(viewer->label), title);
		g_free(title);
	}

	read = purple_log_read(log, &flags);
	viewer->flags = flags;

	gtk_imhtml_clear(GTK_IMHTML(viewer->imhtml));
	gtk_imhtml_set_protocol_name(GTK_IMHTML(viewer->imhtml),
	                            purple_account_get_protocol_name(log->account));

	purple_signal_emit(pidgin_log_get_handle(), "log-displaying", viewer, log);

	gtk_imhtml_append_text(GTK_IMHTML(viewer->imhtml), read,
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_SCROLL |
			       ((flags & PURPLE_LOG_READ_NO_NEWLINE) ? GTK_IMHTML_NO_NEWLINE : 0));
	g_free(read);

	if (viewer->search != NULL) {
		guint source;
		gtk_imhtml_search_clear(GTK_IMHTML(viewer->imhtml));
		source = g_idle_add(search_find_cb, viewer);
		g_object_set_data_full(G_OBJECT(viewer->entry), "search-find-cb",
		                       GINT_TO_POINTER(source), (GDestroyNotify)g_source_remove);
	}

	pidgin_clear_cursor(viewer->window);
}

/* I want to make this smarter, but haven't come up with a cool algorithm to do so, yet.
 * I want the tree to be divided into groups like "Today," "Yesterday," "Last week,"
 * "August," "2002," etc. based on how many conversation took place in each subdivision.
 *
 * For now, I'll just make it a flat list.
 */
static void populate_log_tree(PidginLogViewer *lv)
     /* Logs are made from trees in real life.
        This is a tree made from logs */
{
	const char *month;
	char prev_top_month[30] = "";
	GtkTreeIter toplevel, child;
	GList *logs = lv->logs;

	while (logs != NULL) {
		PurpleLog *log = logs->data;

		month = purple_utf8_strftime(_("%B %Y"),
		                           log->tm ? log->tm : localtime(&log->time));

		if (strcmp(month, prev_top_month) != 0)
		{
			/* top level */
			gtk_tree_store_append(lv->treestore, &toplevel, NULL);
			gtk_tree_store_set(lv->treestore, &toplevel, 0, month, 1, NULL, -1);

			strncpy(prev_top_month, month, sizeof(prev_top_month));
		}

		/* sub */
		gtk_tree_store_append(lv->treestore, &child, &toplevel);
		gtk_tree_store_set(lv->treestore, &child,
						   0, log_get_date(log),
						   1, log,
						   -1);

		logs = logs->next;
	}
}

static PidginLogViewer *display_log_viewer(struct log_viewer_hash_t *ht, GList *logs,
						const char *title, GtkWidget *icon, int log_size)
{
	PidginLogViewer *lv;
	GtkWidget *title_box;
	char *text;
	GtkWidget *pane;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *find_button;
	GtkWidget *size_label;

	if (logs == NULL)
	{
		/* No logs were found. */
		const char *log_preferences = NULL;

		if (ht == NULL) {
			if (!purple_prefs_get_bool("/purple/logging/log_system"))
				log_preferences = _("System events will only be logged if the \"Log all status changes to system log\" preference is enabled.");
		} else {
			if (ht->type == PURPLE_LOG_IM) {
				if (!purple_prefs_get_bool("/purple/logging/log_ims"))
					log_preferences = _("Instant messages will only be logged if the \"Log all instant messages\" preference is enabled.");
			} else if (ht->type == PURPLE_LOG_CHAT) {
				if (!purple_prefs_get_bool("/purple/logging/log_chats"))
					log_preferences = _("Chats will only be logged if the \"Log all chats\" preference is enabled.");
			}
			g_free(ht->buddyname);
			g_free(ht);
		}

		if(icon != NULL)
			gtk_widget_destroy(icon);

		purple_notify_info(NULL, title, _("No logs were found"), log_preferences);
		return NULL;
	}

	lv = g_new0(PidginLogViewer, 1);
	lv->logs = logs;

	if (ht != NULL)
		g_hash_table_insert(log_viewers, ht, lv);

	/* Window ***********/
	lv->window = gtk_dialog_new_with_buttons(title, NULL, 0,
					     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
#ifdef _WIN32
	/* Steal the "HELP" response and use it to trigger browsing to the logs folder */
	gtk_dialog_add_button(GTK_DIALOG(lv->window), _("_Browse logs folder"), GTK_RESPONSE_HELP);
#endif
	gtk_container_set_border_width (GTK_CONTAINER(lv->window), PIDGIN_HIG_BOX_SPACE);
	gtk_dialog_set_has_separator(GTK_DIALOG(lv->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(lv->window)->vbox), 0);
	g_signal_connect(G_OBJECT(lv->window), "response",
					 G_CALLBACK(destroy_cb), ht);
	gtk_window_set_role(GTK_WINDOW(lv->window), "log_viewer");

	/* Icon *************/
	if (icon != NULL) {
		title_box = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_set_border_width(GTK_CONTAINER(title_box), PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(lv->window)->vbox), title_box, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(title_box), icon, FALSE, FALSE, 0);
	} else
		title_box = GTK_DIALOG(lv->window)->vbox;

	/* Label ************/
	lv->label = gtk_label_new(NULL);

	text = g_strdup_printf("<span size='larger' weight='bold'>%s</span>", title);

	gtk_label_set_markup(GTK_LABEL(lv->label), text);
	gtk_misc_set_alignment(GTK_MISC(lv->label), 0, 0);
	gtk_box_pack_start(GTK_BOX(title_box), lv->label, FALSE, FALSE, 0);
	g_free(text);

	/* Pane *************/
	pane = gtk_hpaned_new();
	gtk_container_set_border_width(GTK_CONTAINER(pane), PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(lv->window)->vbox), pane, TRUE, TRUE, 0);

	/* List *************/
	lv->treestore = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	lv->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (lv->treestore));
	g_object_unref(G_OBJECT(lv->treestore));
	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("time", rend, "markup", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(lv->treeview), col);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (lv->treeview), FALSE);
	gtk_paned_add1(GTK_PANED(pane), 
		pidgin_make_scrollable(lv->treeview, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, -1));

	populate_log_tree(lv);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (lv->treeview));
	g_signal_connect (G_OBJECT (sel), "changed",
			G_CALLBACK (log_select_cb),
			lv);
	g_signal_connect (G_OBJECT(lv->treeview), "row-activated",
			G_CALLBACK(log_row_activated_cb),
			lv);
	pidgin_set_accessible_label(lv->treeview, lv->label);

	g_signal_connect(lv->treeview, "button-press-event", G_CALLBACK(log_button_press_cb), lv);
	g_signal_connect(lv->treeview, "popup-menu", G_CALLBACK(log_popup_menu_cb), lv);

	/* Log size ************/
	if(log_size) {
		char *sz_txt = purple_str_size_to_units(log_size);
		text = g_strdup_printf("<span weight='bold'>%s</span> %s", _("Total log size:"), sz_txt);
		size_label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(size_label), text);
		/*		gtk_paned_add1(GTK_PANED(pane), size_label); */
		gtk_misc_set_alignment(GTK_MISC(size_label), 0, 0);
		gtk_box_pack_end(GTK_BOX(GTK_DIALOG(lv->window)->vbox), size_label, FALSE, FALSE, 0);
		g_free(sz_txt);
		g_free(text);
	}

	/* A fancy little box ************/
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_add2(GTK_PANED(pane), vbox);

	/* Viewer ************/
	frame = pidgin_create_imhtml(FALSE, &lv->imhtml, NULL, NULL);
	gtk_widget_set_name(lv->imhtml, "pidgin_log_imhtml");
	gtk_widget_set_size_request(lv->imhtml, 320, 200);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	/* Search box **********/
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	lv->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), lv->entry, TRUE, TRUE, 0);
	find_button = gtk_button_new_from_stock(GTK_STOCK_FIND);
	gtk_box_pack_start(GTK_BOX(hbox), find_button, FALSE, FALSE, 0);
	g_signal_connect(GTK_ENTRY(lv->entry), "activate", G_CALLBACK(search_cb), lv);
	g_signal_connect(GTK_BUTTON(find_button), "clicked", G_CALLBACK(search_cb), lv);

	select_first_log(lv);

	gtk_widget_show_all(lv->window);

	return lv;
}

void pidgin_log_show(PurpleLogType type, const char *buddyname, PurpleAccount *account) {
	struct log_viewer_hash_t *ht;
	PidginLogViewer *lv = NULL;
	const char *name = buddyname;
	char *title;
	GdkPixbuf *prpl_icon;

	g_return_if_fail(account != NULL);
	g_return_if_fail(buddyname != NULL);

	ht = g_new0(struct log_viewer_hash_t, 1);

	ht->type = type;
	ht->buddyname = g_strdup(buddyname);
	ht->account = account;

	if (log_viewers == NULL) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gtk_window_present(GTK_WINDOW(lv->window));
		g_free(ht->buddyname);
		g_free(ht);
		return;
	}

	if (type == PURPLE_LOG_CHAT) {
		PurpleChat *chat;

		chat = purple_blist_find_chat(account, buddyname);
		if (chat != NULL)
			name = purple_chat_get_name(chat);

		title = g_strdup_printf(_("Conversations in %s"), name);
	} else {
		PurpleBuddy *buddy;

		buddy = purple_find_buddy(account, buddyname);
		if (buddy != NULL)
			name = purple_buddy_get_contact_alias(buddy);

		title = g_strdup_printf(_("Conversations with %s"), name);
	}

	prpl_icon = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);

	display_log_viewer(ht, purple_log_get_logs(type, buddyname, account),
			title, gtk_image_new_from_pixbuf(prpl_icon),
			purple_log_get_total_size(type, buddyname, account));

	if (prpl_icon)
		g_object_unref(prpl_icon);
	g_free(title);
}

void pidgin_log_show_contact(PurpleContact *contact) {
	struct log_viewer_hash_t *ht;
	PurpleBlistNode *child;
	PidginLogViewer *lv = NULL;
	GList *logs = NULL;
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	const char *name = NULL;
	char *title;
	int total_log_size = 0;

	g_return_if_fail(contact != NULL);

	ht = g_new0(struct log_viewer_hash_t, 1);
	ht->type = PURPLE_LOG_IM;
	ht->contact = contact;

	if (log_viewers == NULL) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gtk_window_present(GTK_WINDOW(lv->window));
		g_free(ht);
		return;
	}

	for (child = purple_blist_node_get_first_child((PurpleBlistNode*)contact) ;
	     child != NULL ;
	     child = purple_blist_node_get_sibling_next(child)) {
		const char *buddy_name;
		PurpleAccount *account;

		if (!PURPLE_BLIST_NODE_IS_BUDDY(child))
			continue;

		buddy_name = purple_buddy_get_name((PurpleBuddy *)child);
		account = purple_buddy_get_account((PurpleBuddy *)child);
		logs = g_list_concat(purple_log_get_logs(PURPLE_LOG_IM, buddy_name, account), logs);
		total_log_size += purple_log_get_total_size(PURPLE_LOG_IM, buddy_name, account);
	}
	logs = g_list_sort(logs, purple_log_compare);

	image = gtk_image_new();
	pixbuf = gtk_widget_render_icon(image, PIDGIN_STOCK_STATUS_PERSON,
					gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL), "GtkWindow");
	if (pixbuf) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
		g_object_unref(pixbuf);
	} else {
		gtk_widget_destroy(image);
		image = NULL;
	}

	if (contact->alias != NULL)
		name = contact->alias;
	else if (contact->priority != NULL)
		name = purple_buddy_get_contact_alias(contact->priority);

	/* This will happen if the contact doesn't have an alias,
	 * and none of the contact's buddies are online.
	 * There is probably a better way to deal with this. */
	if (name == NULL) {
		if (contact->node.child != NULL && PURPLE_BLIST_NODE_IS_BUDDY(contact->node.child))
			name = purple_buddy_get_contact_alias((PurpleBuddy *) contact->node.child);
		if (name == NULL)
			name = "";
	}

	title = g_strdup_printf(_("Conversations with %s"), name);
	display_log_viewer(ht, logs, title, image, total_log_size);
	g_free(title);
}

void pidgin_syslog_show()
{
	GList *accounts = NULL;
	GList *logs = NULL;

	if (syslog_viewer != NULL) {
		gtk_window_present(GTK_WINDOW(syslog_viewer->window));
		return;
	}

	for(accounts = purple_accounts_get_all(); accounts != NULL; accounts = accounts->next) {

		PurpleAccount *account = (PurpleAccount *)accounts->data;
		if(purple_find_prpl(purple_account_get_protocol_id(account)) == NULL)
			continue;

		logs = g_list_concat(purple_log_get_system_logs(account), logs);
	}
	logs = g_list_sort(logs, purple_log_compare);

	syslog_viewer = display_log_viewer(NULL, logs, _("System Log"), NULL, 0);
}

/****************************************************************************
 * GTK+ LOG SUBSYSTEM *******************************************************
 ****************************************************************************/

void *
pidgin_log_get_handle(void)
{
	static int handle;

	return &handle;
}

void pidgin_log_init(void)
{
	void *handle = pidgin_log_get_handle();

	purple_signal_register(handle, "log-displaying",
	                     purple_marshal_VOID__POINTER_POINTER,
	                     NULL, 2,
	                     purple_value_new(PURPLE_TYPE_BOXED,
	                                    "PidginLogViewer *"),
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_LOG));
}

void
pidgin_log_uninit(void)
{
	purple_signals_unregister_by_instance(pidgin_log_get_handle());
}
