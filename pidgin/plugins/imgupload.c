/*
 * Image Uploader - an inline images implementation for protocols without
 * support for such feature.
 *
 * Copyright (C) 2014, Tomasz Wasilczyk <twasilczyk@pidgin.im>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"

#include "debug.h"
#include "glibcompat.h"
#include "http.h"
#include "version.h"

#include "gtk3compat.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"
#include "gtkwebviewtoolbar.h"

#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#define IMGUP_IMGUR_CLIENT_ID "b6d33c6bb80e1b6"
#define IMGUP_PREF_PREFIX "/plugins/gtk/imgupload/"

static PurplePlugin *plugin_handle = NULL;
static SoupSession *session = NULL;

static void
imgup_upload_done(PidginWebView *webview, const gchar *url, const gchar *title);
static void
imgup_upload_failed(PidginWebView *webview);


/******************************************************************************
 * Helper functions
 ******************************************************************************/

static gboolean
imgup_conn_is_hooked(PurpleConnection *gc)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gc), "imgupload-set"));
}


/******************************************************************************
 * Imgur implementation
 ******************************************************************************/

static void
imgup_imgur_uploaded(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                     gpointer _webview)
{
	JsonParser *parser;
	JsonObject *result;
	PidginWebView *webview = PIDGIN_WEBVIEW(_webview);
	const gchar *url, *title;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		imgup_upload_failed(webview);
		return;
	}

	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, msg->response_body->data,
	                                msg->response_body->length, NULL)) {
		purple_debug_warning("imgupload", "Invalid json got from imgur");

		imgup_upload_failed(webview);
		return;
	}

	result = json_node_get_object(json_parser_get_root(parser));

	if (!json_object_get_boolean_member(result, "success")) {
		g_object_unref(parser);

		purple_debug_warning("imgupload", "imgur - not a success");

		imgup_upload_failed(webview);
		return;
	}

	result = json_object_get_object_member(result, "data");
	url = json_object_get_string_member(result, "link");

	title = g_object_get_data(G_OBJECT(msg), "imgupload-imgur-name");

	imgup_upload_done(webview, url, title);

	g_object_unref(parser);
	g_object_set_data(G_OBJECT(msg), "imgupload-imgur-name", NULL);
}

static PurpleHttpConnection *
imgup_imgur_upload(PidginWebView *webview, PurpleImage *image)
{
	SoupMessage *msg;
	gchar *req_data, *img_data, *img_data_e;

	msg = soup_message_new("POST", "https://api.imgur.com/3/image");
	soup_message_headers_replace(msg, "Authorization",
	                             "Client-ID " IMGUP_IMGUR_CLIENT_ID);

	/* TODO: make it a plain, multipart/form-data request */
	img_data = g_base64_encode(purple_image_get_data(image),
		purple_image_get_data_size(image));
	img_data_e = g_uri_escape_string(img_data, NULL, FALSE);
	g_free(img_data);
	req_data = g_strdup_printf("type=base64&image=%s", img_data_e);
	g_free(img_data_e);

	soup_message_set_request(msg, "application/x-www-form-urlencoded",
	                         SOUP_MESSAGE_TAKE, req_data, strlen(req_data));

	g_object_set_data_full(G_OBJECT(msg), "imgupload-imgur-name",
	                       g_strdup(purple_image_get_friendly_filename(image)),
	                       g_free);

	soup_session_queue_message(session, msg, imgup_imgur_uploaded, webview);

	return msg;
}

/******************************************************************************
 * Image/link upload and insertion
 ******************************************************************************/

static void
imgup_upload_finish(PidginWebView *webview)
{
	gpointer plswait;

	g_object_steal_data(G_OBJECT(webview), "imgupload-msg");
	plswait = g_object_get_data(G_OBJECT(webview), "imgupload-plswait");
	g_object_set_data(G_OBJECT(webview), "imgupload-plswait", NULL);

	if (plswait)
		purple_request_close(PURPLE_REQUEST_WAIT, plswait);
}

static void
imgup_upload_done(PidginWebView *webview, const gchar *url, const gchar *title)
{
	gboolean url_desc;

	imgup_upload_finish(webview);

	if (!purple_prefs_get_bool(IMGUP_PREF_PREFIX "use_url_desc"))
		url_desc = FALSE;
	else {
		PidginWebViewButtons format;

		format = pidgin_webview_get_format_functions(webview);
		url_desc = format & PIDGIN_WEBVIEW_LINKDESC;
	}

	pidgin_webview_insert_link(webview, url, url_desc ? title : NULL);
}

static void
imgup_upload_failed(PidginWebView *webview)
{
	gboolean is_cancelled;

	imgup_upload_finish(webview);

	is_cancelled = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(webview),
		"imgupload-cancelled"));
	g_object_set_data(G_OBJECT(webview), "imgupload-cancelled", NULL);

	if (!is_cancelled)
		purple_debug_error("imgupload", "Failed uploading image");
}

static void
imgup_upload_cancel_message(SoupMessage *msg)
{
	soup_session_cancel_message(session, msg, SOUP_STATUS_CANCELLED);
}

static void
imgup_upload_cancel(gpointer _webview)
{
	SoupMessage *msg;
	PidginWebView *webview = PIDGIN_WEBVIEW(_webview);

	g_object_set_data(G_OBJECT(webview), "imgupload-plswait", NULL);
	g_object_set_data(G_OBJECT(webview), "imgupload-cancelled",
		GINT_TO_POINTER(TRUE));
	msg = g_object_steal_data(G_OBJECT(webview), "imgupload-msg");
	if (msg) {
		soup_session_cancel_message(session, msg, SOUP_STATUS_CANCELLED);
	}
}

static gboolean
imgup_upload_start(PidginWebView *webview, PurpleImage *image, gpointer _gtkconv)
{
	PidginConversation *gtkconv = _gtkconv;
	PurpleConversation *conv = gtkconv->active_conv;
	SoupMessage *msg;
	gpointer plswait;

	if (!imgup_conn_is_hooked(purple_conversation_get_connection(conv)))
		return FALSE;

	msg = imgup_imgur_upload(webview, image);
	g_object_set_data_full(G_OBJECT(webview), "imgupload-msg", msg,
	                       (GDestroyNotify)imgup_upload_cancel_message);

	plswait = purple_request_wait(plugin_handle, _("Uploading image"),
		_("Please wait for image URL being retrieved..."),
		NULL, FALSE, imgup_upload_cancel,
		purple_request_cpar_from_conversation(conv), webview);
	g_object_set_data(G_OBJECT(webview), "imgupload-plswait", plswait);

	return TRUE;
}


/******************************************************************************
 * Setup/cleanup
 ******************************************************************************/

static void
imgup_pidconv_init(PidginConversation *gtkconv)
{
	PidginWebView *webview;

	webview = PIDGIN_WEBVIEW(gtkconv->entry);

	g_signal_connect(G_OBJECT(webview), "insert-image",
		G_CALLBACK(imgup_upload_start), gtkconv);
}

static void
imgup_pidconv_uninit(PidginConversation *gtkconv)
{
	PidginWebView *webview;

	webview = PIDGIN_WEBVIEW(gtkconv->entry);

	g_signal_handlers_disconnect_by_func(G_OBJECT(webview),
		G_CALLBACK(imgup_upload_start), gtkconv);
}

static void
imgup_conv_init(PurpleConversation *conv)
{
	PurpleConnection *gc;

	gc = purple_conversation_get_connection(conv);
	if (!gc)
		return;
	if (!imgup_conn_is_hooked(gc))
		return;

	purple_conversation_set_features(conv,
		purple_conversation_get_features(conv) &
		~PURPLE_CONNECTION_FLAG_NO_IMAGES);

	g_object_set_data(G_OBJECT(conv), "imgupload-set", GINT_TO_POINTER(TRUE));
}

static void
imgup_conv_uninit(PurpleConversation *conv)
{
	PurpleConnection *gc;

	gc = purple_conversation_get_connection(conv);
	if (gc) {
		if (!imgup_conn_is_hooked(gc))
			return;
	} else {
		if (!g_object_get_data(G_OBJECT(conv), "imgupload-set"))
			return;
	}

	purple_conversation_set_features(conv,
		purple_conversation_get_features(conv) |
		PURPLE_CONNECTION_FLAG_NO_IMAGES);

	g_object_set_data(G_OBJECT(conv), "imgupload-set", NULL);
}

static void
imgup_conn_init(PurpleConnection *gc)
{
	PurpleConnectionFlags flags;

	flags = purple_connection_get_flags(gc);

	if (!(flags & PURPLE_CONNECTION_FLAG_NO_IMAGES))
		return;

	flags &= ~PURPLE_CONNECTION_FLAG_NO_IMAGES;
	purple_connection_set_flags(gc, flags);

	g_object_set_data(G_OBJECT(gc), "imgupload-set", GINT_TO_POINTER(TRUE));
}

static void
imgup_conn_uninit(PurpleConnection *gc)
{
	if (!imgup_conn_is_hooked(gc))
		return;

	purple_connection_set_flags(gc, purple_connection_get_flags(gc) |
		PURPLE_CONNECTION_FLAG_NO_IMAGES);

	g_object_set_data(G_OBJECT(gc), "imgupload-set", NULL);
}

/******************************************************************************
 * Prefs
 ******************************************************************************/

static void
imgup_prefs_ok(gpointer _unused, PurpleRequestFields *fields)
{
	gboolean use_url_desc;

	use_url_desc = purple_request_fields_get_bool(fields, "use_url_desc");

	purple_prefs_set_bool(IMGUP_PREF_PREFIX "use_url_desc", use_url_desc);
}

static gpointer
imgup_prefs_get(PurplePlugin *plugin)
{
	PurpleRequestCommonParameters *cpar;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	gpointer handle;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_bool_new("use_url_desc",
		_("Use image filename as link description"),
		purple_prefs_get_bool(IMGUP_PREF_PREFIX "use_url_desc"));
	purple_request_field_group_add_field(group, field);

	cpar = purple_request_cpar_new();
	purple_request_cpar_set_icon(cpar, PURPLE_REQUEST_ICON_DIALOG);

	handle = purple_request_fields(plugin,
		_("Image Uploader"), NULL, NULL, fields,
		_("OK"), (GCallback)imgup_prefs_ok,
		_("Cancel"), NULL,
		cpar, NULL);

	return handle;
}

/******************************************************************************
 * Plugin stuff
 ******************************************************************************/

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Tomasz Wasilczyk <twasilczyk@pidgin.im>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",              "gtk-imgupload",
		"name",            N_("Image Uploader"),
		"version",         DISPLAY_VERSION,
		"category",        N_("Utility"),
		"summary",         N_("Inline images implementation for protocols "
		                      "without such feature."),
		"description",     N_("Adds inline images support for protocols "
		                      "lacking this feature by uploading them to the "
		                      "external service."),
		"authors",         authors,
		"website",         PURPLE_WEBSITE,
		"abi-version",     PURPLE_ABI_VERSION,
		"pref-request-cb", imgup_prefs_get,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	GList *it;

	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/imgupload");

	purple_prefs_add_bool(IMGUP_PREF_PREFIX "use_url_desc", TRUE);

	plugin_handle = plugin;
	session = soup_session_new();

	it = purple_connections_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConnection *gc = it->data;
		imgup_conn_init(gc);
	}

	it = purple_conversations_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;
		imgup_conv_init(conv);
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			imgup_pidconv_init(PIDGIN_CONVERSATION(conv));
	}

	purple_signal_connect(purple_connections_get_handle(),
		"signed-on", plugin,
		PURPLE_CALLBACK(imgup_conn_init), NULL);
	purple_signal_connect(purple_connections_get_handle(),
		"signing-off", plugin,
		PURPLE_CALLBACK(imgup_conn_uninit), NULL);
	purple_signal_connect(pidgin_conversations_get_handle(),
		"conversation-displayed", plugin,
		PURPLE_CALLBACK(imgup_pidconv_init), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	GList *it;

	soup_session_abort(session);

	it = purple_conversations_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;
		imgup_conv_uninit(conv);
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			imgup_pidconv_uninit(PIDGIN_CONVERSATION(conv));
	}

	it = purple_connections_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConnection *gc = it->data;
		imgup_conn_uninit(gc);
	}

	g_clear_object(&session);
	plugin_handle = NULL;

	return TRUE;
}

PURPLE_PLUGIN_INIT(imgupload, plugin_query, plugin_load, plugin_unload);
