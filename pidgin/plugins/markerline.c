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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include "internal.h"

#define PLUGIN_ID			"gtk-plugin_pack-markerline"
#define PLUGIN_NAME			N_("Markerline")
#define PLUGIN_STATIC_NAME	"Markerline"
#define PLUGIN_SUMMARY		N_("Draw a line to indicate new messages in a conversation.")
#define PLUGIN_DESCRIPTION	N_("Draw a line to indicate new messages in a conversation.")
#define PLUGIN_AUTHOR		"Sadrul H Chowdhury <sadrul@users.sourceforge.net>"

/* System headers */
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>

/* Gaim headers */
#include <gtkconv.h>
#include <gtkimhtml.h>
#include <gtkplugin.h>
#include <version.h>

#define PREF_PREFIX     "/plugins/gtk/" PLUGIN_ID
#define PREF_IMS        PREF_PREFIX "/ims"
#define PREF_CHATS      PREF_PREFIX "/chats"

static int
imhtml_expose_cb(GtkWidget *widget, GdkEventExpose *event, PidginConversation *gtkconv)
{
	int y, last_y, offset;
	GdkRectangle visible_rect;
	GtkTextIter iter;
	GdkRectangle buf;
	int pad;
	GaimConversation *conv = gtkconv->active_conv;
	GaimConversationType type = gaim_conversation_get_type(conv);

	if ((type == GAIM_CONV_TYPE_CHAT && !gaim_prefs_get_bool(PREF_CHATS)) ||
			(type == GAIM_CONV_TYPE_IM && !gaim_prefs_get_bool(PREF_IMS)))
		return FALSE;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(widget), &visible_rect);

	offset = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "markerline"));
	if (offset)
	{
		gtk_text_buffer_get_iter_at_offset(gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget)),
							&iter, offset);

		gtk_text_view_get_iter_location(GTK_TEXT_VIEW(widget), &iter, &buf);
		last_y = buf.y + buf.height;
		pad = (gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(widget)) + 
				gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(widget))) / 2;
		last_y += pad;
	}
	else
		last_y = 0;

	gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(widget), GTK_TEXT_WINDOW_TEXT,
										0, last_y, 0, &y);

	if (y >= event->area.y)
	{
		GdkColor red = {0, 0xffff, 0, 0};
		GdkGC *gc = gdk_gc_new(GDK_DRAWABLE(event->window));

		gdk_gc_set_rgb_fg_color(gc, &red);
		gdk_draw_line(event->window, gc,
					0, y, visible_rect.width, y);
		gdk_gc_unref(gc);
	}
	return FALSE;
}

static void
update_marker_for_gtkconv(PidginConversation *gtkconv)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;
	g_return_if_fail(gtkconv != NULL);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));

	if (!gtk_text_buffer_get_char_count(buffer))
		return;

	gtk_text_buffer_get_end_iter(buffer, &iter);

	g_object_set_data(G_OBJECT(gtkconv->imhtml), "markerline",
						GINT_TO_POINTER(gtk_text_iter_get_offset(&iter)));
	gtk_widget_queue_draw(gtkconv->imhtml);
}

static gboolean
focus_removed(GtkWidget *widget, GdkEventVisibility *event, PidginWindow *win)
{
	GaimConversation *conv;
	PidginConversation *gtkconv;

	conv = pidgin_conv_window_get_active_conversation(win);
	g_return_val_if_fail(conv != NULL, FALSE);

	gtkconv = PIDGIN_CONVERSATION(conv);
	update_marker_for_gtkconv(gtkconv);

	return FALSE;
}

#if 0
static gboolean
window_resized(GtkWidget *w, GdkEventConfigure *event, PidginWindow *win)
{
	GList *list;

	list = pidgin_conv_window_get_gtkconvs(win);
	
	for (; list; list = list->next)
		update_marker_for_gtkconv(list->data);

	return FALSE;
}

static gboolean
imhtml_resize_cb(GtkWidget *w, GtkAllocation *allocation, PidginConversation *gtkconv)
{
	gtk_widget_queue_draw(w);
	return FALSE;
}
#endif

static void
page_switched(GtkWidget *widget, GtkWidget *page, gint num, PidginWindow *win)
{
	focus_removed(NULL, NULL, win);
}

static void
detach_from_gtkconv(PidginConversation *gtkconv, gpointer null)
{
	g_signal_handlers_disconnect_by_func(G_OBJECT(gtkconv->imhtml), imhtml_expose_cb, gtkconv);
}

static void
detach_from_pidgin_window(PidginWindow *win, gpointer null)
{
	g_list_foreach(pidgin_conv_window_get_gtkconvs(win), (GFunc)detach_from_gtkconv, NULL);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->notebook), page_switched, win);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->window), focus_removed, win);

	gtk_widget_queue_draw(win->window);
}

static void
attach_to_gtkconv(PidginConversation *gtkconv, gpointer null)
{
	detach_from_gtkconv(gtkconv, NULL);
	g_signal_connect(G_OBJECT(gtkconv->imhtml), "expose_event",
					 G_CALLBACK(imhtml_expose_cb), gtkconv);
}

static void
attach_to_pidgin_window(PidginWindow *win, gpointer null)
{
	g_list_foreach(pidgin_conv_window_get_gtkconvs(win), (GFunc)attach_to_gtkconv, NULL);

	g_signal_connect(G_OBJECT(win->window), "focus_out_event",
					 G_CALLBACK(focus_removed), win);

	g_signal_connect(G_OBJECT(win->notebook), "switch_page",
					G_CALLBACK(page_switched), win);

	gtk_widget_queue_draw(win->window);
}

static void
detach_from_all_windows()
{
	g_list_foreach(pidgin_conv_windows_get_list(), (GFunc)detach_from_pidgin_window, NULL);
}

static void
attach_to_all_windows()
{
	g_list_foreach(pidgin_conv_windows_get_list(), (GFunc)attach_to_pidgin_window, NULL);
}

static void
conv_created(GaimConversation *conv, gpointer null)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	PidginWindow *win;

	if (!gtkconv)
		return;

	win = pidginconv_get_window(gtkconv);

	detach_from_pidgin_window(win, NULL);
	attach_to_pidgin_window(win, NULL);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	attach_to_all_windows();

	gaim_signal_connect(gaim_conversations_get_handle(), "conversation-created",
						plugin, GAIM_CALLBACK(conv_created), NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	detach_from_all_windows();

	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *pref;

	frame = gaim_plugin_pref_frame_new();

	pref = gaim_plugin_pref_new_with_label(_("Draw Markerline in "));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREF_IMS,
					_("_IM windows"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(PREF_CHATS,
					_("C_hat windows"));
	gaim_plugin_pref_frame_add(frame, pref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL,
};

static GaimPluginInfo info = {
	GAIM_PLUGIN_MAGIC,			/* Magic				*/
	GAIM_MAJOR_VERSION,			/* Gaim Major Version	*/
	GAIM_MINOR_VERSION,			/* Gaim Minor Version	*/
	GAIM_PLUGIN_STANDARD,		/* plugin type			*/
	PIDGIN_PLUGIN_TYPE,		/* ui requirement		*/
	0,							/* flags				*/
	NULL,						/* dependencies			*/
	GAIM_PRIORITY_DEFAULT,		/* priority				*/

	PLUGIN_ID,					/* plugin id			*/
	PLUGIN_NAME,			/* name					*/
	VERSION,					/* version				*/
	PLUGIN_SUMMARY,			/* summary				*/
	PLUGIN_DESCRIPTION,		/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	GAIM_WEBSITE,				/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,						/* destroy				*/

	NULL,						/* ui_info				*/
	NULL,						/* extra_info			*/
	&prefs_info,				/* prefs_info			*/
	NULL						/* actions				*/
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none(PREF_PREFIX);
	gaim_prefs_add_bool(PREF_IMS, FALSE);
	gaim_prefs_add_bool(PREF_CHATS, TRUE);
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
