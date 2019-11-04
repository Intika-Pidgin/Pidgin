/*
 * nmconn.c
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

#include <glib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "purple-gio.h"

#include "nmconn.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "util.h"

#define NO_ESCAPE(ch) ((ch == 0x20) || (ch >= 0x30 && ch <= 0x39) || \
					(ch >= 0x41 && ch <= 0x5a) || (ch >= 0x61 && ch <= 0x7a))

static char *
url_escape_string(char *src)
{
	guint32 escape = 0;
	char *p;
	char *q;
	char *encoded = NULL;
	int ch;

	static const char hex_table[16] = "0123456789abcdef";

	if (src == NULL) {
		return NULL;
	}

	/* Find number of chars to escape */
	for (p = src; *p != '\0'; p++) {
		ch = (guchar) *p;
		if (!NO_ESCAPE(ch)) {
			escape++;
		}
	}

	encoded = g_malloc((p - src) + (escape * 2) + 1);

	/* Escape the string */
	for (p = src, q = encoded; *p != '\0'; p++) {
		ch = (guchar) * p;
		if (NO_ESCAPE(ch)) {
			if (ch != 0x20) {
				*q = ch;
				q++;
			} else {
				*q = '+';
				q++;
			}
		} else {
			*q = '%';
			q++;

			*q = hex_table[ch >> 4];
			q++;

			*q = hex_table[ch & 15];
			q++;
		}
	}
	*q = '\0';

	return encoded;
}

static char *
encode_method(guint8 method)
{
	char *str;

	switch (method) {
		case NMFIELD_METHOD_EQUAL:
			str = "G";
			break;
		case NMFIELD_METHOD_UPDATE:
			str = "F";
			break;
		case NMFIELD_METHOD_GTE:
			str = "E";
			break;
		case NMFIELD_METHOD_LTE:
			str = "D";
			break;
		case NMFIELD_METHOD_NE:
			str = "C";
			break;
		case NMFIELD_METHOD_EXIST:
			str = "B";
			break;
		case NMFIELD_METHOD_NOTEXIST:
			str = "A";
			break;
		case NMFIELD_METHOD_SEARCH:
			str = "9";
			break;
		case NMFIELD_METHOD_MATCHBEGIN:
			str = "8";
			break;
		case NMFIELD_METHOD_MATCHEND:
			str = "7";
			break;
		case NMFIELD_METHOD_NOT_ARRAY:
			str = "6";
			break;
		case NMFIELD_METHOD_OR_ARRAY:
			str = "5";
			break;
		case NMFIELD_METHOD_AND_ARRAY:
			str = "4";
			break;
		case NMFIELD_METHOD_DELETE_ALL:
			str = "3";
			break;
		case NMFIELD_METHOD_DELETE:
			str = "2";
			break;
		case NMFIELD_METHOD_ADD:
			str = "1";
			break;
		default:					/* NMFIELD_METHOD_VALID */
			str = "0";
			break;
	}

	return str;
}

NMConn *
nm_create_conn(const char *addr, int port)
{
	NMConn *conn = 	g_new0(NMConn, 1);
	conn->addr = g_strdup(addr);
	conn->port = port;
	return conn;
}

void nm_release_conn(NMConn *conn)
{
	g_return_if_fail(conn != NULL);

	g_slist_free_full(conn->requests, (GDestroyNotify)nm_release_request);
	conn->requests = NULL;

	if (conn->input) {
		purple_gio_graceful_close(conn->stream, G_INPUT_STREAM(conn->input),
		                          conn->output);
	}
	g_clear_object(&conn->input);
	g_clear_object(&conn->output);
	g_clear_object(&conn->stream);

	g_clear_pointer(&conn->addr, g_free);
	g_free(conn);
}

NMERR_T
nm_write_fields(NMUser *user, NMField *fields)
{
	NMConn *conn;
	NMERR_T rc = NM_OK;
	NMField *field;
	char *value = NULL;
	char *method = NULL;
	char buffer[4096];
	int ret;
	int bytes_to_send;
	int val = 0;

	g_return_val_if_fail(user != NULL, NMERR_BAD_PARM);
	conn = user->conn;
	g_return_val_if_fail(conn != NULL, NMERR_BAD_PARM);
	g_return_val_if_fail(fields != NULL, NMERR_BAD_PARM);

	/* Format each field as valid "post" data and write it out */
	for (field = fields; (rc == NM_OK) && (field->tag); field++) {

		/* We don't currently handle binary types */
		if (field->method == NMFIELD_METHOD_IGNORE ||
			field->type == NMFIELD_TYPE_BINARY) {
			continue;
		}

		/* Write the field tag */
		bytes_to_send = g_snprintf(buffer, sizeof(buffer), "&tag=%s", field->tag);
		ret = g_output_stream_write(conn->output, buffer, bytes_to_send,
		                            user->cancellable, NULL);
		if (ret < 0) {
			rc = NMERR_TCP_WRITE;
		}

		/* Write the field method */
		if (rc == NM_OK) {
			method = encode_method(field->method);
			bytes_to_send = g_snprintf(buffer, sizeof(buffer), "&cmd=%s", method);
			ret = g_output_stream_write(conn->output, buffer, bytes_to_send,
			                            user->cancellable, NULL);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		}

		/* Write the field value */
		if (rc == NM_OK) {
			switch (field->type) {
				case NMFIELD_TYPE_UTF8:
				case NMFIELD_TYPE_DN:

					value = url_escape_string((char *) field->ptr_value);
					bytes_to_send = g_snprintf(buffer, sizeof(buffer),
											   "&val=%s", value);
					if (bytes_to_send > (int)sizeof(buffer)) {
						ret = g_output_stream_write(conn->output, buffer,
						                            sizeof(buffer),
						                            user->cancellable, NULL);
					} else {
						ret = g_output_stream_write(conn->output, buffer,
						                            bytes_to_send,
						                            user->cancellable, NULL);
					}

					if (ret < 0) {
						rc = NMERR_TCP_WRITE;
					}

					g_free(value);

					break;

				case NMFIELD_TYPE_ARRAY:
				case NMFIELD_TYPE_MV:

					val = nm_count_fields((NMField *) field->ptr_value);
					bytes_to_send = g_snprintf(buffer, sizeof(buffer),
											   "&val=%u", val);
					ret = g_output_stream_write(conn->output, buffer,
					                            bytes_to_send,
					                            user->cancellable, NULL);
					if (ret < 0) {
						rc = NMERR_TCP_WRITE;
					}

					break;

				default:

					bytes_to_send = g_snprintf(buffer, sizeof(buffer),
											   "&val=%u", field->value);
					ret = g_output_stream_write(conn->output, buffer,
					                            bytes_to_send,
					                            user->cancellable, NULL);
					if (ret < 0) {
						rc = NMERR_TCP_WRITE;
					}

					break;
			}
		}

		/* Write the field type */
		if (rc == NM_OK) {
			bytes_to_send = g_snprintf(buffer, sizeof(buffer),
									   "&type=%u", field->type);
			ret = g_output_stream_write(conn->output, buffer, bytes_to_send,
			                            user->cancellable, NULL);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		}

		/* If the field is a sub array then post its fields */
		if (rc == NM_OK && val > 0) {
			if (field->type == NMFIELD_TYPE_ARRAY ||
				field->type == NMFIELD_TYPE_MV) {

				rc = nm_write_fields(user, (NMField *)field->ptr_value);
			}
		}
	}

	return rc;
}

NMERR_T
nm_send_request(NMUser *user, char *cmd, NMField *fields, nm_response_cb cb,
                gpointer data, NMRequest **request)
{
	NMConn *conn;
	NMERR_T rc = NM_OK;
	char buffer[512];
	int bytes_to_send;
	int ret;
	NMField *request_fields = NULL;
	char *str = NULL;

	g_return_val_if_fail(user != NULL, NMERR_BAD_PARM);
	conn = user->conn;
	g_return_val_if_fail(conn != NULL, NMERR_BAD_PARM);
	g_return_val_if_fail(cmd != NULL, NMERR_BAD_PARM);

	/* Write the post */
	bytes_to_send = g_snprintf(buffer, sizeof(buffer),
							   "POST /%s HTTP/1.0\r\n", cmd);
	ret = g_output_stream_write(conn->output, buffer, bytes_to_send,
	                            user->cancellable, NULL);
	if (ret < 0) {
		rc = NMERR_TCP_WRITE;
	}

	/* Write headers */
	if (rc == NM_OK) {
		if (purple_strequal("login", cmd)) {
			bytes_to_send = g_snprintf(buffer, sizeof(buffer),
									   "Host: %s:%d\r\n\r\n", conn->addr, conn->port);
			ret = g_output_stream_write(conn->output, buffer, bytes_to_send,
			                            user->cancellable, NULL);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		} else {
			bytes_to_send = g_snprintf(buffer, sizeof(buffer), "\r\n");
			ret = g_output_stream_write(conn->output, buffer, bytes_to_send,
			                            user->cancellable, NULL);
			if (ret < 0) {
				rc = NMERR_TCP_WRITE;
			}
		}
	}

	/* Add the transaction id to the request fields */
	if (rc == NM_OK) {
		if (fields)
			request_fields = nm_copy_field_array(fields);

		str = g_strdup_printf("%d", ++(conn->trans_id));
		request_fields = nm_field_add_pointer(request_fields, NM_A_SZ_TRANSACTION_ID, 0,
											  NMFIELD_METHOD_VALID, 0,
											  str, NMFIELD_TYPE_UTF8);
	}

	/* Send the request to the server */
	if (rc == NM_OK) {
		rc = nm_write_fields(user, request_fields);
	}

	/* Write the CRLF to terminate the data */
	if (rc == NM_OK) {
		ret = g_output_stream_write(conn->output, "\r\n", strlen("\r\n"),
		                            user->cancellable, NULL);
		if (ret < 0) {
			rc = NMERR_TCP_WRITE;
		}
	}

	/* Create a request struct, add it to our queue, and return it */
	if (rc == NM_OK) {
		NMRequest *new_request =
		        nm_create_request(cmd, conn->trans_id, cb, NULL, data);
		nm_conn_add_request_item(conn, new_request);

		/* Set the out param if it was sent in, otherwise release the request */
		if (request)
			*request = new_request;
		else
			nm_release_request(new_request);
	}

	if (request_fields != NULL)
		nm_free_fields(&request_fields);

	return rc;
}

NMERR_T
nm_read_header(NMUser *user)
{
	NMConn *conn;
	NMERR_T rc = NM_OK;
	gchar *buffer;
	char *ptr = NULL;
	int i;
	char rtn_buf[8];
	int rtn_code = 0;
	GError *error = NULL;

	g_return_val_if_fail(user != NULL, NMERR_BAD_PARM);
	conn = user->conn;
	g_return_val_if_fail(conn != NULL, NMERR_BAD_PARM);

	buffer = g_data_input_stream_read_line(conn->input, NULL, user->cancellable,
	                                       &error);
	if (error == NULL) {
		/* Find the return code */
		ptr = strchr(buffer, ' ');
		if (ptr != NULL) {
			ptr++;

			i = 0;
			while (isdigit(*ptr) && (i < 3)) {
				rtn_buf[i] = *ptr;
				i++;
				ptr++;
			}
			rtn_buf[i] = '\0';

			if (i > 0)
				rtn_code = atoi(rtn_buf);
		}
	}

	/* Finish reading header, in the future we might want to do more processing here */
	/* TODO: handle more general redirects in the future */
	while ((error == NULL) && !purple_strequal(buffer, "\r")) {
		g_free(buffer);
		buffer = g_data_input_stream_read_line(conn->input, NULL,
		                                       user->cancellable, &error);
	}
	g_free(buffer);

	if (error != NULL) {
		if (error->code != G_IO_ERROR_WOULD_BLOCK &&
		    error->code != G_IO_ERROR_CANCELLED) {
			rc = NMERR_TCP_READ;
		}
		g_error_free(error);
	}

	if (rc == NM_OK && rtn_code == 301)
		rc = NMERR_SERVER_REDIRECT;

	return rc;
}

NMERR_T
nm_read_fields(NMUser *user, int count, NMField **fields)
{
	NMConn *conn;
	NMERR_T rc = NM_OK;
	guint8 type;
	guint8 method;
	guint32 val;
	char tag[64];
	NMField *sub_fields = NULL;
	char *str = NULL;
	GError *error = NULL;

	g_return_val_if_fail(user != NULL, NMERR_BAD_PARM);
	conn = user->conn;
	g_return_val_if_fail(conn != NULL, NMERR_BAD_PARM);
	g_return_val_if_fail(fields != NULL, NMERR_BAD_PARM);

	do {
		if (count > 0) {
			count--;
		}

		/* Read the field type, method, and tag */
		type = g_data_input_stream_read_byte(conn->input, user->cancellable,
		                                     &error);
		if (error != NULL || type == 0) {
			break;
		}

		method = g_data_input_stream_read_byte(conn->input, user->cancellable,
		                                       &error);
		if (error != NULL) {
			break;
		}

		val = g_data_input_stream_read_uint32(conn->input, user->cancellable,
		                                      &error);
		if (error != NULL) {
			break;
		}

		if (val > sizeof(tag)) {
			rc = NMERR_PROTOCOL;
			break;
		}

		g_input_stream_read_all(G_INPUT_STREAM(conn->input), tag, val, NULL,
		                        user->cancellable, &error);
		if (error != NULL) {
			break;
		}

		if (type == NMFIELD_TYPE_MV || type == NMFIELD_TYPE_ARRAY) {

			/* Read the subarray (first read the number of items in the array) */
			val = g_data_input_stream_read_uint32(conn->input,
			                                      user->cancellable, &error);
			if (error != NULL) {
				break;
			}

			if (val > 0) {
				rc = nm_read_fields(user, val, &sub_fields);
				if (rc != NM_OK)
					break;
			}

			*fields = nm_field_add_pointer(*fields, tag, 0, method,
									   0, sub_fields, type);

			sub_fields = NULL;

		} else if (type == NMFIELD_TYPE_UTF8 || type == NMFIELD_TYPE_DN) {

			/* Read the string (first read the length) */
			val = g_data_input_stream_read_uint32(conn->input,
			                                      user->cancellable, &error);
			if (error != NULL) {
				break;
			}

			if (val >= NMFIELD_MAX_STR_LENGTH) {
				rc = NMERR_PROTOCOL;
				break;
			}

			if (val > 0) {
				str = g_new0(char, val + 1);

				g_input_stream_read_all(G_INPUT_STREAM(conn->input), str, val,
				                        NULL, user->cancellable, &error);
				if (error != NULL) {
					break;
				}

				*fields = nm_field_add_pointer(*fields, tag, 0, method,
											   0, str, type);
				str = NULL;
			}

		} else {

			/* Read the numerical value */
			val = g_data_input_stream_read_uint32(conn->input,
			                                      user->cancellable, &error);
			if (error != NULL) {
				break;
			}

			*fields = nm_field_add_number(*fields, tag, 0, method,
										  0, val, type);
		}

	} while (count != 0);

	g_free(str);

	if (sub_fields != NULL) {
		nm_free_fields(&sub_fields);
	}

	if (error != NULL) {
		if (error->code != G_IO_ERROR_WOULD_BLOCK && error->code != G_IO_ERROR_CANCELLED) {
			rc = NMERR_TCP_READ;
		}
		g_error_free(error);
	}

	return rc;
}

void
nm_conn_add_request_item(NMConn * conn, NMRequest * request)
{
	if (conn == NULL || request == NULL)
		return;

	nm_request_add_ref(request);
	conn->requests = g_slist_append(conn->requests, request);
}

void
nm_conn_remove_request_item(NMConn * conn, NMRequest * request)
{
	if (conn == NULL || request == NULL)
		return;

	conn->requests = g_slist_remove(conn->requests, request);
	nm_release_request(request);
}

NMRequest *
nm_conn_find_request(NMConn * conn, int trans_id)
{
	NMRequest *req = NULL;
	GSList *itr = NULL;

	if (conn == NULL)
		return NULL;

	itr = conn->requests;
	while (itr) {
		req = (NMRequest *) itr->data;
		if (req != NULL && nm_request_get_trans_id(req) == trans_id) {
			return req;
		}
		itr = g_slist_next(itr);
	}
	return NULL;
}
