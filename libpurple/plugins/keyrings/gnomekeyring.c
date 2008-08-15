/**
 * @file gnomekeyring.c Gnome keyring password storage
 * @ingroup plugins
 *
 * @todo 
 *   cleanup error handling and reporting
 *   refuse unloading when active (in internal keyring too)
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
#include <gnome-keyring.h>

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

#define GNOMEKEYRING_NAME		"Gnome-Keyring"
#define GNOMEKEYRING_VERSION		"0.2"
#define GNOMEKEYRING_DESCRIPTION	"This plugin provides the default password storage behaviour for libpurple."
#define	GNOMEKEYRING_AUTHOR		"Scrouaf (scrouaf[at]soc.pidgin.im)"
#define GNOMEKEYRING_ID		"core-scrouaf-gnomekeyring"

#define ERR_GNOMEKEYRINGPLUGIN 	gkp_error_domain()

static PurpleKeyring * keyring_handler = NULL;

typedef struct _InfoStorage InfoStorage;

struct _InfoStorage
{
	gpointer cb;
	gpointer user_data;
	PurpleAccount * account;
};




/* a few prototypes : */
static GQuark 		gkp_error_domain(void);
static void 		gkp_read(PurpleAccount *, PurpleKeyringReadCallback, gpointer);
static void		gkp_read_continue(GnomeKeyringResult, char *, gpointer);
static void 		gkp_save(PurpleAccount *, gchar *, GDestroyNotify, PurpleKeyringSaveCallback, gpointer);
static void		gkp_save_continue(GnomeKeyringResult, gpointer);
static const char * 	gkp_read_sync(const PurpleAccount *);
static void 		gkp_save_sync(PurpleAccount *, const gchar *);
static void		gkp_close(GError **);
static gboolean		gkp_import_password(PurpleAccount *, char *, char *, GError **);
static gboolean 	gkp_export_password(PurpleAccount *, const char **, char **, GError **, GDestroyNotify *);
static gboolean		gkp_init(void);
static void		gkp_uninit(void);
static gboolean		gkp_load(PurplePlugin *);
static gboolean		gkp_unload(PurplePlugin *);
static void		gkp_destroy(PurplePlugin *);

GQuark gkp_error_domain(void)
{
	return g_quark_from_static_string("Gnome-Keyring plugin");
}


/***********************************************/
/*     Keyring interface                       */
/***********************************************/
static void 
gkp_read(PurpleAccount * account,
	 PurpleKeyringReadCallback cb,
	 gpointer data)
{
	InfoStorage * storage = g_malloc(sizeof(InfoStorage));

	storage->cb = cb;
	storage->user_data = data;
	storage->account = account;

	gnome_keyring_find_password(GNOME_KEYRING_NETWORK_PASSWORD,
				    gkp_read_continue,
				    storage,
				    g_free,
				    "user", purple_account_get_username(account),
				    "protocol", purple_account_get_protocol_id(account),
				    NULL);
}

static void gkp_read_continue(GnomeKeyringResult result,
                       char *password,
                       gpointer data)
/* XXX : make sure list is freed on return */
{
	InfoStorage * storage = data;
	PurpleAccount * account =storage->account;
	PurpleKeyringReadCallback cb = storage->cb;
	GError * error;

	if (result != GNOME_KEYRING_RESULT_OK) {

		switch(result)
		{
			case GNOME_KEYRING_RESULT_NO_MATCH :
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN, 
					ERR_NOPASSWD, "no password found for account : %s",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				return;

			case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON :
			case GNOME_KEYRING_RESULT_IO_ERROR :
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN, 
					ERR_NOCHANNEL, "Failed to communicate with gnome keyring (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				return;

			default :
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN, 
					ERR_NOCHANNEL, "Unknown error (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, NULL, error, storage->user_data);
				g_error_free(error);
				return;
		}

	} else {

		if(cb != NULL)
			cb(account, password, NULL, storage->user_data);
		return;

	}

}


static void
gkp_save(PurpleAccount * account,
	 gchar * password,
	 GDestroyNotify destroy,
	 PurpleKeyringSaveCallback cb,
	 gpointer data)
{
	InfoStorage * storage = g_malloc(sizeof(InfoStorage));

	storage->account = account;
	storage->cb = cb;
	storage->user_data = data;

	if(password != NULL) {

		purple_debug_info("Gnome keyring plugin",
			"Updating password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

		gnome_keyring_store_password(GNOME_KEYRING_NETWORK_PASSWORD,
					     NULL, 	/*default keyring */
					     g_strdup_printf("pidgin-%s", purple_account_get_username(account)),
					     password,
		                             gkp_save_continue,
					     storage,
					     g_free,		/* function to free storage */
					     "user", purple_account_get_username(account),
					     "protocol", purple_account_get_protocol_id(account),
					     NULL);
	
		if (destroy)
			destroy(password);

	} else {	/* password == NULL, delete password. */

		purple_debug_info("Gnome keyring plugin",
			"Forgetting password for account %s (%s).\n",
			purple_account_get_username(account),
			purple_account_get_protocol_id(account));

		gnome_keyring_delete_password(GNOME_KEYRING_NETWORK_PASSWORD,
					      gkp_save_continue,
					      storage,
					      g_free,
					      "user", purple_account_get_username(account),
					      "protocol", purple_account_get_protocol_id(account),
					      NULL);

	}
	return;
}

static void
gkp_save_continue(GnomeKeyringResult result,
            gpointer data)
{
	InfoStorage * storage = data;
	PurpleKeyringSaveCallback cb = storage->cb;
	GError * error;
	PurpleAccount * account = storage->account;

	if (result != GNOME_KEYRING_RESULT_OK) {
		switch(result)
		{
			case GNOME_KEYRING_RESULT_NO_MATCH :
				purple_debug_info("Gnome keyring plugin",
					"Could not update password for %s (%s) : not found.\n",
					purple_account_get_username(account),
					purple_account_get_protocol_id(account));
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN, 
					ERR_NOPASSWD, "Could not update password for %s : not found",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				return;

			case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON :
			case GNOME_KEYRING_RESULT_IO_ERROR :
				purple_debug_info("Gnome keyring plugin",
					"Failed to communicate with gnome keyring (account : %s (%s)).\n",
					purple_account_get_username(account),
					purple_account_get_protocol_id(account));
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN, 
					ERR_NOCHANNEL, "Failed to communicate with gnome keyring (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				return;

			default :
				purple_debug_info("Gnome keyring plugin",
					"Unknown error (account : %s (%s)).\n",
					purple_account_get_username(account),
					purple_account_get_protocol_id(account));
				error = g_error_new(ERR_GNOMEKEYRINGPLUGIN, 
					ERR_NOCHANNEL, "Unknown error (account : %s).",
					purple_account_get_username(account));
				if(cb != NULL)
					cb(account, error, storage->user_data);
				g_error_free(error);
				return;
		}

	} else {

		purple_debug_info("gnome-keyring-plugin", "password for %s updated.\n",
			purple_account_get_username(account));

		if(cb != NULL)
			cb(account, NULL, storage->user_data);
		return;
	
	}
}

static const char * 
gkp_read_sync(const PurpleAccount * account)
/**
 * This is tricky, since the calling function will not free the password. 
 * Since we free the password on the next request, the last request will leak.
 * But that part of the code shouldn't be used anyway. It might however be an issue
 * if another call is made and the old pointer still used.
 * Elegant solution would include a hashtable or a linked list, but would be
 * complex. Also, this might just be dropped at 1.0 of the plugin
 */
{
	GnomeKeyringResult result;
	static char * password = NULL;

	gnome_keyring_free_password(password);

	result = gnome_keyring_find_password_sync(GNOME_KEYRING_NETWORK_PASSWORD,
		&password,
		"user", purple_account_get_username(account),
		"protocol", purple_account_get_protocol_id(account),
		NULL);

	return password;
}

static void
gkp_save_sync(PurpleAccount * account,
	const char * password)
{
	char * copy = g_strdup(password);
	gkp_save(account, copy, NULL, NULL, NULL);
	g_free(copy);
}

static void
gkp_close(GError ** error)
{
	return;
}

static gboolean
gkp_import_password(PurpleAccount * account, 
		    char * mode,
		    char * data,
		    GError ** error)
{
	purple_debug_info("Gnome Keyring plugin",
		"Importing password.\n");
	return TRUE;
}

static gboolean 
gkp_export_password(PurpleAccount * account,
				 const char ** mode,
				 char ** data,
				 GError ** error,
				 GDestroyNotify * destroy)
{
	purple_debug_info("Gnome Keyring plugin",
		"Exporting password.\n");
	*data = NULL;
	*mode = NULL;
	*destroy = NULL;

	return TRUE;
}




static gboolean
gkp_init()
{
	purple_debug_info("gnome-keyring-plugin", "init.\n");

	if (gnome_keyring_is_available()) {

		keyring_handler = purple_keyring_new();

		purple_keyring_set_name(keyring_handler, GNOMEKEYRING_NAME);
		purple_keyring_set_id(keyring_handler, GNOMEKEYRING_ID);
		purple_keyring_set_read_sync(keyring_handler, gkp_read_sync);
		purple_keyring_set_save_sync(keyring_handler, gkp_save_sync);
		purple_keyring_set_read_password(keyring_handler, gkp_read);
		purple_keyring_set_save_password(keyring_handler, gkp_save);
		purple_keyring_set_close_keyring(keyring_handler, gkp_close);
		purple_keyring_set_change_master(keyring_handler, NULL);
		purple_keyring_set_import_password(keyring_handler, gkp_import_password);
		purple_keyring_set_export_password(keyring_handler, gkp_export_password);

		purple_keyring_register(keyring_handler);

		return TRUE;

	} else {

		purple_debug_info("gnome-keyring-plugin",
			"failed to communicate with daemon, not loading.");
		return FALSE;
	}
}

static void
gkp_uninit()
{
	purple_debug_info("gnome-keyring-plugin", "uninit.\n");
	gkp_close(NULL);
	purple_keyring_unregister(keyring_handler);
	purple_keyring_free(keyring_handler);
	keyring_handler = NULL;
}



/***********************************************/
/*     Plugin interface                        */
/***********************************************/

static gboolean
gkp_load(PurplePlugin *plugin)
{
	return gkp_init();
}

static gboolean
gkp_unload(PurplePlugin *plugin)
{
	gkp_uninit();
	return TRUE;
}

static void
gkp_destroy(PurplePlugin *plugin)
{
	gkp_uninit();
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
	GNOMEKEYRING_ID,						/* id */
	GNOMEKEYRING_NAME,						/* name */
	GNOMEKEYRING_VERSION,					/* version */
	"Internal Keyring Plugin",					/* summary */
	GNOMEKEYRING_DESCRIPTION,					/* description */
	GNOMEKEYRING_AUTHOR,						/* author */
	"N/A",								/* homepage */
	gkp_load,						/* load */
	gkp_unload,					/* unload */
	gkp_destroy,					/* destroy */
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

