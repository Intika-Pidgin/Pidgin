/**
 * @file xfer.h File Transfer API
 * @ingroup core
 * @see @ref xfer-signals
 */

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
#ifndef _PURPLE_XFER_H_
#define _PURPLE_XFER_H_

#define PURPLE_TYPE_XFER             (purple_xfer_get_type())
#define PURPLE_XFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_XFER, PurpleXfer))
#define PURPLE_XFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_XFER, PurpleXferClass))
#define PURPLE_IS_XFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_XFER))
#define PURPLE_IS_XFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_XFER))
#define PURPLE_XFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_XFER, PurpleXferClass))

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/
/** @copydoc _PurpleXfer */
typedef struct _PurpleXfer PurpleXfer;
/** @copydoc _PurpleXferClass */
typedef struct _PurpleXferClass PurpleXferClass;

#include <glib.h>
#include <stdio.h>

#include "account.h"

/**
 * Types of file transfers.
 */
typedef enum
{
	PURPLE_XFER_TYPE_UNKNOWN = 0,  /**< Unknown file transfer type. */
	PURPLE_XFER_TYPE_SEND,         /**< File sending.               */
	PURPLE_XFER_TYPE_RECEIVE       /**< File receiving.             */

} PurpleXferType;

/**
 * The different states of the xfer.
 */
typedef enum
{
	PURPLE_XFER_STATUS_UNKNOWN = 0,   /**< Unknown, the xfer may be null. */
	PURPLE_XFER_STATUS_NOT_STARTED,   /**< It hasn't started yet. */
	PURPLE_XFER_STATUS_ACCEPTED,      /**< Receive accepted, but destination file not selected yet */
	PURPLE_XFER_STATUS_STARTED,       /**< purple_xfer_start has been called. */
	PURPLE_XFER_STATUS_DONE,          /**< The xfer completed successfully. */
	PURPLE_XFER_STATUS_CANCEL_LOCAL,  /**< The xfer was cancelled by us. */
	PURPLE_XFER_STATUS_CANCEL_REMOTE  /**< The xfer was cancelled by the other end, or we couldn't connect. */
} PurpleXferStatus;

/**
 * File transfer UI operations.
 *
 * Any UI representing a file transfer must assign a filled-out
 * PurpleXferUiOps structure to the purple_xfer.
 */
typedef struct
{
	void (*new_xfer)(PurpleXfer *xfer);
	void (*destroy)(PurpleXfer *xfer);
	void (*add_xfer)(PurpleXfer *xfer);
	void (*update_progress)(PurpleXfer *xfer, double percent);
	void (*cancel_local)(PurpleXfer *xfer);
	void (*cancel_remote)(PurpleXfer *xfer);

	/**
	 * UI op to write data received from the prpl. The UI must deal with the
	 * entire buffer and return size, or it is treated as an error.
	 *
	 * @param xfer    The file transfer structure
	 * @param buffer  The buffer to write
	 * @param size    The size of the buffer
	 *
	 * @return size if the write was successful, or a value between 0 and
	 *         size on error.
	 */
	gssize (*ui_write)(PurpleXfer *xfer, const guchar *buffer, gssize size);

	/**
	 * UI op to read data to send to the prpl for a file transfer.
	 *
	 * @param xfer    The file transfer structure
	 * @param buffer  A pointer to a buffer. The UI must allocate this buffer.
	 *                libpurple will free the data.
	 * @param size    The maximum amount of data to put in the buffer.
	 *
	 * @returns The amount of data in the buffer, 0 if nothing is available,
	 *          and a negative value if an error occurred and the transfer
	 *          should be cancelled (libpurple will cancel).
	 */
	gssize (*ui_read)(PurpleXfer *xfer, guchar **buffer, gssize size);

	/**
	 * Op to notify the UI that not all the data read in was written. The UI
	 * should re-enqueue this data and return it the next time read is called.
	 *
	 * This MUST be implemented if read and write are implemented.
	 *
	 * @param xfer    The file transfer structure
	 * @param buffer  A pointer to the beginning of the unwritten data.
	 * @param size    The amount of unwritten data.
	 */
	void (*data_not_sent)(PurpleXfer *xfer, const guchar *buffer, gsize size);

	/**
	 * Op to create a thumbnail image for a file transfer
	 *
	 * @param xfer   The file transfer structure
	 */
	void (*add_thumbnail)(PurpleXfer *xfer, const gchar *formats);
} PurpleXferUiOps;

/**
 * A core representation of a file transfer.
 */
struct _PurpleXfer
{
	GObject gparent;

	/** The UI data associated with this file transfer. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/** Base class for all #PurpleXfer's */
struct _PurpleXferClass
{
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name File Transfer API                                               */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleXfer object.
 */
GType purple_xfer_get_type(void);

/**
 * Creates a new file transfer handle.
 * This is called by prpls.
 *
 * @param account The account sending or receiving the file.
 * @param type    The type of file transfer.
 * @param who     The name of the remote user.
 *
 * @return A file transfer handle.
 */
PurpleXfer *purple_xfer_new(PurpleAccount *account,
								PurpleXferType type, const char *who);

/**
 * Requests confirmation for a file transfer from the user. If receiving
 * a file which is known at this point, this requests user to accept and
 * save the file. If the filename is unknown (not set) this only requests user
 * to accept the file transfer. In this case protocol must call this function
 * again once the filename is available.
 *
 * @param xfer The file transfer to request confirmation on.
 */
void purple_xfer_request(PurpleXfer *xfer);

/**
 * Called if the user accepts the file transfer request.
 *
 * @param xfer     The file transfer.
 * @param filename The filename.
 */
void purple_xfer_request_accepted(PurpleXfer *xfer, const char *filename);

/**
 * Called if the user rejects the file transfer request.
 *
 * @param xfer The file transfer.
 */
void purple_xfer_request_denied(PurpleXfer *xfer);

/**
 * Returns the socket file descriptor.
 *
 * @param xfer The file transfer.
 *
 * @return The socket file descriptor.
 */
int purple_xfer_get_fd(PurpleXfer *xfer);

/**
 * Returns the Watcher for the transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The watcher.
 */
int purple_xfer_get_watcher(PurpleXfer *xfer);

/**
 * Returns the type of file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The type of the file transfer.
 */
PurpleXferType purple_xfer_get_xfer_type(const PurpleXfer *xfer);

/**
 * Returns the account the file transfer is using.
 *
 * @param xfer The file transfer.
 *
 * @return The account.
 */
PurpleAccount *purple_xfer_get_account(const PurpleXfer *xfer);

/**
 * Sets the name of the remote user.
 *
 * @param xfer The file transfer.
 * @param who  The name of the remote user.
 */
void purple_xfer_set_remote_user(PurpleXfer *xfer, const char *who);

/**
 * Returns the name of the remote user.
 *
 * @param xfer The file transfer.
 *
 * @return The name of the remote user.
 */
const char *purple_xfer_get_remote_user(const PurpleXfer *xfer);

/**
 * Returns the status of the xfer.
 *
 * @param xfer The file transfer.
 *
 * @return The status.
 */
PurpleXferStatus purple_xfer_get_status(const PurpleXfer *xfer);

/**
 * Returns true if the file transfer was cancelled.
 *
 * @param xfer The file transfer.
 *
 * @return Whether or not the transfer was cancelled.
 */
gboolean purple_xfer_is_cancelled(const PurpleXfer *xfer);

/**
 * Returns the completed state for a file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The completed state.
 */
gboolean purple_xfer_is_completed(const PurpleXfer *xfer);

/**
 * Returns the name of the file being sent or received.
 *
 * @param xfer The file transfer.
 *
 * @return The filename.
 */
const char *purple_xfer_get_filename(const PurpleXfer *xfer);

/**
 * Returns the file's destination filename,
 *
 * @param xfer The file transfer.
 *
 * @return The destination filename.
 */
const char *purple_xfer_get_local_filename(const PurpleXfer *xfer);

/**
 * Returns the number of bytes sent (or received) so far.
 *
 * @param xfer The file transfer.
 *
 * @return The number of bytes sent.
 */
goffset purple_xfer_get_bytes_sent(const PurpleXfer *xfer);

/**
 * Returns the number of bytes remaining to send or receive.
 *
 * @param xfer The file transfer.
 *
 * @return The number of bytes remaining.
 */
goffset purple_xfer_get_bytes_remaining(const PurpleXfer *xfer);

/**
 * Returns the size of the file being sent or received.
 *
 * @param xfer The file transfer.
 *
 * @return The total size of the file.
 */
goffset purple_xfer_get_size(const PurpleXfer *xfer);

/**
 * Returns the current percentage of progress of the transfer.
 *
 * This is a number between 0 (0%) and 1 (100%).
 *
 * @param xfer The file transfer.
 *
 * @return The percentage complete.
 */
double purple_xfer_get_progress(const PurpleXfer *xfer);

/**
 * Returns the local port number in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The port number on this end.
 */
guint16 purple_xfer_get_local_port(const PurpleXfer *xfer);

/**
 * Returns the remote IP address in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The IP address on the other end.
 */
const char *purple_xfer_get_remote_ip(const PurpleXfer *xfer);

/**
 * Returns the remote port number in the file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The port number on the other end.
 */
guint16 purple_xfer_get_remote_port(const PurpleXfer *xfer);

/**
 * Returns the time the transfer of a file started.
 *
 * @param xfer  The file transfer.
 *
 * @return The time when the transfer started.
 */
time_t purple_xfer_get_start_time(const PurpleXfer *xfer);

/**
 * Returns the time the transfer of a file ended.
 *
 * @param xfer  The file transfer.
 *
 * @return The time when the transfer ended.
 */
time_t purple_xfer_get_end_time(const PurpleXfer *xfer);

/**
 * Sets the socket file descriptor.
 *
 * @param xfer      The file transfer.
 * @param fd        The file descriptor.
 */
void purple_xfer_set_fd(PurpleXfer *xfer, int fd);

/**
 * Sets the watcher for the file transfer.
 *
 * @param xfer      The file transfer.
 * @param watcher   The watcher.
 */
void purple_xfer_set_watcher(PurpleXfer *xfer, int watcher);

/**
 * Sets the completed state for the file transfer.
 *
 * @param xfer      The file transfer.
 * @param completed The completed state.
 */
void purple_xfer_set_completed(PurpleXfer *xfer, gboolean completed);

/**
 * Sets the current status for the file transfer.
 *
 * @param xfer      The file transfer.
 * @param status    The current status.
 */
void purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatus status);

/**
 * Sets the message for the file transfer.
 *
 * @param xfer     The file transfer.
 * @param message The message.
 */
void purple_xfer_set_message(PurpleXfer *xfer, const char *message);

/**
 * Returns the message for the file transfer.
 *
 * @param xfer     The file transfer.
 *
 * @return The message.
 */
const char *purple_xfer_get_message(const PurpleXfer *xfer);

/**
 * Sets the filename for the file transfer.
 *
 * @param xfer     The file transfer.
 * @param filename The filename.
 */
void purple_xfer_set_filename(PurpleXfer *xfer, const char *filename);

/**
 * Sets the local filename for the file transfer.
 *
 * @param xfer     The file transfer.
 * @param filename The filename
 */
void purple_xfer_set_local_filename(PurpleXfer *xfer, const char *filename);

/**
 * Sets the size of the file in a file transfer.
 *
 * @param xfer The file transfer.
 * @param size The size of the file.
 */
void purple_xfer_set_size(PurpleXfer *xfer, goffset size);

/**
 * Sets the local port of the file transfer.
 *
 * @param xfer          The file transfer.
 * @param local_port    The local port.
 */
void purple_xfer_set_local_port(PurpleXfer *xfer, guint16 local_port);

/**
 * Sets the current working position in the active file transfer.  This
 * can be used to jump backward in the file if the protocol detects
 * that some bit of data needs to be resent or has been sent twice.
 *
 * It's used for pausing and resuming an oscar file transfer.
 *
 * @param xfer       The file transfer.
 * @param bytes_sent The new current position in the file.  If we're
 *                   sending a file then this is the byte that we will
 *                   send.  If we're receiving a file, this is the
 *                   next byte that we expect to receive.
 */
void purple_xfer_set_bytes_sent(PurpleXfer *xfer, goffset bytes_sent);

/**
 * Returns the UI operations structure for a file transfer.
 *
 * @param xfer The file transfer.
 *
 * @return The UI operations structure.
 */
PurpleXferUiOps *purple_xfer_get_ui_ops(const PurpleXfer *xfer);

/**
 * Sets the read function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The read function.
 */
void purple_xfer_set_read_fnc(PurpleXfer *xfer,
		gssize (*fnc)(guchar **, PurpleXfer *));

/**
 * Sets the write function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The write function.
 */
void purple_xfer_set_write_fnc(PurpleXfer *xfer,
		gssize (*fnc)(const guchar *, size_t, PurpleXfer *));

/**
 * Sets the acknowledge function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The acknowledge function.
 */
void purple_xfer_set_ack_fnc(PurpleXfer *xfer,
		void (*fnc)(PurpleXfer *, const guchar *, size_t));

/**
 * Sets the function to be called if the request is denied.
 *
 * @param xfer The file transfer.
 * @param fnc The request denied prpl callback.
 */
void purple_xfer_set_request_denied_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * Sets the transfer initialization function for the file transfer.
 *
 * This function is required, and must call purple_xfer_start() with
 * the necessary parameters. This will be called if the file transfer
 * is accepted by the user.
 *
 * @param xfer The file transfer.
 * @param fnc  The transfer initialization function.
 */
void purple_xfer_set_init_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * Sets the start transfer function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The start transfer function.
 */
void purple_xfer_set_start_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * Sets the end transfer function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The end transfer function.
 */
void purple_xfer_set_end_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * Sets the cancel send function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The cancel send function.
 */
void purple_xfer_set_cancel_send_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * Sets the cancel receive function for the file transfer.
 *
 * @param xfer The file transfer.
 * @param fnc  The cancel receive function.
 */
void purple_xfer_set_cancel_recv_fnc(PurpleXfer *xfer, void (*fnc)(PurpleXfer *));

/**
 * Reads in data from a file transfer stream.
 *
 * @param xfer   The file transfer.
 * @param buffer The buffer that will be created to contain the data.
 *
 * @return The number of bytes read, or -1.
 */
gssize purple_xfer_read(PurpleXfer *xfer, guchar **buffer);

/**
 * Writes data to a file transfer stream.
 *
 * @param xfer   The file transfer.
 * @param buffer The buffer to read the data from.
 * @param size   The number of bytes to write.
 *
 * @return The number of bytes written, or -1.
 */
gssize purple_xfer_write(PurpleXfer *xfer, const guchar *buffer, gsize size);

/**
 * Writes chunk of received file.
 *
 * @param xfer   The file transfer.
 * @param buffer The buffer to read the data from.
 * @param size   The number of bytes to write.
 *
 * @return TRUE on success, FALSE otherwise.
 */
gboolean
purple_xfer_write_file(PurpleXfer *xfer, const guchar *buffer, gsize size);

/**
 * Writes chunk of file being sent.
 *
 * @param xfer   The file transfer.
 * @param buffer The buffer to write the data to.
 * @param size   The size of buffer.
 *
 * @return Number of bytes written (0 means, the device is busy), or -1 on
 *         failure.
 */
gssize
purple_xfer_read_file(PurpleXfer *xfer, guchar *buffer, gsize size);

/**
 * Starts a file transfer.
 *
 * Either @a fd must be specified <i>or</i> @a ip and @a port on a
 * file receive transfer. On send, @a fd must be specified, and
 * @a ip and @a port are ignored.
 *
 * Passing @a fd as '-1' is a special-case and indicates to the
 * protocol plugin to facilitate the file transfer itself.
 *
 * @param xfer The file transfer.
 * @param fd   The file descriptor for the socket.
 * @param ip   The IP address to connect to.
 * @param port The port to connect to.
 */
void purple_xfer_start(PurpleXfer *xfer, int fd, const char *ip, guint16 port);

/**
 * Ends a file transfer.
 *
 * @param xfer The file transfer.
 */
void purple_xfer_end(PurpleXfer *xfer);

/**
 * Adds a new file transfer to the list of file transfers. Call this only
 * if you are not using purple_xfer_start.
 *
 * @param xfer The file transfer.
 */
void purple_xfer_add(PurpleXfer *xfer);

/**
 * Cancels a file transfer on the local end.
 *
 * @param xfer The file transfer.
 */
void purple_xfer_cancel_local(PurpleXfer *xfer);

/**
 * Cancels a file transfer from the remote end.
 *
 * @param xfer The file transfer.
 */
void purple_xfer_cancel_remote(PurpleXfer *xfer);

/**
 * Displays a file transfer-related error message.
 *
 * This is a wrapper around purple_notify_error(), which automatically
 * specifies a title ("File transfer to <i>user</i> failed" or
 * "File Transfer from <i>user</i> failed").
 *
 * @param type    The type of file transfer.
 * @param account The account sending or receiving the file.
 * @param who     The user on the other end of the transfer.
 * @param msg     The message to display.
 */
void purple_xfer_error(PurpleXferType type, PurpleAccount *account, const char *who, const char *msg);

/**
 * Updates file transfer progress.
 *
 * @param xfer      The file transfer.
 */
void purple_xfer_update_progress(PurpleXfer *xfer);

/**
 * Displays a file transfer-related message in the conversation window
 *
 * This is a wrapper around purple_conversation_write
 *
 * @param xfer The file transfer to which this message relates.
 * @param message The message to display.
 * @param is_error Is this an error message?.
 */
void purple_xfer_conversation_write(PurpleXfer *xfer, const gchar *message, gboolean is_error);

/**
 * Allows the UI to signal it's ready to send/receive data (depending on
 * the direction of the file transfer. Used when the UI is providing
 * read/write/data_not_sent UI ops.
 *
 * @param xfer The file transfer which is ready.
 */
void purple_xfer_ui_ready(PurpleXfer *xfer);

/**
 * Allows the prpl to signal it's ready to send/receive data (depending on
 * the direction of the file transfer. Used when the prpl provides read/write
 * ops and cannot/does not provide a raw fd to the core.
 *
 * @param xfer The file transfer which is ready.
 */
void purple_xfer_prpl_ready(PurpleXfer *xfer);

/**
 * Gets the thumbnail data for a transfer
 *
 * @param xfer The file transfer to get the thumbnail for
 * @param len  If not @c NULL, the length of the thumbnail data returned
 *             will be set in the location pointed to by this.
 * @return The thumbnail data, or NULL if there is no thumbnail
 */
gconstpointer purple_xfer_get_thumbnail(const PurpleXfer *xfer, gsize *len);

/**
 * Gets the mimetype of the thumbnail preview for a transfer
 *
 * @param xfer The file transfer to get the mimetype for
 * @return The mimetype of the thumbnail, or @c NULL if not thumbnail is set
 */
const gchar *purple_xfer_get_thumbnail_mimetype(const PurpleXfer *xfer);


/**
 * Sets the thumbnail data for a transfer
 *
 * @param xfer The file transfer to set the data for
 * @param thumbnail A pointer to the thumbnail data, this will be copied
 * @param size The size in bytes of the passed in thumbnail data
 * @param mimetype The mimetype of the generated thumbnail
 */
void purple_xfer_set_thumbnail(PurpleXfer *xfer, gconstpointer thumbnail,
	gsize size, const gchar *mimetype);

/**
 * Prepare a thumbnail for a transfer (if the UI supports it)
 * will be no-op in case the UI doesn't implement thumbnail creation
 *
 * @param xfer The file transfer to create a thumbnail for
 * @param formats A comma-separated list of mimetypes for image formats
 *	 	  the protocols can use for thumbnails.
 */
void purple_xfer_prepare_thumbnail(PurpleXfer *xfer, const gchar *formats);

/**
 * Sets the protocol data for a file transfer.
 *
 * @param xfer			The file transfer.
 * @param proto_data	The protocol data to set for the file transfer.
 */
void purple_xfer_set_protocol_data(PurpleXfer *xfer, gpointer proto_data);
 
/**
 * Gets the protocol data for a file transfer.
 *
 * @param xfer			The file transfer.
 *
 * @return The protocol data for the file transfer.
 */
gpointer purple_xfer_get_protocol_data(const PurpleXfer *xfer);

/**
 * Set the UI data associated with this file transfer.
 *
 * @param xfer			The file transfer.
 * @param ui_data		A pointer to associate with this file transfer.
 */
void purple_xfer_set_ui_data(PurpleXfer *xfer, gpointer ui_data);

/**
 * Get the UI data associated with this file transfer.
 *
 * @param xfer			The file transfer.
 *
 * @return The UI data associated with this file transfer.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_xfer_get_ui_data(const PurpleXfer *xfer);

/*@}*/

/**************************************************************************/
/** @name File Transfer Subsystem API                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns all xfers
 *
 * @return all current xfers with refs
 */
GList *purple_xfers_get_all(void);

/**
 * Returns the handle to the file transfer subsystem
 *
 * @return The handle
 */
void *purple_xfers_get_handle(void);

/**
 * Initializes the file transfer subsystem
 */
void purple_xfers_init(void);

/**
 * Uninitializes the file transfer subsystem
 */
void purple_xfers_uninit(void);

/**
 * Sets the UI operations structure to be used in all purple file transfers.
 *
 * @param ops The UI operations structure.
 */
void purple_xfers_set_ui_ops(PurpleXferUiOps *ops);

/**
 * Returns the UI operations structure to be used in all purple file transfers.
 *
 * @return The UI operations structure.
 */
PurpleXferUiOps *purple_xfers_get_ui_ops(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_XFER_H_ */

