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

#include <talkatu.h>

#include "account.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "request.h"
#include "util.h"

#include "pidginstock.h"
#include "gtkblist.h"
#include "gtkutils.h"
#include "pidginlog.h"

#define PIDGIN_TYPE_LOG_VIEWER pidgin_log_viewer_get_type()
/**
 * PidginLogViewer:
 * @logs:          The list of logs viewed in this viewer
 * @browse_button: The button for opening a log folder externally
 * @treestore:     The treestore containing said logs
 * @treeview:      The treeview representing said treestore
 * @log_view:      The talkatu view to display said logs
 * @log_buffer:    The talkatu buffer to hold said logs
 * @title_box:     The box containing the title (and optional icon)
 * @label:         The label at the top of the log viewer
 * @size_label:    The label to show the size of the logs
 * @entry:         The search entry, in which search terms are entered
 * @search:        The string currently being searched for
 *
 * A Pidgin Log Viewer.  You can look at logs with it.
 */
G_DECLARE_FINAL_TYPE(PidginLogViewer, pidgin_log_viewer, PIDGIN, LOG_VIEWER, GtkDialog)

struct _PidginLogViewer {
	GtkDialog parent;

	GList *logs;

	GtkWidget *browse_button;

	GtkTreeStore     *treestore;
	GtkWidget        *treeview;
	GtkWidget        *log_view;
	TalkatuHtmlBuffer *log_buffer;

	GtkWidget *title_box;
	GtkLabel         *label;

	GtkWidget *size_label;

	GtkWidget *entry;
	char *search;
};

G_DEFINE_TYPE(PidginLogViewer, pidgin_log_viewer, GTK_TYPE_DIALOG)

static GHashTable *log_viewers = NULL;
static void populate_log_tree(PidginLogViewer *lv);
static PidginLogViewer *syslog_viewer = NULL;

struct log_viewer_hash {
	PurpleLogType type;
	char *buddyname;
	PurpleAccount *account;
	PurpleContact *contact;
};

static guint log_viewer_hash(gconstpointer data)
{
	const struct log_viewer_hash *viewer = data;

	if (viewer->contact != NULL)
		return g_direct_hash(viewer->contact);

	return g_str_hash(viewer->buddyname) +
		g_str_hash(purple_account_get_username(viewer->account));
}

static gboolean log_viewer_equal(gconstpointer y, gconstpointer z)
{
	const struct log_viewer_hash *a, *b;
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
		purple_strequal(normal, purple_normalize(b->account, b->buddyname));
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

static gchar *log_get_date(PurpleLog *log)
{
	GDateTime *dt;
	gchar *ret;
	dt = g_date_time_to_local(log->time);
	ret = g_date_time_format(dt, "%c");
	g_date_time_unref(dt);
	return ret;
}

static void
entry_stop_search_cb(GtkWidget *entry, PidginLogViewer *lv)
{
	/* reset the tree */
	gtk_tree_store_clear(lv->treestore);
	populate_log_tree(lv);
	g_clear_pointer(&lv->search, g_free);
#if 0
	webkit_web_view_unmark_text_matches(WEBKIT_WEB_VIEW(lv->log_view));
#endif
	select_first_log(lv);
}

static void
entry_search_changed_cb(GtkWidget *button, PidginLogViewer *lv)
{
	const char *search_term = gtk_entry_get_text(GTK_ENTRY(lv->entry));
	GList *logs;

	if (lv->search != NULL && purple_strequal(lv->search, search_term))
	{
#if 0
		/* Searching for the same term acts as "Find Next" */
		webkit_web_view_search_text(WEBKIT_WEB_VIEW(lv->log_view), lv->search, FALSE, TRUE, TRUE);
#endif
		return;
	}

	pidgin_set_cursor(GTK_WIDGET(lv), GDK_WATCH);

	g_free(lv->search);
	lv->search = g_strdup(search_term);

	gtk_tree_store_clear(lv->treestore);
	talkatu_buffer_clear(TALKATU_BUFFER(lv->log_buffer));

	for (logs = lv->logs; logs != NULL; logs = logs->next) {
		char *read = purple_log_read((PurpleLog*)logs->data, NULL);
		if (read && *read && purple_strcasestr(read, search_term)) {
			GtkTreeIter iter;
			PurpleLog *log = logs->data;
			gchar *log_date = log_get_date(log);

			gtk_tree_store_append (lv->treestore, &iter, NULL);
			gtk_tree_store_set(lv->treestore, &iter,
					   0, log_date,
					   1, log, -1);
			g_free(log_date);
		}
		g_free(read);
	}

	select_first_log(lv);
	pidgin_clear_cursor(GTK_WIDGET(lv));
}

static void
destroy_cb(GtkWidget *w, gint resp, struct log_viewer_hash *ht)
{
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
			logdir = g_build_filename(purple_data_dir(), "logs", NULL);
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

	g_list_free_full(lv->logs, (GDestroyNotify)purple_log_free);

	g_free(lv->search);

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
		                  _("Check permissions and try again."), NULL);
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
	GtkTreeIter *iter = data[2];
	gchar *time = log_get_date(log);
	const char *name;
	char *tmp;
	gpointer *data2;

	if (log->type == PURPLE_LOG_IM)
	{
		PurpleBuddy *buddy = purple_blist_find_buddy(log->account, log->name);
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
	else {
		g_free(time);
		g_free(iter);
		g_return_if_reached();
	}

	/* The only way to free data in all cases is to tie it to the menuitem with
	 * g_object_set_data_full().  But, since we need to get some data down to
	 * delete_log_cb() to delete the log from the log viewer after the file is
	 * deleted, we have to allocate a new data array and make sure it gets freed
	 * either way. */
	data2 = g_new(gpointer, 3);
	data2[0] = lv->treestore;
	data2[1] = iter;
	data2[2] = log;
	purple_request_action(lv, NULL, _("Delete Log?"), tmp, 0,
						NULL,
						data2, 2,
						_("Delete"), delete_log_cb,
						_("Cancel"), delete_log_cleanup_cb);
	g_free(time);
	g_free(tmp);
}

static GtkWidget *
log_create_popup_menu(GtkWidget *treeview, PidginLogViewer *lv, GtkTreeIter *iter)
{
	GValue val;
	PurpleLog *log;
	GtkWidget *menu;
	GtkWidget *menuitem;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(lv->treestore), iter, 1, &val);
	log = g_value_get_pointer(&val);
	if (log == NULL) {
		g_free(iter);
		return NULL;
	}

	menu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_label(_("Delete Log..."));

	if (purple_log_is_deletable(log)) {
		gpointer *data = g_new(gpointer, 3);
		data[0] = lv;
		data[1] = log;
		data[2] = iter;

		g_signal_connect(menuitem, "activate", G_CALLBACK(log_delete_log_cb), data);
		g_object_set_data_full(G_OBJECT(menuitem), "log-viewer-data", data, g_free);
	} else {
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show_all(menu);

	return menu;
}

static gboolean log_button_press_cb(GtkWidget *treeview, GdkEventButton *event, PidginLogViewer *lv)
{
	if (gdk_event_triggers_context_menu((GdkEvent *)event)) {
		GtkTreePath *path;
		GtkTreeIter *iter;
		GtkWidget *menu;

		if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), event->x, event->y, &path, NULL, NULL, NULL))
			return FALSE;
		iter = g_new(GtkTreeIter, 1);
		gtk_tree_model_get_iter(GTK_TREE_MODEL(lv->treestore), iter, path);
		gtk_tree_path_free(path);

		menu = log_create_popup_menu(treeview, lv, iter);
		if (menu) {
			gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
			return TRUE;
		} else {
			return FALSE;
		}
	}

	return FALSE;
}

static gboolean log_popup_menu_cb(GtkWidget *treeview, PidginLogViewer *lv)
{
	GtkTreeSelection *sel;
	GtkTreeIter *iter;
	GtkWidget *menu;

	iter = g_new(GtkTreeIter, 1);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(lv->treeview));
	if (!gtk_tree_selection_get_selected(sel, NULL, iter)) {
		g_free(iter);
		return FALSE;
	}

	menu = log_create_popup_menu(treeview, lv, iter);
	if (menu) {
		pidgin_menu_popup_at_treeview_selection(menu, treeview);
		return TRUE;
	} else {
		return FALSE;
	}
}

#if 0 /* FIXME: Add support in Talkatu for highlighting search terms. */
static gboolean search_find_cb(gpointer data)
{
	PidginLogViewer *viewer = data;
	webkit_web_view_mark_text_matches(WEBKIT_WEB_VIEW(viewer->log_view), viewer->search, FALSE, 0);
	webkit_web_view_set_highlight_text_matches(WEBKIT_WEB_VIEW(viewer->log_view), TRUE);
	webkit_web_view_search_text(WEBKIT_WEB_VIEW(viewer->log_view), viewer->search, FALSE, TRUE, TRUE);
	return FALSE;
}
#endif

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

	pidgin_set_cursor(GTK_WIDGET(viewer), GDK_WATCH);

	if (log->type != PURPLE_LOG_SYSTEM) {
		gchar *log_date = log_get_date(log);
		gchar *title;
		if (log->type == PURPLE_LOG_CHAT) {
			title = g_strdup_printf(_("Conversation in %s on %s"),
			                        log->name, log_date);
		} else {
			title = g_strdup_printf(_("Conversation with %s on %s"),
			                        log->name, log_date);
		}

		gtk_label_set_markup(viewer->label, title);
		g_free(log_date);
		g_free(title);
	}

	read = purple_log_read(log, &flags);

	talkatu_buffer_clear(TALKATU_BUFFER(viewer->log_buffer));

	purple_signal_emit(pidgin_log_get_handle(), "log-displaying", viewer, log);
	
	/* plaintext log (html one starts with <html> tag) */
	if (read[0] != '<')
	{
		char *newRead = purple_strreplace(read, "\n", "<br>");
		g_free(read);
		read = newRead;
	}

	talkatu_markup_set_html(TALKATU_BUFFER(viewer->log_buffer), read, -1);
	g_free(read);

	if (viewer->search != NULL) {
#if 0 /* FIXME: Add support in Talkatu for highlighting search terms. */
		webkit_web_view_unmark_text_matches(WEBKIT_WEB_VIEW(viewer->log_view));
		g_idle_add(search_find_cb, viewer);
#endif
	}

	pidgin_clear_cursor(GTK_WIDGET(viewer));
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
	gchar *month;
	char prev_top_month[30] = "";
	GtkTreeIter toplevel, child;
	GList *logs = lv->logs;

	while (logs != NULL) {
		PurpleLog *log = logs->data;
		GDateTime *dt;
		gchar *log_date;

		dt = g_date_time_to_local(log->time);
		month = g_date_time_format(dt, _("%B %Y"));

		if (!purple_strequal(month, prev_top_month)) {
			/* top level */
			gtk_tree_store_append(lv->treestore, &toplevel, NULL);
			gtk_tree_store_set(lv->treestore, &toplevel, 0, month, 1, NULL, -1);

			g_strlcpy(prev_top_month, month, sizeof(prev_top_month));
		}

		/* sub */
		log_date = g_date_time_format(dt, "%c");
		gtk_tree_store_append(lv->treestore, &child, &toplevel);
		gtk_tree_store_set(lv->treestore, &child,
						   0, log_date,
						   1, log,
						   -1);

		g_free(log_date);
		g_free(month);
		g_date_time_unref(dt);
		logs = logs->next;
	}
}

static PidginLogViewer *
display_log_viewer(struct log_viewer_hash *ht, GList *logs, const char *title,
                   GtkWidget *icon, int log_size)
{
	PidginLogViewer *lv;

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

		purple_notify_info(NULL, title, _("No logs were found"), log_preferences, NULL);
		return NULL;
	}

	/* Window ***********/
	lv = g_object_new(PIDGIN_TYPE_LOG_VIEWER, NULL);
	gtk_window_set_title(GTK_WINDOW(lv), title);

	lv->logs = logs;

	if (ht != NULL)
		g_hash_table_insert(log_viewers, ht, lv);

#ifndef _WIN32
	gtk_widget_hide(lv->browse_button);
#endif

	g_signal_connect(G_OBJECT(lv), "response", G_CALLBACK(destroy_cb), ht);

	/* Icon *************/
	if (icon != NULL) {
		gtk_box_pack_start(GTK_BOX(lv->title_box), icon, FALSE, FALSE,
		                   0);
	}

	/* Label ************/
	gtk_label_set_markup(lv->label, title);

	/* List *************/
	populate_log_tree(lv);

	/* Log size ************/
	if(log_size) {
		char *sz_txt = g_format_size(log_size);
		char *text = g_strdup_printf("<span weight='bold'>%s</span> %s",
		                             _("Total log size:"), sz_txt);
		gtk_label_set_markup(GTK_LABEL(lv->size_label), text);
		g_free(sz_txt);
		g_free(text);
	} else {
		gtk_widget_hide(lv->size_label);
	}

	select_first_log(lv);

	gtk_widget_show(GTK_WIDGET(lv));

	return lv;
}

/****************************************************************************
 * GObject Implementation
 ****************************************************************************/
static void
pidgin_log_viewer_class_init(PidginLogViewerClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin/Log/log-viewer.ui");

	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, browse_button);

	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, title_box);
	gtk_widget_class_bind_template_child_internal(widget_class,
	                                              PidginLogViewer, label);

	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, treeview);
	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, treestore);
	gtk_widget_class_bind_template_callback(widget_class, log_select_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        log_row_activated_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        log_button_press_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        log_popup_menu_cb);

	gtk_widget_class_bind_template_child_internal(widget_class,
	                                              PidginLogViewer, entry);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        entry_search_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        entry_stop_search_cb);

	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, log_view);
	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, log_buffer);

	gtk_widget_class_bind_template_child_internal(
	        widget_class, PidginLogViewer, size_label);
}

static void
pidgin_log_viewer_init(PidginLogViewer *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));
}

/****************************************************************************
 * Public API
 ****************************************************************************/

void pidgin_log_show(PurpleLogType type, const char *buddyname, PurpleAccount *account) {
	struct log_viewer_hash *ht;
	PidginLogViewer *lv = NULL;
	const char *name = buddyname;
	char *title;
	GdkPixbuf *protocol_icon;

	g_return_if_fail(account != NULL);
	g_return_if_fail(buddyname != NULL);

	ht = g_new0(struct log_viewer_hash, 1);

	ht->type = type;
	ht->buddyname = g_strdup(buddyname);
	ht->account = account;

	if (log_viewers == NULL) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gtk_window_present(GTK_WINDOW(lv));
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

		buddy = purple_blist_find_buddy(account, buddyname);
		if (buddy != NULL)
			name = purple_buddy_get_contact_alias(buddy);

		title = g_strdup_printf(_("Conversations with %s"), name);
	}

	protocol_icon = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_MEDIUM);

	display_log_viewer(ht, purple_log_get_logs(type, buddyname, account),
			title, gtk_image_new_from_pixbuf(protocol_icon),
			purple_log_get_total_size(type, buddyname, account));

	if (protocol_icon)
		g_object_unref(protocol_icon);
	g_free(title);
}

void pidgin_log_show_contact(PurpleContact *contact) {
	struct log_viewer_hash *ht;
	PurpleBlistNode *child;
	PidginLogViewer *lv = NULL;
	GList *logs = NULL;
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	const char *name = NULL;
	char *title;
	int total_log_size = 0;

	g_return_if_fail(contact != NULL);

	ht = g_new0(struct log_viewer_hash, 1);
	ht->type = PURPLE_LOG_IM;
	ht->contact = contact;

	if (log_viewers == NULL) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gtk_window_present(GTK_WINDOW(lv));
		g_free(ht);
		return;
	}

	for (child = purple_blist_node_get_first_child((PurpleBlistNode*)contact) ;
	     child != NULL ;
	     child = purple_blist_node_get_sibling_next(child)) {
		const char *buddy_name;
		PurpleAccount *account;

		if (!PURPLE_IS_BUDDY(child))
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

	name = purple_contact_get_alias(contact);

	/* This will happen if the contact doesn't have an alias,
	 * and none of the contact's buddies are online.
	 * There is probably a better way to deal with this. */
	if (name == NULL) {
		if (PURPLE_BLIST_NODE(contact)->child != NULL &&
				PURPLE_IS_BUDDY(PURPLE_BLIST_NODE(contact)->child))
			name = purple_buddy_get_contact_alias(PURPLE_BUDDY(
					PURPLE_BLIST_NODE(contact)->child));
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
		gtk_window_present(GTK_WINDOW(syslog_viewer));
		return;
	}

	for(accounts = purple_accounts_get_all(); accounts != NULL; accounts = accounts->next) {

		PurpleAccount *account = (PurpleAccount *)accounts->data;
		if(purple_protocols_find(purple_account_get_protocol_id(account)) == NULL)
			continue;

		logs = g_list_concat(purple_log_get_system_logs(account), logs);
	}
	logs = g_list_sort(logs, purple_log_compare);

	syslog_viewer = display_log_viewer(NULL, logs, _("System Log"), NULL, 0);
}

/****************************************************************************
 * PIDGIN LOG SUBSYSTEM *****************************************************
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
	                     G_TYPE_NONE, 2,
	                     G_TYPE_POINTER, /* (PidginLogViewer *) */
	                     PURPLE_TYPE_LOG);
}

void
pidgin_log_uninit(void)
{
	purple_signals_unregister_by_instance(pidgin_log_get_handle());
}
