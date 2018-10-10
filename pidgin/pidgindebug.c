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

#include "gtkdialogs.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidgindebug.h"
#include "pidginstock.h"

#ifdef ENABLE_GLIBTRACE
#include <execinfo.h>
#endif

#include <gdk/gdkkeysyms.h>

#include "gtk3compat.h"

#include "pidginresources.h"

struct _PidginDebugWindow {
	GtkWindow parent;

	GtkWidget *toolbar;
	GtkWidget *textview;
	GtkTextBuffer *buffer;
	GtkTextMark *start_mark;
	GtkTextMark *end_mark;
	struct {
		GtkTextTag *level[PURPLE_DEBUG_FATAL + 1];
		GtkTextTag *category;
		GtkTextTag *filtered_invisible;
		GtkTextTag *filtered_visible;
		GtkTextTag *match;
		GtkTextTag *paused;
	} tags;
	GtkWidget *filter;
	GtkWidget *expression;
	GtkWidget *filterlevel;

	gboolean paused;

#if GTK_CHECK_VERSION(3,12,0)
	GtkWidget *popover;
#endif
	gboolean invert;
	gboolean highlight;
	GRegex *regex;
};

static PidginDebugWindow *debug_win = NULL;

struct _PidginDebugUi
{
	GObject parent;

	/* Other members, including private data. */
	guint debug_enabled_timer;
};

static void pidgin_debug_ui_finalize(GObject *gobject);
static void pidgin_debug_ui_interface_init(PurpleDebugUiInterface *iface);

G_DEFINE_TYPE_WITH_CODE(PidginDebugUi, pidgin_debug_ui, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(PURPLE_TYPE_DEBUG_UI,
                                              pidgin_debug_ui_interface_init));
G_DEFINE_TYPE(PidginDebugWindow, pidgin_debug_window, GTK_TYPE_WINDOW);

static gint
debug_window_destroy(GtkWidget *w, GdkEvent *event, void *unused)
{
	purple_prefs_disconnect_by_handle(pidgin_debug_get_handle());

	if (debug_win->regex != NULL)
		g_regex_unref(debug_win->regex);

	/* If the "Save Log" dialog is open then close it */
	purple_request_close_with_handle(debug_win);

	debug_win = NULL;

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", FALSE);

	return FALSE;
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *event, void *unused)
{
	if (gtk_widget_get_visible(w)) {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/debug/height", event->height);
	}

	return FALSE;
}

static gboolean
view_near_bottom(PidginDebugWindow *win)
{
	GtkAdjustment *adj = gtk_scrollable_get_vadjustment(
			GTK_SCROLLABLE(win->textview));
	return (gtk_adjustment_get_value(adj) >=
			(gtk_adjustment_get_upper(adj) -
			 gtk_adjustment_get_page_size(adj) * 1.5));
}

static void
save_writefile_cb(void *user_data, const char *filename)
{
	PidginDebugWindow *win = (PidginDebugWindow *)user_data;
	FILE *fp;
	GtkTextIter start, end;
	char *tmp;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		purple_notify_error(win, NULL, _("Unable to open file."), NULL, NULL);
		return;
	}

	gtk_text_buffer_get_bounds(win->buffer, &start, &end);
	tmp = gtk_text_buffer_get_text(win->buffer, &start, &end, TRUE);
	fprintf(fp, "Pidgin Debug Log : %s\n", purple_date_format_full(NULL));
	fprintf(fp, "%s", tmp);
	g_free(tmp);

	fclose(fp);
}

static void
save_cb(GtkWidget *w, PidginDebugWindow *win)
{
	purple_request_file(win, _("Save Debug Log"), "purple-debug.log", TRUE,
		G_CALLBACK(save_writefile_cb), NULL, NULL, win);
}

static void
clear_cb(GtkWidget *w, PidginDebugWindow *win)
{
	gtk_text_buffer_set_text(win->buffer, "", 0);
}

static void
pause_cb(GtkWidget *w, PidginDebugWindow *win)
{
	win->paused = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w));

	if (!win->paused) {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds(win->buffer, &start, &end);
		gtk_text_buffer_remove_tag(win->buffer, win->tags.paused,
				&start, &end);
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(win->textview),
				win->end_mark, 0, TRUE, 0, 1);
	}
}

/******************************************************************************
 * regex stuff
 *****************************************************************************/
static void
regex_clear_color(GtkWidget *w) {
	GtkStyleContext *context = gtk_widget_get_style_context(w);
	gtk_style_context_remove_class(context, "good-filter");
	gtk_style_context_remove_class(context, "bad-filter");
}

static void
regex_change_color(GtkWidget *w, gboolean success) {
	GtkStyleContext *context = gtk_widget_get_style_context(w);

	if (success) {
		gtk_style_context_add_class(context, "good-filter");
		gtk_style_context_remove_class(context, "bad-filter");
	} else {
		gtk_style_context_add_class(context, "bad-filter");
		gtk_style_context_remove_class(context, "good-filter");
	}
}

static void
do_regex(PidginDebugWindow *win, GtkTextIter *start, GtkTextIter *end)
{
	GError *error = NULL;
	GMatchInfo *match;
	gint initial_position;
	gint start_pos, end_pos;
	GtkTextIter match_start, match_end;
	gchar *text;

	if (!win->regex)
		return;

	initial_position = gtk_text_iter_get_offset(start);

	if (!win->invert) {
		/* First hide everything. */
		gtk_text_buffer_apply_tag(win->buffer,
				win->tags.filtered_invisible, start, end);
	}

	text = gtk_text_buffer_get_text(win->buffer, start, end, TRUE);
	g_regex_match(win->regex, text, 0, &match);
	while (g_match_info_matches(match)) {
		g_match_info_fetch_pos(match, 0, &start_pos, &end_pos);
		start_pos += initial_position;
		end_pos += initial_position;

		/* Expand match to full line of message. */
		gtk_text_buffer_get_iter_at_offset(win->buffer,
				&match_start, start_pos);
		gtk_text_iter_set_line_index(&match_start, 0);
		gtk_text_buffer_get_iter_at_offset(win->buffer,
				&match_end, end_pos);
		gtk_text_iter_forward_line(&match_end);

		if (win->invert) {
			/* Make invisible. */
			gtk_text_buffer_apply_tag(win->buffer,
					win->tags.filtered_invisible,
					&match_start, &match_end);
		} else {
			/* Make visible again (with higher priority.) */
			gtk_text_buffer_apply_tag(win->buffer,
					win->tags.filtered_visible,
					&match_start, &match_end);

			if (win->highlight) {
				gtk_text_buffer_get_iter_at_offset(
						win->buffer,
						&match_start,
						start_pos);
				gtk_text_buffer_get_iter_at_offset(
						win->buffer,
						&match_end,
						end_pos);
				gtk_text_buffer_apply_tag(win->buffer,
						win->tags.match,
						&match_start,
						&match_end);
			}
		}

		g_match_info_next(match, &error);
	}

	g_match_info_free(match);
	g_free(text);
}

static void
regex_toggle_filter(PidginDebugWindow *win, gboolean filter)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(win->buffer, &start, &end);
	gtk_text_buffer_remove_tag(win->buffer, win->tags.match, &start, &end);
	gtk_text_buffer_remove_tag(win->buffer, win->tags.filtered_invisible,
			&start, &end);
	gtk_text_buffer_remove_tag(win->buffer, win->tags.filtered_visible,
			&start, &end);

	if (filter) {
		do_regex(win, &start, &end);
	}
}

static void
regex_pref_filter_cb(const gchar *name, PurplePrefType type,
					 gconstpointer val, gpointer data)
{
	PidginDebugWindow *win = (PidginDebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val), current;

	if (!win)
		return;

	current = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter));
	if (active != current)
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter), active);
}

static void
regex_pref_expression_cb(const gchar *name, PurplePrefType type,
						 gconstpointer val, gpointer data)
{
	PidginDebugWindow *win = (PidginDebugWindow *)data;
	const gchar *exp = (const gchar *)val;

	gtk_entry_set_text(GTK_ENTRY(win->expression), exp);
}

static void
regex_pref_invert_cb(const gchar *name, PurplePrefType type,
					 gconstpointer val, gpointer data)
{
	PidginDebugWindow *win = (PidginDebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->invert = active;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_toggle_filter(win, TRUE);
}

static void
regex_pref_highlight_cb(const gchar *name, PurplePrefType type,
						gconstpointer val, gpointer data)
{
	PidginDebugWindow *win = (PidginDebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->highlight = active;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter)))
		regex_toggle_filter(win, TRUE);
}

static void
regex_changed_cb(GtkWidget *w, PidginDebugWindow *win) {
	const gchar *text;

	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(win->filter))) {
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(win->filter),
									 FALSE);
	}

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/debug/regex", text);

	if (text == NULL || *text == '\0') {
		regex_clear_color(win->expression);
		gtk_widget_set_sensitive(win->filter, FALSE);
		return;
	}

	if (win->regex)
		g_regex_unref(win->regex);

	win->regex = g_regex_new(text, G_REGEX_CASELESS|G_REGEX_JAVASCRIPT_COMPAT, 0, NULL);

	if (win->regex == NULL) {
		/* failed to compile */
		regex_change_color(win->expression, FALSE);
		gtk_widget_set_sensitive(win->filter, FALSE);
	} else {
		/* compiled successfully */
		regex_change_color(win->expression, TRUE);
		gtk_widget_set_sensitive(win->filter, TRUE);
	}
}

static void
regex_key_release_cb(GtkWidget *w, GdkEventKey *e, PidginDebugWindow *win) {
	if (gtk_widget_is_sensitive(win->filter)) {
		GtkToggleToolButton *tb = GTK_TOGGLE_TOOL_BUTTON(win->filter);
		if ((e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter) &&
			!gtk_toggle_tool_button_get_active(tb))
		{
			gtk_toggle_tool_button_set_active(tb, TRUE);
		}
		if (e->keyval == GDK_KEY_Escape &&
			gtk_toggle_tool_button_get_active(tb))
		{
			gtk_toggle_tool_button_set_active(tb, FALSE);
		}
	}
}

static void
regex_menu_cb(GtkWidget *item, const gchar *pref) {
	gboolean active;

#if GTK_CHECK_VERSION(3,12,0)
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item));
#else
	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
#endif

	purple_prefs_set_bool(pref, active);
}

static void
regex_popup_cb(GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event,
		PidginDebugWindow *win)
{
#if GTK_CHECK_VERSION(3,12,0)
	GdkRectangle rect;
	gtk_entry_get_icon_area(entry, icon_pos, &rect);
	gtk_popover_set_pointing_to(GTK_POPOVER(win->popover), &rect);
#if GTK_CHECK_VERSION(3,22,0)
	gtk_popover_popup(GTK_POPOVER(win->popover));
#else
	gtk_widget_show(win->popover);
#endif
#else
	GtkWidget *menu;

	menu = gtk_menu_new();
	pidgin_new_check_item(menu, _("Invert"),
						G_CALLBACK(regex_menu_cb),
						PIDGIN_PREFS_ROOT "/debug/invert", win->invert);
	pidgin_new_check_item(menu, _("Highlight matches"),
						G_CALLBACK(regex_menu_cb),
						PIDGIN_PREFS_ROOT "/debug/highlight", win->highlight);

#if GTK_CHECK_VERSION(3,22,0)
	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(entry),
			GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, event);
#else
	gtk_menu_popup_at_pointer(GTK_MENU(menu), event);
#endif
#endif
}

static void
regex_filter_toggled_cb(GtkToggleToolButton *button, PidginDebugWindow *win)
{
	gboolean active;

	active = gtk_toggle_tool_button_get_active(button);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/filter", active);

	regex_toggle_filter(win, active);
}

static void
debug_window_set_filter_level(PidginDebugWindow *win, int level)
{
	gboolean scroll;
	int i;

	if (level != gtk_combo_box_get_active(GTK_COMBO_BOX(win->filterlevel)))
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel), level);

	scroll = view_near_bottom(win);
	for (i = 0; i <= PURPLE_DEBUG_FATAL; i++) {
		g_object_set(G_OBJECT(win->tags.level[i]),
				"invisible", i < level,
				NULL);
	}
	if (scroll) {
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(win->textview),
				win->end_mark, 0, TRUE, 0, 1);
	}
}

static void
filter_level_pref_changed(const char *name, PurplePrefType type, gconstpointer value, gpointer data)
{
	PidginDebugWindow *win = data;
	int level = GPOINTER_TO_INT(value);

	debug_window_set_filter_level(win, level);
}

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
toolbar_context(GtkWidget *toolbar, gint x, gint y, gint button, gpointer null)
{
	GtkWidget *menu, *item;
	const char *text[3];
	GtkToolbarStyle value[3];
	int i;

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

	gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
	return FALSE;
}

static void
pidgin_debug_window_class_init(PidginDebugWindowClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/Debug/debug.ui"
	);

	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, toolbar);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, textview);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, buffer);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.category);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.filtered_invisible);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.filtered_visible);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.level[0]);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.level[1]);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.level[2]);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.level[3]);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.level[4]);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.level[5]);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.paused);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, filter);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, filterlevel);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, expression);
	gtk_widget_class_bind_template_child(
			widget_class, PidginDebugWindow, tags.match);
	gtk_widget_class_bind_template_callback(widget_class, toolbar_context);
	gtk_widget_class_bind_template_callback(widget_class, save_cb);
	gtk_widget_class_bind_template_callback(widget_class, clear_cb);
	gtk_widget_class_bind_template_callback(widget_class, pause_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			regex_filter_toggled_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			regex_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, regex_popup_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			regex_key_release_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			filter_level_changed_cb);
}

static void
pidgin_debug_window_init(PidginDebugWindow *win)
{
	gint width, height;
	void *handle;
	GtkTextIter end;
	GtkStyleContext *context;
	GtkCssProvider *filter_css;
	const gchar filter_style[] =
		".bad-filter {"
			"color: @error_fg_color;"
			"text-shadow: 0 1px @error_text_shadow;"
			"background-image: none;"
			"background-color: @error_bg_color;"
		"}"
		".good-filter {"
			"color: @question_fg_color;"
			"text-shadow: 0 1px @question_text_shadow;"
			"background-image: none;"
			"background-color: @success_color;"
		"}";
#if GTK_CHECK_VERSION(3,12,0)
	GtkBuilder *builder;
	GtkWidget *popover_invert;
	GtkWidget *popover_highlight;
#endif

	gtk_widget_init_template(GTK_WIDGET(win));

	width  = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/width");
	height = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/height");

	purple_debug_info("gtkdebug", "Setting dimensions to %d, %d\n",
					width, height);

	gtk_window_set_default_size(GTK_WINDOW(win), width, height);

	g_signal_connect(G_OBJECT(win), "delete_event",
	                 G_CALLBACK(debug_window_destroy), NULL);
	g_signal_connect(G_OBJECT(win), "configure_event",
	                 G_CALLBACK(configure_cb), NULL);

	handle = pidgin_debug_get_handle();

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/toolbar")) {
		/* Setup our top button bar thingie. */
		gtk_toolbar_set_style(GTK_TOOLBAR(win->toolbar),
		                      purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/style"));
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/style",
	                                toolbar_style_pref_changed_cb, win->toolbar);

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
		filter_css = gtk_css_provider_new();
		gtk_css_provider_load_from_data(filter_css, filter_style, -1, NULL);
		context = gtk_widget_get_style_context(win->expression);
		gtk_style_context_add_provider(context,
		                               GTK_STYLE_PROVIDER(filter_css),
		                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

		gtk_entry_set_text(GTK_ENTRY(win->expression),
						   purple_prefs_get_string(PIDGIN_PREFS_ROOT "/debug/regex"));
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/regex",
									regex_pref_expression_cb, win);

		/* connect the rest of our pref callbacks */
		win->invert = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/invert");
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/invert",
									regex_pref_invert_cb, win);

		win->highlight = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/highlight");
		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/highlight",
									regex_pref_highlight_cb, win);

		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel),
					purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"));

		purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/debug/filterlevel",
						filter_level_pref_changed, win);

#if GTK_CHECK_VERSION(3,12,0)
		builder = gtk_builder_new_from_resource(
				"/im/pidgin/Pidgin/Debug/filter-popover.ui");
		win->popover = GTK_WIDGET(gtk_builder_get_object(builder,
					"popover"));
		gtk_popover_set_relative_to(GTK_POPOVER(win->popover),
				win->expression);

		popover_invert = GTK_WIDGET(gtk_builder_get_object(builder,
					"popover.invert"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popover_invert),
				win->invert);
		g_signal_connect(G_OBJECT(popover_invert), "toggled",
				G_CALLBACK(regex_menu_cb),
				PIDGIN_PREFS_ROOT "/debug/invert");

		popover_highlight = GTK_WIDGET(gtk_builder_get_object(builder,
					"popover.highlight"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(popover_highlight),
				win->highlight);
		g_signal_connect(G_OBJECT(popover_highlight), "toggled",
				G_CALLBACK(regex_menu_cb),
				PIDGIN_PREFS_ROOT "/debug/highlight");

		g_object_unref(builder);
#endif
	}

	/* The *start* and *end* marks bound the beginning and end of an
	   insertion, used for filtering. The *end* mark is also used for
	   auto-scrolling. */
	gtk_text_buffer_get_end_iter(win->buffer, &end);
	win->start_mark = gtk_text_buffer_create_mark(win->buffer,
			"start", &end, TRUE);
	win->end_mark = gtk_text_buffer_create_mark(win->buffer,
			"end", &end, FALSE);

	/* Set active filter level in textview */
	debug_window_set_filter_level(win,
			purple_prefs_get_int(PIDGIN_PREFS_ROOT "/debug/filterlevel"));

	clear_cb(NULL, win);
}

static gboolean
debug_enabled_timeout_cb(gpointer data)
{
	PidginDebugUi *ui = PIDGIN_DEBUG_UI(data);

	ui->debug_enabled_timer = 0;

	pidgin_debug_window_show();

	return FALSE;
}

static gboolean
debug_disabled_timeout_cb(gpointer data)
{
	PidginDebugUi *ui = PIDGIN_DEBUG_UI(data);

	ui->debug_enabled_timer = 0;

	pidgin_debug_window_hide();

	return FALSE;
}

static void
debug_enabled_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	PidginDebugUi *ui = PIDGIN_DEBUG_UI(data);

	if (GPOINTER_TO_INT(value))
		ui->debug_enabled_timer = g_timeout_add(0, debug_enabled_timeout_cb, data);
	else
		ui->debug_enabled_timer = g_timeout_add(0, debug_disabled_timeout_cb, data);
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
#ifdef ENABLE_GLIBTRACE
		void *bt_buff[20];
		size_t bt_size;

		bt_size = backtrace(bt_buff, 20);
		fprintf(stderr, "\nBacktrace for \"%s\" (%s):\n", new_msg,
			new_domain != NULL ? new_domain : "g_log");
		backtrace_symbols_fd(bt_buff, bt_size, STDERR_FILENO);
		fprintf(stderr, "\n");
#endif

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

static void
pidgin_debug_ui_init(PidginDebugUi *self)
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

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/debug/regex", "");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/filter", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/invert", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/case_insensitive", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/debug/highlight", FALSE);

	purple_prefs_connect_callback(NULL, PIDGIN_PREFS_ROOT "/debug/enabled",
	                              debug_enabled_cb, self);

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
	REGISTER_G_LOG_HANDLER("Json");
#ifdef USE_GSTREAMER
	REGISTER_G_LOG_HANDLER("GStreamer");
#endif

#ifdef _WIN32
	if (!purple_debug_is_enabled())
		g_set_print_handler(pidgin_glib_dummy_print_handler);
#endif
}

static void
pidgin_debug_ui_finalize(GObject *gobject)
{
	PidginDebugUi *self = PIDGIN_DEBUG_UI(gobject);

	if (self->debug_enabled_timer != 0)
		g_source_remove(self->debug_enabled_timer);
	self->debug_enabled_timer = 0;

	G_OBJECT_CLASS(pidgin_debug_ui_parent_class)->finalize(gobject);
}

void
pidgin_debug_window_show(void)
{
	if (debug_win == NULL) {
		debug_win = PIDGIN_DEBUG_WINDOW(
				g_object_new(PIDGIN_TYPE_DEBUG_WINDOW, NULL));
	}

	gtk_widget_show(GTK_WIDGET(debug_win));

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", TRUE);
}

void
pidgin_debug_window_hide(void)
{
	if (debug_win != NULL) {
		gtk_widget_destroy(GTK_WIDGET(debug_win));
		debug_window_destroy(NULL, NULL, NULL);
	}
}

static void
pidgin_debug_print(PurpleDebugUi *self,
                   PurpleDebugLevel level, const char *category,
                   const char *arg_s)
{
	GtkTextTag *level_tag;
	const char *mdate;
	time_t mtime;
	GtkTextIter end;
	gboolean scroll;

	if (debug_win == NULL)
		return;
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"))
		return;

	scroll = view_near_bottom(debug_win);
	gtk_text_buffer_get_end_iter(debug_win->buffer, &end);
	gtk_text_buffer_move_mark(debug_win->buffer, debug_win->start_mark,
			&end);

	level_tag = debug_win->tags.level[level];

	mtime = time(NULL);
	mdate = purple_utf8_strftime("(%H:%M:%S) ", localtime(&mtime));
	gtk_text_buffer_insert_with_tags(
			debug_win->buffer,
			&end,
			mdate,
			-1,
			level_tag,
			debug_win->paused ? debug_win->tags.paused : NULL,
			NULL);

	if (category && *category) {
		gtk_text_buffer_insert_with_tags(
				debug_win->buffer,
				&end,
				category,
				-1,
				level_tag,
				debug_win->tags.category,
				debug_win->paused ? debug_win->tags.paused : NULL,
				NULL);
		gtk_text_buffer_insert_with_tags(
				debug_win->buffer,
				&end,
				": ",
				2,
				level_tag,
				debug_win->tags.category,
				debug_win->paused ? debug_win->tags.paused : NULL,
				NULL);
	}

	gtk_text_buffer_insert_with_tags(
			debug_win->buffer,
			&end,
			arg_s,
			-1,
			level_tag,
			debug_win->paused ? debug_win->tags.paused : NULL,
			NULL);
	gtk_text_buffer_insert_with_tags(
			debug_win->buffer,
			&end,
			"\n",
			1,
			level_tag,
			debug_win->paused ? debug_win->tags.paused : NULL,
			NULL);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/filter") && debug_win->regex) {
		/* Filter out any new messages. */
		GtkTextIter start;

		gtk_text_buffer_get_iter_at_mark(debug_win->buffer, &start,
				debug_win->start_mark);
		gtk_text_buffer_get_iter_at_mark(debug_win->buffer, &end,
				debug_win->end_mark);

		do_regex(debug_win, &start, &end);
	}

	if (scroll) {
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(debug_win->textview),
				debug_win->end_mark, 0, TRUE, 0, 1);
	}
}

static gboolean
pidgin_debug_is_enabled(PurpleDebugUi *self, PurpleDebugLevel level, const char *category)
{
	return (debug_win != NULL &&
			purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"));
}

static void
pidgin_debug_ui_interface_init(PurpleDebugUiInterface *iface)
{
	iface->print = pidgin_debug_print;
	iface->is_enabled = pidgin_debug_is_enabled;
}

static void
pidgin_debug_ui_class_init(PidginDebugUiClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = pidgin_debug_ui_finalize;
}

PidginDebugUi *
pidgin_debug_ui_new(void)
{
	return g_object_new(PIDGIN_TYPE_DEBUG_UI, NULL);
}

void *
pidgin_debug_get_handle() {
	static int handle;

	return &handle;
}

