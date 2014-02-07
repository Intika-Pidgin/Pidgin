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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_SSLCONN_H_
#define _PURPLE_SSLCONN_H_
/**
 * SECTION:sslconn
 * @section_id: libpurple-sslconn
 * @short_description: <filename>sslconn.h</filename>
 * @title: SSL API
 */

/**
 * PurpleSslErrorType:
 *
 * Possible SSL errors.
 */
typedef enum
{
	PURPLE_SSL_HANDSHAKE_FAILED = 1,
	PURPLE_SSL_CONNECT_FAILED = 2,
	PURPLE_SSL_CERTIFICATE_INVALID = 3
} PurpleSslErrorType;

#include "certificate.h"
#include "proxy.h"

#define PURPLE_SSL_DEFAULT_PORT 443

typedef struct _PurpleSslConnection PurpleSslConnection;
typedef struct _PurpleSslOps PurpleSslOps;

typedef void (*PurpleSslInputFunction)(gpointer, PurpleSslConnection *,
									 PurpleInputCondition);
typedef void (*PurpleSslErrorFunction)(PurpleSslConnection *, PurpleSslErrorType,
									 gpointer);

/**
 * PurpleSslConnection:
 * @host:            Hostname to which the SSL connection will be made
 * @port:            Port to connect to
 * @connect_cb_data: Data to pass to @connect_cb
 * @connect_cb:      Callback triggered once the SSL handshake is complete
 * @error_cb:        Callback triggered if there is an error during connection
 * @recv_cb_data:    Data passed to @recv_cb
 * @recv_cb:         User-defined callback executed when the SSL connection
 *                   receives data
 * @fd:              File descriptor used to refer to the socket
 * @inpa:            Glib event source ID; used to refer to the received data
 *                   callback in the glib eventloop
 * @connect_data:    Data related to the underlying TCP connection
 * @private_data:    Internal connection data managed by the SSL backend
 *                   (GnuTLS/LibNSS/whatever)
 * @verifier:        Verifier to use in authenticating the peer
 */
struct _PurpleSslConnection
{
	char *host;
	int port;
	void *connect_cb_data;
	PurpleSslInputFunction connect_cb;
	PurpleSslErrorFunction error_cb;
	void *recv_cb_data;
	PurpleSslInputFunction recv_cb;

	int fd;
	guint inpa;
	PurpleProxyConnectData *connect_data;

	void *private_data;
	PurpleCertificateVerifier *verifier;
};

/**
 * PurpleSslOps:
 * @init: Initializes the SSL system provided. See purple_ssl_init().
 *        <sbr/>Returns: %TRUE if initialization succeeded
 * @uninit: Unloads the SSL system. Inverse of PurpleSslOps::init.
 *          See purple_ssl_uninit().
 * @connectfunc: Sets up the SSL connection for a #PurpleSslConnection once the
 *               TCP connection has been established. See purple_ssl_connect().
 * @close: Destroys the internal data of the SSL connection provided. Freeing
 *         @gsc itself is left to purple_ssl_close().
 * @read: Reads data from a connection (like POSIX read()).
 *        See purple_ssl_read().
 *        <sbr/>@gsc:    Connection context
 *        <sbr/>@data:   Pointer to buffer to drop data into
 *        <sbr/>@len:    Maximum number of bytes to read
 *        <sbr/>Returns: Number of bytes actually written into @data
 *                       (which may be less than @len), or <0 on error
 * @write: Writes data to a connection (like POSIX send()).
 *         See purple_ssl_write().
 *         <sbr/>@gsc:    Connection context
 *         <sbr/>@data:   Data buffer to send data from
 *         <sbr/>@len:    Number of bytes to send from buffer
 *         <sbr/>Returns: The number of bytes written to @data (may be less than
 *                        @len) or <0 on error
 * @get_peer_certificates: Obtains the certificate chain provided by the peer.
 *                         See #PurpleCertificate.
 *                         <sbr/>@gsc:    Connection context
 *                         <sbr/>Returns: A newly allocated list containing the
 *                                        certificates the peer provided.
 *
 * SSL implementation operations structure.
 *
 * Every SSL implementation must provide all of these and register it via
 * purple_ssl_set_ops().
 * These should not be called directly! Instead, use the purple_ssl_* functions.
 */
struct _PurpleSslOps
{
	gboolean (*init)(void);
	void (*uninit)(void);
	void (*connectfunc)(PurpleSslConnection *gsc);
	void (*close)(PurpleSslConnection *gsc);
	size_t (*read)(PurpleSslConnection *gsc, void *data, size_t len);
	size_t (*write)(PurpleSslConnection *gsc, const void *data, size_t len);

	/* TODO Decide whether the ordering of certificates in this list can be
	        guaranteed. */
	GList * (* get_peer_certificates)(PurpleSslConnection * gsc);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* SSL API                                                                */
/**************************************************************************/

/**
 * purple_ssl_is_supported:
 *
 * Returns whether or not SSL is currently supported.
 *
 * Returns: %TRUE if SSL is supported, or %FALSE otherwise.
 */
gboolean purple_ssl_is_supported(void);

/**
 * purple_ssl_strerror:
 * @error:      Error code
 *
 * Returns a human-readable string for an SSL error.
 *
 * Returns: Human-readable error explanation
 */
const gchar * purple_ssl_strerror(PurpleSslErrorType error);

/**
 * purple_ssl_connect:
 * @account:    The account making the connection.
 * @host:       The destination host.
 * @port:       The destination port.
 * @func:       The SSL input handler function.
 * @error_func: The SSL error handler function.  This function
 *              should <emphasis>NOT</emphasis> call purple_ssl_close(). In
 *              the event of an error the #PurpleSslConnection will be
 *              destroyed for you.
 * @data:       User-defined data.
 *
 * Makes a SSL connection to the specified host and port.  The caller
 * should keep track of the returned value and use it to cancel the
 * connection, if needed.
 *
 * Returns: The SSL connection handle.
 */
PurpleSslConnection *purple_ssl_connect(PurpleAccount *account, const char *host,
									int port, PurpleSslInputFunction func,
									PurpleSslErrorFunction error_func,
									void *data);

/**
 * purple_ssl_connect_with_ssl_cn:
 * @account:    The account making the connection.
 * @host:       The destination host.
 * @port:       The destination port.
 * @func:       The SSL input handler function.
 * @error_func: The SSL error handler function.  This function
 *              should <emphasis>NOT</emphasis> call purple_ssl_close(). In
 *              the event of an error the #PurpleSslConnection will be
 *              destroyed for you.
 * @ssl_host:   The hostname of the other peer (to verify the CN)
 * @data:       User-defined data.
 *
 * Makes a SSL connection to the specified host and port, using the separate
 * name to verify with the certificate.  The caller should keep track of the
 * returned value and use it to cancel the connection, if needed.
 *
 * Returns: The SSL connection handle.
 */
PurpleSslConnection *purple_ssl_connect_with_ssl_cn(PurpleAccount *account, const char *host,
									int port, PurpleSslInputFunction func,
									PurpleSslErrorFunction error_func,
									const char *ssl_host,
									void *data);

/**
 * purple_ssl_connect_with_host_fd:
 * @account:    The account making the connection.
 * @fd:         The file descriptor.
 * @func:       The SSL input handler function.
 * @error_func: The SSL error handler function.
 * @host:       The hostname of the other peer (to verify the CN)
 * @data:       User-defined data.
 *
 * Makes a SSL connection using an already open file descriptor.
 *
 * Returns: The SSL connection handle.
 */
PurpleSslConnection *purple_ssl_connect_with_host_fd(PurpleAccount *account, int fd,
                                           PurpleSslInputFunction func,
                                           PurpleSslErrorFunction error_func,
                                           const char *host,
                                           void *data);

/**
 * purple_ssl_input_add:
 * @gsc:   The SSL connection handle.
 * @func:  The callback function.
 * @data:  User-defined data.
 *
 * Adds an input watcher for the specified SSL connection.
 * Once the SSL handshake is complete, use this to watch for actual data across it.
 */
void purple_ssl_input_add(PurpleSslConnection *gsc, PurpleSslInputFunction func,
						void *data);

/**
 * purple_ssl_input_remove:
 * @gsc: The SSL connection handle.
 *
 * Removes an input watcher, added with purple_ssl_input_add().
 *
 * If there is no input watcher set, does nothing.
 */
void
purple_ssl_input_remove(PurpleSslConnection *gsc);

/**
 * purple_ssl_close:
 * @gsc: The SSL connection to close.
 *
 * Closes a SSL connection.
 */
void purple_ssl_close(PurpleSslConnection *gsc);

/**
 * purple_ssl_read:
 * @gsc:    The SSL connection handle.
 * @buffer: The destination buffer.
 * @len:    The maximum number of bytes to read.
 *
 * Reads data from an SSL connection.
 *
 * Returns: The number of bytes read.
 */
size_t purple_ssl_read(PurpleSslConnection *gsc, void *buffer, size_t len);

/**
 * purple_ssl_write:
 * @gsc:    The SSL connection handle.
 * @buffer: The buffer to write.
 * @len:    The length of the data to write.
 *
 * Writes data to an SSL connection.
 *
 * Returns: The number of bytes written.
 */
size_t purple_ssl_write(PurpleSslConnection *gsc, const void *buffer, size_t len);

/**
 * purple_ssl_get_peer_certificates:
 * @gsc:    The SSL connection handle
 *
 * Obtains the peer's presented certificates
 *
 * Returns: The peer certificate chain, in the order of certificate, issuer,
 *         issuer's issuer, etc. %NULL if no certificates have been provided,
 */
GList * purple_ssl_get_peer_certificates(PurpleSslConnection *gsc);

/**************************************************************************/
/* Subsystem API                                                          */
/**************************************************************************/

/**
 * purple_ssl_set_ops:
 * @ops: The SSL operations structure to assign.
 *
 * Sets the current SSL operations structure.
 */
void purple_ssl_set_ops(PurpleSslOps *ops);

/**
 * purple_ssl_get_ops:
 *
 * Returns the current SSL operations structure.
 *
 * Returns: The SSL operations structure.
 */
PurpleSslOps *purple_ssl_get_ops(void);

/**
 * purple_ssl_init:
 *
 * Initializes the SSL subsystem.
 */
void purple_ssl_init(void);

/**
 * purple_ssl_uninit:
 *
 * Uninitializes the SSL subsystem.
 */
void purple_ssl_uninit(void);

G_END_DECLS

#endif /* _PURPLE_SSLCONN_H_ */
