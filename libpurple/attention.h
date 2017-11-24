/* purple
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

#ifndef PURPLE_ATTENTION_H
#define PURPLE_ATTENTION_H

/**
 * SECTION:attention
 * @section_id: libpurple-attention
 * @short_description: <filename>attention.h</filename>
 * @title: Attention Object and Interfaces
 */

#define PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE     (purple_protocol_attention_iface_get_type())

typedef struct _PurpleProtocolAttentionIface PurpleProtocolAttentionIface;

/**
 * PurpleProtocolAttentionIface:
 *
 * The protocol attention interface.
 *
 * This interface provides attention API for sending and receiving
 * zaps/nudges/buzzes etc.
 */
struct _PurpleProtocolAttentionIface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	gboolean (*send)(PurpleConnection *gc, const char *username,
							   guint type);

	GList *(*get_types)(PurpleAccount *acct);
};

#define PURPLE_PROTOCOL_HAS_ATTENTION_IFACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE))
#define PURPLE_PROTOCOL_GET_ATTENTION_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE, \
                                                  PurpleProtocolAttentionIface))

#endif /* PURPLE_ATTENTION_H */
