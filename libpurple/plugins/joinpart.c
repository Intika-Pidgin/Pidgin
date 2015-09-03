/**
 * purple
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
#include "conversation.h"
#include "debug.h"
#include "plugins.h"
#include "version.h"

#define JOINPART_PLUGIN_ID "core-rlaager-joinpart"


/* Preferences */

/* The number of minutes before a person is considered
 * to have stopped being part of active conversation. */
#define DELAY_PREF "/plugins/core/joinpart/delay"
#define DELAY_DEFAULT 10

/* The number of people that must be in a room for this
 * plugin to have any effect */
#define THRESHOLD_PREF "/plugins/core/joinpart/threshold"
#define THRESHOLD_DEFAULT 20

/* Hide buddies */
#define HIDE_BUDDIES_PREF "/plugins/core/joinpart/hide_buddies"
#define HIDE_BUDDIES_DEFAULT FALSE

struct joinpart_key
{
	PurpleConversation *conv;
	char *user;
};

static guint joinpart_key_hash(const struct joinpart_key *key)
{
	g_return_val_if_fail(key != NULL, 0);

	return g_direct_hash(key->conv) + g_str_hash(key->user);
}

static gboolean joinpart_key_equal(const struct joinpart_key *a, const struct joinpart_key *b)
{
	if (a == NULL)
		return (b == NULL);
	else if (b == NULL)
		return FALSE;

	return (a->conv == b->conv) && !strcmp(a->user, b->user);
}

static void joinpart_key_destroy(struct joinpart_key *key)
{
	g_return_if_fail(key != NULL);

	g_free(key->user);
	g_free(key);
}

static gboolean should_hide_notice(PurpleConversation *conv, const char *name,
                                   GHashTable *users)
{
	PurpleChatConversation *chat;
	guint threshold;
	struct joinpart_key key;
	time_t *last_said;

	g_return_val_if_fail(conv != NULL, FALSE);
	g_return_val_if_fail(PURPLE_IS_CHAT_CONVERSATION(conv), FALSE);

	/* If the room is small, don't bother. */
	chat = PURPLE_CHAT_CONVERSATION(conv);
	threshold = purple_prefs_get_int(THRESHOLD_PREF);
	if (purple_chat_conversation_get_users_count(chat) < threshold)
		return FALSE;

	if (!purple_prefs_get_bool(HIDE_BUDDIES_PREF) &&
	    purple_blist_find_buddy(purple_conversation_get_account(conv), name))
		return FALSE;

	/* Only show the notice if the user has spoken recently. */
	key.conv = conv;
	key.user = (gchar *)name;
	last_said = g_hash_table_lookup(users, &key);
	if (last_said != NULL)
	{
		int delay = purple_prefs_get_int(DELAY_PREF);
		if (delay > 0 && (*last_said + (delay * 60)) >= time(NULL))
			return FALSE;
	}

	return TRUE;
}

static gboolean chat_user_leaving_cb(PurpleConversation *conv, const char *name,
                               const char *reason, GHashTable *users)
{
	return should_hide_notice(conv, name, users);
}

static gboolean chat_user_joining_cb(PurpleConversation *conv, const char *name,
                                      PurpleChatUserFlags flags,
                                      GHashTable *users)
{
	return should_hide_notice(conv, name, users);
}

static void received_chat_msg_cb(PurpleAccount *account, char *sender,
                                 char *message, PurpleConversation *conv,
                                 PurpleMessageFlags flags, GHashTable *users)
{
	struct joinpart_key key;
	time_t *last_said;

	/* Most of the time, we'll already have tracked the user,
	 * so we avoid memory allocation here. */
	key.conv = conv;
	key.user = sender;
	last_said = g_hash_table_lookup(users, &key);
	if (last_said != NULL)
	{
		/* They just said something, so update the time. */
		time(last_said);
	}
	else
	{
		struct joinpart_key *key2;

		key2 = g_new(struct joinpart_key, 1);
		key2->conv = conv;
		key2->user = g_strdup(sender);

		last_said = g_new(time_t, 1);
		time(last_said);

		g_hash_table_insert(users, key2, last_said);
	}
}

static gboolean check_expire_time(struct joinpart_key *key,
                                  time_t *last_said, time_t *limit)
{
	purple_debug_info("joinpart", "Removing key for %s\n", key->user);
	return (*last_said < *limit);
}

static gboolean clean_users_hash(GHashTable *users)
{
	int delay = purple_prefs_get_int(DELAY_PREF);
	time_t limit = time(NULL) - (60 * delay);

	g_hash_table_foreach_remove(users, (GHRFunc)check_expire_time, &limit);

	return TRUE;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;

	g_return_val_if_fail(plugin != NULL, FALSE);

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("Hide Joins/Parts"));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(THRESHOLD_PREF,
	                                                 /* Translators: Followed by an input request a number of people */
	                                                 _("For rooms with more than this many people"));
	purple_plugin_pref_set_bounds(ppref, 0, 1000);
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(DELAY_PREF,
	                                                 _("If user has not spoken in this many minutes"));
	purple_plugin_pref_set_bounds(ppref, 0, 8 * 60); /* 8 Hours */
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(HIDE_BUDDIES_PREF,
	                                                 _("Apply hiding rules to buddies"));
	purple_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Richard Laager <rlaager@pidgin.im>",
		NULL
	};

	return purple_plugin_info_new(
		"id",             JOINPART_PLUGIN_ID,
		"name",           N_("Join/Part Hiding"),
		"version",        DISPLAY_VERSION,
		"category",       N_("User interface"),
		"summary",        N_("Hides extraneous join/part messages."),
		"description",    N_("This plugin hides join/part messages in "
		                     "large rooms, except for those users actively "
		                     "taking part in a conversation."),
		"authors",        authors,
		"website",        PURPLE_WEBSITE,
		"abi-version",    PURPLE_ABI_VERSION,
		"pref-frame-cb",  get_plugin_pref_frame,
		NULL
	);
}

static gboolean plugin_load(PurplePlugin *plugin, GError **error)
{
	void *conv_handle;
	GHashTable *users;
	guint id;

	purple_prefs_add_none("/plugins/core/joinpart");

	purple_prefs_add_int(DELAY_PREF, DELAY_DEFAULT);
	purple_prefs_add_int(THRESHOLD_PREF, THRESHOLD_DEFAULT);
	purple_prefs_add_bool(HIDE_BUDDIES_PREF, HIDE_BUDDIES_DEFAULT);

	users = g_hash_table_new_full((GHashFunc)joinpart_key_hash,
	                              (GEqualFunc)joinpart_key_equal,
	                              (GDestroyNotify)joinpart_key_destroy,
	                              g_free);

	conv_handle = purple_conversations_get_handle();
	purple_signal_connect(conv_handle, "chat-user-joining", plugin,
	                    PURPLE_CALLBACK(chat_user_joining_cb), users);
	purple_signal_connect(conv_handle, "chat-user-leaving", plugin,
	                    PURPLE_CALLBACK(chat_user_leaving_cb), users);
	purple_signal_connect(conv_handle, "received-chat-msg", plugin,
	                    PURPLE_CALLBACK(received_chat_msg_cb), users);

	/* Cleanup every 5 minutes */
	id = purple_timeout_add_seconds(60 * 5, (GSourceFunc)clean_users_hash, users);

	g_object_set_data(G_OBJECT(plugin), "users", users);
	g_object_set_data(G_OBJECT(plugin), "id", GUINT_TO_POINTER(id));

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin, GError **error)
{
	/* Destroy the hash table. The core plugin code will
	 * disconnect the signals, and since Purple is single-threaded,
	 * we don't have to worry one will be called after this. */
	g_hash_table_destroy((GHashTable *)g_object_get_data(G_OBJECT(plugin), "users"));

	purple_timeout_remove(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(plugin), "id")));

	return TRUE;
}

PURPLE_PLUGIN_INIT(joinpart, plugin_query, plugin_load, plugin_unload);
