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

#include "dbus-server.h"

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_attention_type_new(void) {
	PurpleAttentionType *type = NULL;

	type = purple_attention_type_new("id", "name", "incoming", "outgoing");

	/* check the values new sets */
	g_assert_cmpstr(purple_attention_type_get_unlocalized_name(type), ==, "id");
	g_assert_cmpstr(purple_attention_type_get_name(type), ==, "name");
	g_assert_cmpstr(purple_attention_type_get_incoming_desc(type), ==, "incoming");
	g_assert_cmpstr(purple_attention_type_get_outgoing_desc(type), ==, "outgoing");

	/* check that default values are correct */
	g_assert_cmpstr(purple_attention_type_get_icon_name(type), ==, NULL);

	g_free(type);
}

static void
test_purple_attention_type_copy(void) {
	PurpleAttentionType *type1 = NULL, *type2 = NULL;

	type1 = purple_attention_type_new("id", "name", "incoming", "outgoing");

	type2 = purple_attention_type_copy(type1);

	/* check the values new sets */
	g_assert_cmpstr(purple_attention_type_get_unlocalized_name(type2), ==, "id");
	g_assert_cmpstr(purple_attention_type_get_name(type2), ==, "name");
	g_assert_cmpstr(purple_attention_type_get_incoming_desc(type2), ==, "incoming");
	g_assert_cmpstr(purple_attention_type_get_outgoing_desc(type2), ==, "outgoing");

	/* check that default values are correct */
	g_assert_cmpstr(purple_attention_type_get_icon_name(type2), ==, NULL);

	g_free(type1);
	g_free(type2);
}

static void
test_purple_attention_type_set_unlocalized_name(void) {
	PurpleAttentionType *type = NULL;

	type = purple_attention_type_new("id", "name", "incoming", "outgoing");

	purple_attention_type_set_unlocalized_name(type, "this-is-my-id");
	g_assert_cmpstr(purple_attention_type_get_unlocalized_name(type), ==, "this-is-my-id");

	g_free(type);
}

static void
test_purple_attention_type_set_name(void) {
	PurpleAttentionType *type = NULL;

	type = purple_attention_type_new("id", "name", "incoming", "outgoing");

	purple_attention_type_set_name(type, "this-is-my-name");
	g_assert_cmpstr(purple_attention_type_get_name(type), ==, "this-is-my-name");

	g_free(type);
}

static void
test_purple_attention_type_set_incoming_desc(void) {
	PurpleAttentionType *type = NULL;

	type = purple_attention_type_new("id", "name", "incoming", "outgoing");

	purple_attention_type_set_incoming_desc(type, "this-is-my-incoming-desc");
	g_assert_cmpstr(purple_attention_type_get_incoming_desc(type), ==, "this-is-my-incoming-desc");

	g_free(type);
}

static void
test_purple_attention_type_set_outgoing_desc(void) {
	PurpleAttentionType *type = NULL;

	type = purple_attention_type_new("id", "name", "incoming", "outgoing");

	purple_attention_type_set_outgoing_desc(type, "this-is-my-outgoing-desc");
	g_assert_cmpstr(purple_attention_type_get_outgoing_desc(type), ==, "this-is-my-outgoing-desc");

	g_free(type);
}

static void
test_purple_attention_type_set_icon_name(void) {
	PurpleAttentionType *type = NULL;

	type = purple_attention_type_new("id", "name", "incoming", "outgoing");

	purple_attention_type_set_icon_name(type, "this-is-my-icon-name");
	g_assert_cmpstr(purple_attention_type_get_icon_name(type), ==, "this-is-my-icon-name");

	g_free(type);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar **argv) {
	gint res = 0;

	g_test_init(&argc, &argv, NULL);

	g_test_set_nonfatal_assertions();

	g_test_add_func(
		"/attention-type/new",
		test_purple_attention_type_new
	);

	g_test_add_func(
		"/attention-type/copy",
		test_purple_attention_type_copy
	);

	g_test_add_func(
		"/attention-type/set-unlocalized-name",
		test_purple_attention_type_set_unlocalized_name
	);

	g_test_add_func(
		"/attention-type/set-name",
		test_purple_attention_type_set_name
	);

	g_test_add_func(
		"/attention-type/set-incoming-desc",
		test_purple_attention_type_set_incoming_desc
	);

	g_test_add_func(
		"/attention-type/set-outgoing-desc",
		test_purple_attention_type_set_outgoing_desc
	);

	g_test_add_func(
		"/attention-type/set-icon-name",
		test_purple_attention_type_set_icon_name
	);

	res = g_test_run();

	return res;
}
