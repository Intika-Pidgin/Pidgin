/*
 * PluginPref Example Plugin
 *
 * Copyright (C) 2004, Gary Kramlich <amc_grim@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

/* When writing a third-party plugin, do not include libpurple's internal.h
 * included below. This file is for internal libpurple use only. We're including
 * it here for our own convenience. */
#include "internal.h"

/* This file defines PURPLE_PLUGINS and includes all the libpurple headers */
#include <purple.h>

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label("boolean");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/bool",
									"boolean pref");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_label("integer");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/int",
									"integer pref");
	purple_plugin_pref_set_bounds(ppref, 0, 255);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/int_choice",
									"integer choice");
	purple_plugin_pref_set_pref_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(ppref, "One", GINT_TO_POINTER(1));
	purple_plugin_pref_add_choice(ppref, "Two", GINT_TO_POINTER(2));
	purple_plugin_pref_add_choice(ppref, "Four", GINT_TO_POINTER(4));
	purple_plugin_pref_add_choice(ppref, "Eight", GINT_TO_POINTER(8));
	purple_plugin_pref_add_choice(ppref, "Sixteen", GINT_TO_POINTER(16));
	purple_plugin_pref_add_choice(ppref, "Thirty Two", GINT_TO_POINTER(32));
	purple_plugin_pref_add_choice(ppref, "Sixty Four", GINT_TO_POINTER(64));
	purple_plugin_pref_add_choice(ppref, "One Hundred Twenty Eight", GINT_TO_POINTER(128));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_label("string");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
								"/plugins/core/pluginpref_example/string",
								"string pref");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
								"/plugins/core/pluginpref_example/masked_string",
								"masked string");
	purple_plugin_pref_set_masked(ppref, TRUE);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
							"/plugins/core/pluginpref_example/max_string",
							"string pref\n(max length of 16)");
	purple_plugin_pref_set_max_length(ppref, 16);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
							"/plugins/core/pluginpref_example/string_choice",
							"string choice");
	purple_plugin_pref_set_pref_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(ppref, "red", "red");
	purple_plugin_pref_add_choice(ppref, "orange", "orange");
	purple_plugin_pref_add_choice(ppref, "yellow", "yellow");
	purple_plugin_pref_add_choice(ppref, "green", "green");
	purple_plugin_pref_add_choice(ppref, "blue", "blue");
	purple_plugin_pref_add_choice(ppref, "purple", "purple");
	purple_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Gary Kramlich <amc_grim@users.sf.net>",
		NULL
	};

	return purple_plugin_info_new(
		"id",             "core-pluginpref_example",
		"name",           "Pluginpref Example",
		"version",        DISPLAY_VERSION,
		"category",       "Example",
		"summary",        "An example of how to use pluginprefs",
		"description",    "An example of how to use pluginprefs",
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
	purple_prefs_add_none("/plugins/core/pluginpref_example");
	purple_prefs_add_bool("/plugins/core/pluginpref_example/bool", TRUE);
	purple_prefs_add_int("/plugins/core/pluginpref_example/int", 0);
	purple_prefs_add_int("/plugins/core/pluginpref_example/int_choice", 1);
	purple_prefs_add_string("/plugins/core/pluginpref_example/string",
							"string");
	purple_prefs_add_string("/plugins/core/pluginpref_example/max_string",
							"max length string");
	purple_prefs_add_string("/plugins/core/pluginpref_example/masked_string", "masked");
	purple_prefs_add_string("/plugins/core/pluginpref_example/string_choice", "red");

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(ppexample, plugin_query, plugin_load, plugin_unload);
