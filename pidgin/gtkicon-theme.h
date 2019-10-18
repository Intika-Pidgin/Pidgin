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

#ifndef PIDGIN_ICON_THEME_H
#define PIDGIN_ICON_THEME_H
/**
 * SECTION:gtkicon-theme
 * @section_id: pidgin-gtkicon-theme
 * @short_description: <filename>gtkicon-theme.h</filename>
 * @title: Icon Theme Class
 */

#include <glib.h>
#include <glib-object.h>
#include "theme.h"

#define PIDGIN_TYPE_ICON_THEME  pidgin_icon_theme_get_type()

struct _PidginIconThemeClass
{
	PurpleThemeClass parent_class;
};

/**************************************************************************/
/* Pidgin Icon Theme API                                                  */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * pidgin_icon_theme_get_type:
 *
 * Returns: The #GType for an icon theme.
 */
G_DECLARE_DERIVABLE_TYPE(PidginIconTheme, pidgin_icon_theme, PIDGIN,
		ICON_THEME, PurpleTheme)

/**
 * pidgin_icon_theme_get_icon:
 * @theme:     the theme
 * @event:		the pidgin icon event to look up
 *
 * Returns a copy of the filename for the icon event or NULL if it is not set
 *
 * Returns: the filename of the icon event
 */
const gchar *pidgin_icon_theme_get_icon(PidginIconTheme *theme,
		const gchar *event);

/**
 * pidgin_icon_theme_set_icon:
 * @theme:         the theme
 * @icon_id:		a string representing what the icon is to be used for
 * @filename:		the name of the file to be used for the given id
 *
 * Sets the filename for a given icon id, setting the icon to NULL will remove the icon from the theme
 */
void pidgin_icon_theme_set_icon(PidginIconTheme *theme,
		const gchar *icon_id,
		const gchar *filename);

G_END_DECLS

#endif /* PIDGIN_ICON_THEME_H */
