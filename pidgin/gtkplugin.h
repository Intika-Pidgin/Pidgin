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

#ifndef _PIDGINPLUGIN_H_
#define _PIDGINPLUGIN_H_
/**
 * SECTION:gtkplugin
 * @section_id: pidgin-gtkplugin
 * @short_description: <filename>gtkplugin.h</filename>
 * @title: Plugin API
 */

#include "pidgin.h"
#include "plugins.h"

#include "pidginplugininfo.h"

G_BEGIN_DECLS

/**
 * pidgin_plugins_save:
 *
 * Saves all loaded plugins.
 */
void pidgin_plugins_save(void);

/**
 * pidgin_plugin_dialog_show:
 *
 * Shows the Plugins dialog
 */
void pidgin_plugin_dialog_show(void);

G_END_DECLS

#endif /* _PIDGINPLUGIN_H_ */
