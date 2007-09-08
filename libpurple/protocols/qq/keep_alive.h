/**
 * @file keep_alive.h
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
 *
 */

#ifndef _QQ_KEEP_ALIVE_H_
#define _QQ_KEEP_ALIVE_H_

#include <glib.h>
#include "connection.h"
#include "qq.h"

void qq_send_packet_keep_alive(PurpleConnection *gc);

void qq_process_keep_alive_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);
void qq_refresh_all_buddy_status(PurpleConnection *gc);

void qq_update_buddy_contact(PurpleConnection *gc, qq_buddy *q_bud);

#endif
