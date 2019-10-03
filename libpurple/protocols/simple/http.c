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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 *
 */

#include "internal.h"

#include <glib.h>

/* plugin includes */
#include "http.h"

gchar *
simple_http_digest_calculate_session_key(const gchar *username,
                                         const gchar *realm,
                                         const gchar *password,
                                         const gchar *nonce)
{
	GChecksum *hasher;
	gchar *hash;

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(realm != NULL, NULL);
	g_return_val_if_fail(password != NULL, NULL);
	g_return_val_if_fail(nonce != NULL, NULL);

	hasher = g_checksum_new(G_CHECKSUM_MD5);
	g_return_val_if_fail(hasher != NULL, NULL);

	g_checksum_update(hasher, (guchar *)username, -1);
	g_checksum_update(hasher, (guchar *)":", -1);
	g_checksum_update(hasher, (guchar *)realm, -1);
	g_checksum_update(hasher, (guchar *)":", -1);
	g_checksum_update(hasher, (guchar *)password, -1);

	hash = g_strdup(g_checksum_get_string(hasher));
	g_checksum_free(hasher);

	return hash;
}

gchar *
simple_http_digest_calculate_response(const gchar *method,
                                      const gchar *digest_uri,
                                      const gchar *nonce,
                                      const gchar *nonce_count,
                                      const gchar *session_key)
{
	GChecksum *hash;
	gchar *hash2;

	g_return_val_if_fail(method != NULL, NULL);
	g_return_val_if_fail(digest_uri != NULL, NULL);
	g_return_val_if_fail(nonce != NULL, NULL);
	g_return_val_if_fail(session_key != NULL, NULL);

	hash = g_checksum_new(G_CHECKSUM_MD5);
	g_return_val_if_fail(hash != NULL, NULL);

	g_checksum_update(hash, (guchar *)method, -1);
	g_checksum_update(hash, (guchar *)":", -1);
	g_checksum_update(hash, (guchar *)digest_uri, -1);

	hash2 = g_strdup(g_checksum_get_string(hash));
	g_checksum_reset(hash);

	if (hash2 == NULL) {
		g_checksum_free(hash);
		g_return_val_if_reached(NULL);
	}

	g_checksum_update(hash, (guchar *)session_key, -1);
	g_checksum_update(hash, (guchar *)":", -1);
	g_checksum_update(hash, (guchar *)nonce, -1);
	g_checksum_update(hash, (guchar *)":", -1);

	g_checksum_update(hash, (guchar *)hash2, -1);
	g_free(hash2);

	hash2 = g_strdup(g_checksum_get_string(hash));
	g_checksum_free(hash);

	return hash2;
}
