/**
 * @file utils.h
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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

#ifndef _PURPLE_GG_UTILS_H
#define _PURPLE_GG_UTILS_H

#include <internal.h>
#include <libgadu.h>

/**
 * Converts stringified UIN to uin_t.
 *
 * @param str The string to convert.
 *
 * @return Converted UIN or 0 if an error occurred.
 */
uin_t ggp_str_to_uin(const char *str);

/**
 * Stringifies UIN.
 *
 * @param uin UIN to stringify.
 *
 * @return Stringified UIN.
 */
const char * ggp_uin_to_str(uin_t uin);

/**
 * Converts encoding of a given string from UTF-8 to CP1250.
 *
 * @param src Input string.
 *
 * @return Converted string (must be freed with g_free). If src is NULL,
 * then NULL is returned.
 */
gchar * ggp_convert_to_cp1250(const gchar *src);

/**
 * Converts encoding of a given string from CP1250 to UTF-8.
 *
 * @param src Input string.
 *
 * @return Converted string (must be freed with g_free). If src is NULL,
 * then NULL is returned.
 */
gchar * ggp_convert_from_cp1250(const gchar *src);

/**
 * Adds an input handler in purple event loop for http request.
 *
 * @see purple_input_add
 *
 * @param http_req  Http connection to watch.
 * @param func      The callback function for data.
 * @param user_data User-specified data.
 *
 * @return The resulting handle (will be greater than 0).
 */
guint ggp_http_input_add(struct gg_http *http_req, PurpleInputFunction func,
	gpointer user_data);

#endif /* _PURPLE_GG_UTILS_H */
