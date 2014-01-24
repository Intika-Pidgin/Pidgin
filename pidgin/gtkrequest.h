/**
 * @file gtkrequest.h GTK+ Request API
 * @ingroup pidgin
 */

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

#include "request.h"

G_BEGIN_DECLS

/**
 * Returns the UI operations structure for GTK+ request functions.
 *
 * @return The GTK+ UI request operations structure.
 */
PurpleRequestUiOps *pidgin_request_get_ui_ops(void);

/**
 * Gets dialog window for specified libpurple request.
 *
 * @param ui_handle The UI handle.
 *
 * @return The dialog window.
 */
GtkWindow *
pidgin_request_get_dialog_window(void *ui_handle);

/**************************************************************************/
/** @name GTK+ Requests Subsystem                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns the gtk requests subsystem handle.
 *
 * @return The requests subsystem handle.
 */
void *pidgin_request_get_handle(void);

/**
 * Initializes the GTK+ requests subsystem.
 */
void pidgin_request_init(void);

/**
 * Uninitializes the GTK+ requests subsystem.
 */
void pidgin_request_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PIDGINREQUEST_H_ */
