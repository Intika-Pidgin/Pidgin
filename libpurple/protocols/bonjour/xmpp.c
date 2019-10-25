/*
 * purple - Bonjour Protocol Plugin
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
 */

#include "internal.h"
#include <purple.h>

#ifndef _WIN32
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <sys/types.h>

/* Solaris */
#if defined (__SVR4) && defined (__sun)
#include <sys/sockio.h>
#endif

#include <glib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#include "xmpp.h"
#include "parser.h"
#include "bonjour.h"
#include "buddy.h"
#include "bonjour_ft.h"

#ifdef _SIZEOF_ADDR_IFREQ
#  define HX_SIZE_OF_IFREQ(a) _SIZEOF_ADDR_IFREQ(a)
#else
#  define HX_SIZE_OF_IFREQ(a) sizeof(a)
#endif

#define STREAM_END "</stream:stream>"
/* TODO: specify version='1.0' and send stream features */
#define DOCTYPE "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" \
		"<stream:stream xmlns=\"xmpp:client\" xmlns:stream=\"http://etherx.xmpp.org/streams\" from=\"%s\" to=\"%s\">"

enum sent_stream_start_types {
	NOT_SENT       = 0,
	PARTIALLY_SENT = 1,
	FULLY_SENT     = 2
};

static void
xep_iq_parse(PurpleXmlNode *packet, PurpleBuddy *pb);

static BonjourXMPPConversation *
bonjour_xmpp_conv_new(PurpleBuddy *pb, PurpleAccount *account, const char *ip) {

	BonjourXMPPConversation *bconv = g_new0(BonjourXMPPConversation, 1);
	bconv->cancellable = g_cancellable_new();
	bconv->tx_buf = purple_circular_buffer_new(512);
	bconv->tx_handler = 0;
	bconv->rx_handler = 0;
	bconv->pb = pb;
	bconv->account = account;
	bconv->ip = g_strdup(ip);

	bonjour_parser_setup(bconv);

	return bconv;
}

static const char *
_font_size_ichat_to_purple(int size)
{
	if (size > 24) {
		return "7";
	} else if (size >= 21) {
		return "6";
	} else if (size >= 17) {
		return "5";
	} else if (size >= 14) {
		return "4";
	} else if (size >= 12) {
		return "3";
	} else if (size >= 10) {
		return "2";
	}

	return "1";
}

static gchar *
get_xmlnode_contents(PurpleXmlNode *node)
{
	gchar *contents;

	contents = purple_xmlnode_to_str(node, NULL);

	/* we just want the stuff inside <font></font>
	 * There isn't stuff exposed in PurpleXmlNode.c to do this more cleanly. */

	if (contents) {
		char *bodystart = strchr(contents, '>');
		char *bodyend = bodystart ? strrchr(bodystart, '<') : NULL;
		if (bodystart && bodyend && (bodystart + 1) != bodyend) {
			*bodyend = '\0';
			memmove(contents, bodystart + 1, (bodyend - bodystart));
		}
	}

	return contents;
}

static void
_xmpp_parse_and_write_message_to_ui(PurpleXmlNode *message_node, PurpleBuddy *pb)
{
	PurpleXmlNode *body_node, *html_node, *events_node;
	PurpleConnection *gc = purple_account_get_connection(purple_buddy_get_account(pb));
	gchar *body = NULL;

	body_node = purple_xmlnode_get_child(message_node, "body");
	html_node = purple_xmlnode_get_child(message_node, "html");

	if (body_node == NULL && html_node == NULL) {
		purple_debug_error("bonjour", "No body or html node found, discarding message.\n");
		return;
	}

	events_node = purple_xmlnode_get_child_with_namespace(message_node, "x", "xmpp:x:event");
	if (events_node != NULL) {
		if (purple_xmlnode_get_child(events_node, "id") != NULL) {
			/* The user is just typing */
			/* TODO: Deal with typing notification */
			return;
		}
	}

	if (html_node != NULL) {
		PurpleXmlNode *html_body_node;

		html_body_node = purple_xmlnode_get_child(html_node, "body");
		if (html_body_node != NULL) {
			PurpleXmlNode *html_body_font_node;

			html_body_font_node = purple_xmlnode_get_child(html_body_node, "font");
			/* Types of messages sent by iChat */
			if (html_body_font_node != NULL) {
				gchar *html_body;
				const char *font_face, *font_size, *font_color,
					*ichat_balloon_color, *ichat_text_color;

				font_face = purple_xmlnode_get_attrib(html_body_font_node, "face");
				/* The absolute iChat font sizes should be converted to 1..7 range */
				font_size = purple_xmlnode_get_attrib(html_body_font_node, "ABSZ");
				if (font_size != NULL)
					font_size = _font_size_ichat_to_purple(atoi(font_size));
				font_color = purple_xmlnode_get_attrib(html_body_font_node, "color");
				ichat_balloon_color = purple_xmlnode_get_attrib(html_body_node, "ichatballooncolor");
				ichat_text_color = purple_xmlnode_get_attrib(html_body_node, "ichattextcolor");

				html_body = get_xmlnode_contents(html_body_font_node);

				if (html_body == NULL)
					/* This is the kind of formatted messages that Purple creates */
					html_body = purple_xmlnode_to_str(html_body_font_node, NULL);

				if (html_body != NULL) {
					GString *str = g_string_new("<font");

					if (font_face)
						g_string_append_printf(str, " face='%s'", font_face);
					if (font_size)
						g_string_append_printf(str, " size='%s'", font_size);
					if (font_color)
						g_string_append_printf(str, " color='%s'", font_color);
					else if (ichat_text_color)
						g_string_append_printf(str, " color='%s'", ichat_text_color);
					if (ichat_balloon_color)
						g_string_append_printf(str, " back='%s'", ichat_balloon_color);
					g_string_append_printf(str, ">%s</font>", html_body);

					body = g_string_free(str, FALSE);

					g_free(html_body);
				}
			}
		}
	}

	/* Compose the message */
	if (body == NULL && body_node != NULL)
		body = purple_xmlnode_get_data(body_node);

	if (body == NULL) {
		purple_debug_error("bonjour", "No html body or regular body found.\n");
		return;
	}

	/* Send the message to the UI */
	purple_serv_got_im(gc, purple_buddy_get_name(pb), body, 0, time(NULL));

	g_free(body);
}

struct _match_buddies_by_address {
	const char *address;
	GSList *matched_buddies;
};

static void
_match_buddies_by_address(gpointer value, gpointer data)
{
	PurpleBuddy *pb = value;
	BonjourBuddy *bb = NULL;
	struct _match_buddies_by_address *mbba = data;

	bb = purple_buddy_get_protocol_data(pb);

	/*
	 * If the current PurpleBuddy's data is not null, then continue to determine
	 * whether one of the buddies IPs matches the target IP.
	 */
	if (bb != NULL)
	{
		const char *ip;
		GSList *tmp = bb->ips;

		while(tmp) {
			ip = tmp->data;
			if (ip != NULL && g_ascii_strcasecmp(ip, mbba->address) == 0) {
				mbba->matched_buddies = g_slist_prepend(mbba->matched_buddies, pb);
				break;
			}
			tmp = tmp->next;
		}
	}
}

static void
_send_data_write_cb(GObject *stream, gpointer data)
{
	PurpleBuddy *pb = data;
	BonjourBuddy *bb = purple_buddy_get_protocol_data(pb);
	BonjourXMPPConversation *bconv = bb->conversation;
	gsize writelen;
	gssize ret;
	GError *error = NULL;

	writelen = purple_circular_buffer_get_max_read(bconv->tx_buf);

	if (writelen == 0) {
		g_source_remove(bconv->tx_handler);
		bconv->tx_handler = 0;
		return;
	}

	ret = g_pollable_output_stream_write_nonblocking(
	        G_POLLABLE_OUTPUT_STREAM(stream),
	        purple_circular_buffer_get_output(bconv->tx_buf), writelen,
	        bconv->cancellable, &error);

	if (ret < 0 && error->code == G_IO_ERROR_WOULD_BLOCK) {
		g_clear_error(&error);
		return;
	} else if (ret <= 0) {
		PurpleConversation *conv = NULL;
		PurpleAccount *account = NULL;

		purple_debug_error(
		        "bonjour",
		        "Error sending message to buddy %s error: %s",
		        purple_buddy_get_name(pb),
		        error ? error->message : "(null)");

		account = purple_buddy_get_account(pb);

		conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bb->name, account));
		if (conv != NULL)
			purple_conversation_write_system_message(conv,
				_("Unable to send message."),
				PURPLE_MESSAGE_ERROR);

		bonjour_xmpp_close_conversation(bb->conversation);
		bb->conversation = NULL;
		g_clear_error(&error);
		return;
	}

	purple_circular_buffer_mark_read(bconv->tx_buf, ret);
}

static gint
_send_data(PurpleBuddy *pb, char *message)
{
	BonjourBuddy *bb = purple_buddy_get_protocol_data(pb);
	BonjourXMPPConversation *bconv = bb->conversation;
	gsize len = strlen(message);
	gssize ret;
	GError *error = NULL;

	/* If we're not ready to actually send, append it to the buffer */
	if (bconv->tx_handler != 0
			|| bconv->sent_stream_start != FULLY_SENT
			|| !bconv->recv_stream_start
			|| purple_circular_buffer_get_max_read(bconv->tx_buf) > 0) {
		ret = -1;
		g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK,
		                    "Not yet ready to send.");
	} else {
		ret = g_pollable_output_stream_write_nonblocking(
		        G_POLLABLE_OUTPUT_STREAM(bconv->output), message, len,
		        bconv->cancellable, &error);
	}

	if (ret == -1 && error->code == G_IO_ERROR_WOULD_BLOCK) {
		ret = 0;
		g_clear_error(&error);
	} else if (ret <= 0) {
		PurpleConversation *conv;
		PurpleAccount *account;

		purple_debug_error(
		        "bonjour",
		        "Error sending message to buddy %s error: %s",
		        purple_buddy_get_name(pb),
		        error ? error->message : "(null)");

		account = purple_buddy_get_account(pb);

		conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bb->name, account));
		if (conv != NULL)
			purple_conversation_write_system_message(conv,
				_("Unable to send message."),
				PURPLE_MESSAGE_ERROR);

		bonjour_xmpp_close_conversation(bb->conversation);
		bb->conversation = NULL;
		g_clear_error(&error);
		return -1;
	}

	if (ret < len) {
		/* Don't interfere with the stream starting */
		if (bconv->sent_stream_start == FULLY_SENT &&
		    bconv->recv_stream_start && bconv->tx_handler == 0) {
			GSource *source =
			        g_pollable_output_stream_create_source(
			                G_POLLABLE_OUTPUT_STREAM(bconv->output),
			                bconv->cancellable);
			g_source_set_callback(source,
			                      (GSourceFunc)_send_data_write_cb,
			                      pb, NULL);
			bconv->tx_handler = g_source_attach(source, NULL);
		}
		purple_circular_buffer_append(bconv->tx_buf, message + ret, len - ret);
	}

	return ret;
}

void bonjour_xmpp_process_packet(PurpleBuddy *pb, PurpleXmlNode *packet) {

	g_return_if_fail(packet != NULL);
	g_return_if_fail(pb != NULL);

	if (purple_strequal(packet->name, "message"))
		_xmpp_parse_and_write_message_to_ui(packet, pb);
	else if (purple_strequal(packet->name, "iq"))
		xep_iq_parse(packet, pb);
	else {
		purple_debug_warning("bonjour", "Unknown packet: %s\n",
			packet->name ? packet->name : "(null)");
	}
}

static void bonjour_xmpp_stream_ended(BonjourXMPPConversation *bconv) {

	/* Inform the user that the conversation has been closed */
	BonjourBuddy *bb = NULL;
	const gchar *name = bconv->pb ? purple_buddy_get_name(bconv->pb) : "(unknown)";

	purple_debug_info("bonjour", "Received conversation close notification from %s.\n", name);

	if(bconv->pb != NULL)
		bb = purple_buddy_get_protocol_data(bconv->pb);

	/* Close the socket, clear the watcher and free memory */
	bonjour_xmpp_close_conversation(bconv);
	if(bb)
		bb->conversation = NULL;
}

static gboolean
_client_socket_handler(GObject *stream, gpointer data)
{
	BonjourXMPPConversation *bconv = data;
	GError *error = NULL;
	gssize len;
	static char message[4096];

	/* Read the data from the socket */
	len = g_pollable_input_stream_read_nonblocking(
	        G_POLLABLE_INPUT_STREAM(stream), message, sizeof(message) - 1,
	        bconv->cancellable, &error);
	if (len == -1) {
		/* There has been an error reading from the socket */
		if (error == NULL || (error->code != G_IO_ERROR_WOULD_BLOCK &&
		                      error->code != G_IO_ERROR_CANCELLED)) {
			purple_debug_warning(
			        "bonjour",
			        "receive of %" G_GSSIZE_FORMAT " error: %s",
			        len, error ? error->message : "(null)");

			bonjour_xmpp_close_conversation(bconv);
			if (bconv->pb != NULL) {
				BonjourBuddy *bb = purple_buddy_get_protocol_data(bconv->pb);

				if(bb != NULL)
					bb->conversation = NULL;
			}

			/* I guess we really don't need to notify the user.
			 * If they try to send another message it'll reconnect */
		}
		g_clear_error(&error);
		return FALSE;
	} else if (len == 0) { /* The other end has closed the socket */
		const gchar *name = purple_buddy_get_name(bconv->pb);
		purple_debug_warning("bonjour", "Connection closed (without stream end) by %s.\n", (name) ? name : "(unknown)");
		bonjour_xmpp_stream_ended(bconv);
		return FALSE;
	}

	message[len] = '\0';

	purple_debug_info("bonjour", "Receive: -%s- %" G_GSSIZE_FORMAT " bytes\n", message, len);
	bonjour_parser_process(bconv, message, len);

	return TRUE;
}

struct _stream_start_data {
	char *msg;
};

static void
_start_stream(GObject *stream, gpointer data)
{
	BonjourXMPPConversation *bconv = data;
	struct _stream_start_data *ss = bconv->stream_data;
	GError *error = NULL;
	gsize len;
	gssize ret;

	len = strlen(ss->msg);

	/* Start Stream */
	ret = g_pollable_output_stream_write_nonblocking(
	        G_POLLABLE_OUTPUT_STREAM(stream), ss->msg, len,
	        bconv->cancellable, &error);

	if (ret == -1 && error->code == G_IO_ERROR_WOULD_BLOCK) {
		g_clear_error(&error);
		return;
	} else if (ret <= 0) {
		PurpleConversation *conv;
		const char *bname = bconv->buddy_name;
		BonjourBuddy *bb = NULL;

		if(bconv->pb) {
			bb = purple_buddy_get_protocol_data(bconv->pb);
			bname = purple_buddy_get_name(bconv->pb);
		}

		purple_debug_error(
		        "bonjour",
		        "Error starting stream with buddy %s at %s error: %s",
		        bname ? bname : "(unknown)", bconv->ip,
		        error ? error->message : "(null)");

		conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bname, bconv->account));
		if (conv != NULL)
			purple_conversation_write_system_message(conv,
				_("Unable to send the message, the conversation couldn't be started."),
				PURPLE_MESSAGE_ERROR);

		bonjour_xmpp_close_conversation(bconv);
		if(bb != NULL)
			bb->conversation = NULL;

		g_clear_error(&error);
		return;
	}

	/* This is EXTREMELY unlikely to happen */
	if (ret < len) {
		char *tmp = g_strdup(ss->msg + ret);
		g_free(ss->msg);
		ss->msg = tmp;
		return;
	}

	g_free(ss->msg);
	g_free(ss);
	bconv->stream_data = NULL;

	/* Stream started; process the send buffer if there is one */
	g_source_remove(bconv->tx_handler);
	bconv->tx_handler = 0;
	bconv->sent_stream_start = FULLY_SENT;

	bonjour_xmpp_stream_started(bconv);
}

static gboolean
bonjour_xmpp_send_stream_init(BonjourXMPPConversation *bconv,
                                GError **error)
{
	gchar *stream_start;
	gsize len;
	gssize ret;
	const char *bname = bconv->buddy_name;

	g_return_val_if_fail(error != NULL, FALSE);

	if (bconv->pb != NULL)
		bname = purple_buddy_get_name(bconv->pb);

	/* If we have no idea who "to" is, use an empty string.
	 * If we don't know now, it is because the other side isn't playing nice, so they can't complain. */
	if (bname == NULL)
		bname = "";

	stream_start = g_strdup_printf(DOCTYPE, bonjour_get_jid(bconv->account), bname);
	len = strlen(stream_start);

	bconv->sent_stream_start = PARTIALLY_SENT;

	/* Start the stream */
	ret = g_pollable_output_stream_write_nonblocking(
	        G_POLLABLE_OUTPUT_STREAM(bconv->output), stream_start, len,
	        bconv->cancellable, error);
	if (ret == -1 && (*error)->code == G_IO_ERROR_WOULD_BLOCK) {
		ret = 0;
		g_clear_error(error);
	} else if (ret <= 0) {
		purple_debug_error(
		        "bonjour",
		        "Error starting stream with buddy %s at %s error: %s",
		        (*bname) ? bname : "(unknown)", bconv->ip,
		        *error ? (*error)->message : "(null)");

		if (bconv->pb) {
			PurpleConversation *conv;
			conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bname, bconv->account));
			if (conv != NULL)
				purple_conversation_write_system_message(conv,
					_("Unable to send the message, the conversation couldn't be started."),
					PURPLE_MESSAGE_ERROR);
		}

		purple_gio_graceful_close(G_IO_STREAM(bconv->socket),
		                          G_INPUT_STREAM(bconv->input),
		                          G_OUTPUT_STREAM(bconv->output));
		g_clear_object(&bconv->socket);
		bconv->input = NULL;
		bconv->output = NULL;
		g_free(stream_start);

		return FALSE;
	}

	/* This is unlikely to happen */
	if (ret < len) {
		GSource *source;
		struct _stream_start_data *ss = g_new(struct _stream_start_data, 1);
		ss->msg = g_strdup(stream_start + ret);
		bconv->stream_data = ss;
		/* Finish sending the stream start */
		source = g_pollable_output_stream_create_source(
		        G_POLLABLE_OUTPUT_STREAM(bconv->output),
		        bconv->cancellable);
		g_source_set_callback(source, (GSourceFunc)_start_stream, bconv,
		                      NULL);
		bconv->tx_handler = g_source_attach(source, NULL);
	} else {
		bconv->sent_stream_start = FULLY_SENT;
	}

	g_free(stream_start);

	return TRUE;
}

/* This gets called when we've successfully sent our <stream:stream />
 * AND when we've received a <stream:stream /> */
void
bonjour_xmpp_stream_started(BonjourXMPPConversation *bconv)
{
	GError *error = NULL;

	if (bconv->sent_stream_start == NOT_SENT &&
	    !bonjour_xmpp_send_stream_init(bconv, &error)) {
		const char *bname = bconv->buddy_name;

		if (bconv->pb)
			bname = purple_buddy_get_name(bconv->pb);

		purple_debug_error(
		        "bonjour",
		        "Error starting stream with buddy %s at %s error: %s",
		        bname ? bname : "(unknown)", bconv->ip,
		        error ? error->message : "(null)");

		if (bconv->pb) {
			PurpleConversation *conv;
			conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bname, bconv->account));
			if (conv != NULL)
				purple_conversation_write_system_message(conv,
					_("Unable to send the message, the conversation couldn't be started."),
					PURPLE_MESSAGE_ERROR);
		}

		/* We don't want to recieve anything else */
		purple_gio_graceful_close(G_IO_STREAM(bconv->socket),
		                          G_INPUT_STREAM(bconv->input),
		                          G_OUTPUT_STREAM(bconv->output));
		g_clear_object(&bconv->socket);
		bconv->input = NULL;
		bconv->output = NULL;

		/* This must be asynchronous because it destroys the parser and we
		 * may be in the middle of parsing.
		 */
		async_bonjour_xmpp_close_conversation(bconv);
		g_clear_error(&error);
		return;
	}

	/* If the stream has been completely started and we know who we're talking to, we can start doing stuff. */
	/* I don't think the circ_buffer can actually contain anything without a buddy being associated, but lets be explicit. */
	if (bconv->sent_stream_start == FULLY_SENT && bconv->recv_stream_start
			&& bconv->pb && purple_circular_buffer_get_max_read(bconv->tx_buf) > 0) {
		/* Watch for when we can write the buffered messages */
		GSource *source = g_pollable_output_stream_create_source(
		        G_POLLABLE_OUTPUT_STREAM(bconv->output),
		        bconv->cancellable);
		g_source_set_callback(source, (GSourceFunc)_send_data_write_cb,
		                      bconv->pb, NULL);
		bconv->tx_handler = g_source_attach(source, NULL);
		/* We can probably write the data right now. */
		_send_data_write_cb(G_OBJECT(bconv->output), bconv->pb);
	}
}

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

static void
_server_socket_handler(GSocketService *service, GSocketConnection *connection,
                       GObject *source_object, gpointer data)
{
	BonjourXMPP *jdata = data;
	GSocketAddress *their_addr; /* connector's address information */
	GInetAddress *their_inet_addr;
	gchar *address_text;
	struct _match_buddies_by_address *mbba;
	BonjourXMPPConversation *bconv;
	GSList *buddies;
	GSource *source;

	their_addr = g_socket_connection_get_remote_address(connection, NULL);
	if (their_addr == NULL) {
		return;
	}
	their_inet_addr = g_inet_socket_address_get_address(
	        G_INET_SOCKET_ADDRESS(their_addr));

	/* Look for the buddy that has opened the conversation and fill information */
	address_text = g_inet_address_to_string(their_inet_addr);
	if (g_inet_address_get_family(their_inet_addr) ==
	            G_SOCKET_FAMILY_IPV6 &&
	    g_inet_address_get_is_link_local(their_inet_addr)) {
		gchar *tmp = g_strdup_printf(
		        "%s%%%d", address_text,
		        g_inet_socket_address_get_scope_id(
		                G_INET_SOCKET_ADDRESS(their_addr)));
		g_free(address_text);
		address_text = tmp;
	}
	g_object_unref(their_addr);

	purple_debug_info("bonjour", "Received incoming connection from %s.\n", address_text);
	mbba = g_new0(struct _match_buddies_by_address, 1);
	mbba->address = address_text;

	buddies = purple_blist_find_buddies(jdata->account, NULL);
	g_slist_foreach(buddies, _match_buddies_by_address, mbba);
	g_slist_free(buddies);

	if (mbba->matched_buddies == NULL) {
		purple_debug_info("bonjour", "We don't like invisible buddies, this is not a superheroes comic\n");
		g_free(address_text);
		g_free(mbba);
		return;
	}

	g_slist_free(mbba->matched_buddies);
	g_free(mbba);

	/* We've established that this *could* be from one of our buddies.
	 * Wait for the stream open to see if that matches too before assigning it.
	 */
	bconv = bonjour_xmpp_conv_new(NULL, jdata->account, address_text);

	/* We wait for the stream start before doing anything else */
	bconv->socket = g_object_ref(connection);
	bconv->input = g_io_stream_get_input_stream(G_IO_STREAM(bconv->socket));
	bconv->output =
	        g_io_stream_get_output_stream(G_IO_STREAM(bconv->socket));
	source = g_pollable_input_stream_create_source(
	        G_POLLABLE_INPUT_STREAM(bconv->input), bconv->cancellable);
	g_source_set_callback(source, (GSourceFunc)_client_socket_handler,
	                      bconv, NULL);
	bconv->rx_handler = g_source_attach(source, NULL);
	g_free(address_text);
}

gint
bonjour_xmpp_start(BonjourXMPP *jdata)
{
	GError *error = NULL;
	guint16 port;

	purple_debug_info("bonjour", "Attempting to bind IP socket to port %d.",
	                  jdata->port);

	/* Open a listening server for incoming conversations */
	jdata->service = g_socket_service_new();
	g_socket_listener_set_backlog(G_SOCKET_LISTENER(jdata->service), 10);
	port = jdata->port;
	if (!g_socket_listener_add_inet_port(G_SOCKET_LISTENER(jdata->service),
	                                     port, NULL, &error)) {
		purple_debug_info("bonjour",
		                  "Unable to bind to specified port %i: %s",
		                  port, error ? error->message : "(unknown)");
		g_clear_error(&error);
		port = g_socket_listener_add_any_inet_port(
		        G_SOCKET_LISTENER(jdata->service), NULL, &error);
		if (port == 0) {
			purple_debug_error(
			        "bonjour", "Unable to create socket: %s",
			        error ? error->message : "(unknown)");
			g_clear_error(&error);
			return -1;
		}
	}
	purple_debug_info("bonjour", "Bound IP socket to port %u.", port);
	jdata->port = port;

	g_signal_connect(G_OBJECT(jdata->service), "incoming",
	                 G_CALLBACK(_server_socket_handler), jdata);

	return jdata->port;
}

static void
_connected_to_buddy(GObject *source, GAsyncResult *res, gpointer user_data)
{
	PurpleBuddy *pb = user_data;
	BonjourBuddy *bb = purple_buddy_get_protocol_data(pb);
	GSocketConnection *conn;
	GSource *rx_source;
	GError *error = NULL;

	conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
	                                              res, &error);

	if (conn == NULL) {
		PurpleConversation *conv = NULL;
		PurpleAccount *account = NULL;
		GSList *tmp;

		if (error && error->code == G_IO_ERROR_CANCELLED) {
			/* This conversation was closed before it started. */
			g_error_free(error);
			return;
		}

		purple_debug_error("bonjour",
		                   "Error connecting to buddy %s at %s:%d "
		                   "(%s); Trying next IP address",
		                   purple_buddy_get_name(pb),
		                   bb->conversation->ip, bb->port_p2pj,
		                   error ? error->message : "(unknown)");
		g_clear_error(&error);

		/* There may be multiple entries for the same IP - one per
		 * presence recieved (e.g. multiple interfaces).
		 * We need to make sure that we find the previously used entry.
		 */
		tmp = g_slist_find(bb->ips, bb->conversation->ip_link);
		if (tmp)
			tmp = g_slist_next(tmp);

		account = purple_buddy_get_account(pb);

		if (tmp != NULL) {
			const gchar *ip;
			GSocketClient *client;

			bb->conversation->ip_link = ip = tmp->data;

			purple_debug_info("bonjour", "Starting conversation with %s at %s:%d\n",
					  purple_buddy_get_name(pb), ip, bb->port_p2pj);

			/* Make sure to connect without a proxy. */
			client = g_socket_client_new();
			if (client != NULL) {
				g_free(bb->conversation->ip);
				bb->conversation->ip = g_strdup(ip);
				g_socket_client_connect_to_host_async(
				        client, ip, bb->port_p2pj,
				        bb->conversation->cancellable,
				        _connected_to_buddy, pb);
				g_object_unref(client);
				return;
			}
		}

		purple_debug_error("bonjour", "No more addresses for buddy %s. Aborting", purple_buddy_get_name(pb));

		conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bb->name, account));
		if (conv != NULL)
			purple_conversation_write_system_message(conv,
				_("Unable to send the message, the conversation couldn't be started."),
				PURPLE_MESSAGE_ERROR);

		bonjour_xmpp_close_conversation(bb->conversation);
		bb->conversation = NULL;
		return;
	}

	bb->conversation->socket = conn;
	bb->conversation->input =
	        g_io_stream_get_input_stream(G_IO_STREAM(conn));
	bb->conversation->output =
	        g_io_stream_get_output_stream(G_IO_STREAM(conn));

	if (!bonjour_xmpp_send_stream_init(bb->conversation, &error)) {
		PurpleConversation *conv = NULL;
		PurpleAccount *account = NULL;

		purple_debug_error("bonjour",
		                   "Error starting stream with buddy %s at "
		                   "%s:%d error: %s",
		                   purple_buddy_get_name(pb),
		                   bb->conversation->ip, bb->port_p2pj,
		                   error ? error->message : "(null)");

		account = purple_buddy_get_account(pb);

		conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(bb->name, account));
		if (conv != NULL)
			purple_conversation_write_system_message(conv,
				_("Unable to send the message, the conversation couldn't be started."),
				PURPLE_MESSAGE_ERROR);

		bonjour_xmpp_close_conversation(bb->conversation);
		bb->conversation = NULL;
		g_clear_error(&error);
		return;
	}

	/* Start listening for the stream acknowledgement */
	rx_source = g_pollable_input_stream_create_source(
	        G_POLLABLE_INPUT_STREAM(bb->conversation->input),
	        bb->conversation->cancellable);
	g_source_set_callback(rx_source, (GSourceFunc)_client_socket_handler,
	                      bb->conversation, NULL);
	bb->conversation->rx_handler = g_source_attach(rx_source, NULL);
}

void
bonjour_xmpp_conv_match_by_name(BonjourXMPPConversation *bconv) {
	PurpleBuddy *pb = NULL;
	BonjourBuddy *bb = NULL;

	g_return_if_fail(bconv->ip != NULL);
	g_return_if_fail(bconv->pb == NULL);

	pb = purple_blist_find_buddy(bconv->account, bconv->buddy_name);
	if (pb && (bb = purple_buddy_get_protocol_data(pb))) {
		const char *ip;
		GSList *tmp = bb->ips;

		purple_debug_info("bonjour", "Found buddy %s for incoming conversation \"from\" attrib.\n",
			purple_buddy_get_name(pb));

		/* Check that one of the buddy's IPs matches */
		while(tmp) {
			ip = tmp->data;
			if (ip != NULL && g_ascii_strcasecmp(ip, bconv->ip) == 0) {
				PurpleConnection *pc = purple_account_get_connection(bconv->account);
				BonjourData *bd = purple_connection_get_protocol_data(pc);
				BonjourXMPP *jdata = bd->xmpp_data;

				purple_debug_info("bonjour", "Matched buddy %s to incoming conversation \"from\" attrib and IP (%s)\n",
					purple_buddy_get_name(pb), bconv->ip);

				/* Attach conv. to buddy and remove from pending list */
				jdata->pending_conversations = g_slist_remove(jdata->pending_conversations, bconv);

				/* Check if the buddy already has a conversation and, if so, replace it */
				if(bb->conversation != NULL && bb->conversation != bconv)
					bonjour_xmpp_close_conversation(bb->conversation);

				bconv->pb = pb;
				bb->conversation = bconv;

				break;
			}
			tmp = tmp->next;
		}
	}

	/* We've failed to match a buddy - give up */
	if (bconv->pb == NULL) {
		/* This must be asynchronous because it destroys the parser and we
		 * may be in the middle of parsing.
		 */
		async_bonjour_xmpp_close_conversation(bconv);
	}
}


void
bonjour_xmpp_conv_match_by_ip(BonjourXMPPConversation *bconv) {
	PurpleConnection *pc = purple_account_get_connection(bconv->account);
	BonjourData *bd = purple_connection_get_protocol_data(pc);
	BonjourXMPP *jdata = bd->xmpp_data;
	struct _match_buddies_by_address *mbba;
	GSList *buddies;

	mbba = g_new0(struct _match_buddies_by_address, 1);
	mbba->address = bconv->ip;

	buddies = purple_blist_find_buddies(jdata->account, NULL);
	g_slist_foreach(buddies, _match_buddies_by_address, mbba);
	g_slist_free(buddies);

	/* If there is exactly one match, use it */
	if(mbba->matched_buddies != NULL) {
		if(mbba->matched_buddies->next != NULL)
			purple_debug_error("bonjour", "More than one buddy matched for ip %s.\n", bconv->ip);
		else {
			PurpleBuddy *pb = mbba->matched_buddies->data;
			BonjourBuddy *bb = purple_buddy_get_protocol_data(pb);

			purple_debug_info("bonjour", "Matched buddy %s to incoming conversation using IP (%s)\n",
				purple_buddy_get_name(pb), bconv->ip);

			/* Attach conv. to buddy and remove from pending list */
			jdata->pending_conversations = g_slist_remove(jdata->pending_conversations, bconv);

			/* Check if the buddy already has a conversation and, if so, replace it */
			if (bb->conversation != NULL && bb->conversation != bconv)
				bonjour_xmpp_close_conversation(bb->conversation);

			bconv->pb = pb;
			bb->conversation = bconv;
		}
	} else
		purple_debug_error("bonjour", "No buddies matched for ip %s.\n", bconv->ip);

	/* We've failed to match a buddy - give up */
	if (bconv->pb == NULL) {
		/* This must be asynchronous because it destroys the parser and we
		 * may be in the middle of parsing.
		 */
		async_bonjour_xmpp_close_conversation(bconv);
	}

	g_slist_free(mbba->matched_buddies);
	g_free(mbba);
}

static PurpleBuddy *
_find_or_start_conversation(BonjourXMPP *jdata, const gchar *to)
{
	PurpleBuddy *pb = NULL;
	BonjourBuddy *bb = NULL;

	g_return_val_if_fail(jdata != NULL, NULL);
	g_return_val_if_fail(to != NULL, NULL);

	pb = purple_blist_find_buddy(jdata->account, to);
	if (pb == NULL || (bb = purple_buddy_get_protocol_data(pb)) == NULL)
		/* You can not send a message to an offline buddy */
		return NULL;

	/* Check if there is a previously open conversation */
	if (bb->conversation == NULL) {
		GSocketClient *client;
		/* Start with the first IP address. */
		const gchar *ip = bb->ips->data;

		purple_debug_info("bonjour",
		                  "Starting conversation with %s at %s:%d", to,
		                  ip, bb->port_p2pj);

		/* Make sure to connect without a proxy. */
		client = g_socket_client_new();
		if (client == NULL) {
			purple_debug_error("bonjour",
			                   "Unable to connect to buddy (%s).",
			                   to);
			return NULL;
		}

		bb->conversation = bonjour_xmpp_conv_new(pb, jdata->account, ip);
		bb->conversation->ip_link = ip;

		g_socket_client_connect_to_host_async(
		        client, ip, bb->port_p2pj,
		        bb->conversation->cancellable, _connected_to_buddy, pb);
		g_object_unref(client);
	}
	return pb;
}

int
bonjour_xmpp_send_message(BonjourXMPP *jdata, const gchar *to, const gchar *body)
{
	PurpleXmlNode *message_node, *node, *node2;
	gchar *message, *xhtml;
	PurpleBuddy *pb;
	BonjourBuddy *bb;
	int ret;

	pb = _find_or_start_conversation(jdata, to);
	if (pb == NULL || (bb = purple_buddy_get_protocol_data(pb)) == NULL) {
		purple_debug_info("bonjour", "Can't send a message to an offline buddy (%s).\n", to);
		/* You can not send a message to an offline buddy */
		return -10000;
	}

	purple_markup_html_to_xhtml(body, &xhtml, &message);

	message_node = purple_xmlnode_new("message");
	purple_xmlnode_set_attrib(message_node, "to", bb->name);
	purple_xmlnode_set_attrib(message_node, "from", bonjour_get_jid(jdata->account));
	purple_xmlnode_set_attrib(message_node, "type", "chat");

	/* Enclose the message from the UI within a "font" node */
	node = purple_xmlnode_new_child(message_node, "body");
	purple_xmlnode_insert_data(node, message, strlen(message));
	g_free(message);

	node = purple_xmlnode_new_child(message_node, "html");
	purple_xmlnode_set_namespace(node, "http://www.w3.org/1999/xhtml");

	node = purple_xmlnode_new_child(node, "body");
	message = g_strdup_printf("<font>%s</font>", xhtml);
	node2 = purple_xmlnode_from_str(message, strlen(message));
	g_free(xhtml);
	g_free(message);
	purple_xmlnode_insert_child(node, node2);

	node = purple_xmlnode_new_child(message_node, "x");
	purple_xmlnode_set_namespace(node, "xmpp:x:event");
	purple_xmlnode_insert_child(node, purple_xmlnode_new("composing"));

	message = purple_xmlnode_to_str(message_node, NULL);
	purple_xmlnode_free(message_node);

	ret = _send_data(pb, message) >= 0;

	g_free(message);

	return ret;
}

static gboolean
_async_bonjour_xmpp_close_conversation_cb(gpointer data) {
	BonjourXMPPConversation *bconv = data;
	bonjour_xmpp_close_conversation(bconv);
	return FALSE;
}

void
async_bonjour_xmpp_close_conversation(BonjourXMPPConversation *bconv) {
	PurpleConnection *pc = purple_account_get_connection(bconv->account);
	BonjourData *bd = purple_connection_get_protocol_data(pc);
	BonjourXMPP *jdata = bd->xmpp_data;

	jdata->pending_conversations = g_slist_remove(jdata->pending_conversations, bconv);

	/* Disconnect this conv. from the buddy here so it can't be disposed of twice.*/
	if(bconv->pb != NULL) {
		BonjourBuddy *bb = purple_buddy_get_protocol_data(bconv->pb);
		if (bb->conversation == bconv)
			bb->conversation = NULL;
	}

	bconv->close_timeout = g_timeout_add(0, _async_bonjour_xmpp_close_conversation_cb, bconv);
}

void
bonjour_xmpp_close_conversation(BonjourXMPPConversation *bconv)
{
	BonjourData *bd = NULL;
	PurpleConnection *pc = NULL;

	if (bconv == NULL) {
		return;
	}

	pc = purple_account_get_connection(bconv->account);
	PURPLE_ASSERT_CONNECTION_IS_VALID(pc);

	bd = purple_connection_get_protocol_data(pc);
	if (bd) {
		bd->xmpp_data->pending_conversations = g_slist_remove(
			bd->xmpp_data->pending_conversations, bconv);
	}

	/* Cancel any file transfers that are waiting to begin */
	/* There wont be any transfers if it hasn't been attached to a buddy */
	if (bconv->pb != NULL && bd != NULL) {
		GSList *xfers, *tmp_next;
		xfers = bd->xfer_lists;
		while (xfers != NULL) {
			PurpleXfer *xfer = xfers->data;
			tmp_next = xfers->next;
			/* We only need to cancel this if it hasn't actually started transferring. */
			/* This will change if we ever support IBB transfers. */
			if (purple_strequal(purple_xfer_get_remote_user(xfer), purple_buddy_get_name(bconv->pb))
					&& (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_NOT_STARTED
						|| purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_UNKNOWN)) {
				purple_xfer_cancel_remote(xfer);
			}
			xfers = tmp_next;
		}
	}

	/* Close the socket and remove the watcher */
	if (bconv->socket != NULL) {
		/* Send the end of the stream to the other end of the conversation */
		if (bconv->sent_stream_start == FULLY_SENT) {
			size_t len = strlen(STREAM_END);
			if (g_pollable_output_stream_write_nonblocking(
			            G_POLLABLE_OUTPUT_STREAM(bconv->output),
			            STREAM_END, len, bconv->cancellable,
			            NULL) != (gssize)len) {
				purple_debug_error("bonjour",
					"bonjour_xmpp_close_conversation: "
					"couldn't send data\n");
			}
		}
		/* TODO: We're really supposed to wait for "</stream:stream>" before closing the socket */
		purple_gio_graceful_close(G_IO_STREAM(bconv->socket),
		                          G_INPUT_STREAM(bconv->input),
		                          G_OUTPUT_STREAM(bconv->output));
	}
	if (bconv->rx_handler != 0) {
		g_source_remove(bconv->rx_handler);
		bconv->rx_handler = 0;
	}
	if (bconv->tx_handler != 0) {
		g_source_remove(bconv->tx_handler);
		bconv->tx_handler = 0;
	}

	/* Cancel any pending operations. */
	if (bconv->cancellable != NULL) {
		g_cancellable_cancel(bconv->cancellable);
		g_clear_object(&bconv->cancellable);
	}

	/* Free all the data related to the conversation */
	g_clear_object(&bconv->socket);
	bconv->input = NULL;
	bconv->output = NULL;

	g_object_unref(G_OBJECT(bconv->tx_buf));
	if (bconv->stream_data != NULL) {
		struct _stream_start_data *ss = bconv->stream_data;
		g_free(ss->msg);
		g_free(ss);
	}

	if (bconv->context != NULL) {
		bonjour_parser_setup(bconv);
	}

	if (bconv->close_timeout != 0) {
		g_source_remove(bconv->close_timeout);
	}

	g_free(bconv->buddy_name);
	g_free(bconv->ip);
	g_free(bconv);
}

void
bonjour_xmpp_stop(BonjourXMPP *jdata)
{
	/* Close the server socket and remove the watcher */
	if (jdata->service) {
		g_socket_service_stop(jdata->service);
		g_socket_listener_close(G_SOCKET_LISTENER(jdata->service));
		g_clear_object(&jdata->service);
	}

	/* Close all the conversation sockets and remove all the watchers after sending end streams */
	if (!purple_account_is_disconnected(jdata->account)) {
		GSList *buddies, *l;

		buddies = purple_blist_find_buddies(jdata->account, NULL);
		for (l = buddies; l; l = l->next) {
			BonjourBuddy *bb = purple_buddy_get_protocol_data((PurpleBuddy*) l->data);
			if (bb && bb->conversation) {
				/* Any ongoing connection attempt is cancelled
				 * when a connection is destroyed */
				bonjour_xmpp_close_conversation(bb->conversation);
				bb->conversation = NULL;
			}
		}

		g_slist_free(buddies);
	}

	g_slist_free_full(jdata->pending_conversations, (GDestroyNotify)bonjour_xmpp_close_conversation);
}

XepIq *
xep_iq_new(void *data, XepIqType type, const char *to, const char *from, const char *id)
{
	PurpleXmlNode *iq_node = NULL;
	XepIq *iq = NULL;

	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(to != NULL, NULL);
	g_return_val_if_fail(id != NULL, NULL);

	iq_node = purple_xmlnode_new("iq");

	purple_xmlnode_set_attrib(iq_node, "to", to);
	purple_xmlnode_set_attrib(iq_node, "from", from);
	purple_xmlnode_set_attrib(iq_node, "id", id);
	switch (type) {
		case XEP_IQ_SET:
			purple_xmlnode_set_attrib(iq_node, "type", "set");
			break;
		case XEP_IQ_GET:
			purple_xmlnode_set_attrib(iq_node, "type", "get");
			break;
		case XEP_IQ_RESULT:
			purple_xmlnode_set_attrib(iq_node, "type", "result");
			break;
		case XEP_IQ_ERROR:
			purple_xmlnode_set_attrib(iq_node, "type", "error");
			break;
		case XEP_IQ_NONE:
		default:
			purple_xmlnode_set_attrib(iq_node, "type", "none");
			break;
	}

	iq = g_new0(XepIq, 1);
	iq->node = iq_node;
	iq->type = type;
	iq->data = ((BonjourData*)data)->xmpp_data;
	iq->to = (char*)to;

	return iq;
}

static gboolean
check_if_blocked(PurpleBuddy *pb)
{
	gboolean blocked = FALSE;
	GSList *l = NULL;
	PurpleAccount *acc = purple_buddy_get_account(pb);
	const gchar *name;

	if(acc == NULL)
		return FALSE;

	l = purple_account_privacy_get_denied(acc);
	name = purple_buddy_get_name(pb);

	if(g_slist_find_custom(l, name, (GCompareFunc)purple_utf8_strcasecmp) != NULL) {
		const gchar *username = bonjour_get_jid(acc);

		purple_debug_info("bonjour", "%s has been blocked by %s.\n", name, username);
		blocked = TRUE;
	}
	return blocked;
}

static void
xep_iq_parse(PurpleXmlNode *packet, PurpleBuddy *pb)
{
	PurpleAccount *account;
	PurpleConnection *gc;

	if(check_if_blocked(pb))
		return;

	account = purple_buddy_get_account(pb);
	gc = purple_account_get_connection(account);

	if (purple_xmlnode_get_child(packet, "si") != NULL || purple_xmlnode_get_child(packet, "error") != NULL)
		xep_si_parse(gc, packet, pb);
	else
		xep_bytestreams_parse(gc, packet, pb);
}

int
xep_iq_send_and_free(XepIq *iq)
{
	int ret = -1;
	PurpleBuddy *pb = NULL;

	/* start the talk, reuse the message socket  */
	pb = _find_or_start_conversation((BonjourXMPP*) iq->data, iq->to);
	/* Send the message */
	if (pb != NULL) {
		/* Convert xml node into stream */
		gchar *msg = purple_xmlnode_to_str(iq->node, NULL);
		ret = _send_data(pb, msg);
		g_free(msg);
	}

	purple_xmlnode_free(iq->node);
	iq->node = NULL;
	g_free(iq);

	return (ret >= 0) ? 0 : -1;
}

/* This returns a list containing all non-localhost IPs */
GSList *
bonjour_xmpp_get_local_ips(int fd)
{
	GSList *ips = NULL;
	const char *address_text;
	int ret;

#ifdef HAVE_GETIFADDRS /* This is required for IPv6 */
	struct ifaddrs *ifap, *ifa;
	common_sockaddr_t addr;
	char addrstr[INET6_ADDRSTRLEN];

	ret = getifaddrs(&ifap);
	if (ret != 0) {
		const char *error = g_strerror(errno);
		purple_debug_error("bonjour", "getifaddrs() error: %s\n", error ? error : "(null)");
		return NULL;
	}

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_flags & IFF_RUNNING) || (ifa->ifa_flags & IFF_LOOPBACK) || ifa->ifa_addr == NULL)
			continue;

		memcpy(&addr, ifa->ifa_addr, sizeof(addr));
		address_text = NULL;
		switch (addr.sa.sa_family) {
			case AF_INET:
				address_text = inet_ntop(addr.sa.sa_family,
					&addr.in.sin_addr,
					addrstr, sizeof(addrstr));
				break;
#ifdef PF_INET6
			case AF_INET6:
				address_text = inet_ntop(addr.sa.sa_family,
					&addr.in6.sin6_addr,
					addrstr, sizeof(addrstr));
				break;
#endif
		}

		if (address_text != NULL) {
			if (addr.sa.sa_family == AF_INET)
				ips = g_slist_append(ips, g_strdup(address_text));
			else
				ips = g_slist_prepend(ips, g_strdup(address_text));
		}
	}

	freeifaddrs(ifap);
#else
	char *tmp;
	struct ifconf ifc;
	struct ifreq *ifr;
	char buffer[1024];
	struct sockaddr_in *sinptr;
	int source = fd;

	if (fd < 0)
		source = socket(PF_INET, SOCK_STREAM, 0);

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_req = (struct ifreq *)buffer;
	ret = ioctl(source, SIOCGIFCONF, &ifc);

	if (fd < 0)
		close(source);

	if (ret < 0) {
		const char *error = g_strerror(errno);
		purple_debug_error("bonjour", "ioctl(SIOCGIFCONF) error: %s\n", error ? error : "(null)");
		return NULL;
	}

	tmp = buffer;
	while (tmp < buffer + ifc.ifc_len) {
		ifr = (struct ifreq *)tmp;
		tmp += HX_SIZE_OF_IFREQ(*ifr);

		if (ifr->ifr_addr.sa_family == AF_INET) {
			sinptr = (struct sockaddr_in *)&ifr->ifr_addr;
			if ((ntohl(sinptr->sin_addr.s_addr) >> 24) != 127) {
				address_text = inet_ntoa(sinptr->sin_addr);
				ips = g_slist_prepend(ips, g_strdup(address_text));
			}
		}
	}
#endif

	return ips;
}

void
append_iface_if_linklocal(char *ip, guint32 interface_param) {
	struct in6_addr in6_addr;
	int len_remain = INET6_ADDRSTRLEN - strlen(ip);

	if (len_remain <= 1)
		return;

	if (inet_pton(AF_INET6, ip, &in6_addr) != 1 ||
	    !IN6_IS_ADDR_LINKLOCAL(&in6_addr))
		return;

	snprintf(ip + strlen(ip), len_remain, "%%%d",
		 interface_param);
}
