/*
 * Conversation Themes for Pidgin
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
 */

#include "gtkconv-theme.h"

#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "xmlnode.h"

#include "pidgin.h"
#include "gtkconv.h"
#include "gtkwebview.h"

#include <stdlib.h>
#include <string.h>

/* GObject data keys - this will probably go away soon... */
#define MESSAGE_STYLE_KEY "message-style"

#define PIDGIN_CONV_THEME_GET_PRIVATE(Gobject) \
	(G_TYPE_INSTANCE_GET_PRIVATE((Gobject), PIDGIN_TYPE_CONV_THEME, PidginConvThemePrivate))

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	/* current config options */
	char     *variant; /* allowed to be NULL if there are no variants */

	/* Info.plist keys that change with Variant */

	/* Static Info.plist keys */
	int      message_view_version;
	char     *cf_bundle_name;
	char     *cf_bundle_identifier;
	char     *cf_bundle_get_info_string;
	char     *default_font_family;
	int      default_font_size;
	gboolean shows_user_icons;
	gboolean disable_combine_consecutive;
	gboolean default_background_is_transparent;
	gboolean disable_custom_background;
	char     *default_background_color;
	gboolean allow_text_colors;
	char     *image_mask;
	char     *default_variant;

	/* paths */
	char    *style_dir;

	/* caches */
	char    *template_html;
	char    *header_html;
	char    *footer_html;
	char    *incoming_content_html;
	char    *outgoing_content_html;
	char    *incoming_next_content_html;
	char    *outgoing_next_content_html;
	char    *status_html;
	char    *basestyle_css;
} PidginConvThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_conv_theme_init(GTypeInstance *instance,
		gpointer klass)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(instance);
}

static void
pidgin_conv_theme_finalize(GObject *obj)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(obj);

	g_free(priv->cf_bundle_name);
	g_free(priv->cf_bundle_identifier);
	g_free(priv->cf_bundle_get_info_string);
	g_free(priv->default_font_family);
	g_free(priv->default_background_color);
	g_free(priv->image_mask);
	g_free(priv->default_variant);

	g_free(priv->style_dir);

	g_free(priv->template_html);
	g_free(priv->incoming_content_html);
	g_free(priv->outgoing_content_html);
	g_free(priv->outgoing_next_content_html);
	g_free(priv->status_html);
	g_free(priv->basestyle_css);

	parent_class->finalize(obj);
}

static void
pidgin_conv_theme_class_init(PidginConvThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = pidgin_conv_theme_finalize;
}

GType
pidgin_conversation_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginConvThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_conv_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(PidginConvTheme),
			0, /* n_preallocs */
			pidgin_conv_theme_init, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME,
				"PidginConvTheme", &info, G_TYPE_FLAG_ABSTRACT);
	}
	return type;
}

/******************************************************************************
 * Helper Functions
 *****************************************************************************/

static PidginConvTheme *
pidgin_conversation_theme_new(const char *styledir)
{
	PidginConvTheme *ret = g_object_new(PIDGIN_TYPE_CONV_THEME, NULL);
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(ret);
	priv->style_dir = g_strdup(styledir);

	return ret;
}

static const char *
get_template_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->template_html)
		return priv->template_html;

	/* The template path can either come from the theme, or can
	 * be stock Template.html that comes with the plugin */
	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Template.html", NULL);

	if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
		g_free(file);
		file = g_build_filename(DATADIR, "pidgin", "webkit", "Template.html", NULL);
	}

	if (!g_file_get_contents(file, &priv->template_html, NULL, NULL)) {
		purple_debug_error("webkit", "Could not locate a Template.html (%s)\n", file);
		priv->template_html = g_strdup("");
	}
	g_free(file);

	return priv->template_html;
}

static const char *
get_status_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->status_html)
		return priv->status_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Status.html", NULL);
	if (!g_file_get_contents(file, &priv->status_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s could not find Resources/Status.html", priv->style_dir);
		priv->status_html = g_strdup("");
	}
	g_free(file);

	return priv->status_html;
}

static const char *
get_basestyle_css(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->basestyle_css)
		return priv->basestyle_css;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "main.css", NULL);
	if (!g_file_get_contents(file, &priv->basestyle_css, NULL, NULL))
		priv->basestyle_css = g_strdup("");
	g_free(file);

	return priv->basestyle_css;
}

static const char *
get_header_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->header_html)
		return priv->header_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Header.html", NULL);
	if (!g_file_get_contents(file, &priv->header_html, NULL, NULL))
		priv->header_html = g_strdup("");
	g_free(file);

	return priv->header_html;
}

static const char *
get_footer_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->footer_html)
		return priv->footer_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Footer.html", NULL);
	if (!g_file_get_contents(file, &priv->footer_html, NULL, NULL))
		priv->footer_html = g_strdup("");
	g_free(file);

	return priv->footer_html;
}

static const char *
get_incoming_content_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->incoming_content_html)
		return priv->incoming_content_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Incoming", "Content.html", NULL);
	if (!g_file_get_contents(file, &priv->incoming_content_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s did not have a Incoming/Content.html\n", priv->style_dir);
		priv->incoming_content_html = g_strdup("");
	}
	g_free(file);

	return priv->incoming_content_html;
}

static const char *
get_incoming_next_content_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->incoming_next_content_html)
		return priv->incoming_next_content_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Incoming", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &priv->incoming_next_content_html, NULL, NULL)) {
		priv->incoming_next_content_html = g_strdup(priv->incoming_content_html);
	}
	g_free(file);

	return priv->incoming_next_content_html;
}

static const char *
get_outgoing_content_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->outgoing_content_html)
		return priv->outgoing_content_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Outgoing", "Content.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_content_html, NULL, NULL)) {
		priv->outgoing_content_html = g_strdup(priv->incoming_content_html);
	}
	g_free(file);

	return priv->outgoing_content_html;
}

static const char *
get_outgoing_next_content_html(PidginConvThemePrivate *priv)
{
	char *file;

	if (priv->outgoing_next_content_html)
		return priv->outgoing_next_content_html;

	file = g_build_filename(priv->style_dir, "Contents", "Resources", "Outgoing", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_next_content_html, NULL, NULL)) {
		priv->outgoing_next_content_html = g_strdup(priv->outgoing_content_html);
	}

	return priv->outgoing_next_content_html;
}

static char *
replace_header_tokens(const char *text, PurpleConversation *conv)
{
	GString *str = g_string_new(NULL);
	const char *cur = text;
	const char *prev = cur;

	if (text == NULL)
		return NULL;

	while ((cur = strchr(cur, '%'))) {
		const char *replace = NULL;
		char *fin = NULL;

		if (!strncmp(cur, "%chatName%", strlen("%chatName%"))) {
			replace = conv->name;
		} else if (!strncmp(cur, "%sourceName%", strlen("%sourceName%"))) {
			replace = purple_account_get_alias(conv->account);
			if (replace == NULL)
				replace = purple_account_get_username(conv->account);
		} else if (!strncmp(cur, "%destinationName%", strlen("%destinationName%"))) {
			PurpleBuddy *buddy = purple_find_buddy(conv->account, conv->name);
			if (buddy) {
				replace = purple_buddy_get_alias(buddy);
			} else {
				replace = conv->name;
			}
		} else if (!strncmp(cur, "%incomingIconPath%", strlen("%incomingIconPath%"))) {
			PurpleBuddyIcon *icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));
			replace = purple_buddy_icon_get_full_path(icon);
		} else if (!strncmp(cur, "%outgoingIconPath%", strlen("%outgoingIconPath%"))) {
		} else if (!strncmp(cur, "%timeOpened", strlen("%timeOpened"))) {
			char *format = NULL;
			if (*(cur + strlen("%timeOpened")) == '{') {
				const char *start = cur + strlen("%timeOpened") + 1;
				char *end = strstr(start, "}%");
				if (!end) /* Invalid string */
					continue;
				format = g_strndup(start, end - start);
				fin = end + 1;
			}
			replace = purple_utf8_strftime(format ? format : "%X", NULL);
			g_free(format);
		} else {
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len(str, prev, cur - prev);
		g_string_append(str, replace);

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;
		} else {
			prev = cur = strchr(cur + 1, '%') + 1;
		}
	}

	/* And wrap it up */
	g_string_append(str, prev);
	return g_string_free(str, FALSE);
}

static char *
replace_template_tokens(PidginConvTheme *theme, const char *text, const char *header, const char *footer)
{
	PidginConvThemePrivate *priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	GString *str = g_string_new(NULL);

	char **ms = g_strsplit(text, "%@", 6);
	char *base = NULL;
	char *csspath = pidgin_conversation_theme_get_css(theme);
	if (ms[0] == NULL || ms[1] == NULL || ms[2] == NULL || ms[3] == NULL || ms[4] == NULL || ms[5] == NULL) {
		g_strfreev(ms);
		g_string_free(str, TRUE);
		return NULL;
	}

	g_string_append(str, ms[0]);
	g_string_append(str, "file://");
	base = g_build_filename(priv->style_dir, "Contents", "Resources", "Template.html", NULL);
	g_string_append(str, base);
	g_free(base);

	g_string_append(str, ms[1]);

	g_string_append(str, get_basestyle_css(priv));

	g_string_append(str, ms[2]);

	g_string_append(str, "file://");
	g_string_append(str, csspath);

	g_string_append(str, ms[3]);
	if (header)
		g_string_append(str, header);
	g_string_append(str, ms[4]);
	if (footer)
		g_string_append(str, footer);
	g_string_append(str, ms[5]);

	g_strfreev(ms);
	g_free(csspath);
	return g_string_free(str, FALSE);
}

static void
set_theme_webkit_settings(WebKitWebView *webview, PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	WebKitWebSettings *settings;

	g_object_get(G_OBJECT(webview), "settings", &settings, NULL);
	if (priv->default_font_family)
		g_object_set(G_OBJECT(settings), "default-font-family", priv->default_font_family, NULL);

	if (priv->default_font_size)
		g_object_set(G_OBJECT(settings), "default-font-size", GINT_TO_POINTER(priv->default_font_size), NULL);

	/* this does not work :( */
	webkit_web_view_set_transparent(webview, priv->default_background_is_transparent);
}

/*
 * The style specification says that if the conversation is a group
 * chat then the <div id="Chat"> element will be given a class
 * 'groupchat'. I can't add another '%@' in Template.html because
 * that breaks style-specific Template.html's. I have to either use libxml
 * or conveniently play with WebKit's javascript engine. The javascript
 * engine should work, but it's not an identical behavior.
 */
static void
webkit_set_groupchat(GtkWebView *webview)
{
	gtk_webview_safe_execute_script(webview, "document.getElementById('Chat').className = 'groupchat'");
}

static void
webkit_on_webview_destroy(GtkObject *object, gpointer data)
{
	g_object_unref(G_OBJECT(data));
	g_object_set_data(G_OBJECT(object), MESSAGE_STYLE_KEY, NULL);
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

void
pidgin_conversation_theme_save_state(const PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	char *prefname;
	char *variant;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	prefname = g_strdup_printf("/plugins/gtk/adiumthemes/%s", priv->cf_bundle_identifier);
	variant = g_strdup_printf("%s/variant", prefname);

	purple_debug_info("webkit", "saving state with variant %s\n", priv->variant);
	purple_prefs_add_none(prefname);
	purple_prefs_add_string(variant, "");
	purple_prefs_set_string(variant, priv->variant);

	g_free(prefname);
	g_free(variant);
}

static void
pidgin_conversation_theme_load_state(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	char *prefname;
	char *variant;
	const char* value;
	gboolean changed;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	prefname = g_strdup_printf("/plugins/gtk/adiumthemes/%s", priv->cf_bundle_identifier);
	variant = g_strdup_printf("%s/variant", prefname);

	value = purple_prefs_get_string(variant);
	changed = !priv->variant || !g_str_equal(priv->variant, value);

	g_free(priv->variant);
	priv->variant = g_strdup(value);

	if (changed)
		pidgin_conversation_theme_read_info_plist(theme, priv->variant);

	g_free(prefname);
	g_free(variant);
}


static gboolean
parse_info_plist_key_value(xmlnode *key, gpointer destination, const char *expected)
{
	xmlnode *val = key->next;

	for (; val && val->type != XMLNODE_TYPE_TAG; val = val->next)
		;
	if (!val)
		return FALSE;

	if (expected == NULL || g_str_equal(expected, "string")) {
		char **dest = (char **)destination;
		if (!g_str_equal(val->name, "string"))
			return FALSE;
		if (*dest)
			g_free(*dest);
		*dest = xmlnode_get_data_unescaped(val);
	} else if (g_str_equal(expected, "integer")) {
		int *dest = (int *)destination;
		char *value = xmlnode_get_data_unescaped(val);

		if (!g_str_equal(val->name, "integer"))
			return FALSE;
		*dest = atoi(value);
		g_free(value);
	} else if (g_str_equal(expected, "boolean")) {
		gboolean *dest = (gboolean *)destination;
		if (g_str_equal(val->name, "true"))
			*dest = TRUE;
		else if (g_str_equal(val->name, "false"))
			*dest = FALSE;
		else
			return FALSE;
	} else return FALSE;

	return TRUE;
}

static gboolean
str_for_key(const char *key, const char *found, const char *variant)
{
	if (g_str_equal(key, found))
		return TRUE;
	if (!variant)
		return FALSE;
	return (g_str_has_prefix(found, key)
		&& g_str_has_suffix(found, variant)
		&& strlen(found) == strlen(key) + strlen(variant) + 1);
}

/**
 * Info.plist should be re-read every time the variant changes, this is because
 * the keys that take precedence depend on the value of the current variant.
 */
void
pidgin_conversation_theme_read_info_plist(PidginConvTheme *theme, const char *variant)
{
	PidginConvThemePrivate *priv;
	char *contents;
	xmlnode *plist, *iter;
	xmlnode *dict;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	/* note that if a variant is used the option:VARIANTNAME takes precedence */
	contents = g_build_filename(priv->style_dir, "Contents", NULL);
	plist = xmlnode_from_file(contents, "Info.plist", "Info.plist", "webkit");
	dict = xmlnode_get_child(plist, "dict");

	g_assert (dict);
	for (iter = xmlnode_get_child(dict, "key"); iter; iter = xmlnode_get_next_twin(iter)) {
		char* key = xmlnode_get_data_unescaped(iter);
		gboolean pr = TRUE;

		if (g_str_equal("MessageViewVersion", key))
			pr = parse_info_plist_key_value(iter, &priv->message_view_version, "integer");
		else if (g_str_equal("CFBundleName", key))
			pr = parse_info_plist_key_value(iter, &priv->cf_bundle_name, "string");
		else if (g_str_equal("CFBundleIdentifier", key))
			pr = parse_info_plist_key_value(iter, &priv->cf_bundle_identifier, "string");
		else if (g_str_equal("CFBundleGetInfoString", key))
			pr = parse_info_plist_key_value(iter, &priv->cf_bundle_get_info_string, "string");
		else if (str_for_key("DefaultFontFamily", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->default_font_family, "string");
		else if (str_for_key("DefaultFontSize", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->default_font_size, "integer");
		else if (str_for_key("ShowsUserIcons", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->shows_user_icons, "boolean");
		else if (str_for_key("DisableCombineConsecutive", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->disable_combine_consecutive, "boolean");
		else if (str_for_key("DefaultBackgroundIsTransparent", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->default_background_is_transparent, "boolean");
		else if (str_for_key("DisableCustomBackground", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->disable_custom_background, "boolean");
		else if (str_for_key("DefaultBackgroundColor", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->default_background_color, "string");
		else if (str_for_key("AllowTextColors", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->allow_text_colors, "integer");
		else if (str_for_key("ImageMask", key, variant))
			pr = parse_info_plist_key_value(iter, &priv->image_mask, "string");

		if (!pr)
			purple_debug_warning("webkit", "Failed to parse key %s\n", key);
		g_free(key);
	}

	xmlnode_free(plist);
}

PidginConvTheme *
pidgin_conversation_theme_load(const char *styledir)
{
	/*
	 * the loading process described:
	 *
	 * First we load all the style .html files, etc.
	 * The we load any config options that have been stored for
	 * this variant.
	 * Then we load the Info.plist, for the currently decided variant.
	 * At this point, if we find that variants exist, yet
	 * we don't have a variant selected, we choose DefaultVariant
	 * and if that does not exist, we choose the first one in the
	 * directory.
	 */
	PidginConvTheme *theme = NULL;
	PidginConvThemePrivate *priv;

	theme = pidgin_conversation_theme_new(styledir);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	/* load all other files */

	pidgin_conversation_theme_read_info_plist(theme, NULL);
	pidgin_conversation_theme_load_state(theme);

	/* non variant dependent Info.plist checks */
	if (priv->message_view_version < 3) {
		purple_debug_info("webkit", "%s is a legacy style (version %d) and will not be loaded\n", priv->cf_bundle_name, priv->message_view_version);
		g_object_unref(G_OBJECT(theme));
		return NULL;
	}

	if (!priv->variant)
	{
		GList *variants = pidgin_conversation_theme_get_variants(theme);

		if (variants)
			pidgin_conversation_theme_set_variant(theme, variants->data);

		for (; variants; variants = g_list_delete_link(variants, variants))
			g_free(variants->data);
	}

	return theme;
}

PidginConvTheme *
pidgin_conversation_theme_copy(const PidginConvTheme *theme)
{
	PidginConvTheme *ret;
	PidginConvThemePrivate *old, *new;

	old = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	ret = pidgin_conversation_theme_new(old->style_dir);
	new = PIDGIN_CONV_THEME_GET_PRIVATE(ret);

	new->variant = g_strdup(old->variant);
	new->message_view_version = old->message_view_version;
	new->cf_bundle_name = g_strdup(old->cf_bundle_name);
	new->cf_bundle_identifier = g_strdup(old->cf_bundle_identifier);
	new->cf_bundle_get_info_string = g_strdup(old->cf_bundle_get_info_string);
	new->default_font_family = g_strdup(old->default_font_family);
	new->default_font_size = old->default_font_size;
	new->shows_user_icons = old->shows_user_icons;
	new->disable_combine_consecutive = old->disable_combine_consecutive;
	new->default_background_is_transparent = old->default_background_is_transparent;
	new->disable_custom_background = old->disable_custom_background;
	new->default_background_color = g_strdup(old->default_background_color);
	new->allow_text_colors = old->allow_text_colors;
	new->image_mask = g_strdup(old->image_mask);
	new->default_variant = g_strdup(old->default_variant);

	new->template_html = g_strdup(old->template_html);
	new->header_html = g_strdup(old->header_html);
	new->footer_html = g_strdup(old->footer_html);
	new->incoming_content_html = g_strdup(old->incoming_content_html);
	new->outgoing_content_html = g_strdup(old->outgoing_content_html);
	new->incoming_next_content_html = g_strdup(old->incoming_next_content_html);
	new->outgoing_next_content_html = g_strdup(old->outgoing_next_content_html);
	new->status_html = g_strdup(old->status_html);
	new->basestyle_css = g_strdup(old->basestyle_css);

	return ret;
}

void
pidgin_conversation_theme_set_variant(PidginConvTheme *theme, const char *variant)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	/* I'm not going to test whether this variant is valid! */
	g_free(priv->variant);
	priv->variant = g_strdup(variant);

	pidgin_conversation_theme_read_info_plist(theme, variant);

	/* todo, the style has "changed". Ideally, I would like to use signals at this point. */
}

char *
pidgin_conversation_theme_get_variant(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	return g_strdup(priv->variant);
}

/**
 * Get a list of variants supported by the style.
 */
GList *
pidgin_conversation_theme_get_variants(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	GList *ret = NULL;
	GDir *variants;
	const char *css_file;
	char *css;
	char *variant_dir;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	g_assert(priv->style_dir);
	variant_dir = g_build_filename(priv->style_dir, "Contents", "Resources", "Variants", NULL);

	variants = g_dir_open(variant_dir, 0, NULL);
	if (!variants)
		return NULL;

	while ((css_file = g_dir_read_name(variants)) != NULL) {
		if (!g_str_has_suffix(css_file, ".css"))
			continue;

		css = g_strndup(css_file, strlen(css_file) - 4);
		ret = g_list_append(ret, css);
	}

	g_dir_close(variants);
	g_free(variant_dir);

	ret = g_list_sort(ret, (GCompareFunc)g_strcmp0);
	return ret;
}

char *
pidgin_conversation_theme_get_css(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	if (!priv->variant) {
		return g_build_filename(priv->style_dir, "Contents", "Resources", "main.css", NULL);
	} else {
		char *file = g_strdup_printf("%s.css", priv->variant);
		char *ret = g_build_filename(priv->style_dir, "Contents", "Resources", "Variants",  file, NULL);
		g_free(file);
		return ret;
	}
}

/**
 * Called when either a new PurpleConversation is created
 * or when a PidginConversation changes its active PurpleConversation
 * This will not change the theme if the theme is already set.
 * (This is to prevent accidental theme changes if a new
 * PurpleConversation gets added.
 *
 * FIXME: it's not at all clear to me as to how
 * Adium themes handle the case when the PurpleConversation
 * changes.
 */
void
pidgin_conversation_theme_apply(PidginConvTheme *theme, PurpleConversation *conv)
{
	GtkWidget *webkit = PIDGIN_CONVERSATION(conv)->webview;
	char *header, *footer;
	char *template;
	char *basedir;
	char *baseuri;
	PidginConvTheme *oldTheme;
	PidginConvTheme *copy;
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	oldTheme = g_object_get_data(G_OBJECT(webkit), MESSAGE_STYLE_KEY);
	if (oldTheme)
		return;

	g_assert(theme);

	header = replace_header_tokens(get_header_html(priv), conv);
	footer = replace_header_tokens(get_footer_html(priv), conv);
	template = replace_template_tokens(theme, get_template_html(priv), header, footer);

	g_assert(template);

	purple_debug_info("webkit", "template: %s\n", template);

	set_theme_webkit_settings(WEBKIT_WEB_VIEW(webkit), theme);
	basedir = g_build_filename(priv->style_dir, "Contents", "Resources", "Template.html", NULL);
	baseuri = g_strdup_printf("file://%s", basedir);
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webkit), template, "text/html", "UTF-8", baseuri);

	copy = pidgin_conversation_theme_copy(theme);
	g_object_set_data(G_OBJECT(webkit), MESSAGE_STYLE_KEY, copy);

	g_object_unref(G_OBJECT(theme));
	/* I need to unref this style when the webkit object destroys */
	g_signal_connect(G_OBJECT(webkit), "destroy", G_CALLBACK(webkit_on_webview_destroy), copy);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		webkit_set_groupchat(GTK_WEBVIEW(webkit));
	g_free(basedir);
	g_free(baseuri);
	g_free(header);
	g_free(footer);
	g_free(template);
}

