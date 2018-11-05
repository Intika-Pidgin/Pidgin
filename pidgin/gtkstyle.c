/*
 * @file gtkstyle.c GTK+ Style utility functions
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
#include "gtkstyle.h"

/* Assume light mode */
static gboolean dark_mode_cache = FALSE;

gboolean
pidgin_style_is_dark(GtkStyle *style) {
	GdkColor bg;

	if (!style) {
		return dark_mode_cache;
	}

	bg = style->base[GTK_STATE_NORMAL];

	if (bg.red != 0xFFFF || bg.green != 0xFFFF || bg.blue != 0xFFFF) {
		dark_mode_cache =  ((int) bg.red + (int) bg.green + (int) bg.blue) < (65536 * 3 / 2);
	}

	return dark_mode_cache;
}

void
pidgin_style_adjust_contrast(GtkStyle *style, GdkRGBA *rgba) {
	if (pidgin_style_is_dark(style)) {
		gdouble h, s, v;

		gtk_rgb_to_hsv(rgba->red, rgba->green, rgba->blue, &h, &s, &v);

		v += 0.3;
		v = v > 1.0 ? 1.0 : v;
		s = 0.7;

		gtk_hsv_to_rgb(h, s, v, &rgba->red, &rgba->green, &rgba->blue);
	}
}
