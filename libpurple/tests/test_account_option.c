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
 * Helpers
 *****************************************************************************/
static void
test_purple_account_option_compare(PurpleAccountOption *opt1,
                                   PurpleAccountOption *opt2)
{
	g_assert_cmpint(purple_account_option_get_pref_type(opt1),
	                ==,
	                purple_account_option_get_pref_type(opt2));
	g_assert_cmpstr(purple_account_option_get_text(opt1),
	                ==,
	                purple_account_option_get_text(opt2));
	g_assert_cmpstr(purple_account_option_get_setting(opt1),
	                ==,
	                purple_account_option_get_setting(opt2));
}

static void
test_purple_account_option_compare_string(PurpleAccountOption *opt1,
                                          PurpleAccountOption *opt2)
{
	test_purple_account_option_compare(opt1, opt2);

	g_assert_cmpstr(purple_account_option_get_default_string(opt1),
	                ==,
	                purple_account_option_get_default_string(opt2));
	g_assert_cmpint(purple_account_option_string_get_masked(opt1),
	                ==,
	                purple_account_option_string_get_masked(opt2));
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_account_option_copy_int(void) {
	PurpleAccountOption *opt1, *opt2;

	opt1 = purple_account_option_new(PURPLE_PREF_INT, "int", "test-int");
	opt2 = purple_account_option_copy(opt1);

	test_purple_account_option_compare(opt1, opt2);
}

static void
test_purple_account_option_copy_int_with_default(void) {
	PurpleAccountOption *opt1, *opt2;

	opt1 = purple_account_option_new(PURPLE_PREF_INT, "int", "test-int");
	purple_account_option_set_default_int(opt1, 42);

	opt2 = purple_account_option_copy(opt1);

	test_purple_account_option_compare(opt1, opt2);

	g_assert_cmpint(purple_account_option_get_default_int(opt1),
	                ==,
	                purple_account_option_get_default_int(opt2));
}

static void
test_purple_account_option_copy_string(void) {
	PurpleAccountOption *opt1, *opt2;

	opt1 = purple_account_option_new(PURPLE_PREF_STRING, "string", "test-string");

	opt2 = purple_account_option_copy(opt1);
	test_purple_account_option_compare_string(opt1, opt2);
}

static void
test_purple_account_option_copy_string_with_default(void) {
	PurpleAccountOption *opt1, *opt2;

	opt1 = purple_account_option_new(PURPLE_PREF_STRING, "string", "test-string");
	purple_account_option_set_default_string(opt1, "default");

	opt2 = purple_account_option_copy(opt1);
	test_purple_account_option_compare_string(opt1, opt2);
}

static void
test_purple_account_option_copy_string_with_masked(void) {
	PurpleAccountOption *opt1, *opt2;

	opt1 = purple_account_option_new(PURPLE_PREF_STRING, "string", "test-string");
	purple_account_option_string_set_masked(opt1, TRUE);

	opt2 = purple_account_option_copy(opt1);
	test_purple_account_option_compare_string(opt1, opt2);
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
		"/account_option/copy/int",
		test_purple_account_option_copy_int
	);

	g_test_add_func(
		"/account_option/copy/int_with_default",
		test_purple_account_option_copy_int_with_default
	);

	g_test_add_func(
		"/account_option/copy/string",
		test_purple_account_option_copy_string
	);

	g_test_add_func(
		"/account_option/copy/string_with_default",
		test_purple_account_option_copy_string_with_default
	);

	g_test_add_func(
		"/account_option/copy/string_with_masked",
		test_purple_account_option_copy_string_with_masked
	);

	res = g_test_run();

	return res;
}
