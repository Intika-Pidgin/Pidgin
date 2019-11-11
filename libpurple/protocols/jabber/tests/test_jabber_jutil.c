#include <glib.h>

#include "account.h"
#include "conversation.h"
#include "glibcompat.h"
#include "tests.h"
#include "xmlnode.h"
#include "protocols/jabber/jutil.h"

PurpleTestStringData test_jabber_util_get_resource_exists_data[] = {
        {"foo@bar/baz", "baz"},
        {"bar/baz", "baz"},
        {"foo@bar/baz/bat", "baz/bat"},
        {"bar/baz/bat", "baz/bat"},
        {NULL, NULL},
};
static void
test_jabber_util_get_resource_exists(const PurpleTestStringData *data)
{
	g_assert_cmpstr(data->output, ==, jabber_get_resource(data->input));
}

PurpleTestStringData test_jabber_util_get_resource_none_data[] = {
        {"foo@bar", NULL},
        {"bar", NULL},
        {NULL, NULL},
};
static void
test_jabber_util_get_resource_none(const PurpleTestStringData *data)
{
	g_assert_cmpstr(data->output, ==, jabber_get_resource(data->input));
}

PurpleTestStringData test_jabber_util_get_bare_jid_data[] = {
        {"foo@bar", "foo@bar"},
        {"foo@bar/baz", "foo@bar"},
        {"bar", "bar"},
        {"bar/baz", "bar"},
        {NULL, NULL},
};
static void
test_jabber_util_get_bare_jid(const PurpleTestStringData *data)
{
	g_assert_cmpstr(data->output, ==, jabber_get_bare_jid(data->input));
}

static void
test_jabber_util_nodeprep_validate(void) {
	const gchar *data[] = {
		"foo",
		"%d",
		"y\\z",
		"a=",
		"a,",
		NULL,
	};
	gchar *longnode;
	gint i;

	for(i = 0; data[i]; i++) {
		g_assert_true(jabber_nodeprep_validate(data[i]));
	}

	longnode = g_strnfill(1023, 'a');
	g_assert_true(jabber_nodeprep_validate(longnode));
	g_free(longnode);

	longnode = g_strnfill(1024, 'a');
	g_assert_false(jabber_nodeprep_validate(longnode));
	g_free(longnode);
}

const gchar *test_jabber_util_nodeprep_validate_illegal_chars_data[] = {
        "don't",
        "m@ke",
        "\"me\"",
        "&ngry",
        "c:",
        "a/b",
        "4>2",
        "4<7",
        NULL,
};
static void
test_jabber_util_nodeprep_validate_illegal_chars(const gchar *data)
{
	g_assert_false(jabber_nodeprep_validate(data));
}

static void
test_jabber_util_nodeprep_validate_too_long(void) {
	gchar *longnode = g_strnfill(1024, 'a');

	g_assert_false(jabber_nodeprep_validate(longnode));

	g_free(longnode);
}

const gchar *test_jabber_util_jabber_id_new_valid_data[] = {
        "gmail.com",
        "gmail.com/Test",
        "gmail.com/Test@",
        "gmail.com/@",
        "gmail.com/Test@alkjaweflkj",
        "noone@example.com",
        "noone@example.com/Test12345",
        "noone@example.com/Test@12345",
        "noone@example.com/Te/st@12@//345",
        "わいど@conference.jabber.org",
        "まりるーむ@conference.jabber.org",
        "noone@example.com/まりるーむ",
        "noone@example/stuff.org",
        "noone@nödåtXäYZ.example",
        "noone@nödåtXäYZ.example/まりるーむ",
        "noone@わいど.org",
        "noone@まつ.おおかみ.net",
        "noone@310.0.42.230/s",
        "noone@[::1]", /* IPv6 */
        "noone@[3001:470:1f05:d58::2]",
        "noone@[3001:470:1f05:d58::2]/foo",
        "no=one@310.0.42.230",
        "no,one@310.0.42.230",
        NULL,
};
static void
test_jabber_util_jabber_id_new_valid(const gchar *data)
{
	JabberID *jid = jabber_id_new(data);

	g_assert_nonnull(jid);

	jabber_id_free(jid);
}

const gchar *test_jabber_util_jabber_id_new_invalid_data[] = {
        "@gmail.com",
        "@@gmail.com",
        "noone@@example.com/Test12345",
        "no@one@example.com/Test12345",
        "@example.com/Test@12345",
        "/Test@12345",
        "noone@",
        "noone/",
        "noone@gmail_stuff.org",
        "noone@gmail[stuff.org",
        "noone@gmail\\stuff.org",
        "noone@[::1]124",
        "noone@2[::1]124/as",
        "noone@まつ.おおかみ/\x01",
        /*
         * RFC 3454 Section 6 reads, in part,
         * "If a string contains any RandALCat character, the
         *  string MUST NOT contain any LCat character."
         * The character is U+066D (ARABIC FIVE POINTED STAR).
         */
        "foo@example.com/٭simplexe٭",
        NULL,
};
static void
test_jabber_util_jabber_id_new_invalid(const gchar *jid)
{
	g_assert_null(jabber_id_new(jid));
}

#define assert_jid_parts(expect_node, expect_domain, str) G_STMT_START { \
	JabberID *jid = jabber_id_new(str); \
	g_assert_nonnull(jid); \
	g_assert_nonnull(jid->node); \
	g_assert_nonnull(jid->domain); \
	g_assert_null(jid->resource); \
	g_assert_cmpstr(expect_node, ==, jid->node); \
	g_assert_cmpstr(expect_domain, ==, jid->domain); \
	jabber_id_free(jid); \
} G_STMT_END


static void
test_jabber_util_jid_parts(void) {
	/* Ensure that jabber_id_new is properly lowercasing node and domains */
	assert_jid_parts("noone", "example.com", "NoOne@example.com");
	assert_jid_parts("noone", "example.com", "noone@ExaMPle.CoM");

	/* These case-mapping tests culled from examining RFC3454 B.2 */

	/* Cyrillic capital EF (U+0424) maps to lowercase EF (U+0444) */
	assert_jid_parts("ф", "example.com", "Ф@example.com");

#ifdef USE_IDN
	/*
	 * These character (U+A664 and U+A665) are not mapped to anything in
	 * RFC3454 B.2. This first test *fails* when not using IDN because glib's
	 * case-folding/utf8_strdown improperly (for XMPP) lowercases the character.
	 *
	 * This is known, but not (very?) likely to actually cause a problem, so
	 * this test is commented out when using glib's functions.
	 */
	assert_jid_parts("Ꙥ", "example.com", "Ꙥ@example.com");
	assert_jid_parts("ꙥ", "example.com", "ꙥ@example.com");
#endif

	/* U+04E9 to U+04E9 */
	assert_jid_parts("noone", "өexample.com", "noone@Өexample.com");
}

PurpleTestStringData test_jabber_util_jabber_normalize_data[] = {
        {"NoOnE@ExAMplE.com", "noone@example.com"},
        {"NoOnE@ExampLE.cOM/", "noone@example.com"},
        {"NoONe@exAMPle.CoM/resource", "noone@example.com"},
        {NULL, NULL},
};
static void
test_jabber_util_jabber_normalize(const PurpleTestStringData *data)
{
	g_assert_cmpstr(data->output, ==, jabber_normalize(NULL, data->input));
}

gint
main(gint argc, gchar **argv) {
	gchar *test_name;
	gint i;
	g_test_init(&argc, &argv, NULL);
	g_test_set_nonfatal_assertions();

	for (i = 0; test_jabber_util_get_resource_exists_data[i].input; i++) {
		test_name = g_strdup_printf("/jabber/util/get_resource/exists/%d", i);
		g_test_add_data_func(
		        test_name, &test_jabber_util_get_resource_exists_data[i],
		        (GTestDataFunc)test_jabber_util_get_resource_exists);
		g_free(test_name);
	}
	for (i = 0; test_jabber_util_get_resource_none_data[i].input; i++) {
		test_name = g_strdup_printf("/jabber/util/get_resource/none/%d", i);
		g_test_add_data_func(test_name,
		                     &test_jabber_util_get_resource_none_data[i],
		                     (GTestDataFunc)test_jabber_util_get_resource_none);
		g_free(test_name);
	}

	for (i = 0; test_jabber_util_get_bare_jid_data[i].input; i++) {
		test_name = g_strdup_printf("/jabber/util/get_bare_jid/%d", i);
		g_test_add_data_func(test_name, &test_jabber_util_get_bare_jid_data[i],
		                     (GTestDataFunc)test_jabber_util_get_bare_jid);
		g_free(test_name);
	}

	g_test_add_func("/jabber/util/nodeprep/validate/valid",
	                test_jabber_util_nodeprep_validate);
	for (i = 0; test_jabber_util_nodeprep_validate_illegal_chars_data[i]; i++) {
		test_name = g_strdup_printf(
		        "/jabber/util/nodeprep/validate/illegal_chars/%d", i);
		g_test_add_data_func(
		        test_name,
		        test_jabber_util_nodeprep_validate_illegal_chars_data[i],
		        (GTestDataFunc)
		                test_jabber_util_nodeprep_validate_illegal_chars);
		g_free(test_name);
	}
	g_test_add_func("/jabber/util/nodeprep/validate/too_long",
	                test_jabber_util_nodeprep_validate_too_long);

	for (i = 0; test_jabber_util_jabber_id_new_valid_data[i]; i++) {
		test_name = g_strdup_printf("/jabber/util/id_new/valid/%d", i);
		g_test_add_data_func(
		        test_name, test_jabber_util_jabber_id_new_valid_data[i],
		        (GTestDataFunc)test_jabber_util_jabber_id_new_valid);
		g_free(test_name);
	}
	for (i = 0; test_jabber_util_jabber_id_new_invalid_data[i]; i++) {
		test_name = g_strdup_printf("/jabber/util/id_new/invalid/%d", i);
		g_test_add_data_func(
		        test_name, test_jabber_util_jabber_id_new_invalid_data[i],
		        (GTestDataFunc)test_jabber_util_jabber_id_new_invalid);
		g_free(test_name);
	}
	g_test_add_func("/jabber/util/id_new/jid_parts",
	                test_jabber_util_jid_parts);

	for (i = 0; test_jabber_util_jabber_normalize_data[i].input; i++) {
		test_name = g_strdup_printf("/jabber/util/normalize/%d", i);
		g_test_add_data_func(test_name,
		                     &test_jabber_util_jabber_normalize_data[i],
		                     (GTestDataFunc)test_jabber_util_jabber_normalize);
		g_free(test_name);
	}

	return g_test_run();
}
