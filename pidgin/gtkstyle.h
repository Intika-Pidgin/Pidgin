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

/**
 * SECTION:gtkstyle
 * @section_id: pidgin-gtkstyle
 * @short_description: <filename>gtkstyle.h</filename>
 * @title: Style API
 */

#ifndef _PIDGINSTYLE_H_
#define _PIDGINSTYLE_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * pidgin_style_is_dark:
 * @style: The GtkStyle in use, or NULL to use a cached version.
 *
 * Returns whether or not dark mode is enabled.
 *
 * Returns: TRUE if dark mode is enabled and foreground colours should be invertred
 */

gboolean pidgin_style_is_dark(GtkStyle *style);

/**
 * pidgin_style_adjust_contrast:
 * @style: The GtkStyle in use.
 * @color: Color to be lightened. Transformed color will be written here.
 *
 * Lighten a color if dark mode is enabled.
 */

void pidgin_style_adjust_contrast(GtkStyle *style, GdkRGBA *color);

G_END_DECLS

#endif /* _PIDGINSTYLE_H_ */
