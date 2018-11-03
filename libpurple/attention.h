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

#include <glib.h>
#include <glib-object.h>

/**
 * SECTION:attention
 * @section_id: libpurple-attention
 * @short_description: <filename>attention.h</filename>
 * @title: Attention Object and Interfaces
 */

#define PURPLE_TYPE_ATTENTION_TYPE  (purple_attention_type_get_type())

/**
 * PurpleAttentionType:
 * @name: The name to show in GUI elements.
 * @incoming_description: Shown when received.
 * @outgoing_description: Shown when sent.
 * @icon_name: Optional name of the icon to display.
 * @unlocalized_name: An unlocalized name for UIs that would rather use that.
 *
 * Represents "nudges" and "buzzes" that you may send to a buddy to attract
 * their attention (or vice-versa).
 */
typedef struct _PurpleAttentionType PurpleAttentionType;

#define PURPLE_TYPE_PROTOCOL_ATTENTION           (purple_protocol_attention_get_type())
#define PURPLE_PROTOCOL_ATTENTION(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PROTOCOL_ATTENTION, PurpleProtocolAttention))
#define PURPLE_IS_PROTOCOL_ATTENTION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_ATTENTION))
#define PURPLE_PROTOCOL_ATTENTION_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_ATTENTION, PurpleProtocolAttentionInterface))

typedef struct _PurpleProtocolAttention          PurpleProtocolAttention;
typedef struct _PurpleProtocolAttentionInterface PurpleProtocolAttentionInterface;

#include "account.h"
#include "connection.h"

/**
 * PurpleProtocolAttentionInterface:
 *
 * The protocol attention interface.
 *
 * This interface provides attention API for sending and receiving
 * zaps/nudges/buzzes etc.
 */
struct _PurpleProtocolAttentionInterface
{
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	gboolean (*send)(PurpleProtocolAttention *attn, PurpleConnection *gc, const gchar *username, guint type);

	GList *(*get_types)(PurpleProtocolAttention *attn, PurpleAccount *acct);
};

G_BEGIN_DECLS

/******************************************************************************
 * AttentionType API
 *****************************************************************************/

/**
 * purple_attention_type_get_type:
 *
 * Returns: The #GType for the #PurpleAttentionType boxed structure.
 */
GType purple_attention_type_get_type(void);

PurpleAttentionType *purple_attention_type_copy(PurpleAttentionType *attn);

/**
 * purple_attention_type_new:
 * @unlocalized_name: A non-localized string that can be used by UIs in need of such
 *               non-localized strings.  This should be the same as @name,
 *               without localization.
 * @name: A localized string that the UI may display for the event. This
 *             should be the same string as @unlocalized_name, with localization.
 * @incoming_description: A localized description shown when the event is received.
 * @outgoing_description: A localized description shown when the event is sent.
 *
 * Creates a new #PurpleAttentionType object and sets its mandatory parameters.
 *
 * Returns: A pointer to the new object.
 */
PurpleAttentionType *purple_attention_type_new(const gchar *unlocalized_name, const gchar *name,
								const gchar *incoming_description, const gchar *outgoing_description);

/**
 * purple_attention_type_get_name:
 * @type: The attention type.
 *
 * Get the attention type's name as displayed by the UI.
 *
 * Returns: The name.
 */
const gchar *purple_attention_type_get_name(const PurpleAttentionType *type);

/**
 * purple_attention_type_set_name:
 * @type: The attention type.
 * @name: The localized name that will be displayed by UIs. This should be
 *             the same string given as the unlocalized name, but with
 *             localization.
 *
 * Sets the displayed name of the attention-demanding event.
 */
void purple_attention_type_set_name(PurpleAttentionType *type, const gchar *name);

/**
 * purple_attention_type_get_incoming_desc:
 * @type: The attention type.
 *
 * Get the attention type's description shown when the event is received.
 *
 * Returns: The description.
 */
const gchar *purple_attention_type_get_incoming_desc(const PurpleAttentionType *type);

/**
 * purple_attention_type_set_incoming_desc:
 * @type: The attention type.
 * @desc: The localized description for incoming events.
 *
 * Sets the description of the attention-demanding event shown in conversations
 * when the event is received.
 */
void purple_attention_type_set_incoming_desc(PurpleAttentionType *type, const gchar *desc);

/**
 * purple_attention_type_get_outgoing_desc:
 * @type: The attention type.
 *
 * Get the attention type's description shown when the event is sent.
 *
 * Returns: The description.
 */
const gchar *purple_attention_type_get_outgoing_desc(const PurpleAttentionType *type);

/**
 * purple_attention_type_set_outgoing_desc:
 * @type: The attention type.
 * @desc: The localized description for outgoing events.
 *
 * Sets the description of the attention-demanding event shown in conversations
 * when the event is sent.
 */
void purple_attention_type_set_outgoing_desc(PurpleAttentionType *type, const gchar *desc);

/**
 * purple_attention_type_get_icon_name:
 * @type: The attention type.
 *
 * Get the attention type's icon name.
 *
 * Note: Icons are optional for attention events.
 *
 * Returns: The icon name or %NULL if unset/empty.
 */
const gchar *purple_attention_type_get_icon_name(const PurpleAttentionType *type);

/**
 * purple_attention_type_set_icon_name:
 * @type: The attention type.
 * @name: The icon's name.
 *
 * Sets the name of the icon to display for the attention event; this is optional.
 *
 * Note: Icons are optional for attention events.
 */
void purple_attention_type_set_icon_name(PurpleAttentionType *type, const gchar *name);

/**
 * purple_attention_type_get_unlocalized_name:
 * @type: The attention type
 *
 * Get the attention type's unlocalized name; this is useful for some UIs.
 *
 * Returns: The unlocalized name.
 */
const gchar *purple_attention_type_get_unlocalized_name(const PurpleAttentionType *type);

/**
 * purple_attention_type_set_unlocalized_name:
 * @type: The attention type.
 * @ulname: The unlocalized name.  This should be the same string given as
 *               the localized name, but without localization.
 *
 * Sets the unlocalized name of the attention event; some UIs may need this,
 * thus it is required.
 */
void purple_attention_type_set_unlocalized_name(PurpleAttentionType *type, const gchar *ulname);

/******************************************************************************
 * Protocol Interface
 *****************************************************************************/

/**
 * purple_protocol_attention_get_type:
 *
 * Returns: The #GType for the protocol attention interface.
 */
GType purple_protocol_attention_get_type(void);

/**
 * purple_protocol_attention_get_types:
 * @attn: The #PurpleProtocolAttention.
 * @acct: The #PurpleAccount whose attention types to get.
 *
 * Returns a list of #PurpleAttentionType's for @attn.
 *
 * Returns: (transfer container) (element-type PurpleAttentionType): The list of #PurpleAttentionType's.
 */
GList *purple_protocol_attention_get_types(PurpleProtocolAttention *attn, PurpleAccount *acct);

/**
 * purple_protocol_attention_send:
 * @attn: The #PurpleProtocolAttention instance.
 * @gc: The #PurpleConnection to send on
 * @username: The name of the user to send the attention to.
 * @type: The type of attention to send.
 *
 * Sends an attention message of @type to @username.
 *
 * Returns: TRUE on success, FALSE otherwise.
 */
gboolean purple_protocol_attention_send(PurpleProtocolAttention *attn, PurpleConnection *gc, const gchar *username, guint type);

G_END_DECLS

#endif /* PURPLE_ATTENTION_H */
