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

#include <glib.h>

#include "internal.h"
#include "notify.h"
#include "plugins.h"
#include "version.h"

#define PREF_ROOT "/plugins"
#define PREF_TEST "/plugins/tests"
#define PREF_PREFIX "/plugins/tests/request-input"
#define PREF_SINGLE PREF_PREFIX "/single"
#define PREF_MULTIPLE PREF_PREFIX "/multiple"
#define PREF_HTML PREF_PREFIX "/html"

static void
plugin_input_callback(const gchar *pref, const gchar *text) {
	purple_prefs_set_string(pref, text);
}

static void
plugin_input_single(PurplePluginAction *action) {
	purple_request_input(
		NULL,
		_("Test request input single"),
		_("Test request input single"),
		NULL,
		purple_prefs_get_string(PREF_SINGLE),
		FALSE,
		FALSE,
		NULL,
		_("OK"),
		PURPLE_CALLBACK(plugin_input_callback),
		_("Cancel"),
		NULL,
		purple_request_cpar_new(),
		PREF_SINGLE
	);
}

static void
plugin_input_multiple(PurplePluginAction *action) {
	purple_request_input(
		NULL,
		_("Test request input multiple"),
		_("Test request input multiple"),
		NULL,
		purple_prefs_get_string(PREF_MULTIPLE),
		TRUE,
		FALSE,
		NULL,
		_("OK"),
		PURPLE_CALLBACK(plugin_input_callback),
		_("Cancel"),
		NULL,
		purple_request_cpar_new(),
		PREF_MULTIPLE
	);
}

static void
plugin_input_html(PurplePluginAction *action) {
	purple_request_input(
		NULL,
		_("Test request input HTML"),
		_("Test request input HTML"),
		NULL,
		purple_prefs_get_string(PREF_HTML),
		FALSE,
		FALSE,
		"html",
		_("OK"),
		PURPLE_CALLBACK(plugin_input_callback),
		_("Cancel"),
		NULL,
		purple_request_cpar_new(),
		PREF_HTML
	);
}

static GList *
plugin_actions(PurplePlugin *plugin) {
	GList *l = NULL;
	PurplePluginAction *action = NULL;

	action = purple_plugin_action_new(_("Input single"), plugin_input_single);
	l = g_list_append(l, action);

	action = purple_plugin_action_new(_("Input multiple"), plugin_input_multiple);
	l = g_list_append(l, action);

	action = purple_plugin_action_new(_("Input html"), plugin_input_html);
	l = g_list_append(l, action);

	return l;
}

static PurplePluginInfo *
plugin_query(GError **error) {
	const gchar * const authors[] = {
		"Gary Kramlich <grim@reaperworld.com>",
		NULL
	};

	return purple_plugin_info_new(
		"id", "core-test_request_input",
		"name", N_("Test: request input"),
		"version", DISPLAY_VERSION,
		"category", N_("Testing"),
		"summary", N_("Test Request Input"),
		"description", N_("This plugin adds actions to test purple_request_input"),
		"authors", authors,
		"website", "https://pidgin.im",
		"abi-version", PURPLE_ABI_VERSION,
		"actions-cb", plugin_actions,
		NULL
	);
};

static gboolean
plugin_load(PurplePlugin *plugin, GError **error) {
	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_none(PREF_TEST);
	purple_prefs_add_none(PREF_PREFIX);
	purple_prefs_add_string(PREF_SINGLE, "");
	purple_prefs_add_string(PREF_MULTIPLE, "");
	purple_prefs_add_string(PREF_HTML, "");

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error) {
	return TRUE;
}

PURPLE_PLUGIN_INIT(test_request_input, plugin_query, plugin_load, plugin_unload);
