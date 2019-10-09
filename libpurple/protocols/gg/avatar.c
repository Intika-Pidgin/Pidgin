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

#include "avatar.h"

#include <debug.h>

#include "gg.h"
#include "utils.h"
#include "oauth/oauth-purple.h"

/* Common */

#define GGP_AVATAR_USERAGENT "GG Client build 11.0.0.7562"
#define GGP_AVATAR_SIZE_MAX 1048576

/* Buddy avatars updating */

typedef struct
{
	uin_t uin;
	time_t timestamp;
	PurpleConnection *gc;
} ggp_avatar_buddy_update_req;

#define GGP_AVATAR_BUDDY_URL "http://avatars.gg.pl/%u/s,big"

/* Own avatar setting */

struct _ggp_avatar_session_data {
	PurpleImage *own_img;
};

#define GGP_AVATAR_RESPONSE_MAX 10240

/*******************************************************************************
 * Common.
 ******************************************************************************/

static inline ggp_avatar_session_data *
ggp_avatar_get_avdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->avatar_data;
}

void ggp_avatar_setup(PurpleConnection *gc)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);

	info->avatar_data = g_new0(ggp_avatar_session_data, 1);
}

void ggp_avatar_cleanup(PurpleConnection *gc)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);

	g_free(info->avatar_data);
}

/*******************************************************************************
 * Buddy avatars updating.
 ******************************************************************************/

void ggp_avatar_buddy_remove(PurpleConnection *gc, uin_t uin)
{
	if (purple_debug_is_verbose()) {
		purple_debug_misc("gg", "ggp_avatar_buddy_remove(%p, %u)\n", gc, uin);
	}

	purple_buddy_icons_set_for_user(purple_connection_get_account(gc),
		ggp_uin_to_str(uin), NULL, 0, NULL);
}

static void
ggp_avatar_buddy_update_received(G_GNUC_UNUSED SoupSession *session,
                                 SoupMessage *msg, gpointer _pending_update)
{
	ggp_avatar_buddy_update_req *pending_update = _pending_update;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleConnection *gc = pending_update->gc;
	gchar timestamp_str[20];
	const gchar *got_data;
	size_t got_len;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		purple_debug_error("gg",
		                   "ggp_avatar_buddy_update_received: bad response "
		                   "while getting avatar for %u: %s",
		                   pending_update->uin, msg->reason_phrase);
		g_free(pending_update);
		return;
	}

	account = purple_connection_get_account(gc);
	buddy = purple_blist_find_buddy(account,
	                                ggp_uin_to_str(pending_update->uin));

	if (!buddy) {
		purple_debug_warning(
		        "gg", "ggp_avatar_buddy_update_received: buddy %u disappeared",
		        pending_update->uin);
		g_free(pending_update);
		return;
	}

	g_snprintf(timestamp_str, sizeof(timestamp_str), "%lu",
	           pending_update->timestamp);
	got_data = msg->response_body->data;
	got_len = msg->response_body->length;
	purple_buddy_icons_set_for_user(account, purple_buddy_get_name(buddy),
	                                g_memdup(got_data, got_len), got_len,
	                                timestamp_str);

	purple_debug_info("gg",
	                  "ggp_avatar_buddy_update_received: got avatar for buddy "
	                  "%u [ts=%lu]",
	                  pending_update->uin, pending_update->timestamp);
	g_free(pending_update);
}

void
ggp_avatar_buddy_update(PurpleConnection *gc, uin_t uin, time_t timestamp)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	gchar *url;
	SoupMessage *req;
	ggp_avatar_buddy_update_req *pending_update;
	PurpleBuddy *buddy;
	PurpleAccount *account = purple_connection_get_account(gc);
	time_t old_timestamp;
	const char *old_timestamp_str;

	if (purple_debug_is_verbose()) {
		purple_debug_misc("gg", "ggp_avatar_buddy_update(%p, %u, %lu)", gc, uin,
		                  timestamp);
	}

	buddy = purple_blist_find_buddy(account, ggp_uin_to_str(uin));

	if (!buddy) {
		if (ggp_str_to_uin(purple_account_get_username(account)) == uin) {
			purple_debug_misc(
			        "gg",
			        "ggp_avatar_buddy_update(%p): own avatar update requested, "
			        "but we don't have ourselves on buddy list",
			        gc);
		} else {
			purple_debug_warning("gg",
			                     "ggp_avatar_buddy_update(%p): %u update "
			                     "requested, but he's not on buddy list",
			                     gc, uin);
		}
		return;
	}

	old_timestamp_str = purple_buddy_icons_get_checksum_for_user(buddy);
	old_timestamp = old_timestamp_str ? g_ascii_strtoull(
		old_timestamp_str, NULL, 10) : 0;
	if (old_timestamp == timestamp) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("gg",
			                  "ggp_avatar_buddy_update(%p): %u have up to date "
			                  "avatar with ts=%lu",
			                  gc, uin, timestamp);
		}
		return;
	}
	if (old_timestamp > timestamp) {
		purple_debug_warning("gg",
		                     "ggp_avatar_buddy_update(%p): saved timestamp for "
		                     "%u is newer than received (%lu > %lu)",
		                     gc, uin, old_timestamp, timestamp);
	}

	purple_debug_info("gg",
	                  "ggp_avatar_buddy_update(%p): updating %u with ts=%lu...",
	                  gc, uin, timestamp);

	pending_update = g_new(ggp_avatar_buddy_update_req, 1);
	pending_update->uin = uin;
	pending_update->timestamp = timestamp;
	pending_update->gc = gc;

	url = g_strdup_printf(GGP_AVATAR_BUDDY_URL, pending_update->uin);
	req = soup_message_new("GET", url);
	g_free(url);
	soup_message_headers_replace(req->request_headers, "User-Agent",
	                             GGP_AVATAR_USERAGENT);
	// purple_http_request_set_max_len(req, GGP_AVATAR_SIZE_MAX);
	soup_session_queue_message(
	        info->http, req, ggp_avatar_buddy_update_received, pending_update);
}

/*******************************************************************************
 * Own avatar setting.
 ******************************************************************************/

/**
 * TODO: use new, GG11 method, when IMToken will be provided by libgadu.
 *
 * POST https://avatars.mpa.gg.pl/avatars/user,<uin>/0
 * Authorization: IMToken 0123456789abcdef0123456789abcdef01234567
 * photo=<avatar content>
 */

static void
ggp_avatar_own_sent(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                    gpointer user_data)
{
	PurpleConnection *gc = user_data;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		purple_debug_error("gg", "ggp_avatar_own_sent: avatar not sent. %s\n",
		                   msg->reason_phrase);
		return;
	}
	purple_debug_info("gg", "ggp_avatar_own_sent: %s\n",
	                  msg->response_body->data);
}

static void
ggp_avatar_own_got_token(PurpleConnection *gc, const gchar *token,
	gpointer _img)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	ggp_avatar_session_data *avdata = ggp_avatar_get_avdata(gc);
	SoupMessage *req;
	PurpleImage *img = _img;
	gchar *img_data, *uin_str;
	PurpleAccount *account = purple_connection_get_account(gc);
	uin_t uin = ggp_str_to_uin(purple_account_get_username(account));

	if (img != avdata->own_img) {
		purple_debug_warning("gg", "ggp_avatar_own_got_token: "
			"avatar was changed in meantime\n");
		return;
	}
	avdata->own_img = NULL;

	img_data = g_base64_encode(purple_image_get_data(img),
		purple_image_get_data_size(img));
	uin_str = g_strdup_printf("%d", uin);

	purple_debug_misc("gg", "ggp_avatar_own_got_token: "
		"uploading new avatar...\n");

	req = soup_form_request_new("POST", "http://avatars.nowe.gg/upload", "uin",
	                            uin_str, "photo", img_data, NULL);
	// purple_http_request_set_max_len(req, GGP_AVATAR_RESPONSE_MAX);
	soup_message_headers_replace(req->request_headers, "Authorization", token);
	soup_message_headers_replace(req->request_headers, "From",
	                             "avatars to avatars");
	soup_session_queue_message(info->http, req, ggp_avatar_own_sent, gc);
	g_free(img_data);
	g_free(uin_str);
}

void
ggp_avatar_own_set(PurpleConnection *gc, PurpleImage *img)
{
	ggp_avatar_session_data *avdata;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	purple_debug_info("gg", "ggp_avatar_own_set(%p, %p)", gc, img);

	avdata = ggp_avatar_get_avdata(gc);

	if (img == NULL) {
		purple_debug_warning("gg", "ggp_avatar_own_set: avatar removing is "
		                           "probably not possible within old protocol");
		return;
	}

	avdata->own_img = img;

	ggp_oauth_request(gc, ggp_avatar_own_got_token, img, NULL, NULL);
}
