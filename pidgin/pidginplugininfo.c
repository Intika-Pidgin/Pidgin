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
#include "pidginplugininfo.h"

struct _PidginPluginInfo {
	PurplePluginInfo parent;
};

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginPluginInfo, pidgin_plugin_info, PURPLE_TYPE_PLUGIN_INFO);

static void
pidgin_plugin_info_init(PidginPluginInfo *info) {
}

static void
pidgin_plugin_info_class_init(PidginPluginInfoClass *klass) {
}

/******************************************************************************
 * API
 *****************************************************************************/
PidginPluginInfo *
pidgin_plugin_info_new(const char *first_property, ...)
{
	GObject *info;
	va_list var_args;

	/* at least ID is required */
	if (!first_property) {
		return NULL;
	}

	va_start(var_args, first_property);
	info = g_object_new_valist(PIDGIN_TYPE_PLUGIN_INFO, first_property,
	                           var_args);
	va_end(var_args);

	g_object_set(info, "ui-requirement", PIDGIN_UI, NULL);

	return PIDGIN_PLUGIN_INFO(info);
}
