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

#include "internal.h"
#include "pidgin.h"

#include "account.h"
#include "conversation.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "glibcompat.h"
#include "log.h"
#include "network.h"
#include "notify.h"
#include "options.h"
#include "prefs.h"
#include "protocol.h"
#include "pounce.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"
#include "xfer.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkdocklet.h"
#include "gtkxfer.h"
#include "gtkidle.h"
#include "gtklog.h"
#include "gtkmedia.h"
#include "gtknotify.h"
#include "gtkplugin.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkrequest.h"
#include "gtkroomlist.h"
#include "gtksavedstatuses.h"
#include "gtksmiley-theme.h"
#include "gtksound.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "gtkwhiteboard.h"
#include "pidgindebug.h"

#ifndef _WIN32
#include <signal.h>
#endif

#ifndef _WIN32

/*
 * Lists of signals we wish to catch and those we wish to ignore.
 * Each list terminated with -1
 */
static const int catch_sig_list[] = {
	SIGSEGV,
	SIGINT,
	SIGTERM,
	SIGQUIT,
	SIGCHLD,
	-1
};

static const int ignore_sig_list[] = {
	SIGPIPE,
	-1
};
#endif /* !_WIN32 */

static void
dologin_named(const char *name)
{
	PurpleAccount *account;
	char **names;
	int i;

	if (name != NULL) { /* list of names given */
		names = g_strsplit(name, ",", 64);
		for (i = 0; names[i] != NULL; i++) {
			account = purple_accounts_find(names[i], NULL);
			if (account != NULL) { /* found a user */
				purple_account_set_enabled(account, PIDGIN_UI, TRUE);
			}
		}
		g_strfreev(names);
	} else { /* no name given, use the first account */
		GList *accounts;

		accounts = purple_accounts_get_all();
		if (accounts != NULL)
		{
			account = (PurpleAccount *)accounts->data;
			purple_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}
}

#ifndef _WIN32
static char *segfault_message;

static int signal_sockets[2];

static void sighandler(int sig);

static void sighandler(int sig)
{
	ssize_t written;

	/*
	 * We won't do any of the heavy lifting for the signal handling here
	 * because we have no idea what was interrupted.  Previously this signal
	 * handler could result in some calls to malloc/free, which can cause
	 * deadlock in libc when the signal handler was interrupting a previous
	 * malloc or free.  So instead we'll do an ugly hack where we write the
	 * signal number to one end of a socket pair.  The other half of the
	 * socket pair is watched by our main loop.  When the main loop sees new
	 * data on the socket it reads in the signal and performs the appropriate
	 * action without fear of interrupting stuff.
	 */
	if (sig == SIGSEGV) {
		fprintf(stderr, "%s", segfault_message);
		abort();
		return;
	}

	written = write(signal_sockets[0], &sig, sizeof(int));
	if (written < 0 || written != sizeof(int)) {
		/* This should never happen */
		purple_debug_error("sighandler", "Received signal %d but only "
				"wrote %" G_GSSIZE_FORMAT " bytes out of %"
				G_GSIZE_FORMAT ": %s\n",
				sig, written, sizeof(int), g_strerror(errno));
		exit(1);
	}
}

static gboolean
mainloop_sighandler(GIOChannel *source, GIOCondition cond, gpointer data)
{
	GIOStatus stat;
	int sig;
	gsize bytes_read;
	GError *error = NULL;

	/* read the signal number off of the io channel */
	stat = g_io_channel_read_chars(source, (gchar *)&sig, sizeof(int),
			&bytes_read, &error);
	if (stat != G_IO_STATUS_NORMAL) {
		purple_debug_error("sighandler", "Signal callback failed to read "
				"from signal socket: %s", error->message);
		purple_core_quit();
		return FALSE;
	}

	switch (sig) {
		case SIGCHLD:
			/* Restore signal catching */
			signal(SIGCHLD, sighandler);
			break;
		default:
			purple_debug_warning("sighandler", "Caught signal %d\n", sig);
			purple_core_quit();
	}

	return TRUE;
}
#endif /* !_WIN32 */

static int
ui_main(void)
{
#ifndef _WIN32
	GList *icons = NULL;
	GdkPixbuf *icon = NULL;
	char *icon_path;
	gsize i;
	struct {
		const char *dir;
		const char *filename;
	} icon_sizes[] = {
		{"16x16", "pidgin.png"},
		{"24x24", "pidgin.png"},
		{"32x32", "pidgin.png"},
		{"48x48", "pidgin.png"},
		{"scalable", "pidgin.svg"}
	};

#endif

	pidgin_blist_setup_sort_methods();

#ifndef _WIN32
	/* use the nice PNG icon for all the windows */
	for(i=0; i<G_N_ELEMENTS(icon_sizes); i++) {
		icon_path = g_build_filename(PURPLE_DATADIR, "icons", "hicolor",
			icon_sizes[i].dir, "apps", icon_sizes[i].filename, NULL);
		icon = pidgin_pixbuf_new_from_file(icon_path);
		g_free(icon_path);
		if (icon) {
			icons = g_list_append(icons,icon);
		} else {
			purple_debug_error("ui_main",
					"Failed to load the default window icon (%spx version)!\n", icon_sizes[i].dir);
		}
	}
	if(NULL == icons) {
		purple_debug_error("ui_main", "Unable to load any size of default window icon!\n");
	} else {
		gtk_window_set_default_icon_list(icons);

		g_list_foreach(icons, (GFunc)g_object_unref, NULL);
		g_list_free(icons);
	}
#endif

	return 0;
}

static void
debug_init(void)
{
	PidginDebugUi *ui = pidgin_debug_ui_new();
	purple_debug_set_ui(PURPLE_DEBUG_UI(ui));
}

static void
pidgin_ui_init(void)
{
	gchar *path;

	path = g_build_filename(PURPLE_DATADIR, "pidgin", "icons", NULL);
	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(), path);
	g_free(path);

	pidgin_stock_init();

	/* Set the UI operation structures. */
	purple_accounts_set_ui_ops(pidgin_accounts_get_ui_ops());
	purple_xfers_set_ui_ops(pidgin_xfers_get_ui_ops());
	purple_blist_set_ui_ops(pidgin_blist_get_ui_ops());
	purple_notify_set_ui_ops(pidgin_notify_get_ui_ops());
	purple_request_set_ui_ops(pidgin_request_get_ui_ops());
	purple_sound_set_ui_ops(pidgin_sound_get_ui_ops());
	purple_connections_set_ui_ops(pidgin_connections_get_ui_ops());
	purple_whiteboard_set_ui_ops(pidgin_whiteboard_get_ui_ops());
	purple_idle_set_ui_ops(pidgin_idle_get_ui_ops());

	pidgin_accounts_init();
	pidgin_connection_init();
	pidgin_request_init();
	pidgin_blist_init();
	pidgin_status_init();
	pidgin_conversations_init();
	pidgin_pounces_init();
	pidgin_privacy_init();
	pidgin_xfers_init();
	pidgin_roomlist_init();
	pidgin_log_init();
	pidgin_docklet_init();
	_pidgin_smiley_theme_init();
	pidgin_utils_init();
	pidgin_medias_init();
	pidgin_notify_init();
}

static GHashTable *ui_info = NULL;

static void
pidgin_quit(void)
{
	/* Uninit */
	PurpleDebugUi *ui;

	pidgin_utils_uninit();
	pidgin_notify_uninit();
	_pidgin_smiley_theme_uninit();
	pidgin_conversations_uninit();
	pidgin_status_uninit();
	pidgin_docklet_uninit();
	pidgin_blist_uninit();
	pidgin_request_uninit();
	pidgin_connection_uninit();
	pidgin_accounts_uninit();
	pidgin_xfers_uninit();
	ui = purple_debug_get_ui();
	purple_debug_set_ui(NULL);
	g_object_unref(ui);

	if(NULL != ui_info)
		g_hash_table_destroy(ui_info);

	/* and end it all... */
	g_application_quit(g_application_get_default());
}

static GHashTable *pidgin_ui_get_info(void)
{
	if(NULL == ui_info) {
		ui_info = g_hash_table_new(g_str_hash, g_str_equal);

		g_hash_table_insert(ui_info, "name", (char*)PIDGIN_NAME);
		g_hash_table_insert(ui_info, "version", VERSION);
		g_hash_table_insert(ui_info, "website", "https://pidgin.im");
		g_hash_table_insert(ui_info, "dev_website", "https://developer.pidgin.im");
		g_hash_table_insert(ui_info, "client_type", "pc");

		/*
		 * prpl-aim-clientkey is a DevID (or "client key") for Pidgin, given to
		 * us by AOL in September 2016.  prpl-icq-clientkey is also a client key
		 * for Pidgin, owned by the AIM account "markdoliner."  Please don't use 
		 * either for other applications.  Instead, you can either not specify a 
		 * client key, in which case the default "libpurple" key will be used,
		 * or you can try to register your own at the AIM or ICQ web sites
		 * (although this functionality was removed at some point, it's possible 
		 * it has been re-added).
		 */
		g_hash_table_insert(ui_info, "prpl-aim-clientkey", "do1UCeb5gNqxB1S1");
		g_hash_table_insert(ui_info, "prpl-icq-clientkey", "ma1cSASNCKFtrdv9");

		/*
		 * prpl-aim-distid is a distID for Pidgin, given to us by AOL in
		 * September 2016.  prpl-icq-distid is also a distID for Pidgin, given
		 * to us by AOL.  Please don't use either for other applications.
		 * Instead, you can just not specify a distID and libpurple will use a
		 * default.
		 */
		g_hash_table_insert(ui_info, "prpl-aim-distid", GINT_TO_POINTER(1715));
		g_hash_table_insert(ui_info, "prpl-icq-distid", GINT_TO_POINTER(1550));
	}

	return ui_info;
}

static PurpleCoreUiOps core_ops =
{
	pidgin_prefs_init,
	debug_init,
	pidgin_ui_init,
	pidgin_quit,
	pidgin_ui_get_info,
	NULL,
	NULL,
	NULL,
	NULL
};

static PurpleCoreUiOps *
pidgin_core_get_ui_ops(void)
{
	return &core_ops;
}

static void
pidgin_activate_cb(GApplication *application, gpointer user_data)
{
	purple_blist_set_visible(TRUE);
}

static gboolean opt_login = FALSE;
static gchar *opt_login_arg = NULL;

static gboolean
login_opt_arg_func(const gchar *option_name, const gchar *value,
		gpointer data, GError **error)
{
	opt_login = TRUE;

	g_free(opt_login_arg);
	opt_login_arg = g_strdup(value);

	return TRUE;
}

int pidgin_start(int argc, char *argv[])
{
	GApplication *app;
	gboolean opt_nologin = FALSE;
	gboolean opt_version = FALSE;
	gboolean opt_si = TRUE;     /* Check for single instance? */
	char *opt_config_dir_arg = NULL;
	char *search_path;
	GtkCssProvider *provider;
	GdkScreen *screen;
	GList *accounts;
#ifndef _WIN32
	int sig_indx;	/* for setting up signal catching */
	sigset_t sigset;
	char errmsg[BUFSIZ];
	GIOChannel *signal_channel;
	GIOStatus signal_status;
	guint signal_channel_watcher;
#ifndef DEBUG
	char *segfault_message_tmp;
#endif /* DEBUG */
#endif /* !_WIN32 */
	GOptionContext *context;
	gchar *summary;
	gchar **args;
	gboolean gui_check;
	GList *active_accounts;
	GStatBuf st;
	GError *error = NULL;
	int ret;

	GOptionEntry option_entries[] = {
		{"config", 'c', 0,
			G_OPTION_ARG_FILENAME, &opt_config_dir_arg,
			_("use DIR for config files"), _("DIR")},
		{"login", 'l', G_OPTION_FLAG_OPTIONAL_ARG,
			G_OPTION_ARG_CALLBACK, &login_opt_arg_func,
			_("enable specified account(s) (optional argument NAME\n"
			  "                            "
			  "specifies account(s) to use, separated by commas.\n"
			  "                            "
			  "Without this only the first account will be enabled)"),
			_("[NAME]")},
		{"multiple", 'm', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE,  &opt_si,
			_("allow multiple instances"), NULL},
		{"nologin", 'n', 0,
			G_OPTION_ARG_NONE, &opt_nologin,
			_("don't automatically login"), NULL},
		{"version", 'v', 0,
			G_OPTION_ARG_NONE, &opt_version,
			_("display the current version and exit"), NULL},
		{NULL}
	};

#ifdef DEBUG
	purple_debug_set_enabled(TRUE);
#endif

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, PURPLE_LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
#endif

	/* Locale initialization is not complete here.  See gtk_init_check() */
	setlocale(LC_ALL, "");

#ifndef _WIN32

#ifndef DEBUG
		/* We translate this here in case the crash breaks gettext. */
		segfault_message_tmp = g_strdup_printf(_(
			"%s %s has segfaulted and attempted to dump a core file.\n"
			"This is a bug in the software and has happened through\n"
			"no fault of your own.\n\n"
			"If you can reproduce the crash, please notify the developers\n"
			"by reporting a bug at:\n"
			"%ssimpleticket/\n\n"
			"Please make sure to specify what you were doing at the time\n"
			"and post the backtrace from the core file.  If you do not know\n"
			"how to get the backtrace, please read the instructions at\n"
			"%swiki/GetABacktrace\n"),
			PIDGIN_NAME, DISPLAY_VERSION, PURPLE_DEVEL_WEBSITE, PURPLE_DEVEL_WEBSITE
		);

		/* we have to convert the message (UTF-8 to console
		   charset) early because after a segmentation fault
		   it's not a good practice to allocate memory */
		segfault_message = g_locale_from_utf8(segfault_message_tmp,
						      -1, NULL, NULL, &error);
		if (segfault_message != NULL) {
			g_free(segfault_message_tmp);
		}
		else {
			/* use 'segfault_message_tmp' (UTF-8) as a fallback */
			g_warning("%s\n", error->message);
			g_clear_error(&error);
			segfault_message = segfault_message_tmp;
		}
#else
		/* Don't mark this for translation. */
		segfault_message = g_strdup(
			"Hi, user.  We need to talk.\n"
			"I think something's gone wrong here.  It's probably my fault.\n"
			"No, really, it's not you... it's me... no no no, I think we get along well\n"
			"it's just that.... well, I want to see other people.  I... what?!?  NO!  I \n"
			"haven't been cheating on you!!  How many times do you want me to tell you?!  And\n"
			"for the last time, it's just a rash!\n"
		);
#endif

	/*
	 * Create a socket pair for receiving unix signals from a signal
	 * handler.
	 */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, signal_sockets) < 0) {
		perror("Failed to create sockets for GLib signal handling");
		exit(1);
	}
	signal_channel = g_io_channel_unix_new(signal_sockets[1]);

	/*
	 * Set the channel encoding to raw binary instead of the default of
	 * UTF-8, because we'll be sending integers across instead of strings.
	 */
	signal_status = g_io_channel_set_encoding(signal_channel, NULL, &error);
	if (signal_status != G_IO_STATUS_NORMAL) {
		fprintf(stderr, "Failed to set the signal channel to raw "
				"binary: %s", error->message);
		g_clear_error(&error);
		exit(1);
	}
	signal_channel_watcher = g_io_add_watch(signal_channel, G_IO_IN, mainloop_sighandler, NULL);
	g_io_channel_unref(signal_channel);

	/* Let's not violate any PLA's!!!! */
	/* jseymour: whatever the fsck that means */
	/* Robot101: for some reason things like gdm like to block     *
	 * useful signals like SIGCHLD, so we unblock all the ones we  *
	 * declare a handler for. thanks JSeymour and Vann.            */
	if (sigemptyset(&sigset)) {
		snprintf(errmsg, sizeof(errmsg), "Warning: couldn't initialise empty signal set");
		perror(errmsg);
	}
	for(sig_indx = 0; catch_sig_list[sig_indx] != -1; ++sig_indx) {
		if(signal(catch_sig_list[sig_indx], sighandler) == SIG_ERR) {
			snprintf(errmsg, sizeof(errmsg), "Warning: couldn't set signal %d for catching",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
		if(sigaddset(&sigset, catch_sig_list[sig_indx])) {
			snprintf(errmsg, sizeof(errmsg), "Warning: couldn't include signal %d for unblocking",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
	}
	for(sig_indx = 0; ignore_sig_list[sig_indx] != -1; ++sig_indx) {
		if(signal(ignore_sig_list[sig_indx], SIG_IGN) == SIG_ERR) {
			snprintf(errmsg, sizeof(errmsg), "Warning: couldn't set signal %d to ignore",
				ignore_sig_list[sig_indx]);
			perror(errmsg);
		}
	}

	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		snprintf(errmsg, sizeof(errmsg), "Warning: couldn't unblock signals");
		perror(errmsg);
	}
#endif /* !_WIN32 */

	context = g_option_context_new(NULL);

	summary = g_strdup_printf("%s %s", PIDGIN_NAME, DISPLAY_VERSION);
	g_option_context_set_summary(context, summary);
	g_free(summary);

	g_option_context_add_main_entries(context, option_entries, PACKAGE);
	g_option_context_add_group(context, purple_get_option_group());
#ifdef PURPLE_PLUGINS
	g_option_context_add_group(context, gplugin_get_option_group());
#endif
	g_option_context_add_group(context, gtk_get_option_group(TRUE));

#ifdef G_OS_WIN32
	/* Handle Unicode filenames on Windows. See GOptionContext docs. */
	args = g_win32_get_command_line();
#else
	args = g_strdupv(argv);
#endif

	if (!g_option_context_parse_strv(context, &args, &error)) {
		g_strfreev(args);
		g_printerr(_("%s %s: %s\nTry `%s -h' for more information.\n"),
				PIDGIN_NAME, DISPLAY_VERSION, error->message,
				argv[0]);
		g_clear_error(&error);
#ifndef _WIN32
		g_free(segfault_message);
#endif
		return 1;
	}

	g_strfreev(args);

	/* show version message */
	if (opt_version) {
		printf("%s %s (libpurple %s)\n", PIDGIN_NAME, DISPLAY_VERSION,
		                                 purple_core_get_version());
#ifndef _WIN32
		g_free(segfault_message);
#endif
		return 0;
	}

	/* set a user-specified config directory */
	if (opt_config_dir_arg != NULL) {
		if (g_path_is_absolute(opt_config_dir_arg)) {
			purple_util_set_user_dir(opt_config_dir_arg);
		} else {
			/* Make an absolute (if not canonical) path */
			char *cwd = g_get_current_dir();
			char *path = g_build_path(G_DIR_SEPARATOR_S, cwd, opt_config_dir_arg, NULL);
			purple_util_set_user_dir(path);
			g_free(path);
			g_free(cwd);
		}
	}

	/*
	 * We're done piddling around with command line arguments.
	 * Fire up this baby.
	 */

	app = G_APPLICATION(gtk_application_new("im.pidgin.Pidgin",
				G_APPLICATION_NON_UNIQUE));

	g_object_set(app, "register-session", TRUE, NULL);

	g_signal_connect(app, "activate",
			G_CALLBACK(pidgin_activate_cb), NULL);

	if (!g_application_register(app, NULL, &error)) {
		purple_debug_error("gtk",
				"Unable to register GApplication: %s\n",
				error->message);
		g_clear_error(&error);
		g_object_unref(app);
#ifndef _WIN32
		g_free(segfault_message);
#endif
		return 1;
	}

	search_path = g_build_filename(purple_user_dir(), "gtk-3.0.css", NULL);

	provider = gtk_css_provider_new();
	gui_check = gtk_css_provider_load_from_path(provider, search_path, &error);

	if (gui_check && !error) {
		screen = gdk_screen_get_default();
		gtk_style_context_add_provider_for_screen(screen,
		                                          GTK_STYLE_PROVIDER(provider),
		                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
	} else {
		purple_debug_error("gtk", "Unable to load custom gtk-3.0.css: %s\n",
		                   error ? error->message : "(unknown error)");
		g_clear_error(&error);
	}

	g_free(search_path);

#ifdef _WIN32
	winpidgin_init();
#endif

	purple_core_set_ui_ops(pidgin_core_get_ui_ops());

	if (!purple_core_init(PIDGIN_UI)) {
		fprintf(stderr,
				"Initialization of the libpurple core failed. Dumping core.\n"
				"Please report this!\n");
#ifndef _WIN32
		g_free(segfault_message);
#endif
		abort();
	}

	if (!g_getenv("PURPLE_PLUGINS_SKIP")) {
		search_path = g_build_filename(purple_user_dir(),
				"plugins", NULL);
		if (!g_stat(search_path, &st))
			g_mkdir(search_path, S_IRUSR | S_IWUSR | S_IXUSR);
		purple_plugins_add_search_path(search_path);
		g_free(search_path);

		purple_plugins_add_search_path(PIDGIN_LIBDIR);
	} else {
		purple_debug_info("gtk",
				"PURPLE_PLUGINS_SKIP environment variable "
				"set, skipping normal Pidgin plugin paths");
	}

	purple_plugins_refresh();

	if (opt_si && !purple_core_ensure_single_instance()) {
#ifdef HAVE_DBUS
		DBusConnection *conn = purple_dbus_get_connection();
		DBusMessage *message = dbus_message_new_method_call(PURPLE_DBUS_SERVICE, PURPLE_DBUS_PATH,
				PURPLE_DBUS_INTERFACE, "PurpleBlistSetVisible");
		gboolean tr = TRUE;
		dbus_message_append_args(message, DBUS_TYPE_INT32, &tr, DBUS_TYPE_INVALID);
		dbus_connection_send_with_reply_and_block(conn, message, -1, NULL);
		dbus_message_unref(message);
#endif
		gdk_notify_startup_complete();
		purple_core_quit();
		g_printerr(_("Exiting because another libpurple client is already running.\n"));
#ifndef _WIN32
		g_free(segfault_message);
#endif
		return 0;
	}

	/* load plugins we had when we quit */
	purple_plugins_load_saved(PIDGIN_PREFS_ROOT "/plugins/loaded");

	ui_main();

	g_free(opt_config_dir_arg);
	opt_config_dir_arg = NULL;

	/*
	 * We want to show the blist early in the init process so the
	 * user feels warm and fuzzy (not cold and prickley).
	 */
	purple_blist_show();

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled"))
		pidgin_debug_window_show();

	if (opt_login) {
		/* disable all accounts */
		for (accounts = purple_accounts_get_all(); accounts != NULL; accounts = accounts->next) {
			PurpleAccount *account = accounts->data;
			purple_account_set_enabled(account, PIDGIN_UI, FALSE);
		}
		/* honor the startup status preference */
		if (!purple_prefs_get_bool("/purple/savedstatus/startup_current_status"))
			purple_savedstatus_activate(purple_savedstatus_get_startup());
		/* now enable the requested ones */
		dologin_named(opt_login_arg);
		g_free(opt_login_arg);
		opt_login_arg = NULL;
	} else if (opt_nologin)	{
		/* Set all accounts to "offline" */
		PurpleSavedStatus *saved_status;

		/* If we've used this type+message before, lookup the transient status */
		saved_status = purple_savedstatus_find_transient_by_type_and_message(
							PURPLE_STATUS_OFFLINE, NULL);

		/* If this type+message is unique then create a new transient saved status */
		if (saved_status == NULL)
			saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_OFFLINE);

		/* Set the status for each account */
		purple_savedstatus_activate(saved_status);
	} else {
		/* Everything is good to go--sign on already */
		if (!purple_prefs_get_bool("/purple/savedstatus/startup_current_status"))
			purple_savedstatus_activate(purple_savedstatus_get_startup());
		purple_accounts_restore_current_statuses();
	}

	if ((active_accounts = purple_accounts_get_all_active()) == NULL)
	{
		pidgin_accounts_window_show();
	}
	else
	{
		g_list_free(active_accounts);
	}

	/* GTK clears the notification for us when opening the first window,
	 * but we may have launched with only a status icon, so clear the it
	 * just in case. */
	gdk_notify_startup_complete();

#ifdef _WIN32
	winpidgin_post_init();
#endif

	/* TODO: Use GtkApplicationWindow or add a window instead */
	g_application_hold(app);

	ret = g_application_run(app, 0, NULL);

	/* Make sure purple has quit in case something in GApplication
	 * has caused g_application_run() to finish on its own. This can
	 * happen, for example, if the desktop session is ending.
	 */
	if (purple_get_core() != NULL) {
		purple_core_quit();
	}

	/* Now that we're sure purple_core_quit() has been called,
	 * this can be freed.
	 */
	g_object_unref(app);

#ifndef _WIN32
	g_free(segfault_message);
	g_source_remove(signal_channel_watcher);
	close(signal_sockets[0]);
	close(signal_sockets[1]);
#endif

#ifdef _WIN32
	winpidgin_cleanup();
#endif

	return ret;
}