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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef PIDGIN_GDK_PIXBUF_H
#define PIDGIN_GDK_PIXBUF_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <purple.h>

/**
 * SECTION:pidgingdkpixbuf
 * @section_id: pidgin-gdk-pixbuf
 * @short_description: <filename>pidgingdkpixbuf.h</filename>
 * @title: GDK Pixbuf Helpers
 */

G_BEGIN_DECLS

GdkPixbuf *pidgin_gdk_pixbuf_new_from_image(PurpleImage *image, GError **error);

/**
 * pidgin_gdk_pixbuf_make_round:
 * @pixbuf:  The buddy icon to transform
 *
 * Rounds the corners of a GdkPixbuf in place.
 */
void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf);

/**
 * pidgin_gdk_pixbuf_is_opaque:
 * @pixbuf:  The pixbuf
 *
 * Returns TRUE if the GdkPixbuf is opaque, as determined by no
 * alpha at any of the edge pixels.
 *
 * Returns: TRUE if the pixbuf is opaque around the edges, FALSE otherwise
 */
gboolean pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf);

G_END_DECLS

#endif /* PIDGIN_GDK_PIXBUF_H */
