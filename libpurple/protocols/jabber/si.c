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
 *
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"

#include "buddylist.h"
#include "debug.h"
#include "xfer.h"
#include "request.h"
#include "network.h"
#include "notify.h"

#include "buddy.h"
#include "data.h"
#include "disco.h"
#include "jabber.h"
#include "ibb.h"
#include "iq.h"
#include "si.h"

#define STREAMHOST_CONNECT_TIMEOUT 15
#define ENABLE_FT_THUMBNAILS 0

typedef struct _JabberSIXfer {
	JabberStream *js;

	PurpleProxyConnectData *connect_data;
	PurpleNetworkListenData *listen_data;
	guint connect_timeout;

	gboolean accepted;

	char *stream_id;
	char *iq_id;

	enum {
		STREAM_METHOD_UNKNOWN     = 0,
		STREAM_METHOD_BYTESTREAMS = 2 << 1,
		STREAM_METHOD_IBB         = 2 << 2,
		STREAM_METHOD_UNSUPPORTED = 2 << 30
	} stream_method;

	GList *streamhosts;
	PurpleProxyInfo *gpi;

	char *rxqueue;
	size_t rxlen;
	gsize rxmaxlen;
	int local_streamhost_fd;

	JabberIBBSession *ibb_session;
	guint ibb_timeout_handle;
	PurpleCircularBuffer *ibb_buffer;
} JabberSIXfer;

/* some forward declarations */
static void jabber_si_xfer_ibb_send_init(JabberStream *js, PurpleXfer *xfer);

static PurpleXfer*
jabber_si_xfer_find(JabberStream *js, const char *sid, const char *from)
{
	GList *xfers;

	if(!sid || !from)
		return NULL;

	for(xfers = js->file_transfers; xfers; xfers = xfers->next) {
		PurpleXfer *xfer = xfers->data;
		JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
		if(jsx->stream_id && purple_xfer_get_remote_user(xfer) &&
				!strcmp(jsx->stream_id, sid) && !strcmp(purple_xfer_get_remote_user(xfer), from))
			return xfer;
	}

	return NULL;
}

static void
jabber_si_free_streamhost(gpointer data, gpointer user_data)
{
	JabberBytestreamsStreamhost *sh = data;

	if(!data)
		return;

	g_free(sh->jid);
	g_free(sh->host);
	g_free(sh->zeroconf);
	g_free(sh);
}



static void jabber_si_bytestreams_attempt_connect(PurpleXfer *xfer);

static void
jabber_si_bytestreams_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	JabberIq *iq;
	PurpleXmlNode *query, *su;
	JabberBytestreamsStreamhost *streamhost = jsx->streamhosts->data;

	purple_proxy_info_destroy(jsx->gpi);
	jsx->gpi = NULL;
	jsx->connect_data = NULL;

	if (jsx->connect_timeout > 0)
		purple_timeout_remove(jsx->connect_timeout);
	jsx->connect_timeout = 0;

	if(source < 0) {
		purple_debug_warning("jabber",
				"si connection failed, jid was %s, host was %s, error was %s\n",
				streamhost->jid, streamhost->host,
				error_message ? error_message : "(null)");
		jsx->streamhosts = g_list_remove(jsx->streamhosts, streamhost);
		jabber_si_free_streamhost(streamhost, NULL);
		jabber_si_bytestreams_attempt_connect(xfer);
		return;
	}

	/* unknown file transfer type is assumed to be RECEIVE */
	if(purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND)
	{
		PurpleXmlNode *activate;
		iq = jabber_iq_new_query(jsx->js, JABBER_IQ_SET, NS_BYTESTREAMS);
		purple_xmlnode_set_attrib(iq->node, "to", streamhost->jid);
		query = purple_xmlnode_get_child(iq->node, "query");
		purple_xmlnode_set_attrib(query, "sid", jsx->stream_id);
		activate = purple_xmlnode_new_child(query, "activate");
		purple_xmlnode_insert_data(activate, purple_xfer_get_remote_user(xfer), -1);

		/* TODO: We need to wait for an activation result before starting */
	}
	else
	{
		iq = jabber_iq_new_query(jsx->js, JABBER_IQ_RESULT, NS_BYTESTREAMS);
		purple_xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
		jabber_iq_set_id(iq, jsx->iq_id);
		query = purple_xmlnode_get_child(iq->node, "query");
		su = purple_xmlnode_new_child(query, "streamhost-used");
		purple_xmlnode_set_attrib(su, "jid", streamhost->jid);
	}

	jabber_iq_send(iq);

	purple_xfer_start(xfer, source, NULL, -1);
}

static gboolean
connect_timeout_cb(gpointer data)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	purple_debug_info("jabber", "Streamhost connection timeout of %d seconds exceeded.\n", STREAMHOST_CONNECT_TIMEOUT);

	jsx->connect_timeout = 0;

	if (jsx->connect_data != NULL)
		purple_proxy_connect_cancel(jsx->connect_data);
	jsx->connect_data = NULL;

	/* Trigger the connect error manually */
	jabber_si_bytestreams_connect_cb(xfer, -1, "Timeout Exceeded.");

	return FALSE;
}

static void
jabber_si_bytestreams_ibb_timeout_remove(JabberSIXfer *jsx)
{
	if (jsx->ibb_timeout_handle) {
		purple_timeout_remove(jsx->ibb_timeout_handle);
		jsx->ibb_timeout_handle = 0;
	}
}

static gboolean
jabber_si_bytestreams_ibb_timeout_cb(gpointer data)
{
	PurpleXfer *xfer = (PurpleXfer *) data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	if (jsx && !jsx->ibb_session) {
		purple_debug_info("jabber",
			"jabber_si_bytestreams_ibb_timeout called and IBB session not set "
			" up yet, cancel transfer");
		jabber_si_bytestreams_ibb_timeout_remove(jsx);
		purple_xfer_cancel_local(xfer);
	}

	return FALSE;
}

static void jabber_si_bytestreams_attempt_connect(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	JabberBytestreamsStreamhost *streamhost;
	JabberID *dstjid;

	if(!jsx->streamhosts) {
		JabberIq *iq = jabber_iq_new(jsx->js, JABBER_IQ_ERROR);
		PurpleXmlNode *error, *inf;

		if(jsx->iq_id)
			jabber_iq_set_id(iq, jsx->iq_id);

		purple_xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
		error = purple_xmlnode_new_child(iq->node, "error");
		purple_xmlnode_set_attrib(error, "code", "404");
		purple_xmlnode_set_attrib(error, "type", "cancel");
		inf = purple_xmlnode_new_child(error, "item-not-found");
		purple_xmlnode_set_namespace(inf, NS_XMPP_STANZAS);

		jabber_iq_send(iq);

		/* if IBB is available, revert to that before giving up... */
		if (jsx->stream_method & STREAM_METHOD_IBB) {
			/* if we are the initializer, init IBB */
			purple_debug_info("jabber",
				"jabber_si_bytestreams_attempt_connect: "
				"no streamhosts found, trying IBB\n");
			/* if we are the sender, open an IBB session, but not if we already
			  did it, since we could have received the error <iq/> from the
			  receiver already... */
			if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND
				&& !jsx->ibb_session) {
				jabber_si_xfer_ibb_send_init(jsx->js, xfer);
			} else {
				/* setup a timeout to cancel waiting for IBB open */
				jsx->ibb_timeout_handle = purple_timeout_add_seconds(30,
					jabber_si_bytestreams_ibb_timeout_cb, xfer);
			}
			/* if we are the receiver, just wait for IBB open, callback is
			  already set up... */
		} else {
			purple_xfer_cancel_local(xfer);
		}

		return;
	}

	streamhost = jsx->streamhosts->data;

	if (jsx->connect_data) {
		purple_debug_info("jabber",
				"jabber_si_bytestreams_attempt_connect: "
				"cancelling existing connection attempt and restarting\n");
		purple_proxy_connect_cancel(jsx->connect_data);
		jsx->connect_data = NULL;
		if (jsx->connect_timeout > 0)
			purple_timeout_remove(jsx->connect_timeout);
		jsx->connect_timeout = 0;
	}
	if (jsx->gpi != NULL)
		purple_proxy_info_destroy(jsx->gpi);
	jsx->gpi = NULL;

	dstjid = jabber_id_new(purple_xfer_get_remote_user(xfer));

	/* TODO: Deal with zeroconf */

	if(dstjid != NULL && streamhost->host && streamhost->port > 0) {
		char *dstaddr, *hash;
		PurpleAccount *account;
		jsx->gpi = purple_proxy_info_new();
		purple_proxy_info_set_proxy_type(jsx->gpi, PURPLE_PROXY_SOCKS5);
		purple_proxy_info_set_host(jsx->gpi, streamhost->host);
		purple_proxy_info_set_port(jsx->gpi, streamhost->port);

		/* unknown file transfer type is assumed to be RECEIVE */
		if(purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND)
			dstaddr = g_strdup_printf("%s%s@%s/%s%s@%s/%s", jsx->stream_id, jsx->js->user->node, jsx->js->user->domain,
				jsx->js->user->resource, dstjid->node, dstjid->domain, dstjid->resource);
		else
			dstaddr = g_strdup_printf("%s%s@%s/%s%s@%s/%s", jsx->stream_id, dstjid->node, dstjid->domain, dstjid->resource,
				jsx->js->user->node, jsx->js->user->domain, jsx->js->user->resource);

		/* Per XEP-0065, the 'host' must be SHA1(SID + from JID + to JID) */
		hash = jabber_calculate_data_hash(dstaddr, strlen(dstaddr), "sha1");

		account = purple_connection_get_account(jsx->js->gc);
		jsx->connect_data = purple_proxy_connect_socks5_account(NULL, account,
				jsx->gpi, hash, 0,
				jabber_si_bytestreams_connect_cb, xfer);
		g_free(hash);
		g_free(dstaddr);

		/* When selecting a streamhost, timeout after STREAMHOST_CONNECT_TIMEOUT seconds, otherwise it takes forever */
		if (purple_xfer_get_xfer_type(xfer) != PURPLE_XFER_TYPE_SEND && jsx->connect_data != NULL)
			jsx->connect_timeout = purple_timeout_add_seconds(
				STREAMHOST_CONNECT_TIMEOUT, connect_timeout_cb, xfer);

		jabber_id_free(dstjid);
	}

	if (jsx->connect_data == NULL)
	{
		jsx->streamhosts = g_list_remove(jsx->streamhosts, streamhost);
		jabber_si_free_streamhost(streamhost, NULL);
		jabber_si_bytestreams_attempt_connect(xfer);
	}
}

void jabber_bytestreams_parse(JabberStream *js, const char *from,
                              JabberIqType type, const char *id, PurpleXmlNode *query)
{
	PurpleXfer *xfer;
	JabberSIXfer *jsx;
	PurpleXmlNode *streamhost;
	const char *sid;

	if(type != JABBER_IQ_SET)
		return;

	if(!from)
		return;

	if(!(sid = purple_xmlnode_get_attrib(query, "sid")))
		return;

	if(!(xfer = jabber_si_xfer_find(js, sid, from)))
		return;

	jsx = purple_xfer_get_protocol_data(xfer);

	if(!jsx->accepted)
		return;

	if(jsx->iq_id)
		g_free(jsx->iq_id);
	jsx->iq_id = g_strdup(id);

	for(streamhost = purple_xmlnode_get_child(query, "streamhost"); streamhost;
			streamhost = purple_xmlnode_get_next_twin(streamhost)) {
		const char *jid, *host = NULL, *port, *zeroconf;
		int portnum = 0;

		if((jid = purple_xmlnode_get_attrib(streamhost, "jid")) &&
				((zeroconf = purple_xmlnode_get_attrib(streamhost, "zeroconf")) ||
				((host = purple_xmlnode_get_attrib(streamhost, "host")) &&
				(port = purple_xmlnode_get_attrib(streamhost, "port")) &&
				(portnum = atoi(port))))) {
			JabberBytestreamsStreamhost *sh = g_new0(JabberBytestreamsStreamhost, 1);
			sh->jid = g_strdup(jid);
			sh->host = g_strdup(host);
			sh->port = portnum;
			sh->zeroconf = g_strdup(zeroconf);
			/* If there were a lot of these, it'd be worthwhile to prepend and reverse. */
			jsx->streamhosts = g_list_append(jsx->streamhosts, sh);
		}
	}

	jabber_si_bytestreams_attempt_connect(xfer);
}


static void
jabber_si_xfer_bytestreams_send_read_again_resp_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	int len;

	len = write(source, jsx->rxqueue + jsx->rxlen, jsx->rxmaxlen - jsx->rxlen);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	}
	jsx->rxlen += len;

	if (jsx->rxlen < jsx->rxmaxlen)
		return;

	purple_input_remove(purple_xfer_get_watcher(xfer));
	purple_xfer_set_watcher(xfer, 0);
	g_free(jsx->rxqueue);
	jsx->rxqueue = NULL;

	/* Before actually starting sending the file, we need to wait until the
	 * recipient sends the IQ result with <streamhost-used/>
	 */
	purple_debug_info("jabber", "SOCKS5 connection negotiation completed. "
					  "Waiting for IQ result to start file transfer.\n");
}

static void
jabber_si_xfer_bytestreams_send_read_again_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	char buffer[42]; /* 40 for DST.ADDR + 2 bytes for port number*/
	int len;
	char *dstaddr, *hash;
	const char *host;

	purple_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_read_again_cb\n");

	if(jsx->rxlen < 5) {
		purple_debug_info("jabber", "reading the first 5 bytes\n");
		len = read(source, buffer, 5 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
		return;
	} else if(jsx->rxqueue[0] != 0x05 || jsx->rxqueue[1] != 0x01 ||
			jsx->rxqueue[3] != 0x03 || jsx->rxqueue[4] != 40) {
		purple_debug_info("jabber", "Invalid socks5 conn req. header[0x%x,0x%x,0x%x,0x%x,0x%x]\n",
				  jsx->rxqueue[0], jsx->rxqueue[1], jsx->rxqueue[2],
				  jsx->rxqueue[3], jsx->rxqueue[4]);
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	} else if(jsx->rxlen - 5 < (size_t)jsx->rxqueue[4] + 2) {
		/* Upper-bound of 257 (jsx->rxlen = 5, jsx->rxqueue[4] = 0xFF) */
		unsigned short to_read = jsx->rxqueue[4] + 2 - (jsx->rxlen - 5);
		purple_debug_info("jabber", "reading %u bytes for DST.ADDR + port num (trying to read %hu now)\n",
				  jsx->rxqueue[4] + 2, to_read);
		len = read(source, buffer, to_read);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
	}

	/* Have we not read all of DST.ADDR and the following 2-byte port number? */
	if(jsx->rxlen - 5 < (size_t)jsx->rxqueue[4] + 2)
		return;

	purple_input_remove(purple_xfer_get_watcher(xfer));
	purple_xfer_set_watcher(xfer, 0);

	dstaddr = g_strdup_printf("%s%s@%s/%s%s", jsx->stream_id,
			jsx->js->user->node, jsx->js->user->domain,
			jsx->js->user->resource, purple_xfer_get_remote_user(xfer));

	/* Per XEP-0065, the 'host' must be SHA1(SID + from JID + to JID) */
	hash = jabber_calculate_data_hash(dstaddr, strlen(dstaddr), "sha1");

	if(strncmp(hash, jsx->rxqueue + 5, 40) ||
			jsx->rxqueue[45] != 0x00 || jsx->rxqueue[46] != 0x00) {
		if (jsx->rxqueue[45] != 0x00 || jsx->rxqueue[46] != 0x00)
			purple_debug_error("jabber", "Got SOCKS5 BS conn with the wrong DST.PORT"
						     " (must be 0 - got[0x%x,0x%x]).\n",
						     jsx->rxqueue[45], jsx->rxqueue[46]);
		else
			purple_debug_error("jabber", "Got SOCKS5 BS conn with the wrong DST.ADDR"
						     " (expected '%s' - got '%.40s').\n",
						     hash, jsx->rxqueue + 5);
		close(source);
		purple_xfer_cancel_remote(xfer);
		g_free(hash);
		g_free(dstaddr);
		return;
	}

	g_free(hash);
	g_free(dstaddr);

	g_free(jsx->rxqueue);
	host = purple_network_get_my_ip(jsx->js->fd);

	jsx->rxmaxlen = 5 + strlen(host) + 2;
	jsx->rxqueue = g_malloc(jsx->rxmaxlen);
	jsx->rxlen = 0;

	jsx->rxqueue[0] = 0x05;
	jsx->rxqueue[1] = 0x00;
	jsx->rxqueue[2] = 0x00;
	jsx->rxqueue[3] = 0x03;
	jsx->rxqueue[4] = strlen(host);
	memcpy(jsx->rxqueue + 5, host, strlen(host));
	jsx->rxqueue[5+strlen(host)] = 0x00;
	jsx->rxqueue[6+strlen(host)] = 0x00;

	purple_xfer_set_watcher(xfer, purple_input_add(source, PURPLE_INPUT_WRITE,
		jabber_si_xfer_bytestreams_send_read_again_resp_cb, xfer));
	jabber_si_xfer_bytestreams_send_read_again_resp_cb(xfer, source,
		PURPLE_INPUT_WRITE);
}

static void
jabber_si_xfer_bytestreams_send_read_response_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	int len;

	len = write(source, jsx->rxqueue + jsx->rxlen, jsx->rxmaxlen - jsx->rxlen);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	}
	jsx->rxlen += len;

	if (jsx->rxlen < jsx->rxmaxlen)
		return;

	/* If we sent a "Success", wait for a response, otherwise give up and cancel */
	if (jsx->rxqueue[1] == 0x00) {
		purple_input_remove(purple_xfer_get_watcher(xfer));
		purple_xfer_set_watcher(xfer, purple_input_add(source, PURPLE_INPUT_READ,
			jabber_si_xfer_bytestreams_send_read_again_cb, xfer));
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		jsx->rxlen = 0;
	} else {
		close(source);
		purple_xfer_cancel_remote(xfer);
	}
}

static void
jabber_si_xfer_bytestreams_send_read_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	int i;
	int len;
	char buffer[256];

	purple_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_read_cb\n");

	purple_xfer_set_fd(xfer, source);

	/** Try to read the SOCKS5 header */
	if(jsx->rxlen < 2) {
		purple_debug_info("jabber", "reading those first two bytes\n");
		len = read(source, buffer, 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
		return;
	} else if(jsx->rxlen - 2 < (size_t)jsx->rxqueue[1]) {
		/* Has a maximum value of 255 (jsx->rxlen = 2, jsx->rxqueue[1] = 0xFF) */
		unsigned short to_read = jsx->rxqueue[1] - (jsx->rxlen - 2);
		purple_debug_info("jabber", "reading %u bytes for auth methods (trying to read %hu now)\n",
				  jsx->rxqueue[1], to_read);
		len = read(source, buffer, to_read);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
	}

	/* Have we not read all the auth. method bytes? */
	if(jsx->rxlen -2 < (size_t)jsx->rxqueue[1])
		return;

	purple_debug_info("jabber", "checking to make sure we're socks FIVE\n");

	if(jsx->rxqueue[0] != 0x05) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_debug_info("jabber", "going to test %u different methods\n", (guint)jsx->rxqueue[1]);

	for(i=0; i<jsx->rxqueue[1]; i++) {

		purple_debug_info("jabber", "testing %u\n", (guint)jsx->rxqueue[i+2]);
		if(jsx->rxqueue[i+2] == 0x00) {
			g_free(jsx->rxqueue);
			jsx->rxlen = 0;
			jsx->rxmaxlen = 2;
			jsx->rxqueue = g_malloc(jsx->rxmaxlen);
			jsx->rxqueue[0] = 0x05;
			jsx->rxqueue[1] = 0x00;
			purple_input_remove(purple_xfer_get_watcher(xfer));
			purple_xfer_set_watcher(xfer, purple_input_add(source, PURPLE_INPUT_WRITE,
				jabber_si_xfer_bytestreams_send_read_response_cb,
				xfer));
			jabber_si_xfer_bytestreams_send_read_response_cb(xfer,
				source, PURPLE_INPUT_WRITE);
			jsx->rxqueue = NULL;
			jsx->rxlen = 0;
			return;
		}
	}

	g_free(jsx->rxqueue);
	jsx->rxlen = 0;
	jsx->rxmaxlen = 2;
	jsx->rxqueue = g_malloc(jsx->rxmaxlen);
	jsx->rxqueue[0] = 0x05;
	jsx->rxqueue[1] = 0xFF;
	purple_input_remove(purple_xfer_get_watcher(xfer));
	purple_xfer_set_watcher(xfer, purple_input_add(source, PURPLE_INPUT_WRITE,
		jabber_si_xfer_bytestreams_send_read_response_cb, xfer));
	jabber_si_xfer_bytestreams_send_read_response_cb(xfer,
		source, PURPLE_INPUT_WRITE);
}

static gint
jabber_si_compare_jid(gconstpointer a, gconstpointer b)
{
	const JabberBytestreamsStreamhost *sh = a;

	if(!a)
		return -1;

	return strcmp(sh->jid, (char *)b);
}

static void
jabber_si_xfer_bytestreams_send_connected_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	int acceptfd;

	purple_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_connected_cb\n");

	acceptfd = accept(source, NULL, 0);
	if(acceptfd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	else if(acceptfd == -1) {
		purple_debug_warning("jabber", "accept: %s\n", g_strerror(errno));
		/* Don't cancel the ft - allow it to fall to the next streamhost.*/
		return;
	}

	purple_input_remove(purple_xfer_get_watcher(xfer));
	close(source);
	jsx->local_streamhost_fd = -1;

	_purple_network_set_common_socket_flags(acceptfd);

	purple_xfer_set_watcher(xfer, purple_input_add(acceptfd, PURPLE_INPUT_READ,
					 jabber_si_xfer_bytestreams_send_read_cb, xfer));
}

static void
jabber_si_connect_proxy_cb(JabberStream *js, const char *from,
                           JabberIqType type, const char *id,
                           PurpleXmlNode *packet, gpointer data)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx;
	PurpleXmlNode *query, *streamhost_used;
	const char *jid;
	GList *matched;

	/* TODO: This need to send errors if we don't see what we're looking for */

	/* Make sure that the xfer is actually still valid and we're not just receiving an old iq response */
	if (!g_list_find(js->file_transfers, xfer)) {
		purple_debug_error("jabber", "Got bytestreams response for no longer existing xfer (%p)\n", xfer);
		return;
	}

	jsx = purple_xfer_get_protocol_data(xfer);

	/* In the case of a direct file transfer, this is expected to return */
	if(!jsx)
		return;

	if(type != JABBER_IQ_RESULT) {
		purple_debug_info("jabber",
			    "jabber_si_xfer_connect_proxy_cb: type = error\n");
		/* if IBB is available, open IBB session */
		purple_debug_info("jabber",
			"jabber_si_xfer_connect_proxy_cb: got error, method: %d\n",
			jsx->stream_method);
		if (jsx->stream_method & STREAM_METHOD_IBB) {
			purple_debug_info("jabber", "IBB is possible, try it\n");
			/* if we are the sender and haven't already opened an IBB
			  session, do so now (we might already have failed to open
			  the bytestream proxy ourselves when receiving this <iq/> */
			if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND
				&& !jsx->ibb_session) {
				jabber_si_xfer_ibb_send_init(js, xfer);
			} else {
				jsx->ibb_timeout_handle = purple_timeout_add_seconds(30,
					jabber_si_bytestreams_ibb_timeout_cb, xfer);
			}
			/* if we are receiver, just wait for IBB open stanza, callback
			  is already set up */
		} else {
			purple_xfer_cancel_remote(xfer);
		}
		return;
	}

	if (!from)
		return;

	if(!(query = purple_xmlnode_get_child(packet, "query")))
		return;

	if(!(streamhost_used = purple_xmlnode_get_child(query, "streamhost-used")))
		return;

	if(!(jid = purple_xmlnode_get_attrib(streamhost_used, "jid")))
		return;

	purple_debug_info("jabber", "jabber_si_connect_proxy_cb() will be looking at jsx %p: jsx->streamhosts is %p and jid is %s\n",
					  jsx, jsx->streamhosts, jid);

	if(!(matched = g_list_find_custom(jsx->streamhosts, jid, jabber_si_compare_jid)))
	{
		gchar *my_jid = g_strdup_printf("%s@%s/%s", jsx->js->user->node,
			jsx->js->user->domain, jsx->js->user->resource);
		if (!strcmp(jid, my_jid)) {
			purple_debug_info("jabber", "Got local SOCKS5 streamhost-used.\n");
			purple_xfer_start(xfer, purple_xfer_get_fd(xfer), NULL, -1);
		} else {
			/* if available, try to revert to IBB... */
			if (jsx->stream_method & STREAM_METHOD_IBB) {
				purple_debug_info("jabber",
					"jabber_si_connect_proxy_cb: trying to revert to IBB\n");
				if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND) {
					jabber_si_xfer_ibb_send_init(jsx->js, xfer);
				} else {
					jsx->ibb_timeout_handle = purple_timeout_add_seconds(30,
						jabber_si_bytestreams_ibb_timeout_cb, xfer);
				}
				/* if we are the receiver, we are already set up...*/
			} else {
				purple_debug_info("jabber",
					"streamhost-used does not match any proxy that was offered to target\n");
				purple_xfer_cancel_local(xfer);
			}
		}
		g_free(my_jid);
		return;
	}

	/* Clean up the local streamhost - it isn't going to be used.*/
	if (purple_xfer_get_watcher(xfer) > 0) {
		purple_input_remove(purple_xfer_get_watcher(xfer));
		purple_xfer_set_watcher(xfer, 0);
	}
	if (jsx->local_streamhost_fd >= 0) {
		close(jsx->local_streamhost_fd);
		jsx->local_streamhost_fd = -1;
	}

	jsx->streamhosts = g_list_remove_link(jsx->streamhosts, matched);
	g_list_foreach(jsx->streamhosts, jabber_si_free_streamhost, NULL);
	g_list_free(jsx->streamhosts);

	jsx->streamhosts = matched;

	jabber_si_bytestreams_attempt_connect(xfer);
}

static void
jabber_si_xfer_bytestreams_listen_cb(int sock, gpointer data)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx;
	JabberIq *iq;
	PurpleXmlNode *query, *streamhost;
	char port[6];
	GList *tmp;
	JabberBytestreamsStreamhost *sh, *sh2;
	int streamhost_count = 0;

	jsx = purple_xfer_get_protocol_data(xfer);
	jsx->listen_data = NULL;

	/* I'm not sure under which conditions this can happen
	 * (it seems like it shouldn't be possible */
	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) {
		g_object_unref(xfer);
		return;
	}

	g_object_unref(xfer);

	iq = jabber_iq_new_query(jsx->js, JABBER_IQ_SET, NS_BYTESTREAMS);
	purple_xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
	query = purple_xmlnode_get_child(iq->node, "query");

	purple_xmlnode_set_attrib(query, "sid", jsx->stream_id);

	/* If we successfully started listening locally */
	if (sock >= 0) {
		gchar *jid;
		GList *local_ips =
			purple_network_get_all_local_system_ips();
		const char *public_ip;
		gboolean has_public_ip = FALSE;

		jsx->local_streamhost_fd = sock;

		jid = g_strdup_printf("%s@%s/%s", jsx->js->user->node,
			jsx->js->user->domain, jsx->js->user->resource);
		purple_xfer_set_local_port(xfer, purple_network_get_port_from_fd(sock));
		g_snprintf(port, sizeof(port), "%hu", purple_xfer_get_local_port(xfer));

		public_ip = purple_network_get_my_ip(jsx->js->fd);

		/* Include the localhost's IPs (for in-network transfers) */
		while (local_ips) {
			gchar *local_ip = local_ips->data;
			streamhost_count++;
			streamhost = purple_xmlnode_new_child(query, "streamhost");
			purple_xmlnode_set_attrib(streamhost, "jid", jid);
			purple_xmlnode_set_attrib(streamhost, "host", local_ip);
			purple_xmlnode_set_attrib(streamhost, "port", port);
			if (purple_strequal(local_ip, public_ip))
				has_public_ip = TRUE;
			g_free(local_ip);
			local_ips = g_list_delete_link(local_ips, local_ips);
		}

		/* Include the public IP (assuming that there is a port mapped somehow) */
		if (!has_public_ip && strcmp(public_ip, "0.0.0.0") != 0) {
			streamhost_count++;
			streamhost = purple_xmlnode_new_child(query, "streamhost");
			purple_xmlnode_set_attrib(streamhost, "jid", jid);
			purple_xmlnode_set_attrib(streamhost, "host", public_ip);
			purple_xmlnode_set_attrib(streamhost, "port", port);
		}

		g_free(jid);

		/* The listener for the local proxy */
		purple_xfer_set_watcher(xfer, purple_input_add(sock, PURPLE_INPUT_READ,
				jabber_si_xfer_bytestreams_send_connected_cb, xfer));
	}

	for (tmp = jsx->js->bs_proxies; tmp; tmp = tmp->next) {
		sh = tmp->data;

		/* TODO: deal with zeroconf proxies */

		if (!(sh->jid && sh->host && sh->port > 0))
			continue;

		purple_debug_info("jabber", "jabber_si_xfer_bytestreams_listen_cb() will be looking at jsx %p: jsx->streamhosts %p and sh->jid %p\n",
						  jsx, jsx->streamhosts, sh->jid);
		if(g_list_find_custom(jsx->streamhosts, sh->jid, jabber_si_compare_jid) != NULL)
			continue;

		streamhost_count++;
		streamhost = purple_xmlnode_new_child(query, "streamhost");
		purple_xmlnode_set_attrib(streamhost, "jid", sh->jid);
		purple_xmlnode_set_attrib(streamhost, "host", sh->host);
		g_snprintf(port, sizeof(port), "%hu", sh->port);
		purple_xmlnode_set_attrib(streamhost, "port", port);

		sh2 = g_new0(JabberBytestreamsStreamhost, 1);
		sh2->jid = g_strdup(sh->jid);
		sh2->host = g_strdup(sh->host);
		/*sh2->zeroconf = g_strdup(sh->zeroconf);*/
		sh2->port = sh->port;

		jsx->streamhosts = g_list_prepend(jsx->streamhosts, sh2);
	}

	/* We have no way of transferring, cancel the transfer */
	if (streamhost_count == 0) {
		jabber_iq_free(iq);

		/* if available, revert to IBB */
		if (jsx->stream_method & STREAM_METHOD_IBB) {
			purple_debug_info("jabber",
				"jabber_si_xfer_bytestreams_listen_cb: trying to revert to IBB\n");
			if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND) {
				/* if we are the sender, init the IBB session... */
				jabber_si_xfer_ibb_send_init(jsx->js, xfer);
			} else {
				jsx->ibb_timeout_handle = purple_timeout_add_seconds(30,
					jabber_si_bytestreams_ibb_timeout_cb, xfer);
			}
			/* if we are the receiver, we should just wait... the IBB open
			  handler has already been set up... */
		} else {
			/* We should probably notify the target,
			  but this really shouldn't ever happen */
			purple_xfer_cancel_local(xfer);
		}

		return;
	}

	jabber_iq_set_callback(iq, jabber_si_connect_proxy_cb, xfer);

	jabber_iq_send(iq);

}

static void
jabber_si_xfer_bytestreams_send_init(PurpleXfer *xfer)
{
	JabberSIXfer *jsx;
	PurpleProxyType proxy_type;

	g_object_ref(xfer);

	jsx = purple_xfer_get_protocol_data(xfer);

	/* TODO: This should probably be done with an account option instead of
	 *       piggy-backing on the TOR proxy type. */
	proxy_type = purple_proxy_info_get_proxy_type(
		purple_proxy_get_setup(purple_connection_get_account(jsx->js->gc)));
	if (proxy_type == PURPLE_PROXY_TOR) {
		purple_debug_info("jabber", "Skipping attempting local streamhost.\n");
		jsx->listen_data = NULL;
	} else
		jsx->listen_data = purple_network_listen_range(0, 0, AF_UNSPEC, SOCK_STREAM, TRUE,
				jabber_si_xfer_bytestreams_listen_cb, xfer);

	if (jsx->listen_data == NULL) {
		/* We couldn't open a local port.  Perhaps we can use a proxy. */
		jabber_si_xfer_bytestreams_listen_cb(-1, xfer);
	}

}

static void
jabber_si_xfer_ibb_error_cb(JabberIBBSession *sess)
{
	PurpleXfer *xfer = (PurpleXfer *) jabber_ibb_session_get_user_data(sess);

	purple_debug_error("jabber", "an error occurred during IBB file transfer\n");
	purple_xfer_cancel_remote(xfer);
}

static void
jabber_si_xfer_ibb_closed_cb(JabberIBBSession *sess)
{
	PurpleXfer *xfer = (PurpleXfer *) jabber_ibb_session_get_user_data(sess);

	purple_debug_info("jabber", "the remote user closed the transfer\n");
	if (purple_xfer_get_bytes_remaining(xfer) > 0) {
		purple_xfer_cancel_remote(xfer);
	} else {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	}
}

static void
jabber_si_xfer_ibb_recv_data_cb(JabberIBBSession *sess, gpointer data,
	gsize size)
{
	PurpleXfer *xfer = (PurpleXfer *) jabber_ibb_session_get_user_data(sess);
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	if ((goffset)size <= purple_xfer_get_bytes_remaining(xfer)) {
		purple_debug_info("jabber", "about to write %" G_GSIZE_FORMAT " bytes from IBB stream\n",
			size);
		purple_circular_buffer_append(jsx->ibb_buffer, data, size);
		purple_xfer_prpl_ready(xfer);
	} else {
		/* trying to write past size of file transfers negotiated size,
		  reject transfer to protect against malicious behaviour */
		purple_debug_error("jabber",
			"IBB file transfer send more data than expected\n");
		purple_xfer_cancel_remote(xfer);
	}

}

static gssize
jabber_si_xfer_ibb_read(guchar **out_buffer, size_t buf_size, PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	guchar *buffer;
	gsize size = purple_circular_buffer_get_used(jsx->ibb_buffer);
	gsize tmp;

	*out_buffer = buffer = g_malloc(size);
	while ((tmp = purple_circular_buffer_get_max_read(jsx->ibb_buffer))) {
		const gchar *output = purple_circular_buffer_get_output(jsx->ibb_buffer);
		memcpy(buffer, output, tmp);
		buffer += tmp;
		purple_circular_buffer_mark_read(jsx->ibb_buffer, tmp);
	}

	return size;
}

static gboolean
jabber_si_xfer_ibb_open_cb(JabberStream *js, const char *who, const char *id,
                           PurpleXmlNode *open)
{
	const gchar *sid = purple_xmlnode_get_attrib(open, "sid");
	PurpleXfer *xfer = jabber_si_xfer_find(js, sid, who);
	if (xfer) {
		JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
		JabberIBBSession *sess =
			jabber_ibb_session_create_from_xmlnode(js, who, id, open, xfer);

		jabber_si_bytestreams_ibb_timeout_remove(jsx);

		if (sess) {
			/* setup callbacks here...*/
			jabber_ibb_session_set_data_received_callback(sess,
				jabber_si_xfer_ibb_recv_data_cb);
			jabber_ibb_session_set_closed_callback(sess,
				jabber_si_xfer_ibb_closed_cb);
			jabber_ibb_session_set_error_callback(sess,
				jabber_si_xfer_ibb_error_cb);

			jsx->ibb_session = sess;
			/* we handle up to block-size bytes of decoded data, to handle
			 clients interpreting the block-size attribute as that
			 (see also remark in ibb.c) */
			jsx->ibb_buffer =
				purple_circular_buffer_new(jabber_ibb_session_get_block_size(sess));

			/* set up read function */
			purple_xfer_set_read_fnc(xfer, jabber_si_xfer_ibb_read);

			/* start the transfer */
			purple_xfer_start(xfer, -1, NULL, 0);
			return TRUE;
		} else {
			/* failed to create IBB session */
			purple_debug_error("jabber", "failed to create IBB session\n");
			purple_xfer_cancel_remote(xfer);
			return FALSE;
		}
	} else {
		/* we got an IBB <open/> for an unknown file transfer, pass along... */
		purple_debug_info("jabber",
			"IBB open did not match any SI file transfer\n");
		return FALSE;
	}
}

static gssize
jabber_si_xfer_ibb_write(const guchar *buffer, size_t len, PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	JabberIBBSession *sess = jsx->ibb_session;
	gsize packet_size = len < jabber_ibb_session_get_max_data_size(sess) ?
		len : jabber_ibb_session_get_max_data_size(sess);

	jabber_ibb_session_send_data(sess, buffer, packet_size);

	return packet_size;
}

static void
jabber_si_xfer_ibb_sent_cb(JabberIBBSession *sess)
{
	PurpleXfer *xfer = (PurpleXfer *) jabber_ibb_session_get_user_data(sess);
	goffset remaining = purple_xfer_get_bytes_remaining(xfer);

	if (remaining == 0) {
		/* close the session */
		jabber_ibb_session_close(sess);
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	} else {
		/* send more... */
		purple_xfer_prpl_ready(xfer);
	}
}

static void
jabber_si_xfer_ibb_opened_cb(JabberIBBSession *sess)
{
	PurpleXfer *xfer = (PurpleXfer *) jabber_ibb_session_get_user_data(sess);

	if (jabber_ibb_session_get_state(sess) == JABBER_IBB_SESSION_OPENED) {
		purple_xfer_start(xfer, -1, NULL, 0);
		purple_xfer_prpl_ready(xfer);
	} else {
		/* error */
		purple_xfer_end(xfer);
	}
}

static void
jabber_si_xfer_ibb_send_init(JabberStream *js, PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	jsx->ibb_session = jabber_ibb_session_create(js, jsx->stream_id,
		purple_xfer_get_remote_user(xfer), xfer);

	if (jsx->ibb_session) {
		/* should set callbacks here... */
		jabber_ibb_session_set_opened_callback(jsx->ibb_session,
			jabber_si_xfer_ibb_opened_cb);
		jabber_ibb_session_set_data_sent_callback(jsx->ibb_session,
			jabber_si_xfer_ibb_sent_cb);
		jabber_ibb_session_set_closed_callback(jsx->ibb_session,
			jabber_si_xfer_ibb_closed_cb);
		jabber_ibb_session_set_error_callback(jsx->ibb_session,
			jabber_si_xfer_ibb_error_cb);

		purple_xfer_set_write_fnc(xfer, jabber_si_xfer_ibb_write);

		jsx->ibb_buffer =
			purple_circular_buffer_new(jabber_ibb_session_get_max_data_size(jsx->ibb_session));

		/* open the IBB session */
		jabber_ibb_session_open(jsx->ibb_session);

	} else {
		/* failed to create IBB session */
		purple_debug_error("jabber",
			"failed to initiate IBB session for file transfer\n");
		purple_xfer_cancel_local(xfer);
	}
}

static void jabber_si_xfer_send_method_cb(JabberStream *js, const char *from,
                                          JabberIqType type, const char *id,
                                          PurpleXmlNode *packet, gpointer data)
{
	PurpleXfer *xfer = data;
	PurpleXmlNode *si, *feature, *x, *field, *value;
	gboolean found_method = FALSE;

	if(!(si = purple_xmlnode_get_child_with_namespace(packet, "si", "http://jabber.org/protocol/si"))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if(!(feature = purple_xmlnode_get_child_with_namespace(si, "feature", "http://jabber.org/protocol/feature-neg"))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if(!(x = purple_xmlnode_get_child_with_namespace(feature, "x", "jabber:x:data"))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	for(field = purple_xmlnode_get_child(x, "field"); field; field = purple_xmlnode_get_next_twin(field)) {
		const char *var = purple_xmlnode_get_attrib(field, "var");
		JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

		if(var && !strcmp(var, "stream-method")) {
			if((value = purple_xmlnode_get_child(field, "value"))) {
				char *val = purple_xmlnode_get_data(value);
				if(val && !strcmp(val, NS_BYTESTREAMS)) {
					jabber_si_xfer_bytestreams_send_init(xfer);
					jsx->stream_method |= STREAM_METHOD_BYTESTREAMS;
					found_method = TRUE;
				} else if (val && !strcmp(val, NS_IBB)) {
					jsx->stream_method |= STREAM_METHOD_IBB;
					if (!found_method) {
						/* we haven't tried to init a bytestream session, yet
						  start IBB right away... */
						jabber_si_xfer_ibb_send_init(js, xfer);
						found_method = TRUE;
					}
				}
				g_free(val);
			}
		}
	}

	if (!found_method) {
		purple_xfer_cancel_remote(xfer);
	}

}

static void jabber_si_xfer_send_request(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	JabberIq *iq;
	PurpleXmlNode *si, *file, *feature, *x, *field, *option, *value;
	char buf[32];
#if ENABLE_FT_THUMBNAILS
	gconstpointer thumb;
	gsize thumb_size;

	purple_xfer_prepare_thumbnail(xfer, "jpeg,png");
#endif
	purple_xfer_set_filename(xfer, g_path_get_basename(purple_xfer_get_local_filename(xfer)));

	iq = jabber_iq_new(jsx->js, JABBER_IQ_SET);
	purple_xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
	si = purple_xmlnode_new_child(iq->node, "si");
	purple_xmlnode_set_namespace(si, "http://jabber.org/protocol/si");
	jsx->stream_id = jabber_get_next_id(jsx->js);
	purple_xmlnode_set_attrib(si, "id", jsx->stream_id);
	purple_xmlnode_set_attrib(si, "profile", NS_SI_FILE_TRANSFER);

	file = purple_xmlnode_new_child(si, "file");
	purple_xmlnode_set_namespace(file, NS_SI_FILE_TRANSFER);
	purple_xmlnode_set_attrib(file, "name", purple_xfer_get_filename(xfer));
	g_snprintf(buf, sizeof(buf), "%" G_GOFFSET_FORMAT, purple_xfer_get_size(xfer));
	purple_xmlnode_set_attrib(file, "size", buf);
	/* maybe later we'll do hash and date attribs */

#if ENABLE_FT_THUMBNAILS
	/* add thumbnail, if appropriate */
	if ((thumb = purple_xfer_get_thumbnail(xfer, &thumb_size))) {
		const gchar *mimetype = purple_xfer_get_thumbnail_mimetype(xfer);
		JabberData *thumbnail_data =
			jabber_data_create_from_data(thumb, thumb_size,
				mimetype, TRUE, jsx->js);
		PurpleXmlNode *thumbnail = purple_xmlnode_new_child(file, "thumbnail");
		purple_xmlnode_set_namespace(thumbnail, NS_THUMBS);
		purple_xmlnode_set_attrib(thumbnail, "cid",
			jabber_data_get_cid(thumbnail_data));
		purple_xmlnode_set_attrib(thumbnail, "mime-type", mimetype);
		/* cache data */
		jabber_data_associate_local(thumbnail_data, NULL);
	}
#endif

	feature = purple_xmlnode_new_child(si, "feature");
	purple_xmlnode_set_namespace(feature, "http://jabber.org/protocol/feature-neg");
	x = purple_xmlnode_new_child(feature, "x");
	purple_xmlnode_set_namespace(x, "jabber:x:data");
	purple_xmlnode_set_attrib(x, "type", "form");
	field = purple_xmlnode_new_child(x, "field");
	purple_xmlnode_set_attrib(field, "var", "stream-method");
	purple_xmlnode_set_attrib(field, "type", "list-single");
	/* maybe we should add an option to always skip bytestreams for people
		behind troublesome firewalls */
	option = purple_xmlnode_new_child(field, "option");
	value = purple_xmlnode_new_child(option, "value");
	purple_xmlnode_insert_data(value, NS_BYTESTREAMS, -1);
	option = purple_xmlnode_new_child(field, "option");
	value = purple_xmlnode_new_child(option, "value");
	purple_xmlnode_insert_data(value, NS_IBB, -1);

	jabber_iq_set_callback(iq, jabber_si_xfer_send_method_cb, xfer);

	/* Store the IQ id so that we can cancel the callback */
	g_free(jsx->iq_id);
	jsx->iq_id = g_strdup(iq->id);

	jabber_iq_send(iq);
}

static void jabber_si_xfer_free(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	if (jsx) {
		JabberStream *js = jsx->js;

		js->file_transfers = g_list_remove(js->file_transfers, xfer);

		if (jsx->connect_data != NULL)
			purple_proxy_connect_cancel(jsx->connect_data);
		if (jsx->listen_data != NULL)
			purple_network_listen_cancel(jsx->listen_data);
		if (jsx->iq_id != NULL)
			jabber_iq_remove_callback_by_id(js, jsx->iq_id);
		if (jsx->local_streamhost_fd >= 0)
			close(jsx->local_streamhost_fd);
		if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND && purple_xfer_get_fd(xfer) >= 0) {
			purple_debug_info("jabber", "remove port mapping\n");
			purple_network_remove_port_mapping(purple_xfer_get_fd(xfer));
		}
		if (jsx->connect_timeout > 0)
			purple_timeout_remove(jsx->connect_timeout);
		if (jsx->ibb_timeout_handle > 0)
			purple_timeout_remove(jsx->ibb_timeout_handle);

		if (jsx->streamhosts) {
			g_list_foreach(jsx->streamhosts, jabber_si_free_streamhost, NULL);
			g_list_free(jsx->streamhosts);
		}

		if (jsx->ibb_session) {
			purple_debug_info("jabber",
				"jabber_si_xfer_free: destroying IBB session\n");
			jabber_ibb_session_destroy(jsx->ibb_session);
		}

		if (jsx->ibb_buffer) {
			g_object_unref(G_OBJECT(jsx->ibb_buffer));
		}

		purple_debug_info("jabber", "jabber_si_xfer_free(): freeing jsx %p\n", jsx);

		g_free(jsx->stream_id);
		g_free(jsx->iq_id);
		/* XXX: free other stuff */
		g_free(jsx->rxqueue);
		g_free(jsx);
		purple_xfer_set_protocol_data(xfer, NULL);
	}
}

/*
 * These four functions should only be called from the PurpleXfer functions
 * (typically purple_xfer_cancel_(remote|local), purple_xfer_end, or
 * purple_xfer_request_denied.
 */
static void jabber_si_xfer_cancel_send(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	/* if there is an IBB session active, send close on that */
	if (jsx->ibb_session) {
		jabber_ibb_session_close(jsx->ibb_session);
	}
	jabber_si_xfer_free(xfer);
	purple_debug_info("jabber", "in jabber_si_xfer_cancel_send\n");
}


static void jabber_si_xfer_request_denied(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	JabberStream *js = jsx->js;

	/*
	 * TODO: It's probably an error if jsx->iq_id == NULL. g_return_if_fail
	 * might be warranted.
	 */
	if (jsx->iq_id && !jsx->accepted) {
		JabberIq *iq;
		PurpleXmlNode *error, *child;
		iq = jabber_iq_new(js, JABBER_IQ_ERROR);
		purple_xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
		jabber_iq_set_id(iq, jsx->iq_id);

		error = purple_xmlnode_new_child(iq->node, "error");
		purple_xmlnode_set_attrib(error, "type", "cancel");
		child = purple_xmlnode_new_child(error, "forbidden");
		purple_xmlnode_set_namespace(child, NS_XMPP_STANZAS);
		child = purple_xmlnode_new_child(error, "text");
		purple_xmlnode_set_namespace(child, NS_XMPP_STANZAS);
		purple_xmlnode_insert_data(child, "Offer Declined", -1);

		jabber_iq_send(iq);
	}

	jabber_si_xfer_free(xfer);
	purple_debug_info("jabber", "in jabber_si_xfer_request_denied\n");
}


static void jabber_si_xfer_cancel_recv(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	/* if there is an IBB session active, send close */
	if (jsx->ibb_session) {
		jabber_ibb_session_close(jsx->ibb_session);
	}
	jabber_si_xfer_free(xfer);
	purple_debug_info("jabber", "in jabber_si_xfer_cancel_recv\n");
}


static void jabber_si_xfer_end(PurpleXfer *xfer)
{
	jabber_si_xfer_free(xfer);
}


static void jabber_si_xfer_send_disco_cb(JabberStream *js, const char *who,
		JabberCapabilities capabilities, gpointer data)
{
	PurpleXfer *xfer = (PurpleXfer *) data;
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);

	if (capabilities & JABBER_CAP_IBB) {
		purple_debug_info("jabber",
			"jabber_si_xfer_send_disco_cb: remote JID supports IBB\n");
		jsx->stream_method |= STREAM_METHOD_IBB;
	}

	if (capabilities & JABBER_CAP_SI_FILE_XFER) {
		jabber_si_xfer_send_request(xfer);
	} else {
		char *msg = g_strdup_printf(_("Unable to send file to %s, user does not support file transfers"), who);
		purple_notify_error(js->gc, _("File Send Failed"),
			_("File Send Failed"), msg,
			purple_request_cpar_from_connection(js->gc));
		g_free(msg);
		purple_xfer_cancel_local(xfer);
	}
}

static void resource_select_cancel_cb(PurpleXfer *xfer, PurpleRequestFields *fields)
{
	purple_xfer_cancel_local(xfer);
}

static void do_transfer_send(PurpleXfer *xfer, const char *resource)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	char **who_v = g_strsplit(purple_xfer_get_remote_user(xfer), "/", 2);
	char *who;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;

	jb = jabber_buddy_find(jsx->js, who_v[0], FALSE);
	if (jb) {
		jbr = jabber_buddy_find_resource(jb, resource);
	}

	who = g_strdup_printf("%s/%s", who_v[0], resource);
	g_strfreev(who_v);
	purple_xfer_set_remote_user(xfer, who);

	if (jbr && jabber_resource_know_capabilities(jbr)) {
		char *msg;

		if (jabber_resource_has_capability(jbr, NS_IBB))
			jsx->stream_method |= STREAM_METHOD_IBB;
		if (jabber_resource_has_capability(jbr, NS_SI_FILE_TRANSFER)) {
			jabber_si_xfer_send_request(xfer);
			g_free(who);
			return;
		}

		msg = g_strdup_printf(_("Unable to send file to %s, user does not support file transfers"), who);
		purple_notify_error(jsx->js->gc, _("File Send Failed"),
			_("File Send Failed"), msg,
			purple_request_cpar_from_connection(jsx->js->gc));
		g_free(msg);
		purple_xfer_cancel_local(xfer);
	} else {
		jabber_disco_info_do(jsx->js, who,
				jabber_si_xfer_send_disco_cb, xfer);
	}

	g_free(who);
}

static void resource_select_ok_cb(PurpleXfer *xfer, PurpleRequestFields *fields)
{
	PurpleRequestField *field = purple_request_fields_get_field(fields, "resource");
	const char *selected_label = purple_request_field_choice_get_value(field);

	do_transfer_send(xfer, selected_label);
}

static void jabber_si_xfer_init(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = purple_xfer_get_protocol_data(xfer);
	JabberIq *iq;
	if(purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		char *resource;
		GList *resources = NULL;

		if(NULL != (resource = jabber_get_resource(purple_xfer_get_remote_user(xfer)))) {
			/* they've specified a resource, no need to ask or
			 * default or anything, just do it */

			do_transfer_send(xfer, resource);
			g_free(resource);
			return;
		}

		jb = jabber_buddy_find(jsx->js, purple_xfer_get_remote_user(xfer), TRUE);

		if (jb) {
			GList *l;

			for (l = jb->resources ; l ; l = g_list_next(l)) {
				jbr = l->data;

				if (!jabber_resource_know_capabilities(jbr) ||
				    (jabber_resource_has_capability(jbr, NS_SI_FILE_TRANSFER)
				     && (jabber_resource_has_capability(jbr, NS_BYTESTREAMS)
				         || jabber_resource_has_capability(jbr, NS_IBB)))) {
					resources = g_list_append(resources, jbr);
				}
			}
		}

		if (!resources) {
			/* no resources online, we're trying to send to someone
			 * whose presence we're not subscribed to, or
			 * someone who is offline.  Let's inform the user */
			char *msg;

			if(!jb) {
				msg = g_strdup_printf(_("Unable to send file to %s, invalid JID"), purple_xfer_get_remote_user(xfer));
			} else if(jb->subscription & JABBER_SUB_TO) {
				msg = g_strdup_printf(_("Unable to send file to %s, user is not online"), purple_xfer_get_remote_user(xfer));
			} else {
				msg = g_strdup_printf(_("Unable to send file to %s, not subscribed to user presence"), purple_xfer_get_remote_user(xfer));
			}

			purple_notify_error(jsx->js->gc, _("File Send Failed"),
				_("File Send Failed"), msg,
				purple_request_cpar_from_connection(jsx->js->gc));
			g_free(msg);
		} else if (g_list_length(resources) == 1) {
			/* only 1 resource online (probably our most common case)
			 * so no need to ask who to send to */
			jbr = resources->data;
			do_transfer_send(xfer, jbr->name);
		} else {
			/* we've got multiple resources, we need to pick one to send to */
			GList *l;
			char *msg = g_strdup_printf(_("Please select the resource of %s to which you would like to send a file"), purple_xfer_get_remote_user(xfer));
			PurpleRequestFields *fields = purple_request_fields_new();
			PurpleRequestField *field = purple_request_field_choice_new("resource", _("Resource"), 0);
			PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);

			purple_request_field_choice_set_data_destructor(field, g_free);

			for(l = resources; l; l = l->next) {
				jbr = l->data;
				purple_request_field_choice_add(field, jbr->name, g_strdup(jbr->name));
			}

			purple_request_field_group_add_field(group, field);

			purple_request_fields_add_group(fields, group);

			purple_request_fields(jsx->js->gc, _("Select a Resource"), msg, NULL, fields,
					_("Send File"), G_CALLBACK(resource_select_ok_cb), _("Cancel"), G_CALLBACK(resource_select_cancel_cb),
					purple_request_cpar_from_connection(jsx->js->gc), xfer);

			g_free(msg);
		}

		g_list_free(resources);
	} else {
		PurpleXmlNode *si, *feature, *x, *field, *value;

		iq = jabber_iq_new(jsx->js, JABBER_IQ_RESULT);
		purple_xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
		if(jsx->iq_id)
			jabber_iq_set_id(iq, jsx->iq_id);
		else
			purple_debug_error("jabber", "Sending SI result with new IQ id.\n");

		jsx->accepted = TRUE;

		si = purple_xmlnode_new_child(iq->node, "si");
		purple_xmlnode_set_namespace(si, "http://jabber.org/protocol/si");

		feature = purple_xmlnode_new_child(si, "feature");
		purple_xmlnode_set_namespace(feature, "http://jabber.org/protocol/feature-neg");

		x = purple_xmlnode_new_child(feature, "x");
		purple_xmlnode_set_namespace(x, "jabber:x:data");
		purple_xmlnode_set_attrib(x, "type", "submit");
		field = purple_xmlnode_new_child(x, "field");
		purple_xmlnode_set_attrib(field, "var", "stream-method");

		/* we should maybe "remember" if bytestreams has failed before (in the
			same session) with this JID, and only present IBB as an option to
			avoid unnessesary timeout */
		/* maybe we should have an account option to always just try IBB
			for people who know their firewalls are very restrictive */
		if (jsx->stream_method & STREAM_METHOD_BYTESTREAMS) {
			value = purple_xmlnode_new_child(field, "value");
			purple_xmlnode_insert_data(value, NS_BYTESTREAMS, -1);
		} else if(jsx->stream_method & STREAM_METHOD_IBB) {
			value = purple_xmlnode_new_child(field, "value");
			purple_xmlnode_insert_data(value, NS_IBB, -1);
		}

		jabber_iq_send(iq);
	}
}

PurpleXfer *jabber_si_new_xfer(PurpleConnection *gc, const char *who)
{
	JabberStream *js;

	PurpleXfer *xfer;
	JabberSIXfer *jsx;

	js = purple_connection_get_protocol_data(gc);

	xfer = purple_xfer_new(purple_connection_get_account(gc), PURPLE_XFER_TYPE_SEND, who);
	if (xfer)
	{
		jsx = g_new0(JabberSIXfer, 1);
		purple_xfer_set_protocol_data(xfer, jsx);
		jsx->js = js;
		jsx->local_streamhost_fd = -1;

		jsx->ibb_session = NULL;

		purple_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
		purple_xfer_set_cancel_send_fnc(xfer, jabber_si_xfer_cancel_send);
		purple_xfer_set_end_fnc(xfer, jabber_si_xfer_end);

		js->file_transfers = g_list_append(js->file_transfers, xfer);
	}

	return xfer;
}

void jabber_si_xfer_send(PurpleConnection *gc, const char *who, const char *file)
{
	PurpleXfer *xfer;

	xfer = jabber_si_new_xfer(gc, who);

	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

#if ENABLE_FT_THUMBNAILS
static void
jabber_si_thumbnail_cb(JabberData *data, gchar *alt, gpointer userdata)
{
	PurpleXfer *xfer = (PurpleXfer *) userdata;

	if (data) {
		purple_xfer_set_thumbnail(xfer, jabber_data_get_data(data),
			jabber_data_get_size(data), jabber_data_get_type(data));
		/* data is ephemeral, get rid of now (the xfer re-owned the thumbnail */
		jabber_data_destroy(data);
	}

	purple_xfer_request(xfer);
}
#endif

void jabber_si_parse(JabberStream *js, const char *from, JabberIqType type,
                     const char *id, PurpleXmlNode *si)
{
	JabberSIXfer *jsx;
	PurpleXfer *xfer;
	PurpleXmlNode *file, *feature, *x, *field, *option, *value;
#if ENABLE_FT_THUMBNAILS
	PurpleXmlNode *thumbnail;
#endif
	const char *stream_id, *filename, *filesize_c, *profile;
	goffset filesize = 0;

	if(!(profile = purple_xmlnode_get_attrib(si, "profile")) ||
			strcmp(profile, NS_SI_FILE_TRANSFER))
		return;

	if(!(stream_id = purple_xmlnode_get_attrib(si, "id")))
		return;

	if(!(file = purple_xmlnode_get_child(si, "file")))
		return;

	if(!(filename = purple_xmlnode_get_attrib(file, "name")))
		return;

	if((filesize_c = purple_xmlnode_get_attrib(file, "size")))
		filesize = g_ascii_strtoull(filesize_c, NULL, 10);

	if(!(feature = purple_xmlnode_get_child(si, "feature")))
		return;

	if(!(x = purple_xmlnode_get_child_with_namespace(feature, "x", "jabber:x:data")))
		return;

	if(!from)
		return;

	/* if they've already sent us this file transfer with the same damn id
	 * then we're gonna ignore it, until I think of something better to do
	 * with it */
	if(jabber_si_xfer_find(js, stream_id, from) != NULL)
		return;

	jsx = g_new0(JabberSIXfer, 1);
	jsx->local_streamhost_fd = -1;

	jsx->ibb_session = NULL;

	for(field = purple_xmlnode_get_child(x, "field"); field; field = purple_xmlnode_get_next_twin(field)) {
		const char *var = purple_xmlnode_get_attrib(field, "var");
		if(var && !strcmp(var, "stream-method")) {
			for(option = purple_xmlnode_get_child(field, "option"); option;
					option = purple_xmlnode_get_next_twin(option)) {
				if((value = purple_xmlnode_get_child(option, "value"))) {
					char *val;
					if((val = purple_xmlnode_get_data(value))) {
						if(!strcmp(val, NS_BYTESTREAMS)) {
							jsx->stream_method |= STREAM_METHOD_BYTESTREAMS;
						} else if(!strcmp(val, NS_IBB)) {
							jsx->stream_method |= STREAM_METHOD_IBB;
						}
						g_free(val);
					}
				}
			}
		}
	}

	if(jsx->stream_method == STREAM_METHOD_UNKNOWN) {
		g_free(jsx);
		return;
	}

	jsx->js = js;
	jsx->stream_id = g_strdup(stream_id);
	jsx->iq_id = g_strdup(id);

	xfer = purple_xfer_new(purple_connection_get_account(js->gc), PURPLE_XFER_TYPE_RECEIVE, from);
	g_return_if_fail(xfer != NULL);

	purple_xfer_set_protocol_data(xfer, jsx);

	purple_xfer_set_filename(xfer, filename);
	if(filesize > 0)
		purple_xfer_set_size(xfer, filesize);

	purple_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
	purple_xfer_set_request_denied_fnc(xfer, jabber_si_xfer_request_denied);
	purple_xfer_set_cancel_recv_fnc(xfer, jabber_si_xfer_cancel_recv);
	purple_xfer_set_end_fnc(xfer, jabber_si_xfer_end);

	js->file_transfers = g_list_append(js->file_transfers, xfer);

#if ENABLE_FT_THUMBNAILS
	/* if there is a thumbnail, we should request it... */
	if ((thumbnail = purple_xmlnode_get_child_with_namespace(file, "thumbnail",
		NS_THUMBS))) {
		const char *cid = purple_xmlnode_get_attrib(thumbnail, "cid");
		if (cid) {
			jabber_data_request(js, cid, purple_xfer_get_remote_user(xfer),
			    NULL, TRUE, jabber_si_thumbnail_cb, xfer);
			return;
		}
	}
#endif

	purple_xfer_request(xfer);
}

void
jabber_si_init(void)
{
	jabber_iq_register_handler("si", "http://jabber.org/protocol/si", jabber_si_parse);

	jabber_ibb_register_open_handler(jabber_si_xfer_ibb_open_cb);
}

void
jabber_si_uninit(void)
{
	jabber_ibb_unregister_open_handler(jabber_si_xfer_ibb_open_cb);
}

