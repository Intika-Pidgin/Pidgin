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

#include "attention.h"

/******************************************************************************
 * PurpleAttentionType API
 *****************************************************************************/
struct _PurpleAttentionType {
	const gchar *name;
	const gchar *incoming_description;
	const gchar *outgoing_description;
	const gchar *icon_name;
	const gchar *unlocalized_name;
};

G_DEFINE_BOXED_TYPE(
	PurpleAttentionType,
	purple_attention_type,
	purple_attention_type_copy,
	g_free
);

PurpleAttentionType *
purple_attention_type_new(const gchar *unlocalized_name,
                          const gchar *name,
                          const gchar *incoming_description,
                          const gchar *outgoing_description)
{
	PurpleAttentionType *attn = g_new0(PurpleAttentionType, 1);

	attn->unlocalized_name = unlocalized_name;
	attn->name = name;
	attn->incoming_description = incoming_description;
	attn->outgoing_description = outgoing_description;

	return attn;
}

PurpleAttentionType *
purple_attention_type_copy(PurpleAttentionType *attn) {
	PurpleAttentionType *attn_copy = NULL;

	g_return_val_if_fail(attn != NULL, NULL);

	attn_copy  = g_new(PurpleAttentionType, 1);
	*attn_copy = *attn;

	return attn_copy;
}

const gchar *
purple_attention_type_get_name(const PurpleAttentionType *type) {
	g_return_val_if_fail(type, NULL);

	return type->name;
}

void
purple_attention_type_set_name(PurpleAttentionType *type, const gchar *name) {
	g_return_if_fail(type);

	type->name = name;
}

const gchar *
purple_attention_type_get_incoming_desc(const PurpleAttentionType *type) {
	g_return_val_if_fail(type, NULL);

	return type->incoming_description;
}

void
purple_attention_type_set_incoming_desc(PurpleAttentionType *type, const gchar *desc) {
	g_return_if_fail(type);

	type->incoming_description = desc;
}

const gchar *
purple_attention_type_get_outgoing_desc(const PurpleAttentionType *type) {
	g_return_val_if_fail(type, NULL);

	return type->outgoing_description;
}

void
purple_attention_type_set_outgoing_desc(PurpleAttentionType *type, const gchar *desc) {
	g_return_if_fail(type != NULL);

	type->outgoing_description = desc;
}

const gchar *
purple_attention_type_get_icon_name(const PurpleAttentionType *type) {
	g_return_val_if_fail(type, NULL);

	if(type->icon_name == NULL || *(type->icon_name) == '\0')
		return NULL;

	return type->icon_name;
}

void
purple_attention_type_set_icon_name(PurpleAttentionType *type, const gchar *name) {
	g_return_if_fail(type);

	type->icon_name = name;
}

const gchar *
purple_attention_type_get_unlocalized_name(const PurpleAttentionType *type) {
	g_return_val_if_fail(type, NULL);

	return type->unlocalized_name;
}

void
purple_attention_type_set_unlocalized_name(PurpleAttentionType *type, const gchar *ulname) {
	g_return_if_fail(type);

	type->unlocalized_name = ulname;
}

/******************************************************************************
 * PurpleAttentionType API
 *****************************************************************************/
G_DEFINE_INTERFACE(PurpleProtocolAttention, purple_protocol_attention, G_TYPE_INVALID);

static void
purple_protocol_attention_default_init(PurpleProtocolAttentionInterface *iface) {
}

gboolean
purple_protocol_attention_send(PurpleProtocolAttention *attn, PurpleConnection *gc, const gchar *username, guint type) {
	PurpleProtocolAttentionInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_ATTENTION(attn), FALSE);

	iface = PURPLE_PROTOCOL_ATTENTION_GET_IFACE(attn);
	if(iface && iface->send) {
		return iface->send(attn, gc, username, type);
	}

	return FALSE;
}

GList *
purple_protocol_attention_get_types(PurpleProtocolAttention *attn, PurpleAccount *account) {
	PurpleProtocolAttentionInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_ATTENTION(attn), NULL);

	iface = PURPLE_PROTOCOL_ATTENTION_GET_IFACE(attn);
	if(iface && iface->get_types) {
		return iface->get_types(attn, account);
	}

	return NULL;
}

