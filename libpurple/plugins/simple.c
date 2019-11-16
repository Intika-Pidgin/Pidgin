/*
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

#include "internal.h"
#include <purple.h>

/** Plugin id : type-author-name (to guarantee uniqueness) */
#define SIMPLE_PLUGIN_ID "core-ewarmenhoven-simple"

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Eric Warmenhoven <eric@warmenhoven.org>",
		NULL
	};

	return purple_plugin_info_new(
		"id",           SIMPLE_PLUGIN_ID,
		"name",         N_("Simple Plugin"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Testing"),
		"summary",      N_("Tests to see that most things are working."),
		"description",  N_("Tests to see that most things are working."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_debug(PURPLE_DEBUG_INFO, "simple", "simple plugin loaded.\n");

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	purple_debug(PURPLE_DEBUG_INFO, "simple", "simple plugin unloaded.\n");

	return TRUE;
}

PURPLE_PLUGIN_INIT(simple, plugin_query, plugin_load, plugin_unload);
