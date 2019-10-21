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
#include <gmime/gmime.h>

/* purple includes */
#include "image-store.h"

/* plugin includes */
#include "sametime.h"
#include "im_mime.h"


/** generate "cid:908@20582notesbuddy" from "<908@20582notesbuddy>" */
static char *
make_cid(const char *cid)
{
	g_return_val_if_fail(cid != NULL, NULL);

	return g_strdup_printf("cid:%s", cid);
}


/* Create a MIME parser from some input data. */
static GMimeParser *
create_parser(const char *data)
{
	GMimeStream *stream;
	GMimeFilter *filter;
	GMimeStream *filtered_stream;
	GMimeParser *parser;

	stream = g_mime_stream_mem_new_with_buffer(data, strlen(data));

	filtered_stream = g_mime_stream_filter_new(stream);
	g_object_unref(G_OBJECT(stream));

	/* Add a dos2unix filter so in-message newlines are simplified. */
	filter = g_mime_filter_dos2unix_new(FALSE);
	g_mime_stream_filter_add(GMIME_STREAM_FILTER(filtered_stream), filter);
	g_object_unref(G_OBJECT(filter));

	parser = g_mime_parser_new_with_stream(filtered_stream);
	g_object_unref(G_OBJECT(filtered_stream));

	return parser;
}


/* Replace each IMG tag's SRC attribute with an ID attribute. This
   actually modifies the contents of str */
static void
replace_img_src_by_id(GString *str, GHashTable *img_by_cid)
{
	GData *attribs;
	char *start, *end;
	char *tmp = str->str;

	while (*tmp &&
		purple_markup_find_tag("img", tmp,
			(const char **) &start, (const char **) &end,
			&attribs)) {

		char *alt, *align, *border, *src;
		guint img = 0;

		alt = g_datalist_get_data(&attribs, "alt");
		align = g_datalist_get_data(&attribs, "align");
		border = g_datalist_get_data(&attribs, "border");
		src = g_datalist_get_data(&attribs, "src");

		if (src) {
			img = GPOINTER_TO_UINT(g_hash_table_lookup(img_by_cid, src));
		}

		if (img) {
			GString *atstr;
			gsize len = (end - start);
			gsize mov;

			atstr = g_string_new("");
			if (alt) {
				g_string_append_printf(atstr, " alt=\"%s\"", alt);
			}
			if (align) {
				g_string_append_printf(atstr, " align=\"%s\"", align);
			}
			if (border) {
				g_string_append_printf(atstr, " border=\"%s\"", border);
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

gchar *
im_mime_parse(const char *data)
{
	GHashTable *img_by_cid;

	GString *str;

	GMimeParser *parser;
	GMimeObject *doc;
	int i, count;

	img_by_cid = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* don't want the contained string to ever be NULL */
	str = g_string_new("");

	parser = create_parser(data);
	doc = g_mime_parser_construct_part(parser, NULL);
	if (!GMIME_IS_MULTIPART(doc)) {
		g_object_unref(G_OBJECT(doc));
		g_object_unref(G_OBJECT(parser));
		return g_string_free(str, FALSE);
	}

	/* handle all the MIME parts */
	count = g_mime_multipart_get_count(GMIME_MULTIPART(doc));
	for (i = 0; i < count; i++) {
		GMimeObject *obj = g_mime_multipart_get_part(GMIME_MULTIPART(doc), i);
		GMimeContentType *type = g_mime_object_get_content_type(obj);

		if (!type) {
			; /* feh */

		} else if (g_mime_content_type_is_type(type, "image", "*")) {
			/* put images into the image store */

			GMimePart *part = GMIME_PART(obj);
			GMimeDataWrapper *wrapper;
			GMimeStream *stream;
			GByteArray *bytearray;
			GBytes *data;
			char *cid;
			PurpleImage *image;
			guint imgid;

			/* obtain and unencode the data */
			bytearray = g_byte_array_new();
			stream = g_mime_stream_mem_new_with_byte_array(bytearray);
			g_mime_stream_mem_set_owner(GMIME_STREAM_MEM(stream), FALSE);
			wrapper = g_mime_part_get_content(part);
			g_mime_data_wrapper_write_to_stream(wrapper, stream);
			data = g_byte_array_free_to_bytes(bytearray);
			g_clear_object(&stream);

			/* look up the content id */
			cid = make_cid(g_mime_part_get_content_id(part));

			/* add image to the purple image store */
			image = purple_image_new_from_bytes(data);
			purple_image_set_friendly_filename(image, cid);
			g_bytes_unref(data);

			/* map the cid to the image store identifier */
			imgid = purple_image_store_add(image);
			g_hash_table_insert(img_by_cid, cid, GUINT_TO_POINTER(imgid));

		} else if (GMIME_IS_TEXT_PART(obj)) {
			/* concatenate all the text parts together */
			char *data;

			data = g_mime_text_part_get_text(GMIME_TEXT_PART(obj));
			g_string_append(str, data);
			g_free(data);
		}
	}

	g_object_unref(G_OBJECT(doc));
	g_object_unref(G_OBJECT(parser));

	replace_img_src_by_id(str, img_by_cid);

	/* clean up the cid table */
	g_hash_table_destroy(img_by_cid);

	return g_string_free(str, FALSE);
}


/** generates a random-ish content id string */
static char *
im_mime_content_id(void)
{
	gint id = g_random_int();
	return g_strdup_printf("%03x@%05xmeanwhile", (id & 0xfff00000) >> 20,
	                       id & 0xfffff);
}


/** generates a random-ish boundary value */
static char *
im_mime_boundary(void)
{
	gint id = g_random_int();
	return g_strdup_printf("related_MW%03x_%04x", (id & 0xfff0000) >> 16,
	                       id & 0xffff);
}

/** create MIME image from purple image */
static GMimePart *
im_mime_img_to_part(PurpleImage *img)
{
	const gchar *mimetype;
	GMimePart *part;
	GByteArray *data;
	GMimeStream *stream;
	GMimeDataWrapper *wrapper;

	mimetype = purple_image_get_mimetype(img);
	if (mimetype && g_str_has_prefix(mimetype, "image/")) {
		mimetype += sizeof("image/") - 1;
	}

	part = g_mime_part_new_with_type("image", mimetype);
	g_mime_object_set_disposition(GMIME_OBJECT(part),
			GMIME_DISPOSITION_ATTACHMENT);
	g_mime_part_set_content_encoding(part, GMIME_CONTENT_ENCODING_BASE64);
	g_mime_part_set_filename(part, purple_image_get_friendly_filename(img));

	data = g_bytes_unref_to_array(purple_image_get_contents(img));
	stream = g_mime_stream_mem_new_with_byte_array(data);

	wrapper = g_mime_data_wrapper_new_with_stream(stream,
			GMIME_CONTENT_ENCODING_BINARY);
	g_object_unref(G_OBJECT(stream));

	g_mime_part_set_content(part, wrapper);
	g_object_unref(G_OBJECT(wrapper));

	return part;
}


gchar *
im_mime_generate(const char *message)
{
	GString *str;
	GMimeMultipart *doc;
	GMimeFormatOptions *opts;
	GMimeTextPart *text;

	GData *attr;
	char *tmp, *start, *end;

	str = g_string_new(NULL);

	doc = g_mime_multipart_new_with_subtype("related");

	g_mime_object_set_header(GMIME_OBJECT(doc), "Mime-Version", "1.0", NULL);
	g_mime_object_set_disposition(GMIME_OBJECT(doc), GMIME_DISPOSITION_INLINE);

	tmp = im_mime_boundary();
	g_mime_multipart_set_boundary(doc, tmp);
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
			GMimePart *part;
			char *cid;

			cid = im_mime_content_id();
			part = im_mime_img_to_part(img);
			g_mime_part_set_content_id(part, cid);
			g_mime_multipart_add(doc, GMIME_OBJECT(part));
			g_object_unref(G_OBJECT(part));

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
	text = g_mime_text_part_new_with_subtype("html");
	g_mime_object_set_disposition(GMIME_OBJECT(text),
			GMIME_DISPOSITION_INLINE);

	tmp = purple_utf8_ncr_encode(str->str);
	g_mime_text_part_set_text(text, tmp);
	g_free(tmp);

	g_mime_multipart_insert(doc, 0, GMIME_OBJECT(text));
	g_object_unref(G_OBJECT(text));
	g_string_free(str, TRUE);

	opts = g_mime_format_options_new();
	g_mime_format_options_set_newline_format(opts, GMIME_NEWLINE_FORMAT_DOS);
	tmp = g_mime_object_to_string(GMIME_OBJECT(doc), opts);
	g_mime_format_options_free(opts);
	g_object_unref(G_OBJECT(doc));
	return tmp;
}
