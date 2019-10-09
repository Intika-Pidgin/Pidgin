/*
 * purple - Jabber Protocol Plugin
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

#include "internal.h"

#include <glib.h>
#include "namespaces.h"
#include "xmlnode.h"
#include "jabber.h"
#include "debug.h"
#include "notify.h"
#include "stream_management.h"

#define MAX_QUEUE_LENGTH 10000

GHashTable *jabber_sm_accounts;

static void
jabber_sm_accounts_queue_free(gpointer q)
{
	g_queue_free_full(q, (GDestroyNotify)xmlnode_free);
}

/* Returns a queue for a JabberStream's account (based on JID),
   creates it if there's none. */
static GQueue *
jabber_sm_accounts_queue_get(JabberStream *js)
{
	GQueue *queue;
	gchar *jid = jabber_id_get_bare_jid(js->user);
	if (g_hash_table_contains(jabber_sm_accounts, jid) == TRUE) {
		queue = g_hash_table_lookup(jabber_sm_accounts, jid);
		g_free(jid);
	} else {
		queue = g_queue_new();
		g_hash_table_insert(jabber_sm_accounts, jid, queue);
	}
	return queue;
}

void
jabber_sm_init(void)
{
	jabber_sm_accounts = g_hash_table_new_full(g_str_hash, g_str_equal, free,
	                                           jabber_sm_accounts_queue_free);
}

void
jabber_sm_uninit(void)
{
	g_hash_table_destroy(jabber_sm_accounts);
}

/* Processes incoming NS_STREAM_MANAGEMENT packets. */
void
jabber_sm_process_packet(JabberStream *js, xmlnode *packet) {
	gchar *jid;
	const char *name = packet->name;
	if (purple_strequal(name, "enabled")) {
		purple_debug_info("XEP-0198", "Stream management is enabled\n");
		js->sm_inbound_count = 0;
		js->sm_state = SM_ENABLED;
	} else if (purple_strequal(name, "failed")) {
		purple_debug_error("XEP-0198", "Failed to enable stream management\n");
		js->sm_state = SM_DISABLED;
		jid = jabber_id_get_bare_jid(js->user);
		g_hash_table_remove(jabber_sm_accounts, jid);
		g_free(jid);
	} else if (purple_strequal(name, "r")) {
		jabber_sm_ack_send(js);
	} else if (purple_strequal(name, "a")) {
		jabber_sm_ack_read(js, packet);
	} else {
		purple_debug_error("XEP-0198", "Unknown packet: %s\n", name);
	}
}

/* Sends an acknowledgement. */
void
jabber_sm_ack_send(JabberStream *js)
{
	xmlnode *ack;
	char *ack_h;
	if (js->sm_state != SM_ENABLED) {
		return;
	}
	ack = xmlnode_new("a");
	ack_h = g_strdup_printf("%u", js->sm_inbound_count);
	xmlnode_set_namespace(ack, NS_STREAM_MANAGEMENT);
	xmlnode_set_attrib(ack, "h", ack_h);
	jabber_send(js, ack);
	xmlnode_free(ack);
	g_free(ack_h);
}

/* Reads acknowledgements, removes queued stanzas. */
void
jabber_sm_ack_read(JabberStream *js, xmlnode *packet)
{
	guint32 i;
	guint32 h;
	GQueue *queue;
	xmlnode *stanza;
	const char *ack_h = xmlnode_get_attrib(packet, "h");
	if (ack_h == NULL) {
		purple_debug_error("XEP-0198",
		                   "The 'h' attribute is not defined for an answer.");
		return;
	}
	h = strtoul(ack_h, NULL, 10);

	/* Remove stanzas from the queue */
	queue = jabber_sm_accounts_queue_get(js);
	for (i = js->sm_outbound_confirmed; i < h; i++) {
		stanza = g_queue_pop_head(queue);
		if (stanza == NULL) {
			purple_debug_error("XEP-0198", "The queue is empty\n");
			break;
		}
		xmlnode_free(stanza);
	}

	js->sm_outbound_confirmed = h;
	purple_debug_info("XEP-0198",
	                  "Acknowledged %u out of %u outbound stanzas\n",
	                  js->sm_outbound_confirmed, js->sm_outbound_count);
}

/* Asks a server to enable stream management, resends queued
   stanzas. */
void
jabber_sm_enable(JabberStream *js)
{
	xmlnode *enable;
	xmlnode *stanza;
	GQueue *queue;
	guint queue_len;
	guint i;
	js->server_caps |= JABBER_CAP_STREAM_MANAGEMENT;
	purple_debug_info("XEP-0198", "Enabling stream management\n");
	enable = xmlnode_new("enable");
	xmlnode_set_namespace(enable, NS_STREAM_MANAGEMENT);
	jabber_send(js, enable);
	xmlnode_free(enable);
	js->sm_outbound_count = 0;
	js->sm_outbound_confirmed = 0;
	js->sm_state = SM_REQUESTED;

	/* Resend unacknowledged stanzas from the queue. */
	queue = jabber_sm_accounts_queue_get(js);
	queue_len = g_queue_get_length(queue);
	if (queue_len > 0) {
		purple_debug_info("XEP-0198", "Resending %u stanzas\n", queue_len);
	}
	for (i = 0; i < queue_len; i++) {
		stanza = g_queue_pop_head(queue);
		jabber_send(js, stanza);
		xmlnode_free(stanza);
	}
}

/* Tracks outbound stanzas, stores those into a queue, requests
   acknowledgements. */
void
jabber_sm_outbound(JabberStream *js, xmlnode *packet)
{
	if (jabber_is_stanza(packet) && js->sm_state != SM_DISABLED) {
		/* Counting stanzas even if there's no confirmation that SM is
		   enabled yet, so that we won't miss any. */

		/* Add this stanza to the queue, unless the queue is full. */
		xmlnode *stanza;
		xmlnode *req;
		GQueue *queue = jabber_sm_accounts_queue_get(js);
		if (g_queue_get_length(queue) < MAX_QUEUE_LENGTH) {
			stanza = xmlnode_copy(packet);
			g_queue_push_tail(queue, stanza);
			if (g_queue_get_length(queue) == MAX_QUEUE_LENGTH) {
				gchar *jid;
				gchar *queue_is_full_message;
				jid = jabber_id_get_bare_jid(js->user);
				queue_is_full_message =
					g_strdup_printf(
						_("The queue for %s has reached its maximum length of %u."),
						jid, MAX_QUEUE_LENGTH);
				purple_debug_warning("XEP-0198",
				                     "Stanza queue for %s is full (%u stanzas).\n",
				                     jid, MAX_QUEUE_LENGTH);
				g_free(jid);
				purple_notify_formatted(js->gc, _("XMPP stream management"),
				                        _("Stanza queue is full"),
				                        _("No further messages will be queued"),
				                        queue_is_full_message,
				                        NULL, NULL);
				g_free(queue_is_full_message);
			}
		}

		/* Count the stanza */
		js->sm_outbound_count++;

		/* Requesting acknowledgements with either SM_REQUESTED or
		   SM_ENABLED state as well, so that it would be harder to lose
		   stanzas. */
		req = xmlnode_new("r");
		xmlnode_set_namespace(req, NS_STREAM_MANAGEMENT);
		jabber_send(js, req);
		xmlnode_free(req);
	}
}

/* Counts inbound stanzas. */
void
jabber_sm_inbound(JabberStream *js, xmlnode *packet)
{
	/* Count stanzas for XEP-0198, excluding stream management
	   packets. */
	if (jabber_is_stanza(packet)) {
		js->sm_inbound_count++;
	}
}
