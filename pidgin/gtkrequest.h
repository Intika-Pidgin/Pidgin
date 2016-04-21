/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#ifndef _PIDGINREQUEST_H_
#define _PIDGINREQUEST_H_
/**
 * SECTION:gtkrequest
 * @section_id: pidgin-gtkrequest
 * @short_description: <filename>gtkrequest.h</filename>
 * @title: Request API
 */

#include "request.h"

G_BEGIN_DECLS

/**
 * pidgin_request_get_ui_ops:
 *
 * Returns the UI operations structure for GTK+ request functions.
 *
 * Returns: The GTK+ UI request operations structure.
 */
PurpleRequestUiOps *pidgin_request_get_ui_ops(void);

/**
 * pidgin_request_get_dialog_window:
 * @ui_handle: The UI handle.
 *
 * Gets dialog window for specified libpurple request.
 *
 * Returns: The dialog window.
 */
GtkWindow *
pidgin_request_get_dialog_window(void *ui_handle);

/**************************************************************************/
/* GTK+ Requests Subsystem                                                */
/**************************************************************************/

/**
 * pidgin_request_get_handle:
 *
 * Returns the gtk requests subsystem handle.
 *
 * Returns: The requests subsystem handle.
 */
void *pidgin_request_get_handle(void);

/**
 * pidgin_request_init:
 *
 * Initializes the GTK+ requests subsystem.
 */
void pidgin_request_init(void);

/**
 * pidgin_request_uninit:
 *
 * Uninitializes the GTK+ requests subsystem.
 */
void pidgin_request_uninit(void);

G_END_DECLS

#endif /* _PIDGINREQUEST_H_ */
