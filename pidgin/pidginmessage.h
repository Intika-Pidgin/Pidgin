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

#ifndef PIDGIN_MESSAGE_H
#define PIDGIN_MESSAGE_H

/**
 * SECTION:pidginmessage
 * @section_id: pidgin-pidginmessage
 * @short_description: <filename>pidginmessage.h</filename>
 * @title: Pidgin Message API
 */

#include <purple.h>

#include <talkatu.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_MESSAGE (pidgin_message_get_type())
G_DECLARE_FINAL_TYPE(PidginMessage, pidgin_message, PIDGIN, MESSAGE, GObject)

/**
 * pidgin_message_new:
 * @msg: The #PurpleMessage to wrap.
 *
 * Wraps @msg so that it can be used as a #TalkatuMessage.
 *
 * Returns: (transfer full): The new #PidginMessage instance.
 */
PidginMessage *pidgin_message_new(PurpleMessage *msg);

/**
 * pidgin_message_get_message:
 * @msg: The #PidginMessage instance.
 *
 * Gets the #PurpleMessage that @msg is wrapping.
 *
 * Returns: (transfer none): The #PurpleMessage that @msg is wrapping.
 */
PurpleMessage *pidgin_message_get_message(PidginMessage *msg);

G_END_DECLS

#endif /* PIDGIN_MESSAGE_H */
