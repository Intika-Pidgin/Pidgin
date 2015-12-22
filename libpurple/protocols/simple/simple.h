/**
 * @file simple.h
 *
 * purple
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
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

#ifndef _PURPLE_SIMPLE_H
#define _PURPLE_SIMPLE_H

#include <glib.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <time.h>

#include "cipher.h"
#include "circularbuffer.h"
#include "network.h"
#include "proxy.h"
#include "protocol.h"

#include "sipmsg.h"

#define SIMPLE_BUF_INC 1024
#define SIMPLE_REGISTER_RETRY_MAX 2

#define SIMPLE_REGISTER_SENT 1
#define SIMPLE_REGISTER_RETRY 2
#define SIMPLE_REGISTER_COMPLETE 3

#define PUBLISH_EXPIRATION 600
#define SUBSCRIBE_EXPIRATION 1200

#define SIMPLE_TYPE_PROTOCOL             (simple_protocol_get_type())
#define SIMPLE_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), SIMPLE_TYPE_PROTOCOL, SIMPLEProtocol))
#define SIMPLE_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), SIMPLE_TYPE_PROTOCOL, SIMPLEProtocolClass))
#define SIMPLE_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), SIMPLE_TYPE_PROTOCOL))
#define SIMPLE_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), SIMPLE_TYPE_PROTOCOL))
#define SIMPLE_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), SIMPLE_TYPE_PROTOCOL, SIMPLEProtocolClass))

typedef struct _SIMPLEProtocol
{
	PurpleProtocol parent;
} SIMPLEProtocol;

typedef struct _SIMPLEProtocolClass
{
	PurpleProtocolClass parent_class;
} SIMPLEProtocolClass;

struct sip_dialog {
	gchar *ourtag;
	gchar *theirtag;
	gchar *callid;
};

struct simple_watcher {
	gchar *name;
	time_t expire;
	struct sip_dialog dialog;
	gboolean needsxpidf;
};

struct simple_buddy {
	gchar *name;
	time_t resubscribe;
	struct sip_dialog *dialog;
};

struct sip_auth {
	int type; /* 1 = Digest / 2 = NTLM */
	gchar *nonce;
	gchar *opaque;
	gchar *realm;
	gchar *target;
	guint32 flags;
	int nc;
	gchar *digest_session_key;
	int retries;
};

struct simple_account_data {
	PurpleConnection *gc;
	gchar *servername;
	gchar *username;
	gchar *password;
	GCancellable *cancellable;
	PurpleNetworkListenData *listen_data;
	int fd;
	int cseq;
	time_t reregister;
	time_t republish;
	int registerstatus; /* 0 nothing, 1 first registration send, 2 auth received, 3 registered */
	struct sip_auth registrar;
	struct sip_auth proxy;
	int listenfd;
	int listenport;
	int listenpa;
	gchar *status;
	GHashTable *buddies;
	guint registertimeout;
	guint resendtimeout;
	gboolean connecting;
	PurpleAccount *account;
	PurpleCircularBuffer *txbuf;
	guint tx_handler;
	gchar *regcallid;
	GSList *transactions;
	GSList *watcher;
	GSList *openconns;
	gboolean udp;
	struct sockaddr_in serveraddr;
	int registerexpire;
	gchar *realhostname;
	int realport; /* port and hostname from SRV record */
	gchar *publish_etag;
};

struct sip_connection {
	int fd;
	gchar *inbuf;
	int inbuflen;
	int inbufused;
	int inputhandler;
};

struct transaction;

typedef gboolean (*TransCallback) (struct simple_account_data *, struct sipmsg *, struct transaction *);

struct transaction {
	time_t time;
	int retries;
	int transport; /* 0 = tcp, 1 = udp */
	int fd;
	const gchar *cseq;
	struct sipmsg *msg;
	TransCallback callback;
};

G_MODULE_EXPORT GType simple_protocol_get_type(void);

#endif /* _PURPLE_SIMPLE_H */
