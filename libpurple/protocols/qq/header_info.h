/**
 * @file header_info.h
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

#ifndef _QQ_HEADER_INFO_H_
#define _QQ_HEADER_INFO_H_

#include <glib.h>

#define QQ_UDP_HEADER_LENGTH    7
#define QQ_TCP_HEADER_LENGTH    9

#define QQ_PACKET_TAG           0x02	/* all QQ text packets starts with it */
#define QQ_PACKET_TAIL          0x03	/* all QQ text packets end with it */

#define QQ_CLIENT       0x0d55

/* list of known QQ commands */
enum {
	QQ_CMD_LOGOUT = 0x0001,				/* log out */
	QQ_CMD_KEEP_ALIVE = 0x0002,			/* get onlines from tencent */
	QQ_CMD_UPDATE_INFO = 0x0004,			/* update information */
	QQ_CMD_SEARCH_USER = 0x0005,			/* search for user */
	QQ_CMD_GET_USER_INFO = 0x0006,			/* get user information */
	QQ_CMD_ADD_FRIEND_WO_AUTH = 0x0009,		/* add friend without auth */
	QQ_CMD_DEL_FRIEND = 0x000a,			/* delete a friend  */
	QQ_CMD_BUDDY_AUTH = 0x000b,			/* buddy authentication */
	QQ_CMD_CHANGE_ONLINE_STATUS = 0x000d,		/* change my online status */
	QQ_CMD_ACK_SYS_MSG = 0x0012,			/* ack system message */
	QQ_CMD_SEND_IM = 0x0016,			/* send message */
	QQ_CMD_RECV_IM = 0x0017,			/* receive message */
	QQ_CMD_REMOVE_SELF = 0x001c,			/* remove self */
	QQ_CMD_REQUEST_KEY = 0x001d,			/* request key for file transfer */
	QQ_CMD_CELL_PHONE_1 = 0x0021,			/* cell phone 1 */
	QQ_CMD_LOGIN = 0x0022,				/* login */
	QQ_CMD_GET_FRIENDS_LIST = 0x0026,		/* retrieve my freinds list */
	QQ_CMD_GET_FRIENDS_ONLINE = 0x0027,		/* get my online friends list */
	QQ_CMD_CELL_PHONE_2 = 0x0029,			/* cell phone 2 */
	QQ_CMD_GROUP_CMD = 0x0030,			/* group command */
	QQ_CMD_GET_ALL_LIST_WITH_GROUP = 0x0058,  
	QQ_CMD_GET_LEVEL = 0x005C,			/* get level for one or more buddies */
	QQ_CMD_REQUEST_LOGIN_TOKEN  = 0x0062, 		/* get login token */
	QQ_CMD_RECV_MSG_SYS = 0x0080,			/* receive a system message */
	QQ_CMD_RECV_MSG_FRIEND_CHANGE_STATUS = 0x0081,	/* friends change status */
};

const gchar *qq_get_cmd_desc(gint type);

const gchar *qq_get_source_str(gint source);

#endif
