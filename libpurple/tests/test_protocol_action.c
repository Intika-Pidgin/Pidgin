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

#include <purple.h>

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
_test_purple_protocol_action_callback(PurpleProtocolAction *action) {
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_protocol_action_new(void) {
	PurpleProtocolAction *action = NULL;

	action = purple_protocol_action_new(
		"label",
		_test_purple_protocol_action_callback
	);
	g_assert_nonnull(action);

	g_assert_cmpstr(action->label, ==, "label");
	g_assert(action->callback == _test_purple_protocol_action_callback);

	purple_protocol_action_free(action);
}

static void
test_purple_protocol_action_copy(void) {
	PurpleProtocolAction *orig = NULL, *copy = NULL;

	orig = purple_protocol_action_new(
		"label",
		_test_purple_protocol_action_callback
	);
	g_assert_nonnull(orig);

	copy = purple_protocol_action_copy(orig);
	purple_protocol_action_free(orig);

	g_assert_cmpstr(copy->label, ==, "label");
	g_assert(copy->callback == _test_purple_protocol_action_callback);

	purple_protocol_action_free(copy);
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
		"/protocol-action/new",
		test_purple_protocol_action_new
	);

	g_test_add_func(
		"/protocol-action/copy",
		test_purple_protocol_action_copy
	);

	res = g_test_run();

	return res;
}
