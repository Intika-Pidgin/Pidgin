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

/******************************************************************************
 * Tests
 *****************************************************************************/

/* This test just verifies that purple_circular_buffer_new creates this
 * properly according to the docs.
 */
static void
test_circular_buffer_new(void) {
	PurpleCircularBuffer *buffer = purple_circular_buffer_new(0);

	g_assert_true(PURPLE_IS_CIRCULAR_BUFFER(buffer));

	g_assert_cmpuint(256, ==, purple_circular_buffer_get_grow_size(buffer));
	g_assert_cmpuint(0, ==, purple_circular_buffer_get_used(buffer));
	g_assert_cmpuint(0, ==, purple_circular_buffer_get_max_read(buffer));

	g_object_unref(buffer);
}

/* This test make sure that purple_circular_buffer_reset works as described in
 * the documentation.
 */
static void
test_circular_buffer_reset(void) {
	PurpleCircularBuffer *buffer = purple_circular_buffer_new(0);
	const gchar *data;

	purple_circular_buffer_append(buffer, "abc\0", 4);
	g_assert_cmpuint(4, ==, purple_circular_buffer_get_used(buffer));
	g_assert_cmpuint(4, ==, purple_circular_buffer_get_max_read(buffer));

	purple_circular_buffer_get_output(buffer);
	data = purple_circular_buffer_get_output(buffer);
	g_assert_cmpstr("abc", ==, data);

	purple_circular_buffer_reset(buffer);

	data = purple_circular_buffer_get_output(buffer);
	g_assert_cmpstr("abc", ==, data);

	g_object_unref(buffer);
}

/* This test verifies that purple_circular_buffer_mark_read works as described
 * in the documentation.
 */
static void
test_circular_buffer_mark_read(void) {
	PurpleCircularBuffer *buffer = purple_circular_buffer_new(0);
	const gchar *data;

	purple_circular_buffer_append(buffer, "abc\0", 4);
	g_assert_cmpuint(4, ==, purple_circular_buffer_get_used(buffer));
	g_assert_cmpuint(4, ==, purple_circular_buffer_get_max_read(buffer));

	/* force a read to move the output */
	purple_circular_buffer_get_output(buffer);
	data = purple_circular_buffer_get_output(buffer);
	g_assert_cmpstr("abc", ==, data);

	purple_circular_buffer_mark_read(buffer, 4);

	g_assert_cmpuint(0, ==, purple_circular_buffer_get_used(buffer));
	g_assert_cmpuint(0, ==, purple_circular_buffer_get_max_read(buffer));

	g_object_unref(buffer);
}

/* this test verifies that the buffer will grow 1 grow size to fit the data. */
static void
test_circular_buffer_single_default_grow(void) {
	PurpleCircularBuffer *buffer = purple_circular_buffer_new(0);
	const gchar *data;

	purple_circular_buffer_append(buffer, "abc\0", 4);

	g_assert_cmpuint(4, ==, purple_circular_buffer_get_used(buffer));
	g_assert_cmpuint(4, ==, purple_circular_buffer_get_max_read(buffer));

	data = purple_circular_buffer_get_output(buffer);

	g_assert_cmpstr("abc", ==, data);

	g_object_unref(buffer);
}

/* this test create a circular buffer with a grow size of 1 to easily test
 * multiple grows.
 */
static void
test_circular_buffer_multiple_grows(void) {
	PurpleCircularBuffer *buffer = purple_circular_buffer_new(1);

	purple_circular_buffer_append(buffer, "abcdefghijklmnopqrstuvwxyz\0", 27);

	g_assert_cmpuint(27, ==, purple_circular_buffer_get_used(buffer));
	g_assert_cmpuint(27, ==, purple_circular_buffer_get_max_read(buffer));

	g_assert_cmpstr("abcdefghijklmnopqrstuvwxyz", ==, purple_circular_buffer_get_output(buffer));

	g_object_unref(buffer);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_set_nonfatal_assertions();

	g_test_add_func("/circular_buffer/new", test_circular_buffer_new);
	g_test_add_func("/circular_buffer/reset", test_circular_buffer_reset);
	g_test_add_func("/circular_buffer/mark_read", test_circular_buffer_mark_read);
	g_test_add_func("/circular_buffer/single_default_grow", test_circular_buffer_single_default_grow);
	g_test_add_func("/circular_buffer/multiple_grows", test_circular_buffer_multiple_grows);

	return g_test_run();
}
