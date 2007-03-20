/*

  silcpurple_ft.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2004 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcincludes.h"
#include "silcclient.h"
#include "silcpurple.h"

/****************************** File Transfer ********************************/

/* This implements the secure file transfer protocol (SFTP) using the SILC
   SFTP library implementation.  The API we use from the SILC Toolkit is the
   SILC Client file transfer API, as it provides a simple file transfer we
   need in this case.  We could use the SILC SFTP API directly, but it would
   be an overkill since we'd effectively re-implement the file transfer what
   the SILC Client's file transfer API already provides.

   From Purple we do NOT use the FT API to do the transfer as it is very limiting.
   In fact it does not suite to file transfers like SFTP at all.  For example,
   it assumes that read operations are synchronous what they are not in SFTP.
   It also assumes that the file transfer socket is to be handled by the Purple
   eventloop, and this naturally is something we don't want to do in case of
   SILC Toolkit.  The FT API suites well to purely stream based file transfers
   like HTTP GET and similar.

   For this reason, we directly access the Purple GKT FT API and hack the FT
   API to merely provide the user interface experience and all the magic
   is done in the SILC Toolkit.  Ie. we update the statistics information in
   the FT API for user interface, and that's it.  A bit dirty but until the
   FT API gets better this is the way to go.  Good thing that FT API allowed
   us to do this. */

typedef struct {
	SilcPurple sg;
	SilcClientEntry client_entry;
	SilcUInt32 session_id;
	char *hostname;
	SilcUInt16 port;
	PurpleXfer *xfer;

	SilcClientFileName completion;
	void *completion_context;
} *SilcPurpleXfer;

static void
silcpurple_ftp_monitor(SilcClient client,
		     SilcClientConnection conn,
		     SilcClientMonitorStatus status,
		     SilcClientFileError error,
		     SilcUInt64 offset,
		     SilcUInt64 filesize,
		     SilcClientEntry client_entry,
		     SilcUInt32 session_id,
		     const char *filepath,
		     void *context)
{
	SilcPurpleXfer xfer = context;
	PurpleConnection *gc = xfer->sg->gc;
	char tmp[256];

	if (status == SILC_CLIENT_FILE_MONITOR_CLOSED) {
		purple_xfer_unref(xfer->xfer);
		silc_free(xfer);
		return;
	}

	if (status == SILC_CLIENT_FILE_MONITOR_KEY_AGREEMENT)
		return;

	if (status == SILC_CLIENT_FILE_MONITOR_ERROR) {
		if (error == SILC_CLIENT_FILE_NO_SUCH_FILE) {
			g_snprintf(tmp, sizeof(tmp), "No such file %s",
				   filepath ? filepath : "[N/A]");
			purple_notify_error(gc, _("Secure File Transfer"),
					  _("Error during file transfer"), tmp);
		} else if (error == SILC_CLIENT_FILE_PERMISSION_DENIED) {
			purple_notify_error(gc, _("Secure File Transfer"),
					  _("Error during file transfer"),
					  _("Permission denied"));
		} else if (error == SILC_CLIENT_FILE_KEY_AGREEMENT_FAILED) {
			purple_notify_error(gc, _("Secure File Transfer"),
					  _("Error during file transfer"),
					  _("Key agreement failed"));
		} else if (error == SILC_CLIENT_FILE_UNKNOWN_SESSION) {
			purple_notify_error(gc, _("Secure File Transfer"),
					  _("Error during file transfer"),
					  _("File transfer session does not exist"));
		} else {
			purple_notify_error(gc, _("Secure File Transfer"),
					  _("Error during file transfer"), NULL);
		}
		silc_client_file_close(client, conn, session_id);
		purple_xfer_unref(xfer->xfer);
		silc_free(xfer);
		return;
	}

	/* Update file transfer UI */
	if (!offset && filesize)
		purple_xfer_set_size(xfer->xfer, filesize);
	if (offset && filesize) {
		xfer->xfer->bytes_sent = offset;
		xfer->xfer->bytes_remaining = filesize - offset;
	}
	purple_xfer_update_progress(xfer->xfer);

	if (status == SILC_CLIENT_FILE_MONITOR_SEND ||
	    status == SILC_CLIENT_FILE_MONITOR_RECEIVE) {
		if (offset == filesize) {
			/* Download finished */
			purple_xfer_set_completed(xfer->xfer, TRUE);
			silc_client_file_close(client, conn, session_id);
		}
	}
}

static void
silcpurple_ftp_cancel(PurpleXfer *x)
{
	SilcPurpleXfer xfer = x->data;
	xfer->xfer->status = PURPLE_XFER_STATUS_CANCEL_LOCAL;
	purple_xfer_update_progress(xfer->xfer);
	silc_client_file_close(xfer->sg->client, xfer->sg->conn, xfer->session_id);
}

static void
silcpurple_ftp_ask_name_cancel(PurpleXfer *x)
{
	SilcPurpleXfer xfer = x->data;

	/* Cancel the transmission */
	xfer->completion(NULL, xfer->completion_context);
	silc_client_file_close(xfer->sg->client, xfer->sg->conn, xfer->session_id);
}

static void
silcpurple_ftp_ask_name_ok(PurpleXfer *x)
{
	SilcPurpleXfer xfer = x->data;
	const char *name;

	name = purple_xfer_get_local_filename(x);
	g_unlink(name);
	xfer->completion(name, xfer->completion_context);
}

static void
silcpurple_ftp_ask_name(SilcClient client,
		      SilcClientConnection conn,
		      SilcUInt32 session_id,
		      const char *remote_filename,
		      SilcClientFileName completion,
		      void *completion_context,
		      void *context)
{
	SilcPurpleXfer xfer = context;

	xfer->completion = completion;
	xfer->completion_context = completion_context;

	purple_xfer_set_init_fnc(xfer->xfer, silcpurple_ftp_ask_name_ok);
	purple_xfer_set_request_denied_fnc(xfer->xfer, silcpurple_ftp_ask_name_cancel);

	/* Request to save the file */
	purple_xfer_set_filename(xfer->xfer, remote_filename);
	purple_xfer_request(xfer->xfer);
}

static void
silcpurple_ftp_request_result(PurpleXfer *x)
{
	SilcPurpleXfer xfer = x->data;
	SilcClientFileError status;
	PurpleConnection *gc = xfer->sg->gc;

	if (purple_xfer_get_status(x) != PURPLE_XFER_STATUS_ACCEPTED)
		return;

	/* Start the file transfer */
	status = silc_client_file_receive(xfer->sg->client, xfer->sg->conn,
					  silcpurple_ftp_monitor, xfer,
					  NULL, xfer->session_id,
					  silcpurple_ftp_ask_name, xfer);
	switch (status) {
	case SILC_CLIENT_FILE_OK:
		return;
		break;

	case SILC_CLIENT_FILE_UNKNOWN_SESSION:
		purple_notify_error(gc, _("Secure File Transfer"),
				  _("No file transfer session active"), NULL);
		break;

	case SILC_CLIENT_FILE_ALREADY_STARTED:
		purple_notify_error(gc, _("Secure File Transfer"),
				  _("File transfer already started"), NULL);
		break;

	case SILC_CLIENT_FILE_KEY_AGREEMENT_FAILED:
		purple_notify_error(gc, _("Secure File Transfer"),
				  _("Could not perform key agreement for file transfer"),
				  NULL);
		break;

	default:
		purple_notify_error(gc, _("Secure File Transfer"),
				  _("Could not start the file transfer"), NULL);
		break;
	}

	/* Error */
	purple_xfer_unref(xfer->xfer);
	g_free(xfer->hostname);
	silc_free(xfer);
}

static void
silcpurple_ftp_request_denied(PurpleXfer *x)
{

}

void silcpurple_ftp_request(SilcClient client, SilcClientConnection conn,
			  SilcClientEntry client_entry, SilcUInt32 session_id,
			  const char *hostname, SilcUInt16 port)
{
	PurpleConnection *gc = client->application;
	SilcPurple sg = gc->proto_data;
	SilcPurpleXfer xfer;

	xfer = silc_calloc(1, sizeof(*xfer));
	if (!xfer) {
		silc_client_file_close(sg->client, sg->conn, session_id);
		return;
	}

	xfer->sg = sg;
	xfer->client_entry = client_entry;
	xfer->session_id = session_id;
	xfer->hostname = g_strdup(hostname);
	xfer->port = port;
	xfer->xfer = purple_xfer_new(xfer->sg->account, PURPLE_XFER_RECEIVE,
				   xfer->client_entry->nickname);
	if (!xfer->xfer) {
		silc_client_file_close(xfer->sg->client, xfer->sg->conn, xfer->session_id);
		g_free(xfer->hostname);
		silc_free(xfer);
		return;
	}
	purple_xfer_set_init_fnc(xfer->xfer, silcpurple_ftp_request_result);
	purple_xfer_set_request_denied_fnc(xfer->xfer, silcpurple_ftp_request_denied);
	purple_xfer_set_cancel_recv_fnc(xfer->xfer, silcpurple_ftp_cancel);
	xfer->xfer->remote_ip = g_strdup(hostname);
	xfer->xfer->remote_port = port;
	xfer->xfer->data = xfer;

	/* File transfer request */
	purple_xfer_request(xfer->xfer);
}

static void
silcpurple_ftp_send_cancel(PurpleXfer *x)
{
	SilcPurpleXfer xfer = x->data;
	silc_client_file_close(xfer->sg->client, xfer->sg->conn, xfer->session_id);
	purple_xfer_unref(xfer->xfer);
	g_free(xfer->hostname);
	silc_free(xfer);
}

static void
silcpurple_ftp_send(PurpleXfer *x)
{
	SilcPurpleXfer xfer = x->data;
	const char *name;
	char *local_ip = NULL, *remote_ip = NULL;
	gboolean local = TRUE;

	name = purple_xfer_get_local_filename(x);

	/* Do the same magic what we do with key agreement (see silcpurple_buddy.c)
	   to see if we are behind NAT. */
	if (silc_net_check_local_by_sock(xfer->sg->conn->sock->sock,
					 NULL, &local_ip)) {
		/* Check if the IP is private */
		if (silcpurple_ip_is_private(local_ip)) {
			local = FALSE;
			/* Local IP is private, resolve the remote server IP to see whether
			   we are talking to Internet or just on LAN. */
			if (silc_net_check_host_by_sock(xfer->sg->conn->sock->sock, NULL,
							&remote_ip))
				if (silcpurple_ip_is_private(remote_ip))
					/* We assume we are in LAN.  Let's provide the connection point. */
					local = TRUE;
		}
	}

	if (local && !local_ip)
		local_ip = silc_net_localip();

	/* Send the file */
	silc_client_file_send(xfer->sg->client, xfer->sg->conn,
			      silcpurple_ftp_monitor, xfer,
			      local_ip, 0, !local, xfer->client_entry,
			      name, &xfer->session_id);

	silc_free(local_ip);
	silc_free(remote_ip);
}

static void
silcpurple_ftp_send_file_resolved(SilcClient client,
				SilcClientConnection conn,
				SilcClientEntry *clients,
				SilcUInt32 clients_count,
				void *context)
{
	PurpleConnection *gc = client->application;
	char tmp[256];

	if (!clients) {
		g_snprintf(tmp, sizeof(tmp),
			   _("User %s is not present in the network"),
			   (const char *)context);
		purple_notify_error(gc, _("Secure File Transfer"),
				  _("Cannot send file"), tmp);
		silc_free(context);
		return;
	}

	silcpurple_ftp_send_file(client->application, (const char *)context, NULL);
	silc_free(context);
}

PurpleXfer *silcpurple_ftp_new_xfer(PurpleConnection *gc, const char *name)
{
	SilcPurple sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcClientEntry *clients;
	SilcUInt32 clients_count;
	SilcPurpleXfer xfer;
	char *nickname;

	g_return_val_if_fail(name != NULL, NULL);

	if (!silc_parse_userfqdn(name, &nickname, NULL))
		return NULL;

	/* Find client entry */
	clients = silc_client_get_clients_local(client, conn, nickname, name,
											&clients_count);
	if (!clients) {
		silc_client_get_clients(client, conn, nickname, NULL,
								silcpurple_ftp_send_file_resolved,
								strdup(name));
		silc_free(nickname);
		return NULL;
	}

	xfer = silc_calloc(1, sizeof(*xfer));

	g_return_val_if_fail(xfer != NULL, NULL);

	xfer->sg = sg;
	xfer->client_entry = clients[0];
	xfer->xfer = purple_xfer_new(xfer->sg->account, PURPLE_XFER_SEND,
							   xfer->client_entry->nickname);
	if (!xfer->xfer) {
		silc_client_file_close(xfer->sg->client, xfer->sg->conn, xfer->session_id);
		g_free(xfer->hostname);
		silc_free(xfer);
		return NULL;
	}
	purple_xfer_set_init_fnc(xfer->xfer, silcpurple_ftp_send);
	purple_xfer_set_request_denied_fnc(xfer->xfer, silcpurple_ftp_request_denied);
	purple_xfer_set_cancel_send_fnc(xfer->xfer, silcpurple_ftp_send_cancel);
	xfer->xfer->data = xfer;

	silc_free(clients);
	silc_free(nickname);

	return xfer->xfer;
}

void silcpurple_ftp_send_file(PurpleConnection *gc, const char *name, const char *file)
{
	PurpleXfer *xfer = silcpurple_ftp_new_xfer(gc, name);

	g_return_if_fail(xfer != NULL);

	/* Choose file to send */
	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}
