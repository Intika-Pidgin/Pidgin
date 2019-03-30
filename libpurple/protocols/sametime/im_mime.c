/*
 * purple - Sametime Protocol Plugin
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

#include "internal.h"

#include <glib.h>

/* purple includes */
#include "image-store.h"
#include "mime.h"

/* plugin includes */
#include "sametime.h"
#include "im_mime.h"


/** generate "cid:908@20582notesbuddy" from "<908@20582notesbuddy>" */
static char *
make_cid(const char *cid)
{
	gsize n;
	char *c, *d;

	g_return_val_if_fail(cid != NULL, NULL);

	n = strlen(cid);
	g_return_val_if_fail(n > 2, NULL);

	c = g_strndup(cid+1, n-2);
	d = g_strdup_printf("cid:%s", c);

	g_free(c);
	return d;
}


gchar *
im_mime_parse(const char *data)
{
	GHashTable *img_by_cid;

	GString *str;

	PurpleMimeDocument *doc;
	GList *parts;

	img_by_cid = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			g_object_unref);

	/* don't want the contained string to ever be NULL */
	str = g_string_new("");

	doc = purple_mime_document_parse(data);

	/* handle all the MIME parts */
	parts = purple_mime_document_get_parts(doc);
	for (; parts; parts = parts->next) {
		PurpleMimePart *part = parts->data;
		const char *type;

		type = purple_mime_part_get_field(part, "content-type");
		purple_debug_info("sametime", "MIME part Content-Type: %s\n",
				type);

		if (!type) {
			; /* feh */

		} else if (purple_str_has_prefix(type, "image")) {
			/* put images into the image store */

			guchar *d_dat;
			gsize d_len;
			char *cid;
			PurpleImage *image;

			/* obtain and unencode the data */
			purple_mime_part_get_data_decoded(part, &d_dat, &d_len);

			/* look up the content id */
			cid = (char *)purple_mime_part_get_field(part,
					"Content-ID");
			cid = make_cid(cid);

			/* add image to the purple image store */
			image = purple_image_new_from_data(d_dat, d_len);
			purple_image_set_friendly_filename(image, cid);

			/* map the cid to the image store identifier */
			g_hash_table_insert(img_by_cid, cid, image);

		} else if (purple_str_has_prefix(type, "text")) {

			/* concatenate all the text parts together */
			guchar *data;
			gsize len;

			purple_mime_part_get_data_decoded(part, &data, &len);
			g_string_append(str, (const char *)data);
			g_free(data);
		}
	}

	purple_mime_document_free(doc);

	/* @todo should put this in its own function */
	{ /* replace each IMG tag's SRC attribute with an ID attribute. This
	     actually modifies the contents of str */
		GData *attribs;
		char *start, *end;
		char *tmp = str->str;

		while (*tmp &&
			purple_markup_find_tag("img", tmp,
				(const char **) &start, (const char **) &end,
				&attribs)) {

			char *alt, *align, *border, *src;
			int img = 0;

			alt = g_datalist_get_data(&attribs, "alt");
			align = g_datalist_get_data(&attribs, "align");
			border = g_datalist_get_data(&attribs, "border");
			src = g_datalist_get_data(&attribs, "src");

			if (src) {
				img = GPOINTER_TO_INT(g_hash_table_lookup(img_by_cid, src));
			}

			if (img) {
				GString *atstr;
				gsize len = (end - start);
				gsize mov;

				atstr = g_string_new("");
				if (alt) {
					g_string_append_printf(atstr,
							" alt=\"%s\"", alt);
				}
				if (align) {
					g_string_append_printf(atstr,
							" align=\"%s\"", align);
				}
				if (border) {
					g_string_append_printf(atstr,
							" border=\"%s\"", border);
				}

				mov = g_snprintf(start, len,
						"<img src=\"" PURPLE_IMAGE_STORE_PROTOCOL
						"%u\"%s", img, atstr->str);
				while (mov < len) start[mov++] = ' ';

				g_string_free(atstr, TRUE);
			}

			g_datalist_clear(&attribs);
			tmp = end + 1;
		}
	}

	/* clean up the cid table */
	g_hash_table_destroy(img_by_cid);

	return g_string_free(str, FALSE);
}


static int
mw_rand(void)
{
	static int seed = 0;

	/* for diversity, not security. don't touch */
	srand(time(NULL) ^ seed);
	seed = rand();

	return seed;
}


/** generates a random-ish content id string */
static char *
im_mime_content_id(void)
{
	return g_strdup_printf("%03x@%05xmeanwhile",
			mw_rand() & 0xfff, mw_rand() & 0xfffff);
}


/** generates a multipart/related content type with a random-ish
    boundary value */
static char *
im_mime_content_type(void)
{
	return g_strdup_printf(
			"multipart/related; boundary=related_MW%03x_%04x",
			mw_rand() & 0xfff, mw_rand() & 0xffff);
}

/** determine content type from contents */
static gchar *
im_mime_img_content_type(PurpleImage *img)
{
	const gchar *mimetype;

	mimetype = purple_image_get_mimetype(img);
	if (!mimetype) {
		mimetype = "image";
	}

	return g_strdup_printf("%s; name=\"%s\"", mimetype,
			purple_image_get_friendly_filename(img));
}


static char *
im_mime_img_content_disp(PurpleImage *img)
{
	return g_strdup_printf("attachment; filename=\"%s\"",
		purple_image_get_friendly_filename(img));
}


gchar *
im_mime_generate(const char *message)
{
	GString *str;
	PurpleMimeDocument *doc;
	PurpleMimePart *part;

	GData *attr;
	char *tmp, *start, *end;

	str = g_string_new(NULL);

	doc = purple_mime_document_new();

	purple_mime_document_set_field(doc, "Mime-Version", "1.0");
	purple_mime_document_set_field(doc, "Content-Disposition", "inline");

	tmp = im_mime_content_type();
	purple_mime_document_set_field(doc, "Content-Type", tmp);
	g_free(tmp);

	tmp = (char *)message;
	while (*tmp &&
		purple_markup_find_tag("img", tmp, (const char **) &start,
			(const char **) &end, &attr)) {
		gchar *uri;
		PurpleImage *img = NULL;

		gsize len = (start - tmp);

		/* append the in-between-tags text */
		if (len) {
			g_string_append_len(str, tmp, len);
		}

		uri = g_datalist_get_data(&attr, "src");
		if (uri) {
			img = purple_image_store_get_from_uri(uri);
		}

		if (img) {
			char *cid;
			gpointer data;
			gsize size;

			part = purple_mime_part_new(doc);

			data = im_mime_img_content_disp(img);
			purple_mime_part_set_field(part, "Content-Disposition",
					data);
			g_free(data);

			data = im_mime_img_content_type(img);
			purple_mime_part_set_field(part, "Content-Type", data);
			g_free(data);

			cid = im_mime_content_id();
			data = g_strdup_printf("<%s>", cid);
			purple_mime_part_set_field(part, "Content-ID", data);
			g_free(data);

			purple_mime_part_set_field(part,
					"Content-transfer-encoding", "base64");

			/* obtain and base64 encode the image data, and put it
			   in the mime part */
			size = purple_image_get_data_size(img);
			data = g_base64_encode(purple_image_get_data(img), size);
			purple_mime_part_set_data(part, data);
			g_free(data);

			/* append the modified tag */
			g_string_append_printf(str, "<img src=\"cid:%s\">", cid);
			g_free(cid);

		} else {
			/* append the literal image tag, since we couldn't find
			   a relative PurpleImage object */
			gsize len = (end - start) + 1;
			g_string_append_len(str, start, len);
		}

		g_datalist_clear(&attr);
		tmp = end + 1;
	}

	/* append left-overs */
	g_string_append(str, tmp);

	/* add the text/html part */
	part = purple_mime_part_new(doc);
	purple_mime_part_set_field(part, "Content-Disposition", "inline");

	tmp = purple_utf8_ncr_encode(str->str);
	purple_mime_part_set_field(part, "Content-Type", "text/html");
	purple_mime_part_set_field(part, "Content-Transfer-Encoding", "7bit");
	purple_mime_part_set_data(part, tmp);
	g_free(tmp);

	g_string_free(str, TRUE);

	str = g_string_new(NULL);
	purple_mime_document_write(doc, str);
	return g_string_free(str, FALSE);
}
