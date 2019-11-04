/*
 * nmconn.h
 *
 * Copyright (c) 2004 Novell, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA	02111-1301	USA
 *
 */

#ifndef PURPLE_NOVELL_NMCONN_H
#define PURPLE_NOVELL_NMCONN_H

#include <gio/gio.h>

typedef struct _NMConn NMConn;

#include "nmfield.h"
#include "nmuser.h"

typedef int (*nm_ssl_read_cb) (gpointer ssl_data, void *buff, int len);
typedef int (*nm_ssl_write_cb) (gpointer ssl_data, const void *buff, int len);

struct _NMConn
{

	/* The address of the server that we are connecting to. */
	char *addr;

	/* The port that we are connecting to. */
	int port;

	/* The transaction counter. */
	int trans_id;

	/* A list of requests currently awaiting a response. */
	GSList *requests;

	/* Connections to server. */
	GSocketClient *client;
	GIOStream *stream;
	GDataInputStream *input;
	GOutputStream *output;
};

/**
 * Allocate a new NMConn struct
 *
 * @param 	The address of the server that we are connecting to.
 * @param 	The port that we are connecting to.
 *
 * @return		A pointer to a newly allocated NMConn struct, should
 *				be freed by calling nm_release_conn()
 */
NMConn *nm_create_conn(const char *addr, int port);

/**
 * Release an NMConn
 *
 * @param 	Pointer to the NMConn to release.
 *
 */
void nm_release_conn(NMConn *conn);

/**
 * Dispatch a request to the server.
 *
 * @param user		The logged-in user.
 * @param cmd		The request to dispatch.
 * @param fields	The field list for the request.
 * @param cb		The response callback for the new request object.
 * @param data		The user defined data for the request (to be passed to the resp cb).
 * @param req		The request. Should be freed with nm_release_request.
 *
 * @return			NM_OK on success.
 */
NMERR_T
nm_send_request(NMUser *user, char *cmd, NMField *fields, nm_response_cb cb,
                gpointer data, NMRequest **request);

/**
 * Write out the given field list.
 *
 * @param user		The logged-in user.
 * @param fields	The field list to write.
 *
 * @return			NM_OK on success.
 */
NMERR_T nm_write_fields(NMUser *user, NMField *fields);

/**
 * Read the headers for a response.
 *
 * @param user		The logged-in user.
 *
 * @return			NM_OK on success.
 */
NMERR_T nm_read_header(NMUser *user);

/**
 * Read a field list from the connection.
 *
 * @param user		The logged-in user.
 * @param count		The maximum number of fields to read (or -1 for no max).
 * @param fields	The field list. This is an out param. It
 *					should be freed by calling nm_free_fields
 *					when finished.
 *
 * @return			NM_OK on success.
 */
NMERR_T nm_read_fields(NMUser *user, int count, NMField **fields);

/**
 * Add a request to the connections request list.
 *
 * @param conn		The connection.
 * @param request	The request to add to the list.
 */
void nm_conn_add_request_item(NMConn * conn, NMRequest * request);

/**
 * Remove a request from the connections list.
 *
 * @param conn		The connection.
 * @param request	The request to remove from the list.
 */
void nm_conn_remove_request_item(NMConn * conn, NMRequest * request);

/**
 * Find the request with the given transaction id in the connections
 * request list.
 *
 * @param conn		The connection.
 * @param trans_id	The transaction id of the request to return.
 *
 * @return 			The request, or NULL if a matching request is not
 *					found.
 */
NMRequest *nm_conn_find_request(NMConn * conn, int trans_id);

#endif /* PURPLE_NOVELL_NMCONN_H */
