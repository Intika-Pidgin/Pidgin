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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */
#include "internal.h"
#include "keyring.h"

#include <kwallet.h>
#include <QQueue>

namespace KWalletPlugin {

class request
{
	public:
		virtual ~request();
		virtual void detailedAbort(enum PurpleKeyringError error) = 0;
		void abort();
		virtual void execute(KWallet::Wallet *wallet) = 0;

	protected:
		gpointer data;
		PurpleAccount *account;
		QString password;
		bool noPassword;
};

class engine : private QObject, private QQueue<request*>
{
	Q_OBJECT

	public:
		engine();
		~engine();
		void queue(request *req);
		void abortAll();
		static engine *instance(bool create);
		static void closeInstance(void);

	private slots:
		void walletOpened(bool opened);
		void walletClosed();

	private:
		static engine *pinstance;

		bool connected;
		bool failed;
		bool closing;
		bool externallyClosed;
		bool busy;
		bool closeAfterBusy;

		KWallet::Wallet *wallet;

		void reopenWallet();
		void executeRequests();
};

class save_request : public request
{
	public:
		save_request(PurpleAccount *account, const char *password,
			PurpleKeyringSaveCallback cb, void *data);
		void detailedAbort(enum PurpleKeyringError error);
		void execute(KWallet::Wallet *wallet);

	private:
		PurpleKeyringSaveCallback callback;
};

class read_request : public request
{
	public:
		read_request(PurpleAccount *account,
			PurpleKeyringReadCallback cb, void *data);
		void detailedAbort(enum PurpleKeyringError error);
		void execute(KWallet::Wallet *wallet);

	private:
		PurpleKeyringReadCallback callback;
};

}
