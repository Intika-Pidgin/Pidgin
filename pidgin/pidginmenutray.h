/* Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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

#ifndef PIDGIN_MENU_TRAY_H
#define PIDGIN_MENU_TRAY_H

/**
 * SECTION:gtkmenutray
 * @section_id: pidgin-gtkmenutray
 * @short_description: <filename>gtkmenutray.h</filename>
 * @title: Tray Menu Item
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_MENU_TRAY (pidgin_menu_tray_get_type())
G_DECLARE_FINAL_TYPE(PidginMenuTray, pidgin_menu_tray, PIDGIN, MENU_TRAY, GtkMenuItem)

/**
 * PidginMenuTray:
 *
 * A PidginMenuTray is a #GtkMenuItem that allows you to pack icons into it
 * similar to a system notification area but in a windows menu bar.
 */

/**
 * pidgin_menu_tray_new:
 *
 * Creates a new PidginMenuTray
 *
 * Returns: A new PidginMenuTray
 */
GtkWidget *pidgin_menu_tray_new(void);

/**
 * pidgin_menu_tray_get_box:
 * @menu_tray: The PidginMenuTray
 *
 * Gets the box for the PidginMenuTray
 *
 * Returns: (transfer none): The box that this menu tray is using
 */
GtkWidget *pidgin_menu_tray_get_box(PidginMenuTray *menu_tray);

/**
 * pidgin_menu_tray_append:
 * @menu_tray: The tray
 * @widget:    The widget
 * @tooltip:   The tooltip for this widget
 *
 * Appends a widget into the tray
 */
void pidgin_menu_tray_append(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

/**
 * pidgin_menu_tray_prepend:
 * @menu_tray: The tray
 * @widget:    The widget
 * @tooltip:   The tooltip for this widget
 *
 * Prepends a widget into the tray
 */
void pidgin_menu_tray_prepend(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

/**
 * pidgin_menu_tray_set_tooltip:
 * @menu_tray: The tray
 * @widget:    The widget
 * @tooltip:   The tooltip to set for the widget
 *
 * Set the tooltip for a widget
 */
void pidgin_menu_tray_set_tooltip(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

G_END_DECLS

#endif /* PIDGIN_MENU_TRAY_H */
