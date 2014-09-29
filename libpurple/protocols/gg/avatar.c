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
#include <glibcompat.h>
#include <http.h>

#include "gg.h"
#include "utils.h"
#include "oauth/oauth-purple.h"

/* Common */

static inline ggp_avatar_session_data *
ggp_avatar_get_avdata(PurpleConnection *gc);

static gboolean ggp_avatar_timer_cb(gpointer _gc);

#define GGP_AVATAR_USERAGENT "GG Client build 11.0.0.7562"
#define GGP_AVATAR_SIZE_MAX 1048576

/* Buddy avatars updating */

typedef struct
{
	uin_t uin;
	time_t timestamp;

	PurpleConnection *gc;
	PurpleHttpConnection *request;
} ggp_avatar_buddy_update_req;

static gboolean ggp_avatar_buddy_update_next(PurpleConnection *gc);
static void ggp_avatar_buddy_update_received(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

#define GGP_AVATAR_BUDDY_URL "http://avatars.gg.pl/%u/s,big"

/* Own avatar setting */

typedef struct
{
	PurpleImage *img;
} ggp_avatar_own_data;

static void ggp_avatar_own_got_token(PurpleConnection *gc, const gchar *token,
	gpointer img);
static void ggp_avatar_own_sent(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

#define GGP_AVATAR_RESPONSE_MAX 10240

/*******************************************************************************
 * Common.
 ******************************************************************************/

void ggp_avatar_setup(PurpleConnection *gc)
{
	ggp_avatar_session_data *avdata = ggp_avatar_get_avdata(gc);

	avdata->pending_updates = NULL;
	avdata->current_update = NULL;
	avdata->own_data = g_new0(ggp_avatar_own_data, 1);

	avdata->timer = purple_timeout_add_seconds(1, ggp_avatar_timer_cb, gc);
}

void ggp_avatar_cleanup(PurpleConnection *gc)
{
	ggp_avatar_session_data *avdata = ggp_avatar_get_avdata(gc);

	purple_timeout_remove(avdata->timer);

	if (avdata->current_update != NULL) {
		ggp_avatar_buddy_update_req *current_update =
			avdata->current_update;

		purple_http_conn_cancel(current_update->request);
		g_free(current_update);
	}
	avdata->current_update = NULL;

	g_free(avdata->own_data);

	g_list_free_full(avdata->pending_updates, &g_free);
	avdata->pending_updates = NULL;
}

static inline ggp_avatar_session_data *
ggp_avatar_get_avdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return &accdata->avatar_data;
}

static gboolean ggp_avatar_timer_cb(gpointer _gc)
{
	PurpleConnection *gc = _gc;
	ggp_avatar_session_data *avdata;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	avdata = ggp_avatar_get_avdata(gc);
	if (avdata->current_update != NULL) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("gg", "ggp_avatar_timer_cb(%p): "
				"there is already an update running\n", gc);
		}
		return TRUE;
	}

	while (!ggp_avatar_buddy_update_next(gc));

	return TRUE;
}

/*******************************************************************************
 * Buddy avatars updating.
 ******************************************************************************/

void ggp_avatar_buddy_update(PurpleConnection *gc, uin_t uin, time_t timestamp)
{
	ggp_avatar_session_data *avdata = ggp_avatar_get_avdata(gc);
	ggp_avatar_buddy_update_req *pending_update =
		g_new(ggp_avatar_buddy_update_req, 1); /* TODO: leak? */

	if (purple_debug_is_verbose()) {
		purple_debug_misc("gg", "ggp_avatar_buddy_update(%p, %u, %lu)\n", gc,
			uin, timestamp);
	}

	pending_update->uin = uin;
	pending_update->timestamp = timestamp;

	avdata->pending_updates = g_list_append(avdata->pending_updates,
		pending_update);
}

void ggp_avatar_buddy_remove(PurpleConnection *gc, uin_t uin)
{
	if (purple_debug_is_verbose()) {
		purple_debug_misc("gg", "ggp_avatar_buddy_remove(%p, %u)\n", gc, uin);
	}

	purple_buddy_icons_set_for_user(purple_connection_get_account(gc),
		ggp_uin_to_str(uin), NULL, 0, NULL);
}

/* return TRUE if avatar update was performed or there is no new requests,
   FALSE if we can request another one immediately */
static gboolean ggp_avatar_buddy_update_next(PurpleConnection *gc)
{
	PurpleHttpRequest *req;
	ggp_avatar_session_data *avdata = ggp_avatar_get_avdata(gc);
	GList *pending_update_it;
	ggp_avatar_buddy_update_req *pending_update;
	PurpleBuddy *buddy;
	PurpleAccount *account = purple_connection_get_account(gc);
	time_t old_timestamp;
	const char *old_timestamp_str;

	pending_update_it = g_list_first(avdata->pending_updates);
	if (pending_update_it == NULL)
		return TRUE;

	pending_update = pending_update_it->data;
	avdata->pending_updates = g_list_remove(avdata->pending_updates,
		pending_update);
	buddy = purple_blist_find_buddy(account, ggp_uin_to_str(pending_update->uin));

	if (!buddy) {
		if (ggp_str_to_uin(purple_account_get_username(account)) ==
			pending_update->uin)
		{
			purple_debug_misc("gg",
				"ggp_avatar_buddy_update_next(%p): own "
				"avatar update requested, but we don't have "
				"ourselves on buddy list\n", gc);
		} else {
			purple_debug_warning("gg",
				"ggp_avatar_buddy_update_next(%p): "
				"%u update requested, but he's not on buddy "
				"list\n", gc, pending_update->uin);
		}
		return FALSE;
	}

	old_timestamp_str = purple_buddy_icons_get_checksum_for_user(buddy);
	old_timestamp = old_timestamp_str ? g_ascii_strtoull(
		old_timestamp_str, NULL, 10) : 0;
	if (old_timestamp == pending_update->timestamp) {
		if (purple_debug_is_verbose()) {
			purple_debug_misc("gg",
				"ggp_avatar_buddy_update_next(%p): "
				"%u have up to date avatar with ts=%lu\n", gc,
				pending_update->uin, pending_update->timestamp);
		}
		return FALSE;
	}
	if (old_timestamp > pending_update->timestamp) {
		purple_debug_warning("gg",
			"ggp_avatar_buddy_update_next(%p): "
			"saved timestamp for %u is newer than received "
			"(%lu > %lu)\n", gc, pending_update->uin, old_timestamp,
			pending_update->timestamp);
	}

	purple_debug_info("gg",
		"ggp_avatar_buddy_update_next(%p): "
		"updating %u with ts=%lu...\n", gc, pending_update->uin,
		pending_update->timestamp);

	pending_update->gc = gc;
	avdata->current_update = pending_update;

	req = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(req, GGP_AVATAR_BUDDY_URL,
		pending_update->uin);
	purple_http_request_header_set(req, "User-Agent", GGP_AVATAR_USERAGENT);
	purple_http_request_set_max_len(req, GGP_AVATAR_SIZE_MAX);
	pending_update->request = purple_http_request(gc, req,
		ggp_avatar_buddy_update_received, pending_update);
	purple_http_request_unref(req);

	return TRUE;
}

static void ggp_avatar_buddy_update_received(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _pending_update)
{
	ggp_avatar_buddy_update_req *pending_update = _pending_update;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleConnection *gc = pending_update->gc;
	ggp_avatar_session_data *avdata;
	gchar timestamp_str[20];
	const gchar *got_data;
	size_t got_len;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	avdata = ggp_avatar_get_avdata(gc);
	g_assert(pending_update == avdata->current_update);
	avdata->current_update = NULL;

	if (!purple_http_response_is_successful(response)) {
		purple_debug_error("gg", "ggp_avatar_buddy_update_received: bad"
			" response while getting avatar for %u: %s\n",
			pending_update->uin,
			purple_http_response_get_error(response));
		g_free(pending_update);
		return;
	}

	account = purple_connection_get_account(gc);
	buddy = purple_blist_find_buddy(account, ggp_uin_to_str(pending_update->uin));

	if (!buddy) {
		purple_debug_warning("gg", "ggp_avatar_buddy_update_received: "
			"buddy %u disappeared\n", pending_update->uin);
		g_free(pending_update);
		return;
	}

	g_snprintf(timestamp_str, sizeof(timestamp_str), "%lu",
		pending_update->timestamp);
	got_data = purple_http_response_get_data(response, &got_len);
	purple_buddy_icons_set_for_user(account, purple_buddy_get_name(buddy),
		g_memdup(got_data, got_len), got_len, timestamp_str);

	purple_debug_info("gg", "ggp_avatar_buddy_update_received: "
		"got avatar for buddy %u [ts=%lu]\n", pending_update->uin,
		pending_update->timestamp);
	g_free(pending_update);
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

void ggp_avatar_own_set(PurpleConnection *gc, PurpleImage *img)
{
	ggp_avatar_own_data *own_data;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	purple_debug_info("gg", "ggp_avatar_own_set(%p, %p)", gc, img);

	own_data = ggp_avatar_get_avdata(gc)->own_data;

	if (img == NULL) {
		purple_debug_warning("gg", "ggp_avatar_own_set: avatar removing"
			" is probably not possible within old protocol");
		return;
	}

	own_data->img = img;

	ggp_oauth_request(gc, ggp_avatar_own_got_token, img, NULL, NULL);
}

static void
ggp_avatar_own_got_token(PurpleConnection *gc, const gchar *token,
	gpointer _img)
{
	PurpleHttpRequest *req;
	PurpleImage *img = _img;
	ggp_avatar_own_data *own_data = ggp_avatar_get_avdata(gc)->own_data;
	gchar *img_data, *img_data_e, *request_data;
	PurpleAccount *account = purple_connection_get_account(gc);
	uin_t uin = ggp_str_to_uin(purple_account_get_username(account));

	if (img != own_data->img) {
		purple_debug_warning("gg", "ggp_avatar_own_got_token: "
			"avatar was changed in meantime\n");
		return;
	}
	own_data->img = NULL;

	img_data = purple_base64_encode(purple_image_get_data(img),
		purple_image_get_size(img));
	img_data_e = g_uri_escape_string(img_data, NULL, FALSE);
	g_free(img_data);
	request_data = g_strdup_printf("uin=%d&photo=%s", uin, img_data_e);
	g_free(img_data_e);

	purple_debug_misc("gg", "ggp_avatar_own_got_token: "
		"uploading new avatar...\n");

	req = purple_http_request_new("http://avatars.nowe.gg/upload");
	purple_http_request_set_max_len(req, GGP_AVATAR_RESPONSE_MAX);
	purple_http_request_set_method(req, "POST");
	purple_http_request_header_set(req, "Authorization", token);
	purple_http_request_header_set(req, "From", "avatars to avatars");
	purple_http_request_header_set(req, "Content-Type",
		"application/x-www-form-urlencoded");
	purple_http_request_set_contents(req, request_data, -1);
	purple_http_request(gc, req, ggp_avatar_own_sent, NULL);
	purple_http_request_unref(req);

	g_free(request_data);
}

static void ggp_avatar_own_sent(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data)
{
	PurpleConnection *gc =
		purple_http_conn_get_purple_connection(http_conn);

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	if (!purple_http_response_is_successful(response)) {
		purple_debug_error("gg", "ggp_avatar_own_sent: "
			"avatar not sent. %s\n",
			purple_http_response_get_error(response));
		return;
	}
	purple_debug_info("gg", "ggp_avatar_own_sent: %s\n",
		purple_http_response_get_data(response, NULL));
}
