/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include <glib.h>
#include <string.h>

#include <purple.h>

#include "test_ui.h"

/******************************************************************************
 * PurpleProtcolAttention Implementations
 *****************************************************************************/
static GType test_purple_protocol_attention_get_type(void);

typedef struct {
	PurpleProtocol parent;

	gboolean send_called;
} TestPurpleProtocolAttention;

typedef struct {
	PurpleProtocolClass parent;
} TestPurpleProtocolAttentionClass;


static gboolean
test_purple_protocol_attention_send(PurpleProtocolAttention *attn, PurpleConnection *c, const gchar *username, guint type) {
	TestPurpleProtocolAttention *test_attn = (TestPurpleProtocolAttention *)attn;

	test_attn->send_called = TRUE;

	return TRUE;
}

static GList *
test_purple_protocol_attention_get_types(PurpleProtocolAttention *attn, PurpleAccount *acct) {
	GList *types = NULL;

	types = g_list_append(types, purple_attention_type_new("id", "name", "incoming", "outgoing"));

	return types;
}

static void
test_purple_protocol_attention_iface_init(PurpleProtocolAttentionInterface *iface) {
	iface->send = test_purple_protocol_attention_send;
	iface->get_types = test_purple_protocol_attention_get_types;
}

G_DEFINE_TYPE_WITH_CODE(
	TestPurpleProtocolAttention,
	test_purple_protocol_attention,
	PURPLE_TYPE_PROTOCOL,
	G_IMPLEMENT_INTERFACE(
		PURPLE_TYPE_PROTOCOL_ATTENTION,
		test_purple_protocol_attention_iface_init
	)
);

static void
test_purple_protocol_attention_init(TestPurpleProtocolAttention *prplattn) {
	PurpleProtocol *prpl = PURPLE_PROTOCOL(prplattn);

	prpl->id = "prpl-attention";
}

static void
test_purple_protocol_attention_class_init(TestPurpleProtocolAttentionClass *klass) {
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_protocol_attention_can_send(void) {
	TestPurpleProtocolAttention *attn = g_object_new(test_purple_protocol_attention_get_type(), NULL);
	PurpleAccount *a = purple_account_new("prpl-attn-can-send", "prpl-attn");
	PurpleConnection *c = g_object_new(PURPLE_TYPE_CONNECTION, "account", a, NULL);
	gboolean actual = FALSE;

	attn->send_called = FALSE;
	actual = purple_protocol_attention_send(PURPLE_PROTOCOL_ATTENTION(attn), c, "someguy", 0);
	g_assert_true(actual);

	g_assert_true(attn->send_called);
}

static void
test_purple_protocol_attention_can_get_types(void) {
	TestPurpleProtocolAttention *attn = g_object_new(test_purple_protocol_attention_get_type(), NULL);
	PurpleAccount *a = purple_account_new("prpl-attn-can-get-types", "prpl-attn");
	GList *types = NULL;
	PurpleAttentionType *type = NULL;

	types = purple_protocol_attention_get_types(PURPLE_PROTOCOL_ATTENTION(attn), a);
	g_assert_true(g_list_length(types) == 1);

	/* take the first item and cast it into type */
	type = (PurpleAttentionType *)(types->data);
	g_assert(type);

	g_assert_cmpstr(purple_attention_type_get_unlocalized_name(type), ==, "id");
	g_assert_cmpstr(purple_attention_type_get_name(type), ==, "name");
	g_assert_cmpstr(purple_attention_type_get_incoming_desc(type), ==, "incoming");
	g_assert_cmpstr(purple_attention_type_get_outgoing_desc(type), ==, "outgoing");
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar **argv) {
	gint res = 0;

	g_test_init(&argc, &argv, NULL);

	g_test_set_nonfatal_assertions();

	test_ui_purple_init();

	g_test_add_func(
		"/protocol-attention/send",
		test_purple_protocol_attention_can_send
	);

	g_test_add_func(
		"/protocol-attention/get-types",
		test_purple_protocol_attention_can_get_types
	);

	res = g_test_run();

	return res;
}
