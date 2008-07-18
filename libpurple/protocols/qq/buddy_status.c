/**
 * @file buddy_status.c
 *
 * purple
 *
 * Purple is the legal property ofr its developers, whose names are too numerous
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

#include <string.h>
#include "internal.h"
#include "debug.h"
#include "prefs.h"

#include "buddy_info.h"
#include "buddy_status.h"
#include "crypt.h"
#include "header_info.h"
#include "keep_alive.h"
#include "packet_parse.h"
#include "utils.h"

#include "qq_network.h"

#define QQ_MISC_STATUS_HAVING_VIIDEO      0x00000001
#define QQ_CHANGE_ONLINE_STATUS_REPLY_OK 	0x30	/* ASCII value of "0" */

void qq_buddy_status_dump_unclear(qq_buddy_status *s)
{
	GString *dump;

	g_return_if_fail(s != NULL);

	dump = g_string_new("");
	g_string_append_printf(dump, "unclear fields for [%d]:\n", s->uid);
	g_string_append_printf(dump, "004:     %02x   (unknown)\n", s->unknown1);
	/* g_string_append_printf(dump, "005-008:     %09x   (ip)\n", *(s->ip)); */
	g_string_append_printf(dump, "009-010:     %04x   (port)\n", s->port);
	g_string_append_printf(dump, "011:     %02x   (unknown)\n", s->unknown2);
	g_string_append_printf(dump, "012:     %02x   (status)\n", s->status);
	g_string_append_printf(dump, "013-014:     %04x   (client_version)\n", s->client_version);
	/* g_string_append_printf(dump, "015-030:     %s   (unknown key)\n", s->unknown_key); */
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Buddy status entry, %s", dump->str);
	qq_show_packet("Unknown key", s->unknown_key, QQ_KEY_LENGTH);
	g_string_free(dump, TRUE);
}

/* TODO: figure out what's going on with the IP region. Sometimes I get valid IP addresses, 
 * but the port number's weird, other times I get 0s. I get these simultaneously on the same buddy, 
 * using different accounts to get info. */

/* parse the data into qq_buddy_status */
gint qq_buddy_status_read(qq_buddy_status *s, guint8 *data)
{
	gint bytes = 0;

	g_return_val_if_fail(data != NULL && s != NULL, -1);

	/* 000-003: uid */
	bytes += qq_get32(&s->uid, data + bytes);
	/* 004-004: 0x01 */
	bytes += qq_get8(&s->unknown1, data + bytes);
	/* this is no longer the IP, it seems QQ (as of 2006) no longer sends
	 * the buddy's IP in this packet. all 0s */
	/* 005-008: ip */
	s->ip = g_new0(guint8, 4);
	bytes += qq_getdata(s->ip, 4, data + bytes);
	/* port info is no longer here either */
	/* 009-010: port */
	bytes += qq_get16(&s->port, data + bytes);
	/* 011-011: 0x00 */
	bytes += qq_get8(&s->unknown2, data + bytes);
	/* 012-012: status */
	bytes += qq_get8(&s->status, data + bytes);
	/* 013-014: client_version */
	bytes += qq_get16(&s->client_version, data + bytes);
	/* 015-030: unknown key */
	s->unknown_key = g_new0(guint8, QQ_KEY_LENGTH);
	bytes += qq_getdata(s->unknown_key, QQ_KEY_LENGTH, data + bytes);

	if (s->uid == 0 || bytes != 31)
		return -1;

	return bytes;
}

/* check if status means online or offline */
gboolean is_online(guint8 status)
{
	switch(status) {
		case QQ_BUDDY_ONLINE_NORMAL:
		case QQ_BUDDY_ONLINE_AWAY:
		case QQ_BUDDY_ONLINE_INVISIBLE:
			return TRUE;
		case QQ_BUDDY_ONLINE_OFFLINE:
			return FALSE;
	}
	return FALSE;
}

/* Help calculate the correct icon index to tell the server. */
gint get_icon_offset(PurpleConnection *gc)
{ 
	PurpleAccount *account;
	PurplePresence *presence; 

	account = purple_connection_get_account(gc);
	presence = purple_account_get_presence(account);

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		return 2;
	} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_UNAVAILABLE)) {
		return 1;
	} else {
		return 0;
	}
}

/* send a packet to change my online status */
void qq_send_packet_change_status(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 raw_data[16] = {0};
	gint bytes = 0;
	guint8 away_cmd;
	guint32 misc_status;
	gboolean fake_video;
	PurpleAccount *account;
	PurplePresence *presence; 

	account = purple_connection_get_account(gc);
	presence = purple_account_get_presence(account);

	qd = (qq_data *) gc->proto_data;
	if (!qd->logged_in)
		return;

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		away_cmd = QQ_BUDDY_ONLINE_INVISIBLE;
	} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_UNAVAILABLE)) {
		away_cmd = QQ_BUDDY_ONLINE_AWAY;
	} else {
		away_cmd = QQ_BUDDY_ONLINE_NORMAL;
	}

	misc_status = 0x00000000;
	fake_video = purple_prefs_get_bool("/plugins/prpl/qq/show_fake_video");
	if (fake_video)
		misc_status |= QQ_MISC_STATUS_HAVING_VIIDEO;

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, away_cmd);
	bytes += qq_put32(raw_data + bytes, misc_status);

	qq_send_cmd(qd, QQ_CMD_CHANGE_ONLINE_STATUS, raw_data, bytes);
}

/* parse the reply packet for change_status */
void qq_process_change_status_reply(guint8 *buf, gint buf_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint len, bytes;
	guint8 *data, reply;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	gchar *name;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if ( !qq_decrypt(buf, buf_len, qd->session_key, data, &len) ) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Error decrypt chg status reply\n");
		return;
	}

	bytes = 0;
	bytes = qq_get8(&reply, data + bytes);
	if (reply != QQ_CHANGE_ONLINE_STATUS_REPLY_OK) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ", "Change status fail\n");
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Change status OK\n");
		name = uid_to_purple_name(qd->uid);
		b = purple_find_buddy(gc->account, name);
		g_free(name);
		q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
		qq_update_buddy_contact(gc, q_bud);
	}
}

/* it is a server message indicating that one of my buddies has changed its status */
void qq_process_friend_change_status(guint8 *buf, gint buf_len, PurpleConnection *gc) 
{
	qq_data *qd;
	gint len, bytes;
	guint32 my_uid;
	guint8 *data;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	qq_buddy_status *s;
	gchar *name;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if ( !qq_decrypt(buf, buf_len, qd->session_key, data, &len) ) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Error decrypt buddy status change packet\n");
		return;
	}

	s = g_new0(qq_buddy_status, 1);
	bytes = 0;
	/* 000-030: qq_buddy_status */
	bytes += qq_buddy_status_read(s, data + bytes);
	/* 031-034: my uid */ 
	/* This has a value of 0 when we've changed our status to 
	 * QQ_BUDDY_ONLINE_INVISIBLE */
	bytes += qq_get32(&my_uid, data + bytes);

	if (bytes != 35) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "bytes(%d) != 35\n", bytes);
		g_free(s->ip);
		g_free(s->unknown_key);
		g_free(s);
		return;
	}

	name = uid_to_purple_name(s->uid);
	b = purple_find_buddy(gc->account, name);
	g_free(name);
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "s->uid = %d, q_bud->uid = %d\n", s->uid , q_bud->uid);
		if(0 != *((guint32 *)s->ip)) { 
			g_memmove(q_bud->ip, s->ip, 4);
			q_bud->port = s->port;
		}
		q_bud->status = s->status;
		if(0 != s->client_version) {
			q_bud->client_version = s->client_version; 
		}
		if (q_bud->status == QQ_BUDDY_ONLINE_NORMAL) {
			qq_send_packet_get_level(gc, q_bud->uid);
		}
		qq_update_buddy_contact(gc, q_bud);
	} else {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"got information of unknown buddy %d\n", s->uid);
	}

	g_free(s->ip);
	g_free(s->unknown_key);
	g_free(s);
}
