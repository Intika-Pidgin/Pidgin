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
#include "cmds.h"
#include "connection.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "xfer.h"
#include "glibcompat.h"
#include "http.h"
#include "idle.h"
#include "image-store.h"
#include "keyring.h"
#include "message.h"
#include "network.h"
#include "notify.h"
#include "plugins.h"
#include "pounce.h"
#include "prefs.h"
#include "proxy.h"
#include "savedstatuses.h"
#include "signals.h"
#include "smiley-custom.h"
#include "smiley-parser.h"
#include "smiley-theme.h"
#include "sound.h"
#include "sound-theme-loader.h"
#include "sslconn.h"
#include "status.h"
#include "stun.h"
#include "theme-manager.h"
#include "util.h"

struct PurpleCore
{
	char *ui;

	void *reserved;
};

static PurpleCoreUiOps *_ops  = NULL;
static PurpleCore      *_core = NULL;

static void
purple_core_print_version(void)
{
	GHashTable *ui_info = purple_core_get_ui_info();
	const gchar *ui_name;
	const gchar *ui_version;
	gchar *ui_full_name = NULL;

	ui_name = ui_info ? g_hash_table_lookup(ui_info, "name") : NULL;
	ui_version = ui_info ? g_hash_table_lookup(ui_info, "version") : NULL;

	if (ui_name) {
		ui_full_name = g_strdup_printf("%s%s%s", ui_name,
			ui_version ? " " : "", ui_version);
	}

	purple_debug_info("main", "Launching %s%slibpurple %s",
		ui_full_name ? ui_full_name : "",
		ui_full_name ? " with " : "",
		purple_core_get_version());

}

gboolean
purple_core_init(const char *ui)
{
	PurpleCoreUiOps *ops;
	PurpleCore *core;

	g_return_val_if_fail(ui != NULL, FALSE);
	g_return_val_if_fail(purple_get_core() == NULL, FALSE);

	bindtextdomain(PACKAGE, PURPLE_LOCALEDIR);

#ifdef _WIN32
	wpurple_init();
#endif

	_core = core = g_new0(PurpleCore, 1);
	core->ui = g_strdup(ui);
	core->reserved = NULL;

	ops = purple_core_get_ui_ops();

	/* The signals subsystem is important and should be first. */
	purple_signals_init();

	purple_util_init();

	purple_signal_register(core, "uri-handler",
		purple_marshal_BOOLEAN__POINTER_POINTER_POINTER,
		G_TYPE_BOOLEAN, 3,
		G_TYPE_STRING, /* Protocol */
		G_TYPE_STRING, /* Command */
		G_TYPE_POINTER); /* Parameters (GHashTable *) */

	purple_signal_register(core, "quitting", purple_marshal_VOID, G_TYPE_NONE,
		0);
	purple_signal_register(core, "core-initialized", purple_marshal_VOID,
		G_TYPE_NONE, 0);

	purple_core_print_version();

	/* The prefs subsystem needs to be initialized before static protocols
	 * for protocol prefs to work. */
	purple_prefs_init();

	purple_debug_init();

	if (ops != NULL)
	{
		if (ops->ui_prefs_init != NULL)
			ops->ui_prefs_init();

		if (ops->debug_ui_init != NULL)
			ops->debug_ui_init();
	}

	purple_cmds_init();
	purple_protocols_init();

	/* Since plugins get probed so early we should probably initialize their
	 * subsystem right away too.
	 */
	purple_plugins_init();

	purple_keyring_init(); /* before accounts */
	purple_theme_manager_init();

	/* The buddy icon code uses the image store, so init it early. */
	_purple_image_store_init();

	/* Accounts use status, buddy icons and connection signals, so
	 * initialize these before accounts
	 */
	purple_statuses_init();
	purple_buddy_icons_init();
	purple_connections_init();

	purple_accounts_init();
	purple_savedstatuses_init();
	purple_notify_init();
	_purple_message_init();
	purple_conversations_init();
	purple_blist_init();
	purple_log_init();
	purple_network_init();
	purple_pounces_init();
	purple_proxy_init();
	purple_sound_init();
	purple_stun_init();
	purple_xfers_init();
	purple_idle_init();
	purple_http_init();
	_purple_smiley_custom_init();
	_purple_smiley_parser_init();

	/*
	 * Call this early on to try to auto-detect our IP address and
	 * hopefully save some time later.
	 */
	purple_network_get_my_ip(-1);

	if (ops != NULL && ops->ui_init != NULL)
		ops->ui_init();

	/* The UI may have registered some theme types, so refresh them */
	purple_theme_manager_refresh();

	/* Load the buddy list after UI init */
	purple_blist_boot();

	purple_signal_emit(purple_get_core(), "core-initialized");

	return TRUE;
}

void
purple_core_quit(void)
{
	PurpleCoreUiOps *ops;
	PurpleCore *core = purple_get_core();

	g_return_if_fail(core != NULL);

	/* The self destruct sequence has been initiated */
	purple_signal_emit(purple_get_core(), "quitting");

	/* Transmission ends */
	purple_connections_disconnect_all();

	/* Save .xml files, remove signals, etc. */
	_purple_smiley_theme_uninit();
	_purple_smiley_custom_uninit();
	_purple_smiley_parser_uninit();
	purple_http_uninit();
	purple_idle_uninit();
	purple_pounces_uninit();
	purple_conversations_uninit();
	purple_blist_uninit();
	purple_notify_uninit();
	purple_connections_uninit();
	purple_buddy_icons_uninit();
	purple_savedstatuses_uninit();
	purple_statuses_uninit();
	purple_accounts_uninit();
	purple_keyring_uninit(); /* after accounts */
	purple_sound_uninit();
	purple_theme_manager_uninit();
	purple_xfers_uninit();
	purple_proxy_uninit();
	_purple_image_store_uninit();
	purple_network_uninit();

	ops = purple_core_get_ui_ops();
	if (ops != NULL && ops->quit != NULL)
		ops->quit();

	/* Everything after prefs_uninit must not try to read any prefs */
	purple_prefs_uninit();
	purple_plugins_uninit();

	purple_protocols_uninit();

	purple_cmds_uninit();
	purple_log_uninit();
	_purple_message_uninit();
	/* Everything after util_uninit cannot try to write things to the
	 * confdir nor use purple_escape_js
	 */
	purple_util_uninit();

	purple_signals_uninit();

	g_free(core->ui);
	g_free(core);

#ifdef _WIN32
	wpurple_cleanup();
#endif

	_core = NULL;
}

gboolean
purple_core_quit_cb(gpointer unused)
{
	purple_core_quit();

	return FALSE;
}

const char *
purple_core_get_version(void)
{
	return VERSION;
}

const char *
purple_core_get_ui(void)
{
	PurpleCore *core = purple_get_core();

	g_return_val_if_fail(core != NULL, NULL);

	return core->ui;
}

PurpleCore *
purple_get_core(void)
{
	return _core;
}

static PurpleCoreUiOps *
purple_core_ui_ops_copy(PurpleCoreUiOps *ops)
{
	PurpleCoreUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleCoreUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_core_ui_ops_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleCoreUiOps",
				(GBoxedCopyFunc)purple_core_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

void
purple_core_set_ui_ops(PurpleCoreUiOps *ops)
{
	_ops = ops;
}

PurpleCoreUiOps *
purple_core_get_ui_ops(void)
{
	return _ops;
}

GHashTable* purple_core_get_ui_info() {
	PurpleCoreUiOps *ops = purple_core_get_ui_ops();

	if(NULL == ops || NULL == ops->get_ui_info)
		return NULL;

	return ops->get_ui_info();
}

#define MIGRATE_TO_XDG_DIR(xdg_base_dir, legacy_path) \
	G_STMT_START { \
		gboolean migrate_res; \
		\
		migrate_res = purple_move_to_xdg_base_dir(xdg_base_dir, legacy_path); \
		if (!migrate_res) { \
			purple_debug_error("core", "Error migrating %s to %s\n", \
						legacy_path, xdg_base_dir); \
			return FALSE; \
		} \
	} G_STMT_END

gboolean
purple_core_migrate_to_xdg_base_dirs(void)
{
	gboolean xdg_dir_exists;

	xdg_dir_exists = g_file_test(purple_data_dir(), G_FILE_TEST_EXISTS);
	if (!xdg_dir_exists) {
		MIGRATE_TO_XDG_DIR(purple_data_dir(), "certificates");
		MIGRATE_TO_XDG_DIR(purple_data_dir(), "logs");
		MIGRATE_TO_XDG_DIR(purple_config_dir(), "pounces.xml");
	}

	return TRUE;
}
