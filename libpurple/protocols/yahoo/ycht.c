/**
 * @file ycht.c The Yahoo! protocol plugin, YCHT protocol stuff.
 *
 * purple
 *
 * Copyright (C) 2004 Timothy Ringenbach <omarvo@hotmail.com>
 * Liberal amounts of code borrowed from the rest of the Yahoo! prpl.
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
#include "prpl.h"
#include "notify.h"
#include "account.h"
#include "proxy.h"
#include "debug.h"
#include "conversation.h"
#include "util.h"

#include "libymsg.h"
#include "yahoo_packet.h"
#include "ycht.h"
#include "yahoochat.h"

/*
 * dword: YCHT
 * dword: 0x000000AE
 * dword: service
 * word:  status
 * word:  size
 */
#define YAHOO_CHAT_ID (1)
/************************************************************************************
 * Functions to process various kinds of packets.
 ************************************************************************************/
static void ycht_process_login(YchtConn *ycht, YchtPkt *pkt)
{
	PurpleConnection *gc = ycht->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	if (ycht->logged_in)
		return;

	yd->chat_online = TRUE;
	ycht->logged_in = TRUE;

	if (ycht->room)
		ycht_chat_join(ycht, ycht->room);
}

static void ycht_process_logout(YchtConn *ycht, YchtPkt *pkt)
{
	PurpleConnection *gc = ycht->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	yd->chat_online = FALSE;
	ycht->logged_in = FALSE;
}

static void ycht_process_chatjoin(YchtConn *ycht, YchtPkt *pkt)
{
	char *room, *topic;
	PurpleConnection *gc = ycht->gc;
	PurpleChatConversation *c = NULL;
	gboolean new_room = FALSE;
	char **members;
	int i;

	room = g_list_nth_data(pkt->data, 0);
	topic = g_list_nth_data(pkt->data, 1);
	if (!g_list_nth_data(pkt->data, 4))
		return;
	if (!room)
		return;

	members = g_strsplit(g_list_nth_data(pkt->data, 4), "\001", 0);
	for (i = 0; members[i]; i++) {
		char *tmp = strchr(members[i], '\002');
		if (tmp)
			*tmp = '\0';
	}

	if (g_list_length(pkt->data) > 5)
		new_room = TRUE;

	if (new_room && ycht->changing_rooms) {
		purple_serv_got_chat_left(gc, YAHOO_CHAT_ID);
		ycht->changing_rooms = FALSE;
		c = purple_serv_got_joined_chat(gc, YAHOO_CHAT_ID, room);
	} else {
		c = purple_conversations_find_chat(gc, YAHOO_CHAT_ID);
	}

	if (topic)
		purple_chat_conversation_set_topic(c, NULL, topic);

	for (i = 0; members[i]; i++) {
		if (new_room) {
			/*if (!strcmp(members[i], purple_connection_get_display_name(ycht->gc)))
				continue;*/
			purple_chat_conversation_add_user(c, members[i], NULL, PURPLE_CHAT_USER_NONE, TRUE);
		} else {
			yahoo_chat_add_user(c, members[i], NULL);
		}
	}

	g_strfreev(members);
}

static void ycht_process_chatpart(YchtConn *ycht, YchtPkt *pkt)
{
	char *room, *who;

	room = g_list_nth_data(pkt->data, 0);
	who = g_list_nth_data(pkt->data, 1);

	if (who && room) {
		PurpleChatConversation *c = purple_conversations_find_chat(ycht->gc, YAHOO_CHAT_ID);
		if (c && !purple_utf8_strcasecmp(purple_conversation_get_name(PURPLE_CONVERSATION(c)), room))
			purple_chat_conversation_remove_user(c, who, NULL);

	}
}

static void ycht_progress_chatmsg(YchtConn *ycht, YchtPkt *pkt)
{
	char *who, *what, *msg;
	PurpleChatConversation *c;
	PurpleConnection *gc = ycht->gc;

	who = g_list_nth_data(pkt->data, 1);
	what = g_list_nth_data(pkt->data, 2);

	if (!who || !what)
		return;

	c = purple_conversations_find_chat(gc, YAHOO_CHAT_ID);
	if (!c)
		return;

	msg = yahoo_string_decode(gc, what, 1);
	what = yahoo_codes_to_html(msg);
	g_free(msg);

	if (pkt->service == YCHT_SERVICE_CHATMSG_EMOTE) {
		char *tmp = g_strdup_printf("/me %s", what);
		g_free(what);
		what = tmp;
	}

	purple_serv_got_chat_in(gc, YAHOO_CHAT_ID, who, PURPLE_MESSAGE_RECV, what, time(NULL));
	g_free(what);
}

static void ycht_progress_online_friends(YchtConn *ycht, YchtPkt *pkt)
{
#if 0
	PurpleConnection *gc = ycht->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	if (ycht->logged_in)
		return;

	yd->chat_online = TRUE;
	ycht->logged_in = TRUE;

	if (ycht->room)
		ycht_chat_join(ycht, ycht->room);
#endif
}

/*****************************************************************************
 * Functions dealing with YCHT packets and their contents directly.
 *****************************************************************************/
static void ycht_packet_dump(const guchar *data, int len)
{
#ifdef YAHOO_YCHT_DEBUG
	int i;

	purple_debug_misc("yahoo", "");

	for (i = 0; i + 1 < len; i += 2) {
		if ((i % 16 == 0) && i) {
			purple_debug_misc(NULL, "\n");
			purple_debug_misc("yahoo", "");
		}

		purple_debug_misc(NULL, "%02hhx%02hhx ", data[i], data[i + 1]);
	}
	if (i < len)
		purple_debug_misc(NULL, "%02hhx", data[i]);

	purple_debug_misc(NULL, "\n");
	purple_debug_misc("yahoo", "");

	for (i = 0; i < len; i++) {
		if ((i % 16 == 0) && i) {
			purple_debug_misc(NULL, "\n");
			purple_debug_misc("yahoo", "");
		}

		if (g_ascii_isprint(data[i]))
			purple_debug_misc(NULL, "%c ", data[i]);
		else
			purple_debug_misc(NULL, ". ");
	}

	purple_debug_misc(NULL, "\n");
#endif /* YAHOO_YCHT_DEBUG */
}

static YchtPkt *ycht_packet_new(guint version, guint service, int status)
{
	YchtPkt *ret;

	ret = g_new0(YchtPkt, 1);

	ret->version = version;
	ret->service = service;
	ret->status = status;

	return ret;
}

static void ycht_packet_append(YchtPkt *pkt, const char *str)
{
	g_return_if_fail(pkt != NULL);
	g_return_if_fail(str != NULL);

	pkt->data = g_list_append(pkt->data, g_strdup(str));
}

static int ycht_packet_length(YchtPkt *pkt)
{
	int ret;
	GList *l;

	ret = YCHT_HEADER_LEN;

	for (l = pkt->data; l; l = l->next) {
		ret += strlen(l->data);
		if (l->next)
			ret += strlen(YCHT_SEP);
	}

	return ret;
}

static void ycht_packet_send_write_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	YchtConn *ycht = data;
	int ret, writelen;
	const gchar *output = NULL;

	writelen = purple_circular_buffer_get_max_read(ycht->txbuf);

	if (writelen == 0) {
		purple_input_remove(ycht->tx_handler);
		ycht->tx_handler = 0;
		return;
	}

	output = purple_circular_buffer_get_output(ycht->txbuf);

	ret = write(ycht->fd, output, writelen);

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0) {
		/* TODO: error handling */
/*
		gchar *tmp = g_strdup_printf(_("Lost connection with server: %s"),
				g_strerror(errno));
		purple_connection_error(purple_account_get_connection(irc->account),
			      PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
*/
		return;
	}

	purple_circular_buffer_mark_read(ycht->txbuf, ret);

}

static void ycht_packet_send(YchtConn *ycht, YchtPkt *pkt)
{
	int len, pos, written;
	char *buf;
	GList *l;

	g_return_if_fail(ycht != NULL);
	g_return_if_fail(pkt != NULL);
	g_return_if_fail(ycht->fd != -1);

	pos = 0;
	len = ycht_packet_length(pkt);
	buf = g_malloc(len);

	memcpy(buf + pos, "YCHT", 4); pos += 4;
	pos += yahoo_put32(buf + pos, pkt->version);
	pos += yahoo_put32(buf + pos, pkt->service);
	pos += yahoo_put16(buf + pos, pkt->status);
	pos += yahoo_put16(buf + pos, len - YCHT_HEADER_LEN);

	for (l = pkt->data; l; l = l->next) {
		int slen = strlen(l->data);
		memcpy(buf + pos, l->data, slen); pos += slen;

		if (l->next) {
			memcpy(buf + pos, YCHT_SEP, strlen(YCHT_SEP));
			pos += strlen(YCHT_SEP);
		}
	}

	if (!ycht->tx_handler)
		written = write(ycht->fd, buf, len);
	else {
		written = -1;
		errno = EAGAIN;
	}

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		/* TODO: Error handling (was none before NBIO changes) */
		written = 0;
	}

	if (written < len) {
		if (!ycht->tx_handler)
			ycht->tx_handler = purple_input_add(ycht->fd,
				PURPLE_INPUT_WRITE, ycht_packet_send_write_cb,
				ycht);
		purple_circular_buffer_append(ycht->txbuf, buf + written,
			len - written);
	}

	g_free(buf);
}

static void ycht_packet_read(YchtPkt *pkt, const char *buf, int len)
{
	const char *pos = buf;
	const char *needle;
	char *tmp, *tmp2;
	int i = 0;

	while (len > 0 && (needle = g_strstr_len(pos, len, YCHT_SEP))) {
		tmp = g_strndup(pos, needle - pos);
		pkt->data = g_list_append(pkt->data, tmp);
		len -= needle - pos + strlen(YCHT_SEP);
		pos = needle + strlen(YCHT_SEP);
		tmp2 = g_strescape(tmp, NULL);
		purple_debug_misc("yahoo", "Data[%d]:\t%s\n", i++, tmp2);
		g_free(tmp2);
	}

	if (len) {
		tmp = g_strndup(pos, len);
		pkt->data = g_list_append(pkt->data, tmp);
		tmp2 = g_strescape(tmp, NULL);
		purple_debug_misc("yahoo", "Data[%d]:\t%s\n", i, tmp2);
		g_free(tmp2);
	};

	purple_debug_misc("yahoo", "--==End of incoming YCHT packet==--\n");
}

static void ycht_packet_process(YchtConn *ycht, YchtPkt *pkt)
{
	if (pkt->data && !strncmp(pkt->data->data, "*** Danger Will Robinson!!!", strlen("*** Danger Will Robinson!!!")))
		return;

	switch (pkt->service) {
	case YCHT_SERVICE_LOGIN:
		ycht_process_login(ycht, pkt);
		break;
	case YCHT_SERVICE_LOGOUT:
		ycht_process_logout(ycht, pkt);
		break;
	case YCHT_SERVICE_CHATJOIN:
		ycht_process_chatjoin(ycht, pkt);
		break;
	case YCHT_SERVICE_CHATPART:
		ycht_process_chatpart(ycht, pkt);
		break;
	case YCHT_SERVICE_CHATMSG:
	case YCHT_SERVICE_CHATMSG_EMOTE:
		ycht_progress_chatmsg(ycht, pkt);
		break;
	case YCHT_SERVICE_ONLINE_FRIENDS:
		ycht_progress_online_friends(ycht, pkt);
		break;
	default:
		purple_debug_warning("yahoo", "YCHT: warning, unhandled service 0x%02x\n", pkt->service);
	}
}

static void ycht_packet_free(YchtPkt *pkt)
{
	g_return_if_fail(pkt != NULL);

	g_list_free_full(pkt->data, g_free);
	g_free(pkt);
}

/************************************************************************************
 * Functions dealing with connecting and disconnecting and reading data into YchtPkt
 * structs, and all that stuff.
 ************************************************************************************/

void ycht_connection_close(YchtConn *ycht)
{
	YahooData *yd = purple_connection_get_protocol_data(ycht->gc);

	if (yd) {
		yd->ycht = NULL;
		yd->chat_online = FALSE;
	}

	if (ycht->fd > 0)
		close(ycht->fd);
	if (ycht->inpa)
		purple_input_remove(ycht->inpa);

	if (ycht->tx_handler)
		purple_input_remove(ycht->tx_handler);

	g_object_unref(G_OBJECT(ycht->txbuf));

	g_free(ycht->rxqueue);

	g_free(ycht);
}

static void ycht_connection_error(YchtConn *ycht, const gchar *error)
{

	purple_notify_info(ycht->gc, NULL,
		_("Connection problem with the YCHT server"), error,
		purple_request_cpar_from_connection(ycht->gc));
	ycht_connection_close(ycht);
}

static void ycht_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	YchtConn *ycht = data;
	char buf[1024];
	int len;

	len = read(ycht->fd, buf, sizeof(buf));

	if (len < 0) {
		gchar *tmp;

		if (errno == EAGAIN)
			/* No worries */
			return;

		tmp = g_strdup_printf(_("Lost connection with server: %s"),
				g_strerror(errno));
		ycht_connection_error(ycht, tmp);
		g_free(tmp);
		return;
	} else if (len == 0) {
		ycht_connection_error(ycht, _("Server closed the connection"));
		return;
	}

	ycht->rxqueue = g_realloc(ycht->rxqueue, len + ycht->rxlen);
	memcpy(ycht->rxqueue + ycht->rxlen, buf, len);
	ycht->rxlen += len;

	while (1) {
		YchtPkt *pkt;
		int pos = 0;
		guint pktlen;
		guint service;
		guint version;
		gint status;

		if (ycht->rxlen < YCHT_HEADER_LEN)
			return;

		if (strncmp("YCHT", (char *)ycht->rxqueue, 4) != 0)
			purple_debug_error("yahoo", "YCHT: protocol error.\n");

		pos += 4; /* YCHT */

		version = yahoo_get32(ycht->rxqueue + pos); pos += 4;
		service = yahoo_get32(ycht->rxqueue + pos); pos += 4;
		status = yahoo_get16(ycht->rxqueue + pos); pos += 2;
		pktlen  = yahoo_get16(ycht->rxqueue + pos); pos += 2;
		purple_debug_misc("yahoo", "ycht: %d bytes to read, rxlen is %d\n",
				pktlen, ycht->rxlen);

		if (ycht->rxlen < (YCHT_HEADER_LEN + pktlen))
			return;

		purple_debug_misc("yahoo", "--==Incoming YCHT packet==--\n");
		purple_debug_misc("yahoo", "YCHT Service: 0x%02x Version: 0x%02x Status: 0x%02x\n",
				service, version, status);
		ycht_packet_dump(ycht->rxqueue, YCHT_HEADER_LEN + pktlen);

		pkt = ycht_packet_new(version, service, status);
		ycht_packet_read(pkt, (char *)ycht->rxqueue + pos, pktlen);

		ycht->rxlen -= YCHT_HEADER_LEN + pktlen;
		if (ycht->rxlen) {
			guchar *tmp = g_memdup(ycht->rxqueue + YCHT_HEADER_LEN + pktlen, ycht->rxlen);
			g_free(ycht->rxqueue);
			ycht->rxqueue = tmp;
		} else {
			g_free(ycht->rxqueue);
			ycht->rxqueue = NULL;
		}

		ycht_packet_process(ycht, pkt);

		ycht_packet_free(pkt);
	}
}

static void ycht_got_connected(gpointer data, gint source, const gchar *error_message)
{
	YchtConn *ycht = data;
	PurpleConnection *gc = ycht->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	YchtPkt *pkt;
	char *buf;

	if (source < 0) {
		ycht_connection_error(ycht, _("Unable to connect"));
		return;
	}

	ycht->fd = source;

	pkt = ycht_packet_new(YCHT_VERSION, YCHT_SERVICE_LOGIN, 0);

	buf = g_strdup_printf("%s\001Y=%s; T=%s", purple_connection_get_display_name(gc), yd->cookie_y, yd->cookie_t);
	ycht_packet_append(pkt, buf);
	g_free(buf);

	ycht_packet_send(ycht, pkt);

	ycht_packet_free(pkt);

	ycht->inpa = purple_input_add(ycht->fd, PURPLE_INPUT_READ, ycht_pending, ycht);
}

void ycht_connection_open(PurpleConnection *gc)
{
	YchtConn *ycht;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);

	ycht = g_new0(YchtConn, 1);
	ycht->gc = gc;
	ycht->fd = -1;

	yd->ycht = ycht;

	if (purple_proxy_connect(gc, account,
	                       purple_account_get_string(account, "ycht-server",  YAHOO_YCHT_HOST),
	                       purple_account_get_int(account, "ycht-port", YAHOO_YCHT_PORT),
	                       ycht_got_connected, ycht) == NULL)
	{
		ycht_connection_error(ycht, _("Unable to connect"));
		return;
	}
}

/*******************************************************************************************
 * These are functions called because the user did something.
 *******************************************************************************************/

void ycht_chat_join(YchtConn *ycht, const char *room)
{
	YchtPkt *pkt;
	char *tmp;

	tmp = g_strdup(room);
	g_free(ycht->room);
	ycht->room = tmp;

	if (!ycht->logged_in)
		return;

	ycht->changing_rooms = TRUE;
	pkt = ycht_packet_new(YCHT_VERSION, YCHT_SERVICE_CHATJOIN, 0);
	ycht_packet_append(pkt, ycht->room);
	ycht_packet_send(ycht, pkt);
	ycht_packet_free(pkt);
}

int ycht_chat_send(YchtConn *ycht, const char *room, const char *what)
{
	YchtPkt *pkt;
	char *msg1, *msg2, *buf;

	if (strcmp(room, ycht->room))
		purple_debug_warning("yahoo", "uhoh, sending to the wrong room!\n");

	pkt = ycht_packet_new(YCHT_VERSION, YCHT_SERVICE_CHATMSG, 0);

	msg1 = yahoo_html_to_codes(what);
	msg2 = yahoo_string_encode(ycht->gc, msg1, FALSE);
	g_free(msg1);

	buf = g_strdup_printf("%s\001%s", ycht->room, msg2);
	ycht_packet_append(pkt, buf);
	g_free(msg2);
	g_free(buf);

	ycht_packet_send(ycht, pkt);
	ycht_packet_free(pkt);
	return 1;
}

void ycht_chat_leave(YchtConn *ycht, const char *room, gboolean logout)
{
	if (logout)
		ycht_connection_close(ycht);
}

void ycht_chat_send_invite(YchtConn *ycht, const char *room, const char *buddy, const char *msg)
{
}

void ycht_chat_goto_user(YchtConn *ycht, const char *name)
{
}

void ycht_chat_send_keepalive(YchtConn *ycht)
{
	YchtPkt *pkt;

	pkt = ycht_packet_new(YCHT_VERSION, YCHT_SERVICE_PING, 0);
	ycht_packet_send(ycht, pkt);
	ycht_packet_free(pkt);
}
