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

#include "oauth-purple.h"
#include "gg.h"

#include "oauth.h"
#include "../utils.h"
#include "../xml.h"

#include <debug.h>
#include <http.h>

#define GGP_OAUTH_RESPONSE_MAX 10240

typedef struct
{
	PurpleConnection *gc;
	ggp_oauth_request_cb callback;
	gpointer user_data;
	gchar *token;
	gchar *token_secret;

	gchar *sign_method, *sign_url;
} ggp_oauth_data;

static void ggp_oauth_data_free(ggp_oauth_data *data)
{
	g_free(data->token);
	g_free(data->token_secret);
	g_free(data->sign_method);
	g_free(data->sign_url);
	g_free(data);
}

static void
ggp_oauth_access_token_got(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                           gpointer user_data)
{
	ggp_oauth_data *data = user_data;
	gchar *token, *token_secret;
	PurpleXmlNode *xml;
	gboolean succ = TRUE;

	xml = purple_xmlnode_from_str(msg->response_body->data,
	                              msg->response_body->length);
	if (xml == NULL) {
		purple_debug_error("gg", "ggp_oauth_access_token_got: invalid xml");
		ggp_oauth_data_free(data);
		return;
	}

	succ &= ggp_xml_get_string(xml, "oauth_token", &token);
	succ &= ggp_xml_get_string(xml, "oauth_token_secret", &token_secret);
	purple_xmlnode_free(xml);
	if (!succ || strlen(token) < 10) {
		purple_debug_error("gg", "ggp_oauth_access_token_got: invalid xml - "
		                         "token is not present");
		ggp_oauth_data_free(data);
		return;
	}

	if (data->sign_url) {
		PurpleAccount *account;
		gchar *auth;

		purple_debug_misc("gg", "ggp_oauth_access_token_got: got access token, "
		                        "returning signed url");

		account = purple_connection_get_account(data->gc);
		auth = gg_oauth_generate_header(
		        data->sign_method, data->sign_url,
		        purple_account_get_username(account),
		        purple_connection_get_password(data->gc), token, token_secret);
		data->callback(data->gc, auth, data->user_data);
	} else {
		purple_debug_misc(
		        "gg",
		        "ggp_oauth_access_token_got: got access token, returning it");
		data->callback(data->gc, token, data->user_data);
	}

	g_free(token);
	g_free(token_secret);
	ggp_oauth_data_free(data);
}

static void
ggp_oauth_authorization_done(SoupSession *session, SoupMessage *msg,
                             gpointer user_data)
{
	ggp_oauth_data *data = user_data;
	PurpleAccount *account;
	char *auth;
	const char *method = "POST";
	const char *url = "http://api.gadu-gadu.pl/access_token";

	PURPLE_ASSERT_CONNECTION_IS_VALID(data->gc);

	account = purple_connection_get_account(data->gc);

	if (msg->status_code != 302) {
		purple_debug_error("gg",
		                   "ggp_oauth_authorization_done: failed (code = %d)",
		                   msg->status_code);
		ggp_oauth_data_free(data);
		return;
	}

	purple_debug_misc("gg", "ggp_oauth_authorization_done: authorization done, "
	                        "requesting access token...");

	auth = gg_oauth_generate_header(method, url,
	                                purple_account_get_username(account),
	                                purple_connection_get_password(data->gc),
	                                data->token, data->token_secret);

	msg = soup_message_new(method, url);
	// purple_http_request_set_max_len(req, GGP_OAUTH_RESPONSE_MAX);
	soup_message_headers_replace(msg->request_headers, "Authorization", auth);
	soup_session_queue_message(session, msg, ggp_oauth_access_token_got, data);

	g_free(auth);
}

static void
ggp_oauth_request_token_got(SoupSession *session, SoupMessage *msg,
                            gpointer user_data)
{
	ggp_oauth_data *data = user_data;
	PurpleAccount *account;
	PurpleXmlNode *xml;
	gchar *request_data;
	gboolean succ = TRUE;

	PURPLE_ASSERT_CONNECTION_IS_VALID(data->gc);

	account = purple_connection_get_account(data->gc);

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"requested token not received\n");
		ggp_oauth_data_free(data);
		return;
	}

	purple_debug_misc("gg", "ggp_oauth_request_token_got: "
		"got request token, doing authorization...\n");

	xml = purple_xmlnode_from_str(msg->response_body->data,
	                              msg->response_body->length);
	if (xml == NULL) {
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"invalid xml\n");
		ggp_oauth_data_free(data);
		return;
	}

	succ &= ggp_xml_get_string(xml, "oauth_token", &data->token);
	succ &= ggp_xml_get_string(xml, "oauth_token_secret",
		&data->token_secret);
	purple_xmlnode_free(xml);
	if (!succ) {
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"invalid xml - token is not present\n");
		ggp_oauth_data_free(data);
		return;
	}

	request_data = g_strdup_printf(
		"callback_url=http://www.mojageneracja.pl&request_token=%s&"
		"uin=%s&password=%s", data->token,
		purple_account_get_username(account),
		purple_connection_get_password(data->gc));

	msg = soup_message_new("POST", "https://login.gadu-gadu.pl/authorize");
	// purple_http_request_set_max_len(msg, GGP_OAUTH_RESPONSE_MAX);
	/* we don't need any results, nor 302 redirection */
	soup_message_set_flags(msg, SOUP_MESSAGE_NO_REDIRECT);
	soup_message_set_request(msg, "application/x-www-form-urlencoded",
	                         SOUP_MEMORY_TAKE, request_data, -1);
	soup_session_queue_message(session, msg, ggp_oauth_authorization_done,
	                           data);
}

void
ggp_oauth_request(PurpleConnection *gc, ggp_oauth_request_cb callback,
                  gpointer user_data, const gchar *sign_method,
                  const gchar *sign_url)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	SoupMessage *msg;
	char *auth;
	const char *method = "POST";
	const char *url = "http://api.gadu-gadu.pl/request_token";
	ggp_oauth_data *data;

	purple_debug_misc("gg", "ggp_oauth_request: requesting token...\n");

	auth = gg_oauth_generate_header(
	        method, url, purple_account_get_username(account),
	        purple_connection_get_password(gc), NULL, NULL);

	data = g_new0(ggp_oauth_data, 1);
	data->gc = gc;
	data->callback = callback;
	data->user_data = user_data;
	data->sign_method = g_strdup(sign_method);
	data->sign_url = g_strdup(sign_url);

	msg = soup_message_new(method, url);
	// purple_http_request_set_max_len(req, GGP_OAUTH_RESPONSE_MAX);
	soup_message_headers_replace(msg->request_headers, "Authorization", auth);
	soup_session_queue_message(info->http, msg, ggp_oauth_request_token_got,
	                           data);

	g_free(auth);
}
