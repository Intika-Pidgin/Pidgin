/**
 * @file dcc_send.c Functions used in sending files with DCC SEND
 *
 * purple
 *
 * Copyright (C) 2004, Timothy T Ringenbach <omarvo@hotmail.com>
 * Copyright (C) 2003, Robbert Haarman <purple@inglorion.net>
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
#include "irc.h"
#include "debug.h"
#include "xfer.h"
#include "notify.h"
#include "network.h"

struct _IrcXfer {
	PurpleXfer parent;

	/* receive properties */
	gchar *ip;
	guint remote_port;

	/* send properties */
	PurpleNetworkListenData *listen_data;
	gint inpa;
	gint fd;
	guchar *rxqueue;
	guint rxlen;
};

G_DEFINE_DYNAMIC_TYPE(IrcXfer, irc_xfer, PURPLE_TYPE_XFER);

/***************************************************************************
 * Functions related to receiving files via DCC SEND
 ***************************************************************************/

/*
 * This function is called whenever data is received.
 * It sends the acknowledgement (in the form of a total byte count as an
 * unsigned 4 byte integer in network byte order)
 */
static void irc_dccsend_recv_ack(PurpleXfer *xfer, const guchar *data, size_t size) {
	guint32 l;
	gssize result;

	if(purple_xfer_get_xfer_type(xfer) != PURPLE_XFER_TYPE_RECEIVE) {
		return;
	}

	l = htonl(purple_xfer_get_bytes_sent(xfer));
	result = purple_xfer_write(xfer, (guchar *)&l, sizeof(l));
	if (result != sizeof(l)) {
		purple_debug_error("irc", "unable to send acknowledgement: %s\n", g_strerror(errno));
		/* TODO: We should probably close the connection here or something. */
	}
}

static void irc_dccsend_recv_init(PurpleXfer *xfer) {
	IrcXfer *xd = IRC_XFER(xfer);

	purple_xfer_start(xfer, -1, xd->ip, xd->remote_port);
}

/* This function makes the necessary arrangements for receiving files */
void irc_dccsend_recv(struct irc_conn *irc, const char *from, const char *msg) {
	IrcXfer *xfer;
	gchar **token;
	struct in_addr addr;
	GString *filename;
	int i = 0;
	guint32 nip;

	token = g_strsplit(msg, " ", 0);
	if (!token[0] || !token[1] || !token[2]) {
		g_strfreev(token);
		return;
	}

	filename = g_string_new("");
	if (token[0][0] == '"') {
		if (!strchr(&(token[0][1]), '"')) {
			g_string_append(filename, &(token[0][1]));
			for (i = 1; token[i]; i++)
				if (!strchr(token[i], '"')) {
					g_string_append_printf(filename, " %s", token[i]);
				} else {
					g_string_append_len(filename, token[i], strlen(token[i]) - 1);
					break;
				}
		} else {
			g_string_append_len(filename, &(token[0][1]), strlen(&(token[0][1])) - 1);
		}
	} else {
		g_string_append(filename, token[0]);
	}

	if (!token[i] || !token[i+1] || !token[i+2]) {
		g_strfreev(token);
		g_string_free(filename, TRUE);
		return;
	}
	i++;

	xfer = g_object_new(
		IRC_TYPE_XFER,
		"account", irc->account,
		"type", PURPLE_XFER_TYPE_RECEIVE,
		"remote-user", from,
		NULL
	);

	purple_xfer_set_filename(PURPLE_XFER(xfer), filename->str);

	xfer->remote_port = atoi(token[i+1]);

	nip = strtoul(token[i], NULL, 10);
	if (nip) {
		addr.s_addr = htonl(nip);
		xfer->ip = g_strdup(inet_ntoa(addr));
	} else {
		xfer->ip = g_strdup(token[i]);
	}

	purple_debug(PURPLE_DEBUG_INFO, "irc", "Receiving file (%s) from %s\n",
		     filename->str, xfer->ip);
	purple_xfer_set_size(PURPLE_XFER(xfer), token[i+2] ? atoi(token[i+2]) : 0);

	purple_xfer_request(PURPLE_XFER(xfer));

	g_strfreev(token);
	g_string_free(filename, TRUE);
}

/*******************************************************************
 * Functions related to sending files via DCC SEND
 *******************************************************************/

/* just in case you were wondering, this is why DCC is crappy */
static void irc_dccsend_send_read(gpointer data, int source, PurpleInputCondition cond)
{
	PurpleXfer *xfer = PURPLE_XFER(data);
	IrcXfer *xd = IRC_XFER(xfer);
	char buffer[64];
	int len;

	len = read(source, buffer, sizeof(buffer));

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0) {
		/* XXX: Shouldn't this be canceling the transfer? */
		purple_input_remove(xd->inpa);
		xd->inpa = 0;
		return;
	}

	xd->rxqueue = g_realloc(xd->rxqueue, len + xd->rxlen);
	memcpy(xd->rxqueue + xd->rxlen, buffer, len);
	xd->rxlen += len;

	while (1) {
		gint32 val;
		size_t acked;

		if (xd->rxlen < 4)
			break;

		memcpy(&val, xd->rxqueue, sizeof(val));
		acked = ntohl(val);

		xd->rxlen -= 4;
		if (xd->rxlen) {
			unsigned char *tmp = g_memdup(xd->rxqueue + 4, xd->rxlen);
			g_free(xd->rxqueue);
			xd->rxqueue = tmp;
		} else {
			g_free(xd->rxqueue);
			xd->rxqueue = NULL;
		}

		if ((goffset)acked >= purple_xfer_get_size(xfer)) {
			purple_input_remove(xd->inpa);
			xd->inpa = 0;
			purple_xfer_set_completed(xfer, TRUE);
			purple_xfer_end(xfer);
			return;
		}
	}
}

static gssize irc_dccsend_send_write(PurpleXfer *xfer, const guchar *buffer, size_t size)
{
	gssize s;
	gssize ret;

	s = MIN((gssize)purple_xfer_get_bytes_remaining(xfer), (gssize)size);
	if (!s) {
		return 0;
	}

	ret = PURPLE_XFER_CLASS(irc_xfer_parent_class)->write(xfer, buffer, s);

	if (ret < 0 && errno == EAGAIN) {
		ret = 0;
	}

	return ret;
}

static void irc_dccsend_send_connected(gpointer data, int source, PurpleInputCondition cond) {
	PurpleXfer *xfer = PURPLE_XFER(data);
	IrcXfer *xd = IRC_XFER(xfer);
	int conn;

	conn = accept(xd->fd, NULL, 0);
	if (conn == -1) {
		/* Accepting the connection failed. This could just be related
		 * to the nonblocking nature of the listening socket, so we'll
		 * just try again next time */
		/* Let's print an error message anyway */
		purple_debug_warning("irc", "accept: %s\n", g_strerror(errno));
		return;
	}

	purple_input_remove(purple_xfer_get_watcher(xfer));
	purple_xfer_set_watcher(xfer, 0);
	close(xd->fd);
	xd->fd = -1;

	_purple_network_set_common_socket_flags(conn);

	xd->inpa = purple_input_add(conn, PURPLE_INPUT_READ, irc_dccsend_send_read, xfer);
	/* Start the transfer */
	purple_xfer_start(xfer, conn, NULL, 0);
}

static void
irc_dccsend_network_listen_cb(int sock, gpointer data)
{
	PurpleXfer *xfer = PURPLE_XFER(data);
	IrcXfer *xd = IRC_XFER(xfer);
	PurpleConnection *gc;
	struct irc_conn *irc;
	GSocket *gsock;
	int fd = -1;
	const char *arg[2];
	char *tmp;
	struct in_addr addr;
	unsigned short int port;

	/* not sure what the deal is here, but it needs to be here.. gk 20190626 */
	xd->listen_data = NULL;

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL
			|| purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_REMOTE) {
		g_object_unref(xfer);
		return;
	}

	gc = purple_account_get_connection(purple_xfer_get_account(xfer));
	irc = purple_connection_get_protocol_data(gc);

	if (sock < 0) {
		purple_notify_error(gc, NULL, _("File Transfer Failed"),
			_("Unable to open a listening port."),
			purple_request_cpar_from_connection(gc));
		purple_xfer_cancel_local(xfer);
		return;
	}

	xd->fd = sock;

	port = purple_network_get_port_from_fd(sock);
	purple_debug_misc("irc", "port is %hu\n", port);
	/* Monitor the listening socket */
	purple_xfer_set_watcher(
		xfer,
		purple_input_add(sock, PURPLE_INPUT_READ, irc_dccsend_send_connected, xfer)
	);

	/* Send the intended recipient the DCC request */
	arg[0] = purple_xfer_get_remote_user(xfer);

	/* Fetching this fd here assumes it won't be modified */
	gsock = g_socket_connection_get_socket(irc->conn);
	if (gsock != NULL) {
		fd = g_socket_get_fd(gsock);
	}

	inet_aton(purple_network_get_my_ip(fd), &addr);
	arg[1] = tmp = g_strdup_printf(
		"\001DCC SEND \"%s\" %u %hu %" G_GOFFSET_FORMAT "\001",
    	purple_xfer_get_filename(xfer),
    	ntohl(addr.s_addr),
    	port,
    	purple_xfer_get_size(xfer)
    );

	irc_cmd_privmsg(purple_connection_get_protocol_data(gc), "msg", NULL, arg);
	g_free(tmp);
}

/*
 * This function is called after the user has selected a file to send.
 */
static void irc_dccsend_send_init(PurpleXfer *xfer) {
	IrcXfer *xd = IRC_XFER(xfer);
	PurpleConnection *gc = purple_account_get_connection(purple_xfer_get_account(xfer));

	purple_xfer_set_filename(xfer, g_path_get_basename(purple_xfer_get_local_filename(xfer)));

	/* Create a listening socket */
	xd->listen_data = purple_network_listen_range(
		0,
		0,
		AF_UNSPEC,
		SOCK_STREAM,
		TRUE,
		irc_dccsend_network_listen_cb,
		xfer
	);
	if (xd->listen_data == NULL) {
		purple_notify_error(gc, NULL, _("File Transfer Failed"),
			_("Unable to open a listening port."),
			purple_request_cpar_from_connection(gc));
		purple_xfer_cancel_local(xfer);
	}
}

PurpleXfer *irc_dccsend_new_xfer(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *who) {
	return g_object_new(
		IRC_TYPE_XFER,
		"account", purple_connection_get_account(gc),
		"type", PURPLE_XFER_TYPE_SEND,
		"remote-user", who,
		NULL
	);
}

/**
 * Purple calls this function when the user selects Send File from the
 * buddy menu
 * It sets up the PurpleXfer struct and tells Purple to go ahead
 */
void irc_dccsend_send_file(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *who, const char *file) {
	PurpleXfer *xfer = irc_dccsend_new_xfer(prplxfer, gc, who);

	/* Perform the request */
	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

/******************************************************************************
 * PurpleXfer Implementation
 *****************************************************************************/
static void
irc_dccsend_init(PurpleXfer *xfer) {
	PurpleXferType type = purple_xfer_get_xfer_type(xfer);

	if(type == PURPLE_XFER_TYPE_SEND) {
		irc_dccsend_send_init(xfer);
	} else if(type == PURPLE_XFER_TYPE_RECEIVE) {
		irc_dccsend_recv_init(xfer);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
irc_xfer_init(IrcXfer *xfer) {
	xfer->fd = -1;
}

static void
irc_xfer_finalize(GObject *obj) {
	IrcXfer *xfer = IRC_XFER(obj);

	/* clean up the receiving proprties */
	g_free(xfer->ip);
	g_free(xfer->rxqueue);

	/* clean up the sending properties */
	g_clear_pointer(&xfer->listen_data, purple_network_listen_cancel);
	if(xfer->inpa > 0) {
		purple_input_remove(xfer->inpa);
	}
	if(xfer->fd != -1) {
		close(xfer->fd);
	}

	G_OBJECT_CLASS(irc_xfer_parent_class)->finalize(obj);
}

static void
irc_xfer_class_finalize(IrcXferClass *klass) {

}

static void
irc_xfer_class_init(IrcXferClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleXferClass *xfer_class = PURPLE_XFER_CLASS(klass);

	obj_class->finalize = irc_xfer_finalize;

	xfer_class->init = irc_dccsend_init;
	xfer_class->ack = irc_dccsend_recv_ack;
	xfer_class->write = irc_dccsend_send_write;
}

void
irc_xfer_register(GTypeModule *module) {
	irc_xfer_register_type(module);
}
