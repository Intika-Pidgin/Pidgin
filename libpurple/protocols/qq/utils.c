/**
 * @file utils.c
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

#include "limits.h"
#include "stdlib.h"
#include "string.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "char_conv.h"
#include "debug.h"
#include "prefs.h"
#include "qq.h"
#include "util.h"
#include "utils.h"

#define QQ_NAME_FORMAT    "%d"

/* These functions are used only in development phase */
/*
   static void _qq_show_socket(gchar *desc, gint fd) {
   struct sockaddr_in sin;
   socklen_t len = sizeof(sin);
   getsockname(fd, (struct sockaddr *)&sin, &len);
   purple_debug(PURPLE_DEBUG_INFO, desc, "%s:%d\n",
   inet_ntoa(sin.sin_addr), g_ntohs(sin.sin_port));
   }
   */

gchar *get_name_by_index_str(gchar **array, const gchar *index_str, gint amount)
{
	gint index;

	index = atoi(index_str);
	if (index < 0 || index >= amount)
		index = 0;

	return array[index];
}

gchar *get_index_str_by_name(gchar **array, const gchar *name, gint amount)
{
	gint index;

	for (index = 0; index <= amount; index++)
		if (g_ascii_strcasecmp(array[index], name) == 0)
			break;

	if (index >= amount)
		index = 0;	/* meaning no match */
	return g_strdup_printf("%d", index);
}

gint qq_string_to_dec_value(const gchar *str)
{
	g_return_val_if_fail(str != NULL, 0);
	return strtol(str, NULL, 10);
}

/* split the given data(len) with delimit,
 * check the number of field matches the expected_fields (<=0 means all)
 * return gchar* array (needs to be freed by g_strfreev later), or NULL */
gchar **split_data(guint8 *data, gint len, const gchar *delimit, gint expected_fields)
{
	guint8 *input;
	gchar **segments;
	gint i, j;

	g_return_val_if_fail(data != NULL && len != 0 && delimit != 0, NULL);

	/* as the last field would be string, but data is not ended with 0x00
	 * we have to duplicate the data and append a 0x00 at the end */
	input = g_newa(guint8, len + 1);
	g_memmove(input, data, len);
	input[len] = 0x00;

	segments = g_strsplit((gchar *) input, delimit, 0);
	if (expected_fields <= 0)
		return segments;

	for (i = 0; segments[i] != NULL; i++) {;
	}
	if (i < expected_fields) {	/* not enough fields */
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Invalid data, expect %d fields, found only %d, discard\n", expected_fields, i);
		g_strfreev(segments);
		return NULL;
	} else if (i > expected_fields) {	/* more fields, OK */
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			   "Dangerous data, expect %d fields, found %d, return all\n", expected_fields, i);
		/* free up those not used */
		for (j = expected_fields; j < i; j++) {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ", "field[%d] is %s\n", j, segments[j]);
			g_free(segments[j]);
		}

		segments[expected_fields] = NULL;
	}

	return segments;
}

/* given a four-byte ip data, convert it into a human readable ip string
 * the return needs to be freed */
gchar *gen_ip_str(guint8 *ip)
{
	gchar *ret;
	if (ip == NULL || ip[0] == 0) {
		ret = g_new(gchar, 1);
		*ret = '\0';
		return ret;
	} else {
		return g_strdup_printf("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	}
}

guint8 *str_ip_gen(gchar *str) {
	guint8 *ip = g_new(guint8, 4);
	gint a, b, c, d;

	sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
	ip[0] = a;
	ip[1] = b;
	ip[2] = c;
	ip[3] = d;
	return ip;
}

/* convert Purple name to original QQ UID */
guint32 purple_name_to_uid(const gchar *const name)
{
	guint32 ret;
	g_return_val_if_fail(name != NULL, 0);

	ret = strtol(name, NULL, 10);
	if (errno == ERANGE)
		return 0;
	else
		return ret;
}

/* convert a QQ UID to a unique name of Purple
 * the return needs to be freed */
gchar *uid_to_purple_name(guint32 uid)
{
	return g_strdup_printf(QQ_NAME_FORMAT, uid);
}

/* convert name displayed in a chat channel to original QQ UID */
gchar *chat_name_to_purple_name(const gchar *const name)
{
	const gchar *tmp;
	gchar *ret;

	g_return_val_if_fail(name != NULL, NULL);

	tmp = (gchar *) purple_strcasestr(name, "(qq-");
	ret = g_strndup(tmp + 4, strlen(name) - (tmp - name) - 4 - 1);

	return ret;
}

/* try to dump the data as GBK */
gchar* try_dump_as_gbk(const guint8 *const data, gint len)
{
	gint i;
	guint8 *incoming;
	gchar *msg_utf8;

	incoming = g_newa(guint8, len + 1);
	g_memmove(incoming, data, len);
	incoming[len] = 0x00;
	/* GBK code:
	 * Single-byte ASCII:      0x21-0x7E
	 * GBK first byte range:   0x81-0xFE
	 * GBK second byte range:  0x40-0x7E and 0x80-0xFE */
	for (i = 0; i < len; i++)
		if (incoming[i] >= 0x81)
			break;

	msg_utf8 = i < len ? qq_to_utf8((gchar *) &incoming[i], QQ_CHARSET_DEFAULT) : NULL;

	if (msg_utf8 != NULL) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ", "Try extract GB msg: %s\n", msg_utf8);
	}
	return msg_utf8;
}

/* strips whitespace */
static gchar *strstrip(const gchar *const buffer)
{
	GString *stripped;
	gchar *ret, cur;
	gint i;

	g_return_val_if_fail(buffer != NULL, NULL);

	stripped = g_string_new("");
	for (i=0; i<strlen(buffer); i++) {
		cur = buffer[i];
		if (cur != ' ' && cur != '\n')
			g_string_append_c(stripped, buffer[i]);
	}
	ret = stripped->str;
	g_string_free(stripped, FALSE);

	return ret;
}

/* Attempts to dump an ASCII hex string to a string of bytes.
 * The return should be freed later. */
guint8 *hex_str_to_bytes(const gchar *const buffer, gint *out_len)
{
	gchar *hex_str, *hex_buffer, *cursor, tmp;
	guint8 *bytes, nibble1, nibble2;
	gint index;

	g_return_val_if_fail(buffer != NULL, NULL);

	hex_buffer = strstrip(buffer);

	if (strlen(hex_buffer) % 2 != 0) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Unable to convert an odd number of nibbles to a string of bytes!\n");
		g_free(hex_buffer);
		return NULL;
	}
	bytes = g_newa(guint8, strlen(hex_buffer) / 2);
	hex_str = g_ascii_strdown(hex_buffer, -1);
	g_free(hex_buffer);
	index = 0;
	for (cursor = hex_str; cursor < hex_str + sizeof(gchar) * (strlen(hex_str)) - 1; cursor++) {
		if (g_ascii_isdigit(*cursor)) {
			tmp = *cursor; nibble1 = atoi(&tmp);
		} else if (g_ascii_isalpha(*cursor) && (gint) *cursor - 87 < 16) {
			nibble1 = (gint) *cursor - 87;
		} else {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Invalid char \'%c\' found in hex string!\n", *cursor);
			g_free(hex_str);
			return NULL;
		}
		nibble1 = nibble1 << 4;
		cursor++;
		if (g_ascii_isdigit(*cursor)) {
			tmp = *cursor; nibble2 = atoi(&tmp);
		} else if (g_ascii_isalpha(*cursor) && (gint) (*cursor - 87) < 16) {
			nibble2 = (gint) *cursor - 87;
		} else {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Invalid char found in hex string!\n");
			g_free(hex_str);
			return NULL;
		}
		bytes[index++] = nibble1 + nibble2;
	}
	*out_len = strlen(hex_str) / 2;
	g_free(hex_str);
	return g_memdup(bytes, *out_len);
}

/* Dumps a chunk of raw data into an ASCII hex string.
 * The return should be freed later. */
static gchar *hex_dump_to_str(const guint8 *const buffer, gint bytes)
{
	GString *str;
	gchar *ret;
	gint i, j, ch;

	str = g_string_new("");
	for (i = 0; i < bytes; i += 16) {
		/* length label */
		g_string_append_printf(str, "%07x: ", i);

		/* dump hex value */
		for (j = 0; j < 16; j++)
			if ((i + j) < bytes)
				g_string_append_printf(str, " %02x", buffer[i + j]);
			else
				g_string_append(str, "   ");
		g_string_append(str, "  ");

		/* dump ascii value */
		for (j = 0; j < 16 && (i + j) < bytes; j++) {
			ch = buffer[i + j] & 127;
			if (ch < ' ' || ch == 127)
				g_string_append_c(str, '.');
			else
				g_string_append_c(str, ch);
		}
		g_string_append_c(str, '\n');
	}

	ret = str->str;
	/* GString can be freed without freeing it character data */
	g_string_free(str, FALSE);

	return ret;
}

void qq_hex_dump(PurpleDebugLevel level, const char *category,
		const guint8 *pdata, gint bytes,	
		const char *format, ...)
{
	va_list args;
	char *arg_s = NULL;
	gchar *phex = NULL;

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	va_start(args, format);
	arg_s = g_strdup_vprintf(format, args);
	va_end(args);

	if (bytes <= 0) {
		purple_debug(level, category, arg_s);
		return;
	}

	phex = hex_dump_to_str(pdata, bytes);
	purple_debug(level, category, "%s - (len %d)\n%s", arg_s, bytes, phex);
	g_free(phex);
}

void qq_show_packet(const gchar *desc, const guint8 *buf, gint len)
{
	/*
	   char buf1[8*len+2], buf2[10];
	   int i;
	   buf1[0] = 0;
	   for (i = 0; i < len; i++) {
	   sprintf(buf2, " %02x(%d)", buf[i] & 0xff, buf[i] & 0xff);
	   strcat(buf1, buf2);
	   }
	   strcat(buf1, "\n");
	   purple_debug(PURPLE_DEBUG_INFO, desc, "%s", buf1);
	   */

	/* modified by s3e, 20080424 */
	qq_hex_dump(PURPLE_DEBUG_INFO, desc,
		buf, len,
		"");
}

/* convert face num from packet (0-299) to local face (1-100) */
gchar *face_to_icon_str(gint face)
{
	gchar *icon_num_str;
	gint icon_num = face / 3 + 1;
	icon_num_str = g_strdup_printf("%d", icon_num);
	return icon_num_str;
}

/* return the location of the buddy icon dir
 * any application using libpurple but not installing the QQ buddy icons
 * under datadir needs to set the pref below, or buddy icons won't work */
const char *qq_buddy_icon_dir(void)
{
	if (purple_prefs_exists("/prpl/qq/buddy_icon_dir"))
		return purple_prefs_get_string("/prpl/qq/buddy_icon_dir");
	else
		return QQ_BUDDY_ICON_DIR;
}

#ifdef _WIN32
const char *qq_win32_buddy_icon_dir(void)
{
        static char *dir = NULL;
        if (dir == NULL)
                dir = g_build_filename(wpurple_install_dir(), "pixmaps",
                        "purple", "buddy_icons", "qq", NULL);
        return dir;
}
#endif
