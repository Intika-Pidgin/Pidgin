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

#include "account.h"
#include "accountopt.h"
#include "buddylist.h"
#include "core.h"
#include "cmds.h"
#include "connection.h"
#include "conversation.h"
#include "debug.h"
#include "http.h"
#include "message.h"
#include "notify.h"
#include "pluginpref.h"
#include "proxy.h"
#include "protocol.h"
#include "request.h"
#include "server.h"
#include "status.h"
#include "util.h"
#include "version.h"
#include "xmlnode.h"

#include "auth.h"
#include "buddy.h"
#include "caps.h"
#include "chat.h"
#include "data.h"
#include "disco.h"
#include "google/google.h"
#include "google/google_p2p.h"
#include "google/google_roster.h"
#include "google/google_session.h"
#include "ibb.h"
#include "iq.h"
#include "jutil.h"
#include "message.h"
#include "parser.h"
#include "presence.h"
#include "jabber.h"
#include "roster.h"
#include "ping.h"
#include "si.h"
#include "usermood.h"
#include "xdata.h"
#include "pep.h"
#include "adhoccommands.h"
#include "xmpp.h"
#include "gtalk.h"

#include "jingle/jingle.h"
#include "jingle/content.h"
#include "jingle/iceudp.h"
#include "jingle/rawudp.h"
#include "jingle/rtp.h"
#include "jingle/session.h"

#define PING_TIMEOUT 60
/* Send a whitespace keepalive to the server if we haven't sent
 * anything in the last 120 seconds
 */
#define DEFAULT_INACTIVITY_TIME 120

GList *jabber_features = NULL;
GList *jabber_identities = NULL;

static PurpleProtocol *xmpp_protocol = NULL;
static PurpleProtocol *gtalk_protocol = NULL;

static GHashTable *jabber_cmds = NULL; /* PurpleProtocol * => GSList of ids */

static gint plugin_ref = 0;

static void jabber_unregister_account_cb(JabberStream *js);

static void jabber_stream_init(JabberStream *js)
{
	char *open_stream;

	g_free(js->stream_id);
	js->stream_id = NULL;

	open_stream = g_strdup_printf("<stream:stream to='%s' "
				          "xmlns='" NS_XMPP_CLIENT "' "
						  "xmlns:stream='" NS_XMPP_STREAMS "' "
						  "version='1.0'>",
						  js->user->domain);
	/* setup the parser fresh for each stream */
	jabber_parser_setup(js);
	jabber_send_raw(js, open_stream, -1);
	js->reinit = FALSE;
	g_free(open_stream);
}

static void
jabber_session_initialized_cb(JabberStream *js, const char *from,
                              JabberIqType type, const char *id,
                              PurpleXmlNode *packet, gpointer data)
{
	if (type == JABBER_IQ_RESULT) {
		jabber_disco_items_server(js);
		if(js->unregistration)
			jabber_unregister_account_cb(js);
	} else {
		purple_connection_error(js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			("Error initializing session"));
	}
}

static void jabber_session_init(JabberStream *js)
{
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
	PurpleXmlNode *session;

	jabber_iq_set_callback(iq, jabber_session_initialized_cb, NULL);

	session = purple_xmlnode_new_child(iq->node, "session");
	purple_xmlnode_set_namespace(session, NS_XMPP_SESSION);

	jabber_iq_send(iq);
}

static void jabber_bind_result_cb(JabberStream *js, const char *from,
                                  JabberIqType type, const char *id,
                                  PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *bind;

	if (type == JABBER_IQ_RESULT &&
			(bind = purple_xmlnode_get_child_with_namespace(packet, "bind", NS_XMPP_BIND))) {
		PurpleXmlNode *jid;
		char *full_jid;
		if((jid = purple_xmlnode_get_child(bind, "jid")) && (full_jid = purple_xmlnode_get_data(jid))) {
			jabber_id_free(js->user);

			js->user = jabber_id_new(full_jid);
			if (js->user == NULL) {
				purple_connection_error(js->gc,
					PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Invalid response from server"));
				g_free(full_jid);
				return;
			}

			js->user_jb = jabber_buddy_find(js, full_jid, TRUE);
			js->user_jb->subscription |= JABBER_SUB_BOTH;

			purple_connection_set_display_name(js->gc, full_jid);

			g_free(full_jid);
		}
	} else {
		PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
		char *msg = jabber_parse_error(js, packet, &reason);
		purple_connection_error(js->gc, reason, msg);
		g_free(msg);

		return;
	}

	jabber_session_init(js);
}

static char *jabber_prep_resource(char *input) {
	char hostname[256], /* current hostname */
		 *dot = NULL;

	/* Empty resource == don't send any */
	if (input == NULL || *input == '\0')
		return NULL;

	if (strstr(input, "__HOSTNAME__") == NULL)
		return g_strdup(input);

	/* Replace __HOSTNAME__ with hostname */
	if (gethostname(hostname, sizeof(hostname) - 1)) {
		purple_debug_warning("jabber", "gethostname: %s\n", g_strerror(errno));
		/* according to glibc doc, the only time an error is returned
		   is if the hostname is longer than the buffer, in which case
		   glibc 2.2+ would still fill the buffer with partial
		   hostname, so maybe we want to detect that and use it
		   instead
		*/
		g_strlcpy(hostname, "localhost", sizeof(hostname));
	}
	hostname[sizeof(hostname) - 1] = '\0';

	/* We want only the short hostname, not the FQDN - this will prevent the
	 * resource string from being unreasonably long on systems which stuff the
	 * whole FQDN in the hostname */
	if((dot = strchr(hostname, '.')))
		*dot = '\0';

	return purple_strreplace(input, "__HOSTNAME__", hostname);
}

static gboolean
jabber_process_starttls(JabberStream *js, PurpleXmlNode *packet)
{
#if 0
	PurpleXmlNode *starttls;

	PurpleAccount *account;

	account = purple_connection_get_account(js->gc);

	/*
	 * This code DOES NOT EXIST, will never be enabled by default, and
	 * will never ever be supported (by me).
	 * It's literally *only* for developer testing.
	 */
	{
		const gchar *connection_security = purple_account_get_string(account, "connection_security", JABBER_DEFAULT_REQUIRE_TLS);
		if (!g_str_equal(connection_security, "none")) {
			jabber_send_raw(js,
					"<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>", -1);
			return TRUE;
		}
	}
#else
	jabber_send_raw(js,
			"<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>", -1);
	return TRUE;
#endif

#if 0
	starttls = purple_xmlnode_get_child(packet, "starttls");
	if(purple_xmlnode_get_child(starttls, "required")) {
		purple_connection_error(js->gc,
				PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("Server requires TLS/SSL, but no TLS/SSL support was found."));
		return TRUE;
	}

	if (g_str_equal("require_tls", purple_account_get_string(account, "connection_security", JABBER_DEFAULT_REQUIRE_TLS))) {
		purple_connection_error(js->gc,
				PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("You require encryption, but no TLS/SSL support was found."));
		return TRUE;
	}

	return FALSE;
#endif
}

void jabber_stream_features_parse(JabberStream *js, PurpleXmlNode *packet)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	const char *connection_security =
		purple_account_get_string(account, "connection_security", JABBER_DEFAULT_REQUIRE_TLS);

	if (purple_xmlnode_get_child(packet, "starttls")) {
		if (jabber_process_starttls(js, packet)) {
			jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING_ENCRYPTION);
			return;
		}
	} else if (g_str_equal(connection_security, "require_tls") && !jabber_stream_is_ssl(js)) {
		purple_connection_error(js->gc,
			 PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
			_("You require encryption, but it is not available on this server."));
		return;
	}

	if(js->registration) {
		jabber_register_start(js);
	} else if(purple_xmlnode_get_child(packet, "mechanisms")) {
		jabber_stream_set_state(js, JABBER_STREAM_AUTHENTICATING);
		jabber_auth_start(js, packet);
	} else if(purple_xmlnode_get_child(packet, "bind")) {
		PurpleXmlNode *bind, *resource;
		char *requested_resource;
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
		bind = purple_xmlnode_new_child(iq->node, "bind");
		purple_xmlnode_set_namespace(bind, NS_XMPP_BIND);
		requested_resource = jabber_prep_resource(js->user->resource);

		if (requested_resource != NULL) {
			resource = purple_xmlnode_new_child(bind, "resource");
			purple_xmlnode_insert_data(resource, requested_resource, -1);
			g_free(requested_resource);
		}

		jabber_iq_set_callback(iq, jabber_bind_result_cb, NULL);

		jabber_iq_send(iq);
	} else if (purple_xmlnode_get_child_with_namespace(packet, "ver", NS_ROSTER_VERSIONING)) {
		js->server_caps |= JABBER_CAP_ROSTER_VERSIONING;
	} else /* if(purple_xmlnode_get_child_with_namespace(packet, "auth")) */ {
		/* If we get an empty stream:features packet, or we explicitly get
		 * an auth feature with namespace http://jabber.org/features/iq-auth
		 * we should revert back to iq:auth authentication, even though we're
		 * connecting to an XMPP server.  */
		jabber_stream_set_state(js, JABBER_STREAM_AUTHENTICATING);
		jabber_auth_start_old(js);
	}
}

static void jabber_stream_handle_error(JabberStream *js, PurpleXmlNode *packet)
{
	PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
	char *msg = jabber_parse_error(js, packet, &reason);

	purple_connection_error(js->gc, reason, msg);

	g_free(msg);
}

static void tls_init(JabberStream *js);

void jabber_process_packet(JabberStream *js, PurpleXmlNode **packet)
{
	const char *name;
	const char *xmlns;

	purple_signal_emit(purple_connection_get_protocol(js->gc), "jabber-receiving-xmlnode", js->gc, packet);

	/* if the signal leaves us with a null packet, we're done */
	if(NULL == *packet)
		return;

	name = (*packet)->name;
	xmlns = purple_xmlnode_get_namespace(*packet);

	if(!strcmp((*packet)->name, "iq")) {
		jabber_iq_parse(js, *packet);
	} else if(!strcmp((*packet)->name, "presence")) {
		jabber_presence_parse(js, *packet);
	} else if(!strcmp((*packet)->name, "message")) {
		jabber_message_parse(js, *packet);
	} else if (purple_strequal(xmlns, NS_XMPP_STREAMS)) {
		if (g_str_equal(name, "features"))
			jabber_stream_features_parse(js, *packet);
		else if (g_str_equal(name, "error"))
			jabber_stream_handle_error(js, *packet);
	} else if (purple_strequal(xmlns, NS_XMPP_SASL)) {
		if (js->state != JABBER_STREAM_AUTHENTICATING)
			purple_debug_warning("jabber", "Ignoring spurious SASL stanza %s\n", name);
		else {
			if (g_str_equal(name, "challenge"))
				jabber_auth_handle_challenge(js, *packet);
			else if (g_str_equal(name, "success"))
				jabber_auth_handle_success(js, *packet);
			else if (g_str_equal(name, "failure"))
				jabber_auth_handle_failure(js, *packet);
		}
	} else if (purple_strequal(xmlns, NS_XMPP_TLS)) {
		if (js->state != JABBER_STREAM_INITIALIZING_ENCRYPTION || js->gsc)
			purple_debug_warning("jabber", "Ignoring spurious %s\n", name);
		else {
			if (g_str_equal(name, "proceed"))
				tls_init(js);
			/* TODO: Handle <failure/>, I guess? */
		}
	} else {
		purple_debug_warning("jabber", "Unknown packet: %s\n", (*packet)->name);
	}
}

static int jabber_do_send(JabberStream *js, const char *data, int len)
{
	int ret;

	if (js->gsc)
		ret = purple_ssl_write(js->gsc, data, len);
	else
		ret = write(js->fd, data, len);

	return ret;
}

static void jabber_send_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	JabberStream *js = data;
	const gchar *output = NULL;
	int ret, writelen;

	writelen = purple_circular_buffer_get_max_read(js->write_buffer);
	output = purple_circular_buffer_get_output(js->write_buffer);

	if (writelen == 0) {
		purple_input_remove(js->writeh);
		js->writeh = 0;
		return;
	}

	ret = jabber_do_send(js, output, writelen);

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0) {
		gchar *tmp = g_strdup_printf(_("Lost connection with server: %s"),
				g_strerror(errno));
		purple_connection_error(js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	purple_circular_buffer_mark_read(js->write_buffer, ret);
}

static gboolean do_jabber_send_raw(JabberStream *js, const char *data, int len)
{
	int ret;
	gboolean success = TRUE;

	g_return_val_if_fail(len > 0, FALSE);

	if (js->state == JABBER_STREAM_CONNECTED)
		jabber_stream_restart_inactivity_timer(js);

	if (js->writeh == 0)
		ret = jabber_do_send(js, data, len);
	else {
		ret = -1;
		errno = EAGAIN;
	}

	if (ret < 0 && errno != EAGAIN) {
		PurpleAccount *account = purple_connection_get_account(js->gc);
		/*
		 * The server may have closed the socket (on a stream error), so if
		 * we're disconnecting, don't generate (possibly another) error that
		 * (for some UIs) would mask the first.
		 */
		if (!purple_account_is_disconnecting(account)) {
			gchar *tmp = g_strdup_printf(_("Lost connection with server: %s"),
					g_strerror(errno));
			purple_connection_error(js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
			g_free(tmp);
		}

		success = FALSE;
	} else if (ret < len) {
		if (ret < 0)
			ret = 0;
		if (js->writeh == 0)
			js->writeh = purple_input_add(
				js->gsc ? js->gsc->fd : js->fd,
				PURPLE_INPUT_WRITE, jabber_send_cb, js);
		purple_circular_buffer_append(js->write_buffer,
			data + ret, len - ret);
	}

	return success;
}

void jabber_send_raw(JabberStream *js, const char *data, int len)
{
	PurpleConnection *gc;
	PurpleAccount *account;

	gc = js->gc;
	account = purple_connection_get_account(gc);

	g_return_if_fail(data != NULL);

	/* because printing a tab to debug every minute gets old */
	if (data && strcmp(data, "\t") != 0) {
		const char *username;
		char *text = NULL, *last_part = NULL, *tag_start = NULL;

		/* Because debug logs with plaintext passwords make me sad */
		if (!purple_debug_is_unsafe() && js->state != JABBER_STREAM_CONNECTED &&
				/* Either <auth> or <query><password>... */
				(((tag_start = strstr(data, "<auth ")) &&
					strstr(data, "xmlns='" NS_XMPP_SASL "'")) ||
				((tag_start = strstr(data, "<query ")) &&
					strstr(data, "xmlns='jabber:iq:auth'>") &&
					(tag_start = strstr(tag_start, "<password>"))))) {
			char *data_start, *tag_end = strchr(tag_start, '>');
			text = g_strdup(data);

			/* Better to print out some wacky debugging than crash
			 * due to a plugin sending bad xml */
			if (tag_end == NULL)
				tag_end = tag_start;

			data_start = text + (tag_end - data) + 1;

			last_part = strchr(data_start, '<');
			*data_start = '\0';
		}

		username = purple_connection_get_display_name(gc);
		if (!username)
			username = purple_account_get_username(account);

		purple_debug_misc("jabber", "Sending%s (%s): %s%s%s\n",
				jabber_stream_is_ssl(js) ? " (ssl)" : "", username,
				text ? text : data,
				last_part ? "password removed" : "",
				last_part ? last_part : "");

		g_free(text);
	}

	purple_signal_emit(purple_connection_get_protocol(gc), "jabber-sending-text", gc, &data);
	if (data == NULL)
		return;

	if (len == -1)
		len = strlen(data);

	/* If we've got a security layer, we need to encode the data,
	 * splitting it on the maximum buffer length negotiated */
#ifdef HAVE_CYRUS_SASL
	if (js->sasl_maxbuf>0) {
		int pos = 0;

		if (!js->gsc && js->fd<0)
			g_return_if_reached();

		while (pos < len) {
			int towrite;
			const char *out;
			unsigned olen;
			int rc;

			towrite = MIN((len - pos), js->sasl_maxbuf);

			rc = sasl_encode(js->sasl, &data[pos], towrite,
			                 &out, &olen);
			if (rc != SASL_OK) {
				gchar *error =
					g_strdup_printf(_("SASL error: %s"),
						sasl_errdetail(js->sasl));
				purple_debug_error("jabber",
					"sasl_encode error %d: %s\n", rc,
					sasl_errdetail(js->sasl));
				purple_connection_error(gc,
					PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					error);
				g_free(error);
				return;
			}
			pos += towrite;

			/* do_jabber_send_raw returns FALSE when it throws a
			 * connection error.
			 */
			if (!do_jabber_send_raw(js, out, olen))
				break;
		}
		return;
	}
#endif

	if (js->bosh)
		jabber_bosh_connection_send(js->bosh, data);
	else
		do_jabber_send_raw(js, data, len);
}

int jabber_protocol_send_raw(PurpleConnection *gc, const char *buf, int len)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);

	g_return_val_if_fail(js != NULL, -1);
	/* TODO: It's probably worthwhile to restrict this to when the account
	 * state is CONNECTED, but I can /almost/ envision reasons for wanting
	 * to do things during the connection process.
	 */

	jabber_send_raw(js, buf, len);
	return (len < 0 ? (int)strlen(buf) : len);
}

void jabber_send_signal_cb(PurpleConnection *pc, PurpleXmlNode **packet,
                           gpointer unused)
{
	JabberStream *js;
	char *txt;
	int len;

	if (NULL == packet)
		return;

	PURPLE_ASSERT_CONNECTION_IS_VALID(pc);

	js = purple_connection_get_protocol_data(pc);

	if (NULL == js)
		return;

	if (js->bosh)
		if (g_str_equal((*packet)->name, "message") ||
				g_str_equal((*packet)->name, "iq") ||
				g_str_equal((*packet)->name, "presence"))
			purple_xmlnode_set_namespace(*packet, NS_XMPP_CLIENT);
	txt = purple_xmlnode_to_str(*packet, &len);
	jabber_send_raw(js, txt, len);
	g_free(txt);
}

void jabber_send(JabberStream *js, PurpleXmlNode *packet)
{
	purple_signal_emit(purple_connection_get_protocol(js->gc), "jabber-sending-xmlnode", js->gc, &packet);
}

static gboolean jabber_keepalive_timeout(PurpleConnection *gc)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);
	purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Ping timed out"));
	js->keepalive_timeout = 0;
	return FALSE;
}

void jabber_keepalive(PurpleConnection *gc)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);
	time_t now = time(NULL);

	if (js->keepalive_timeout == 0 && (now - js->last_ping) >= PING_TIMEOUT) {
		js->last_ping = now;

		jabber_keepalive_ping(js);
		js->keepalive_timeout = purple_timeout_add_seconds(120,
				(GSourceFunc)(jabber_keepalive_timeout), gc);
	}
}

static void
jabber_recv_cb_ssl(gpointer data, PurpleSslConnection *gsc,
		PurpleInputCondition cond)
{
	PurpleConnection *gc = data;
	JabberStream *js = purple_connection_get_protocol_data(gc);
	int len;
	static char buf[4096];

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	while((len = purple_ssl_read(gsc, buf, sizeof(buf) - 1)) > 0) {
		purple_connection_update_last_received(gc);
		buf[len] = '\0';
		purple_debug_misc("jabber", "Recv (ssl)(%d): %s", len, buf);
		jabber_parser_process(js, buf, len);
		if(js->reinit)
			jabber_stream_init(js);
	}

	if(len < 0 && errno == EAGAIN)
		return;
	else {
		gchar *tmp;
		if (len == 0)
			tmp = g_strdup(_("Server closed the connection"));
		else
			tmp = g_strdup_printf(_("Lost connection with server: %s"),
					g_strerror(errno));
		purple_connection_error(js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
	}
}

static void
jabber_recv_cb(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleConnection *gc = data;
	JabberStream *js = purple_connection_get_protocol_data(gc);
	int len;
	static char buf[4096];

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	if((len = read(js->fd, buf, sizeof(buf) - 1)) > 0) {
		purple_connection_update_last_received(gc);
#ifdef HAVE_CYRUS_SASL
		if (js->sasl_maxbuf > 0) {
			const char *out;
			unsigned int olen;
			int rc;

			rc = sasl_decode(js->sasl, buf, len, &out, &olen);
			if (rc != SASL_OK) {
				gchar *error =
					g_strdup_printf(_("SASL error: %s"),
						sasl_errdetail(js->sasl));
				purple_debug_error("jabber",
					"sasl_decode_error %d: %s\n", rc,
					sasl_errdetail(js->sasl));
				purple_connection_error(gc,
					PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					error);
			} else if (olen > 0) {
				purple_debug_info("jabber", "RecvSASL (%u): %s\n", olen, out);
				jabber_parser_process(js, out, olen);
				if (js->reinit)
					jabber_stream_init(js);
			}
			return;
		}
#endif
		buf[len] = '\0';
		purple_debug_misc("jabber", "Recv (%d): %s", len, buf);
		jabber_parser_process(js, buf, len);
		if(js->reinit)
			jabber_stream_init(js);
	} else if(len < 0 && errno == EAGAIN) {
		return;
	} else {
		gchar *tmp;
		if (len == 0)
			tmp = g_strdup(_("Server closed the connection"));
		else
			tmp = g_strdup_printf(_("Lost connection with server: %s"),
					g_strerror(errno));
		purple_connection_error(js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
	}
}

static void
jabber_login_callback_ssl(gpointer data, PurpleSslConnection *gsc,
		PurpleInputCondition cond)
{
	PurpleConnection *gc = data;
	JabberStream *js;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	js = purple_connection_get_protocol_data(gc);

	if(js->state == JABBER_STREAM_CONNECTING)
		jabber_send_raw(js, "<?xml version='1.0' ?>", -1);
	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING);
	purple_ssl_input_add(gsc, jabber_recv_cb_ssl, gc);

	/* Tell the app that we're doing encryption */
	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING_ENCRYPTION);
}

static void
txt_resolved_cb(GObject *sender, GAsyncResult *result, gpointer data)
{
	GError *error = NULL;
	GList *records = NULL, *l = NULL;
	JabberStream *js = data;
	gboolean found = FALSE;

	records = g_resolver_lookup_records_finish(G_RESOLVER(sender),
			result, &error);
	if(error) {
		purple_debug_warning("jabber", "Unable to find alternative XMPP connection "
				  "methods after failing to connect directly. : %s\n",
				  error->message);

		purple_connection_error(js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Unable to connect"));

		g_error_free(error);

		return;
	}

	for(l = records; l; l = l->next) {
		GVariantIter *iter = NULL;
		gchar *str = NULL;

		g_variant_get((GVariant *)l->data, "(as)", &iter);
		while(g_variant_iter_loop(iter, "s", &str)) {
			gchar **token = g_strsplit(str, "=", 2);

			if(!g_ascii_strcasecmp(token[0], "_xmpp-client-xbosh")) {
				purple_debug_info("jabber","Found alternative connection method using %s at %s.\n", token[0], token[1]);

				js->bosh = jabber_bosh_connection_new(js, token[1]);

				g_strfreev(token);

				break;
			}

			g_strfreev(token);
		}

		g_variant_iter_free(iter);
	}

	g_list_free_full(records, (GDestroyNotify)g_variant_unref);

	if (js->bosh)
		found = TRUE;

	if (!found) {
		purple_debug_warning("jabber", "Unable to find alternative XMPP connection "
				  "methods after failing to connect directly.\n");
		purple_connection_error(js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Unable to connect"));
		return;
	}
}

static void
jabber_login_callback(gpointer data, gint source, const gchar *error)
{
	PurpleConnection *gc = data;
	JabberStream *js = purple_connection_get_protocol_data(gc);

	if (source < 0) {
		GResolver *resolver = g_resolver_get_default();
		gchar *name = g_strdup_printf("_xmppconnect.%s", js->user->domain);

		purple_debug_info("jabber", "Couldn't connect directly to %s.  Trying to find alternative connection methods, like BOSH.\n", js->user->domain);

		g_resolver_lookup_records_async(resolver,
		                                name,
		                                G_RESOLVER_RECORD_TXT,
		                                js->cancellable,
		                                txt_resolved_cb,
		                                js);
		g_free(name);
		g_object_unref(resolver);

		return;
	}

	js->fd = source;

	if(js->state == JABBER_STREAM_CONNECTING)
		jabber_send_raw(js, "<?xml version='1.0' ?>", -1);

	jabber_stream_set_state(js, JABBER_STREAM_INITIALIZING);
	js->inpa = purple_input_add(js->fd, PURPLE_INPUT_READ, jabber_recv_cb, gc);
}

static void
jabber_ssl_connect_failure(PurpleSslConnection *gsc, PurpleSslErrorType error,
		gpointer data)
{
	PurpleConnection *gc = data;
	JabberStream *js;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	js = purple_connection_get_protocol_data(gc);
	js->gsc = NULL;

	purple_connection_ssl_error (gc, error);
}

static void tls_init(JabberStream *js)
{
	purple_input_remove(js->inpa);
	js->inpa = 0;
	js->gsc = purple_ssl_connect_with_host_fd(purple_connection_get_account(js->gc), js->fd,
			jabber_login_callback_ssl, jabber_ssl_connect_failure, js->certificate_CN, js->gc);
	/* The fd is no longer our concern */
	js->fd = -1;
}

static gboolean jabber_login_connect(JabberStream *js, const char *domain, const char *host, int port,
				 gboolean fatal_failure)
{
	/* host should be used in preference to domain to
	 * allow SASL authentication to work with FQDN of the server,
	 * but we use domain as fallback for when users enter IP address
	 * in connect server */
	g_free(js->serverFQDN);
	if (purple_ip_address_is_valid(host))
		js->serverFQDN = g_strdup(domain);
	else
		js->serverFQDN = g_strdup(host);

	if (purple_proxy_connect(js->gc, purple_connection_get_account(js->gc),
			host, port, jabber_login_callback, js->gc) == NULL) {
		if (fatal_failure) {
			purple_connection_error(js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Unable to connect"));
		}

		return FALSE;
	}

	return TRUE;
}

static void
srv_resolved_cb(GObject *sender, GAsyncResult *result, gpointer data)
{
	GError *error = NULL;
	GList *targets = NULL, *l = NULL;
	JabberStream *js = data;

	targets = g_resolver_lookup_service_finish(G_RESOLVER(sender),
			result, &error);
	if(error) {
		purple_debug_warning("jabber",
		                     "SRV lookup failed, proceeding with normal connection : %s",
		                     error->message);

		g_error_free(error);

		jabber_login_connect(js, js->user->domain, js->user->domain,
				purple_account_get_int(purple_connection_get_account(js->gc), "port", 5222),
				TRUE);

	} else {
		for(l = targets; l; l = l->next) {
			GSrvTarget *target = (GSrvTarget *)l->data;
			const gchar *hostname = g_srv_target_get_hostname(target);
			guint port = g_srv_target_get_port(target);

			if(jabber_login_connect(js, hostname, hostname, port, FALSE)) {
				g_resolver_free_targets(targets);

				return;
			}
		}

		g_resolver_free_targets(targets);

		jabber_login_connect(js, js->user->domain, js->user->domain,
				purple_account_get_int(purple_connection_get_account(js->gc), "port", 5222),
				TRUE);		
	}
}

static JabberStream *
jabber_stream_new(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	JabberStream *js;
	PurplePresence *presence;
	gchar *user;
	gchar *slash;

	js = g_new0(JabberStream, 1);
	purple_connection_set_protocol_data(gc, js);
	js->gc = gc;
	js->fd = -1;
	js->http_conns = purple_http_connection_set_new();

	/* we might want to expose this at some point */
	js->cancellable = g_cancellable_new();

	user = g_strdup(purple_account_get_username(account));
	/* jabber_id_new doesn't accept "user@domain/" as valid */
	slash = strchr(user, '/');
	if (slash && *(slash + 1) == '\0')
		*slash = '\0';
	js->user = jabber_id_new(user);

	if (!js->user) {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
			_("Invalid XMPP ID"));
		g_free(user);
		/* Destroying the connection will free the JabberStream */
		return NULL;
	}

	if (!js->user->node || *(js->user->node) == '\0') {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
			_("Invalid XMPP ID. Username portion must be set."));
		g_free(user);
		/* Destroying the connection will free the JabberStream */
		return NULL;
	}

	if (!js->user->domain || *(js->user->domain) == '\0') {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
			_("Invalid XMPP ID. Domain must be set."));
		g_free(user);
		/* Destroying the connection will free the JabberStream */
		return NULL;
	}

	js->buddies = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)jabber_buddy_free);

	/* This is overridden during binding, but we need it here
	 * in case the server only does legacy non-sasl auth!.
	 */
	purple_connection_set_display_name(gc, user);

	js->user_jb = jabber_buddy_find(js, user, TRUE);
	g_free(user);
	if (!js->user_jb) {
		/* This basically *can't* fail, but for good measure... */
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
			_("Invalid XMPP ID"));
		/* Destroying the connection will free the JabberStream */
		g_return_val_if_reached(NULL);
	}

	js->user_jb->subscription |= JABBER_SUB_BOTH;

	js->iq_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)jabber_iq_callbackdata_free);
	js->chats = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)jabber_chat_free);
	js->next_id = g_random_int();
	js->write_buffer = purple_circular_buffer_new(512);
	js->old_length = 0;
	js->keepalive_timeout = 0;
	js->max_inactivity = DEFAULT_INACTIVITY_TIME;
	/* Set the default protocol version to 1.0. Overridden in parser.c. */
	js->protocol_version.major = 1;
	js->protocol_version.minor = 0;
	js->sessions = NULL;
	js->stun_ip = NULL;
	js->stun_port = 0;
	js->google_relay_token = NULL;
	js->google_relay_host = NULL;

	/* if we are idle, set idle-ness on the stream (this could happen if we get
		disconnected and the reconnects while being idle. I don't think it makes
		sense to do this when registering a new account... */
	presence = purple_account_get_presence(account);
	if (purple_presence_is_idle(presence))
		js->idle = purple_presence_get_idle_time(presence);

	return js;
}

static void
jabber_stream_connect(JabberStream *js)
{
	PurpleConnection *gc = js->gc;
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *connect_server = purple_account_get_string(account,
			"connect_server", "");
	const char *bosh_url = purple_account_get_string(account,
			"bosh_url", "");

	jabber_stream_set_state(js, JABBER_STREAM_CONNECTING);

	/* If both BOSH and a Connect Server are specified, we prefer BOSH. I'm not
	 * attached to that choice, though.
	 */
	if (*bosh_url) {
		js->bosh = jabber_bosh_connection_new(js, bosh_url);
		if (!js->bosh) {
			purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
				_("Malformed BOSH URL"));
		}

		return;
	}

	js->certificate_CN = g_strdup(connect_server[0] ? connect_server : js->user->domain);

	/* if they've got old-ssl mode going, we probably want to ignore SRV lookups */
	if (g_str_equal("old_ssl", purple_account_get_string(account, "connection_security", JABBER_DEFAULT_REQUIRE_TLS))) {
		js->gsc = purple_ssl_connect(account, js->certificate_CN,
				purple_account_get_int(account, "port", 5223),
				jabber_login_callback_ssl, jabber_ssl_connect_failure, gc);
		if (!js->gsc) {
			purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("Unable to establish SSL connection"));
		}

		return;
	}

	/* no old-ssl, so if they've specified a connect server, we'll use that, otherwise we'll
	 * invoke the magic of SRV lookups, to figure out host and port */
	if(connect_server[0]) {
		jabber_login_connect(js, js->user->domain, connect_server,
				purple_account_get_int(account, "port", 5222), TRUE);
	} else {
		GResolver *resolver = g_resolver_get_default();
		g_resolver_lookup_service_async(resolver,
		                                "xmpp-client",
		                                "tcp",
		                                js->user->domain,
		                                js->cancellable,
		                                srv_resolved_cb,
		                                js);
		g_object_unref(resolver);
	}
}

void
jabber_login(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	JabberStream *js;
	PurpleImage *image;

	purple_connection_set_flags(gc, PURPLE_CONNECTION_FLAG_HTML |
		PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY |
		PURPLE_CONNECTION_FLAG_NO_IMAGES);
	js = jabber_stream_new(account);
	if (js == NULL)
		return;

	/* replace old default proxies with the new default: NULL
	 * TODO: these can eventually be removed */
	if (purple_strequal("proxy.jabber.org", purple_account_get_string(account, "ft_proxies", ""))
			|| purple_strequal("proxy.eu.jabber.org", purple_account_get_string(account, "ft_proxies", "")))
		purple_account_set_string(account, "ft_proxies", NULL);

	/*
	 * Calculate the avatar hash for our current image so we know (when we
	 * fetch our vCard and PEP avatar) if we should send our avatar to the
	 * server.
	 */
	image = purple_buddy_icons_find_account_icon(account);
	if (image != NULL) {
		js->initial_avatar_hash = jabber_calculate_data_hash(
			purple_image_get_data(image),
			purple_image_get_size(image), "sha1");
		g_object_unref(image);
	}

	jabber_stream_connect(js);
}


static gboolean
conn_close_cb(gpointer data)
{
	JabberStream *js = data;
	PurpleAccount *account = purple_connection_get_account(js->gc);

	jabber_parser_free(js);

	purple_account_disconnect(account);

	js->conn_close_timeout = 0;

	return FALSE;
}

static void
jabber_connection_schedule_close(JabberStream *js)
{
	js->conn_close_timeout = purple_timeout_add(0, conn_close_cb, js);
}

static void
jabber_registration_result_cb(JabberStream *js, const char *from,
                              JabberIqType type, const char *id,
                              PurpleXmlNode *packet, gpointer data)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	char *buf;
	char *to = data;

	if (type == JABBER_IQ_RESULT) {
		if(js->registration) {
			buf = g_strdup_printf(_("Registration of %s@%s successful"),
					js->user->node, js->user->domain);
			purple_account_register_completed(account, TRUE);
		} else {
			g_return_if_fail(to != NULL);
			buf = g_strdup_printf(_("Registration to %s successful"),
				to);
		}
		purple_notify_info(NULL, _("Registration Successful"),
			_("Registration Successful"), buf,
			purple_request_cpar_from_connection(js->gc));
		g_free(buf);
	} else {
		char *msg = jabber_parse_error(js, packet, NULL);

		if(!msg)
			msg = g_strdup(_("Unknown Error"));

		purple_notify_error(NULL, _("Registration Failed"),
			_("Registration Failed"), msg,
			purple_request_cpar_from_connection(js->gc));
		g_free(msg);
		purple_account_register_completed(account, FALSE);
	}
	g_free(to);
	if(js->registration)
		jabber_connection_schedule_close(js);
}

static void
jabber_unregistration_result_cb(JabberStream *js, const char *from,
                                JabberIqType type, const char *id,
                                PurpleXmlNode *packet, gpointer data)
{
	char *buf;
	char *to = data;

	/* This function is never called for unregistering our XMPP account from
	 * the server, so there should always be a 'to' address. */
	g_return_if_fail(to != NULL);

	if (type == JABBER_IQ_RESULT) {
		buf = g_strdup_printf(_("Registration from %s successfully removed"),
							  to);
		purple_notify_info(NULL, _("Unregistration Successful"),
			_("Unregistration Successful"), buf,
			purple_request_cpar_from_connection(js->gc));
		g_free(buf);
	} else {
		char *msg = jabber_parse_error(js, packet, NULL);

		if(!msg)
			msg = g_strdup(_("Unknown Error"));

		purple_notify_error(NULL, _("Unregistration Failed"),
			_("Unregistration Failed"), msg,
			purple_request_cpar_from_connection(js->gc));
		g_free(msg);
	}
	g_free(to);
}

typedef struct _JabberRegisterCBData {
	JabberStream *js;
	char *who;
} JabberRegisterCBData;

static void
jabber_register_cb(JabberRegisterCBData *cbdata, PurpleRequestFields *fields)
{
	GList *groups, *flds;
	PurpleXmlNode *query, *y;
	JabberIq *iq;
	char *username;

	iq = jabber_iq_new_query(cbdata->js, JABBER_IQ_SET, "jabber:iq:register");
	query = purple_xmlnode_get_child(iq->node, "query");
	if (cbdata->who)
		purple_xmlnode_set_attrib(iq->node, "to", cbdata->who);

	for(groups = purple_request_fields_get_groups(fields); groups;
			groups = groups->next) {
		for(flds = purple_request_field_group_get_fields(groups->data);
				flds; flds = flds->next) {
			PurpleRequestField *field = flds->data;
			const char *id = purple_request_field_get_id(field);
			if(!strcmp(id,"unregister")) {
				gboolean value = purple_request_field_bool_get_value(field);
				if(value) {
					/* unregister from service. this doesn't include any of the fields, so remove them from the stanza by recreating it
					   (there's no "remove child" function for PurpleXmlNode) */
					jabber_iq_free(iq);
					iq = jabber_iq_new_query(cbdata->js, JABBER_IQ_SET, "jabber:iq:register");
					query = purple_xmlnode_get_child(iq->node, "query");
					if (cbdata->who)
						purple_xmlnode_set_attrib(iq->node,"to",cbdata->who);
					purple_xmlnode_new_child(query, "remove");

					jabber_iq_set_callback(iq, jabber_unregistration_result_cb, cbdata->who);

					jabber_iq_send(iq);
					g_free(cbdata);
					return;
				}
			} else {
				const char *ids[] = {"username", "password", "name", "email", "nick", "first",
					"last", "address", "city", "state", "zip", "phone", "url", "date",
					NULL};
				const char *value = purple_request_field_string_get_value(field);
				int i;
				for (i = 0; ids[i]; i++) {
					if (!strcmp(id, ids[i]))
						break;
				}

				if (!ids[i])
					continue;
				y = purple_xmlnode_new_child(query, ids[i]);
				purple_xmlnode_insert_data(y, value, -1);
				if(cbdata->js->registration && !strcmp(id, "username")) {
					g_free(cbdata->js->user->node);
					cbdata->js->user->node = g_strdup(value);
				}
				if(cbdata->js->registration && !strcmp(id, "password"))
					purple_account_set_password(purple_connection_get_account(cbdata->js->gc), value, NULL, NULL);
			}
		}
	}

	if(cbdata->js->registration) {
		username = g_strdup_printf("%s@%s%s%s", cbdata->js->user->node, cbdata->js->user->domain,
			cbdata->js->user->resource ? "/" : "",
			cbdata->js->user->resource ? cbdata->js->user->resource : "");
		purple_account_set_username(purple_connection_get_account(cbdata->js->gc), username);
		g_free(username);
	}

	jabber_iq_set_callback(iq, jabber_registration_result_cb, cbdata->who);

	jabber_iq_send(iq);
	g_free(cbdata);
}

static void
jabber_register_cancel_cb(JabberRegisterCBData *cbdata, PurpleRequestFields *fields)
{
	PurpleAccount *account = purple_connection_get_account(cbdata->js->gc);
	if(account && cbdata->js->registration) {
		purple_account_register_completed(account, FALSE);
		jabber_connection_schedule_close(cbdata->js);
	}
	g_free(cbdata->who);
	g_free(cbdata);
}

static void jabber_register_x_data_cb(JabberStream *js, PurpleXmlNode *result, gpointer data)
{
	PurpleXmlNode *query;
	JabberIq *iq;
	char *to = data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");
	query = purple_xmlnode_get_child(iq->node, "query");
	if (to)
		purple_xmlnode_set_attrib(iq->node,"to",to);

	purple_xmlnode_insert_child(query, result);

	jabber_iq_set_callback(iq, jabber_registration_result_cb, to);
	jabber_iq_send(iq);
}

static const struct {
	const char *name;
	const char *label;
} registration_fields[] = {
	{ "email",   N_("Email") },
	{ "nick",    N_("Nickname") },
	{ "first",   N_("First name") },
	{ "last",    N_("Last name") },
	{ "address", N_("Address") },
	{ "city",    N_("City") },
	{ "state",   N_("State") },
	{ "zip",     N_("Postal code") },
	{ "phone",   N_("Phone") },
	{ "url",     N_("URL") },
	{ "date",    N_("Date") },
	{ NULL, NULL }
};

void jabber_register_parse(JabberStream *js, const char *from, JabberIqType type,
                           const char *id, PurpleXmlNode *query)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleXmlNode *x, *y, *node;
	char *instructions;
	JabberRegisterCBData *cbdata;
	gboolean registered = FALSE;
	int i;

	if (type != JABBER_IQ_RESULT)
		return;

	if(js->registration) {
		/* get rid of the login thingy */
		purple_connection_set_state(js->gc, PURPLE_CONNECTION_CONNECTED);
	}

	if(purple_xmlnode_get_child(query, "registered")) {
		registered = TRUE;

		if(js->registration) {
			purple_notify_error(NULL, _("Already Registered"),
				_("Already Registered"), NULL,
				purple_request_cpar_from_connection(js->gc));
			purple_account_register_completed(account, FALSE);
			jabber_connection_schedule_close(js);
			return;
		}
	}

	if((x = purple_xmlnode_get_child_with_namespace(query, "x", "jabber:x:data"))) {
		jabber_x_data_request(js, x, jabber_register_x_data_cb, g_strdup(from));
		return;

	} else if((x = purple_xmlnode_get_child_with_namespace(query, "x", NS_OOB_X_DATA))) {
		PurpleXmlNode *url;

		if((url = purple_xmlnode_get_child(x, "url"))) {
			char *href;
			if((href = purple_xmlnode_get_data(url))) {
				purple_notify_uri(NULL, href);
				g_free(href);

				if(js->registration) {
					/* succeeded, but we have no login info */
					purple_account_register_completed(account, TRUE);
					purple_connection_error(js->gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
							_("Registration completed successfully. Please reconnect to continue."));
					jabber_connection_schedule_close(js);
				}
				return;
			}
		}
	}

	/* as a last resort, use the old jabber:iq:register syntax */

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	if((node = purple_xmlnode_get_child(query, "username"))) {
		char *data = purple_xmlnode_get_data(node);
		if(js->registration)
			field = purple_request_field_string_new("username", _("Username"), data ? data : js->user->node, FALSE);
		else
			field = purple_request_field_string_new("username", _("Username"), data, FALSE);

		purple_request_field_group_add_field(group, field);
		g_free(data);
	}
	if((node = purple_xmlnode_get_child(query, "password"))) {
		if(js->registration)
			field = purple_request_field_string_new("password", _("Password"),
										purple_connection_get_password(js->gc), FALSE);
		else {
			char *data = purple_xmlnode_get_data(node);
			field = purple_request_field_string_new("password", _("Password"), data, FALSE);
			g_free(data);
		}

		purple_request_field_string_set_masked(field, TRUE);
		purple_request_field_group_add_field(group, field);
	}

	if((node = purple_xmlnode_get_child(query, "name"))) {
		if(js->registration)
			field = purple_request_field_string_new("name", _("Name"),
													purple_account_get_private_alias(purple_connection_get_account(js->gc)), FALSE);
		else {
			char *data = purple_xmlnode_get_data(node);
			field = purple_request_field_string_new("name", _("Name"), data, FALSE);
			g_free(data);
		}
		purple_request_field_group_add_field(group, field);
	}

	for (i = 0; registration_fields[i].name != NULL; ++i) {
		if ((node = purple_xmlnode_get_child(query, registration_fields[i].name))) {
			char *data = purple_xmlnode_get_data(node);
			field = purple_request_field_string_new(registration_fields[i].name,
			                                        _(registration_fields[i].label),
			                                        data, FALSE);
			purple_request_field_group_add_field(group, field);
			g_free(data);
		}
	}

	if(registered) {
		field = purple_request_field_bool_new("unregister", _("Unregister"), FALSE);
		purple_request_field_group_add_field(group, field);
	}

	if((y = purple_xmlnode_get_child(query, "instructions")))
		instructions = purple_xmlnode_get_data(y);
	else if(registered)
		instructions = g_strdup(_("Please fill out the information below "
					"to change your account registration."));
	else
		instructions = g_strdup(_("Please fill out the information below "
					"to register your new account."));

	cbdata = g_new0(JabberRegisterCBData, 1);
	cbdata->js = js;
	cbdata->who = g_strdup(from);

	if(js->registration)
		purple_request_fields(js->gc, _("Register New XMPP Account"),
				_("Register New XMPP Account"), instructions, fields,
				_("Register"), G_CALLBACK(jabber_register_cb),
				_("Cancel"), G_CALLBACK(jabber_register_cancel_cb),
				purple_request_cpar_from_connection(js->gc),
				cbdata);
	else {
		char *title;
		g_return_if_fail(from != NULL);
		title = registered ? g_strdup_printf(_("Change Account Registration at %s"), from)
								:g_strdup_printf(_("Register New Account at %s"), from);
		purple_request_fields(js->gc, title, title, instructions,
			fields, (registered ? _("Change Registration") :
			_("Register")), G_CALLBACK(jabber_register_cb),
			_("Cancel"), G_CALLBACK(jabber_register_cancel_cb),
			purple_request_cpar_from_connection(js->gc), cbdata);
		g_free(title);
	}

	g_free(instructions);
}

void jabber_register_start(JabberStream *js)
{
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:register");
	jabber_iq_send(iq);
}

void jabber_register_gateway(JabberStream *js, const char *gateway) {
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:register");
	purple_xmlnode_set_attrib(iq->node, "to", gateway);
	jabber_iq_send(iq);
}

void jabber_register_account(PurpleAccount *account)
{
	JabberStream *js;

	js = jabber_stream_new(account);
	if (js == NULL)
		return;

	js->registration = TRUE;
	jabber_stream_connect(js);
}

static void
jabber_unregister_account_iq_cb(JabberStream *js, const char *from,
                                JabberIqType type, const char *id,
                                PurpleXmlNode *packet, gpointer data)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);

	if (type == JABBER_IQ_ERROR) {
		char *msg = jabber_parse_error(js, packet, NULL);

		purple_notify_error(js->gc, _("Error unregistering account"),
			_("Error unregistering account"), msg,
			purple_request_cpar_from_connection(js->gc));
		g_free(msg);
		if(js->unregistration_cb)
			js->unregistration_cb(account, FALSE, js->unregistration_user_data);
	} else {
		purple_notify_info(js->gc, _("Account successfully "
			"unregistered"), _("Account successfully unregistered"),
			NULL, purple_request_cpar_from_connection(js->gc));
		if(js->unregistration_cb)
			js->unregistration_cb(account, TRUE, js->unregistration_user_data);
	}
}

static void jabber_unregister_account_cb(JabberStream *js) {
	JabberIq *iq;
	PurpleXmlNode *query;

	g_return_if_fail(js->unregistration);

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");

	query = purple_xmlnode_get_child_with_namespace(iq->node, "query", "jabber:iq:register");

	purple_xmlnode_new_child(query, "remove");
	purple_xmlnode_set_attrib(iq->node, "to", js->user->domain);

	jabber_iq_set_callback(iq, jabber_unregister_account_iq_cb, NULL);
	jabber_iq_send(iq);
}

void jabber_unregister_account(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data) {
	PurpleConnection *gc = purple_account_get_connection(account);
	JabberStream *js;

	if (purple_connection_get_state(gc) != PURPLE_CONNECTION_CONNECTED) {
		if (purple_connection_get_state(gc) != PURPLE_CONNECTION_CONNECTING)
			jabber_login(account);
		js = purple_connection_get_protocol_data(gc);
		js->unregistration = TRUE;
		js->unregistration_cb = cb;
		js->unregistration_user_data = user_data;
		return;
	}

	js = purple_connection_get_protocol_data(gc);

	if (js->unregistration) {
		purple_debug_error("jabber", "Unregistration in process; ignoring duplicate request.\n");
		return;
	}

	js->unregistration = TRUE;
	js->unregistration_cb = cb;
	js->unregistration_user_data = user_data;

	jabber_unregister_account_cb(js);
}

/* TODO: As Will pointed out in IRC, after being notified by the core to
 * shutdown, we should async. wait for the server to send us the stream
 * termination before destorying everything. That seems like it would require
 * changing the semantics of protocol's close(), so it's a good idea for 3.0.0.
 */
void jabber_close(PurpleConnection *gc)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);

	/* Close all of the open Jingle sessions on this stream */
	jingle_terminate_sessions(js);

	if (js->bosh) {
		jabber_bosh_connection_destroy(js->bosh);
		js->bosh = NULL;
	} else if ((js->gsc && js->gsc->fd > 0) || js->fd > 0)
		jabber_send_raw(js, "</stream:stream>", -1);

	if(js->gsc) {
		purple_ssl_close(js->gsc);
	} else if (js->fd > 0) {
		if(js->inpa) {
			purple_input_remove(js->inpa);
			js->inpa = 0;
		}
		close(js->fd);
	}

	jabber_buddy_remove_all_pending_buddy_info_requests(js);

	jabber_parser_free(js);

	if(js->iq_callbacks)
		g_hash_table_destroy(js->iq_callbacks);
	if(js->buddies)
		g_hash_table_destroy(js->buddies);
	if(js->chats)
		g_hash_table_destroy(js->chats);

	while(js->chat_servers) {
		g_free(js->chat_servers->data);
		js->chat_servers = g_list_delete_link(js->chat_servers, js->chat_servers);
	}

	while(js->user_directories) {
		g_free(js->user_directories->data);
		js->user_directories = g_list_delete_link(js->user_directories, js->user_directories);
	}

	while(js->bs_proxies) {
		JabberBytestreamsStreamhost *sh = js->bs_proxies->data;
		g_free(sh->jid);
		g_free(sh->host);
		g_free(sh->zeroconf);
		g_free(sh);
		js->bs_proxies = g_list_delete_link(js->bs_proxies, js->bs_proxies);
	}

	purple_http_connection_set_destroy(js->http_conns);

	g_free(js->stream_id);
	if(js->user)
		jabber_id_free(js->user);
	g_free(js->initial_avatar_hash);
	g_free(js->avatar_hash);
	g_free(js->caps_hash);

	if (js->write_buffer)
		g_object_unref(G_OBJECT(js->write_buffer));
	if(js->writeh)
		purple_input_remove(js->writeh);
	if (js->auth_mech && js->auth_mech->dispose)
		js->auth_mech->dispose(js);
#ifdef HAVE_CYRUS_SASL
	if(js->sasl)
		sasl_dispose(&js->sasl);
	if(js->sasl_mechs)
		g_string_free(js->sasl_mechs, TRUE);
	g_free(js->sasl_cb);
	/* Note: _not_ g_free.  See auth_cyrus.c:jabber_sasl_cb_secret */
	free(js->sasl_secret);
	g_free(js->sasl_password);
#endif
	g_free(js->serverFQDN);
	while(js->commands) {
		JabberAdHocCommands *cmd = js->commands->data;
		g_free(cmd->jid);
		g_free(cmd->node);
		g_free(cmd->name);
		g_free(cmd);
		js->commands = g_list_delete_link(js->commands, js->commands);
	}
	g_free(js->server_name);
	g_free(js->certificate_CN);
	g_free(js->gmail_last_time);
	g_free(js->gmail_last_tid);
	g_free(js->old_msg);
	g_free(js->old_avatarhash);
	g_free(js->old_artist);
	g_free(js->old_title);
	g_free(js->old_source);
	g_free(js->old_uri);
	g_free(js->old_track);

	if (js->vcard_timer != 0)
		purple_timeout_remove(js->vcard_timer);

	if (js->keepalive_timeout != 0)
		purple_timeout_remove(js->keepalive_timeout);
	if (js->inactivity_timer != 0)
		purple_timeout_remove(js->inactivity_timer);
	if (js->conn_close_timeout != 0)
		purple_timeout_remove(js->conn_close_timeout);

	g_cancellable_cancel(js->cancellable);
	g_object_unref(G_OBJECT(js->cancellable));

	g_free(js->stun_ip);

	/* remove Google relay-related stuff */
	g_free(js->google_relay_token);
	g_free(js->google_relay_host);

	g_free(js);

	purple_connection_set_protocol_data(gc, NULL);
}

void jabber_stream_set_state(JabberStream *js, JabberStreamState state)
{
#define JABBER_CONNECT_STEPS ((js->gsc || js->state == JABBER_STREAM_INITIALIZING_ENCRYPTION) ? 9 : 5)

	js->state = state;
	switch(state) {
		case JABBER_STREAM_OFFLINE:
			break;
		case JABBER_STREAM_CONNECTING:
			purple_connection_update_progress(js->gc, _("Connecting"), 1,
					JABBER_CONNECT_STEPS);
			break;
		case JABBER_STREAM_INITIALIZING:
			purple_connection_update_progress(js->gc, _("Initializing Stream"),
					js->gsc ? 5 : 2, JABBER_CONNECT_STEPS);
			jabber_stream_init(js);
			break;
		case JABBER_STREAM_INITIALIZING_ENCRYPTION:
			purple_connection_update_progress(js->gc, _("Initializing SSL/TLS"),
											  6, JABBER_CONNECT_STEPS);
			break;
		case JABBER_STREAM_AUTHENTICATING:
			purple_connection_update_progress(js->gc, _("Authenticating"),
					js->gsc ? 7 : 3, JABBER_CONNECT_STEPS);
			break;
		case JABBER_STREAM_POST_AUTH:
			purple_connection_update_progress(js->gc, _("Re-initializing Stream"),
					(js->gsc ? 8 : 4), JABBER_CONNECT_STEPS);

			break;
		case JABBER_STREAM_CONNECTED:
			/* Send initial presence */
			jabber_presence_send(js, TRUE);
			/* Start up the inactivity timer */
			jabber_stream_restart_inactivity_timer(js);

			purple_connection_set_state(js->gc, PURPLE_CONNECTION_CONNECTED);
			break;
	}

#undef JABBER_CONNECT_STEPS
}

char *jabber_get_next_id(JabberStream *js)
{
	return g_strdup_printf("purple%x", js->next_id++);
}


void jabber_idle_set(PurpleConnection *gc, int idle)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);

	js->idle = idle ? time(NULL) - idle : idle;

	/* send out an updated prescence */
	purple_debug_info("jabber", "sending updated presence for idle\n");
	jabber_presence_send(js, FALSE);
}

void jabber_blocklist_parse_push(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 PurpleXmlNode *child)
{
	JabberIq *result;
	PurpleXmlNode *item;
	PurpleAccount *account;
	gboolean is_block;
	GSList *deny;

	if (!jabber_is_own_account(js, from)) {
		PurpleXmlNode *error, *x;
		result = jabber_iq_new(js, JABBER_IQ_ERROR);
		purple_xmlnode_set_attrib(result->node, "id", id);
		if (from)
			purple_xmlnode_set_attrib(result->node, "to", from);

		error = purple_xmlnode_new_child(result->node, "error");
		purple_xmlnode_set_attrib(error, "type", "cancel");
		x = purple_xmlnode_new_child(error, "not-allowed");
		purple_xmlnode_set_namespace(x, NS_XMPP_STANZAS);

		jabber_iq_send(result);
		return;
	}

	account = purple_connection_get_account(js->gc);
	is_block = g_str_equal(child->name, "block");

	item = purple_xmlnode_get_child(child, "item");
	if (!is_block && item == NULL) {
		/* Unblock everyone */
		purple_debug_info("jabber", "Received unblock push. Unblocking everyone.\n");

		while ((deny = purple_account_privacy_get_denied(account)) != NULL) {
			purple_account_privacy_deny_remove(account, deny->data, TRUE);
		}
	} else if (item == NULL) {
		/* An empty <block/> is bogus */
		PurpleXmlNode *error, *x;
		result = jabber_iq_new(js, JABBER_IQ_ERROR);
		purple_xmlnode_set_attrib(result->node, "id", id);

		error = purple_xmlnode_new_child(result->node, "error");
		purple_xmlnode_set_attrib(error, "type", "modify");
		x = purple_xmlnode_new_child(error, "bad-request");
		purple_xmlnode_set_namespace(x, NS_XMPP_STANZAS);

		jabber_iq_send(result);
		return;
	} else {
		for ( ; item; item = purple_xmlnode_get_next_twin(item)) {
			const char *jid = purple_xmlnode_get_attrib(item, "jid");
			if (jid == NULL || *jid == '\0')
				continue;

			if (is_block)
				purple_account_privacy_deny_add(account, jid, TRUE);
			else
				purple_account_privacy_deny_remove(account, jid, TRUE);
		}
	}

	result = jabber_iq_new(js, JABBER_IQ_RESULT);
	purple_xmlnode_set_attrib(result->node, "id", id);
	jabber_iq_send(result);
}

static void jabber_blocklist_parse(JabberStream *js, const char *from,
                                   JabberIqType type, const char *id,
                                   PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *blocklist, *item;
	PurpleAccount *account;
	GSList *deny;

	blocklist = purple_xmlnode_get_child_with_namespace(packet,
			"blocklist", NS_SIMPLE_BLOCKING);
	account = purple_connection_get_account(js->gc);

	if (type == JABBER_IQ_ERROR || blocklist == NULL)
		return;

	/* This is the only privacy method supported by XEP-0191 */
	purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_DENY_USERS);

	/*
	 * TODO: When account->deny is something more than a hash table, this can
	 * be re-written to find the set intersection and difference.
	 */
	while ((deny = purple_account_privacy_get_denied(account)))
		purple_account_privacy_deny_remove(account, deny->data, TRUE);

	item = purple_xmlnode_get_child(blocklist, "item");
	while (item != NULL) {
		const char *jid = purple_xmlnode_get_attrib(item, "jid");
		purple_account_privacy_deny_add(account, jid, TRUE);
		item = purple_xmlnode_get_next_twin(item);
	}
}

void jabber_request_block_list(JabberStream *js)
{
	JabberIq *iq;
	PurpleXmlNode *blocklist;

	iq = jabber_iq_new(js, JABBER_IQ_GET);

	blocklist = purple_xmlnode_new_child(iq->node, "blocklist");
	purple_xmlnode_set_namespace(blocklist, NS_SIMPLE_BLOCKING);

	jabber_iq_set_callback(iq, jabber_blocklist_parse, NULL);

	jabber_iq_send(iq);
}

void jabber_add_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	JabberIq *iq;
	PurpleXmlNode *block, *item;

	g_return_if_fail(who != NULL && *who != '\0');

	js = purple_connection_get_protocol_data(gc);
	if (js == NULL)
		return;

	if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
	{
		jabber_google_roster_add_deny(js, who);
		return;
	}

	if (!(js->server_caps & JABBER_CAP_BLOCKING))
	{
		purple_notify_error(NULL, _("Server doesn't support blocking"),
			_("Server doesn't support blocking"), NULL,
			purple_request_cpar_from_connection(gc));
		return;
	}

	iq = jabber_iq_new(js, JABBER_IQ_SET);

	block = purple_xmlnode_new_child(iq->node, "block");
	purple_xmlnode_set_namespace(block, NS_SIMPLE_BLOCKING);

	item = purple_xmlnode_new_child(block, "item");
	purple_xmlnode_set_attrib(item, "jid", who);

	jabber_iq_send(iq);
}

void jabber_rem_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	JabberIq *iq;
	PurpleXmlNode *unblock, *item;

	g_return_if_fail(who != NULL && *who != '\0');

	js = purple_connection_get_protocol_data(gc);
	if (js == NULL)
		return;

	if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
	{
		jabber_google_roster_rem_deny(js, who);
		return;
	}

	if (!(js->server_caps & JABBER_CAP_BLOCKING))
		return;

	iq = jabber_iq_new(js, JABBER_IQ_SET);

	unblock = purple_xmlnode_new_child(iq->node, "unblock");
	purple_xmlnode_set_namespace(unblock, NS_SIMPLE_BLOCKING);

	item = purple_xmlnode_new_child(unblock, "item");
	purple_xmlnode_set_attrib(item, "jid", who);

	jabber_iq_send(iq);
}

void jabber_add_feature(const char *namespace, JabberFeatureEnabled cb) {
	JabberFeature *feat;

	g_return_if_fail(namespace != NULL);

	feat = g_new0(JabberFeature,1);
	feat->namespace = g_strdup(namespace);
	feat->is_enabled = cb;

	/* try to remove just in case it already exists in the list */
	jabber_remove_feature(namespace);

	jabber_features = g_list_append(jabber_features, feat);
}

void jabber_remove_feature(const char *namespace) {
	GList *feature;
	for(feature = jabber_features; feature; feature = feature->next) {
		JabberFeature *feat = (JabberFeature*)feature->data;
		if(!strcmp(feat->namespace, namespace)) {
			g_free(feat->namespace);
			g_free(feature->data);
			jabber_features = g_list_delete_link(jabber_features, feature);
			break;
		}
	}
}

static void jabber_features_destroy(void)
{
	while (jabber_features) {
		JabberFeature *feature = jabber_features->data;
		g_free(feature->namespace);
		g_free(feature);
		jabber_features = g_list_delete_link(jabber_features, jabber_features);
	}
}

gint
jabber_identity_compare(gconstpointer a, gconstpointer b)
{
	const JabberIdentity *ac;
	const JabberIdentity *bc;
	gint cat_cmp;
	gint typ_cmp;

	ac = a;
	bc = b;

	if ((cat_cmp = strcmp(ac->category, bc->category)) == 0) {
		if ((typ_cmp = strcmp(ac->type, bc->type)) == 0) {
			if (!ac->lang && !bc->lang) {
				return 0;
			} else if (ac->lang && !bc->lang) {
				return 1;
			} else if (!ac->lang && bc->lang) {
				return -1;
			} else {
				return strcmp(ac->lang, bc->lang);
			}
		} else {
			return typ_cmp;
		}
	} else {
		return cat_cmp;
	}
}

void jabber_add_identity(const gchar *category, const gchar *type,
                         const gchar *lang, const gchar *name)
{
	GList *identity;
	JabberIdentity *ident;

	/* both required according to XEP-0030 */
	g_return_if_fail(category != NULL);
	g_return_if_fail(type != NULL);

	/* Check if this identity is already there... */
	for (identity = jabber_identities; identity; identity = identity->next) {
		JabberIdentity *id = identity->data;
		if (g_str_equal(id->category, category) &&
			g_str_equal(id->type, type) &&
			purple_strequal(id->lang, lang))
			return;
	}

	ident = g_new0(JabberIdentity, 1);
	ident->category = g_strdup(category);
	ident->type = g_strdup(type);
	ident->lang = g_strdup(lang);
	ident->name = g_strdup(name);
	jabber_identities = g_list_insert_sorted(jabber_identities, ident,
	                                         jabber_identity_compare);
}

static void jabber_identities_destroy(void)
{
	while (jabber_identities) {
		JabberIdentity *id = jabber_identities->data;
		g_free(id->category);
		g_free(id->type);
		g_free(id->lang);
		g_free(id->name);
		g_free(id);
		jabber_identities = g_list_delete_link(jabber_identities, jabber_identities);
	}
}

gboolean jabber_stream_is_ssl(JabberStream *js)
{
	return (js->bosh && jabber_bosh_connection_is_ssl(js->bosh)) ||
	       (!js->bosh && js->gsc);
}

static gboolean
inactivity_cb(gpointer data)
{
	JabberStream *js = data;

	/* We want whatever is sent to set this.  It's okay because
	 * the eventloop unsets it via the return FALSE.
	 */
	js->inactivity_timer = 0;

	if (js->bosh)
		jabber_bosh_connection_send_keepalive(js->bosh);
	else
		jabber_send_raw(js, "\t", 1);

	return FALSE;
}

void jabber_stream_restart_inactivity_timer(JabberStream *js)
{
	if (js->inactivity_timer != 0) {
		purple_timeout_remove(js->inactivity_timer);
		js->inactivity_timer = 0;
	}

	g_return_if_fail(js->max_inactivity > 0);

	js->inactivity_timer =
		purple_timeout_add_seconds(js->max_inactivity,
		                           inactivity_cb, js);
}

const char *jabber_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "jabber";
}

const char* jabber_list_emblem(PurpleBuddy *b)
{
	JabberStream *js;
	JabberBuddy *jb = NULL;
	PurpleConnection *gc = purple_account_get_connection(purple_buddy_get_account(b));

	if(!gc)
		return NULL;

	js = purple_connection_get_protocol_data(gc);
	if(js)
		jb = jabber_buddy_find(js, purple_buddy_get_name(b), FALSE);

	if(!PURPLE_BUDDY_IS_ONLINE(b)) {
		if(jb && (jb->subscription & JABBER_SUB_PENDING ||
					!(jb->subscription & JABBER_SUB_TO)))
			return "not-authorized";
	}

	if (jb) {
		JabberBuddyResource *jbr = jabber_buddy_find_resource(jb, NULL);
		if (jbr) {
			const gchar *client_type =
				jabber_resource_get_identity_category_type(jbr, "client");

			if (client_type) {
				if (strcmp(client_type, "phone") == 0) {
					return "mobile";
				} else if (strcmp(client_type, "web") == 0) {
					return "external";
				} else if (strcmp(client_type, "handheld") == 0) {
					return "hiptop";
				} else if (strcmp(client_type, "bot") == 0) {
					return "bot";
				}
				/* the default value "pc" falls through and has no emblem */
			}
		}
	}

	return NULL;
}

char *jabber_status_text(PurpleBuddy *b)
{
	char *ret = NULL;
	JabberBuddy *jb = NULL;
	PurpleAccount *account = purple_buddy_get_account(b);
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc && purple_connection_get_protocol_data(gc))
		jb = jabber_buddy_find(purple_connection_get_protocol_data(gc), purple_buddy_get_name(b), FALSE);

	if(jb && !PURPLE_BUDDY_IS_ONLINE(b) && (jb->subscription & JABBER_SUB_PENDING || !(jb->subscription & JABBER_SUB_TO))) {
		ret = g_strdup(_("Not Authorized"));
	} else if(jb && !PURPLE_BUDDY_IS_ONLINE(b) && jb->error_msg) {
		ret = g_strdup(jb->error_msg);
	} else {
		PurplePresence *presence = purple_buddy_get_presence(b);
		PurpleStatus *status = purple_presence_get_active_status(presence);
		const char *message;

		if((message = purple_status_get_attr_string(status, "message"))) {
			ret = g_markup_escape_text(message, -1);
		} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_TUNE)) {
			PurpleStatus *status = purple_presence_get_status(presence, "tune");
			const char *title = purple_status_get_attr_string(status, PURPLE_TUNE_TITLE);
			const char *artist = purple_status_get_attr_string(status, PURPLE_TUNE_ARTIST);
			const char *album = purple_status_get_attr_string(status, PURPLE_TUNE_ALBUM);
			ret = purple_util_format_song_info(title, artist, album, NULL);
		}
	}

	return ret;
}

static void
jabber_tooltip_add_resource_text(JabberBuddyResource *jbr,
	PurpleNotifyUserInfo *user_info, gboolean multiple_resources)
{
	char *text = NULL;
	char *res = NULL;
	char *label, *value;
	const char *state;

	if(jbr->status) {
		text = g_markup_escape_text(jbr->status, -1);
	}

	if(jbr->name)
		res = g_strdup_printf(" (%s)", jbr->name);

	state = jabber_buddy_state_get_name(jbr->state);
	if (text != NULL && !purple_utf8_strcasecmp(state, text)) {
		g_free(text);
		text = NULL;
	}

	label = g_strdup_printf("%s%s", _("Status"), (res ? res : ""));
	value = g_strdup_printf("%s%s%s", state, (text ? ": " : ""), (text ? text : ""));

	purple_notify_user_info_add_pair_html(user_info, label, value);
	g_free(label);
	g_free(value);
	g_free(text);

	/* if the resource is idle, show that */
	/* only show it if there is more than one resource available for
	the buddy, since the "general" idleness will be shown anyway,
	this way we can see see the idleness of lower-priority resources */
	if (jbr->idle && multiple_resources) {
		gchar *idle_str =
			purple_str_seconds_to_string(time(NULL) - jbr->idle);
		label = g_strdup_printf("%s%s", _("Idle"), (res ? res : ""));
		purple_notify_user_info_add_pair_plaintext(user_info, label, idle_str);
		g_free(idle_str);
		g_free(label);
	}
	g_free(res);
}

void jabber_tooltip_text(PurpleBuddy *b, PurpleNotifyUserInfo *user_info, gboolean full)
{
	JabberBuddy *jb;
	PurpleAccount *account;
	PurpleConnection *gc;
	JabberStream *js;

	g_return_if_fail(b != NULL);

	account = purple_buddy_get_account(b);
	g_return_if_fail(account != NULL);

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	js = purple_connection_get_protocol_data(gc);
	g_return_if_fail(js != NULL);

	jb = jabber_buddy_find(js, purple_buddy_get_name(b), FALSE);

	if(jb) {
		JabberBuddyResource *jbr = NULL;
		PurplePresence *presence = purple_buddy_get_presence(b);
		const char *sub;
		GList *l;
		const char *mood;
		gboolean multiple_resources =
			jb->resources && g_list_next(jb->resources);
		JabberBuddyResource *top_jbr = jabber_buddy_find_resource(jb, NULL);

		/* resource-specific info for the top resource */
		if (top_jbr) {
			jabber_tooltip_add_resource_text(top_jbr, user_info,
				multiple_resources);
		}

		for(l=jb->resources; l; l = l->next) {
			jbr = l->data;
			/* the remaining resources */
			if (jbr != top_jbr) {
				jabber_tooltip_add_resource_text(jbr, user_info,
					multiple_resources);
			}
		}

		if (full) {
			PurpleStatus *status;

			status = purple_presence_get_status(presence, "mood");
			mood = purple_status_get_attr_string(status, PURPLE_MOOD_NAME);
			if(mood && *mood) {
				const char *moodtext;
				/* find the mood */
				PurpleMood *moods = jabber_get_moods(account);
				const char *description = NULL;

				for (; moods->mood ; moods++) {
					if (purple_strequal(moods->mood, mood)) {
						description = moods->description;
						break;
					}
				}

				moodtext = purple_status_get_attr_string(status, PURPLE_MOOD_COMMENT);
				if(moodtext && *moodtext) {
					char *moodplustext =
						g_strdup_printf("%s (%s)", description ? _(description) : mood, moodtext);

					purple_notify_user_info_add_pair_html(user_info, _("Mood"), moodplustext);
					g_free(moodplustext);
				} else
					purple_notify_user_info_add_pair_html(user_info, _("Mood"),
					    description ? _(description) : mood);
			}
			if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_TUNE)) {
				PurpleStatus *tune = purple_presence_get_status(presence, "tune");
				const char *title = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
				const char *artist = purple_status_get_attr_string(tune, PURPLE_TUNE_ARTIST);
				const char *album = purple_status_get_attr_string(tune, PURPLE_TUNE_ALBUM);
				char *playing = purple_util_format_song_info(title, artist, album, NULL);
				if (playing) {
					purple_notify_user_info_add_pair_html(user_info, _("Now Listening"), playing);
					g_free(playing);
				}
			}

			if(jb->subscription & JABBER_SUB_FROM) {
				if(jb->subscription & JABBER_SUB_TO)
					sub = _("Both");
				else if(jb->subscription & JABBER_SUB_PENDING)
					sub = _("From (To pending)");
				else
					sub = _("From");
			} else {
				if(jb->subscription & JABBER_SUB_TO)
					sub = _("To");
				else if(jb->subscription & JABBER_SUB_PENDING)
					sub = _("None (To pending)");
				else
					sub = _("None");
			}

			purple_notify_user_info_add_pair_html(user_info, _("Subscription"), sub);

		}

		if(!PURPLE_BUDDY_IS_ONLINE(b) && jb->error_msg) {
			purple_notify_user_info_add_pair_html(user_info, _("Error"), jb->error_msg);
		}
	}
}

GList *jabber_status_types(PurpleAccount *account)
{
	PurpleStatusType *type;
	GList *types = NULL;
	GValue *priority_value;
	GValue *buzz_enabled;

	priority_value = purple_value_new(G_TYPE_INT);
	g_value_set_int(priority_value, 1);
	buzz_enabled = purple_value_new(G_TYPE_BOOLEAN);
	g_value_set_boolean(buzz_enabled, TRUE);
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
			jabber_buddy_state_get_status_id(JABBER_BUDDY_STATE_ONLINE),
			NULL, TRUE, TRUE, FALSE,
			"priority", _("Priority"), priority_value,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			"mood", _("Mood"), purple_value_new(G_TYPE_STRING),
			"moodtext", _("Mood Text"), purple_value_new(G_TYPE_STRING),
			"nick", _("Nickname"), purple_value_new(G_TYPE_STRING),
			"buzz", _("Allow Buzz"), buzz_enabled,
			NULL);
	types = g_list_prepend(types, type);


	type = purple_status_type_new_with_attrs(PURPLE_STATUS_MOOD,
	    "mood", NULL, TRUE, TRUE, TRUE,
			PURPLE_MOOD_NAME, _("Mood Name"), purple_value_new(G_TYPE_STRING),
			PURPLE_MOOD_COMMENT, _("Mood Comment"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_prepend(types, type);

	priority_value = purple_value_new(G_TYPE_INT);
	g_value_set_int(priority_value, 1);
	buzz_enabled = purple_value_new(G_TYPE_BOOLEAN);
	g_value_set_boolean(buzz_enabled, TRUE);
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
			jabber_buddy_state_get_status_id(JABBER_BUDDY_STATE_CHAT),
			_("Chatty"), TRUE, TRUE, FALSE,
			"priority", _("Priority"), priority_value,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			"mood", _("Mood"), purple_value_new(G_TYPE_STRING),
			"moodtext", _("Mood Text"), purple_value_new(G_TYPE_STRING),
			"nick", _("Nickname"), purple_value_new(G_TYPE_STRING),
			"buzz", _("Allow Buzz"), buzz_enabled,
			NULL);
	types = g_list_prepend(types, type);

	priority_value = purple_value_new(G_TYPE_INT);
	g_value_set_int(priority_value, 0);
	buzz_enabled = purple_value_new(G_TYPE_BOOLEAN);
	g_value_set_boolean(buzz_enabled, TRUE);
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY,
			jabber_buddy_state_get_status_id(JABBER_BUDDY_STATE_AWAY),
			NULL, TRUE, TRUE, FALSE,
			"priority", _("Priority"), priority_value,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			"mood", _("Mood"), purple_value_new(G_TYPE_STRING),
			"moodtext", _("Mood Text"), purple_value_new(G_TYPE_STRING),
			"nick", _("Nickname"), purple_value_new(G_TYPE_STRING),
			"buzz", _("Allow Buzz"), buzz_enabled,
			NULL);
	types = g_list_prepend(types, type);

	priority_value = purple_value_new(G_TYPE_INT);
	g_value_set_int(priority_value, 0);
	buzz_enabled = purple_value_new(G_TYPE_BOOLEAN);
	g_value_set_boolean(buzz_enabled, TRUE);
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_EXTENDED_AWAY,
			jabber_buddy_state_get_status_id(JABBER_BUDDY_STATE_XA),
			NULL, TRUE, TRUE, FALSE,
			"priority", _("Priority"), priority_value,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			"mood", _("Mood"), purple_value_new(G_TYPE_STRING),
			"moodtext", _("Mood Text"), purple_value_new(G_TYPE_STRING),
			"nick", _("Nickname"), purple_value_new(G_TYPE_STRING),
			"buzz", _("Allow Buzz"), buzz_enabled,
			NULL);
	types = g_list_prepend(types, type);

	priority_value = purple_value_new(G_TYPE_INT);
	g_value_set_int(priority_value, 0);
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE,
			jabber_buddy_state_get_status_id(JABBER_BUDDY_STATE_DND),
			_("Do Not Disturb"), TRUE, TRUE, FALSE,
			"priority", _("Priority"), priority_value,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			"mood", _("Mood"), purple_value_new(G_TYPE_STRING),
			"moodtext", _("Mood Text"), purple_value_new(G_TYPE_STRING),
			"nick", _("Nickname"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_prepend(types, type);

	/*
	if(js->protocol_version == JABBER_PROTO_0_9)
		"Invisible"
	*/

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE,
			jabber_buddy_state_get_status_id(JABBER_BUDDY_STATE_UNAVAILABLE),
			NULL, TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_prepend(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_TUNE,
			"tune", NULL, FALSE, TRUE, TRUE,
			PURPLE_TUNE_ARTIST, _("Tune Artist"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_TITLE, _("Tune Title"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_ALBUM, _("Tune Album"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_GENRE, _("Tune Genre"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_COMMENT, _("Tune Comment"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_TRACK, _("Tune Track"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_TIME, _("Tune Time"), purple_value_new(G_TYPE_INT),
			PURPLE_TUNE_YEAR, _("Tune Year"), purple_value_new(G_TYPE_INT),
			PURPLE_TUNE_URL, _("Tune URL"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_prepend(types, type);

	return g_list_reverse(types);
}

static void
jabber_password_change_result_cb(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 PurpleXmlNode *packet, gpointer data)
{
	if (type == JABBER_IQ_RESULT) {
		purple_notify_info(js->gc, _("Password Changed"), _("Password "
			"Changed"), _("Your password has been changed."),
			purple_request_cpar_from_connection(js->gc));

		purple_account_set_password(purple_connection_get_account(js->gc), (const char *)data, NULL, NULL);
	} else {
		char *msg = jabber_parse_error(js, packet, NULL);

		purple_notify_error(js->gc, _("Error changing password"),
			_("Error changing password"), msg,
			purple_request_cpar_from_connection(js->gc));
		g_free(msg);
	}

	g_free(data);
}

static void jabber_password_change_cb(JabberStream *js,
		PurpleRequestFields *fields)
{
	const char *p1, *p2;
	JabberIq *iq;
	PurpleXmlNode *query, *y;

	p1 = purple_request_fields_get_string(fields, "password1");
	p2 = purple_request_fields_get_string(fields, "password2");

	if(strcmp(p1, p2)) {
		purple_notify_error(js->gc, NULL,
			_("New passwords do not match."), NULL,
			purple_request_cpar_from_connection(js->gc));
		return;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");

	purple_xmlnode_set_attrib(iq->node, "to", js->user->domain);

	query = purple_xmlnode_get_child(iq->node, "query");

	y = purple_xmlnode_new_child(query, "username");
	purple_xmlnode_insert_data(y, js->user->node, -1);
	y = purple_xmlnode_new_child(query, "password");
	purple_xmlnode_insert_data(y, p1, -1);

	jabber_iq_set_callback(iq, jabber_password_change_result_cb, g_strdup(p1));

	jabber_iq_send(iq);
}

static void jabber_password_change(PurpleProtocolAction *action)
{

	PurpleConnection *gc = (PurpleConnection *) action->connection;
	JabberStream *js = purple_connection_get_protocol_data(gc);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password1", _("Password"),
			"", FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("password2", _("Password (again)"),
			"", FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(js->gc, _("Change XMPP Password"),
			_("Change XMPP Password"), _("Please enter your new password"),
			fields, _("OK"), G_CALLBACK(jabber_password_change_cb),
			_("Cancel"), NULL,
			purple_request_cpar_from_connection(gc), js);
}

GList *jabber_get_actions(PurpleConnection *gc)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);
	GList *m = NULL;
	PurpleProtocolAction *act;

	act = purple_protocol_action_new(_("Set User Info..."),
	                             jabber_setup_set_info);
	m = g_list_append(m, act);

	/* if (js->account_options & CHANGE_PASSWORD) { */
		act = purple_protocol_action_new(_("Change Password..."),
		                             jabber_password_change);
		m = g_list_append(m, act);
	/* } */

	act = purple_protocol_action_new(_("Search for Users..."),
	                             jabber_user_search_begin);
	m = g_list_append(m, act);

	purple_debug_info("jabber", "jabber_get_actions: have pep: %s\n", js->pep?"YES":"NO");

	if(js->pep)
		jabber_pep_init_actions(&m);

	if(js->commands)
		jabber_adhoc_init_server_commands(js, &m);

	return m;
}

PurpleChat *jabber_find_blist_chat(PurpleAccount *account, const char *name)
{
	PurpleBlistNode *gnode, *cnode;
	JabberID *jid;

	if(!(jid = jabber_id_new(name)))
		return NULL;

	for(gnode = purple_blist_get_root(); gnode;
			gnode = purple_blist_node_get_sibling_next(gnode)) {
		for(cnode = purple_blist_node_get_first_child(gnode);
				cnode;
				cnode = purple_blist_node_get_sibling_next(cnode)) {
			PurpleChat *chat = (PurpleChat*)cnode;
			const char *room, *server;
			GHashTable *components;
			if(!PURPLE_IS_CHAT(cnode))
				continue;

			if (purple_chat_get_account(chat) != account)
				continue;

			components = purple_chat_get_components(chat);
			if(!(room = g_hash_table_lookup(components, "room")))
				continue;
			if(!(server = g_hash_table_lookup(components, "server")))
				continue;

			/* FIXME: Collate is wrong in a few cases here; this should be prepped */
			if(jid->node && jid->domain &&
					!g_utf8_collate(room, jid->node) && !g_utf8_collate(server, jid->domain)) {
				jabber_id_free(jid);
				return chat;
			}
		}
	}
	jabber_id_free(jid);
	return NULL;
}

void jabber_convo_closed(PurpleConnection *gc, const char *who)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);
	JabberID *jid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;

	if(!(jid = jabber_id_new(who)))
		return;

	if((jb = jabber_buddy_find(js, who, TRUE)) &&
			(jbr = jabber_buddy_find_resource(jb, jid->resource))) {
		g_free(jbr->thread_id);
		jbr->thread_id = NULL;
	}

	jabber_id_free(jid);
}


char *jabber_parse_error(JabberStream *js,
                         PurpleXmlNode *packet,
                         PurpleConnectionError *reason)
{
	PurpleXmlNode *error;
	const char *code = NULL, *text = NULL;
	const char *xmlns = purple_xmlnode_get_namespace(packet);
	char *cdata = NULL;

#define SET_REASON(x) \
	if(reason != NULL) { *reason = x; }

	if((error = purple_xmlnode_get_child(packet, "error"))) {
		PurpleXmlNode *t = purple_xmlnode_get_child_with_namespace(error, "text", NS_XMPP_STANZAS);
		if (t)
			cdata = purple_xmlnode_get_data(t);
#if 0
		cdata = purple_xmlnode_get_data(error);
#endif
		code = purple_xmlnode_get_attrib(error, "code");

		/* Stanza errors */
		if(purple_xmlnode_get_child(error, "bad-request")) {
			text = _("Bad Request");
		} else if(purple_xmlnode_get_child(error, "conflict")) {
			SET_REASON(PURPLE_CONNECTION_ERROR_NAME_IN_USE);
			text = _("Conflict");
		} else if(purple_xmlnode_get_child(error, "feature-not-implemented")) {
			text = _("Feature Not Implemented");
		} else if(purple_xmlnode_get_child(error, "forbidden")) {
			text = _("Forbidden");
		} else if(purple_xmlnode_get_child(error, "gone")) {
			text = _("Gone");
		} else if(purple_xmlnode_get_child(error, "internal-server-error")) {
			text = _("Internal Server Error");
		} else if(purple_xmlnode_get_child(error, "item-not-found")) {
			text = _("Item Not Found");
		} else if(purple_xmlnode_get_child(error, "jid-malformed")) {
			text = _("Malformed XMPP ID");
		} else if(purple_xmlnode_get_child(error, "not-acceptable")) {
			text = _("Not Acceptable");
		} else if(purple_xmlnode_get_child(error, "not-allowed")) {
			text = _("Not Allowed");
		} else if(purple_xmlnode_get_child(error, "not-authorized")) {
			text = _("Not Authorized");
		} else if(purple_xmlnode_get_child(error, "payment-required")) {
			text = _("Payment Required");
		} else if(purple_xmlnode_get_child(error, "recipient-unavailable")) {
			text = _("Recipient Unavailable");
		} else if(purple_xmlnode_get_child(error, "redirect")) {
			/* XXX */
		} else if(purple_xmlnode_get_child(error, "registration-required")) {
			text = _("Registration Required");
		} else if(purple_xmlnode_get_child(error, "remote-server-not-found")) {
			text = _("Remote Server Not Found");
		} else if(purple_xmlnode_get_child(error, "remote-server-timeout")) {
			text = _("Remote Server Timeout");
		} else if(purple_xmlnode_get_child(error, "resource-constraint")) {
			text = _("Server Overloaded");
		} else if(purple_xmlnode_get_child(error, "service-unavailable")) {
			text = _("Service Unavailable");
		} else if(purple_xmlnode_get_child(error, "subscription-required")) {
			text = _("Subscription Required");
		} else if(purple_xmlnode_get_child(error, "unexpected-request")) {
			text = _("Unexpected Request");
		} else if(purple_xmlnode_get_child(error, "undefined-condition")) {
			text = _("Unknown Error");
		}
	} else if(xmlns && !strcmp(xmlns, NS_XMPP_SASL)) {
		/* Most common reason can be the default */
		SET_REASON(PURPLE_CONNECTION_ERROR_NETWORK_ERROR);
		if(purple_xmlnode_get_child(packet, "aborted")) {
			text = _("Authorization Aborted");
		} else if(purple_xmlnode_get_child(packet, "incorrect-encoding")) {
			text = _("Incorrect encoding in authorization");
		} else if(purple_xmlnode_get_child(packet, "invalid-authzid")) {
			text = _("Invalid authzid");
		} else if(purple_xmlnode_get_child(packet, "invalid-mechanism")) {
			text = _("Invalid Authorization Mechanism");
		} else if(purple_xmlnode_get_child(packet, "mechanism-too-weak")) {
			SET_REASON(PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE);
			text = _("Authorization mechanism too weak");
		} else if(purple_xmlnode_get_child(packet, "not-authorized")) {
			SET_REASON(PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED);
			/* Clear the pasword if it isn't being saved */
			if (!purple_account_get_remember_password(purple_connection_get_account(js->gc)))
				purple_account_set_password(purple_connection_get_account(js->gc), NULL, NULL, NULL);
			text = _("Not Authorized");
		} else if(purple_xmlnode_get_child(packet, "temporary-auth-failure")) {
			text = _("Temporary Authentication Failure");
		} else {
			SET_REASON(PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED);
			text = _("Authentication Failure");
		}
	} else if(!strcmp(packet->name, "stream:error") ||
			 (!strcmp(packet->name, "error") && xmlns &&
				!strcmp(xmlns, NS_XMPP_STREAMS))) {
		/* Most common reason as default: */
		SET_REASON(PURPLE_CONNECTION_ERROR_NETWORK_ERROR);
		if(purple_xmlnode_get_child(packet, "bad-format")) {
			text = _("Bad Format");
		} else if(purple_xmlnode_get_child(packet, "bad-namespace-prefix")) {
			text = _("Bad Namespace Prefix");
		} else if(purple_xmlnode_get_child(packet, "conflict")) {
			SET_REASON(PURPLE_CONNECTION_ERROR_NAME_IN_USE);
			text = _("Resource Conflict");
		} else if(purple_xmlnode_get_child(packet, "connection-timeout")) {
			text = _("Connection Timeout");
		} else if(purple_xmlnode_get_child(packet, "host-gone")) {
			text = _("Host Gone");
		} else if(purple_xmlnode_get_child(packet, "host-unknown")) {
			text = _("Host Unknown");
		} else if(purple_xmlnode_get_child(packet, "improper-addressing")) {
			text = _("Improper Addressing");
		} else if(purple_xmlnode_get_child(packet, "internal-server-error")) {
			text = _("Internal Server Error");
		} else if(purple_xmlnode_get_child(packet, "invalid-id")) {
			text = _("Invalid ID");
		} else if(purple_xmlnode_get_child(packet, "invalid-namespace")) {
			text = _("Invalid Namespace");
		} else if(purple_xmlnode_get_child(packet, "invalid-xml")) {
			text = _("Invalid XML");
		} else if(purple_xmlnode_get_child(packet, "nonmatching-hosts")) {
			text = _("Non-matching Hosts");
		} else if(purple_xmlnode_get_child(packet, "not-authorized")) {
			text = _("Not Authorized");
		} else if(purple_xmlnode_get_child(packet, "policy-violation")) {
			text = _("Policy Violation");
		} else if(purple_xmlnode_get_child(packet, "remote-connection-failed")) {
			text = _("Remote Connection Failed");
		} else if(purple_xmlnode_get_child(packet, "resource-constraint")) {
			text = _("Resource Constraint");
		} else if(purple_xmlnode_get_child(packet, "restricted-xml")) {
			text = _("Restricted XML");
		} else if(purple_xmlnode_get_child(packet, "see-other-host")) {
			text = _("See Other Host");
		} else if(purple_xmlnode_get_child(packet, "system-shutdown")) {
			text = _("System Shutdown");
		} else if(purple_xmlnode_get_child(packet, "undefined-condition")) {
			text = _("Undefined Condition");
		} else if(purple_xmlnode_get_child(packet, "unsupported-encoding")) {
			text = _("Unsupported Encoding");
		} else if(purple_xmlnode_get_child(packet, "unsupported-stanza-type")) {
			text = _("Unsupported Stanza Type");
		} else if(purple_xmlnode_get_child(packet, "unsupported-version")) {
			text = _("Unsupported Version");
		} else if(purple_xmlnode_get_child(packet, "xml-not-well-formed")) {
			text = _("XML Not Well Formed");
		} else {
			text = _("Stream Error");
		}
	}

#undef SET_REASON

	if(text || cdata) {
		char *ret = g_strdup_printf("%s%s%s", code ? code : "",
				code ? ": " : "", text ? text : cdata);
		g_free(cdata);
		return ret;
	} else {
		return NULL;
	}
}

static PurpleCmdRet jabber_cmd_chat_config(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if (!chat)
		return PURPLE_CMD_RET_FAILED;

	jabber_chat_request_room_configure(chat);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_register(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if (!chat)
		return PURPLE_CMD_RET_FAILED;

	jabber_chat_register(chat);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_topic(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if (!chat)
		return PURPLE_CMD_RET_FAILED;

	if (args && args[0] && *args[0])
		jabber_chat_change_topic(chat, args[0]);
	else {
		const char *cur = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));
		char *buf, *tmp, *tmp2;

		if (cur) {
			tmp = g_markup_escape_text(cur, -1);
			tmp2 = purple_markup_linkify(tmp);
			buf = g_strdup_printf(_("current topic is: %s"), tmp2);
			g_free(tmp);
			g_free(tmp2);
		} else
			buf = g_strdup(_("No topic is set"));
		purple_conversation_write_system_message(conv, buf, PURPLE_MESSAGE_NO_LOG);
		g_free(buf);
	}

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_nick(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if(!chat || !args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	if (!jabber_resourceprep_validate(args[0])) {
		*error = g_strdup(_("Invalid nickname"));
		return PURPLE_CMD_RET_FAILED;
	}

	if (jabber_chat_change_nick(chat, args[0]))
		return PURPLE_CMD_RET_OK;
	else
		return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet jabber_cmd_chat_part(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if (!chat)
		return PURPLE_CMD_RET_FAILED;

	jabber_chat_part(chat, args ? args[0] : NULL);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_ban(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if(!chat || !args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	if(!jabber_chat_ban_user(chat, args[0], args[1])) {
		*error = g_strdup_printf(_("Unable to ban user %s"), args[0]);
		return PURPLE_CMD_RET_FAILED;
	}

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_affiliate(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if (!chat || !args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	if (strcmp(args[0], "owner") != 0 &&
	    strcmp(args[0], "admin") != 0 &&
	    strcmp(args[0], "member") != 0 &&
	    strcmp(args[0], "outcast") != 0 &&
	    strcmp(args[0], "none") != 0) {
		*error = g_strdup_printf(_("Unknown affiliation: \"%s\""), args[0]);
		return PURPLE_CMD_RET_FAILED;
	}

	if (args[1]) {
		int i;
		char **nicks = g_strsplit(args[1], " ", -1);

		for (i = 0; nicks[i]; ++i)
			if (!jabber_chat_affiliate_user(chat, nicks[i], args[0])) {
				*error = g_strdup_printf(_("Unable to affiliate user %s as \"%s\""), nicks[i], args[0]);
				g_strfreev(nicks);
				return PURPLE_CMD_RET_FAILED;
			}

		g_strfreev(nicks);
	} else {
		jabber_chat_affiliation_list(chat, args[0]);
	}

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_role(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if (!chat || !args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	if (strcmp(args[0], "moderator") != 0 &&
	    strcmp(args[0], "participant") != 0 &&
	    strcmp(args[0], "visitor") != 0 &&
	    strcmp(args[0], "none") != 0) {
		*error = g_strdup_printf(_("Unknown role: \"%s\""), args[0]);
		return PURPLE_CMD_RET_FAILED;
	}

	if (args[1]) {
		int i;
		char **nicks = g_strsplit(args[1], " ", -1);

		for (i = 0; nicks[i]; i++)
			if (!jabber_chat_role_user(chat, nicks[i], args[0], NULL)) {
				*error = g_strdup_printf(_("Unable to set role \"%s\" for user: %s"),
										 args[0], nicks[i]);
				g_strfreev(nicks);
				return PURPLE_CMD_RET_FAILED;
			}

		g_strfreev(nicks);
	} else {
		jabber_chat_role_list(chat, args[0]);
	}
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_invite(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	if(!args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	jabber_chat_invite(purple_conversation_get_connection(conv),
			purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)), args[1] ? args[1] : "",
			args[0]);

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_join(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));
	GHashTable *components;
	JabberID *jid = NULL;
	const char *room = NULL, *server = NULL, *handle = NULL;

	if (!chat || !args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	if (strchr(args[0], '@'))
		jid = jabber_id_new(args[0]);
	if (jid) {
		room   = jid->node;
		server = jid->domain;
		handle = jid->resource ? jid->resource : chat->handle;
	} else {
		/* If jabber_id_new failed, the user may have just passed in
		 * a room name.  For backward compatibility, handle that here.
		 */
		if (strchr(args[0], '@')) {
			*error = g_strdup(_("Invalid XMPP ID"));
			return PURPLE_CMD_RET_FAILED;
		}

		room   = args[0];
		server = chat->server;
		handle = chat->handle;
	}

	g_hash_table_insert(components, "room", (gpointer)room);
	g_hash_table_insert(components, "server", (gpointer)server);
	g_hash_table_insert(components, "handle", (gpointer)handle);

	if (args[1])
		g_hash_table_insert(components, "password", args[1]);

	jabber_chat_join(purple_conversation_get_connection(conv), components);

	g_hash_table_destroy(components);
	jabber_id_free(jid);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_kick(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));

	if(!chat || !args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	if(!jabber_chat_role_user(chat, args[0], "none", args[1])) {
		*error = g_strdup_printf(_("Unable to kick user %s"), args[0]);
		return PURPLE_CMD_RET_FAILED;
	}

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_chat_msg(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	JabberChat *chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));
	char *who;

	if (!chat)
		return PURPLE_CMD_RET_FAILED;

	who = g_strdup_printf("%s@%s/%s", chat->room, chat->server, args[0]);

	jabber_message_send_im(purple_conversation_get_connection(conv),
		purple_message_new_outgoing(who, args[1], 0));

	g_free(who);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet jabber_cmd_ping(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	PurpleAccount *account;
	PurpleConnection *pc;

	if(!args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	account = purple_conversation_get_account(conv);
	pc = purple_account_get_connection(account);

	if(!jabber_ping_jid(purple_connection_get_protocol_data(pc), args[0])) {
		*error = g_strdup_printf(_("Unable to ping user %s"), args[0]);
		return PURPLE_CMD_RET_FAILED;
	}

	return PURPLE_CMD_RET_OK;
}

static gboolean _jabber_send_buzz(JabberStream *js, const char *username, char **error) {

	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	PurpleConnection *gc = js->gc;
	PurpleBuddy *buddy =
		purple_blist_find_buddy(purple_connection_get_account(gc), username);
	const gchar *alias =
		buddy ? purple_buddy_get_contact_alias(buddy) : username;

	if(!username)
		return FALSE;

	jb = jabber_buddy_find(js, username, FALSE);
	if(!jb) {
		*error = g_strdup_printf(_("Unable to buzz, because there is nothing "
			"known about %s."), alias);
		return FALSE;
	}

	jbr = jabber_buddy_find_resource(jb, NULL);
	if (!jbr) {
		*error = g_strdup_printf(_("Unable to buzz, because %s might be offline."),
			alias);
		return FALSE;
	}

	if (jabber_resource_has_capability(jbr, NS_ATTENTION)) {
		PurpleXmlNode *buzz, *msg = purple_xmlnode_new("message");
		gchar *to;

		to = g_strdup_printf("%s/%s", username, jbr->name);
		purple_xmlnode_set_attrib(msg, "to", to);
		g_free(to);

		/* avoid offline storage */
		purple_xmlnode_set_attrib(msg, "type", "headline");

		buzz = purple_xmlnode_new_child(msg, "attention");
		purple_xmlnode_set_namespace(buzz, NS_ATTENTION);

		jabber_send(js, msg);
		purple_xmlnode_free(msg);

		return TRUE;
	} else {
		*error = g_strdup_printf(_("Unable to buzz, because %s does "
			"not support it or does not wish to receive buzzes now."), alias);
		return FALSE;
	}
}

static PurpleCmdRet jabber_cmd_buzz(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	JabberStream *js = purple_connection_get_protocol_data(purple_account_get_connection(account));
	const gchar *who;
	gchar *description;
	PurpleBuddy *buddy;
	const char *alias;
	PurpleAttentionType *attn =
		purple_get_attention_type_from_code(account, 0);

	if (!args || !args[0]) {
		/* use the buddy from conversation, if it's a one-to-one conversation */
		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			who = purple_conversation_get_name(conv);
		} else {
			return PURPLE_CMD_RET_FAILED;
		}
	} else {
		who = args[0];
	}

	buddy = purple_blist_find_buddy(account, who);
	if (buddy != NULL)
		alias = purple_buddy_get_contact_alias(buddy);
	else
		alias = who;

	description =
		g_strdup_printf(purple_attention_type_get_outgoing_desc(attn), alias);
	purple_conversation_write_system_message(conv, description,
		PURPLE_MESSAGE_NOTIFY);
	g_free(description);
	return _jabber_send_buzz(js, who, error)  ? PURPLE_CMD_RET_OK : PURPLE_CMD_RET_FAILED;
}

GList *jabber_attention_types(PurpleAccount *account)
{
	static GList *types = NULL;

	if (!types) {
		types = g_list_append(types, purple_attention_type_new("Buzz", _("Buzz"),
				_("%s has buzzed you!"), _("Buzzing %s...")));
	}

	return types;
}

gboolean jabber_send_attention(PurpleConnection *gc, const char *username, guint code)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);
	gchar *error = NULL;

	if (!_jabber_send_buzz(js, username, &error)) {
		PurpleAccount *account = purple_connection_get_account(gc);
		PurpleConversation *conv =
			purple_conversations_find_with_account(username, account);
		purple_debug_error("jabber", "jabber_send_attention: jabber_cmd_buzz failed with error: %s\n", error ? error : "(NULL)");

		if (conv) {
			purple_conversation_write_system_message(conv,
				error, PURPLE_MESSAGE_ERROR);
		}

		g_free(error);
		return FALSE;
	}

	return TRUE;
}


gboolean jabber_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

#ifdef USE_VV
gboolean
jabber_audio_enabled(JabberStream *js, const char *namespace)
{
	PurpleMediaManager *manager = purple_media_manager_get();
	PurpleMediaCaps caps = purple_media_manager_get_ui_caps(manager);

	return (caps & (PURPLE_MEDIA_CAPS_AUDIO | PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION));
}

gboolean
jabber_video_enabled(JabberStream *js, const char *namespace)
{
	PurpleMediaManager *manager = purple_media_manager_get();
	PurpleMediaCaps caps = purple_media_manager_get_ui_caps(manager);

	return (caps & (PURPLE_MEDIA_CAPS_VIDEO | PURPLE_MEDIA_CAPS_VIDEO_SINGLE_DIRECTION));
}

typedef struct {
	PurpleAccount *account;
	gchar *who;
	PurpleMediaSessionType type;

} JabberMediaRequest;

static void
jabber_media_cancel_cb(JabberMediaRequest *request,
		PurpleRequestFields *fields)
{
	g_free(request->who);
	g_free(request);
}

static void
jabber_media_ok_cb(JabberMediaRequest *request, PurpleRequestFields *fields)
{
	PurpleRequestField *field =
			purple_request_fields_get_field(fields, "resource");
	const gchar *selected = purple_request_field_choice_get_value(field);
	gchar *who = g_strdup_printf("%s/%s", request->who, selected);
	jabber_initiate_media(request->account, who, request->type);

	g_free(who);
	g_free(request->who);
	g_free(request);
}
#endif

gboolean
jabber_initiate_media(PurpleAccount *account, const char *who,
		      PurpleMediaSessionType type)
{
#ifdef USE_VV
	PurpleConnection *gc = purple_account_get_connection(account);
	JabberStream *js = purple_connection_get_protocol_data(gc);
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;
	char *resource = NULL;

	if (!js) {
		purple_debug_error("jabber",
				"jabber_initiate_media: NULL stream\n");
		return FALSE;
	}

	jb = jabber_buddy_find(js, who, FALSE);

	if(!jb || !jb->resources ||
			(((resource = jabber_get_resource(who)) != NULL)
			 && (jbr = jabber_buddy_find_resource(jb, resource)) == NULL)) {
		/* no resources online, we're trying to initiate with someone
		 * whose presence we're not subscribed to, or
		 * someone who is offline.  Let's inform the user */
		char *msg;

		if(!jb) {
			msg = g_strdup_printf(_("Unable to initiate media with %s: invalid JID"), who);
		} else if(jb->subscription & JABBER_SUB_TO && !jb->resources) {
			msg = g_strdup_printf(_("Unable to initiate media with %s: user is not online"), who);
		} else if(resource) {
			msg = g_strdup_printf(_("Unable to initiate media with %s: resource is not online"), who);
		} else {
			msg = g_strdup_printf(_("Unable to initiate media with %s: not subscribed to user presence"), who);
		}

		purple_notify_error(account, _("Media Initiation Failed"),
			_("Media Initiation Failed"), msg,
			purple_request_cpar_from_connection(gc));
		g_free(msg);
		g_free(resource);
		return FALSE;
	} else if(jbr != NULL) {
		/* they've specified a resource, no need to ask or
		 * default or anything, just do it */

		g_free(resource);

		if (type & PURPLE_MEDIA_AUDIO &&
			!jabber_resource_has_capability(jbr,
				JINGLE_APP_RTP_SUPPORT_AUDIO) &&
			jabber_resource_has_capability(jbr, NS_GOOGLE_VOICE))
			return jabber_google_session_initiate(js, who, type);
		else
			return jingle_rtp_initiate_media(js, who, type);
	} else if(!jb->resources->next) {
		/* only 1 resource online (probably our most common case)
		 * so no need to ask who to initiate with */
		gchar *name;
		gboolean result;
		jbr = jb->resources->data;
		name = g_strdup_printf("%s/%s", who, jbr->name);
		result = jabber_initiate_media(account, name, type);
		g_free(name);
		return result;
	} else {
		/* we've got multiple resources,
		 * we need to pick one to initiate with */
		GList *l;
		char *msg;
		PurpleRequestFields *fields;
		PurpleRequestField *field = purple_request_field_choice_new(
				"resource", _("Resource"), 0);
		PurpleRequestFieldGroup *group;
		JabberMediaRequest *request;

		purple_request_field_choice_set_data_destructor(field, g_free);

		for(l = jb->resources; l; l = l->next)
		{
			JabberBuddyResource *ljbr = l->data;
			PurpleMediaCaps caps;
			gchar *name;
			name = g_strdup_printf("%s/%s", who, ljbr->name);
			caps = jabber_get_media_caps(account, name);
			g_free(name);

			if ((type & PURPLE_MEDIA_AUDIO) &&
					(type & PURPLE_MEDIA_VIDEO)) {
				if (caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO) {
					jbr = ljbr;
					purple_request_field_choice_add(field,
						jbr->name, g_strdup(jbr->name));
				}
			} else if (type & (PURPLE_MEDIA_AUDIO) &&
					(caps & PURPLE_MEDIA_CAPS_AUDIO)) {
				jbr = ljbr;
				purple_request_field_choice_add(field,
					jbr->name, g_strdup(jbr->name));
			}else if (type & (PURPLE_MEDIA_VIDEO) &&
					(caps & PURPLE_MEDIA_CAPS_VIDEO)) {
				jbr = ljbr;
				purple_request_field_choice_add(field,
					jbr->name, g_strdup(jbr->name));
			}
		}

		if (jbr == NULL) {
			purple_debug_error("jabber",
					"No resources available\n");
			return FALSE;
		}

		if (g_list_length(purple_request_field_choice_get_elements(
				field)) <= 2) {
			gchar *name;
			gboolean result;
			purple_request_field_destroy(field);
			name = g_strdup_printf("%s/%s", who, jbr->name);
			result = jabber_initiate_media(account, name, type);
			g_free(name);
			return result;
		}

		msg = g_strdup_printf(_("Please select the resource of %s with which you would like to start a media session."), who);
		fields = purple_request_fields_new();
		group =	purple_request_field_group_new(NULL);
		request = g_new0(JabberMediaRequest, 1);
		request->account = account;
		request->who = g_strdup(who);
		request->type = type;

		purple_request_field_group_add_field(group, field);
		purple_request_fields_add_group(fields, group);
		purple_request_fields(account, _("Select a Resource"), msg,
				NULL, fields, _("Initiate Media"),
				G_CALLBACK(jabber_media_ok_cb), _("Cancel"),
				G_CALLBACK(jabber_media_cancel_cb),
				purple_request_cpar_from_account(account),
				request);

		g_free(msg);
		return TRUE;
	}
#endif
	return FALSE;
}

PurpleMediaCaps jabber_get_media_caps(PurpleAccount *account, const char *who)
{
#ifdef USE_VV
	PurpleConnection *gc = purple_account_get_connection(account);
	JabberStream *js = purple_connection_get_protocol_data(gc);
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	PurpleMediaCaps total = PURPLE_MEDIA_CAPS_NONE;
	gchar *resource;
	GList *specific = NULL, *l;

	if (!js) {
		purple_debug_info("jabber",
				"jabber_can_do_media: NULL stream\n");
		return FALSE;
	}

	jb = jabber_buddy_find(js, who, FALSE);

	if (!jb || !jb->resources) {
		/* no resources online, we're trying to get caps for someone
		 * whose presence we're not subscribed to, or
		 * someone who is offline. */
		return total;

	} else if ((resource = jabber_get_resource(who)) != NULL) {
		/* they've specified a resource, no need to ask or
		 * default or anything, just do it */
		jbr = jabber_buddy_find_resource(jb, resource);
		g_free(resource);

		if (!jbr) {
			purple_debug_error("jabber", "jabber_get_media_caps:"
					" Can't find resource %s\n", who);
			return total;
		}

		l = specific = g_list_prepend(specific, jbr);

	} else {
		/* we've got multiple resources, combine their caps */
		l = jb->resources;
	}

	for (; l; l = l->next) {
		PurpleMediaCaps caps = PURPLE_MEDIA_CAPS_NONE;
		jbr = l->data;

		if (jabber_resource_has_capability(jbr,
				JINGLE_APP_RTP_SUPPORT_AUDIO))
			caps |= PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION |
					PURPLE_MEDIA_CAPS_AUDIO;
		if (jabber_resource_has_capability(jbr,
				JINGLE_APP_RTP_SUPPORT_VIDEO))
			caps |= PURPLE_MEDIA_CAPS_VIDEO_SINGLE_DIRECTION |
					PURPLE_MEDIA_CAPS_VIDEO;
		if (caps & PURPLE_MEDIA_CAPS_AUDIO && caps &
				PURPLE_MEDIA_CAPS_VIDEO)
			caps |= PURPLE_MEDIA_CAPS_AUDIO_VIDEO;
		if (caps != PURPLE_MEDIA_CAPS_NONE) {
			if (!jabber_resource_has_capability(jbr,
					JINGLE_TRANSPORT_ICEUDP) &&
					!jabber_resource_has_capability(jbr,
					JINGLE_TRANSPORT_RAWUDP)) {
				purple_debug_info("jingle-rtp", "Buddy doesn't "
						"support the same transport types\n");
				caps = PURPLE_MEDIA_CAPS_NONE;
			} else
				caps |= PURPLE_MEDIA_CAPS_MODIFY_SESSION |
						PURPLE_MEDIA_CAPS_CHANGE_DIRECTION;
		}
		if (jabber_resource_has_capability(jbr, NS_GOOGLE_VOICE)) {
			caps |= PURPLE_MEDIA_CAPS_AUDIO;
			if (jabber_resource_has_capability(jbr, NS_GOOGLE_VIDEO))
				caps |= PURPLE_MEDIA_CAPS_AUDIO_VIDEO;
		}

		total |= caps;
	}

	if (specific) {
		g_list_free(specific);
	}

	return total;
#else
	return PURPLE_MEDIA_CAPS_NONE;
#endif
}

gboolean jabber_can_receive_file(PurpleConnection *gc, const char *who)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);

	if (js) {
		JabberBuddy *jb = jabber_buddy_find(js, who, FALSE);
		GList *iter;
		gboolean has_resources_without_caps = FALSE;

		/* if we didn't find a JabberBuddy, we don't have presence for this
		 buddy, let's assume they can receive files, disco should tell us
		 when actually trying */
		if (jb == NULL)
			return TRUE;

		/* find out if there is any resources without caps */
		for (iter = jb->resources; iter ; iter = g_list_next(iter)) {
			JabberBuddyResource *jbr = (JabberBuddyResource *) iter->data;

			if (!jabber_resource_know_capabilities(jbr)) {
				has_resources_without_caps = TRUE;
			}
		}

		if (has_resources_without_caps) {
			/* there is at least one resource which we don't have caps for,
			 let's assume they can receive files... */
			return TRUE;
		} else {
			/* we have caps for all the resources, see if at least one has
			 right caps */
			for (iter = jb->resources; iter ; iter = g_list_next(iter)) {
				JabberBuddyResource *jbr = (JabberBuddyResource *) iter->data;

				if (jabber_resource_has_capability(jbr, NS_SI_FILE_TRANSFER)
			    	&& (jabber_resource_has_capability(jbr,
			    			NS_BYTESTREAMS)
			        	|| jabber_resource_has_capability(jbr, NS_IBB))) {
					return TRUE;
				}
			}
			return FALSE;
		}
	} else {
		return TRUE;
	}
}

static PurpleCmdRet
jabber_cmd_mood(PurpleConversation *conv,
		const char *cmd, char **args, char **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	JabberStream *js = purple_connection_get_protocol_data(purple_account_get_connection(account));

	if (js->pep) {
		gboolean ret;

		if (!args || !args[0]) {
			/* No arguments; unset mood */
			ret = jabber_mood_set(js, NULL, NULL);
		} else {
			/* At least one argument.  Relying on the list of arguments
			 * being NULL-terminated.
			 */
			ret = jabber_mood_set(js, args[0], args[1]);
			if (!ret) {
				/* Let's try again */
				char *tmp = g_strjoin(" ", args[0], args[1], NULL);
				ret = jabber_mood_set(js, "undefined", tmp);
				g_free(tmp);
			}
		}

		if (ret) {
			return PURPLE_CMD_RET_OK;
		} else {
			purple_conversation_write_system_message(conv,
				_("Failed to specify mood"),
				PURPLE_MESSAGE_ERROR);
			return PURPLE_CMD_RET_FAILED;
		}
	} else {
		/* account does not support PEP, can't set a mood */
		purple_conversation_write_system_message(conv,
		    _("Account does not support PEP, can't set mood"),
		    PURPLE_MESSAGE_ERROR);
		return PURPLE_CMD_RET_FAILED;
	}
}

static void
jabber_register_commands(PurpleProtocol *protocol)
{
	GSList *commands = NULL;
	PurpleCmdId id;
	const gchar *proto_id = purple_protocol_get_id(protocol);

	id = purple_cmd_register("config", "", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY, proto_id,
		jabber_cmd_chat_config, _("config:  Configure a chat room."),
		NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("configure", "", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY, proto_id,
		jabber_cmd_chat_config, _("configure:  Configure a chat room."),
		NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("nick", "s", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY, proto_id,
		jabber_cmd_chat_nick, _("nick &lt;new nickname&gt;:  "
		"Change your nickname."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("part", "s", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_part,
		_("part [message]:  Leave the room."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("register", "", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY, proto_id,
		jabber_cmd_chat_register,
		_("register:  Register with a chat room."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	/* XXX: there needs to be a core /topic cmd, methinks */
	id = purple_cmd_register("topic", "s", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_topic,
		_("topic [new topic]:  View or change the topic."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("ban", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_ban,
		_("ban &lt;user&gt; [reason]:  Ban a user from the room."),
		NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("affiliate", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id,
		jabber_cmd_chat_affiliate, _("affiliate "
		"&lt;owner|admin|member|outcast|none&gt; [nick1] [nick2] ...: "
		"Get the users with an affiliation or set users' affiliation "
		"with the room."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("role", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_role,
		_("role &lt;moderator|participant|visitor|none&gt; [nick1] "
		"[nick2] ...: Get the users with a role or set users' role "
		"with the room."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("invite", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_invite,
		_("invite &lt;user&gt; [message]:  Invite a user to the room."),
		NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("join", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_join,
		_("join: &lt;room[@server]&gt; [password]:  Join a chat."),
		NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("kick", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_chat_kick,
		_("kick &lt;user&gt; [reason]:  Kick a user from the room."),
		NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("msg", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY, proto_id,
		jabber_cmd_chat_msg, _("msg &lt;user&gt; &lt;message&gt;:  "
		"Send a private message to another user."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("ping", "w", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM |
		PURPLE_CMD_FLAG_PROTOCOL_ONLY, proto_id, jabber_cmd_ping,
		_("ping &lt;jid&gt;:  Ping a user/component/server."), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("buzz", "w", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY |
		PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, proto_id, jabber_cmd_buzz,
		_("buzz: Buzz a user to get their attention"), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	id = purple_cmd_register("mood", "ws", PURPLE_CMD_P_PROTOCOL,
		PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM |
		PURPLE_CMD_FLAG_PROTOCOL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
		proto_id, jabber_cmd_mood,
		_("mood &lt;mood&gt; [text]: Set current user mood"), NULL);
	commands = g_slist_prepend(commands, GUINT_TO_POINTER(id));

	g_hash_table_insert(jabber_cmds, protocol, commands);
}

static void cmds_free_func(gpointer value)
{
	GSList *commands = value;
	while (commands) {
		purple_cmd_unregister(GPOINTER_TO_UINT(commands->data));
		commands = g_slist_delete_link(commands, commands);
	}
}

static void jabber_unregister_commands(PurpleProtocol *protocol)
{
	g_hash_table_remove(jabber_cmds, protocol);
}

#if 0
/* IPC functions */

/**
 * IPC function for determining if a contact supports a certain feature.
 *
 * @param account   The PurpleAccount
 * @param jid       The full JID of the contact.
 * @param feature   The feature's namespace.
 *
 * @return TRUE if supports feature; else FALSE.
 */
static gboolean
jabber_ipc_contact_has_feature(PurpleAccount *account, const gchar *jid,
                               const gchar *feature)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	JabberStream *js;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	gchar *resource;

	if (!purple_account_is_connected(account))
		return FALSE;
	js = purple_connection_get_protocol_data(gc);

	if (!(resource = jabber_get_resource(jid)) ||
	    !(jb = jabber_buddy_find(js, jid, FALSE)) ||
	    !(jbr = jabber_buddy_find_resource(jb, resource))) {
		g_free(resource);
		return FALSE;
	}

	g_free(resource);

	return jabber_resource_has_capability(jbr, feature);
}

static void
jabber_ipc_add_feature(const gchar *feature)
{
	if (!feature)
		return;
	jabber_add_feature(feature, 0);

	/* send presence with new caps info for all connected accounts */
	jabber_caps_broadcast_change();
}
#endif

static PurpleAccount *find_acct(const char *protocol, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, protocol);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(protocol, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

static gboolean xmpp_uri_handler(const char *proto, const char *user, GHashTable *params)
{
	char *acct_id = params ? g_hash_table_lookup(params, "account") : NULL;
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "xmpp"))
		return FALSE;

	acct = find_acct(proto, acct_id);

	if (!acct)
		return FALSE;

	/* xmpp:romeo@montague.net?message;subject=Test%20Message;body=Here%27s%20a%20test%20message */
	/* params is NULL if the URI has no '?' (or anything after it) */
	if (!params || g_hash_table_lookup_extended(params, "message", NULL, NULL)) {
		char *body = g_hash_table_lookup(params, "body");
		if (user && *user) {
			PurpleIMConversation *im =
					purple_im_conversation_new(acct, user);
			purple_conversation_present(PURPLE_CONVERSATION(im));
			if (body && *body)
				purple_conversation_send_confirm(PURPLE_CONVERSATION(im), body);
		}
	} else if (g_hash_table_lookup_extended(params, "roster", NULL, NULL)) {
		char *name = g_hash_table_lookup(params, "name");
		if (user && *user)
			purple_blist_request_add_buddy(acct, user, NULL, name);
	} else if (g_hash_table_lookup_extended(params, "join", NULL, NULL)) {
		PurpleConnection *gc = purple_account_get_connection(acct);
		if (user && *user) {
			GHashTable *params = jabber_chat_info_defaults(gc, user);
			jabber_chat_join(gc, params);
		}
		return TRUE;
	}

	return FALSE;
}

static void
jabber_do_init(void)
{
	GHashTable *ui_info = purple_core_get_ui_info();
	const gchar *ui_type;
	const gchar *type = "pc"; /* default client type, if unknown or
								unspecified */
	const gchar *ui_name = NULL;
#ifdef HAVE_CYRUS_SASL
	/* We really really only want to do this once per process */
	static gboolean sasl_initialized = FALSE;
#ifdef _WIN32
	UINT old_error_mode;
	gchar *sasldir;
#endif
	int ret;
#endif

	/* XXX - If any other plugin wants SASL this won't be good ... */
#ifdef HAVE_CYRUS_SASL
	if (!sasl_initialized) {
		sasl_initialized = TRUE;
#ifdef _WIN32
		sasldir = g_build_filename(wpurple_bin_dir(), "sasl2", NULL);
		sasl_set_path(SASL_PATH_TYPE_PLUGIN, sasldir);
		g_free(sasldir);
		/* Suppress error popups for failing to load sasl plugins */
		old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
		if ((ret = sasl_client_init(NULL)) != SASL_OK) {
			purple_debug_error("xmpp", "Error (%d) initializing SASL.\n", ret);
		}
#ifdef _WIN32
		/* Restore the original error mode */
		SetErrorMode(old_error_mode);
#endif
	}
#endif

	jabber_cmds = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, cmds_free_func);

	ui_type = ui_info ? g_hash_table_lookup(ui_info, "client_type") : NULL;
	if (ui_type) {
		if (strcmp(ui_type, "pc") == 0 ||
			strcmp(ui_type, "console") == 0 ||
			strcmp(ui_type, "phone") == 0 ||
			strcmp(ui_type, "handheld") == 0 ||
			strcmp(ui_type, "web") == 0 ||
			strcmp(ui_type, "bot") == 0) {
			type = ui_type;
		}
	}

	if (ui_info)
		ui_name = g_hash_table_lookup(ui_info, "name");
	if (ui_name == NULL)
		ui_name = PACKAGE;

	jabber_add_identity("client", type, NULL, ui_name);

	/* initialize jabber_features list */
	jabber_add_feature(NS_LAST_ACTIVITY, NULL);
	jabber_add_feature(NS_OOB_IQ_DATA, NULL);
	jabber_add_feature(NS_ENTITY_TIME, NULL);
	jabber_add_feature("jabber:iq:version", NULL);
	jabber_add_feature("jabber:x:conference", NULL);
	jabber_add_feature(NS_BYTESTREAMS, NULL);
	jabber_add_feature("http://jabber.org/protocol/caps", NULL);
	jabber_add_feature("http://jabber.org/protocol/chatstates", NULL);
	jabber_add_feature(NS_DISCO_INFO, NULL);
	jabber_add_feature(NS_DISCO_ITEMS, NULL);
	jabber_add_feature(NS_IBB, NULL);
	jabber_add_feature("http://jabber.org/protocol/muc", NULL);
	jabber_add_feature("http://jabber.org/protocol/muc#user", NULL);
	jabber_add_feature("http://jabber.org/protocol/si", NULL);
	jabber_add_feature(NS_SI_FILE_TRANSFER, NULL);
	jabber_add_feature(NS_XHTML_IM, NULL);
	jabber_add_feature(NS_PING, NULL);

	/* Buzz/Attention */
	jabber_add_feature(NS_ATTENTION, jabber_buzz_isenabled);

	/* Bits Of Binary */
	jabber_add_feature(NS_BOB, NULL);

	/* Jingle features! */
	jabber_add_feature(JINGLE, NULL);

#ifdef USE_VV
	jabber_add_feature(NS_GOOGLE_PROTOCOL_SESSION, jabber_audio_enabled);
	jabber_add_feature(NS_GOOGLE_VOICE, jabber_audio_enabled);
	jabber_add_feature(NS_GOOGLE_VIDEO, jabber_video_enabled);
	jabber_add_feature(NS_GOOGLE_CAMERA, jabber_video_enabled);
	jabber_add_feature(JINGLE_APP_RTP, NULL);
	jabber_add_feature(JINGLE_APP_RTP_SUPPORT_AUDIO, jabber_audio_enabled);
	jabber_add_feature(JINGLE_APP_RTP_SUPPORT_VIDEO, jabber_video_enabled);
	jabber_add_feature(JINGLE_TRANSPORT_RAWUDP, NULL);
	jabber_add_feature(JINGLE_TRANSPORT_ICEUDP, NULL);

	g_signal_connect(G_OBJECT(purple_media_manager_get()), "ui-caps-changed",
			G_CALLBACK(jabber_caps_broadcast_change), NULL);
#endif

	/* reverse order of unload_plugin */
	jabber_iq_init();
	jabber_presence_init();
	jabber_caps_init();
	/* PEP things should be init via jabber_pep_init, not here */
	jabber_pep_init();
	jabber_data_init();
	jabber_bosh_init();

	/* TODO: Implement adding and retrieving own features via IPC API */

	jabber_ibb_init();
	jabber_si_init();

	jabber_auth_init();
}

static void
jabber_do_uninit(void)
{
	/* reverse order of jabber_do_init */
	jabber_bosh_uninit();
	jabber_data_uninit();
	jabber_si_uninit();
	jabber_ibb_uninit();
	/* PEP things should be uninit via jabber_pep_uninit, not here */
	jabber_pep_uninit();
	jabber_caps_uninit();
	jabber_presence_uninit();
	jabber_iq_uninit();

#ifdef USE_VV
	g_signal_handlers_disconnect_by_func(G_OBJECT(purple_media_manager_get()),
			G_CALLBACK(jabber_caps_broadcast_change), NULL);
#endif

	jabber_auth_uninit();
	jabber_features_destroy();
	jabber_identities_destroy();

	g_hash_table_destroy(jabber_cmds);
	jabber_cmds = NULL;
}

static void jabber_init_protocol(PurpleProtocol *protocol)
{
	++plugin_ref;

	if (plugin_ref == 1)
		jabber_do_init();

	jabber_register_commands(protocol);

#if 0
	/* IPC functions */
	purple_plugin_ipc_register(plugin, "contact_has_feature", PURPLE_CALLBACK(jabber_ipc_contact_has_feature),
							 purple_marshal_BOOLEAN__POINTER_POINTER_POINTER,
							 G_TYPE_BOOLEAN, 3,
							 PURPLE_TYPE_ACCOUNT, G_TYPE_STRING, G_TYPE_STRING);

	purple_plugin_ipc_register(plugin, "add_feature", PURPLE_CALLBACK(jabber_ipc_add_feature),
							 purple_marshal_VOID__POINTER,
							 G_TYPE_NONE, 1, G_TYPE_STRING);

	purple_plugin_ipc_register(plugin, "register_namespace_watcher",
	                           PURPLE_CALLBACK(jabber_iq_signal_register),
	                           purple_marshal_VOID__POINTER_POINTER,
	                           G_TYPE_NONE, 2,
	                           G_TYPE_STRING,  /* node */
	                           G_TYPE_STRING); /* namespace */

	purple_plugin_ipc_register(plugin, "unregister_namespace_watcher",
	                           PURPLE_CALLBACK(jabber_iq_signal_unregister),
	                           purple_marshal_VOID__POINTER_POINTER,
	                           G_TYPE_NONE, 2,
	                           G_TYPE_STRING,  /* node */
	                           G_TYPE_STRING); /* namespace */
#endif

	purple_signal_register(protocol, "jabber-register-namespace-watcher",
			purple_marshal_VOID__POINTER_POINTER,
			G_TYPE_NONE, 2,
			G_TYPE_STRING,  /* node */
			G_TYPE_STRING); /* namespace */

	purple_signal_register(protocol, "jabber-unregister-namespace-watcher",
			purple_marshal_VOID__POINTER_POINTER,
			G_TYPE_NONE, 2,
			G_TYPE_STRING,  /* node */
			G_TYPE_STRING); /* namespace */

	purple_signal_connect(protocol, "jabber-register-namespace-watcher",
			protocol, PURPLE_CALLBACK(jabber_iq_signal_register), NULL);
	purple_signal_connect(protocol, "jabber-unregister-namespace-watcher",
			protocol, PURPLE_CALLBACK(jabber_iq_signal_unregister), NULL);


	purple_signal_register(protocol, "jabber-receiving-xmlnode",
			purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_POINTER); /* pointer to a PurpleXmlNode* */

	purple_signal_register(protocol, "jabber-sending-xmlnode",
			purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_POINTER); /* pointer to a PurpleXmlNode* */

	/*
	 * Do not remove this or the plugin will fail. Completely. You have been
	 * warned!
	 */
	purple_signal_connect_priority(protocol, "jabber-sending-xmlnode",
			protocol, PURPLE_CALLBACK(jabber_send_signal_cb),
			NULL, PURPLE_SIGNAL_PRIORITY_HIGHEST);

	purple_signal_register(protocol, "jabber-sending-text",
			     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			     PURPLE_TYPE_CONNECTION,
			     G_TYPE_POINTER); /* pointer to a string */

	purple_signal_register(protocol, "jabber-receiving-message",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER_POINTER,
			G_TYPE_BOOLEAN, 6,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_STRING, /* type */
			G_TYPE_STRING, /* id */
			G_TYPE_STRING, /* from */
			G_TYPE_STRING, /* to */
			PURPLE_TYPE_XMLNODE);

	purple_signal_register(protocol, "jabber-receiving-iq",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
			G_TYPE_BOOLEAN, 5,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_STRING, /* type */
			G_TYPE_STRING, /* id */
			G_TYPE_STRING, /* from */
			PURPLE_TYPE_XMLNODE);

	purple_signal_register(protocol, "jabber-watched-iq",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER_POINTER,
			G_TYPE_BOOLEAN, 5,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_STRING, /* type */
			G_TYPE_STRING, /* id */
			G_TYPE_STRING, /* from */
			PURPLE_TYPE_XMLNODE); /* child */

	purple_signal_register(protocol, "jabber-receiving-presence",
			purple_marshal_BOOLEAN__POINTER_POINTER_POINTER_POINTER,
			G_TYPE_BOOLEAN, 4,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_STRING, /* type */
			G_TYPE_STRING, /* from */
			PURPLE_TYPE_XMLNODE);
}

static void jabber_uninit_protocol(PurpleProtocol *protocol)
{
	g_return_if_fail(plugin_ref > 0);

	purple_signals_unregister_by_instance(protocol);
#if 0
	purple_plugin_ipc_unregister_all(plugin);
#endif
	jabber_unregister_commands(protocol);

	--plugin_ref;
	if (plugin_ref == 0)
		jabber_do_uninit();
}

static void
jabber_protocol_init(PurpleProtocol *protocol)
{
	protocol->id        = "prpl-jabber";
	protocol->name      = "XMPP";
	protocol->options   = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME |
	                      OPT_PROTO_MAIL_CHECK |
#ifdef HAVE_CYRUS_SASL
	                      OPT_PROTO_PASSWORD_OPTIONAL |
#endif
	                      OPT_PROTO_SLASH_COMMANDS_NATIVE;

	protocol->icon_spec = purple_buddy_icon_spec_new("png",
	                                                 32, 32, 96, 96, 0,
	                                                 PURPLE_ICON_SCALE_SEND |
	                                                 PURPLE_ICON_SCALE_DISPLAY);
}

static void
jabber_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->login        = jabber_login;
	klass->close        = jabber_close;
	klass->status_types = jabber_status_types;
	klass->list_icon    = jabber_list_icon;
}

static void
jabber_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_actions     = jabber_get_actions;
	client_iface->list_emblem     = jabber_list_emblem;
	client_iface->status_text     = jabber_status_text;
	client_iface->tooltip_text    = jabber_tooltip_text;
	client_iface->blist_node_menu = jabber_blist_node_menu;
	client_iface->convo_closed    = jabber_convo_closed;
	client_iface->normalize       = jabber_normalize;
	client_iface->find_blist_chat = jabber_find_blist_chat;
	client_iface->offline_message = jabber_offline_message;
	client_iface->get_moods       = jabber_get_moods;
}

static void
jabber_protocol_server_iface_init(PurpleProtocolServerIface *server_iface)
{
	server_iface->register_user   = jabber_register_account;
	server_iface->unregister_user = jabber_unregister_account;
	server_iface->set_info        = jabber_set_info;
	server_iface->get_info        = jabber_buddy_get_info;
	server_iface->set_status      = jabber_set_status;
	server_iface->set_idle        = jabber_idle_set;
	server_iface->add_buddy       = jabber_roster_add_buddy;
	server_iface->remove_buddy    = jabber_roster_remove_buddy;
	server_iface->keepalive       = jabber_keepalive;
	server_iface->alias_buddy     = jabber_roster_alias_change;
	server_iface->group_buddy     = jabber_roster_group_change;
	server_iface->rename_group    = jabber_roster_group_rename;
	server_iface->set_buddy_icon  = jabber_set_buddy_icon;
	server_iface->send_raw        = jabber_protocol_send_raw;
}

static void
jabber_protocol_im_iface_init(PurpleProtocolIMIface *im_iface)
{
	im_iface->send        = jabber_message_send_im;
	im_iface->send_typing = jabber_send_typing;
}

static void
jabber_protocol_chat_iface_init(PurpleProtocolChatIface *chat_iface)
{
	chat_iface->info               = jabber_chat_info;
	chat_iface->info_defaults      = jabber_chat_info_defaults;
	chat_iface->join               = jabber_chat_join;
	chat_iface->get_name           = jabber_get_chat_name;
	chat_iface->invite             = jabber_chat_invite;
	chat_iface->leave              = jabber_chat_leave;
	chat_iface->send               = jabber_message_send_chat;
	chat_iface->get_user_real_name = jabber_chat_user_real_name;
	chat_iface->set_topic          = jabber_chat_set_topic;
}

static void
jabber_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *privacy_iface)
{
	privacy_iface->add_deny = jabber_add_deny;
	privacy_iface->rem_deny = jabber_rem_deny;
}

static void
jabber_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *roomlist_iface)
{
	roomlist_iface->get_list       = jabber_roomlist_get_list;
	roomlist_iface->cancel         = jabber_roomlist_cancel;
	roomlist_iface->room_serialize = jabber_roomlist_room_serialize;
}

static void
jabber_protocol_attention_iface_init(PurpleProtocolAttentionIface *attention_iface)
{
	attention_iface->send      = jabber_send_attention;
	attention_iface->get_types = jabber_attention_types;
}

static void
jabber_protocol_media_iface_init(PurpleProtocolMediaIface *media_iface)
{
	media_iface->initiate_session = jabber_initiate_media;
	media_iface->get_caps         = jabber_get_media_caps;
}

static void
jabber_protocol_xfer_iface_init(PurpleProtocolXferIface *xfer_iface)
{
	xfer_iface->can_receive = jabber_can_receive_file;
	xfer_iface->send        = jabber_si_xfer_send;
	xfer_iface->new_xfer    = jabber_si_new_xfer;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	JabberProtocol, jabber_protocol, PURPLE_TYPE_PROTOCOL, G_TYPE_FLAG_ABSTRACT,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  jabber_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  jabber_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  jabber_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  jabber_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  jabber_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  jabber_protocol_roomlist_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE,
	                                  jabber_protocol_attention_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_MEDIA_IFACE,
	                                  jabber_protocol_media_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                  jabber_protocol_xfer_iface_init)
);

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "prpl-xmpp",
		"name",         "XMPP Protocols",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("XMPP and GTalk Protocols Plugin"),
		"description",  N_("XMPP and GTalk Protocols Plugin"),
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	jingle_session_register_type(plugin);

	jingle_transport_register_type(plugin);
	jingle_iceudp_register_type(plugin);
	jingle_rawudp_register_type(plugin);
	jingle_google_p2p_register_type(plugin);

	jingle_content_register_type(plugin);
#ifdef USE_VV
	jingle_rtp_register_type(plugin);
#endif

	jabber_protocol_register_type(plugin);

	gtalk_protocol_register_type(plugin);
	xmpp_protocol_register_type(plugin);

	xmpp_protocol = purple_protocols_add(XMPP_TYPE_PROTOCOL, error);
	if (!xmpp_protocol)
		return FALSE;

	gtalk_protocol = purple_protocols_add(GTALK_TYPE_PROTOCOL, error);
	if (!gtalk_protocol)
		return FALSE;

	purple_signal_connect(purple_get_core(), "uri-handler", xmpp_protocol,
		PURPLE_CALLBACK(xmpp_uri_handler), NULL);
	purple_signal_connect(purple_get_core(), "uri-handler", gtalk_protocol,
		PURPLE_CALLBACK(xmpp_uri_handler), NULL);

	jabber_init_protocol(xmpp_protocol);
	jabber_init_protocol(gtalk_protocol);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	jabber_uninit_protocol(gtalk_protocol);
	jabber_uninit_protocol(xmpp_protocol);

	if (!purple_protocols_remove(gtalk_protocol, error))
		return FALSE;

	if (!purple_protocols_remove(xmpp_protocol, error))
		return FALSE;

	return TRUE;
}

PURPLE_PLUGIN_INIT(jabber, plugin_query, plugin_load, plugin_unload);
