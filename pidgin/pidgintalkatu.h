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

#ifndef PIDGIN_TALKATU_H
#define PIDGIN_TALKATU_H

/**
 * SECTION:pidgintalkatu
 * @section_id: pidgin-talkatu
 * @short_description: <filename>pidgintalkatu.h</filename>
 * @title: Talkatu Helpers
 */

#include <gtk/gtk.h>

#include <talkatu.h>

#include "connection.h"

G_BEGIN_DECLS

/**
 * pidgin_talkatu_editor_new_for_connection:
 * @pc: The connection for which the editor should be created
 *
 * Creates a new #TalkatuEditor appropriate for use with @pc.
 *
 * Returns: (transfer full): the new #TalkatuEditor
 */
GtkWidget *pidgin_talkatu_editor_new_for_connection(PurpleConnection *pc);

/**
 * pidgin_talkatu_buffer_new_for_connection:
 * @pc: The connection for which the buffer should be created
 *
 * Creates a new #TalkatuBuffer appropriate for use with @pc.
 *
 * Returns: (transfer full): the new #TalkatuBuffer
 */
GtkTextBuffer *pidgin_talkatu_buffer_new_for_connection(PurpleConnection *pc);

G_END_DECLS

#endif /* PIDGIN_TALKATU_H */
