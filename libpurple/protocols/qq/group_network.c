/**
 * @file group_network.c
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

#include "internal.h"

#include "debug.h"
#include "notify.h"

#include "char_conv.h"
#include "crypt.h"
#include "group_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_network.h"
#include "group_opt.h"
#include "group_search.h"
#include "header_info.h"
#include "qq_network.h"
#include "utils.h"

enum {
	QQ_GROUP_CMD_REPLY_OK = 0x00,
	QQ_GROUP_CMD_REPLY_SEARCH_ERROR = 0x02,
	QQ_GROUP_CMD_REPLY_NOT_MEMBER = 0x0a
};

const gchar *qq_group_cmd_get_desc(qq_group_cmd cmd)
{
	switch (cmd) {
	case QQ_GROUP_CMD_CREATE_GROUP:
		return "QQ_GROUP_CMD_CREATE_GROUP";
	case QQ_GROUP_CMD_MEMBER_OPT:
		return "QQ_GROUP_CMD_MEMBER_OPT";
	case QQ_GROUP_CMD_MODIFY_GROUP_INFO:
		return "QQ_GROUP_CMD_MODIFY_GROUP_INFO";
	case QQ_GROUP_CMD_GET_GROUP_INFO:
		return "QQ_GROUP_CMD_GET_GROUP_INFO";
	case QQ_GROUP_CMD_ACTIVATE_GROUP:
		return "QQ_GROUP_CMD_ACTIVATE_GROUP";
	case QQ_GROUP_CMD_SEARCH_GROUP:
		return "QQ_GROUP_CMD_SEARCH_GROUP";
	case QQ_GROUP_CMD_JOIN_GROUP:
		return "QQ_GROUP_CMD_JOIN_GROUP";
	case QQ_GROUP_CMD_JOIN_GROUP_AUTH:
		return "QQ_GROUP_CMD_JOIN_GROUP_AUTH";
	case QQ_GROUP_CMD_EXIT_GROUP:
		return "QQ_GROUP_CMD_EXIT_GROUP";
	case QQ_GROUP_CMD_SEND_MSG:
		return "QQ_GROUP_CMD_SEND_MSG";
	case QQ_GROUP_CMD_GET_ONLINE_MEMBER:
		return "QQ_GROUP_CMD_GET_ONLINE_MEMBER";
	case QQ_GROUP_CMD_GET_MEMBER_INFO:
		return "QQ_GROUP_CMD_GET_MEMBER_INFO";
	default:
		return "Unknown QQ Group Command";
	}
}

/* default process of reply error */
static void _qq_process_group_cmd_reply_error_default(guint8 reply, guint8 *data, gint len, PurpleConnection *gc)
{
	gchar *msg, *msg_utf8;
	g_return_if_fail(data != NULL && len > 0);

	msg = g_strndup((gchar *) data, len);	/* it will append 0x00 */
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	g_free(msg);
	msg = g_strdup_printf(_("Code [0x%02X]: %s"), reply, msg_utf8);
	purple_notify_error(gc, NULL, _("Group Operation Error"), msg);
	g_free(msg);
	g_free(msg_utf8);
}

/* default process, dump only */
static void _qq_process_group_cmd_reply_default(guint8 *data, gint len, PurpleConnection *gc)
{
	g_return_if_fail(data != NULL && len > 0);

	qq_hex_dump(PURPLE_DEBUG_INFO, "QQ",
		data, len, 
		"Dump unprocessed group cmd reply:");
}

/* The lower layer command of send group cmd */
void qq_send_group_cmd(PurpleConnection *gc, qq_group *group, guint8 *raw_data, gint data_len)
{
	qq_data *qd;
	group_packet *p;

	g_return_if_fail(raw_data != NULL && data_len > 0);

	qd = (qq_data *) gc->proto_data;

	qq_send_cmd(qd, QQ_CMD_GROUP_CMD, raw_data, data_len);

	p = g_new0(group_packet, 1);

	p->send_seq = qd->send_seq;
	if (group == NULL)
		p->internal_group_id = 0;
	else
		p->internal_group_id = group->internal_group_id;

	qd->group_packets = g_list_append(qd->group_packets, p);
}

/* the main entry of group cmd processing, called by qq_recv_core.c */
void qq_process_group_cmd_reply(guint8 *buf, gint buf_len, guint16 seq, PurpleConnection *gc)
{
	qq_group *group;
	qq_data *qd;
	gint len, bytes;
	guint32 internal_group_id;
	guint8 *data, sub_cmd, reply;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (!qq_group_find_internal_group_id_by_seq(gc, seq, &internal_group_id)) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ", "We have no record of group cmd, seq [%d]\n", seq);
		return;
	}

	if ( !qq_decrypt(buf, buf_len, qd->session_key, data, &len) ) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Error decrypt group cmd reply\n");
		return;
	}

	if (len <= 2) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Group cmd reply is too short, only %d bytes\n", len);
		return;
	}

	bytes = 0;
	bytes += qq_get8(&sub_cmd, data + bytes);
	bytes += qq_get8(&reply, data + bytes);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);

	if (reply != QQ_GROUP_CMD_REPLY_OK) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			   "Group cmd reply says cmd %s fails\n", qq_group_cmd_get_desc(sub_cmd));

		if (group != NULL)
			qq_set_pending_id(&qd->joining_groups, group->external_group_id, FALSE);

		switch (reply) {	/* this should be all errors */
		case QQ_GROUP_CMD_REPLY_NOT_MEMBER:
			if (group != NULL) {
				purple_debug(PURPLE_DEBUG_WARNING,
					   "QQ",
					   "You are not a member of group \"%s\"\n", group->group_name_utf8);
				group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
				qq_group_refresh(gc, group);
			}
			break;
		case QQ_GROUP_CMD_REPLY_SEARCH_ERROR:
			if (qd->roomlist != NULL) {
				if (purple_roomlist_get_in_progress(qd->roomlist))
					purple_roomlist_set_in_progress(qd->roomlist, FALSE);
			}
			_qq_process_group_cmd_reply_error_default(reply, data + bytes, len - bytes, gc);
			break;
		default:
			_qq_process_group_cmd_reply_error_default(reply, data + bytes, len - bytes, gc);
		}
		return;
	}

	/* seems ok so far, so we process the reply according to sub_cmd */
	switch (sub_cmd) {
	case QQ_GROUP_CMD_GET_GROUP_INFO:
		qq_process_group_cmd_get_group_info(data + bytes, len - bytes, gc);
		if (group != NULL) {
			qq_send_cmd_group_get_members_info(gc, group);
			qq_send_cmd_group_get_online_members(gc, group);
		}
		break;
	case QQ_GROUP_CMD_CREATE_GROUP:
		qq_group_process_create_group_reply(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_MODIFY_GROUP_INFO:
		qq_group_process_modify_info_reply(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_MEMBER_OPT:
		qq_group_process_modify_members_reply(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_ACTIVATE_GROUP:
		qq_group_process_activate_group_reply(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_SEARCH_GROUP:
		qq_process_group_cmd_search_group(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_JOIN_GROUP:
		qq_process_group_cmd_join_group(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_JOIN_GROUP_AUTH:
		qq_process_group_cmd_join_group_auth(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_EXIT_GROUP:
		qq_process_group_cmd_exit_group(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_SEND_MSG:
		qq_process_group_cmd_im(data + bytes, len - bytes, gc);
		break;
	case QQ_GROUP_CMD_GET_ONLINE_MEMBER:
		qq_process_group_cmd_get_online_members(data + bytes, len - bytes, gc);
		if (group != NULL)
			qq_group_conv_refresh_online_member(gc, group);
		break;
	case QQ_GROUP_CMD_GET_MEMBER_INFO:
		qq_process_group_cmd_get_members_info(data + bytes, len - bytes, gc);
		if (group != NULL)
			qq_group_conv_refresh_online_member(gc, group);
		break;
	default:
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			   "Group cmd %s is processed by default\n", qq_group_cmd_get_desc(sub_cmd));
		_qq_process_group_cmd_reply_default(data + bytes, len, gc);
	}
}
