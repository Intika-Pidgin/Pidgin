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

#ifndef PIDGIN_DEBUG_PLUGIN_INFO_H
#define PIDGIN_DEBUG_PLUGIN_INFO_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_DEBUG_PLUGIN_INFO  pidgin_debug_plugin_info_get_type()
G_DECLARE_FINAL_TYPE(PidginDebugPluginInfo, pidgin_debug_plugin_info, PIDGIN,
		DEBUG_PLUGIN_INFO, GtkDialog)

GtkWidget *pidgin_debug_plugin_info_new(void);

void pidgin_debug_plugin_info_show(void);

G_END_DECLS

#endif /* PIDGIN_DEBUG_PLUGIN_INFO_H */
