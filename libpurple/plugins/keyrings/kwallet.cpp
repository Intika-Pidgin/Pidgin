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

#include "internal.h"
#include "account.h"
#include "debug.h"
#include "keyring.h"
#include "plugin.h"
#include "version.h"

#include <QQueue>
#include <QCoreApplication>
#include <kwallet.h>

#define KWALLET_NAME        N_("KWallet")
#define KWALLET_VERSION     "0.3b"
#define KWALLET_DESCRIPTION N_("This plugin will store passwords in KWallet.")
#define KWALLET_AUTHOR      "Scrouaf (scrouaf[at]soc.pidgin.im)"
#define KWALLET_ID          "core-scrouaf-kwallet"

PurpleKeyring *keyring_handler;

#define ERR_KWALLETPLUGIN 	kwallet_plugin_error_domain()

namespace KWalletPlugin {

class request
{
	public:
		virtual void abort() = 0;
		virtual void execute(KWallet::Wallet *wallet) = 0;

	protected:
		gpointer data;
		PurpleAccount *account;
		QString password;
};

class engine : private QObject, private QQueue<request*>
{
	Q_OBJECT

	public:
		engine();
		~engine();
		void queue(request *req);
		static engine *Instance();

	private slots:
		void walletOpened(bool opened);

	private:
		QCoreApplication *app;
		bool connected;
		KWallet::Wallet *wallet;
		static engine *pinstance;

		void ExecuteRequests();
};

class save_request : public request
{
	public:
		save_request(PurpleAccount *account, const char *password, PurpleKeyringSaveCallback cb, void *data);
		void abort();
		void execute(KWallet::Wallet *wallet);

	private:
		PurpleKeyringSaveCallback callback;
};

class read_request : public request
{
	public:
		read_request(PurpleAccount *account, PurpleKeyringReadCallback cb, void *data);
		void abort();
		void execute(KWallet::Wallet *wallet);

	private:
		PurpleKeyringReadCallback callback;
};

static GQuark
kwallet_plugin_error_domain(void)
{
	return g_quark_from_static_string("KWallet keyring");
}

}

KWalletPlugin::engine *KWalletPlugin::engine::pinstance = NULL;

KWalletPlugin::engine::engine()
{
	int argc = 0;
	app = new QCoreApplication(argc, NULL);

	connected = FALSE;
	wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Asynchronous);
	connect(wallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool)));
}

KWalletPlugin::engine::~engine()
{
	while (!isEmpty()) {
		request *req = dequeue();
		req->abort();
		delete req;
	}

	KWallet::Wallet::closeWallet(KWallet::Wallet::NetworkWallet(), TRUE);
	delete wallet;
	delete app;
	pinstance = NULL;
}

KWalletPlugin::engine *
KWalletPlugin::engine::Instance()
{
	if (pinstance == NULL)
		pinstance = new engine;
	return pinstance;
}

void
KWalletPlugin::engine::walletOpened(bool opened)
{
	connected = opened;

	if (opened) {
		ExecuteRequests();
	} else {
		while (!isEmpty()) {
			request *req = dequeue();
			req->abort();
			delete req;
		}
		delete this;
	}
}

void
KWalletPlugin::engine::queue(request *req)
{
	enqueue(req);
	ExecuteRequests();
}

void
KWalletPlugin::engine::ExecuteRequests()
{
	app->processEvents();
	if (connected) {
		while (!isEmpty()) {
			request *req = dequeue();
			req->execute(wallet);
			delete req;
		}
	}
}

KWalletPlugin::save_request::save_request(PurpleAccount *acc, const char *pw, PurpleKeyringSaveCallback cb, void *userdata)
{
	account  = acc;
	data     = userdata;
	callback = cb;
	password = QString(pw);
}

KWalletPlugin::read_request::read_request(PurpleAccount *acc, PurpleKeyringReadCallback cb, void *userdata)
{
	account  = acc;
	data     = userdata;
	callback = cb;
	password = QString();
}

void
KWalletPlugin::save_request::abort()
{
	GError *error;
	if (callback != NULL) {
		error = g_error_new(ERR_KWALLETPLUGIN,
		                    PURPLE_KEYRING_ERROR_UNKNOWN,
		                    "Failed to save password");
		callback(account, error, data);
		g_error_free(error);
	}
}

void
KWalletPlugin::read_request::abort()
{
	GError *error;
	if (callback != NULL) {
		error = g_error_new(ERR_KWALLETPLUGIN,
		                    PURPLE_KEYRING_ERROR_UNKNOWN,
		                    "Failed to read password");
		callback(account, NULL, error, data);
		g_error_free(error);
	}
}

void
KWalletPlugin::read_request::execute(KWallet::Wallet *wallet)
{
	int result;
	QString key;

	key = QString("purple-") + purple_account_get_username(account) + " " + purple_account_get_protocol_id(account);
	result = wallet->readPassword(key, password);

	if (result != 0)
		abort();
	else if (callback != NULL)
		callback(account, password.toUtf8().constData(), NULL, data);
}

void
KWalletPlugin::save_request::execute(KWallet::Wallet *wallet)
{
	int result;
	QString key;

	key = QString("purple-") + purple_account_get_username(account) + " " + purple_account_get_protocol_id(account);
	result = wallet->writePassword(key, password);

	if (result != 0)
		abort();
	else if (callback != NULL)
		callback(account, NULL, data);
}

extern "C"
{

static void
kwallet_read(PurpleAccount *account,
             PurpleKeyringReadCallback cb,
             gpointer data)
{
	KWalletPlugin::read_request *req;
	req = new KWalletPlugin::read_request(account, cb, data);
	KWalletPlugin::engine::Instance()->queue(req);
}

static void
kwallet_save(PurpleAccount *account,
             const char *password,
             PurpleKeyringSaveCallback cb,
             gpointer data)
{
	KWalletPlugin::save_request *req;
	req = new KWalletPlugin::save_request(account, password, cb, data);
	KWalletPlugin::engine::Instance()->queue(req);
}

static void
kwallet_close(GError **error)
{
	delete KWalletPlugin::engine::Instance();
}

static gboolean
kwallet_import(PurpleAccount *account,
               const char *mode,
               const char *data,
               GError **error)
{
	return TRUE;
}

static gboolean
kwallet_export(PurpleAccount *account,
               const char **mode,
               char **data,
               GError **error,
               GDestroyNotify *destroy)
{
	*mode = NULL;
	*data = NULL;
	*destroy = NULL;

	return TRUE;
}

static gboolean
kwallet_load(PurplePlugin *plugin)
{
	keyring_handler = purple_keyring_new();

	purple_keyring_set_name(keyring_handler, KWALLET_NAME);
	purple_keyring_set_id(keyring_handler, KWALLET_ID);
	purple_keyring_set_read_password(keyring_handler, kwallet_read);
	purple_keyring_set_save_password(keyring_handler, kwallet_save);
	purple_keyring_set_close_keyring(keyring_handler, kwallet_close);
/*	purple_keyring_set_change_master(keyring_handler, kwallet_change_master);*/
	purple_keyring_set_import_password(keyring_handler, kwallet_import);
	purple_keyring_set_export_password(keyring_handler, kwallet_export);

	purple_keyring_register(keyring_handler);

	return TRUE;
}

static gboolean
kwallet_unload(PurplePlugin *plugin)
{
	kwallet_close(NULL);
	return TRUE;
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
	NULL,								/* destroy */
	NULL,								/* ui_info */
	NULL,								/* extra_info */
	NULL,								/* prefs_info */
	NULL,								/* actions */
	NULL,								/* padding... */
	NULL,
	NULL,
	NULL,
};

void init_plugin(PurplePlugin *plugin);
void
init_plugin(PurplePlugin *plugin)
{
	purple_debug_info("keyring-kwallet", "init plugin called.\n");
}

PURPLE_INIT_PLUGIN(kwallet_keyring, init_plugin, plugininfo)

} /* extern "C" */

#include "kwallet.moc"

