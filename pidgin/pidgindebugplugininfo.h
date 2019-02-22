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

#include <gtk/gtk.h>

#define PIDGIN_TYPE_DEBUG_PLUGIN_INFO            (pidgin_debug_plugin_info_get_type())
#define PIDGIN_DEBUG_PLUGIN_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_DEBUG_PLUGIN_INFO, PidginDebugPluginInfo))
#define PIDGIN_DEBUG_PLUGIN_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_DEBUG_PLUGIN_INFO, PidginDebugPluginInfoClass))
#define PIDGIN_IS_DEBUG_PLUGIN_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_DEBUG_PLUGIN_INFO))
#define PIDGIN_IS_DEBUG_PLUGIN_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_DEBUG_PLUGIN_INFO))
#define PIDGIN_DEBUG_PLUGIN_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_DEBUG_PLUGIN_INFO, PidginDebugPluginInfoClass))

typedef struct _PidginDebugPluginInfo            PidginDebugPluginInfo;
typedef struct _PidginDebugPluginInfoClass       PidginDebugPluginInfoClass;

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

struct _PidginDebugPluginInfo {
	GtkDialog parent;

	gpointer reserved[4];
};

struct _PidginDebugPluginInfoClass {
	GtkDialogClass parent;

	gpointer reserved[4];
};

G_BEGIN_DECLS

GType pidgin_debug_plugin_info_get_type(void);

GtkWidget *pidgin_debug_plugin_info_new(void);

void pidgin_debug_plugin_info_show(void);

G_END_DECLS

#endif /* PIDGIN_DEBUG_PLUGIN_INFO_H */
