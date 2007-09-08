/**
 * file login_logout.h
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

#ifndef _QQ_LOGIN_LOGOUT_H_
#define _QQ_LOGIN_LOGOUT_H_

#include <glib.h>
#include "connection.h"

#define QQ_LOGIN_MODE_NORMAL        0x0a
#define QQ_LOGIN_MODE_AWAY	    0x1e
#define QQ_LOGIN_MODE_HIDDEN        0x28

void qq_send_packet_request_login_token(PurpleConnection *gc);
void qq_process_request_login_token_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);
void qq_process_login_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);
void qq_send_packet_logout(PurpleConnection *gc);

#endif
