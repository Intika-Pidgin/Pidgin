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

#ifndef _GNT_REQUEST_H
#define _GNT_REQUEST_H
/**
 * SECTION:gntrequest
 * @section_id: finch-gntrequest
 * @short_description: <filename>gntrequest.h</filename>
 * @title: Request API
 */

#include "request.h"
#include "gnt.h"

/**********************************************************************
 * GNT Request API
 **********************************************************************/

/**
 * finch_request_get_ui_ops:
 *
 * Get the ui-functions.
 *
 * Returns: The PurpleRequestUiOps structure populated with the appropriate functions.
 */
PurpleRequestUiOps *finch_request_get_ui_ops(void);

/**
 * finch_request_init:
 *
 * Perform necessary initializations.
 */
void finch_request_init(void);

/**
 * finch_request_uninit:
 *
 * Perform necessary uninitializations.
 */
void finch_request_uninit(void);

/**
 * finch_request_save_in_prefs:
 *
 * Save the request fields in preferences where the id attribute of each field is the
 * id of a preference.
 */
void finch_request_save_in_prefs(gpointer null, PurpleRequestFields *fields);

/**
 * finch_request_field_get_widget:
 * @field:   The request field.
 *
 * Create a widget field for a request-field.
 *
 * Returns: (transfer full): A GntWidget for the request field.
 */
GntWidget *finch_request_field_get_widget(PurpleRequestField *field);

#endif
