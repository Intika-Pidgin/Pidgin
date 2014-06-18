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

#include "libgadu-events.h"

#include <debug.h>

#include "avatar.h"
#include "edisc.h"

void ggp_events_user_data(PurpleConnection *gc, struct gg_event_user_data *data)
{
	guint user_idx;
	gboolean is_update;

	purple_debug_info("gg", "GG_EVENT_USER_DATA [type=%d, user_count=%"
		G_GSIZE_FORMAT "]\n", data->type, data->user_count);

	/*
	type =
		1, 3:	user information sent after connecting (divided by
			20 contacts; 3 - last one; 1 - rest of them)
		0: data update
	*/
	is_update = (data->type == 0);

	for (user_idx = 0; user_idx < data->user_count; user_idx++) {
		struct gg_event_user_data_user *data_user =
			&data->users[user_idx];
		uin_t uin = data_user->uin;
		guint attr_idx;
		gboolean got_avatar = FALSE;
		for (attr_idx = 0; attr_idx < data_user->attr_count; attr_idx++) {
			struct gg_event_user_data_attr *data_attr =
				&data_user->attrs[attr_idx];
			if (strcmp(data_attr->key, "avatar") == 0) {
				time_t timestamp;
				if (data_attr->type == 0) {
					ggp_avatar_buddy_remove(gc, uin);
					continue;
				}

				timestamp = atoi(data_attr->value);
				if (timestamp <= 0)
					continue;
				got_avatar = TRUE;
				ggp_avatar_buddy_update(gc, uin, timestamp);
			}
		}

		if (!is_update && !got_avatar)
			ggp_avatar_buddy_remove(gc, uin);
	}
}

static void ggp_events_new_version(const gchar *data)
{
	/* data = {"severity":"download"} */
	purple_debug_info("gg", "Gadu-Gadu server reports new client version."
		" %s", data);
}

void ggp_events_json(PurpleConnection *gc, struct gg_event_json_event *ev)
{
	static const gchar *ignored_events[] = {
		"edisc/scope_files_changed",
		"notifications/state",
		"invitations/list",
		"notifications/list", /* gifts */
		NULL
	};
	const gchar **it;

	if (g_strcmp0("edisc/send_ticket_changed", ev->type) == 0) {
		ggp_edisc_xfer_ticket_changed(gc, ev->data);
		return;
	}

	if (g_strcmp0("updates/new-version", ev->type) == 0) {
		ggp_events_new_version(ev->data);
		return;
	}

	for (it = ignored_events; *it != NULL; it++) {
		if (g_strcmp0(*it, ev->type) == 0)
			return;
	}

	if (purple_debug_is_unsafe() && purple_debug_is_verbose())
		purple_debug_warning("gg", "ggp_events_json: "
			"unhandled event \"%s\": %s\n",
			ev->type, ev->data);
	else
		purple_debug_warning("gg", "ggp_events_json: "
			"unhandled event \"%s\"\n", ev->type);
}
