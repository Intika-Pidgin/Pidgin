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
#include <gmodule.h>
#include <gplugin.h>

#include <purple.h>

/* make the compiler happy... */
G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **error);
G_MODULE_EXPORT gboolean gplugin_load(GPluginPlugin *plugin, GError **error);
G_MODULE_EXPORT gboolean gplugin_unload(GPluginPlugin *plugin, GError **error);


static GApplication *
_purple_toast_get_application(void) {
	static GApplication *application = NULL;

	if(G_UNLIKELY(application == NULL)) {
		application = g_application_get_default();

		if(application == NULL) {
			application = g_application_new(NULL, G_APPLICATION_FLAGS_NONE);
			g_application_register(application, NULL, NULL);
		}
	}

	return application;
}

static void
_purple_toast_show_notification(const gchar *title,
                                const gchar *body,
                                GIcon *icon)
{
	GNotification *notification = g_notification_new(title);
	gchar *stripped = purple_markup_strip_html(body);

	g_notification_set_body(notification, stripped);
	g_free(stripped);

	if(G_IS_ICON(icon)) {
		g_notification_set_icon(notification, icon);
	}

	g_application_send_notification(_purple_toast_get_application(),
	                                NULL,
	                                notification);

	g_object_unref(G_OBJECT(notification));
}

static GIcon *
_purple_toast_find_icon(PurpleAccount *account,
                        PurpleBuddy *buddy,
                        const gchar *sender)
{
	GIcon *icon = NULL;

	if(PURPLE_IS_BUDDY(buddy)) {
		PurpleBuddyIcon *buddy_icon = purple_buddy_get_icon(buddy);

		if(buddy_icon) {
			const gchar *filename = NULL;

			filename = purple_buddy_icon_get_full_path(buddy_icon);
			if(filename != NULL) {
				GFile *file = g_file_new_for_path(filename);

				icon = g_file_icon_new(file);

				g_object_unref(file);
			}
		}
	} else {
		PurpleImage *image = NULL;
		const gchar *path = NULL;

		image = purple_buddy_icons_find_account_icon(account);
		if(PURPLE_IS_IMAGE(image)) {
			path = purple_image_get_path(image);

			if(path) {
				GFile *file = g_file_new_for_path(path);

				icon = g_file_icon_new(file);

				g_object_unref(G_OBJECT(file));
			}
			g_object_unref(G_OBJECT(image));
		}

		if(!G_IS_ICON(icon)) {
			/* fallback to something sane as we couldn't load a buddy icon */
		}
	}

	g_message("toast icon : %p", icon);

	return icon;
}

static void
_purple_toast_im_message_received(PurpleAccount *account,
                                  const gchar *sender,
                                  const gchar *message,
                                  PurpleConversation *conv,
                                  PurpleMessageFlags flags,
                                  gpointer data)
{
	PurpleBuddy *buddy = NULL;
	GIcon *icon = NULL;
	const gchar *title = NULL;

	buddy = purple_blist_find_buddy(account, sender);
	title = PURPLE_IS_BUDDY(buddy) ? purple_buddy_get_alias(buddy) : sender;
	icon = _purple_toast_find_icon(account, buddy, sender);

	_purple_toast_show_notification(title, message, icon);

	if(G_IS_ICON(icon)) {
		g_object_unref(G_OBJECT(icon));
	}
}

static void
_purple_toast_chat_message_received(PurpleAccount *account,
                                    gchar *sender,
                                    gchar *message,
                                    PurpleConversation *conv,
                                    PurpleMessageFlags flags,
                                    gpointer data)
{
	PurpleBuddy *buddy = NULL;
	GIcon *icon = NULL;
	gchar *title = NULL;
	const gchar *chat_name = NULL, *from = NULL;

	/* figure out the title */
	chat_name = purple_conversation_get_name(PURPLE_CONVERSATION(conv));
	if(chat_name) {
		PurpleChat *chat = purple_blist_find_chat(account, chat_name);

		if(chat) {
			chat_name = purple_chat_get_name(chat);
		}
	}

	from = sender;
	buddy = purple_blist_find_buddy(account, sender);
	if(PURPLE_IS_BUDDY(buddy)) {
		from = purple_buddy_get_alias(buddy);
	}

	title = g_strdup_printf("%s : %s", chat_name, from);

	/* figure out the icon */
	icon = _purple_toast_find_icon(account, buddy, sender);

	/* show the notification */
	_purple_toast_show_notification(title, message, icon);

	/* clean up our memory */
	g_free(title);
	g_object_unref(G_OBJECT(icon));
}

G_MODULE_EXPORT GPluginPluginInfo *
gplugin_query(GError **error) {
	PurplePluginInfo *info = NULL;

	const gchar * const authors[] = {
		"Gary Kramlich <grim@reaperworld.com>",
		NULL
	};

	info = purple_plugin_info_new(
		"id", "purple/toast",
		"abi-version", PURPLE_ABI_VERSION,
		"name", "Purple Toast",
		"version", "0.0.1",
		"authors", authors,
		NULL
	);

	return GPLUGIN_PLUGIN_INFO(info);
}

G_MODULE_EXPORT gboolean
gplugin_load(GPluginPlugin *plugin, GError **error) {
	gpointer conv_handle = purple_conversations_get_handle();

	purple_signal_connect(conv_handle,
	                      "received-im-msg",
	                      plugin,
	                      PURPLE_CALLBACK(_purple_toast_im_message_received),
	                      NULL
	);

	purple_signal_connect(conv_handle,
	                      "received-chat-msg",
	                      plugin,
	                      PURPLE_CALLBACK(_purple_toast_chat_message_received),
	                      NULL
	);

	return TRUE;
}

G_MODULE_EXPORT gboolean
gplugin_unload(GPluginPlugin *plugin, GError **error) {
	return TRUE;
}
