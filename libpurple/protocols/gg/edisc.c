/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Component written by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This file is dual-licensed under the GPL2+ and the X11 (MIT) licences.
 * As a recipient of this file you may choose, which license to receive the
 * code under. As a contributor, you have to ensure the new code is
 * compatible with both.
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
#include "edisc.h"

#include <debug.h>
#include <glibcompat.h>

#include "gg.h"
#include "libgaduw.h"
#include "utils.h"
#include <http.h>

#include <json-glib/json-glib.h>

#define GGP_EDISC_OS "WINNT x86-msvc"
#define GGP_EDISC_TYPE "desktop"
#define GGP_EDISC_API "6"

#define GGP_EDISC_RESPONSE_MAX 10240
#define GGP_EDISC_FNAME_ALLOWED "1234567890" \
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	" [](){}-+=_;'<>,.&$!"

typedef struct _ggp_edisc_xfer ggp_edisc_xfer;

struct _ggp_edisc_session_data
{
	GHashTable *xfers_initialized;
	GHashTable *xfers_history;

	SoupSession *session;
	gchar *security_token;

	SoupMessage *auth_request;
	gboolean auth_done;
	GSList *auth_pending;
};

struct _GGPXfer
{
	PurpleXfer parent;

	gchar *filename;
	gchar *ticket_id;

	gboolean allowed, ready;

	PurpleConnection *gc;
	SoupMessage *msg;
};

typedef enum
{
	GGP_EDISC_XFER_ACK_STATUS_UNKNOWN,
	GGP_EDISC_XFER_ACK_STATUS_ALLOWED,
	GGP_EDISC_XFER_ACK_STATUS_REJECTED
} ggp_edisc_xfer_ack_status;

typedef void (*ggp_ggdrive_auth_cb)(PurpleConnection *gc, gboolean success,
	gpointer user_data);

/*******************************************************************************
 * Setting up.
 ******************************************************************************/

static inline ggp_edisc_session_data *
ggp_edisc_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	accdata = purple_connection_get_protocol_data(gc);
	g_return_val_if_fail(accdata != NULL, NULL);

	return accdata->edisc_data;
}

void
ggp_edisc_setup(PurpleConnection *gc, GProxyResolver *resolver)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_edisc_session_data *sdata = g_new0(ggp_edisc_session_data, 1);

	accdata->edisc_data = sdata;

	sdata->session = soup_session_new_with_options(
	        SOUP_SESSION_PROXY_RESOLVER, resolver,
	        SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_COOKIE_JAR, NULL);
	sdata->xfers_initialized = g_hash_table_new(g_str_hash, g_str_equal);
	sdata->xfers_history = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

void ggp_edisc_cleanup(PurpleConnection *gc)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	g_return_if_fail(sdata != NULL);

	soup_session_abort(sdata->session);
	g_slist_free_full(sdata->auth_pending, g_free);
	g_free(sdata->security_token);

	g_object_unref(sdata->session);
	g_hash_table_destroy(sdata->xfers_initialized);
	g_hash_table_destroy(sdata->xfers_history);

	g_free(sdata);
}

/*******************************************************************************
 * Misc.
 ******************************************************************************/

static void
ggp_edisc_set_defaults(SoupMessage *msg)
{
	// purple_http_request_set_max_len(msg, GGP_EDISC_RESPONSE_MAX);
	soup_message_headers_replace(msg->request_headers, "X-gged-api-version",
	                             GGP_EDISC_API);

	/* optional fields */
	soup_message_headers_replace(
	        msg->request_headers, "User-Agent",
	        "Mozilla/5.0 (Windows NT 6.1; rv:11.0) Gecko/20120613 "
	        "GG/11.0.0.8169 (WINNT_x86-msvc; pl; beta; standard)");
	soup_message_headers_replace(
	        msg->request_headers, "Accept",
	        "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
	soup_message_headers_replace(msg->request_headers, "Accept-Language",
	                             "pl,en-us;q=0.7,en;q=0.3");
	/* soup_message_headers_replace(msg->request_headers, "Accept-Encoding",
	 *                              "gzip, deflate"); */
	soup_message_headers_replace(msg->request_headers, "Accept-Charset",
	                             "ISO-8859-2,utf-8;q=0.7,*;q=0.7");
	soup_message_headers_replace(msg->request_headers, "Connection",
	                             "keep-alive");
	soup_message_headers_replace(
	        msg->request_headers, "Content-Type",
	        "application/x-www-form-urlencoded; charset=UTF-8");
}

static int ggp_edisc_parse_error(const gchar *data)
{
	JsonParser *parser;
	JsonObject *result;
	int error_id;

	parser = ggp_json_parse(data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	error_id = json_object_get_int_member(result, "appStatus");
	purple_debug_info("gg", "edisc error: %s (%d)\n",
		json_object_get_string_member(result, "errorMsg"),
		error_id);
	g_object_unref(parser);

	return error_id;
}

static ggp_edisc_xfer_ack_status
ggp_edisc_xfer_parse_ack_status(const gchar *str)
{
	g_return_val_if_fail(str != NULL, GGP_EDISC_XFER_ACK_STATUS_UNKNOWN);

	if (g_strcmp0("unknown", str) == 0) {
		return GGP_EDISC_XFER_ACK_STATUS_UNKNOWN;
	}
	if (g_strcmp0("allowed", str) == 0) {
		return GGP_EDISC_XFER_ACK_STATUS_ALLOWED;
	}
	if (g_strcmp0("rejected", str) == 0) {
		return GGP_EDISC_XFER_ACK_STATUS_REJECTED;
	}

	purple_debug_warning(
	        "gg", "ggp_edisc_xfer_parse_ack_status: unknown status (%s)", str);
	return GGP_EDISC_XFER_ACK_STATUS_UNKNOWN;
}

/*******************************************************************************
 * General xfer functions.
 ******************************************************************************/

static const gchar *
ggp_edisc_xfer_ticket_url(const gchar *ticket_id)
{
	static gchar ticket_url[150];

	g_snprintf(ticket_url, sizeof(ticket_url),
	           "https://drive.mpa.gg.pl/send_ticket/%s", ticket_id);

	return ticket_url;
}

static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg)
{
	if (purple_xfer_is_cancelled(xfer))
		g_return_if_reached();
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(
		purple_xfer_get_xfer_type(xfer),
		purple_xfer_get_account(xfer),
		purple_xfer_get_remote_user(xfer),
		msg);
	purple_xfer_end(xfer);
}

/*******************************************************************************
 * Authentication.
 ******************************************************************************/

typedef struct _ggp_edisc_auth_data {
	ggp_ggdrive_auth_cb cb;
	gpointer user_data;
} ggp_edisc_auth_data;

static void
ggp_ggdrive_auth_results(PurpleConnection *gc, gboolean success)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	GSList *it;

	purple_debug_info("gg", "ggp_ggdrive_auth_results(gc=%p): %d", gc, success);

	g_return_if_fail(sdata != NULL);

	for (it = sdata->auth_pending; it; it = g_slist_delete_link(it, it)) {
		ggp_edisc_auth_data *auth = it->data;

		auth->cb(gc, success, auth->user_data);
		g_free(auth);
	}
	sdata->auth_pending = NULL;
	sdata->auth_done = TRUE;
}

static void
ggp_ggdrive_auth_done(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                      gpointer user_data)
{
	PurpleConnection *gc = user_data;
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	JsonParser *parser;
	JsonObject *result;
	int status = -1;

	g_return_if_fail(sdata != NULL);

	sdata->auth_request = NULL;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		purple_debug_misc("gg",
		                  "ggp_ggdrive_auth_done: authentication failed due to "
		                  "unsuccessful request (code = %d)",
		                  msg->status_code);
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	parser = ggp_json_parse(msg->response_body->data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status"))
		status = json_object_get_int_member(result, "status");
	g_object_unref(parser);

	if (status != 0) {
		purple_debug_misc("gg",
		                  "ggp_ggdrive_auth_done: authentication failed due to "
		                  "bad result (status=%d)",
		                  status);
		if (purple_debug_is_verbose()) {
			purple_debug_misc("gg", "ggp_ggdrive_auth_done: result = %s",
			                  msg->response_body->data);
		}
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	sdata->security_token = g_strdup(soup_message_headers_get_one(
	        msg->response_headers, "X-gged-security-token"));
	if (!sdata->security_token) {
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: authentication failed "
		                        "due to missing security token header");
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	if (purple_debug_is_unsafe()) {
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: security_token=%s",
		                  sdata->security_token);
	}
	ggp_ggdrive_auth_results(gc, TRUE);
}

static void
ggp_ggdrive_auth(PurpleConnection *gc, ggp_ggdrive_auth_cb cb,
                 gpointer user_data)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	ggp_edisc_auth_data *auth;
	const gchar *imtoken;
	gchar *metadata;
	gchar *tmp;
	SoupMessage *msg;

	g_return_if_fail(sdata != NULL);

	imtoken = ggp_get_imtoken(gc);
	if (!imtoken) {
		cb(gc, FALSE, user_data);
		return;
	}

	if (sdata->auth_done) {
		cb(gc, sdata->security_token != NULL, user_data);
		return;
	}

	auth = g_new0(ggp_edisc_auth_data, 1);
	auth->cb = cb;
	auth->user_data = user_data;
	sdata->auth_pending = g_slist_prepend(sdata->auth_pending, auth);

	if (sdata->auth_request) {
		return;
	}

	purple_debug_info("gg", "ggp_ggdrive_auth(gc=%p)", gc);

	msg = soup_message_new("PUT", "https://drive.mpa.gg.pl/signin");
	ggp_edisc_set_defaults(msg);

	metadata =
	        g_strdup_printf("{"
	                        "\"id\": \"%032x\", "
	                        "\"name\": \"%s\", "
	                        "\"os_version\": \"" GGP_EDISC_OS "\", "
	                        "\"client_version\": \"%s\", "
	                        "\"type\": \"" GGP_EDISC_TYPE "\"}",
	                        g_random_int_range(1, 1 << 16),
	                        purple_get_host_name(), ggp_libgaduw_version(gc));

	tmp = g_strdup_printf("IMToken %s", imtoken);
	soup_message_headers_replace(msg->request_headers, "Authorization", tmp);
	g_free(tmp);
	tmp = g_strdup_printf("gg/pl:%u", accdata->session->uin);
	soup_message_headers_replace(msg->request_headers, "X-gged-user", tmp);
	g_free(tmp);
	soup_message_headers_replace(msg->request_headers, "X-gged-client-metadata",
	                             metadata);
	g_free(metadata);

	soup_session_queue_message(sdata->session, msg, ggp_ggdrive_auth_done, gc);
	sdata->auth_request = msg;
}

static void
ggp_edisc_xfer_send_ticket_changed(PurpleConnection *gc, PurpleXfer *xfer,
                                   gboolean is_allowed)
{
	GGPXfer *edisc_xfer = GGP_XFER(xfer);
	if (!edisc_xfer) {
		purple_debug_fatal(
		        "gg",
		        "ggp_edisc_event_ticket_changed: transfer %p already free'd",
		        xfer);
		return;
	}

	if (!is_allowed) {
		purple_debug_info(
		        "gg", "ggp_edisc_event_ticket_changed: transfer %p rejected",
		        xfer);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if (edisc_xfer->allowed) {
		purple_debug_misc(
		        "gg",
		        "ggp_edisc_event_ticket_changed: transfer %p already allowed",
		        xfer);
		return;
	}
	edisc_xfer->allowed = TRUE;

	purple_xfer_start(xfer, -1, NULL, 0);
}

/*******************************************************************************
 * Sending a file.
 ******************************************************************************/

gboolean
ggp_edisc_xfer_can_receive_file(PurpleProtocolXfer *prplxfer,
                                PurpleConnection *gc, const char *who)
{
	PurpleBuddy *buddy;

	g_return_val_if_fail(gc != NULL, FALSE);
	g_return_val_if_fail(who != NULL, FALSE);

	buddy = purple_blist_find_buddy(purple_connection_get_account(gc), who);
	if (buddy == NULL) {
		return FALSE;
	}

	/* TODO: check, if this buddy have us on his list */

	return PURPLE_BUDDY_IS_ONLINE(buddy);
}

static void
ggp_edisc_xfer_send_init_ticket_created(G_GNUC_UNUSED SoupSession *session,
                                        SoupMessage *msg, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	GGPXfer *edisc_xfer = GGP_XFER(xfer);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	ggp_edisc_xfer_ack_status ack_status;
	JsonParser *parser;
	JsonObject *ticket;

	if (purple_xfer_is_cancelled(xfer))
		return;

	g_return_if_fail(sdata != NULL);

	edisc_xfer->msg = NULL;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		int error_id = ggp_edisc_parse_error(msg->response_body->data);
		if (error_id == 206) /* recipient not logged in */
			ggp_edisc_xfer_error(xfer,
				_("Recipient not logged in"));
		else if (error_id == 207) /* bad sender recipient relation */
			ggp_edisc_xfer_error(xfer, _("You aren't on the "
                                "recipient's buddy list"));
		else
			ggp_edisc_xfer_error(xfer,
				_("Unable to send file"));
		return;
	}

	parser = ggp_json_parse(msg->response_body->data);
	ticket = json_node_get_object(json_parser_get_root(parser));
	ticket = json_object_get_object_member(ticket, "result");
	ticket = json_object_get_object_member(ticket, "send_ticket");
	edisc_xfer->ticket_id = g_strdup(json_object_get_string_member(
		ticket, "id"));
	ack_status = ggp_edisc_xfer_parse_ack_status(
		json_object_get_string_member(ticket, "ack_status"));
	/* send_mode: "normal", "publink" (for legacy clients) */

	g_object_unref(parser);

	if (edisc_xfer->ticket_id == NULL) {
		purple_debug_error("gg",
			"ggp_edisc_xfer_send_init_ticket_created: "
			"couldn't get ticket id\n");
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_send_init_ticket_created: "
		"ticket \"%s\" created\n", edisc_xfer->ticket_id);

	g_hash_table_insert(sdata->xfers_initialized,
		edisc_xfer->ticket_id, xfer);
	g_hash_table_insert(sdata->xfers_history,
		g_strdup(edisc_xfer->ticket_id), GINT_TO_POINTER(1));

	if (ack_status != GGP_EDISC_XFER_ACK_STATUS_UNKNOWN)
		ggp_edisc_xfer_send_ticket_changed(edisc_xfer->gc, xfer,
			ack_status == GGP_EDISC_XFER_ACK_STATUS_ALLOWED);
}

static void
ggp_edisc_xfer_send_init_authenticated(PurpleConnection *gc, gboolean success,
                                       gpointer _xfer)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	SoupMessage *msg;
	PurpleXfer *xfer = _xfer;
	GGPXfer *edisc_xfer = GGP_XFER(xfer);
	gchar *data;

	if (purple_xfer_is_cancelled(xfer)) {
		return;
	}

	if (!success) {
		ggp_edisc_xfer_error(xfer, _("Authentication failed"));
		return;
	}

	g_return_if_fail(sdata != NULL);

	msg = soup_message_new("PUT", "https://drive.mpa.gg.pl/send_ticket");
	ggp_edisc_set_defaults(msg);

	soup_message_headers_replace(msg->request_headers, "X-gged-security-token",
	                             sdata->security_token);

	data = g_strdup_printf("{\"send_ticket\":{"
	                       "\"recipient\":\"%s\","
	                       "\"file_name\":\"%s\","
	                       "\"file_size\":\"%u\""
	                       "}}",
	                       purple_xfer_get_remote_user(xfer),
	                       edisc_xfer->filename,
	                       (int)purple_xfer_get_size(xfer));
	soup_message_set_request(msg,
	                         "application/x-www-form-urlencoded; charset=UTF-8",
	                         SOUP_MEMORY_TAKE, data, -1);

	soup_session_queue_message(sdata->session, msg,
	                           ggp_edisc_xfer_send_init_ticket_created, xfer);
	edisc_xfer->msg = msg;
}

static void
ggp_edisc_xfer_send_init(PurpleXfer *xfer)
{
	GGPXfer *edisc_xfer = GGP_XFER(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_NOT_STARTED);

	edisc_xfer->filename = g_strdup(purple_xfer_get_filename(xfer));
	g_strcanon(edisc_xfer->filename, GGP_EDISC_FNAME_ALLOWED, '_');

	ggp_ggdrive_auth(edisc_xfer->gc, ggp_edisc_xfer_send_init_authenticated,
	                 xfer);
}

static void
ggp_edisc_xfer_send_reader(SoupMessage *msg, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	guchar *buffer;
	/* FIXME: The read/write xfer implementation sizes this dynamically. */
	gsize length = 4096;
	gssize stored;

	buffer = g_new(guchar, length);
	stored = purple_xfer_read_file(xfer, buffer, length);

	if (stored < 0) {
		GGPXfer *edisc_xfer = GGP_XFER(xfer);
		ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
		soup_session_cancel_message(sdata->session, msg, SOUP_STATUS_IO_ERROR);
		return;
	}

	soup_message_body_append(msg->request_body, SOUP_MEMORY_TAKE, buffer,
	                         stored);
	if (purple_xfer_get_bytes_sent(xfer) >= purple_xfer_get_size(xfer)) {
		soup_message_body_complete(msg->request_body);
	}
}

static void
ggp_edisc_xfer_send_done(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                         gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	GGPXfer *edisc_xfer = GGP_XFER(xfer);
	JsonParser *parser;
	JsonObject *result;
	int result_status = -1;

	if (purple_xfer_is_cancelled(xfer)) {
		return;
	}

	g_return_if_fail(edisc_xfer != NULL);

	edisc_xfer->msg = NULL;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		ggp_edisc_xfer_error(xfer, _("Error while sending a file"));
		return;
	}

	parser = ggp_json_parse(msg->response_body->data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status")) {
		result_status = json_object_get_int_member(result, "status");
	}
	g_object_unref(parser);

	if (result_status == 0) {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	} else {
		ggp_edisc_xfer_error(xfer, _("Error while sending a file"));
	}
}

static void ggp_edisc_xfer_send_start(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	GGPXfer *edisc_xfer;
	gchar *upload_url, *filename_e;
	SoupMessage *msg;

	g_return_if_fail(xfer != NULL);
	edisc_xfer = GGP_XFER(xfer);
	g_return_if_fail(edisc_xfer != NULL);
	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	g_return_if_fail(sdata != NULL);

	filename_e = purple_strreplace(edisc_xfer->filename, " ", "%20");
	upload_url = g_strdup_printf("https://drive.mpa.gg.pl/me/file/outbox/"
		"%s%%2C%s", edisc_xfer->ticket_id, filename_e);
	g_free(filename_e);
	msg = soup_message_new("PUT", upload_url);
	g_free(upload_url);

	ggp_edisc_set_defaults(msg);

	soup_message_headers_replace(msg->request_headers, "X-gged-local-revision",
	                             "0");
	soup_message_headers_replace(msg->request_headers, "X-gged-security-token",
	                             sdata->security_token);
	soup_message_headers_replace(msg->request_headers, "X-gged-metadata",
	                             "{\"node_type\": \"file\"}");

	soup_message_set_flags(msg, SOUP_MESSAGE_CAN_REBUILD);
	soup_message_body_set_accumulate(msg->request_body, FALSE);
	soup_message_headers_set_content_length(msg->request_headers,
	                                        purple_xfer_get_size(xfer));
	g_signal_connect(msg, "wrote-headers",
	                 G_CALLBACK(ggp_edisc_xfer_send_reader), xfer);
	g_signal_connect(msg, "wrote-chunk", G_CALLBACK(ggp_edisc_xfer_send_reader),
	                 xfer);

	soup_session_queue_message(sdata->session, msg, ggp_edisc_xfer_send_done,
	                           xfer);
	edisc_xfer->msg = msg;
}

PurpleXfer * ggp_edisc_xfer_send_new(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *who)
{
	GGPXfer *xfer;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(who != NULL, NULL);

	xfer = g_object_new(
		GGP_TYPE_XFER,
		"account", purple_connection_get_account(gc),
		"type", PURPLE_XFER_TYPE_SEND,
		"remote-user", who,
		NULL
	);

	xfer->gc = gc;

	return PURPLE_XFER(xfer);
}

void ggp_edisc_xfer_send_file(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *who,
	const char *filename)
{
	PurpleXfer *xfer;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	/* Nothing interesting here, this code is common among protocols.
	 * See ggp_edisc_xfer_send_new. */

	xfer = ggp_edisc_xfer_send_new(prplxfer, gc, who);
	if (filename)
		purple_xfer_request_accepted(xfer, filename);
	else
		purple_xfer_request(xfer);
}

/*******************************************************************************
 * Receiving a file.
 ******************************************************************************/

static PurpleXfer *
ggp_edisc_xfer_recv_new(PurpleConnection *gc, const char *who)
{
	GGPXfer *xfer;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(who != NULL, NULL);

	xfer = g_object_new(GGP_TYPE_XFER, "account",
	                    purple_connection_get_account(gc), "type",
	                    PURPLE_XFER_TYPE_RECEIVE, "remote-user", who, NULL);

	xfer->gc = gc;

	return PURPLE_XFER(xfer);
}

static void
ggp_edisc_xfer_recv_ack_done(G_GNUC_UNUSED SoupSession *session,
                             SoupMessage *msg, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	GGPXfer *edisc_xfer;

	if (purple_xfer_is_cancelled(xfer)) {
		g_return_if_reached();
	}

	edisc_xfer = GGP_XFER(xfer);
	edisc_xfer->msg = NULL;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		ggp_edisc_xfer_error(xfer, _("Cannot confirm file transfer."));
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_recv_ack_done: [%s]\n",
	                  msg->response_body->data);
}

static void ggp_edisc_xfer_recv_ack(PurpleXfer *xfer, gboolean accept)
{
	GGPXfer *edisc_xfer = GGP_XFER(xfer);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	SoupMessage *msg;

	g_return_if_fail(sdata != NULL);

	edisc_xfer->allowed = accept;

	msg = soup_message_new("PUT",
	                       ggp_edisc_xfer_ticket_url(edisc_xfer->ticket_id));
	ggp_edisc_set_defaults(msg);

	soup_message_headers_replace(msg->request_headers, "X-gged-security-token",
	                             sdata->security_token);
	soup_message_headers_replace(msg->request_headers, "X-gged-ack-status",
	                             accept ? "allow" : "reject");

	soup_session_queue_message(sdata->session, msg,
	                           accept ? ggp_edisc_xfer_recv_ack_done : NULL,
	                           xfer);
	edisc_xfer->msg = msg;

	if (!accept) {
		edisc_xfer->msg = NULL;
	}
}

static void
ggp_edisc_xfer_recv_reject(PurpleXfer *xfer)
{
	ggp_edisc_xfer_recv_ack(xfer, FALSE);
}

static void
ggp_edisc_xfer_recv_accept(PurpleXfer *xfer)
{
	ggp_edisc_xfer_recv_ack(xfer, TRUE);
}

static void ggp_edisc_xfer_recv_ticket_completed(PurpleXfer *xfer)
{
	GGPXfer *edisc_xfer = GGP_XFER(xfer);

	if (edisc_xfer->ready)
		return;
	edisc_xfer->ready = TRUE;

	purple_xfer_start(xfer, -1, NULL, 0);
}

static void
ggp_edisc_xfer_recv_writer(SoupMessage *msg, SoupBuffer *chunk, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	GGPXfer *edisc_xfer = GGP_XFER(xfer);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	gboolean stored;

	if (chunk->length > purple_xfer_get_bytes_remaining(xfer)) {
		purple_debug_error(
		        "gg",
		        "ggp_edisc_xfer_recv_writer: saved too much (%" G_GSIZE_FORMAT
		        " > %" G_GOFFSET_FORMAT ")",
		        chunk->length, purple_xfer_get_bytes_remaining(xfer));
		soup_session_cancel_message(sdata->session, msg, SOUP_STATUS_IO_ERROR);
		return;
	}

	stored = purple_xfer_write_file(xfer, (const guchar *)chunk->data,
	                                chunk->length);

	if (!stored) {
		purple_debug_error("gg", "ggp_edisc_xfer_recv_writer: saved too less");
		soup_session_cancel_message(sdata->session, msg, SOUP_STATUS_IO_ERROR);
		return;
	}
}

static void
ggp_edisc_xfer_recv_done(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                         gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	GGPXfer *edisc_xfer = GGP_XFER(xfer);

	if (purple_xfer_is_cancelled(xfer))
		return;

	edisc_xfer->msg = NULL;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		ggp_edisc_xfer_error(xfer, _("Error while receiving a file"));
		return;
	}

	if (purple_xfer_get_bytes_remaining(xfer) == 0) {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	} else {
		purple_debug_warning("gg", "ggp_edisc_xfer_recv_done: didn't "
			"received everything\n");
		ggp_edisc_xfer_error(xfer, _("Error while receiving a file"));
	}
}

static void
ggp_edisc_xfer_recv_start(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	GGPXfer *edisc_xfer;
	gchar *upload_url;
	SoupMessage *msg;

	g_return_if_fail(xfer != NULL);
	edisc_xfer = GGP_XFER(xfer);
	g_return_if_fail(edisc_xfer != NULL);
	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	g_return_if_fail(sdata != NULL);

	upload_url =
	        g_strdup_printf("https://drive.mpa.gg.pl/me/file/inbox/"
	                        "%s,%s?api_version=%s&security_token=%s",
	                        edisc_xfer->ticket_id,
	                        purple_url_encode(purple_xfer_get_filename(xfer)),
	                        GGP_EDISC_API, sdata->security_token);
	msg = soup_message_new("GET", upload_url);
	g_free(upload_url);

	ggp_edisc_set_defaults(msg);
	// purple_http_request_set_max_len(msg, purple_xfer_get_size(xfer) + 1);

	soup_message_body_set_accumulate(msg->response_body, FALSE);
	g_signal_connect(msg, "got-chunk", G_CALLBACK(ggp_edisc_xfer_recv_writer),
	                 xfer);

	soup_session_queue_message(sdata->session, msg, ggp_edisc_xfer_recv_done,
	                           xfer);
	edisc_xfer->msg = msg;
}

static void
ggp_edisc_xfer_recv_ticket_update_got(G_GNUC_UNUSED SoupSession *session,
                                      SoupMessage *msg, gpointer user_data)
{
	PurpleConnection *gc = user_data;
	PurpleXfer *xfer;
	GGPXfer *edisc_xfer;
	JsonParser *parser;
	JsonObject *result;
	int status = -1;
	ggp_edisc_session_data *sdata;

	const gchar *ticket_id, *file_name, *send_mode_str;
	uin_t sender, recipient;
	int file_size;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		purple_debug_error("gg",
		                   "ggp_edisc_xfer_recv_ticket_update_got: cannot "
		                   "fetch update for ticket (code=%d)",
		                   msg->status_code);
		return;
	}

	sdata = ggp_edisc_get_sdata(gc);
	g_return_if_fail(sdata != NULL);

	parser = ggp_json_parse(msg->response_body->data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status"))
		status = json_object_get_int_member(result, "status");
	result = json_object_get_object_member(result, "send_ticket");

	if (status != 0) {
		purple_debug_warning("gg",
		                     "ggp_edisc_xfer_recv_ticket_update_got: failed to "
		                     "get update (status=%d)",
		                     status);
		g_object_unref(parser);
		return;
	}

	ticket_id = json_object_get_string_member(result, "id");
	sender = ggp_str_to_uin(json_object_get_string_member(result, "sender"));
	recipient =
	        ggp_str_to_uin(json_object_get_string_member(result, "recipient"));
	file_size = g_ascii_strtoll(
	        json_object_get_string_member(result, "file_size"), NULL, 10);
	file_name = json_object_get_string_member(result, "file_name");

	/* GG11: normal
	 * AQQ 2.4.2.10: direct_inbox
	 */
	send_mode_str = json_object_get_string_member(result, "send_mode");

	/* more fields:
	 * send_progress (float), ack_status, send_status
	 */

	if (purple_debug_is_verbose() && purple_debug_is_unsafe()) {
		purple_debug_info("gg",
		                  "Got ticket update: id=%s, sender=%u, recipient=%u, "
		                  "file name=\"%s\", file size=%d, send mode=%s)",
		                  ticket_id, sender, recipient, file_name, file_size,
		                  send_mode_str);
	}

	xfer = g_hash_table_lookup(sdata->xfers_initialized, ticket_id);
	if (xfer != NULL) {
		purple_debug_misc("gg",
		                  "ggp_edisc_xfer_recv_ticket_update_got: ticket %s "
		                  "already updated",
		                  purple_debug_is_unsafe() ? ticket_id : "");
		g_object_unref(parser);
		return;
	}

	if (recipient != ggp_get_my_uin(gc)) {
		purple_debug_misc("gg",
		                  "ggp_edisc_xfer_recv_ticket_update_got: ticket %s is "
		                  "not for incoming transfer (its from %u to %u)",
		                  purple_debug_is_unsafe() ? ticket_id : "", sender,
		                  recipient);
		g_object_unref(parser);
		return;
	}

	xfer = ggp_edisc_xfer_recv_new(gc, ggp_uin_to_str(sender));
	purple_xfer_set_filename(xfer, file_name);
	purple_xfer_set_size(xfer, file_size);
	purple_xfer_request(xfer);
	edisc_xfer = GGP_XFER(xfer);
	edisc_xfer->ticket_id = g_strdup(ticket_id);
	g_hash_table_insert(sdata->xfers_initialized, edisc_xfer->ticket_id, xfer);
	g_hash_table_insert(sdata->xfers_history, g_strdup(ticket_id),
	                    GINT_TO_POINTER(1));

	g_object_unref(parser);
}

static void
ggp_edisc_xfer_recv_ticket_update_authenticated(PurpleConnection *gc,
                                                gboolean success,
                                                gpointer _ticket)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	SoupMessage *msg;
	gchar *ticket = _ticket;

	g_return_if_fail(sdata != NULL);

	if (!success) {
		purple_debug_warning(
		        "gg",
		        "ggp_edisc_xfer_recv_ticket_update_authenticated: update of "
		        "ticket %s aborted due to authentication failure",
		        ticket);
		g_free(ticket);
		return;
	}

	msg = soup_message_new("GET", ggp_edisc_xfer_ticket_url(ticket));
	g_free(ticket);

	ggp_edisc_set_defaults(msg);

	soup_message_headers_replace(msg->request_headers, "X-gged-security-token",
	                             sdata->security_token);

	soup_session_queue_message(sdata->session, msg,
	                           ggp_edisc_xfer_recv_ticket_update_got, gc);
}

static void
ggp_edisc_xfer_recv_ticket_got(PurpleConnection *gc, const gchar *ticket_id)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	g_return_if_fail(sdata != NULL);

	if (g_hash_table_lookup(sdata->xfers_history, ticket_id)) {
		return;
	}

	ggp_ggdrive_auth(gc, ggp_edisc_xfer_recv_ticket_update_authenticated,
	                 g_strdup(ticket_id));
}

void
ggp_edisc_xfer_ticket_changed(PurpleConnection *gc, const char *data)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	PurpleXfer *xfer;
	JsonParser *parser;
	JsonObject *ticket;
	const gchar *ticket_id, *send_status;
	ggp_edisc_xfer_ack_status ack_status;
	gboolean is_completed;

	g_return_if_fail(sdata != NULL);

	parser = ggp_json_parse(data);
	ticket = json_node_get_object(json_parser_get_root(parser));
	ticket_id = json_object_get_string_member(ticket, "id");
	ack_status = ggp_edisc_xfer_parse_ack_status(
	        json_object_get_string_member(ticket, "ack_status"));
	send_status = json_object_get_string_member(ticket, "send_status");

	if (ticket_id == NULL) {
		ticket_id = "";
	}
	xfer = g_hash_table_lookup(sdata->xfers_initialized, ticket_id);
	if (xfer == NULL) {
		purple_debug_misc("gg",
		                  "ggp_edisc_event_ticket_changed: ticket %s not "
		                  "found, updating it...",
		                  purple_debug_is_unsafe() ? ticket_id : "");
		ggp_edisc_xfer_recv_ticket_got(gc, ticket_id);
		g_object_unref(parser);
		return;
	}

	is_completed = FALSE;
	if (g_strcmp0("in_progress", send_status) == 0) {
		/* do nothing */
	} else if (g_strcmp0("completed", send_status) == 0) {
		is_completed = TRUE;
	} else if (g_strcmp0("expired", send_status) == 0)
		ggp_edisc_xfer_error(xfer, _("File transfer expired."));
	else {
		purple_debug_warning(
		        "gg", "ggp_edisc_event_ticket_changed: unknown send_status=%s",
		        send_status);
		g_object_unref(parser);
		return;
	}

	g_object_unref(parser);

	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE) {
		if (is_completed) {
			ggp_edisc_xfer_recv_ticket_completed(xfer);
		}
	} else {
		if (ack_status != GGP_EDISC_XFER_ACK_STATUS_UNKNOWN) {
			ggp_edisc_xfer_send_ticket_changed(
			        gc, xfer, ack_status == GGP_EDISC_XFER_ACK_STATUS_ALLOWED);
		}
	}
}

/*******************************************************************************
 * GObject implementation
 ******************************************************************************/

G_DEFINE_DYNAMIC_TYPE(GGPXfer, ggp_xfer, PURPLE_TYPE_XFER);

static void
ggp_xfer_init_xfer(PurpleXfer *xfer) {
	PurpleXferType type = purple_xfer_get_xfer_type(xfer);

	if(type == PURPLE_XFER_TYPE_SEND) {
		ggp_edisc_xfer_send_init(xfer);
	} else if(type == PURPLE_XFER_TYPE_RECEIVE) {
		ggp_edisc_xfer_recv_accept(xfer);
	}
}

static void
ggp_xfer_start(PurpleXfer *xfer) {
	PurpleXferType type = purple_xfer_get_xfer_type(xfer);

	if(type == PURPLE_XFER_TYPE_SEND) {
		ggp_edisc_xfer_send_start(xfer);
	} else if(type == PURPLE_XFER_TYPE_RECEIVE) {
		ggp_edisc_xfer_recv_start(xfer);
	}
}

static void
ggp_xfer_init(GGPXfer *xfer) {

}

static void
ggp_xfer_finalize(GObject *obj) {
	GGPXfer *edisc_xfer = GGP_XFER(obj);
	ggp_edisc_session_data *sdata;

	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);

	g_free(edisc_xfer->filename);
	soup_session_cancel_message(sdata->session, edisc_xfer->msg,
	                            SOUP_STATUS_CANCELLED);

	if (edisc_xfer->ticket_id != NULL) {
		g_hash_table_remove(sdata->xfers_initialized,
			edisc_xfer->ticket_id);
	}

	G_OBJECT_CLASS(ggp_xfer_parent_class)->finalize(obj);
}

static void
ggp_xfer_class_finalize(GGPXferClass *klass) {

}

static void
ggp_xfer_class_init(GGPXferClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleXferClass *xfer_class = PURPLE_XFER_CLASS(klass);

	obj_class->finalize = ggp_xfer_finalize;

	xfer_class->init = ggp_xfer_init_xfer;
	xfer_class->start = ggp_xfer_start;
	xfer_class->request_denied = ggp_edisc_xfer_recv_reject;
}

void
ggp_xfer_register(GTypeModule *module) {
	ggp_xfer_register_type(module);
}
