/*
 * purple - Sametime Protocol Plugin Tests
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include <glib.h>
#include <gmime/gmime.h>
#include <string.h>

#include "image.h"
#include "image-store.h"
#include "util.h"

#include "protocols/sametime/im_mime.h"

typedef struct _TestData {
	/* Name of the test case. */
	const gchar *name;
	/* HTML version of the message. */
	const gchar *html;
	/* MIME-encoded version of the message. */
	const gchar *mime;
	/* Whether to load an image and substitute %u for its imagestore id. */
	gboolean with_image;
} TestData;

const TestData TEST_CASES[] = {
#include "data/mime-basic.h"
#include "data/mime-multiline.h"
#include "data/mime-image.h"
#include "data/mime-utf8.h"
};

// Image data to use for tests:
#include "data/image-data.h"


static void
test_sametime_im_mime_parse(gconstpointer _data)
{
	const TestData *data = (const TestData *)_data;
	gchar *result, *ncr;

	/* This decoding is normally done in im_recv_html, but it's not used here. */
	ncr = im_mime_parse(data->mime);
	result = purple_utf8_ncr_decode(ncr);
	g_free(ncr);

	if (data->with_image) {
		gchar **expected;
		gchar **got;
		gchar *suffix_expected, *suffix_got;

		expected = g_strsplit(data->html, "<img src=\"", 2);
		g_assert_nonnull(expected);

		got = g_strsplit(result, "<img src=\"", 2);
		g_assert_nonnull(got);

		/* The image will be in the image store with some random id, so
		 * assert it's starting with the same prefix. */
		g_assert_cmpstr(got[0], ==, expected[0]);

		/* Then assert it's ending with the same suffix. */
		g_assert_nonnull(suffix_expected = expected[1]);
		g_assert_nonnull(suffix_got = got[1]);
		if (suffix_expected && suffix_got) {
			/* First trim off the src attribute which will differ
			 * between expected and the result. */
			g_assert_true(g_str_has_prefix(suffix_expected,
						PURPLE_IMAGE_STORE_PROTOCOL "%u\""));
			suffix_expected += sizeof(PURPLE_IMAGE_STORE_PROTOCOL "%u\"") - 1;
			suffix_got = strchr(suffix_got, '"');
			g_assert_nonnull(suffix_got);
			if (suffix_got) {
				/* Skip over any padding after the image store id. */
				suffix_got = strchr(suffix_got, '>');
				g_assert_nonnull(suffix_got);
			}
			g_assert_cmpstr(suffix_got, ==, suffix_expected);
		}

		g_strfreev(expected);
		g_strfreev(got);
	} else {
		g_assert_cmpstr(result, ==, data->html);
	}
	g_free(result);
}


static void
test_sametime_im_mime_generate(gconstpointer _data)
{
	const TestData *data = (const TestData *)_data;
	gchar *input = NULL;
	PurpleImage *image = NULL;
	guint imgid;
	gchar *result;
	gint i = 0;
	gchar **got = NULL;
	gchar **expected = NULL;

	g_random_set_seed(0);
	if (data->with_image) {
		image = purple_image_new_from_data(test_image_data,
				G_N_ELEMENTS(test_image_data));
		imgid = purple_image_store_add_weak(image);
		input = g_strdup_printf(data->html, imgid);
	} else {
		input = g_strdup(data->html);
	}

	result = im_mime_generate(input);
	got = g_strsplit(result, "\r\n", -1);
	expected = g_strsplit(data->mime, "\r\n", -1);

	/* Check lines are the same individually for more verbose errors. */
	g_assert_cmpuint(g_strv_length(got), ==, g_strv_length(expected));
	for (i = 0; got[i] && expected[i]; i++) {
		g_assert_cmpstr(got[i], ==, expected[i]);
	}

	g_strfreev(expected);
	g_strfreev(got);
	g_free(result);
	if (image) {
		g_object_unref(G_OBJECT(image));
	}
	g_free(input);
}


gint
main(gint argc, gchar **argv)
{
	gint i;
	gchar *name;

	g_test_init(&argc, &argv, NULL);
	g_mime_init();
	_purple_image_store_init();

	g_test_set_nonfatal_assertions();

	for (i = 0; i < G_N_ELEMENTS(TEST_CASES); i++) {
		name = g_strdup_printf("/sametime/im_mime/parse/%s",
				TEST_CASES[i].name);
		g_test_add_data_func(name, &TEST_CASES[i],
				test_sametime_im_mime_parse);
		g_free(name);

		name = g_strdup_printf("/sametime/im_mime/generate/%s",
				TEST_CASES[i].name);
		g_test_add_data_func(name, &TEST_CASES[i],
				test_sametime_im_mime_generate);
		g_free(name);
	}

	i = g_test_run();

	_purple_image_store_uninit();
	g_mime_shutdown();
	return i;
}
