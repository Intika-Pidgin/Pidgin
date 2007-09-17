/**
 * @file group_info.c
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

#include "conversation.h"
#include "debug.h"

#include "buddy_status.h"
#include "char_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "buddy_status.h"
#include "group_network.h"

/* we check who needs to update member info every minutes
 * this interval determines if their member info is outdated */
#define QQ_GROUP_CHAT_REFRESH_NICKNAME_INTERNAL  180

static gboolean _is_group_member_need_update_info(qq_buddy *member)
{
	g_return_val_if_fail(member != NULL, FALSE);
	return (member->nickname == NULL) ||
	    (time(NULL) - member->last_refresh) > QQ_GROUP_CHAT_REFRESH_NICKNAME_INTERNAL;
}

/* this is done when we receive the reply to get_online_members sub_cmd
 * all member are set offline, and then only those in reply packets are online */
static void _qq_group_set_members_all_offline(qq_group *group)
{
	GList *list;
	qq_buddy *member;
	g_return_if_fail(group != NULL);

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		member->status = QQ_BUDDY_ONLINE_OFFLINE;
		list = list->next;
	}
}

/* send packet to get detailed information of one group */
void qq_send_cmd_group_get_group_info(PurpleConnection *gc, qq_group *group)
{
	guint8 *raw_data, *cursor;
	gint bytes, data_len;

	g_return_if_fail(group != NULL);

	data_len = 5;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_GET_GROUP_INFO);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);

	if (bytes != data_len)
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_GET_GROUP_INFO));
	else
		qq_send_group_cmd(gc, group, raw_data, data_len);
}

/* send packet to get online group member, called by keep_alive */
void qq_send_cmd_group_get_online_members(PurpleConnection *gc, qq_group *group)
{
	guint8 *raw_data, *cursor;
	gint bytes, data_len;

	g_return_if_fail(group != NULL);

	/* only get online members when conversation window is on */
	if (NULL == purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,group->group_name_utf8, purple_connection_get_account(gc))) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			   "Conv windows for \"%s\" is not on, do not get online members\n", group->group_name_utf8);
		return;
	}

	data_len = 5;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_GET_ONLINE_MEMBER);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);

	if (bytes != data_len)
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_GET_ONLINE_MEMBER));
	else
		qq_send_group_cmd(gc, group, raw_data, data_len);
}

/* send packet to get info for each group member */
void qq_send_cmd_group_get_members_info(PurpleConnection *gc, qq_group *group)
{
	guint8 *raw_data, *cursor;
	gint bytes, data_len, i;
	GList *list;
	qq_buddy *member;

	g_return_if_fail(group != NULL);
	for (i = 0, list = group->members; list != NULL; list = list->next) {
		member = (qq_buddy *) list->data;
		if (_is_group_member_need_update_info(member))
			i++;
	}

	if (i <= 0) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "No group member needs to to update info now.\n");
		return;
	}

	data_len = 5 + 4 * i;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_GET_MEMBER_INFO);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		if (_is_group_member_need_update_info(member))
			bytes += create_packet_dw(raw_data, &cursor, member->uid);
		list = list->next;
	}

	if (bytes != data_len)
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_GET_MEMBER_INFO));
	else
		qq_send_group_cmd(gc, group, raw_data, data_len);
}

void qq_process_group_cmd_get_group_info(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc)
{
	qq_group *group;
	qq_buddy *member;
	qq_data *qd;
	PurpleConversation *purple_conv;
	guint8 organization, role;
	guint16 unknown, max_members;
	guint32 member_uid, internal_group_id, external_group_id;
	GSList *pending_id;
	gint pascal_len, i;
	guint32 unknown4;
	guint8 unknown1;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	read_packet_dw(data, cursor, len, &(internal_group_id));
	g_return_if_fail(internal_group_id > 0);
	read_packet_dw(data, cursor, len, &(external_group_id));
	g_return_if_fail(internal_group_id > 0);

	pending_id = qq_get_pending_id(qd->adding_groups_from_server, internal_group_id);
	if (pending_id != NULL) {
		qq_set_pending_id(&qd->adding_groups_from_server, internal_group_id, FALSE);
		qq_group_create_internal_record(gc, internal_group_id, external_group_id, NULL);
	}

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	read_packet_b(data, cursor, len, &(group->group_type));
	read_packet_dw(data, cursor, len, &unknown4);	/* unknown 4 bytes */
	read_packet_dw(data, cursor, len, &(group->creator_uid));
	read_packet_b(data, cursor, len, &(group->auth_type));
	read_packet_dw(data, cursor, len, &unknown4);	/* oldCategory */
	read_packet_w(data, cursor, len, &unknown);	
	read_packet_dw(data, cursor, len, &(group->group_category));
	read_packet_w(data, cursor, len, &max_members);
	read_packet_b(data, cursor, len, &unknown1);
	read_packet_dw(data, cursor, len, &(unknown4));	/* versionID */

	pascal_len = convert_as_pascal_string(*cursor, &(group->group_name_utf8), QQ_CHARSET_DEFAULT);
	*cursor += pascal_len;
	read_packet_w(data, cursor, len, &(unknown));	/* 0x0000 */
	pascal_len = convert_as_pascal_string(*cursor, &(group->notice_utf8), QQ_CHARSET_DEFAULT);
	*cursor += pascal_len;
	pascal_len = convert_as_pascal_string(*cursor, &(group->group_desc_utf8), QQ_CHARSET_DEFAULT);
	*cursor += pascal_len;

	i = 0;
	/* now comes the member list separated by 0x00 */
	while (*cursor < data + len) {
		read_packet_dw(data, cursor, len, &member_uid);
		i++;
		read_packet_b(data, cursor, len, &organization);
		read_packet_b(data, cursor, len, &role);

		if(organization != 0 || role != 0) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ", "group member %d: organization=%d, role=%d\n", member_uid, organization, role);
		}
		member = qq_group_find_or_add_member(gc, group, member_uid);
		if (member != NULL)
			member->role = role;
	}
        if(*cursor > (data + len)) {
                         purple_debug(PURPLE_DEBUG_ERROR, "QQ", "group_cmd_get_group_info: Dangerous error! maybe protocol changed, notify me!");
        }

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group \"%s\" has %d members\n", group->group_name_utf8, i);

	if (group->creator_uid == qd->uid)
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_ADMIN;

	qq_group_refresh(gc, group);

	purple_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, 
			group->group_name_utf8, purple_connection_get_account(gc));
	if(NULL == purple_conv) {
                purple_debug(PURPLE_DEBUG_WARNING, "QQ",
                           "Conv windows for \"%s\" is not on, do not set topic\n", group->group_name_utf8);
	}
	else {
		purple_conv_chat_set_topic(PURPLE_CONV_CHAT(purple_conv), NULL, group->notice_utf8);
	}
}

void qq_process_group_cmd_get_online_members(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc)
{
	guint32 internal_group_id, member_uid;
	guint8 unknown;
	gint bytes, i;
	qq_group *group;
	qq_buddy *member;

	g_return_if_fail(data != NULL && len > 0);

	if (data + len - *cursor < 4) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Invalid group online member reply, discard it!\n");
		return;
	}

	bytes = 0;
	i = 0;
	bytes += read_packet_dw(data, cursor, len, &internal_group_id);
	bytes += read_packet_b(data, cursor, len, &unknown);	/* 0x3c ?? */
	g_return_if_fail(internal_group_id > 0);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"We have no group info for internal id [%d]\n", internal_group_id);
		return;
	}

	/* set all offline first, then update those online */
	_qq_group_set_members_all_offline(group);
	while (*cursor < data + len) {
		bytes += read_packet_dw(data, cursor, len, &member_uid);
		i++;
		member = qq_group_find_or_add_member(gc, group, member_uid);
		if (member != NULL)
			member->status = QQ_BUDDY_ONLINE_NORMAL;
	}
        if(*cursor > (data + len)) {
                         purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					 "group_cmd_get_online_members: Dangerous error! maybe protocol changed, notify developers!");
        }

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Group \"%s\" has %d online members\n", group->group_name_utf8, i);
}

/* process the reply to get_members_info packet */
void qq_process_group_cmd_get_members_info(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc)
{
	guint32 internal_group_id, member_uid;
	guint16 unknown;
	gint pascal_len, i;
	qq_group *group;
	qq_buddy *member;

	g_return_if_fail(data != NULL && len > 0);

	read_packet_dw(data, cursor, len, &internal_group_id);
	g_return_if_fail(internal_group_id > 0);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	i = 0;
	/* now starts the member info, as get buddy list reply */
	while (*cursor < data + len) {
		read_packet_dw(data, cursor, len, &member_uid);
		g_return_if_fail(member_uid > 0);
		member = qq_group_find_member_by_uid(group, member_uid);
		g_return_if_fail(member != NULL);

		i++;
		read_packet_w(data, cursor, len, &(member->face));
		read_packet_b(data, cursor, len, &(member->age));
		read_packet_b(data, cursor, len, &(member->gender));
		pascal_len = convert_as_pascal_string(*cursor, &(member->nickname), QQ_CHARSET_DEFAULT);
		*cursor += pascal_len;
		read_packet_w(data, cursor, len, &unknown);
		read_packet_b(data, cursor, len, &(member->flag1));
		read_packet_b(data, cursor, len, &(member->comm_flag));

		member->last_refresh = time(NULL);
	}
        if(*cursor > (data + len)) {
                         purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					 "group_cmd_get_members_info: Dangerous error! maybe protocol changed, notify developers!");
        }
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Group \"%s\" obtained %d member info\n", group->group_name_utf8, i);
}
