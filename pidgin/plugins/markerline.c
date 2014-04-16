/*
 * Markerline - Draw a line to indicate new messages in a conversation.
 * Copyright (C) 2006
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"

#define PLUGIN_ID			"gtk-plugin_pack-markerline"
#define PLUGIN_NAME			N_("Markerline")
#define PLUGIN_CATEGORY		N_("User interface")
#define PLUGIN_STATIC_NAME	Markerline
#define PLUGIN_SUMMARY		N_("Draw a line to indicate new messages in a conversation.")
#define PLUGIN_DESCRIPTION	N_("Draw a line to indicate new messages in a conversation.")
#define PLUGIN_AUTHORS		{"Sadrul H Chowdhury <sadrul@users.sourceforge.net>", NULL}

/* System headers */
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>

/* Purple headers */
#include <gtkconv.h>
#include <gtkplugin.h>
#include <gtkwebview.h>
#include <version.h>

#define PREF_PREFIX     "/plugins/gtk/" PLUGIN_ID
#define PREF_IMS        PREF_PREFIX "/ims"
#define PREF_CHATS      PREF_PREFIX "/chats"

static void
update_marker_for_gtkconv(PidginConversation *gtkconv)
{
	PurpleConversation *conv;

	g_return_if_fail(gtkconv != NULL);

	conv = gtkconv->active_conv;

	if ((PURPLE_IS_CHAT_CONVERSATION(conv) && !purple_prefs_get_bool(PREF_CHATS)) ||
	    (PURPLE_IS_IM_CONVERSATION(conv) && !purple_prefs_get_bool(PREF_IMS)))
		return;

	pidgin_webview_safe_execute_script(PIDGIN_WEBVIEW(gtkconv->webview),
		"var mhr = document.getElementById(\"markerhr\");"
		"if (!mhr) {"
			"mhr = document.createElement(\"hr\");"
			"mhr.setAttribute(\"id\", \"markerhr\");"
			"mhr.setAttribute(\"color\", \"#ff0000\");"
			"mhr.setAttribute(\"size\", \"1\");"
		"}"
		"document.getElementById(\"Chat\").appendChild(mhr);");
}

static gboolean
focus_removed(GtkWidget *widget, GdkEventVisibility *event, PidginConvWindow *win)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = pidgin_conv_window_get_active_conversation(win);
	g_return_val_if_fail(conv != NULL, FALSE);

	gtkconv = PIDGIN_CONVERSATION(conv);
	update_marker_for_gtkconv(gtkconv);

	return FALSE;
}

static void
page_switched(GtkWidget *widget, GtkWidget *page, gint num, PidginConvWindow *win)
{
	focus_removed(NULL, NULL, win);
}

static void
detach_from_gtkconv(PidginConversation *gtkconv, gpointer null)
{
	pidgin_webview_safe_execute_script(PIDGIN_WEBVIEW(gtkconv->webview),
		"var mhr = document.getElementById(\"markerhr\");"
		"if (mhr) mhr.parentNode.removeChild(mhr);");
}

static void
detach_from_pidgin_window(PidginConvWindow *win, gpointer null)
{
	g_list_foreach(pidgin_conv_window_get_gtkconvs(win), (GFunc)detach_from_gtkconv, NULL);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->notebook), page_switched, win);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->window), focus_removed, win);
}

static void
attach_to_gtkconv(PidginConversation *gtkconv, gpointer null)
{
	detach_from_gtkconv(gtkconv, NULL);
	update_marker_for_gtkconv(gtkconv);
}

static void
attach_to_pidgin_window(PidginConvWindow *win, gpointer null)
{
	g_list_foreach(pidgin_conv_window_get_gtkconvs(win), (GFunc)attach_to_gtkconv, NULL);

	g_signal_connect(G_OBJECT(win->window), "focus_out_event",
					 G_CALLBACK(focus_removed), win);

	g_signal_connect(G_OBJECT(win->notebook), "switch_page",
					G_CALLBACK(page_switched), win);
}

static void
detach_from_all_windows(void)
{
	g_list_foreach(pidgin_conv_windows_get_list(), (GFunc)detach_from_pidgin_window, NULL);
}

static void
attach_to_all_windows(void)
{
	g_list_foreach(pidgin_conv_windows_get_list(), (GFunc)attach_to_pidgin_window, NULL);
}

static void
conv_created(PidginConversation *gtkconv, gpointer null)
{
	PidginConvWindow *win;

	win = pidgin_conv_get_window(gtkconv);
	if (!win)
		return;

	detach_from_pidgin_window(win, NULL);
	attach_to_pidgin_window(win, NULL);
}

static void
jump_to_markerline(PurpleConversation *conv, gpointer null)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	if (!gtkconv)
		return;

	pidgin_webview_safe_execute_script(PIDGIN_WEBVIEW(gtkconv->webview),
		"var mhr = document.getElementById(\"markerhr\");"
		"if (mhr) {"
			"window.scroll(0, mhr.offsetTop);"
		"}");
}

static void
conv_menu_cb(PurpleConversation *conv, GList **list)
{
	gboolean enabled = ((PURPLE_IS_IM_CONVERSATION(conv) && purple_prefs_get_bool(PREF_IMS)) ||
		(PURPLE_IS_CHAT_CONVERSATION(conv) && purple_prefs_get_bool(PREF_CHATS)));
	PurpleMenuAction *action = purple_menu_action_new(_("Jump to markerline"),
			enabled ? PURPLE_CALLBACK(jump_to_markerline) : NULL, NULL, NULL);
	*list = g_list_append(*list, action);
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	pref = purple_plugin_pref_new_with_label(_("Draw Markerline in "));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_IMS,
					_("_IM windows"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_CHATS,
					_("C_hat windows"));
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = PLUGIN_AUTHORS;

	return pidgin_plugin_info_new(
		"id",             PLUGIN_ID,
		"name",           PLUGIN_NAME,
		"version",        DISPLAY_VERSION,
		"category",       PLUGIN_CATEGORY,
		"summary",        PLUGIN_SUMMARY,
		"description",    PLUGIN_DESCRIPTION,
		"authors",        authors,
		"website",        PURPLE_WEBSITE,
		"abi-version",    PURPLE_ABI_VERSION,
		"pref-frame-cb",  get_plugin_pref_frame,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_prefs_add_none(PREF_PREFIX);
	purple_prefs_add_bool(PREF_IMS, FALSE);
	purple_prefs_add_bool(PREF_CHATS, TRUE);

	attach_to_all_windows();

	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-displayed",
						plugin, PURPLE_CALLBACK(conv_created), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "conversation-extended-menu",
						plugin, PURPLE_CALLBACK(conv_menu_cb), NULL);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	detach_from_all_windows();

	return TRUE;
}

PURPLE_PLUGIN_INIT(PLUGIN_STATIC_NAME, plugin_query, plugin_load, plugin_unload);
