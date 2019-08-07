/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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

#ifndef _GNT_ROOMLIST_H
#define _GNT_ROOMLIST_H
/**
 * SECTION:gntroomlist
 * @section_id: finch-gntroomlist
 * @short_description: <filename>gntroomlist.h</filename>
 * @title: Room List API
 */

#include "roomlist.h"

/**********************************************************************
 * GNT Room List API
 **********************************************************************/

/**
 * finch_roomlist_init:
 *
 * Initialize the roomlist subsystem.
 */
void finch_roomlist_init(void);

/**
 * finch_roomlist_get_ui_ops:
 *
 * Get the ui-functions.
 *
 * Returns: The PurpleRoomlistUiOps structure populated with the appropriate functions.
 */
PurpleRoomlistUiOps *finch_roomlist_get_ui_ops(void);

/**
 * finch_roomlist_show_all:
 *
 * Show the roomlist dialog.
 */
void finch_roomlist_show_all(void);

/**
 * finch_roomlist_uninit:
 *
 * Uninitialize the roomlist subsystem.
 */
void finch_roomlist_uninit(void);

#endif

