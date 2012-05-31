/*
 * @file gtkwebview.c GTK+ WebKitWebView wrapper class.
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"
#include "pidgin.h"

#include <gdk/gdkkeysyms.h>
#include "gtkwebview.h"

#define MAX_FONT_SIZE 7
#define MAX_SCROLL_TIME 0.4 /* seconds */
#define SCROLL_DELAY 33 /* milliseconds */

#define GTK_WEBVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), GTK_TYPE_WEBVIEW, GtkWebViewPriv))

enum {
	BUTTONS_UPDATE,
	TOGGLE_FORMAT,
	CLEAR_FORMAT,
	UPDATE_FORMAT,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct _GtkWebViewPriv {
	GHashTable *images; /**< a map from id to temporary file for the image */
	gboolean empty;     /**< whether anything has been appended **/

	/* JS execute queue */
	GQueue *js_queue;
	gboolean is_loading;

	/* Scroll adjustments */
	GtkAdjustment *vadj;
	guint scroll_src;
	GTimer *scroll_time;

	/* Format options */
	GtkWebViewButtons format_functions;
	struct {
		gboolean wbfo:1;	/* Whole buffer formatting only. */
	} edit;

} GtkWebViewPriv;

/******************************************************************************
 * Globals
 *****************************************************************************/

static WebKitWebViewClass *parent_class = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/

static const char *
get_image_src_from_id(GtkWebViewPriv *priv, int id)
{
	char *src;
	PurpleStoredImage *img;

	if (priv->images) {
		/* Check for already loaded image */
		src = (char *)g_hash_table_lookup(priv->images, GINT_TO_POINTER(id));
		if (src)
			return src;
	} else {
		priv->images = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		                                     NULL, g_free);
	}

	/* Find image in store */
	img = purple_imgstore_find_by_id(id);

	src = (char *)purple_imgstore_get_filename(img);
	if (src) {
		src = g_strdup_printf("file://%s", src);
	} else {
		char *tmp;
		tmp = purple_base64_encode(purple_imgstore_get_data(img),
		                           purple_imgstore_get_size(img));
		src = g_strdup_printf("data:base64,%s", tmp);
		g_free(tmp);
	}

	g_hash_table_insert(priv->images, GINT_TO_POINTER(id), src);

	return src;
}

/*
 * Replace all <img id=""> tags with <img src="">. I hoped to never
 * write any HTML parsing code, but I'm forced to do this, until
 * purple changes the way it works.
 */
static char *
replace_img_id_with_src(GtkWebViewPriv *priv, const char *html)
{
	GString *buffer = g_string_new(NULL);
	const char *cur = html;
	char *id;
	int nid;

	while (*cur) {
		const char *img = strstr(cur, "<img");
		if (!img) {
			g_string_append(buffer, cur);
			break;
		} else
			g_string_append_len(buffer, cur, img - cur);

		cur = strstr(img, "/>");
		if (!cur)
			cur = strstr(img, ">");

		if (!cur) { /* invalid html? */
			g_string_printf(buffer, "%s", html);
			break;
		}

		if (strstr(img, "src=") || !strstr(img, "id=")) {
			g_string_printf(buffer, "%s", html);
			break;
		}

		/*
		 * if this is valid HTML, then I can be sure that it
		 * has an id= and does not have an src=, since
		 * '=' cannot appear in parameters.
		 */

		id = strstr(img, "id=") + 3;

		/* *id can't be \0, since a ">" appears after this */
		if (isdigit(*id))
			nid = atoi(id);
		else
			nid = atoi(id + 1);

		/* let's dump this, tag and then dump the src information */
		g_string_append_len(buffer, img, cur - img);

		g_string_append_printf(buffer, " src='%s' ", get_image_src_from_id(priv, nid));
	}

	return g_string_free(buffer, FALSE);
}

static gboolean
process_js_script_queue(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	char *script;

	if (priv->is_loading)
		return FALSE; /* we will be called when loaded */
	if (!priv->js_queue || g_queue_is_empty(priv->js_queue))
		return FALSE; /* nothing to do! */

	script = g_queue_pop_head(priv->js_queue);
	webkit_web_view_execute_script(WEBKIT_WEB_VIEW(webview), script);
	g_free(script);

	return TRUE; /* there may be more for now */
}

static void
webview_load_started(WebKitWebView *webview, WebKitWebFrame *frame,
                     gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	/* is there a better way to test for is_loading? */
	priv->is_loading = TRUE;
}

static void
webview_load_finished(WebKitWebView *webview, WebKitWebFrame *frame,
                      gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	priv->is_loading = FALSE;
	g_idle_add((GSourceFunc)process_js_script_queue, webview);
}

static gboolean
webview_link_clicked(WebKitWebView *webview,
                     WebKitWebFrame *frame,
                     WebKitNetworkRequest *request,
                     WebKitWebNavigationAction *navigation_action,
                     WebKitWebPolicyDecision *policy_decision,
                     gpointer userdata)
{
	const gchar *uri;
	WebKitWebNavigationReason reason;

	uri = webkit_network_request_get_uri(request);
	reason = webkit_web_navigation_action_get_reason(navigation_action);

	if (reason == WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED) {
		/* the gtk imhtml way was to create an idle cb, not sure
		 * why, so right now just using purple_notify_uri directly */
		purple_notify_uri(NULL, uri);
		webkit_web_policy_decision_ignore(policy_decision);
	} else if (reason == WEBKIT_WEB_NAVIGATION_REASON_OTHER)
		webkit_web_policy_decision_use(policy_decision);
	else
		webkit_web_policy_decision_ignore(policy_decision);

	return TRUE;
}

/*
 * Smoothly scroll a WebView.
 *
 * @return TRUE if the window needs to be scrolled further, FALSE if we're at the bottom.
 */
static gboolean
smooth_scroll_cb(gpointer data)
{
	GtkWebViewPriv *priv = data;
	GtkAdjustment *adj;
	gdouble max_val;
	gdouble scroll_val;

	g_return_val_if_fail(priv->scroll_time != NULL, FALSE);

	adj = priv->vadj;
#if GTK_CHECK_VERSION(2,14,0)
	max_val = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
#else
	max_val = adj->upper - adj->page_size;
#endif
	scroll_val = gtk_adjustment_get_value(adj) +
	             ((max_val - gtk_adjustment_get_value(adj)) / 3);

	if (g_timer_elapsed(priv->scroll_time, NULL) > MAX_SCROLL_TIME
	 || scroll_val >= max_val) {
		/* time's up. jump to the end and kill the timer */
		gtk_adjustment_set_value(adj, max_val);
		g_timer_destroy(priv->scroll_time);
		priv->scroll_time = NULL;
		g_source_remove(priv->scroll_src);
		priv->scroll_src = 0;
		return FALSE;
	}

	/* scroll by 1/3rd the remaining distance */
	gtk_adjustment_set_value(adj, scroll_val);
	return TRUE;
}

static gboolean
scroll_idle_cb(gpointer data)
{
	GtkWebViewPriv *priv = data;
	GtkAdjustment *adj = priv->vadj;
	gdouble max_val;

	if (adj) {
#if GTK_CHECK_VERSION(2,14,0)
		max_val = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
#else
		max_val = adj->upper - adj->page_size;
#endif
		gtk_adjustment_set_value(adj, max_val);
	}

	priv->scroll_src = 0;
	return FALSE;
}

static void
webview_clear_formatting(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	if (!webkit_web_view_get_editable(WEBKIT_WEB_VIEW(webview)))
		return;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "removeFormat", FALSE, "");
}

static void
webview_toggle_format(GtkWebView *webview, GtkWebViewButtons buttons)
{
	/* since this function is the handler for the formatting keystrokes,
	   we need to check here that the formatting attempted is permitted */
	buttons &= gtk_webview_get_format_functions(webview);

	switch (buttons) {
	case GTK_WEBVIEW_BOLD:
		gtk_webview_toggle_bold(webview);
		break;
	case GTK_WEBVIEW_ITALIC:
		gtk_webview_toggle_italic(webview);
		break;
	case GTK_WEBVIEW_UNDERLINE:
		gtk_webview_toggle_underline(webview);
		break;
	case GTK_WEBVIEW_STRIKE:
		gtk_webview_toggle_strike(webview);
		break;
	case GTK_WEBVIEW_SHRINK:
		gtk_webview_font_shrink(webview);
		break;
	case GTK_WEBVIEW_GROW:
		gtk_webview_font_grow(webview);
		break;
	default:
		break;
	}
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

GtkWidget *
gtk_webview_new(void)
{
	return GTK_WIDGET(g_object_new(gtk_webview_get_type(), NULL));
}

static void
gtk_webview_finalize(GObject *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	gpointer temp;

	while ((temp = g_queue_pop_head(priv->js_queue)))
		g_free(temp);
	g_queue_free(priv->js_queue);

	if (priv->images)
		g_hash_table_unref(priv->images);

	G_OBJECT_CLASS(parent_class)->finalize(G_OBJECT(webview));
}

static void
gtk_webview_class_init(GtkWebViewClass *klass, gpointer userdata)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	parent_class = g_type_class_ref(webkit_web_view_get_type());
	gobject_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(GtkWebViewPriv));

	signals[BUTTONS_UPDATE] = g_signal_new("allowed-formats-updated",
	                                       G_TYPE_FROM_CLASS(gobject_class),
	                                       G_SIGNAL_RUN_FIRST,
	                                       G_STRUCT_OFFSET(GtkWebViewClass, buttons_update),
	                                       NULL, 0, g_cclosure_marshal_VOID__INT,
	                                       G_TYPE_NONE, 1, G_TYPE_INT);
	signals[TOGGLE_FORMAT] = g_signal_new("format-toggled",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
	                                      G_STRUCT_OFFSET(GtkWebViewClass, toggle_format),
	                                      NULL, 0, g_cclosure_marshal_VOID__INT,
	                                      G_TYPE_NONE, 1, G_TYPE_INT);
	signals[CLEAR_FORMAT] = g_signal_new("format-cleared",
	                                     G_TYPE_FROM_CLASS(gobject_class),
	                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
	                                     G_STRUCT_OFFSET(GtkWebViewClass, clear_format),
	                                     NULL, 0, g_cclosure_marshal_VOID__VOID,
	                                     G_TYPE_NONE, 0);
	signals[UPDATE_FORMAT] = g_signal_new("format-updated",
	                                      G_TYPE_FROM_CLASS(gobject_class),
	                                      G_SIGNAL_RUN_FIRST,
	                                      G_STRUCT_OFFSET(GtkWebViewClass, update_format),
	                                      NULL, 0, g_cclosure_marshal_VOID__VOID,
	                                      G_TYPE_NONE, 0);

	klass->toggle_format = webview_toggle_format;
	klass->clear_format = webview_clear_formatting;

	gobject_class->finalize = gtk_webview_finalize;

	binding_set = gtk_binding_set_by_class(parent_class);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_b, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_BOLD);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_i, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_ITALIC);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_u, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_UNDERLINE);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_plus, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_GROW);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_equal, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_GROW);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_minus, GDK_CONTROL_MASK,
	                             "format-toggled", 1, G_TYPE_INT,
	                             GTK_WEBVIEW_SHRINK);

	binding_set = gtk_binding_set_by_class(klass);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_r, GDK_CONTROL_MASK,
	                             "format-cleared", 0);
}

static void
gtk_webview_init(GtkWebView *webview, gpointer userdata)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);

	priv->empty = TRUE;
	priv->js_queue = g_queue_new();

	g_signal_connect(webview, "navigation-policy-decision-requested",
			  G_CALLBACK(webview_link_clicked), NULL);

	g_signal_connect(webview, "load-started",
			  G_CALLBACK(webview_load_started), NULL);

	g_signal_connect(webview, "load-finished",
			  G_CALLBACK(webview_load_finished), NULL);
}

GType
gtk_webview_get_type(void)
{
	static GType mview_type = 0;
	if (G_UNLIKELY(mview_type == 0)) {
		static const GTypeInfo mview_info = {
			sizeof(GtkWebViewClass),
			NULL,
			NULL,
			(GClassInitFunc)gtk_webview_class_init,
			NULL,
			NULL,
			sizeof(GtkWebView),
			0,
			(GInstanceInitFunc)gtk_webview_init,
			NULL
		};
		mview_type = g_type_register_static(webkit_web_view_get_type(),
				"GtkWebView", &mview_info, 0);
	}
	return mview_type;
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

gboolean
gtk_webview_is_empty(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	return priv->empty;
}

char *
gtk_webview_quote_js_string(const char *text)
{
	GString *str = g_string_new("\"");
	const char *cur = text;

	while (cur && *cur) {
		switch (*cur) {
			case '\\':
				g_string_append(str, "\\\\");
				break;
			case '\"':
				g_string_append(str, "\\\"");
				break;
			case '\r':
				g_string_append(str, "<br/>");
				break;
			case '\n':
				break;
			default:
				g_string_append_c(str, *cur);
		}
		cur++;
	}

	g_string_append_c(str, '"');

	return g_string_free(str, FALSE);
}

void
gtk_webview_safe_execute_script(GtkWebView *webview, const char *script)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	g_queue_push_tail(priv->js_queue, g_strdup(script));
	g_idle_add((GSourceFunc)process_js_script_queue, webview);
}

void
gtk_webview_load_html_string(GtkWebView *webview, const char *html)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	char *html_imged;

	if (priv->images) {
		g_hash_table_unref(priv->images);
		priv->images = NULL;
	}

	html_imged = replace_img_id_with_src(priv, html);
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webview), html_imged, NULL,
	                            NULL, "file:///");
	g_free(html_imged);
}

/* this is a "hack", my plan is to eventually handle this
 * correctly using a signals and a plugin: the plugin will have
 * the information as to what javascript function to call. It seems
 * wrong to hardcode that here.
 */
void
gtk_webview_append_html(GtkWebView *webview, const char *html)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	WebKitDOMDocument *doc;
	WebKitDOMHTMLElement *body;
	doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	body = webkit_dom_document_get_body(doc);
	webkit_dom_html_element_insert_adjacent_html(body, "beforeend", html, NULL);
	priv->empty = FALSE;
}

void
gtk_webview_set_vadjustment(GtkWebView *webview, GtkAdjustment *vadj)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	priv->vadj = vadj;
}

void
gtk_webview_scroll_to_end(GtkWebView *webview, gboolean smooth)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	if (priv->scroll_time)
		g_timer_destroy(priv->scroll_time);
	if (priv->scroll_src)
		g_source_remove(priv->scroll_src);
	if (smooth) {
		priv->scroll_time = g_timer_new();
		priv->scroll_src = g_timeout_add_full(G_PRIORITY_LOW, SCROLL_DELAY, smooth_scroll_cb, priv, NULL);
	} else {
		priv->scroll_time = NULL;
		priv->scroll_src = g_idle_add_full(G_PRIORITY_LOW, scroll_idle_cb, priv, NULL);
	}
}

void
gtk_webview_page_up(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	GtkAdjustment *vadj = priv->vadj;
	gdouble scroll_val;

#if GTK_CHECK_VERSION(2,14,0)
	scroll_val = gtk_adjustment_get_value(vadj) - gtk_adjustment_get_page_size(vadj);
	scroll_val = MAX(scroll_val, gtk_adjustment_get_lower(vadj));
#else
	scroll_val = gtk_adjustment_get_value(vadj) - vadj->page_size;
	scroll_val = MAX(scroll_val, vadj->lower);
#endif

	gtk_adjustment_set_value(vadj, scroll_val);
}

void
gtk_webview_page_down(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	GtkAdjustment *vadj = priv->vadj;
	gdouble scroll_val;
	gdouble page_size;

#if GTK_CHECK_VERSION(2,14,0)
	page_size = gtk_adjustment_get_page_size(vadj);
	scroll_val = gtk_adjustment_get_value(vadj) + page_size;
	scroll_val = MIN(scroll_val, gtk_adjustment_get_upper(vadj) - page_size);
#else
	page_size = vadj->page_size;
	scroll_val = gtk_adjustment_get_value(vadj) + page_size;
	scroll_val = MIN(scroll_val, vadj->upper - page_size);
#endif

	gtk_adjustment_set_value(vadj, scroll_val);
}

void
gtk_webview_set_editable(GtkWebView *webview, gboolean editable)
{
	webkit_web_view_set_editable(WEBKIT_WEB_VIEW(webview), editable);
}

void
gtk_webview_setup_entry(GtkWebView *webview, PurpleConnectionFlags flags)
{
	GtkWebViewButtons buttons;

	if (flags & PURPLE_CONNECTION_HTML) {
		char color[8];
		GdkColor fg_color, bg_color;
		gboolean bold, italic, underline, strike;

		buttons = GTK_WEBVIEW_ALL;

		if (flags & PURPLE_CONNECTION_NO_BGCOLOR)
			buttons &= ~GTK_WEBVIEW_BACKCOLOR;
		if (flags & PURPLE_CONNECTION_NO_FONTSIZE)
		{
			buttons &= ~GTK_WEBVIEW_GROW;
			buttons &= ~GTK_WEBVIEW_SHRINK;
		}
		if (flags & PURPLE_CONNECTION_NO_URLDESC)
			buttons &= ~GTK_WEBVIEW_LINKDESC;

		gtk_webview_get_current_format(webview, &bold, &italic, &underline, &strike);

		gtk_webview_set_format_functions(webview, GTK_WEBVIEW_ALL);
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold") != bold)
			gtk_webview_toggle_bold(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic") != italic)
			gtk_webview_toggle_italic(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline") != underline)
			gtk_webview_toggle_underline(webview);

		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike") != strike)
			gtk_webview_toggle_strike(webview);

		gtk_webview_toggle_fontface(webview,
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/font_face"));

		if (!(flags & PURPLE_CONNECTION_NO_FONTSIZE))
		{
			int size = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/font_size");

			/* 3 is the default. */
			if (size != 3)
				gtk_webview_font_set_size(webview, size);
		}

		if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor"), "") != 0)
		{
			gdk_color_parse(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor"),
							&fg_color);
			g_snprintf(color, sizeof(color),
			           "#%02x%02x%02x",
			           fg_color.red   / 256,
			           fg_color.green / 256,
			           fg_color.blue  / 256);
		} else
			strcpy(color, "");

		gtk_webview_toggle_forecolor(webview, color);

		if (!(flags & PURPLE_CONNECTION_NO_BGCOLOR) &&
		    strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor"), "") != 0)
		{
			gdk_color_parse(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor"),
							&bg_color);
			g_snprintf(color, sizeof(color),
			           "#%02x%02x%02x",
			           bg_color.red   / 256,
			           bg_color.green / 256,
			           bg_color.blue  / 256);
		} else
			strcpy(color, "");

		gtk_webview_toggle_backcolor(webview, color);

		if (flags & PURPLE_CONNECTION_FORMATTING_WBFO)
			gtk_webview_set_whole_buffer_formatting_only(webview, TRUE);
		else
			gtk_webview_set_whole_buffer_formatting_only(webview, FALSE);
	} else {
		buttons = GTK_WEBVIEW_SMILEY | GTK_WEBVIEW_IMAGE;
		webview_clear_formatting(webview);
	}

	if (flags & PURPLE_CONNECTION_NO_IMAGES)
		buttons &= ~GTK_WEBVIEW_IMAGE;

	if (flags & PURPLE_CONNECTION_ALLOW_CUSTOM_SMILEY)
		buttons |= GTK_WEBVIEW_CUSTOM_SMILEY;
	else
		buttons &= ~GTK_WEBVIEW_CUSTOM_SMILEY;

	gtk_webview_set_format_functions(webview, buttons);
}

void
gtk_webview_set_whole_buffer_formatting_only(GtkWebView *webview, gboolean wbfo)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	priv->edit.wbfo = wbfo;
}

void
gtk_webview_set_format_functions(GtkWebView *webview, GtkWebViewButtons buttons)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	GObject *object = g_object_ref(G_OBJECT(webview));
	priv->format_functions = buttons;
	g_signal_emit(object, signals[BUTTONS_UPDATE], 0, buttons);
	g_object_unref(object);
}

GtkWebViewButtons
gtk_webview_get_format_functions(GtkWebView *webview)
{
	GtkWebViewPriv *priv = GTK_WEBVIEW_GET_PRIVATE(webview);
	return priv->format_functions;
}

void
gtk_webview_get_current_format(GtkWebView *webview, gboolean *bold,
                               gboolean *italic, gboolean *underline,
                               gboolean *strike)
{
	WebKitDOMDocument *dom;
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));

	if (bold)
		*bold = webkit_dom_document_query_command_state(dom, "bold");
	if (italic)
		*italic = webkit_dom_document_query_command_state(dom, "italic");
	if (underline)
		*underline = webkit_dom_document_query_command_state(dom, "underline");
	if (strike)
		*strike = webkit_dom_document_query_command_state(dom, "strikethrough");
}

char *
gtk_webview_get_current_fontface(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "fontName");
}

char *
gtk_webview_get_current_forecolor(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "foreColor");
}

char *
gtk_webview_get_current_backcolor(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	return webkit_dom_document_query_command_value(dom, "backColor");
}

gint
gtk_webview_get_current_fontsize(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	gchar *text;
	gint size;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	text = webkit_dom_document_query_command_value(dom, "fontSize");
	size = atoi(text);
	g_free(text);

	return size;
}

gboolean
gtk_webview_get_editable(GtkWebView *webview)
{
	return webkit_web_view_get_editable(WEBKIT_WEB_VIEW(webview));
}

void
gtk_webview_clear_formatting(GtkWebView *webview)
{
	GObject *object;

	object = g_object_ref(G_OBJECT(webview));
	g_signal_emit(object, signals[CLEAR_FORMAT], 0);

	gtk_widget_grab_focus(GTK_WIDGET(webview));

	g_object_unref(object);
}

void
gtk_webview_toggle_bold(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "bold", FALSE, "");
}

void
gtk_webview_toggle_italic(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "italic", FALSE, "");
}

void
gtk_webview_toggle_underline(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "underline", FALSE, "");
}

void
gtk_webview_toggle_strike(GtkWebView *webview)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "strikethrough", FALSE, "");
}

gboolean
gtk_webview_toggle_forecolor(GtkWebView *webview, const char *color)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "foreColor", FALSE, color);

	return FALSE;
}

gboolean
gtk_webview_toggle_backcolor(GtkWebView *webview, const char *color)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "backColor", FALSE, color);

	return FALSE;
}

gboolean
gtk_webview_toggle_fontface(GtkWebView *webview, const char *face)
{
	WebKitDOMDocument *dom;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	webkit_dom_document_exec_command(dom, "fontName", FALSE, face);

	return FALSE;
}

void
gtk_webview_font_set_size(GtkWebView *webview, gint size)
{
	WebKitDOMDocument *dom;
	char *tmp;

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	tmp = g_strdup_printf("%d", size);
	webkit_dom_document_exec_command(dom, "fontSize", FALSE, tmp);
	g_free(tmp);
}

void
gtk_webview_font_shrink(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	gint fontsize;
	char *tmp;

	fontsize = gtk_webview_get_current_fontsize(webview);
	fontsize = MAX(fontsize - 1, 1);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	tmp = g_strdup_printf("%d", fontsize);
	webkit_dom_document_exec_command(dom, "fontSize", FALSE, tmp);
	g_free(tmp);
}

void
gtk_webview_font_grow(GtkWebView *webview)
{
	WebKitDOMDocument *dom;
	gint fontsize;
	char *tmp;

	fontsize = gtk_webview_get_current_fontsize(webview);
	fontsize = MIN(fontsize + 1, MAX_FONT_SIZE);

	dom = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(webview));
	tmp = g_strdup_printf("%d", fontsize);
	webkit_dom_document_exec_command(dom, "fontSize", FALSE, tmp);
	g_free(tmp);
}

