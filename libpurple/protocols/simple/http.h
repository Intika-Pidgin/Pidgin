/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 *
 */

#ifndef SIMPLE_HTTP_H
#define SIMPLE_HTTP_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * simple_http_digest_calculate_session_key:
 * @username:     The username provided by the user
 * @realm:        The authentication realm provided by the server
 * @password:     The password provided by the user
 * @nonce:        The nonce provided by the server
 * @client_nonce: The nonce provided by the client
 *
 * Calculates a session key for HTTP Digest authentation
 *
 * See RFC 2617 for more information.
 *
 * Returns: The session key, or %NULL if an error occurred.
 */
gchar *simple_http_digest_calculate_session_key(const gchar *username,
                                                const gchar *realm,
                                                const gchar *password,
                                                const gchar *nonce);

/**
 * simple_http_digest_calculate_response:
 * @method:      The HTTP method in use
 * @digest_uri:  The URI from the initial request
 * @nonce:       The nonce provided by the server
 * @nonce_count: The nonce count
 * @session_key: The session key from simple_http_digest_calculate_session_key()
 *
 * Calculate a response for HTTP Digest authentication
 *
 * See RFC 2617 for more information.
 *
 * Returns: The hashed response, or %NULL if an error occurred.
 */
gchar *simple_http_digest_calculate_response(const gchar *method,
                                             const gchar *digest_uri,
                                             const gchar *nonce,
                                             const gchar *nonce_count,
                                             const gchar *session_key);

G_END_DECLS

#endif /* SIMPLE_HTTP_H */
