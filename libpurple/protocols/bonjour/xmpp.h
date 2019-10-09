/**
 * @file xmpp.h The Purple interface to mDNS and peer to peer XMPP.
 *
 * purple
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
 *
 */

#ifndef PURPLE_BONJOUR_XMPP_H
#define PURPLE_BONJOUR_XMPP_H

#include <libxml/parser.h>

#include <purple.h>

typedef struct
{
	GSocketService *service;
	guint16 port;
	PurpleAccount *account;
	GSList *pending_conversations;
} BonjourXMPP;

typedef struct
{
	GCancellable *cancellable;
	GSocketConnection *socket;
	GInputStream *input;
	GOutputStream *output;
	guint rx_handler;
	guint tx_handler;
	guint close_timeout;
	PurpleCircularBuffer *tx_buf;
	int sent_stream_start; /* 0 = Unsent, 1 = Partial, 2 = Complete */
	gboolean recv_stream_start;
	gpointer stream_data;
	xmlParserCtxt *context;
	PurpleXmlNode *current;
	PurpleBuddy *pb;
	PurpleAccount *account;

	/* The following are only needed before attaching to a PurpleBuddy */
	gchar *buddy_name;
	gchar *ip;
	/* This points to a data entry in BonjourBuddy->ips */
	const gchar *ip_link;
} BonjourXMPPConversation;

/**
 * Start listening for xmpp connections.
 *
 * @return -1 if there was a problem, else returns the listening
 *         port number.
 */
gint bonjour_xmpp_start(BonjourXMPP *data);

int bonjour_xmpp_send_message(BonjourXMPP *data, const char *to, const char *body);

void bonjour_xmpp_close_conversation(BonjourXMPPConversation *bconv);

void async_bonjour_xmpp_close_conversation(BonjourXMPPConversation *bconv);

void bonjour_xmpp_stream_started(BonjourXMPPConversation *bconv);

void bonjour_xmpp_process_packet(PurpleBuddy *pb, PurpleXmlNode *packet);

void bonjour_xmpp_stop(BonjourXMPP *data);

void bonjour_xmpp_conv_match_by_ip(BonjourXMPPConversation *bconv);

void bonjour_xmpp_conv_match_by_name(BonjourXMPPConversation *bconv);

typedef enum {
	XEP_IQ_SET,
	XEP_IQ_GET,
	XEP_IQ_RESULT,
	XEP_IQ_ERROR,
	XEP_IQ_NONE
} XepIqType;

typedef struct {
	XepIqType type;
	char *id;
	PurpleXmlNode *node;
	char *to;
	void *data;
} XepIq;

XepIq *xep_iq_new(void *data, XepIqType type, const char *to, const char *from, const char *id);
int xep_iq_send_and_free(XepIq *iq);
GSList * bonjour_xmpp_get_local_ips(int fd);

void append_iface_if_linklocal(char *ip, guint32 interface_param);

#endif /* PURPLE_BONJOUR_XMPP_H */
