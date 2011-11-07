/**
 * @file kwallet.cpp KWallet password storage
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
 #include <config.h>
#endif

#ifndef PURPLE_PLUGINS
 #define PURPLE_PLUGINS
#endif

#include <glib.h>

#include "account.h"
#include "version.h"
#include "keyring.h"
#include "debug.h"
#include "plugin.h"
#include "internal.h"
#include <kwallet.h>

#define KWALLET_NAME        N_("KWallet")
#define KWALLET_VERSION     "0.3b"
#define KWALLET_DESCRIPTION N_("This plugin will store passwords in KWallet.")
#define KWALLET_AUTHOR      "Scrouaf (scrouaf[at]soc.pidgin.im)"
#define KWALLET_ID          "core-scrouaf-kwallet"

PurpleKeyring *keyring_handler;

#define ERR_KWALLETPLUGIN 	kwallet_plugin_error_domain()

namespace KWalletPlugin {
	class engine;
	class request;
	class save_request;
	class read_request;
}

class KWalletPlugin::engine
{
	public :
		engine();
		~engine();
		void queue(request req);
		static engine *Instance();

	signal :
		void walletopened(bool opened);

	private :
		bool connected;
		KWallet::wallet *wallet;
		std::list<request> requests;
		static engine *pinstance;

		KApplication *app;
		void ExecuteRequests();
};

KWalletPlugin::engine::engine()
{
	KAboutData aboutData("libpurple_plugin", N_("LibPurple KWallet Plugin"), "", "", KAboutData::License_GPL, "");
	KCmdLineArgs::init( &aboutData );
	app = new KApplication(false, false);

	connected = FALSE;
	wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, Asynchronous);
	QObject::connect(wallet, SIGNAL(KWallet::Wallet::walletOpened(bool)), SLOT(walletopened(bool)));
}

KWalletPlugin::engine::~engine()
{
	<list<request>>::iterator it;

	for(it = requests.begin(); it != requests.end ; it++) {
		it->abort();
		requests.pop_front();
	}

	KWallet::closeWallet(KWallet::Wallet::NetworkWallet(), TRUE)
	delete wallet;
	pinstance = NULL;
}

engine *
KWalletPlugin::engine::Instance()
{
	if (pinstance == NULL)
		pinstance = new engine;
	return pinstance;
}

void
KWalletPlugin::engine::walletopened(bool opened)
{
	connected = opened;

	if(opened) {
		ExecuteRequests();
	} else {
		for(it = requests.begin(); it != requests.end ; it++) {
			it->abort();
			requests.pop_front();
		}
		delete this;
	}
}

void
KWalletPlugin::engine::queue(request req)
{
	requests.push_back(req);
	ExecuteRequests();
}

void
KWalletPlugin::engine::ExecuteRequests()
{
	if(connected)
		for(it = requests.begin(); it != requests.end() ; it++) {
			it->execute();
			requests.pop_front();
			delete it;
		}
}

class KWalletPlugin::request
{
	public :
		virtual void abort();
		virtual void execute(KWallet::wallet *wallet);

	private :
		gpointer data;
		PurpleAccount *account
		QString password;
}

class KWalletPlugin::save_request : public request
{
	public :
		request(PurpleAccount *account, char *password, void *cb, void *data);
		void abort();
		void execute(KWallet::wallet *wallet);

	private :
		PurpleKeyringReadCallback callback;
}

class KWalletPlugin::read_request : public request
{
	public :
		request(PurpleAccount *account, void *cb, void *data);
		void abort();
		void execute(KWallet::wallet *wallet);

	private :
		PurpleKeyringSaveCallback callback;
}

KWalletPlugin::save_request::save_request(PurpleAccount *acc, char *pw, void *cb, void *userdata)
{
	account  = acc;
	data     = userdata;
	callback = cb;
	password = pw;
}

KWalletPlugin::read_request::read_request(PurpleAccount *acc, void *cb, void *userdata)
{
	account  = acc;
	data     = userdata;
	callback = cb;
	password = NULL;
}

void
KWalletPlugin::save_request::abort()
{
	GError *error;
	if (cb != NULL) {
		error = g_error_new(ERR_KWALLETPLUGIN,
		                    ERR_UNKNOWN,
		                    "Failed to save password");
		cb(account, error, data);
		g_error_free(error);
	}
}

void
KWalletPlugin::read_request::abort()
{
	GError *error;
	if (callback != NULL) {
		error = g_error_new(ERR_KWALLETPLUGIN,
		                    ERR_UNKNOWN,
		                    "Failed to save password");
		cb(account, NULL, error, data);
		g_error_free(error);
	}
}

void
KWalletPlugin::read_request::execute(KWallet::wallet *wallet)
{
	int result;
	GString key;

	key = "purple-" + purple_account_get_username(account) + " " + purple_account_get_protocol_id(account);
	result = wallet.readPassword(key, password);

	if(result != 0)
		abort();
	else
		if (callback != NULL)
			callback(account, (const char *)password, NULL, data);
}

void
KWalletPlugin::save_request::execute(KWallet::wallet *wallet)
{
	int result;
	GString key;

	key = "purple-" + purple_account_get_username(account) + " " + purple_account_get_protocol_id(account);
	result = wallet.writePassword(key, password);

	if(result != 0)
		abort();
	else
		if (callback != NULL)
			callback(account, (const char *)password, NULL, data);
}

extern "C"
{

void
kwallet_read(PurpleAccount *account,
	     PurpleKeyringReadCallback cb,
	     gpointer data)
{
	KWalletPlugin::read_request req(account, cb, data);
	KWalletPlugin::engine::instance()->queue(req);
}

void
kwallet_save(PurpleAccount *account,
	     const char *password,
	     PurpleKeyringSaveCallback cb,
	     gpointer data)
{
	KWalletPlugin::save_request req(account, password, cb, data);
	KWalletPlugin::engine::instance()->queue(req);
}

void
kwallet_close(GError **error)
{
	delete KWalletPlugin::engine::instance();
}

void
kwallet_import(PurpleAccount *account,
	       const char *mode,
	       const char *data,
	       GError **error)
{
	return TRUE;
}

void
kwallet_export(PurpleAccount *account,
	       const char **mode,
	       char **data,
	       GError **error,
	       GDestroyNotify *destroy)
{
	*mode = NULL;
	*data = NULL;
	destroy = NULL;
}

gboolean
kwallet_load(PurplePlugin *plugin)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, KWALLET_NAME);
	purple_keyring_set_id(keyring_handler, KWALLET_ID);
	purple_keyring_set_read_password(keyring_handler, kwallet_read);
	purple_keyring_set_save_password(keyring_handler, kwallet_save);
	purple_keyring_set_close_keyring(keyring_handler, kwallet_close);
	purple_keyring_set_change_master(keyring_handler, kwallet_change_master);
	purple_keyring_set_import_password(keyring_handler, kwallet_import_password);
	purple_keyring_set_export_password(keyring_handler, kwallet_export_password);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

gboolean
kwallet_unload(PurplePlugin *plugin)
{
	kwallet_close();
	return TRUE;
}

void
kwallet_destroy(PurplePlugin *plugin)
{
	kwallet_close();
}

PurplePluginInfo plugininfo =
{
	PURPLE_PLUGIN_MAGIC,				/* magic */
	PURPLE_MAJOR_VERSION,				/* major_version */
	PURPLE_MINOR_VERSION,				/* minor_version */
	PURPLE_PLUGIN_STANDARD,				/* type */
	NULL,								/* ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE|PURPLE_PLUGIN_FLAG_AUTOLOAD,	/* flags */
	NULL,								/* dependencies */
	PURPLE_PRIORITY_DEFAULT,			/* priority */
	KWALLET_ID,							/* id */
	KWALLET_NAME,						/* name */
	KWALLET_VERSION,					/* version */
	"KWallet Keyring Plugin",			/* summary */
	KWALLET_DESCRIPTION,				/* description */
	KWALLET_AUTHOR,						/* author */
	"N/A",								/* homepage */
	kwallet_load,						/* load */
	kwallet_unload,						/* unload */
	kwallet_destroy,					/* destroy */
	NULL,								/* ui_info */
	NULL,								/* extra_info */
	NULL,								/* prefs_info */
	NULL,								/* actions */
	NULL,								/* padding... */
	NULL,
	NULL,
	NULL,
};

void
init_plugin(PurplePlugin *plugin)
{
	purple_debug_info("keyring-kwallet", "init plugin called.\n");
}

GQuark kwallet_plugin_error_domain(void)
{
	return g_quark_from_static_string("KWallet keyring");
}

PURPLE_INIT_PLUGIN(kwallet_keyring, init_plugin, plugininfo)

} /* extern "C" */

