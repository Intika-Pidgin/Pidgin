/**
 * @file internalkeyring.c internal keyring
 * @ingroup plugins
 *
 * @todo 
 *   cleanup error handling and reporting
 */

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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef PURPLE_PLUGINS
# define PURPLE_PLUGINS
#endif

#include <glib.h>

#ifndef G_GNUC_NULL_TERMINATED
# if __GNUC__ >= 4
#  define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define G_GNUC_NULL_TERMINATED
# endif
#endif

#include "account.h"
#include "version.h"
#include "keyring.h"
#include "debug.h"
#include "plugin.h"

#define INTERNALKEYRING_NAME		"Internal keyring"
#define INTERNALKEYRING_VERSION		"0.7"
#define INTERNALKEYRING_DESCRIPTION	"This plugin provides the default password storage behaviour for libpurple."
#define	INTERNALKEYRING_AUTHOR		"Scrouaf (scrouaf[at]soc.pidgin.im)"
#define INTERNALKEYRING_ID		FALLBACK_KEYRING


#define GET_PASSWORD(account) \
	g_hash_table_lookup (internal_keyring_passwords, account)
#define SET_PASSWORD(account, password) \
	g_hash_table_replace(internal_keyring_passwords, account, password)
#define ACTIVATE()\
	if (internal_keyring_is_active == FALSE)\
		internal_keyring_open();	
	

static GHashTable * internal_keyring_passwords = NULL;
static PurpleKeyring * keyring_handler = NULL;
static gboolean internal_keyring_is_active = FALSE;

/* a few prototypes : */
static void 		internal_keyring_read(PurpleAccount *, PurpleKeyringReadCallback, gpointer);
static void 		internal_keyring_save(PurpleAccount *, gchar *, GDestroyNotify, PurpleKeyringSaveCallback, gpointer);
static const char * 	internal_keyring_read_sync(const PurpleAccount *);
static void 		internal_keyring_save_sync(PurpleAccount *, const gchar *);
static void		internal_keyring_close(GError **);
static void		internal_keyring_open(void);
static gboolean		internal_keyring_import_password(PurpleAccount *, const char *, const char *, GError **);
static gboolean 	internal_keyring_export_password(PurpleAccount *, const char **, char **, GError **, GDestroyNotify *);
static void		internal_keyring_init(void);
static void		internal_keyring_uninit(void);
static gboolean		internal_keyring_load(PurplePlugin *);
static gboolean		internal_keyring_unload(PurplePlugin *);
static void		internal_keyring_destroy(PurplePlugin *);

/***********************************************/
/*     Keyring interface                       */
/***********************************************/
static void 
internal_keyring_read(PurpleAccount * account,
		      PurpleKeyringReadCallback cb,
		      gpointer data)
{
	char * password;
	GError * error;

	ACTIVATE();

	purple_debug_info("Internal Keyring",
		"Reading password for account %s (%s).\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account));

	password = GET_PASSWORD(account);

	if (password != NULL) {
		if(cb != NULL)
			cb(account, password, NULL, data);
	} else {
		error = g_error_new(ERR_PIDGINKEYRING, 
			ERR_NOPASSWD, "password not found");
		if(cb != NULL)
			cb(account, NULL, error, data);
		g_error_free(error);
	}
	return;
}

static void
internal_keyring_save(PurpleAccount * account,
		      gchar * password,
		      GDestroyNotify destroy,
		      PurpleKeyringSaveCallback cb,
		      gpointer data)
{
	gchar * copy;

	ACTIVATE();

	if (password == NULL || *password == '\0') {
		g_hash_table_remove(internal_keyring_passwords, account);
		purple_debug_info("Internal Keyring",
			"Deleted password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else {
		copy = g_strdup(password);
		SET_PASSWORD((void *)account, copy);	/* cast prevents warning because account is const */
		purple_debug_info("Internal Keyring",
			"Updated password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

	}

	if(destroy && password)
		destroy(password);

	if (cb != NULL)
		cb(account, NULL, data);
	return;
}


static const char * 
internal_keyring_read_sync(const PurpleAccount * account)
{
	ACTIVATE();

	purple_debug_info("Internal Keyring (_sync_)", 
		"Password for %s (%s) was read.\n",
		purple_account_get_username(account),
		purple_account_get_protocol_id(account));

	return GET_PASSWORD(account);
}

static void
internal_keyring_save_sync(PurpleAccount * account,
			   const char * password)
{
	gchar * copy;

	ACTIVATE();

	if (password == NULL || *password == '\0') {
		g_hash_table_remove(internal_keyring_passwords, account);
		purple_debug_info("Internal Keyring (_sync_)", 
			"Password for %s (%s) was deleted.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	} else {
		copy = g_strdup(password);
		SET_PASSWORD(account, copy);
		purple_debug_info("Internal Keyring (_sync_)", 
			"Password for %s (%s) was set.\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));
	}

	return;
}

static void
internal_keyring_close(GError ** error)
{
	internal_keyring_is_active = FALSE;

	g_hash_table_destroy(internal_keyring_passwords);
	internal_keyring_passwords = NULL;
}

static void
internal_keyring_open()
{
	internal_keyring_passwords = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
	internal_keyring_is_active = TRUE;
}

static gboolean
internal_keyring_import_password(PurpleAccount * account, 
				 const char * mode,
				 const char * data,
				 GError ** error)
{
	gchar * copy;

	ACTIVATE();

	purple_debug_info("Internal keyring",
		"Importing password");

	if (account != NULL && 
	    data != NULL &&
	    (mode == NULL || g_strcmp0(mode, "cleartext") == 0)) {

		copy = g_strdup(data);
		SET_PASSWORD(account, copy);
		return TRUE;

	} else {

		*error = g_error_new(ERR_PIDGINKEYRING, ERR_NOPASSWD, "no password for account");
		return FALSE;

	}

	return TRUE;
}

static gboolean 
internal_keyring_export_password(PurpleAccount * account,
				 const char ** mode,
				 char ** data,
				 GError ** error,
				 GDestroyNotify * destroy)
{
	gchar * password;

	ACTIVATE();

	purple_debug_info("Internal keyring",
		"Exporting password");

	/* we're using this rather than GET_PASSWORD(),
	 * because on account creation, the account might be
	 * exported before the password is known. This would
	 * lead to exporting uninitialised data, which 
	 * we obviously don't want.
	 */
	//password = purple_account_get_password(account);
	password = GET_PASSWORD(account);

	if (password == NULL) {
		return FALSE;
	} else {
		*mode = "cleartext";
		*data = g_strdup(password);
		*destroy = g_free;
		return TRUE;
	}
}




static void
internal_keyring_init()
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, INTERNALKEYRING_NAME);
	purple_keyring_set_id(keyring_handler, INTERNALKEYRING_ID);
	purple_keyring_set_read_sync(keyring_handler, internal_keyring_read_sync);
	purple_keyring_set_save_sync(keyring_handler, internal_keyring_save_sync);
	purple_keyring_set_read_password(keyring_handler, internal_keyring_read);
	purple_keyring_set_save_password(keyring_handler, internal_keyring_save);
	purple_keyring_set_close_keyring(keyring_handler, internal_keyring_close);
	purple_keyring_set_change_master(keyring_handler, NULL);
	purple_keyring_set_import_password(keyring_handler, internal_keyring_import_password);
	purple_keyring_set_export_password(keyring_handler, internal_keyring_export_password);

	purple_keyring_register(keyring_handler);
}

static void
internal_keyring_uninit()
{
	internal_keyring_close(NULL);
	purple_keyring_unregister(keyring_handler);

}



/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static gboolean
internal_keyring_load(PurplePlugin *plugin)
{
	internal_keyring_init();
	return TRUE;
}

static gboolean
internal_keyring_unload(PurplePlugin *plugin)
{
	internal_keyring_uninit();

	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;

	return TRUE;
}

static void
internal_keyring_destroy(PurplePlugin *plugin)
{
	internal_keyring_uninit();
	return;
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,						/* magic */
	PURPLE_MAJOR_VERSION,						/* major_version */
	PURPLE_MINOR_VERSION,						/* minor_version */
	PURPLE_PLUGIN_STANDARD,						/* type */
	NULL,								/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE|PURPLE_PLUGIN_FLAG_AUTOLOAD,	/* flags */
	NULL,								/* dependencies */
	PURPLE_PRIORITY_DEFAULT,					/* priority */
	INTERNALKEYRING_ID,						/* id */
	INTERNALKEYRING_NAME,						/* name */
	INTERNALKEYRING_VERSION,					/* version */
	"Internal Keyring Plugin",					/* summary */
	INTERNALKEYRING_DESCRIPTION,					/* description */
	INTERNALKEYRING_AUTHOR,						/* author */
	"N/A",								/* homepage */
	internal_keyring_load,						/* load */
	internal_keyring_unload,					/* unload */
	internal_keyring_destroy,					/* destroy */
	NULL,								/* ui_info */
	NULL,								/* extra_info */
	NULL,								/* prefs_info */
	NULL,								/* actions */
	NULL,								/* padding... */
	NULL,
	NULL,
	NULL,
};

static void                        
init_plugin(PurplePlugin *plugin)
{                                  
	purple_debug_info("internalkeyring", "init plugin called.\n");
}

PURPLE_INIT_PLUGIN(internal_keyring, init_plugin, plugininfo)

