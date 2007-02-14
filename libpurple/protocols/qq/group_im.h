/**
 * @file group_im.h
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _QQ_GROUP_IM_H_
#define _QQ_GROUP_IM_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

void qq_send_packet_group_im(GaimConnection *gc, qq_group *group, const gchar *msg);
void qq_process_group_cmd_im(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc);
void qq_process_recv_group_im(guint8 *data, 
		guint8 **cursor, gint data_len, guint32 internal_group_id, GaimConnection *gc, guint16 im_type);
void qq_process_recv_group_im_apply_join(guint8 *data,
				    guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc);
void qq_process_recv_group_im_been_rejected(guint8 *data,
				       guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc);
void qq_process_recv_group_im_been_approved(guint8 *data,
				       guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc);
void qq_process_recv_group_im_been_removed(guint8 *data,
				      guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc);
void qq_process_recv_group_im_been_added(guint8 *data,
				    guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc);
#endif
