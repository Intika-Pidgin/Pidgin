/**
 * @file char_conv.h
 *
 * purple
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

#ifndef _QQ_CHAR_CONV_H_
#define _QQ_CHAR_CONV_H_

#include <glib.h>

#define QQ_CHARSET_DEFAULT      "GB18030"

gint qq_get_vstr(gchar **ret, const gchar *from_charset, guint8 *data);
gint qq_put_vstr(guint8 *buf, const gchar *str_utf8, const gchar *to_charset);

gchar *utf8_to_qq(const gchar *str, const gchar *to_charset);
gchar *qq_to_utf8(const gchar *str, const gchar *from_charset);

#endif
