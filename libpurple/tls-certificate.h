/*
 *
 * purple
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

#ifndef _PURPLE_TLS_CERTIFICATE_H
#define _PURPLE_TLS_CERTIFICATE_H
/**
 * SECTION:tls-certificate
 * @section_id: libpurple-tls-certificate
 * @short_description: TLS certificate trust and storage API
 * @title: TLS Certificate API
 *
 * The TLS Certificate API provides functions for trusting and storing
 * certificates for use with TLS/SSL connections. This allows certificates,
 * which aren't considered valid by the TLS implementation, to be manually
 * trusted by the user, distrusted at a later time, and queried by the UI.
 * It also provides functions to simply wire this system into Gio.
 */

#include <gio/gio.h>

/**
 * purple_tls_certificate_list_ids:
 *
 * Returns a list of the IDs for certificates trusted with
 * purple_tls_certificate_trust() and friends. These IDs can then be passed
 * to purple_certificate_path() or used directly, if desired.
 *
 * Returns: (transfer full) (element-type utf8): #GList of IDs described above
 *          Free with purple_certificate_free_ids()
 */
GList *
purple_tls_certificate_list_ids(void);

/**
 * purple_tls_certificate_free_ids:
 * @ids: (transfer full) (element-type utf8): List of ids retrieved from
 *       purple_certificate_list_ids()
 *
 * Frees the list of IDs returned from purple_certificate_list_ids().
 */
void
purple_tls_certificate_free_ids(GList *ids);

/**
 * purple_tls_certificate_new_from_id:
 * @id: ID of certificate to load
 * @error: A GError location to store the error occurring, or NULL to ignore
 *
 * Loads the certificate referenced by ID into a #GTlsCertificate object.
 *
 * Returns: (transfer full): #GTlsCertificate loaded from ID
 */
GTlsCertificate *
purple_tls_certificate_new_from_id(const gchar *id, GError **error);

/**
 * purple_tls_certificate_trust:
 * @id: ID to associate with the certificate
 * @certificate: Certificate to trust for TLS operations
 * @error: A GError location to store the error occurring, or NULL to ignore
 *
 * Trusts the certificate to be allowed for TLS operations even if
 * it would otherwise fail.
 *
 * Returns: #TRUE on success, #FALSE otherwise
 */
gboolean
purple_tls_certificate_trust(const gchar *id, GTlsCertificate *certificate,
		GError **error);

/**
 * purple_tls_certificate_distrust:
 * @id: ID associated with the certificate to distrust
 * @error: A GError location to store the error occurring, or NULL to ignore
 *
 * Revokes full trust of certificate. The certificate will be accepted
 * in TLS operations only if it passes normal validation.
 *
 * Returns: #TRUE on success, #FALSE otherwise
 */
gboolean
purple_tls_certificate_distrust(const gchar *id, GError **error);


/**
 * purple_tls_certificate_attach_to_tls_connection:
 * @conn: #GTlsConnection to connect to
 *
 * Connects the Purple TLS certificate subsystem to @conn so it will accept
 * certificates trusted by purple_tls_certificate_trust() and friends.
 *
 * Returns: (transfer none) (type GObject.Object): @conn, similar to
 *          g_object_connect()
 */
gpointer
purple_tls_certificate_attach_to_tls_connection(GTlsConnection *conn);

/**
 * purple_tls_certificate_attach_to_socket_client:
 * @client: #GSocketClient to connect to
 *
 * Connects the Purple TLS certificate subsystem to @client so any TLS
 * connections it creates will accept certificates trusted by
 * purple_tls_certificate_trust() and friends.
 *
 * Returns: (transfer none) (type GObject.Object): @client, similar to
 *          g_object_connect()
 */
gpointer
purple_tls_certificate_attach_to_socket_client(GSocketClient *client);

G_END_DECLS

#endif /* _PURPLE_TLS_CERTIFICATE_H */
