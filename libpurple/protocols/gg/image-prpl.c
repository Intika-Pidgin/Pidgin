/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "image-prpl.h"

#include <debug.h>
#include <glibcompat.h>

#include "gg.h"
#include "utils.h"

#include <image-store.h>

struct _ggp_image_session_data
{
	GHashTable *recv_images;
	GHashTable *sent_images;
};

typedef struct
{
	PurpleImage *image;
	gchar *conv_name; /* TODO: callback */
} ggp_image_sent;

static void ggp_image_sent_free(gpointer _sent_image)
{
	ggp_image_sent *sent_image = _sent_image;
	g_object_unref(sent_image->image);
	g_free(sent_image->conv_name);
	g_free(sent_image);
}

static uint64_t ggp_image_params_to_id(uint32_t crc32, uint32_t size)
{
	return ((uint64_t)crc32 << 32) | size;
}

static inline ggp_image_session_data *
ggp_image_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->image_data;
}

void ggp_image_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_session_data *sdata = g_new0(ggp_image_session_data, 1);

	accdata->image_data = sdata;

	sdata->recv_images = g_hash_table_new_full(
		g_int64_hash, g_int64_equal, g_free, g_object_unref);
	sdata->sent_images = g_hash_table_new_full(
		g_int64_hash, g_int64_equal, g_free,
		ggp_image_sent_free);
}

void ggp_image_cleanup(PurpleConnection *gc)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);

	g_hash_table_destroy(sdata->recv_images);
	g_hash_table_destroy(sdata->sent_images);
	g_free(sdata);
}

ggp_image_prepare_result
ggp_image_prepare(PurpleConversation *conv, PurpleImage *image, uint64_t *id)
{
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	size_t image_size;
	gconstpointer image_data;
	uint32_t image_crc;
	ggp_image_sent *sent_image;

	g_return_val_if_fail(image, GGP_IMAGE_PREPARE_FAILURE);

	image_size = purple_image_get_size(image);

	if (image_size > GGP_IMAGE_SIZE_MAX) {
		purple_debug_warning("gg", "ggp_image_prepare: image "
			"is too big (max bytes: %d)\n", GGP_IMAGE_SIZE_MAX);
		return GGP_IMAGE_PREPARE_TOO_BIG;
	}

	g_object_ref(image);
	image_data = purple_image_get_data(image);
	image_crc = gg_crc32(0, image_data, image_size);

	purple_debug_info("gg", "ggp_image_prepare: image prepared "
		"[crc=%u, size=%" G_GSIZE_FORMAT "]",
		image_crc, image_size);

	*id = ggp_image_params_to_id(image_crc, image_size);

	g_object_ref(image);
	sent_image = g_new(ggp_image_sent, 1);
	sent_image->image = image;
	sent_image->conv_name = g_strdup(purple_conversation_get_name(conv));
	g_hash_table_insert(sdata->sent_images, ggp_uint64dup(*id),
		sent_image);

	return GGP_IMAGE_PREPARE_OK;
}

void ggp_image_recv(PurpleConnection *gc,
	const struct gg_event_image_reply *image_reply)
{
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	PurpleImage *img;
	uint64_t id;

	id = ggp_image_params_to_id(image_reply->crc32, image_reply->size);
	img = g_hash_table_lookup(sdata->recv_images, &id);
	if (!img) {
		purple_debug_warning("gg", "ggp_image_recv: "
			"image " GGP_IMAGE_ID_FORMAT " wasn't requested\n",
			id);
		return;
	}

	purple_debug_info("gg", "ggp_image_recv: got image "
		"[crc=%u, size=%u, filename=%s, id=" GGP_IMAGE_ID_FORMAT "]",
		image_reply->crc32, image_reply->size,
		image_reply->filename, id);

	purple_image_set_friendly_filename(img, image_reply->filename);

	purple_image_transfer_write(img,
		g_memdup(image_reply->image, image_reply->size),
		image_reply->size);
	purple_image_transfer_close(img);
}

void ggp_image_send(PurpleConnection *gc,
	const struct gg_event_image_request *image_request)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	ggp_image_sent *sent_image;
	PurpleConversation *conv;
	uint64_t id;
	gchar *gg_filename;

	purple_debug_info("gg", "ggp_image_send: got image request "
		"[uin=%u, crc=%u, size=%u]\n",
		image_request->sender,
		image_request->crc32,
		image_request->size);

	id = ggp_image_params_to_id(image_request->crc32, image_request->size);

	sent_image = g_hash_table_lookup(sdata->sent_images, &id);

	if (sent_image == NULL && image_request->sender == ggp_str_to_uin(
		purple_account_get_username(purple_connection_get_account(gc))))
	{
		purple_debug_misc("gg", "ggp_image_send: requested image "
			"not found, but this may be another session request\n");
		return;
	}
	if (sent_image == NULL) {
		purple_debug_warning("gg", "ggp_image_send: requested image "
			"not found\n");
		return;
	}

	purple_debug_misc("gg", "ggp_image_send: requested image found "
		"[id=" GGP_IMAGE_ID_FORMAT ", conv=%s]\n",
		id, sent_image->conv_name);

	g_return_if_fail(sent_image->image);

	/* TODO: check allowed recipients */
	gg_filename = g_strdup_printf(GGP_IMAGE_ID_FORMAT, id);
	gg_image_reply(accdata->session, image_request->sender,
		gg_filename,
		purple_image_get_data(sent_image->image),
		purple_image_get_size(sent_image->image));
	g_free(gg_filename);

	conv = purple_conversations_find_with_account(
		sent_image->conv_name,
		purple_connection_get_account(gc));
	if (conv != NULL) {
		gchar *msg = g_strdup_printf(_("Image delivered to %u."),
			image_request->sender);
		purple_conversation_write_system_message(conv, msg,
			PURPLE_MESSAGE_NO_LOG | PURPLE_MESSAGE_NOTIFY);
		g_free(msg);
	}
}

PurpleImage *
ggp_image_request(PurpleConnection *gc, uin_t uin, uint64_t id)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_image_session_data *sdata = ggp_image_get_sdata(gc);
	PurpleImage *img;
	uint32_t crc = id >> 32;
	uint32_t size = id;

	if (size > GGP_IMAGE_SIZE_MAX && crc <= GGP_IMAGE_SIZE_MAX) {
		uint32_t tmp;
		purple_debug_warning("gg", "ggp_image_request: "
			"crc and size are swapped!\n");
		tmp = crc;
		crc = size;
		size = tmp;
		id = ggp_image_params_to_id(crc, size);
	}

	img = g_hash_table_lookup(sdata->recv_images, &id);
	if (img) {
		purple_debug_info("gg", "ggp_image_request: "
			"image " GGP_IMAGE_ID_FORMAT " got from cache", id);
		return img;
	}


	img = purple_image_transfer_new();
	g_hash_table_insert(sdata->recv_images, ggp_uint64dup(id), img);

	purple_debug_info("gg", "ggp_image_request: requesting image "
		GGP_IMAGE_ID_FORMAT, id);
	if (gg_image_request(accdata->session, uin, size, crc) != 0)
		purple_debug_error("gg", "ggp_image_request: failed");

	return img;
}
