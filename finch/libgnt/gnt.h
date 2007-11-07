/**
 * @defgroup gnt GNT (GLib Ncurses Toolkit)
 *
 * GNT is an ncurses toolkit for creating text-mode graphical user interfaces
 * in a fast and easy way.
 */
/**
 * @file gnt.h GNT API
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include <glib.h>
#include "gntwidget.h"
#include "gntclipboard.h"
#include "gntcolors.h"
#include "gntkeys.h"

/**
 * Get things to compile in Glib < 2.8
 */
#if !GLIB_CHECK_VERSION(2,8,0)
	#define G_PARAM_STATIC_NAME  G_PARAM_PRIVATE
	#define G_PARAM_STATIC_NICK  G_PARAM_PRIVATE
	#define G_PARAM_STATIC_BLURB  G_PARAM_PRIVATE
#endif

/**
 *
 */
void gnt_init(void);

/**
 *
 */
void gnt_main(void);

/**
 *
 *
 * @return
 */
gboolean gnt_ascii_only(void);

/**
 * Present a window. If the event was triggered because of user interaction,
 * the window is moved to the foreground. Otherwise, the Urgent hint is set.
 *
 * @param window   The window to present.
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_window_present(GntWidget *window);

/**
 *
 */
void gnt_screen_occupy(GntWidget *widget);

/**
 *
 */
void gnt_screen_release(GntWidget *widget);

/**
 *
 */
void gnt_screen_update(GntWidget *widget);

/**
 *
 */
void gnt_screen_resize_widget(GntWidget *widget, int width, int height);

/**
 *
 */
void gnt_screen_move_widget(GntWidget *widget, int x, int y);

/**
 *
 */
void gnt_screen_rename_widget(GntWidget *widget, const char *text);

/**
 *
 *
 * @return
 */
gboolean gnt_widget_has_focus(GntWidget *widget);

/**
 *
 */
void gnt_widget_set_urgent(GntWidget *widget);

/**
 *
 */
void gnt_register_action(const char *label, void (*callback)());

/**
 *
 *
 * @return
 */
gboolean gnt_screen_menu_show(gpointer menu);

/**
 *
 */
void gnt_quit(void);

/**
 *
 *
 * @return
 */
GntClipboard * gnt_get_clipboard(void);

/**
 *
 *
 * @return
 */
gchar * gnt_get_clipboard_string(void);

/**
 *
 */
void gnt_set_clipboard_string(gchar *string);

/**
 * Spawn a different application that will consume the console.
 */
gboolean gnt_giveup_console(const char *wd, char **argv, char **envp,
		gint *stin, gint *stout, gint *sterr,
		void (*callback)(int status, gpointer data), gpointer data);

gboolean gnt_is_refugee(void);
