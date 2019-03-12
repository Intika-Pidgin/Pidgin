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
 */

#include <talkatu.h>

#include "internal.h"
#include "plugins.h"

#include "pidgindebugplugininfo.h"

/**
 * SECTION:pidgindebugplugininfo
 * @Title: Debug Plugin Info
 * @Short_description: A widget that lists verbose plugin info
 *
 * When helping users troubleshoot issues with Pidgin we often need to know
 * what plugins that have installed/running.  This widget gives them an easy
 * way to get that info to us.
 */

typedef struct {
	GtkTextBuffer *buffer;
	GtkWidget *view;
} PidginDebugPluginInfoPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(PidginDebugPluginInfo, pidgin_debug_plugin_info, GTK_TYPE_DIALOG)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gint
purple_debug_plugin_compare_plugin_id(gconstpointer a, gconstpointer b) {
	return g_strcmp0(
		purple_plugin_info_get_id(purple_plugin_get_info(PURPLE_PLUGIN(a))),
		purple_plugin_info_get_id(purple_plugin_get_info(PURPLE_PLUGIN(b)))
	);
}

static gchar *
pidgin_debug_plugin_info_build_html(void) {
	GString *str = g_string_new(NULL);
	GList *plugins = NULL, *l = NULL;

	g_string_append_printf(str, "<h2>%s</h2><dl>", _("Plugin Information"));

	plugins = g_list_sort(
		purple_plugins_find_all(),
		purple_debug_plugin_compare_plugin_id
	);

	for(l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);
		PurplePluginInfo *info = purple_plugin_get_info(plugin);
		PurplePluginExtraCb extra_cb;
		gchar *name = g_markup_escape_text(purple_plugin_info_get_name(info), -1);
		gchar *version, *license, *website, *id;
		gchar *authors = NULL, *extra = NULL;
		const gchar *error_message = NULL;
		gchar **authorsv;
		gboolean loaded;

		g_object_get(
			G_OBJECT(info),
			"authors", &authorsv,
			"version", &version,
			"license-id", &license,
			"website", &website,
			"id", &id,
			"extra-cb", &extra_cb,
			NULL
		);

		if(authorsv != NULL) {
			gchar *authorstmp = g_strjoinv(", ", (gchar **)authorsv);
			g_strfreev(authorsv);

			authors = g_markup_escape_text(authorstmp, -1);
			g_free(authorstmp);
		}

		if(extra_cb != NULL) {
			extra = extra_cb(plugin);
		}

		error_message = purple_plugin_info_get_error(info);

		loaded = purple_plugin_is_loaded(plugin);

		g_string_append_printf(str, "<dt>%s</dt><dd>", name);
		g_free(name);

		/* this is not translated as it's meant for debugging */
		g_string_append_printf(
			str,
			"<b>Authors:</b> %s<br/>"
			"<b>Version:</b> %s<br/>"
			"<b>License:</b> %s<br/>"
			"<b>Website:</b> %s<br/>"
			"<b>ID:</b> %s<br/>"
			"<b>Extra:</b> %s<br/>"
			"<b>Errors:</b> %s</br>"
			"<b>Loaded:</b> %s",
			authors ? authors : "",
			version ? version : "",
			license ? license : "",
			website ? website : "",
			id ? id : "",
			extra ? extra : "",
			error_message ? error_message : "",
			loaded ? "Yes" : "No"
		);

		g_clear_pointer(&authors, g_free);
		g_free(version);
		g_free(license);
		g_free(website);
		g_free(id);
		g_clear_pointer(&extra, g_free);

		g_string_append(str, "</dd>");
	}

	g_list_free(plugins);

	g_string_append(str, "</dl>");

	return g_string_free(str, FALSE);
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
static void
pidgin_debug_plugin_info_init(PidginDebugPluginInfo *debug_plugin_info) {
	gtk_widget_init_template(GTK_WIDGET(debug_plugin_info));
}

static void
pidgin_debug_plugin_info_class_init(PidginDebugPluginInfoClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/Debug/plugininfo.ui"
	);

	gtk_widget_class_bind_template_child_private(widget_class, PidginDebugPluginInfo, buffer);
	gtk_widget_class_bind_template_child_private(widget_class, PidginDebugPluginInfo, view);
}

/******************************************************************************
 * Public API
 *****************************************************************************/

/**
 * pidgin_debug_plugin_info_new:
 *
 * Creates a new #PidginDebugPluginInfo that provides the user with an easy way
 * to share information about their plugin state for debugging purposes.
 *
 * Returns: (transfer full): The new #PidginDebugPluginInfo instance.
 */
GtkWidget *pidgin_debug_plugin_info_new(void) {
	return GTK_WIDGET(g_object_new(
		PIDGIN_TYPE_DEBUG_PLUGIN_INFO,
		NULL
	));
}

void
pidgin_debug_plugin_info_show(void) {
	PidginDebugPluginInfoPrivate *priv = NULL;
	GtkWidget *win = NULL;
	gchar *text = NULL;

	win = pidgin_debug_plugin_info_new();
	priv = pidgin_debug_plugin_info_get_instance_private(PIDGIN_DEBUG_PLUGIN_INFO(win));

	text = pidgin_debug_plugin_info_build_html();
	g_warning("text: '%s'", text);
	talkatu_markup_set_html(TALKATU_BUFFER(priv->buffer), text, -1);
	g_free(text);

	gtk_widget_show_all(win);
	gtk_window_present(GTK_WINDOW(win));
}
