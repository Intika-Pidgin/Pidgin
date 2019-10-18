/**
 * Copyright (C) 2009 Richard Nelson <wabz@whatsbeef.net>
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

#include "internal.h"
#include <glib.h>
#include <libsoup/soup.h>

#define PLUGIN_STATIC_NAME	TinyURL
#define PREFS_BASE          "/plugins/gnt/tinyurl"
#define PREF_LENGTH  PREFS_BASE "/length"
#define PREF_URL  PREFS_BASE "/url"

#include <conversation.h>
#include <signals.h>

#include <glib.h>

#include <plugins.h>
#include <version.h>
#include <debug.h>
#include <notify.h>

#include <gntconv.h>

#include <gntplugin.h>

#include <gntlabel.h>
#include <gnttextview.h>
#include <gntwindow.h>

static int tag_num = 0;
static SoupSession *session = NULL;
static GHashTable *tinyurl_cache = NULL;

typedef struct
{
	gchar *original_url;
	PurpleConversation *conv;
	gchar *tag;
	int num;
} CbInfo;

static void process_urls(PurpleConversation *conv, GList *urls);

/* 3 functions from util.c */
static gboolean
badchar(char c)
{
	switch (c) {
	case ' ':
	case ',':
	case '\0':
	case '\n':
	case '\r':
	case '<':
	case '>':
	case '"':
	case '\'':
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean
badentity(const char *c)
{
	if (!g_ascii_strncasecmp(c, "&lt;", 4) ||
		!g_ascii_strncasecmp(c, "&gt;", 4) ||
		!g_ascii_strncasecmp(c, "&quot;", 6)) {
		return TRUE;
	}
	return FALSE;
}

static GList *extract_urls(const char *text)
{
	const char *t, *c, *q = NULL;
	char *url_buf;
	GList *ret = NULL;
	gboolean inside_html = FALSE;
	int inside_paren = 0;
	c = text;
	while (*c) {
		if (*c == '(' && !inside_html) {
			inside_paren++;
			c++;
		}
		if (inside_html) {
			if (*c == '>') {
				inside_html = FALSE;
			} else if (!q && (*c == '\"' || *c == '\'')) {
				q = c;
			} else if(q) {
				if(*c == *q)
					q = NULL;
			}
		} else if (*c == '<') {
			inside_html = TRUE;
			if (!g_ascii_strncasecmp(c, "<A", 2)) {
				while (1) {
					if (*c == '>') {
						inside_html = FALSE;
						break;
					}
					c++;
					if (!(*c))
						break;
				}
			}
		} else if ((*c=='h') && (!g_ascii_strncasecmp(c, "http://", 7) ||
					(!g_ascii_strncasecmp(c, "https://", 8)))) {
			t = c;
			while (1) {
				if (badchar(*t) || badentity(t)) {

					if ((!g_ascii_strncasecmp(c, "http://", 7) && (t - c == 7)) ||
						(!g_ascii_strncasecmp(c, "https://", 8) && (t - c == 8))) {
						break;
					}

					if (*(t) == ',' && (*(t + 1) != ' ')) {
						t++;
						continue;
					}

					if (*(t - 1) == '.')
						t--;
					if ((*(t - 1) == ')' && (inside_paren > 0))) {
						t--;
					}

					url_buf = g_strndup(c, t - c);
					if (!g_list_find_custom(ret, url_buf, (GCompareFunc)strcmp)) {
						purple_debug_info("TinyURL", "Added URL %s\n", url_buf);
						ret = g_list_append(ret, url_buf);
					} else {
						g_free(url_buf);
					}
					c = t;
					break;
				}
				t++;

			}
		} else if (!g_ascii_strncasecmp(c, "www.", 4) && (c == text || badchar(c[-1]) || badentity(c-1))) {
			if (c[4] != '.') {
				t = c;
				while (1) {
					if (badchar(*t) || badentity(t)) {
						if (t - c == 4) {
							break;
						}

						if (*(t) == ',' && (*(t + 1) != ' ')) {
							t++;
							continue;
						}

						if (*(t - 1) == '.')
							t--;
						if ((*(t - 1) == ')' && (inside_paren > 0))) {
							t--;
						}
						url_buf = g_strndup(c, t - c);
						if (!g_list_find_custom(ret, url_buf, (GCompareFunc)strcmp)) {
							purple_debug_info("TinyURL", "Added URL %s\n", url_buf);
							ret = g_list_append(ret, url_buf);
						} else {
							g_free(url_buf);
						}
						c = t;
						break;
					}
					t++;
				}
			}
		}
		if (*c == ')' && !inside_html) {
			inside_paren--;
			c++;
		}
		if (*c == 0)
			break;
		c++;
	}
	return ret;
}

static void
url_fetched(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
            gpointer _data)
{
	CbInfo *data = (CbInfo *)_data;
	PurpleConversation *conv = data->conv;
	GList *convs = purple_conversations_get_all();
	const gchar *url;

	if (SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		url = msg->response_body->data;
		g_hash_table_insert(tinyurl_cache, data->original_url, g_strdup(url));
	} else {
		url = _("Error while querying TinyURL");
		g_free(data->original_url);
	}

	/* ensure the conversation still exists */
	if (g_list_find(convs, conv)) {
		FinchConv *fconv = FINCH_CONV(conv);
		gchar *str = g_strdup_printf("[%d] %s", data->num, url);
		GntTextView *tv = GNT_TEXT_VIEW(fconv->tv);
		gnt_text_view_tag_change(tv, data->tag, str, FALSE);
		g_free(str);
		g_free(data->tag);
		g_free(data);
		return;
	}
	g_free(data->tag);
	g_free(data);
	purple_debug_info("TinyURL", "Conversation no longer exists... :(\n");
}

static gboolean writing_msg(PurpleConversation *conv, PurpleMessage *msg, gpointer _unused)
{
	GString *t;
	GList *iter, *urls, *next;
	int c = 0;

	if (purple_message_get_flags(msg) & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_INVISIBLE))
		return FALSE;

	urls = g_object_get_data(G_OBJECT(conv), "TinyURLs");
	g_list_free_full(urls, g_free);
	urls = extract_urls(purple_message_get_contents(msg));
	if (!urls)
		return FALSE;

	t = g_string_new(g_strdup(purple_message_get_contents(msg)));
	for (iter = urls; iter; iter = next) {
		next = iter->next;
		if (g_utf8_strlen((char *)iter->data, -1) >= purple_prefs_get_int(PREF_LENGTH)) {
			int pos, x = 0;
			gchar *j, *s, *str, *orig;
			glong len = g_utf8_strlen(iter->data, -1);
			s = g_strdup(t->str);
			orig = s;
			str = g_strdup_printf("[%d]", ++c);
			while ((j = strstr(s, iter->data))) { /* replace all occurrences */
				pos = j - orig + (x++ * 3);
				s = j + len;
				t = g_string_insert(t, pos + len, str);
				if (*s == '\0') break;
			}
			g_free(orig);
			g_free(str);
			continue;
		} else {
			g_free(iter->data);
			urls = g_list_delete_link(urls, iter);
		}
	}
	purple_message_set_contents(msg, t->str);
	g_string_free(t, TRUE);
	if (conv != NULL)
		g_object_set_data(G_OBJECT(conv), "TinyURLs", urls);
	return FALSE;
}

static void wrote_msg(PurpleConversation *conv, PurpleMessage *pmsg,
	gpointer _unused)
{
	GList *urls;

	if (purple_message_get_flags(pmsg) & PURPLE_MESSAGE_SEND)
		return;

	urls = g_object_get_data(G_OBJECT(conv), "TinyURLs");
	if (urls == NULL)
		return;

	process_urls(conv, urls);
	g_object_set_data(G_OBJECT(conv), "TinyURLs", NULL);
}

/* Frees 'urls' */
static void
process_urls(PurpleConversation *conv, GList *urls)
{
	GList *iter;
	int c;
	FinchConv *fconv = FINCH_CONV(conv);
	GntTextView *tv = GNT_TEXT_VIEW(fconv->tv);

	for (iter = urls, c = 1; iter; iter = iter->next, c++) {
		int i;
		SoupMessage *msg;
		CbInfo *cbdata;
		gchar *url;
		gchar *original_url;
		const gchar *tiny_url;

		i = gnt_text_view_get_lines_below(tv);

		original_url = purple_unescape_html((char *)iter->data);
		tiny_url = g_hash_table_lookup(tinyurl_cache, original_url);
		if (tiny_url) {
			gchar *str = g_strdup_printf("\n[%d] %s", c, tiny_url);

			g_free(original_url);
			gnt_text_view_append_text_with_flags(tv, str, GNT_TEXT_FLAG_DIM);
			if (i == 0)
				gnt_text_view_scroll(tv, 0);
			g_free(str);
			continue;
		}
		cbdata = g_new(CbInfo, 1);
		cbdata->num = c;
		cbdata->original_url = original_url;
		cbdata->tag = g_strdup_printf("%s%d", "tiny_", tag_num++);
		cbdata->conv = conv;
		if (g_ascii_strncasecmp(original_url, "http://", 7) && g_ascii_strncasecmp(original_url, "https://", 8)) {
			url = g_strdup_printf("%shttp%%3A%%2F%%2F%s", purple_prefs_get_string(PREF_URL), purple_url_encode(original_url));
		} else {
			url = g_strdup_printf("%s%s", purple_prefs_get_string(PREF_URL), purple_url_encode(original_url));
		}
		msg = soup_message_new("GET", url);
		soup_session_queue_message(session, msg, url_fetched, cbdata);
		gnt_text_view_append_text_with_tag((tv), _("\nFetching TinyURL..."),
		                                   GNT_TEXT_FLAG_DIM, cbdata->tag);
		if (i == 0)
			gnt_text_view_scroll(tv, 0);
		g_free(iter->data);
		g_free(url);
	}
	g_list_free(urls);
}

static void
free_conv_urls(PurpleConversation *conv)
{
	GList *urls = g_object_get_data(G_OBJECT(conv), "TinyURLs");
	g_list_free_full(urls, g_free);
}

static void
tinyurl_notify_tinyuri(GntWidget *win, const gchar *url)
{
	gchar *message;
	GntWidget *label = g_object_get_data(G_OBJECT(win), "info-widget");

	message = g_strdup_printf(_("TinyURL for above: %s"), url);
	gnt_label_set_text(GNT_LABEL(label), message);
	g_free(message);
}

static void
cancel_notify_fetch(GntWidget *win, SoupMessage *msg)
{
	soup_session_cancel_message(session, msg, SOUP_STATUS_CANCELLED);
}

static void
tinyurl_notify_fetch_cb(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
                        gpointer _win)
{
	GntWidget *win = _win;
	const gchar *url;
	const gchar *original_url;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		return;
	}

	original_url = g_object_get_data(G_OBJECT(win), "gnttinyurl-original");
	url = msg->response_body->data;
	g_hash_table_insert(tinyurl_cache,
		g_strdup(original_url), g_strdup(url));

	tinyurl_notify_tinyuri(win, url);

	g_signal_handlers_disconnect_matched(G_OBJECT(win), G_SIGNAL_MATCH_FUNC, 0,
	                                     0, NULL,
	                                     G_CALLBACK(cancel_notify_fetch), NULL);
}

static void *
tinyurl_notify_uri(const char *uri)
{
	char *fullurl = NULL;
	GntWidget *win;
	SoupMessage *msg;
	const gchar *tiny_url;

	/* XXX: The following expects that finch_notify_message gets called. This
	 * may not always happen, e.g. when another plugin sets its own
	 * notify_message. So tread carefully. */
	win = purple_notify_message(NULL, PURPLE_NOTIFY_MSG_INFO, _("URI"), uri,
			_("Please wait while TinyURL fetches a shorter URL ..."), NULL, NULL, NULL);
	if (!GNT_IS_WINDOW(win) || !g_object_get_data(G_OBJECT(win), "info-widget"))
		return win;

	tiny_url = g_hash_table_lookup(tinyurl_cache, uri);
	if (tiny_url) {
		tinyurl_notify_tinyuri(win, tiny_url);
		return win;
	}

	if (g_ascii_strncasecmp(uri, "http://", 7) && g_ascii_strncasecmp(uri, "https://", 8)) {
		fullurl = g_strdup_printf("%shttp%%3A%%2F%%2F%s",
				purple_prefs_get_string(PREF_URL), purple_url_encode(uri));
	} else {
		fullurl = g_strdup_printf("%s%s", purple_prefs_get_string(PREF_URL),
				purple_url_encode(uri));
	}

	g_object_set_data_full(G_OBJECT(win), "gnttinyurl-original", g_strdup(uri), g_free);

	/* Store the SoupMessage and cancel that when the window is destroyed,
	 * so that the callback does not try to use a non-existent window.
	 */
	msg = soup_message_new("GET", fullurl);
	soup_session_queue_message(session, msg, tinyurl_notify_fetch_cb, win);
	g_free(fullurl);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(cancel_notify_fetch),
	                 msg);

	return win;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {

  PurplePluginPrefFrame *frame;
  PurplePluginPref *pref;

  frame = purple_plugin_pref_frame_new();

  pref = purple_plugin_pref_new_with_name(PREF_LENGTH);
  purple_plugin_pref_set_label(pref, _("Only create TinyURL for URLs"
				     " of this length or greater"));
  purple_plugin_pref_frame_add(frame, pref);
  pref = purple_plugin_pref_new_with_name(PREF_URL);
  purple_plugin_pref_set_label(pref, _("TinyURL (or other) address prefix"));
  purple_plugin_pref_frame_add(frame, pref);

  return frame;
}

static FinchPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Richard Nelson <wabz@whatsbeef.net>",
		NULL
	};

	return finch_plugin_info_new(
		"id",             "TinyURL",
		"name",           N_("TinyURL"),
		"version",        DISPLAY_VERSION,
		"category",       N_("Utility"),
		"summary",        N_("TinyURL plugin"),
		"description",    N_("When receiving a message with URL(s), "
		                     "use TinyURL for easier copying"),
		"authors",        authors,
		"website",        PURPLE_WEBSITE,
		"abi-version",    PURPLE_ABI_VERSION,
		"pref-frame-cb",  get_plugin_pref_frame,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	PurpleNotifyUiOps *ops = purple_notify_get_ui_ops();

	session = soup_session_new();

	purple_prefs_add_none(PREFS_BASE);
	purple_prefs_add_int(PREF_LENGTH, 30);
	purple_prefs_add_string(PREF_URL, "http://tinyurl.com/api-create.php?url=");

	g_object_set_data(G_OBJECT(plugin), "notify-uri", ops->notify_uri);
	ops->notify_uri = tinyurl_notify_uri;

	tinyurl_cache = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, g_free);

	purple_signal_connect(purple_conversations_get_handle(),
			"wrote-im-msg",
			plugin, PURPLE_CALLBACK(wrote_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"wrote-chat-msg",
			plugin, PURPLE_CALLBACK(wrote_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"writing-im-msg",
			plugin, PURPLE_CALLBACK(writing_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"writing-chat-msg",
			plugin, PURPLE_CALLBACK(writing_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"deleting-conversation",
			plugin, PURPLE_CALLBACK(free_conv_urls), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	PurpleNotifyUiOps *ops = purple_notify_get_ui_ops();
	if (ops->notify_uri == tinyurl_notify_uri)
		ops->notify_uri = g_object_get_data(G_OBJECT(plugin), "notify-uri");

	soup_session_abort(session);
	g_clear_object(&session);

	g_hash_table_destroy(tinyurl_cache);
	tinyurl_cache = NULL;

	return TRUE;
}

PURPLE_PLUGIN_INIT(PLUGIN_STATIC_NAME, plugin_query, plugin_load, plugin_unload);
