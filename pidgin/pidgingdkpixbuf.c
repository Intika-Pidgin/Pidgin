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

#include "pidgin/pidgingdkpixbuf.h"

GdkPixbuf *
pidgin_gdk_pixbuf_new_from_image(PurpleImage *image, GError **error) {
	GdkPixbufLoader *loader = NULL;
	GdkPixbuf *pixbuf = NULL;
	GBytes *data = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	data = purple_image_get_contents(image);
	success = gdk_pixbuf_loader_write_bytes(loader, data, error);
	g_bytes_unref(data);

	if(success) {
		if(error != NULL && *error != NULL) {
			g_object_unref(G_OBJECT(loader));

			return NULL;
		}

		if(gdk_pixbuf_loader_close(loader, error)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
			if(pixbuf != NULL) {
				pixbuf = g_object_ref(pixbuf);
			}
		}

		g_object_unref(G_OBJECT(loader));
	}

	return pixbuf;
}

void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf) {
	gint width, height, rowstride;
	guchar *pixels;

	if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
		return;
	}

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	if (width < 6 || height < 6)
		return;

	/* The following code will conver the alpha of the pixel data in all
	 * corners to look something like the following diagram.
	 *
	 * 00 80 c0 FF FF c0 80 00
	 * 80 FF FF FF FF FF FF 80
	 * c0 FF FF FF FF FF FF c0
	 * FF FF FF FF FF FF FF FF
	 * FF FF FF FF FF FF FF FF
	 * c0 FF FF FF FF FF FF c0
	 * 80 FF FF FF FF FF FF 80
	 * 00 80 c0 FF FF c0 80 00
	 */

	/* Top left */
	pixels[3] = 0;
	pixels[7] = 0x80;
	pixels[11] = 0xC0;
	pixels[rowstride + 3] = 0x80;
	pixels[rowstride * 2 + 3] = 0xC0;

	/* Top right */
	pixels[width * 4 - 1] = 0;
	pixels[width * 4 - 5] = 0x80;
	pixels[width * 4 - 9] = 0xC0;
	pixels[rowstride + (width * 4) - 1] = 0x80;
	pixels[(2 * rowstride) + (width * 4) - 1] = 0xC0;

	/* Bottom left */
	pixels[(height - 1) * rowstride + 3] = 0;
	pixels[(height - 1) * rowstride + 7] = 0x80;
	pixels[(height - 1) * rowstride + 11] = 0xC0;
	pixels[(height - 2) * rowstride + 3] = 0x80;
	pixels[(height - 3) * rowstride + 3] = 0xC0;

	/* Bottom right */
	pixels[height * rowstride - 1] = 0;
	pixels[(height - 1) * rowstride - 1] = 0x80;
	pixels[(height - 2) * rowstride - 1] = 0xC0;
	pixels[height * rowstride - 5] = 0x80;
	pixels[height * rowstride - 9] = 0xC0;
}

gboolean
pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf) {
	gint height, rowstride, i;
	guchar *pixels;
	guchar *row;

	if (!gdk_pixbuf_get_has_alpha(pixbuf))
		return TRUE;

	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	/* check the top row */
	row = pixels;
	for (i = 3; i < rowstride; i+=4) {
		if (row[i] < 0xfe)
			return FALSE;
	}

	/* check the left and right sides */
	for (i = 1; i < height - 1; i++) {
		row = pixels + (i * rowstride);
		if (row[3] < 0xfe || row[rowstride - 1] < 0xfe) {
			return FALSE;
	    }
	}

	/* check the bottom */
	row = pixels + ((height - 1) * rowstride);
	for (i = 3; i < rowstride; i += 4) {
		if (row[i] < 0xfe)
			return FALSE;
	}

	return TRUE;
}

