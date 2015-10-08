/*
 * pidgin
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

#include "purple.h"

#include <glib.h>
#include <glib/gprintf.h>

#include <signal.h>
#include <string.h>
#ifdef _WIN32
#  include <conio.h>
#  include "win32/win32dep.h"
#else
#  include <unistd.h>
#endif

#include "defines.h"

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PurpleGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} PurpleGLibIOClosure;

static void purple_glib_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleGLibIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
		purple_cond);

	return TRUE;
}

static guint glib_input_add(gint fd, PurpleInputCondition condition,
	PurpleInputFunction function, gpointer data)
{
	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;

#if defined _WIN32 && !defined WINPIDGIN_USE_GLIB_IO_CHANNEL
	channel = wpurple_g_io_channel_win32_new_socket(fd);
#else
	channel = g_io_channel_unix_new(fd);
#endif
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT,
		cond, purple_glib_io_invoke, closure, purple_glib_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static PurpleEventLoopUiOps glib_eventloops =
{
	g_timeout_add,
	g_source_remove,
	glib_input_add,
	g_source_remove,
	NULL,
	g_timeout_add_seconds,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};
/*** End of the eventloop functions. ***/

/*** Conversation uiops ***/
static void
null_write_conv(PurpleConversation *conv, PurpleMessage *msg)
{
	time_t mtime = purple_message_get_time(msg);

	printf("(%s) %s %s: %s\n",
		purple_conversation_get_name(conv),
		purple_utf8_strftime("(%H:%M:%S)", localtime(&mtime)),
		purple_message_get_author_alias(msg),
		purple_message_get_contents(msg));
}

static PurpleConversationUiOps null_conv_uiops =
{
	NULL,                      /* create_conversation  */
	NULL,                      /* destroy_conversation */
	NULL,                      /* write_chat           */
	NULL,                      /* write_im             */
	null_write_conv,           /* write_conv           */
	NULL,                      /* chat_add_users       */
	NULL,                      /* chat_rename_user     */
	NULL,                      /* chat_remove_users    */
	NULL,                      /* chat_update_user     */
	NULL,                      /* present              */
	NULL,                      /* has_focus            */
	NULL,                      /* send_confirm         */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
null_ui_init(void)
{
	/**
	 * This should initialize the UI components for all the modules. Here we
	 * just initialize the UI for conversations.
	 */
	purple_conversations_set_ui_ops(&null_conv_uiops);
}

static PurpleCoreUiOps null_core_uiops =
{
	NULL,
	NULL,
	null_ui_init,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_libpurple(void)
{
	/* Set a custom user directory (optional) */
	purple_util_set_user_dir(CUSTOM_USER_DIRECTORY);

	/* We do not want any debugging for now to keep the noise to a minimum. */
	purple_debug_set_enabled(FALSE);

	/* Set the core-uiops, which is used to
	 * 	- initialize the ui specific preferences.
	 * 	- initialize the debug ui.
	 * 	- initialize the ui components for all the modules.
	 * 	- uninitialize the ui components for all the modules when the core terminates.
	 */
	purple_core_set_ui_ops(&null_core_uiops);

	/* Set the uiops for the eventloop. If your client is glib-based, you can safely
	 * copy this verbatim. */
	purple_eventloop_set_ui_ops(&glib_eventloops);

	/* Now that all the essential stuff has been set, let's try to init the core. It's
	 * necessary to provide a non-NULL name for the current ui to the core. This name
	 * is used by stuff that depends on this ui, for example the ui-specific plugins. */
	if (!purple_core_init(UI_ID)) {
		/* Initializing the core failed. Terminate. */
		fprintf(stderr,
				"libpurple initialization failed. Dumping core.\n"
				"Please report this!\n");
		abort();
	}

	/* Set path to search for plugins. The core (libpurple) takes care of loading the
	 * core-plugins, which includes the in-tree protocols. So it is not essential to add
	 * any path here, but it might be desired, especially for ui-specific plugins. */
	purple_plugins_add_search_path(CUSTOM_PLUGIN_PATH);
	purple_plugins_refresh();

	/* Load the preferences. */
	purple_prefs_load();

	/* Load the desired plugins. The client should save the list of loaded plugins in
	 * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
	purple_plugins_load_saved(PLUGIN_SAVE_PREF);
}

static void
signed_on(PurpleConnection *gc, gpointer null)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	printf("Account connected: %s %s\n", purple_account_get_username(account), purple_account_get_protocol_id(account));
}

static void
connect_to_signals_for_demonstration_purposes_only(void)
{
	static int handle;
	purple_signal_connect(purple_connections_get_handle(), "signed-on", &handle,
				PURPLE_CALLBACK(signed_on), NULL);
}

#if defined(_WIN32) || defined(__BIONIC__)
#ifndef PASS_MAX
#  define PASS_MAX 1024
#endif
static gchar *
getpass(const gchar *prompt)
{
	static gchar buff[PASS_MAX + 1];
	guint i = 0;

	g_fprintf(stderr, "%s", prompt);
	fflush(stderr);

	while (i < sizeof(buff) - 1) {
#ifdef __BIONIC__
		buff[i] = getc(stdin);
#else
		buff[i] = _getch();
#endif
		if (buff[i] == '\r' || buff[i] == '\n')
			break;
		i++;
	}
	buff[i] = '\0';
	g_fprintf(stderr, "\n");

	return buff;
}
#endif /* _WIN32 || __BIONIC__ */

int main(int argc, char *argv[])
{
	GList *list, *iter;
	int i, num;
	GList *names = NULL;
	const char *protocol = NULL;
	char name[128];
	char *password;
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	PurpleAccount *account;
	PurpleSavedStatus *status;
	char *res;

#ifndef _WIN32
	/* libpurple's built-in DNS resolution forks processes to perform
	 * blocking lookups without blocking the main process.  It does not
	 * handle SIGCHLD itself, so if the UI does not you quickly get an army
	 * of zombie subprocesses marching around.
	 */
	signal(SIGCHLD, SIG_IGN);
#endif

	init_libpurple();

	printf("libpurple initialized.\n");

	list = purple_protocols_get_all();
	for (i = 0, iter = list; iter; iter = iter->next) {
		PurpleProtocol *protocol = iter->data;
		if (protocol && purple_protocol_get_name(protocol)) {
			printf("\t%d: %s\n", i++, purple_protocol_get_name(protocol));
			names = g_list_append(names, (gpointer)purple_protocol_get_id(protocol));
		}
	}
	g_list_free(list);

	printf("Select the protocol [0-%d]: ", i-1);
	res = fgets(name, sizeof(name), stdin);
	if (!res) {
		fprintf(stderr, "Failed to gets protocol selection.");
		abort();
	}
	if (sscanf(name, "%d", &num) == 1)
		protocol = g_list_nth_data(names, num);
	if (!protocol) {
		fprintf(stderr, "Failed to gets protocol.");
		abort();
	}

	printf("Username: ");
	res = fgets(name, sizeof(name), stdin);
	if (!res) {
		fprintf(stderr, "Failed to read user name.");
		abort();
	}
	name[strlen(name) - 1] = 0;  /* strip the \n at the end */

	/* Create the account */
	account = purple_account_new(name, protocol);

	/* Get the password for the account */
	password = getpass("Password: ");
	purple_account_set_password(account, password, NULL, NULL);

	/* It's necessary to enable the account first. */
	purple_account_set_enabled(account, UI_ID, TRUE);

	/* Now, to connect the account(s), create a status and activate it. */
	status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
	purple_savedstatus_activate(status);

	connect_to_signals_for_demonstration_purposes_only();

	g_main_loop_run(loop);

	return 0;
}
