/**
 * @file gntconn.h GNT Connection API
 * @ingroup finch
 */

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
#ifndef _GNT_CONN_H
#define _GNT_CONN_H

#include "connection.h"

/**********************************************************************
 * @name GNT Connection API
 **********************************************************************/
/*@{*/

/**
 * Get the ui-functions.
 *
 * @return The PurpleConnectionUiOps structure populated with the appropriate functions.
 */
PurpleConnectionUiOps *finch_connections_get_ui_ops(void);

/**
 * Perform necessary initializations.
 */
void finch_connections_init(void);

/**
 * Perform necessary uninitializations.
 */
void finch_connections_uninit(void);

/*@}*/

#endif
