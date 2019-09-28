/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#ifndef PIDGIN_ICON_THEME_LOADER_H
#define PIDGIN_ICON_THEME_LOADER_H
/**
 * SECTION:gtkicon-theme-loader
 * @section_id: pidgin-gtkicon-theme-loader
 * @short_description: <filename>gtkicon-theme-loader.h</filename>
 * @title: Pidgin Icon Theme Loader Class
 */

#include <glib.h>
#include <glib-object.h>
#include "theme-loader.h"

#define PIDGIN_TYPE_ICON_THEME_LOADER  pidgin_icon_theme_loader_get_type()

/**************************************************************************/
/* Pidgin Icon Theme-Loader API                                           */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * pidgin_icon_theme_loader_get_type:
 *
 * Returns: The #GType for an icon theme loader.
 */
G_DECLARE_FINAL_TYPE(PidginIconThemeLoader, pidgin_icon_theme_loader, PIDGIN,
		ICON_THEME_LOADER, PurpleThemeLoader)

G_END_DECLS

#endif /* PIDGIN_ICON_THEME_LOADER_H */
