/**
 * @file gtkdebug.c GTK+ Debug API
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

#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "util.h"

#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "pidginstock.h"

#ifdef HAVE_REGEX_H
# include <regex.h>
#	define USE_REGEX 1
#else
#if GLIB_CHECK_VERSION(2,14,0)
#	define USE_REGEX 1
#endif
#endif /* HAVE_REGEX_H */

#include <gdk/gdkkeysyms.h>

typedef struct
{
	GtkWidget *window;
	GtkWidget *text;

	GtkListStore *store;

	gboolean paused;

#ifdef USE_REGEX
	GtkWidget *filter;
	GtkWidget *expression;

	gboolean invert;
	gboolean highlight;

	guint timer;
#	ifdef HAVE_REGEX_H
	regex_t regex;
#	else
	GRegex *regex;
#	endif /* HAVE_REGEX_H */
#else
	GtkWidget *find;
#endif /* USE_REGEX */
	GtkWidget *filterlevel;
} DebugWindow;

static const char debug_fg_colors[][8] = {
	"#000000",    /**< All debug levels. */
	"#666666",    /**< Misc.             */
	"#000000",    /**< Information.      */
	"#660000",    /**< Warnings.         */
	"#FF0000",    /**< Errors.           */
	"#FF0000",    /**< Fatal errors.     */
};

static DebugWindow *debug_win = NULL;
static guint debug_enabled_timer = 0;

#ifdef USE_REGEX
static void regex_filter_all(DebugWindow *win);
static void regex_show_all(DebugWindow *win);
#endif /* USE_REGEX */

static gint
debug_window_destroy(GtkWidget *w, GdkEvent *event, void *unused)
{
	purple_prefs_disconnect_by_handle(pidgin_debug_get_handle());

#ifdef USE_REGEX
	if(debug_win->timer != 0) {
		const gchar *text;

		purple_timeout_remove(debug_win->timer);

		text = gtk_entry_get_text(GTK_ENTRY(debug_win->expression));
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/debug/regex", text);
	}
#ifdef HAVE_REGEX_H
	regfree(&debug_win->regex);
#else
	g_regex_unref(debug_win->regex);
#endif /* HAVE_REGEX_H */
#endif /* USE_REGEX */

	/* If the "Save Log" dialog is open then close it */
	purple_request_close_with_handle(debug_win);

	g_free(debug_win);
	debug_win = NULL;

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", FALSE);

	return FALSE;
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *event, DebugWindow *win)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/height", event->height);
	}

	return FALSE;
}

#ifndef USE_REGEX
struct _find {
	DebugWindow *window;
	GtkWidget *entry;
};

static void
do_find_cb(GtkWidget *widget, gint response, struct _find *f)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		gtk_imhtml_search_find(GTK_IMHTML(f->window->text),
							   gtk_entry_get_text(GTK_ENTRY(f->entry)));
		break;

	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CLOSE:
		gtk_imhtml_search_clear(GTK_IMHTML(f->window->text));
		gtk_widget_destroy(f->window->find);
		f->window->find = NULL;
		g_free(f);
		break;
	}
}

static void
find_cb(GtkWidget *w, DebugWindow *win)
{
	GtkWidget *hbox, *img, *label;
	struct _find *f;

	if(win->find)
	{
		gtk_window_present(GTK_WINDOW(win->find));
		return;
	}

	f = g_malloc(sizeof(struct _find));
	f->window = win;
	win->find = gtk_dialog_new_with_buttons(_("Find"),
					GTK_WINDOW(win->window), GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					GTK_STOCK_FIND, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(win->find),
					 GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(win->find), "response",
					G_CALLBACK(do_find_cb), f);

	gtk_container_set_border_width(GTK_CONTAINER(win->find), PIDGIN_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(win->find), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(win->find), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(win->find)->vbox), PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(
		GTK_CONTAINER(GTK_DIALOG(win->find)->vbox), PIDGIN_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(win->find)->vbox),
					  hbox);
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
				       gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(win->find),
									  GTK_RESPONSE_OK, FALSE);

	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Search for:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	f->entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(f->entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(f->entry));
	g_signal_connect(G_OBJECT(f->entry), "changed",
					 G_CALLBACK(pidgin_set_sensitive_if_input),
					 win->find);
	gtk_box_pack_start(GTK_BOX(hbox), f->entry, FALSE, FALSE, 0);

	gtk_widget_show_all(win->find);
	gtk_widget_grab_focus(f->entry);
}
#endif /* USE_REGEX */

static void
save_writefile_cb(void *user_data, const char *filename)
{
	DebugWindow *win = (DebugWindow *)user_data;
	FILE *fp;
	char *tmp;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		purple_notify_error(win, NULL, _("Unable to open file."), NULL);
		return;
	}

	tmp = gtk_imhtml_get_text(GTK_IMHTML(win->text), NULL, NULL);
	fprintf(fp, "Pidgin Debug Log : %s\n", purple_date_format_full(NULL));
	fprintf(fp, "%s", tmp);
	g_free(tmp);

	fclose(fp);
}

static void
save_cb(GtkWidget *w, DebugWindow *win)
{
	purple_request_file(win, _("Save Debug Log"), "purple-debug.log", TRUE,
					  G_CALLBACK(save_writefile_cb), NULL,
					  NULL, NULL, NULL,
					  win);
}

static void
clear_cb(GtkWidget *w, DebugWindow *win)
{
	gtk_imhtml_clear(GTK_IMHTML(win->text));

#ifdef USE_REGEX
	gtk_list_store_clear(win->store);
#endif /* USE_REGEX */
}

static void
pause_cb(GtkWidget *w, DebugWindow *win)
{
	win->paused = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));

#ifdef USE_REGEX
	if(!win->paused) {
		if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
			regex_filter_all(win);
		else
			regex_show_all(win);
	}
#endif /* USE_REGEX */
}

/******************************************************************************
 * regex stuff
 *****************************************************************************/
#ifdef USE_REGEX
static void
regex_clear_color(GtkWidget *w) {
	gtk_widget_modify_base(w, GTK_STATE_NORMAL, NULL);
}

static void
regex_change_color(GtkWidget *w, guint16 r, guint16 g, guint16 b) {
	GdkColor color;

	color.red = r;
	color.green = g;
	color.blue = b;

	gtk_widget_modify_base(w, GTK_STATE_NORMAL, &color);
}

static void
regex_highlight_clear(DebugWindow *win) {
	GtkIMHtml *imhtml = GTK_IMHTML(win->text);
	GtkTextIter s, e;

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &s);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &e);
	gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "regex", &s, &e);
}

static void
regex_match(DebugWindow *win, const gchar *text) {
	GtkIMHtml *imhtml = GTK_IMHTML(win->text);
#ifdef HAVE_REGEX_H
	regmatch_t matches[4]; /* adjust if necessary */
	size_t n_matches = sizeof(matches) / sizeof(matches[0]);
	gint inverted;
#else
	GMatchInfo *match_info;
#endif /* HAVE_REGEX_H */
	gchar *plaintext;

	if(!text)
		return;

	/* I don't like having to do this, but we need it for highlighting.  Plus
	 * it makes the ^ and $ operators work :)
	 */
	plaintext = purple_markup_strip_html(text);

	/* we do a first pass to see if it matches at all.  If it does we append
	 * it, and work out the offsets to highlight.
	 */
#ifdef HAVE_REGEX_H
	inverted = (win->invert) ? REG_NOMATCH : 0;
	if(regexec(&win->regex, plaintext, n_matches, matches, 0) == inverted) {
#else
	if(g_regex_match(win->regex, plaintext, 0, &match_info) != win->invert) {
#endif /* HAVE_REGEX_H */
		gchar *p = plaintext;
		GtkTextIter ins;
		gint i, offset = 0;

		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &ins,
							gtk_text_buffer_get_insert(imhtml->text_buffer));
		i = gtk_text_iter_get_offset(&ins);

		gtk_imhtml_append_text(imhtml, text, 0);

		/* If we're not highlighting or the expression is inverted, we're
		 * done and move on.
		 */
		if(!win->highlight || win->invert) {
			g_free(plaintext);
#ifndef HAVE_REGEX_H
			g_match_info_free(match_info);
#endif
			return;
		}

		/* we use a do-while to highlight the first match, and then continue
		 * if necessary...
		 */
#ifdef HAVE_REGEX_H
		do {
			size_t m;

			for(m = 0; m < n_matches; m++) {
				GtkTextIter ms, me;

				if(matches[m].rm_eo == -1)
					break;

				i += offset;

				gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &ms,
												   i + matches[m].rm_so);
				gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &me,
												   i + matches[m].rm_eo);
				gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "regex",
												  &ms, &me);
				offset = matches[m].rm_eo;
			}

			p += offset;
		} while(regexec(&win->regex, p, n_matches, matches, REG_NOTBOL) == inverted);
#else
		do
		{
			gint m;
			gint start_pos, end_pos;
			GtkTextIter ms, me;

			if (!g_match_info_matches(match_info))
				break;

			for (m = 0; m < g_match_info_get_match_count(match_info); m++)
			{
				if (m == 1)
					continue;

				g_match_info_fetch_pos(match_info, m, &start_pos, &end_pos);

				if (end_pos == -1)
					break;

				gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &ms,
													i + start_pos);
				gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &me,
													i + end_pos);
				gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "regex",
												  &ms, &me);
				offset = end_pos;
			}

			g_match_info_free(match_info);
			p += offset;
			i += offset;
		} while (g_regex_match(win->regex, p, G_REGEX_MATCH_NOTBOL, &match_info) != win->invert);
		g_match_info_free(match_info);
#endif /* HAVE_REGEX_H */
	}

	g_free(plaintext);
}

static gboolean
regex_filter_all_cb(GtkTreeModel *m, GtkTreePath *p, GtkTreeIter *iter,
				    gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gchar *text;
	PurpleDebugLevel level;

	gtk_tree_model_get(m, iter, 0, &text, 1, &level, -1);

	if (level >= (PurpleDebugLevel)purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"))
		regex_match(win, text);

	g_free(text);

	return FALSE;
}

static void
regex_filter_all(DebugWindow *win) {
	gtk_imhtml_clear(GTK_IMHTML(win->text));

	if(win->highlight)
		regex_highlight_clear(win);

	gtk_tree_model_foreach(GTK_TREE_MODEL(win->store), regex_filter_all_cb,
						   win);
}

static gboolean
regex_show_all_cb(GtkTreeModel *m, GtkTreePath *p, GtkTreeIter *iter,
				  gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gchar *text;
	PurpleDebugLevel level;

	gtk_tree_model_get(m, iter, 0, &text, 1, &level, -1);
	if (level >= (PurpleDebugLevel)purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"))
		gtk_imhtml_append_text(GTK_IMHTML(win->text), text, 0);
	g_free(text);

	return FALSE;
}

static void
regex_show_all(DebugWindow *win) {
	gtk_imhtml_clear(GTK_IMHTML(win->text));

	if(win->highlight)
		regex_highlight_clear(win);

	gtk_tree_model_foreach(GTK_TREE_MODEL(win->store), regex_show_all_cb,
						   win);
}

static void
regex_compile(DebugWindow *win) {
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));

	if(text == NULL || *text == '\0') {
		regex_clear_color(win->expression);
		gtk_widget_set_sensitive(win->filter, FALSE);
		return;
	}

#ifdef HAVE_REGEX_H
	regfree(&win->regex);
	if(regcomp(&win->regex, text, REG_EXTENDED | REG_ICASE) != 0) {
#else
	if (win->regex)
		g_regex_unref(win->regex);
	win->regex = g_regex_new(text, G_REGEX_EXTENDED | G_REGEX_CASELESS, 0, NULL);
	if(win->regex == NULL) {
#endif
		/* failed to compile */
		regex_change_color(win->expression, 0xFFFF, 0xAFFF, 0xAFFF);
		gtk_widget_set_sensitive(win->filter, FALSE);
	} else {
		/* compiled successfully */
		regex_change_color(win->expression, 0xAFFF, 0xFFFF, 0xAFFF);
		gtk_widget_set_sensitive(win->filter, TRUE);
	}

	/* we check if the filter is on in case it was only of the options that
	 * got changed, and not the expression.
	 */
	if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_filter_all(win);
}

static void
regex_pref_filter_cb(const gchar *name, PurplePrefType type,
					 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val), current;

	if(!win || !win->window)
		return;

	current = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter));
	if(active != current)
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter), active);
}

static void
regex_pref_expression_cb(const gchar *name, PurplePrefType type,
						 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	const gchar *exp = (const gchar *)val;

	gtk_entry_set_text(GTK_ENTRY(win->expression), exp);
}

static void
regex_pref_invert_cb(const gchar *name, PurplePrefType type,
					 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->invert = active;

	if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_filter_all(win);
}

static void
regex_pref_highlight_cb(const gchar *name, PurplePrefType type,
						gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->highlight = active;

	if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_filter_all(win);
}

static void
regex_row_changed_cb(GtkTreeModel *model, GtkTreePath *path,
					 GtkTreeIter *iter, DebugWindow *win)
{
	gchar *text;
	PurpleDebugLevel level;

	if(!win || !win->window)
		return;

	/* If the debug window is paused, we just return since it's in the store.
	 * We don't call regex_match because it doesn't make sense to check the
	 * string if it's paused.  When we unpause we clear the imhtml and
	 * reiterate over the store to handle matches that were outputted when
	 * we were paused.
	 */
	if(win->paused)
		return;

	gtk_tree_model_get(model, iter, 0, &text, 1, &level, -1);

	if (level >= (PurpleDebugLevel)purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel")) {
		if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter))) {
			regex_match(win, text);
		} else {
			gtk_imhtml_append_text(GTK_IMHTML(win->text), text, 0);
		}
	}

	g_free(text);
}

static gboolean
regex_timer_cb(DebugWindow *win) {
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/debug/regex", text);

	win->timer = 0;

	return FALSE;
}

static void
regex_changed_cb(GtkWidget *w, DebugWindow *win) {
	if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter))) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter),
									 FALSE);
	}

	if(win->timer == 0)
		win->timer = purple_timeout_add_seconds(5, (GSourceFunc)regex_timer_cb, win);

	regex_compile(win);
}

static void
regex_key_release_cb(GtkWidget *w, GdkEventKey *e, DebugWindow *win) {
	if(e->keyval == GDK_Return &&
	   GTK_WIDGET_IS_SENSITIVE(win->filter) &&
	   !gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
	{
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter), TRUE);
	}
}

static void
regex_menu_cb(GtkWidget *item, const gchar *pref) {
	gboolean active;

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));

	purple_prefs_set_bool(pref, active);
}

static void
regex_popup_cb(GtkEntry *entry, GtkWidget *menu, DebugWindow *win) {
	pidgin_separator(menu);
	pidgin_new_check_item(menu, _("Invert"),
						G_CALLBACK(regex_menu_cb),
						PIDGIN_PREFS_ROOT "/debug/invert", win->invert);
	pidgin_new_check_item(menu, _("Highlight matches"),
						G_CALLBACK(regex_menu_cb),
						PIDGIN_PREFS_ROOT "/debug/highlight", win->highlight);
}

static void
regex_filter_toggled_cb(GtkToggleToolButton *button, DebugWindow *win) {
	gboolean active;

	active = gtk_toggle_tool_button_get_active(button);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/filter", active);

	if(!GTK_IS_IMHTML(win->text))
		return;

	if(active)
		regex_filter_all(win);
	else
		regex_show_all(win);
}

static void
filter_level_pref_changed(const char *name, PurplePrefType type, gconstpointer value, gpointer data)
{
	DebugWindow *win = data;

	if (GPOINTER_TO_INT(value) != gtk_combo_box_get_active(GTK_COMBO_BOX(win->filterlevel)))
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel), GPOINTER_TO_INT(value));
	if(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_filter_all(win);
	else
		regex_show_all(win);
}
#endif /* USE_REGEX */

static void
filter_level_changed_cb(GtkWidget *combo, gpointer null)
{
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/filterlevel",
				gtk_combo_box_get_active(GTK_COMBO_BOX(combo)));
}

static void
toolbar_style_pref_changed_cb(const char *name, PurplePrefType type, gconstpointer value, gpointer data)
{
	gtk_toolbar_set_style(GTK_TOOLBAR(data), GPOINTER_TO_INT(value));
}

static void
toolbar_icon_pref_changed(GtkWidget *item, GtkWidget *toolbar)
{
	int style = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "user_data"));
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/style", style);
}

static gboolean
toolbar_context(GtkWidget *toolbar, GdkEventButton *event, gpointer null)
{
	GtkWidget *menu, *item;
	const char *text[3];
	GtkToolbarStyle value[3];
	int i;

	if (!(event->button == 3 && event->type == GDK_BUTTON_PRESS))
		return FALSE;

	text[0] = _("_Icon Only");          value[0] = GTK_TOOLBAR_ICONS;
	text[1] = _("_Text Only");          value[1] = GTK_TOOLBAR_TEXT;
	text[2] = _("_Both Icon & Text");   value[2] = GTK_TOOLBAR_BOTH_HORIZ;

	menu = gtk_menu_new();

	for (i = 0; i < 3; i++) {
		item = gtk_check_menu_item_new_with_mnemonic(text[i]);
		g_object_set_data(G_OBJECT(item), "user_data", GINT_TO_POINTER(value[i]));
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(toolbar_icon_pref_changed), toolbar);
		if (value[i] == (GtkToolbarStyle)purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/style"))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	gtk_widget_show_all(menu);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	return FALSE;
}

static DebugWindow *
debug_window_new(void)
{
	DebugWindow *win;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *frame;
	gint width, height;
	void *handle;
	GtkToolItem *item;
#if !GTK_CHECK_VERSION(2,12,0)
	GtkTooltips *tooltips;
#endif

	win = g_new0(DebugWindow, 1);

	width  = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/width");
	height = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/height");

	win->window = pidgin_create_window(_("Debug Window"), 0, "debug", TRUE);
	purple_debug_info("gtkdebug", "Setting dimensions to %d, %d\n",
					width, height);

	gtk_window_set_default_size(GTK_WINDOW(win->window), width, height);

	g_signal_connect(G_OBJECT(win->window), "delete_event",
	                 G_CALLBACK(debug_window_destroy), NULL);
	g_signal_connect(G_OBJECT(win->window), "configure_event",
	                 G_CALLBACK(configure_cb), win);

	handle = pidgin_debug_get_handle();

#ifdef USE_REGEX
	/* the list store for all the messages */
	win->store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

	/* row-changed gets called when we do gtk_list_store_set, and row-inserted
	 * gets called with gtk_list_store_append, which is a
	 * completely empty row. So we just ignore row-inserted, and deal with row
	 * changed. -Gary
	 */
	g_signal_connect(G_OBJECT(win->store), "row-changed",
					 G_CALLBACK(regex_row_changed_cb), win);

#endif /* USE_REGEX */

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win->window), vbox);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/toolbar")) {
		/* Setup our top button bar thingie. */
		toolbar = gtk_toolbar_new();
#if !GTK_CHECK_VERSION(2,12,0)
		tooltips = gtk_tooltips_new();
#endif
#if !GTK_CHECK_VERSION(2,14,0)
		gtk_toolbar_set_tooltips(GTK_TOOLBAR(toolbar), TRUE);
#endif
		gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), TRUE);
		g_signal_connect(G_OBJECT(toolbar), "button-press-event", G_CALLBACK(toolbar_context), win);

		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),
		                      purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/style"));
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/style",
	                                toolbar_style_pref_changed_cb, toolbar);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
		                          GTK_ICON_SIZE_SMALL_TOOLBAR);

		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

#ifndef USE_REGEX
		/* Find button */
		item = gtk_tool_button_new_from_stock(GTK_STOCK_FIND);
		gtk_tool_item_set_is_important(item, TRUE);
#if GTK_CHECK_VERSION(2,12,0)
		gtk_tool_item_set_tooltip_text(item, _("Find"));
#else
		gtk_tool_item_set_tooltip(item, tooltips, _("Find"), NULL);
#endif
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(find_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));
#endif /* USE_REGEX */

		/* Save */
		item = gtk_tool_button_new_from_stock(GTK_STOCK_SAVE);
		gtk_tool_item_set_is_important(item, TRUE);
#if GTK_CHECK_VERSION(2,12,0)
		gtk_tool_item_set_tooltip_text(item, _("Save"));
#else
		gtk_tool_item_set_tooltip(item, tooltips, _("Save"), NULL);
#endif
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(save_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* Clear button */
		item = gtk_tool_button_new_from_stock(GTK_STOCK_CLEAR);
		gtk_tool_item_set_is_important(item, TRUE);
#if GTK_CHECK_VERSION(2,12,0)
		gtk_tool_item_set_tooltip_text(item, _("Clear"));
#else
		gtk_tool_item_set_tooltip(item, tooltips, _("Clear"), NULL);
#endif
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(clear_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		item = gtk_separator_tool_item_new();
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* Pause */
		item = gtk_toggle_tool_button_new_from_stock(PIDGIN_STOCK_PAUSE);
		gtk_tool_item_set_is_important(item, TRUE);
#if GTK_CHECK_VERSION(2,12,0)
		gtk_tool_item_set_tooltip_text(item, _("Pause"));
#else
		gtk_tool_item_set_tooltip(item, tooltips, _("Pause"), NULL);
#endif
		g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(pause_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

#ifdef USE_REGEX
		/* regex stuff */
		item = gtk_separator_tool_item_new();
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* regex toggle button */
		item = gtk_toggle_tool_button_new_from_stock(GTK_STOCK_FIND);
		gtk_tool_item_set_is_important(item, TRUE);
		win->filter = GTK_WIDGET(item);
		gtk_tool_button_set_label(GTK_TOOL_BUTTON(win->filter), _("Filter"));
#if GTK_CHECK_VERSION(2,12,0)
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(win->filter), _("Filter"));
#else
		gtk_tooltips_set_tip(tooltips, win->filter, _("Filter"), NULL);
#endif
		g_signal_connect(G_OBJECT(win->filter), "clicked", G_CALLBACK(regex_filter_toggled_cb), win);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(win->filter));

		/* we purposely disable the toggle button here in case
		 * /purple/gtk/debug/expression has an empty string.  If it does not have
		 * an empty string, the change signal will get called and make the
		 * toggle button sensitive.
		 */
		gtk_widget_set_sensitive(win->filter, FALSE);
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter),
									 purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/filter"));
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/filter",
									regex_pref_filter_cb, win);

		/* regex entry */
		win->expression = gtk_entry_new();
		item = gtk_tool_item_new();
#if GTK_CHECK_VERSION(2,12,0)
		gtk_widget_set_tooltip_text(win->expression, _("Right click for more options."));
#else
		gtk_tooltips_set_tip(tooltips, win->expression, _("Right click for more options."), NULL);
#endif
		gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(win->expression));
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		/* this needs to be before the text is set from the pref if we want it
		 * to colorize a stored expression.
		 */
		g_signal_connect(G_OBJECT(win->expression), "changed",
						 G_CALLBACK(regex_changed_cb), win);
		gtk_entry_set_text(GTK_ENTRY(win->expression),
						   purple_prefs_get_string(PIDGIN_PREFS_ROOT "/debug/regex"));
		g_signal_connect(G_OBJECT(win->expression), "populate-popup",
						 G_CALLBACK(regex_popup_cb), win);
		g_signal_connect(G_OBJECT(win->expression), "key-release-event",
						 G_CALLBACK(regex_key_release_cb), win);
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/regex",
									regex_pref_expression_cb, win);

		/* connect the rest of our pref callbacks */
		win->invert = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/invert");
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/invert",
									regex_pref_invert_cb, win);

		win->highlight = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/highlight");
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/highlight",
									regex_pref_highlight_cb, win);

#endif /* USE_REGEX */

		item = gtk_separator_tool_item_new();
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		item = gtk_tool_item_new();
		gtk_container_add(GTK_CONTAINER(item), gtk_label_new(_("Level ")));
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		win->filterlevel = gtk_combo_box_new_text();
		item = gtk_tool_item_new();
#if GTK_CHECK_VERSION(2,12,0)
		gtk_widget_set_tooltip_text(win->filterlevel, _("Select the debug filter level."));
#else
		gtk_tooltips_set_tip(tooltips, win->filterlevel, _("Select the debug filter level."), NULL);
#endif
		gtk_container_add(GTK_CONTAINER(item), win->filterlevel);
		gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(item));

		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("All"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Misc"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Info"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Warning"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Error "));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Fatal Error"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel),
					purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"));
#ifdef USE_REGEX
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/filterlevel",
						filter_level_pref_changed, win);
#endif
		g_signal_connect(G_OBJECT(win->filterlevel), "changed",
						 G_CALLBACK(filter_level_changed_cb), NULL);
	}

	/* Add the gtkimhtml */
	frame = pidgin_create_imhtml(FALSE, &win->text, NULL, NULL);
	gtk_imhtml_set_format_functions(GTK_IMHTML(win->text),
									GTK_IMHTML_ALL ^ GTK_IMHTML_SMILEY ^ GTK_IMHTML_IMAGE);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

#ifdef USE_REGEX
	/* add the tag for regex highlighting */
	gtk_text_buffer_create_tag(GTK_IMHTML(win->text)->text_buffer, "regex",
							   "background", "#FFAFAF",
							   "weight", "bold",
							   NULL);
#endif /* USE_REGEX */

	gtk_widget_show_all(win->window);

	return win;
}

static gboolean
debug_enabled_timeout_cb(gpointer data)
{
	debug_enabled_timer = 0;

	if (data)
		pidgin_debug_window_show();
	else
		pidgin_debug_window_hide();

	return FALSE;
}

static void
debug_enabled_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	debug_enabled_timer = g_timeout_add(0, debug_enabled_timeout_cb, GINT_TO_POINTER(GPOINTER_TO_INT(value)));
}

static void
pidgin_glib_log_handler(const gchar *domain, GLogLevelFlags flags,
					  const gchar *msg, gpointer user_data)
{
	PurpleDebugLevel level;
	char *new_msg = NULL;
	char *new_domain = NULL;

	if ((flags & G_LOG_LEVEL_ERROR) == G_LOG_LEVEL_ERROR)
		level = PURPLE_DEBUG_ERROR;
	else if ((flags & G_LOG_LEVEL_CRITICAL) == G_LOG_LEVEL_CRITICAL)
		level = PURPLE_DEBUG_FATAL;
	else if ((flags & G_LOG_LEVEL_WARNING) == G_LOG_LEVEL_WARNING)
		level = PURPLE_DEBUG_WARNING;
	else if ((flags & G_LOG_LEVEL_MESSAGE) == G_LOG_LEVEL_MESSAGE)
		level = PURPLE_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_INFO) == G_LOG_LEVEL_INFO)
		level = PURPLE_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_DEBUG) == G_LOG_LEVEL_DEBUG)
		level = PURPLE_DEBUG_MISC;
	else
	{
		purple_debug_warning("gtkdebug",
				   "Unknown glib logging level in %d\n", flags);

		level = PURPLE_DEBUG_MISC; /* This will never happen. */
	}

	if (msg != NULL)
		new_msg = purple_utf8_try_convert(msg);

	if (domain != NULL)
		new_domain = purple_utf8_try_convert(domain);

	if (new_msg != NULL)
	{
		purple_debug(level, (new_domain != NULL ? new_domain : "g_log"),
				   "%s\n", new_msg);

		g_free(new_msg);
	}

	g_free(new_domain);
}

#ifdef _WIN32
static void
pidgin_glib_dummy_print_handler(const gchar *string)
{
}
#endif

void
pidgin_debug_init(void)
{
	/* Debug window preferences. */
	/*
	 * NOTE: This must be set before prefs are loaded, and the callbacks
	 *       set after they are loaded, since prefs sets the enabled
	 *       preference here and that loads the window, which calls the
	 *       configure event, which overrides the width and height! :P
	 */

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/debug");

	/* Controls printing to the debug window */
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/enabled", FALSE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/filterlevel", PURPLE_DEBUG_ALL);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/style", GTK_TOOLBAR_BOTH_HORIZ);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/toolbar", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/width",  450);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/debug/height", 250);

#ifdef USE_REGEX
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/debug/regex", "");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/filter", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/invert", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/case_insensitive", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/highlight", FALSE);
#endif /* USE_REGEX */

	purple_prefs_connect_callback(NULL, PIDGIN_PREFS_ROOT "/debug/enabled",
								debug_enabled_cb, NULL);

#define REGISTER_G_LOG_HANDLER(name) \
	g_log_set_handler((name), G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL \
					  | G_LOG_FLAG_RECURSION, \
					  pidgin_glib_log_handler, NULL)

	/* Register the glib/gtk log handlers. */
	REGISTER_G_LOG_HANDLER(NULL);
	REGISTER_G_LOG_HANDLER("Gdk");
	REGISTER_G_LOG_HANDLER("Gtk");
	REGISTER_G_LOG_HANDLER("GdkPixbuf");
	REGISTER_G_LOG_HANDLER("GLib");
	REGISTER_G_LOG_HANDLER("GModule");
	REGISTER_G_LOG_HANDLER("GLib-GObject");
	REGISTER_G_LOG_HANDLER("GThread");
#ifdef USE_GSTREAMER
	REGISTER_G_LOG_HANDLER("GStreamer");
#endif

#ifdef _WIN32
	if (!purple_debug_is_enabled())
		g_set_print_handler(pidgin_glib_dummy_print_handler);
#endif
}

void
pidgin_debug_uninit(void)
{
	purple_debug_set_ui_ops(NULL);

	if (debug_enabled_timer != 0)
		g_source_remove(debug_enabled_timer);
}

void
pidgin_debug_window_show(void)
{
	if (debug_win == NULL)
		debug_win = debug_window_new();

	gtk_widget_show(debug_win->window);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", TRUE);
}

void
pidgin_debug_window_hide(void)
{
	if (debug_win != NULL) {
		gtk_widget_destroy(debug_win->window);
		debug_window_destroy(NULL, NULL, NULL);
	}
}

static void
pidgin_debug_print(PurpleDebugLevel level, const char *category,
					 const char *arg_s)
{
#ifdef USE_REGEX
	GtkTreeIter iter;
#endif /* USE_REGEX */
	gchar *ts_s;
	gchar *esc_s, *cat_s, *tmp, *s;
	const char *mdate;
	time_t mtime;

	if (debug_win == NULL ||
		!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"))
	{
		return;
	}

	mtime = time(NULL);
	mdate = purple_utf8_strftime("%H:%M:%S", localtime(&mtime));
	ts_s = g_strdup_printf("(%s) ", mdate);
	if (category == NULL)
		cat_s = g_strdup("");
	else
		cat_s = g_strdup_printf("<b>%s:</b> ", category);

	esc_s = g_markup_escape_text(arg_s, -1);

	s = g_strdup_printf("<font color=\"%s\">%s%s%s</font>",
						debug_fg_colors[level], ts_s, cat_s, esc_s);

	g_free(ts_s);
	g_free(cat_s);
	g_free(esc_s);

	tmp = purple_utf8_try_convert(s);
	g_free(s);
	s = tmp;

	if (level == PURPLE_DEBUG_FATAL) {
		tmp = g_strdup_printf("<b>%s</b>", s);
		g_free(s);
		s = tmp;
	}

#ifdef USE_REGEX
	/* add the text to the list store */
	gtk_list_store_append(debug_win->store, &iter);
	gtk_list_store_set(debug_win->store, &iter, 0, s, 1, level, -1);
#else /* USE_REGEX */
	if(!debug_win->paused && level >= purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"))
		gtk_imhtml_append_text(GTK_IMHTML(debug_win->text), s, 0);
#endif /* !USE_REGEX */

	g_free(s);
}

static gboolean
pidgin_debug_is_enabled(PurpleDebugLevel level, const char *category)
{
	return (debug_win != NULL &&
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"));
}

static PurpleDebugUiOps ops =
{
	pidgin_debug_print,
	pidgin_debug_is_enabled,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleDebugUiOps *
pidgin_debug_get_ui_ops(void)
{
	return &ops;
}

void *
pidgin_debug_get_handle() {
	static int handle;

	return &handle;
}
