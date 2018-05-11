/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#include "internal.h"

#include "options.h"

#include "network.h"

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static gboolean
debug_cb(const gchar *option_name, const gchar *value,
		gpointer data, GError **error)
{
	purple_debug_set_enabled(TRUE);

	if (purple_strequal(value, "colored")) {
		purple_debug_set_colored(TRUE);
	}

	return TRUE;
}

static gboolean
force_online_cb(const gchar *option_name, const gchar *value,
                gpointer data, GError **error)
{
	purple_network_force_online();

	return TRUE;
}

/******************************************************************************
 * API
 *****************************************************************************/

/**
 * purple_get_option_group:
 *
 * Returns a #GOptionGroup for the commandline arguments recognized by
 * LibPurple.  You should add this option group to your #GOptionContext with
 * g_option_context_add_group(), if you are using g_option_context_parse() to
 * parse your commandline arguments.
 *
 * Return Value: (transfer full): a #GOptionGroup for the commandline arguments
 *                                recognized by LibPurple.
 */
GOptionGroup *
purple_get_option_group(void) {
	GOptionGroup *group = NULL;
	GOptionEntry entries[] = {
		{
			"debug", 'd', G_OPTION_FLAG_OPTIONAL_ARG,
			G_OPTION_ARG_CALLBACK, &debug_cb,
			_("print debugging messages to stdout"),
			_("[colored]")
		}, {
			"force-online", 'f', G_OPTION_FLAG_NO_ARG,
			G_OPTION_ARG_CALLBACK, &force_online_cb,
			_("force online, regardless of network status"),
			NULL
		}, {
			NULL
		},
	};

	group = g_option_group_new(
		"libpurple",
		_("LibPurple options"),
		_("Show LibPurple Options"),
		NULL,
		NULL
	);

	g_option_group_add_entries(group, entries);

	return group;
}
