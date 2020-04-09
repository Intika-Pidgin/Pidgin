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
 *
 */

#ifndef PIDGIN_PLUGINS_DIALOG_H
#define PIDGIN_PLUGINS_DIALOG_H

/**
 * SECTION:pidginpluginsdialog
 * @section_id: pidgin-plugins-dialog
 * @short_description: A widget to display available plugins
 * @title: Plugins Dialog
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_PLUGINS_DIALOG (pidgin_plugins_dialog_get_type())
G_DECLARE_FINAL_TYPE(PidginPluginsDialog, pidgin_plugins_dialog, PIDGIN,
		PLUGINS_DIALOG, GtkDialog)

/**
 * pidgin_plugins_dialog_new:
 *
 * Creates a new instance of #PidginPluginsDialog.
 *
 * Returns: (transfer full): The new #PidginPluginsDialog.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_plugins_dialog_new(void);

G_END_DECLS

#endif /* PIDGIN_PLUGINS_DIALOG_H */

