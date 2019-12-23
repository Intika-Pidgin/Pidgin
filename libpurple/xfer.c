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
 *
 */
#include "internal.h"
#include "glibcompat.h" /* for purple_g_stat on win32 */

#include "enums.h"
#include "image-store.h"
#include "xfer.h"
#include "network.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "request.h"
#include "util.h"
#include "debug.h"

#define FT_INITIAL_BUFFER_SIZE 4096
#define FT_MAX_BUFFER_SIZE     65535

typedef struct _PurpleXferPrivate  PurpleXferPrivate;

static PurpleXferUiOps *xfer_ui_ops = NULL;
static GList *xfers;

/* Private data for a file transfer */
struct _PurpleXferPrivate {
	PurpleXferType type;         /* The type of transfer.               */

	PurpleAccount *account;      /* The account.                        */

	char *who;                   /* The person on the other end of the
	                                transfer.                           */

	char *message;               /* A message sent with the request     */
	char *filename;              /* The name sent over the network.     */
	char *local_filename;        /* The name on the local hard drive.   */
	goffset size;                /* The size of the file.               */

	FILE *dest_fp;               /* The destination file pointer.       */

	char *remote_ip;             /* The remote IP address.              */
	guint16 local_port;          /* The local port.                     */
	guint16 remote_port;         /* The remote port.                    */

	int fd;                      /* The socket file descriptor.         */
	int watcher;                 /* Watcher.                            */

	goffset bytes_sent;          /* The number of bytes sent.           */
	gint64 start_time;           /* When the transfer of data began.    */
	gint64 end_time;             /* When the transfer of data ended.    */

	size_t current_buffer_size;  /* This gradually increases for fast
	                                 network connections.               */

	PurpleXferStatus status;     /* File Transfer's status.             */

	gboolean visible;            /* Hint the UI that the transfer should
	                                be visible or not. */
	PurpleXferUiOps *ui_ops;     /* UI-specific operations.             */

	/*
	 * Used to moderate the file transfer when either the read/write ui_ops are
	 * set or fd is not set. In those cases, the UI/protocol call the respective
	 * function, which is somewhat akin to a fd watch being triggered.
	 */
	enum {
		PURPLE_XFER_READY_NONE = 0x0,
		PURPLE_XFER_READY_UI   = 0x1,
		PURPLE_XFER_READY_PROTOCOL = 0x2,
	} ready;

	/* TODO: Should really use a PurpleCircBuffer for this. */
	GByteArray *buffer;

	gpointer thumbnail_data;     /* thumbnail image */
	gsize thumbnail_size;
	gchar *thumbnail_mimetype;
};

/* GObject property enums */
enum
{
	PROP_0,
	PROP_TYPE,
	PROP_ACCOUNT,
	PROP_REMOTE_USER,
	PROP_MESSAGE,
	PROP_FILENAME,
	PROP_LOCAL_FILENAME,
	PROP_FILE_SIZE,
	PROP_REMOTE_IP,
	PROP_LOCAL_PORT,
	PROP_REMOTE_PORT,
	PROP_FD,
	PROP_WATCHER,
	PROP_BYTES_SENT,
	PROP_START_TIME,
	PROP_END_TIME,
	PROP_STATUS,
	PROP_PROGRESS,
	PROP_VISIBLE,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

/* GObject signal enums */
enum
{
	SIG_OPEN_LOCAL,
	SIG_QUERY_LOCAL,
	SIG_READ_LOCAL,
	SIG_WRITE_LOCAL,
	SIG_DATA_NOT_SENT,
	SIG_ADD_THUMBNAIL,
	SIG_LAST
};
static guint signals[SIG_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE(PurpleXfer, purple_xfer, G_TYPE_OBJECT);

static int purple_xfer_choose_file(PurpleXfer *xfer);

static const gchar *
purple_xfer_status_type_to_string(PurpleXferStatus type)
{
	static const struct {
		PurpleXferStatus type;
		const char *name;
	} type_names[] = {
		{ PURPLE_XFER_STATUS_UNKNOWN, "unknown" },
		{ PURPLE_XFER_STATUS_NOT_STARTED, "not started" },
		{ PURPLE_XFER_STATUS_ACCEPTED, "accepted" },
		{ PURPLE_XFER_STATUS_STARTED, "started" },
		{ PURPLE_XFER_STATUS_DONE, "done" },
		{ PURPLE_XFER_STATUS_CANCEL_LOCAL, "cancelled locally" },
		{ PURPLE_XFER_STATUS_CANCEL_REMOTE, "cancelled remotely" }
	};
	gsize i;

	for (i = 0; i < G_N_ELEMENTS(type_names); ++i)
		if (type_names[i].type == type)
			return type_names[i].name;

	return "invalid state";
}

void
purple_xfer_set_status(PurpleXfer *xfer, PurpleXferStatus status)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	if (purple_debug_is_verbose())
		purple_debug_info("xfer", "Changing status of xfer %p from %s to %s\n",
				xfer, purple_xfer_status_type_to_string(priv->status),
				purple_xfer_status_type_to_string(status));

	if (priv->status == status)
		return;

	priv->status = status;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_STATUS]);
}

void
purple_xfer_set_visible(PurpleXfer *xfer, gboolean visible)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	priv->visible = visible;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_VISIBLE]);
}

static void
purple_xfer_conversation_write_internal(PurpleXfer *xfer,
	const char *message, gboolean is_error, gboolean print_thumbnail)
{
	PurpleIMConversation *im = NULL;
	PurpleMessageFlags flags = PURPLE_MESSAGE_SYSTEM;
	char *escaped;
	gconstpointer thumbnail_data;
	gsize size;
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	thumbnail_data = purple_xfer_get_thumbnail(xfer, &size);

	im = purple_conversations_find_im_with_account(priv->who,
											   purple_xfer_get_account(xfer));

	if (im == NULL)
		return;

	escaped = g_markup_escape_text(message, -1);

	if (is_error)
		flags |= PURPLE_MESSAGE_ERROR;

	if (print_thumbnail && thumbnail_data) {
		gchar *message_with_img;
		PurpleImage *img;
		guint id;

		img = purple_image_new_from_data(thumbnail_data, size);
		id = purple_image_store_add(img);
		g_object_unref(img);

		message_with_img = g_strdup_printf("<img src=\""
			PURPLE_IMAGE_STORE_PROTOCOL "%u\"> %s", id, escaped);
		purple_conversation_write_system_message(
			PURPLE_CONVERSATION(im), message_with_img, flags);
		g_free(message_with_img);
	} else {
		purple_conversation_write_system_message(
			PURPLE_CONVERSATION(im), escaped, flags);
	}
	g_free(escaped);
}

void
purple_xfer_conversation_write(PurpleXfer *xfer, const gchar *message,
	gboolean is_error)
{
	g_return_if_fail(PURPLE_IS_XFER(xfer));
	g_return_if_fail(message != NULL);

	purple_xfer_conversation_write_internal(xfer, message, is_error, FALSE);
}

/* maybe this one should be exported publically? */
static void
purple_xfer_conversation_write_with_thumbnail(PurpleXfer *xfer,
	const gchar *message)
{
	purple_xfer_conversation_write_internal(xfer, message, FALSE, TRUE);
}


static void purple_xfer_show_file_error(PurpleXfer *xfer, const char *filename)
{
	int err = errno;
	gchar *msg = NULL, *utf8;
	PurpleXferType xfer_type = purple_xfer_get_xfer_type(xfer);
	PurpleAccount *account = purple_xfer_get_account(xfer);
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	switch(xfer_type) {
		case PURPLE_XFER_TYPE_SEND:
			msg = g_strdup_printf(_("Error reading %s: \n%s.\n"),
								  utf8, g_strerror(err));
			break;
		case PURPLE_XFER_TYPE_RECEIVE:
			msg = g_strdup_printf(_("Error writing %s: \n%s.\n"),
								  utf8, g_strerror(err));
			break;
		default:
			msg = g_strdup_printf(_("Error accessing %s: \n%s.\n"),
								  utf8, g_strerror(err));
			break;
	}
	g_free(utf8);

	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(xfer_type, account, priv->who, msg);
	g_free(msg);
}

static void
purple_xfer_choose_file_ok_cb(void *user_data, const char *filename)
{
	PurpleXfer *xfer;
	PurpleXferType type;
	GStatBuf st;
	gchar *dir;

	xfer = (PurpleXfer *)user_data;
	type = purple_xfer_get_xfer_type(xfer);

	if (g_stat(filename, &st) != 0) {
		/* File not found. */
		if (type == PURPLE_XFER_TYPE_RECEIVE) {
#ifndef _WIN32
			int mode = W_OK;
#else
			int mode = F_OK;
#endif
			dir = g_path_get_dirname(filename);

			if (g_access(dir, mode) == 0) {
				purple_xfer_request_accepted(xfer, filename);
			} else {
				g_object_ref(xfer);
				purple_notify_message(
					NULL, PURPLE_NOTIFY_MSG_ERROR, NULL,
					_("Directory is not writable."), NULL,
					purple_request_cpar_from_account(
						purple_xfer_get_account(xfer)),
					(PurpleNotifyCloseCallback)purple_xfer_choose_file, xfer);
			}

			g_free(dir);
		}
		else {
			purple_xfer_show_file_error(xfer, filename);
			purple_xfer_cancel_local(xfer);
		}
	}
	else if ((type == PURPLE_XFER_TYPE_SEND) && (st.st_size == 0)) {

		purple_notify_error(NULL, NULL,
			_("Cannot send a file of 0 bytes."), NULL,
			purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));

		purple_xfer_cancel_local(xfer);
	}
	else if ((type == PURPLE_XFER_TYPE_SEND) && S_ISDIR(st.st_mode)) {
		/*
		 * XXX - Sending a directory should be valid for some protocols.
		 */
		purple_notify_error(NULL, NULL, _("Cannot send a directory."),
			NULL, purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));

		purple_xfer_cancel_local(xfer);
	}
	else if ((type == PURPLE_XFER_TYPE_RECEIVE) && S_ISDIR(st.st_mode)) {
		char *msg, *utf8;
		utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
		msg = g_strdup_printf(
					_("%s is not a regular file. Cowardly refusing to overwrite it.\n"), utf8);
		g_free(utf8);
		purple_notify_error(NULL, NULL, msg, NULL,
			purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));
		g_free(msg);
		purple_xfer_request_denied(xfer);
	}
	else if (type == PURPLE_XFER_TYPE_SEND) {
#ifndef _WIN32
		int mode = R_OK;
#else
		int mode = F_OK;
#endif

		if (g_access(filename, mode) == 0) {
			purple_xfer_request_accepted(xfer, filename);
		} else {
			g_object_ref(xfer);
			purple_notify_message(
				NULL, PURPLE_NOTIFY_MSG_ERROR, NULL,
				_("File is not readable."), NULL,
				purple_request_cpar_from_account(
					purple_xfer_get_account(xfer)),
				(PurpleNotifyCloseCallback)purple_xfer_choose_file, xfer);
		}
	}
	else {
		purple_xfer_request_accepted(xfer, filename);
	}

	g_object_unref(xfer);
}

static void
purple_xfer_choose_file_cancel_cb(void *user_data, const char *filename)
{
	PurpleXfer *xfer = (PurpleXfer *)user_data;

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND)
		purple_xfer_cancel_local(xfer);
	else
		purple_xfer_request_denied(xfer);
	g_object_unref(xfer);
}

static int
purple_xfer_choose_file(PurpleXfer *xfer)
{
	purple_request_file(xfer, NULL, purple_xfer_get_filename(xfer),
		(purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE),
		G_CALLBACK(purple_xfer_choose_file_ok_cb),
		G_CALLBACK(purple_xfer_choose_file_cancel_cb),
		purple_request_cpar_from_account(purple_xfer_get_account(xfer)),
		xfer);

	return 0;
}

static int
cancel_recv_cb(PurpleXfer *xfer)
{
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	purple_xfer_request_denied(xfer);
	g_object_unref(xfer);

	return 0;
}

static void
purple_xfer_ask_recv(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);
	char *buf, *size_buf;
	goffset size;
	gconstpointer thumb;
	gsize thumb_size;

	/* If we have already accepted the request, ask the destination file
	   name directly */
	if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_ACCEPTED) {
		PurpleRequestCommonParameters *cpar;
		PurpleBuddy *buddy = purple_blist_find_buddy(priv->account, priv->who);

		if (purple_xfer_get_filename(xfer) != NULL)
		{
			size = purple_xfer_get_size(xfer);
			size_buf = g_format_size_full(size, G_FORMAT_SIZE_LONG_FORMAT);
			buf = g_strdup_printf(_("%s wants to send you %s (%s)"),
						  buddy ? purple_buddy_get_alias(buddy) : priv->who,
						  purple_xfer_get_filename(xfer), size_buf);
			g_free(size_buf);
		}
		else
		{
			buf = g_strdup_printf(_("%s wants to send you a file"),
						buddy ? purple_buddy_get_alias(buddy) : priv->who);
		}

		if (priv->message != NULL)
			purple_serv_got_im(purple_account_get_connection(priv->account),
								 priv->who, priv->message, 0, time(NULL));

		cpar = purple_request_cpar_from_account(priv->account);
		if ((thumb = purple_xfer_get_thumbnail(xfer, &thumb_size))) {
			purple_request_cpar_set_custom_icon(cpar, thumb,
				thumb_size);
		}

		purple_request_accept_cancel(xfer, NULL, buf, NULL,
			PURPLE_DEFAULT_ACTION_NONE, cpar, xfer,
			G_CALLBACK(purple_xfer_choose_file),
			G_CALLBACK(cancel_recv_cb));

		g_free(buf);
	} else
		purple_xfer_choose_file(xfer);
}

static int
ask_accept_ok(PurpleXfer *xfer)
{
	purple_xfer_request_accepted(xfer, NULL);

	return 0;
}

static int
ask_accept_cancel(PurpleXfer *xfer)
{
	purple_xfer_request_denied(xfer);
	g_object_unref(xfer);

	return 0;
}

static void
purple_xfer_ask_accept(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);
	char *buf, *buf2 = NULL;
	PurpleBuddy *buddy = purple_blist_find_buddy(priv->account, priv->who);

	buf = g_strdup_printf(_("Accept file transfer request from %s?"),
				  buddy ? purple_buddy_get_alias(buddy) : priv->who);
	if (purple_xfer_get_remote_ip(xfer) &&
		purple_xfer_get_remote_port(xfer))
		buf2 = g_strdup_printf(_("A file is available for download from:\n"
					 "Remote host: %s\nRemote port: %d"),
					   purple_xfer_get_remote_ip(xfer),
					   purple_xfer_get_remote_port(xfer));
	purple_request_accept_cancel(xfer, NULL, buf, buf2,
		PURPLE_DEFAULT_ACTION_NONE,
		purple_request_cpar_from_account(priv->account), xfer,
		G_CALLBACK(ask_accept_ok), G_CALLBACK(ask_accept_cancel));
	g_free(buf);
	g_free(buf2);
}

void
purple_xfer_request(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	/* this is unref'd in the finishers, like cancel and stop */
	g_object_ref(xfer);

	priv = purple_xfer_get_instance_private(xfer);

	if (priv->type == PURPLE_XFER_TYPE_RECEIVE)
	{
		purple_signal_emit(purple_xfers_get_handle(), "file-recv-request", xfer);
		if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL)
		{
			/* The file-transfer was cancelled by a plugin */
			purple_xfer_cancel_local(xfer);
		}
		else if (priv->filename || priv->status == PURPLE_XFER_STATUS_ACCEPTED)
		{
			gchar* message = NULL;
			PurpleBuddy *buddy = purple_blist_find_buddy(priv->account, priv->who);

			message = g_strdup_printf(_("%s is offering to send file %s"),
				buddy ? purple_buddy_get_alias(buddy) : priv->who, purple_xfer_get_filename(xfer));
			purple_xfer_conversation_write_with_thumbnail(xfer, message);
			g_free(message);

			/* Ask for a filename to save to if it's not already given by a plugin */
			if (priv->local_filename == NULL)
				purple_xfer_ask_recv(xfer);
		}
		else
		{
			purple_xfer_ask_accept(xfer);
		}
	}
	else
	{
		purple_xfer_choose_file(xfer);
	}
}

static gboolean
do_query_local(PurpleXfer *xfer, const gchar *filename)
{
	GStatBuf st;

	if (g_stat(filename, &st) == -1) {
		purple_xfer_show_file_error(xfer, filename);
		return FALSE;
	}

	purple_xfer_set_size(xfer, st.st_size);
	return TRUE;
}

void
purple_xfer_request_accepted(PurpleXfer *xfer, const gchar *filename)
{
	PurpleXferClass *klass = NULL;
	PurpleXferPrivate *priv = NULL;
	gchar *msg, *utf8, *base;
	PurpleBuddy *buddy;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	klass = PURPLE_XFER_GET_CLASS(xfer);
	priv = purple_xfer_get_instance_private(xfer);

	purple_debug_misc("xfer", "request accepted for %p\n", xfer);

	if(filename == NULL) {
		if(priv->type == PURPLE_XFER_TYPE_RECEIVE) {
			purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_ACCEPTED);
			klass->init(xfer);
		} else {
			purple_debug_warning(
				"xfer",
				"can not set file transfer without a file name"
			);
		}

		return;
	}

	buddy = purple_blist_find_buddy(priv->account, priv->who);

	if (priv->type == PURPLE_XFER_TYPE_SEND) {
		/* Sending a file */
		/* Check the filename. */
		gboolean query_status;

#ifdef _WIN32
		if (g_strrstr(filename, "../") || g_strrstr(filename, "..\\"))
#else
		if (g_strrstr(filename, "../"))
#endif
		{
			utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);

			msg = g_strdup_printf(_("%s is not a valid filename.\n"), utf8);
			purple_xfer_error(priv->type, priv->account, priv->who, msg);
			g_free(utf8);
			g_free(msg);

			g_object_unref(xfer);
			return;
		}

		g_signal_emit(xfer, signals[SIG_QUERY_LOCAL], 0, filename,
		              &query_status);
		if (!query_status) {
			g_object_unref(xfer);
			return;
		}
		purple_xfer_set_local_filename(xfer, filename);

		base = g_path_get_basename(filename);
		utf8 = g_filename_to_utf8(base, -1, NULL, NULL, NULL);
		g_free(base);
		purple_xfer_set_filename(xfer, utf8);

		msg = g_strdup_printf(_("Offering to send %s to %s"),
				utf8, buddy ? purple_buddy_get_alias(buddy) : priv->who);
		g_free(utf8);
		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}
	else {
		/* Receiving a file */
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_ACCEPTED);
		purple_xfer_set_local_filename(xfer, filename);

		msg = g_strdup_printf(_("Starting transfer of %s from %s"),
				priv->filename, buddy ? purple_buddy_get_alias(buddy) : priv->who);
		purple_xfer_conversation_write(xfer, msg, FALSE);
		g_free(msg);
	}

	purple_xfer_set_visible(xfer, TRUE);

	klass->init(xfer);
}

void
purple_xfer_request_denied(PurpleXfer *xfer)
{
	PurpleXferClass *klass = NULL;

	g_return_if_fail(PURPLE_XFER(xfer));

	klass = PURPLE_XFER_GET_CLASS(xfer);

	purple_debug_misc("xfer", "xfer %p denied\n", xfer);

	if(klass && klass->request_denied) {
		klass->request_denied(xfer);
	}

	g_object_unref(xfer);
}

int purple_xfer_get_fd(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->fd;
}

int purple_xfer_get_watcher(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->watcher;
}

PurpleXferType
purple_xfer_get_xfer_type(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), PURPLE_XFER_TYPE_UNKNOWN);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->type;
}

PurpleAccount *
purple_xfer_get_account(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->account;
}

void
purple_xfer_set_remote_user(PurpleXfer *xfer, const char *who)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	g_free(priv->who);
	priv->who = g_strdup(who);

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_REMOTE_USER]);
}

const char *
purple_xfer_get_remote_user(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->who;
}

PurpleXferStatus
purple_xfer_get_status(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), PURPLE_XFER_STATUS_UNKNOWN);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->status;
}

gboolean
purple_xfer_get_visible(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), FALSE);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->visible;
}

gboolean
purple_xfer_is_cancelled(PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), TRUE);

	if ((purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) ||
	    (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_REMOTE))
		return TRUE;
	else
		return FALSE;
}

gboolean
purple_xfer_is_completed(PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), TRUE);

	return (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_DONE);
}

const char *
purple_xfer_get_filename(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->filename;
}

const char *
purple_xfer_get_local_filename(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->local_filename;
}

goffset
purple_xfer_get_bytes_sent(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->bytes_sent;
}

goffset
purple_xfer_get_bytes_remaining(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->size - priv->bytes_sent;
}

goffset
purple_xfer_get_size(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->size;
}

double
purple_xfer_get_progress(PurpleXfer *xfer)
{
	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0.0);

	if (purple_xfer_get_size(xfer) == 0)
		return 0.0;

	return ((double)purple_xfer_get_bytes_sent(xfer) /
			(double)purple_xfer_get_size(xfer));
}

guint16
purple_xfer_get_local_port(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), -1);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->local_port;
}

const char *
purple_xfer_get_remote_ip(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->remote_ip;
}

guint16
purple_xfer_get_remote_port(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), -1);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->remote_port;
}

gint64
purple_xfer_get_start_time(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->start_time;
}

gint64
purple_xfer_get_end_time(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->end_time;
}

void purple_xfer_set_fd(PurpleXfer *xfer, int fd)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	priv->fd = fd;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_FD]);
}

void purple_xfer_set_watcher(PurpleXfer *xfer, int watcher)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	priv->watcher = watcher;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_WATCHER]);
}

void
purple_xfer_set_completed(PurpleXfer *xfer, gboolean completed)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	if (completed == TRUE) {
		char *msg = NULL;
		PurpleIMConversation *im;

		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_DONE);

		if (purple_xfer_get_filename(xfer) != NULL)
		{
			char *filename = g_markup_escape_text(purple_xfer_get_filename(xfer), -1);
			if (purple_xfer_get_local_filename(xfer)
			 && purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE)
			{
				char *local = g_markup_escape_text(purple_xfer_get_local_filename(xfer), -1);
				msg = g_strdup_printf(_("Transfer of file <A HREF=\"file://%s\">%s</A> complete"),
				                      local, filename);
				g_free(local);
			}
			else
				msg = g_strdup_printf(_("Transfer of file %s complete"),
				                      filename);
			g_free(filename);
		}
		else
			msg = g_strdup(_("File transfer complete"));

		im = purple_conversations_find_im_with_account(priv->who,
		                                             purple_xfer_get_account(xfer));

		if (im != NULL) {
			purple_conversation_write_system_message(
				PURPLE_CONVERSATION(im), msg, 0);
		}
		g_free(msg);
	}

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_PROGRESS]);
}

void
purple_xfer_set_message(PurpleXfer *xfer, const char *message)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	if (message != priv->message) {
		g_free(priv->message);
		priv->message = g_strdup(message);
	}

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_MESSAGE]);
}

const char *
purple_xfer_get_message(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->message;
}

void
purple_xfer_set_filename(PurpleXfer *xfer, const char *filename)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	if (filename != priv->filename) {
		g_free(priv->filename);
		priv->filename = g_strdup(filename);
	}

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_FILENAME]);
}

void
purple_xfer_set_local_filename(PurpleXfer *xfer, const char *filename)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	if (filename != priv->local_filename) {
		g_free(priv->local_filename);
		priv->local_filename = g_strdup(filename);
	}

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_LOCAL_FILENAME]);
}

void
purple_xfer_set_size(PurpleXfer *xfer, goffset size)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	priv->size = size;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_FILE_SIZE]);
	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_PROGRESS]);
}

void
purple_xfer_set_local_port(PurpleXfer *xfer, guint16 local_port)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	priv->local_port = local_port;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_LOCAL_PORT]);
}

void
purple_xfer_set_bytes_sent(PurpleXfer *xfer, goffset bytes_sent)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);
	priv->bytes_sent = bytes_sent;

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_BYTES_SENT]);
	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_PROGRESS]);
}

PurpleXferUiOps *
purple_xfer_get_ui_ops(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);
	return priv->ui_ops;
}

static void
purple_xfer_increase_buffer_size(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	priv->current_buffer_size = MIN(priv->current_buffer_size * 1.5,
			FT_MAX_BUFFER_SIZE);
}

static gssize
do_read(PurpleXfer *xfer, guchar **buffer, gsize size)
{
	PurpleXferPrivate *priv = NULL;
	gssize r;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);
	g_return_val_if_fail(buffer != NULL, 0);

	priv = purple_xfer_get_instance_private(xfer);

	*buffer = g_malloc0(size);

	r = read(priv->fd, *buffer, size);
	if (r < 0 && errno == EAGAIN) {
		r = 0;
	} else if (r < 0) {
		r = -1;
	} else if (r == 0) {
		r = -1;
	}

	return r;
}

gssize
purple_xfer_read(PurpleXfer *xfer, guchar **buffer)
{
	PurpleXferPrivate *priv = NULL;
	PurpleXferClass *klass = NULL;
	gsize s;
	gssize r;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);
	g_return_val_if_fail(buffer != NULL, 0);

	priv = purple_xfer_get_instance_private(xfer);

	if (purple_xfer_get_size(xfer) == 0) {
		s = priv->current_buffer_size;
	} else {
		s = MIN(
			(gssize)purple_xfer_get_bytes_remaining(xfer),
			(gssize)priv->current_buffer_size
		);
	}

	klass = PURPLE_XFER_GET_CLASS(xfer);
	if(klass && klass->read) {
		r = klass->read(xfer, buffer, s);
	} else {
		r = do_read(xfer, buffer, s);
	}

	if (r >= 0 && (gsize)r == priv->current_buffer_size) {
		/*
		 * We managed to read the entire buffer.  This means our
		 * network is fast and our buffer is too small, so make it
		 * bigger.
		 */
		purple_xfer_increase_buffer_size(xfer);
	}

	return r;
}

static gssize
do_write(PurpleXfer *xfer, const guchar *buffer, gsize size)
{
	PurpleXferPrivate *priv = NULL;
	gssize r;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);
	g_return_val_if_fail(buffer != NULL, 0);
	g_return_val_if_fail(size != 0, 0);

	priv = purple_xfer_get_instance_private(xfer);

	r = write(priv->fd, buffer, size);
	if (r < 0 && errno == EAGAIN)
		r = 0;

	return r;
}

gssize
purple_xfer_write(PurpleXfer *xfer, const guchar *buffer, gsize size)
{
	PurpleXferClass *klass = NULL;
	gssize s;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);

	s = MIN(
		(gssize)purple_xfer_get_bytes_remaining(xfer),
		(gssize)size
	);

	klass = PURPLE_XFER_GET_CLASS(xfer);
	if(klass && klass->write) {
		return klass->write(xfer, buffer, s);
	}

	return do_write(xfer, buffer, s);
}

static gssize
do_write_local(PurpleXfer *xfer, const guchar *buffer, gssize size)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	if (priv->dest_fp == NULL) {
		purple_debug_error("xfer", "File is not opened for writing");
		return -1;
	}

	return fwrite(buffer, 1, size, priv->dest_fp);
}

gboolean
purple_xfer_write_file(PurpleXfer *xfer, const guchar *buffer, gsize size)
{
	PurpleXferPrivate *priv = NULL;
	gsize wc;
	goffset remaining;
	gboolean fs_known;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), FALSE);
	g_return_val_if_fail(buffer != NULL, FALSE);

	priv = purple_xfer_get_instance_private(xfer);

	fs_known = (priv->size > 0);

	remaining = purple_xfer_get_bytes_remaining(xfer);
	if (fs_known && (goffset)size > remaining) {
		purple_debug_warning("xfer",
			"Got too much data (truncating at %" G_GOFFSET_FORMAT
			").\n", purple_xfer_get_size(xfer));
		size = remaining;
	}

	g_signal_emit(xfer, signals[SIG_WRITE_LOCAL], 0, buffer, size, &wc);

	if (wc != size) {
		purple_debug_error("xfer",
			"Unable to write whole buffer.\n");
		purple_xfer_cancel_local(xfer);
		return FALSE;
	}

	purple_xfer_set_bytes_sent(
		xfer,
		purple_xfer_get_bytes_sent(xfer) + size
	);

	return TRUE;
}

static gssize
do_read_local(PurpleXfer *xfer, guchar *buffer, gssize size)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);
	gssize got_len;

	if (priv->dest_fp == NULL) {
		purple_debug_error("xfer", "File is not opened for reading");
		return -1;
	}

	got_len = fread(buffer, 1, size, priv->dest_fp);
	if ((got_len < 0 || got_len != size) && ferror(priv->dest_fp)) {
		purple_debug_error("xfer", "Unable to read file.");
		return -1;
	}

	return got_len;
}

gssize
purple_xfer_read_file(PurpleXfer *xfer, guchar *buffer, gsize size)
{
	gssize got_len;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), 0);
	g_return_val_if_fail(buffer != NULL, 0);

	g_signal_emit(xfer, signals[SIG_READ_LOCAL], 0, buffer, size, &got_len);
	if (got_len < 0 || got_len > size) {
		purple_debug_error("xfer", "Unable to read file.");
		purple_xfer_cancel_local(xfer);
		return -1;
	}

	if (got_len > 0) {
		purple_xfer_set_bytes_sent(xfer,
			purple_xfer_get_bytes_sent(xfer) + got_len);
	}

	return got_len;
}

static gboolean
do_data_not_sent(PurpleXfer *xfer, const guchar *buffer, gsize size)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	if (priv->buffer == NULL) {
		priv->buffer = g_byte_array_sized_new(FT_INITIAL_BUFFER_SIZE);
		g_byte_array_append(priv->buffer, buffer, size);
	}

	return TRUE;
}

static void
do_transfer(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);
	guchar *buffer = NULL;
	gssize r = 0;

	if (priv->type == PURPLE_XFER_TYPE_RECEIVE) {
		r = purple_xfer_read(xfer, &buffer);
		if (r > 0) {
			if (!purple_xfer_write_file(xfer, buffer, r)) {
				g_free(buffer);
				return;
			}

		} else if(r < 0) {
			purple_xfer_cancel_remote(xfer);
			g_free(buffer);
			return;
		}
	} else if (priv->type == PURPLE_XFER_TYPE_SEND) {
		gssize result = 0;
		gsize s = MIN(
			(gsize)purple_xfer_get_bytes_remaining(xfer),
			(gsize)priv->current_buffer_size
		);
		gboolean read_more = TRUE;
		gboolean existing_buffer = FALSE;

		/* this is so the protocol can keep the connection open
		   if it needs to for some odd reason. */
		if (s == 0) {
			if (priv->watcher) {
				purple_input_remove(priv->watcher);
				purple_xfer_set_watcher(xfer, 0);
			}
			return;
		}

		if (priv->buffer) {
			existing_buffer = TRUE;
			if (priv->buffer->len < s) {
				s -= priv->buffer->len;
				read_more = TRUE;
			} else {
				read_more = FALSE;
			}
		}

		if (read_more) {
			buffer = g_new(guchar, s);
			result = purple_xfer_read_file(xfer, buffer, s);
			if (result == 0) {
				/*
				 * The UI claimed it was ready, but didn't have any data for
				 * us...  It will call purple_xfer_ui_ready when ready, which
				 * sets back up this watcher.
				 */
				if (priv->watcher != 0) {
					purple_input_remove(priv->watcher);
					purple_xfer_set_watcher(xfer, 0);
				}

				/* Need to indicate the protocol is still ready... */
				priv->ready |= PURPLE_XFER_READY_PROTOCOL;

				g_free(buffer);
				g_return_if_reached();
			}
			if (result < 0) {
				g_free(buffer);
				return;
			}
		}

		if (priv->buffer) {
			g_byte_array_append(priv->buffer, buffer, result);
			g_free(buffer);
			buffer = priv->buffer->data;
			result = priv->buffer->len;
		}

		r = do_write(xfer, buffer, result);

		if (r == -1) {
			purple_debug_error("xfer", "do_write failed! %s\n", g_strerror(errno));
			purple_xfer_cancel_remote(xfer);
			if (!priv->buffer)
				/* We don't free buffer if priv->buffer is set, because in
				   that case buffer doesn't belong to us. */
				g_free(buffer);
			return;
		} else if (r == result) {
			/*
			 * We managed to write the entire buffer.  This means our
			 * network is fast and our buffer is too small, so make it
			 * bigger.
			 */
			purple_xfer_increase_buffer_size(xfer);
		} else {
			gboolean result;
			g_signal_emit(xfer, signals[SIG_DATA_NOT_SENT], 0, buffer + r,
			              result - r, &result);
			if (!result) {
				purple_xfer_cancel_local(xfer);
			}
		}

		if (existing_buffer && priv->buffer) {
			/*
			 * Remove what we wrote
			 * If we wrote the whole buffer the byte array will be empty
			 * Otherwise we'll keep what wasn't sent for next time.
			 */
			buffer = NULL;
			g_byte_array_remove_range(priv->buffer, 0, r);
		}
	}

	if (r > 0) {
		PurpleXferClass *klass = PURPLE_XFER_GET_CLASS(xfer);

		if (klass && klass->ack)
			klass->ack(xfer, buffer, r);
	}

	g_free(buffer);

	if (purple_xfer_get_bytes_sent(xfer) >= purple_xfer_get_size(xfer) &&
			!purple_xfer_is_completed(xfer)) {
		purple_xfer_set_completed(xfer, TRUE);
	}

	/* TODO: Check if above is the only place xfers are marked completed.
	 *       If so, merge these conditions.
	 */
	if (purple_xfer_is_completed(xfer)) {
		purple_xfer_end(xfer);
	}
}

static void
transfer_cb(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer = data;
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	if (priv->dest_fp == NULL) {
		/* The UI is moderating its side manually */
		if (0 == (priv->ready & PURPLE_XFER_READY_UI)) {
			priv->ready |= PURPLE_XFER_READY_PROTOCOL;

			purple_input_remove(priv->watcher);
			purple_xfer_set_watcher(xfer, 0);

			purple_debug_misc("xfer", "Protocol is ready on ft %p, waiting for UI\n", xfer);
			return;
		}

		priv->ready = PURPLE_XFER_READY_NONE;
	}

	do_transfer(xfer);
}

static gboolean
do_open_local(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	priv->dest_fp =
	        g_fopen(purple_xfer_get_local_filename(xfer),
	                priv->type == PURPLE_XFER_TYPE_RECEIVE ? "wb" : "rb");

	if (priv->dest_fp == NULL) {
		purple_xfer_show_file_error(xfer, purple_xfer_get_local_filename(xfer));
		return FALSE;
	}

	if (fseek(priv->dest_fp, priv->bytes_sent, SEEK_SET) != 0) {
		purple_debug_error("xfer", "couldn't seek");
		purple_xfer_show_file_error(xfer, purple_xfer_get_local_filename(xfer));
		return FALSE;
	}

	return TRUE;
}

static void
begin_transfer(PurpleXfer *xfer, PurpleInputCondition cond)
{
	PurpleXferClass *klass = PURPLE_XFER_GET_CLASS(xfer);
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);
	gboolean open_status = FALSE;

	if (priv->start_time != 0) {
		purple_debug_error("xfer", "Transfer is being started multiple times\n");
		g_return_if_reached();
	}

	g_signal_emit(xfer, signals[SIG_OPEN_LOCAL], 0, &open_status);
	if (!open_status) {
		purple_xfer_cancel_local(xfer);
	}

	if (priv->fd != -1) {
		purple_xfer_set_watcher(
			xfer,
			purple_input_add(priv->fd, cond, transfer_cb, xfer)
		);
	}

	priv->start_time = g_get_monotonic_time();

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_START_TIME]);

	if (klass && klass->start) {
		klass->start(xfer);
	}
}

static void
connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer = PURPLE_XFER(data);

	if (source < 0) {
		purple_xfer_cancel_local(xfer);
		return;
	}

	purple_xfer_set_fd(xfer, source);
	begin_transfer(xfer, PURPLE_INPUT_READ);
}

void
purple_xfer_ui_ready(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;
	PurpleInputCondition cond = 0;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	priv->ready |= PURPLE_XFER_READY_UI;

	if (0 == (priv->ready & PURPLE_XFER_READY_PROTOCOL)) {
		purple_debug_misc("xfer", "UI is ready on ft %p, waiting for protocol\n", xfer);
		return;
	}

	purple_debug_misc("xfer", "UI (and protocol) ready on ft %p, so proceeding\n", xfer);

	if (priv->type == PURPLE_XFER_TYPE_SEND) {
		cond = PURPLE_INPUT_WRITE;
	} else if (priv->type == PURPLE_XFER_TYPE_RECEIVE) {
		cond = PURPLE_INPUT_READ;
	}

	if (priv->watcher == 0 && priv->fd != -1) {
		purple_xfer_set_watcher(
			xfer,
			purple_input_add(priv->fd, cond, transfer_cb, xfer)
		);
	}

	priv->ready = PURPLE_XFER_READY_NONE;

	do_transfer(xfer);
}

void
purple_xfer_protocol_ready(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	priv->ready |= PURPLE_XFER_READY_PROTOCOL;

	/* I don't think fwrite/fread are ever *not* ready */
	if (priv->dest_fp == NULL && 0 == (priv->ready & PURPLE_XFER_READY_UI)) {
		purple_debug_misc("xfer", "Protocol is ready on ft %p, waiting for UI\n", xfer);
		return;
	}

	purple_debug_misc("xfer", "Protocol (and UI) ready on ft %p, so proceeding\n", xfer);

	priv->ready = PURPLE_XFER_READY_NONE;

	do_transfer(xfer);
}

void
purple_xfer_start(PurpleXfer *xfer, int fd, const char *ip, guint16 port)
{
	PurpleXferPrivate *priv = NULL;
	PurpleInputCondition cond;
	GObject *obj;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_STARTED);

	if (priv->type == PURPLE_XFER_TYPE_RECEIVE) {
		cond = PURPLE_INPUT_READ;

		if (ip != NULL) {
			priv->remote_ip   = g_strdup(ip);
			priv->remote_port = port;

			obj = G_OBJECT(xfer);
			g_object_freeze_notify(obj);
			g_object_notify_by_pspec(obj, properties[PROP_REMOTE_IP]);
			g_object_notify_by_pspec(obj, properties[PROP_REMOTE_PORT]);
			g_object_thaw_notify(obj);

			/* Establish a file descriptor. */
			purple_proxy_connect(
				NULL,
				priv->account,
				priv->remote_ip,
				priv->remote_port,
				connect_cb,
				xfer
			);

			return;
		} else {
			purple_xfer_set_fd(xfer, fd);
		}
	} else {
		cond = PURPLE_INPUT_WRITE;

		purple_xfer_set_fd(xfer, fd);
	}

	begin_transfer(xfer, cond);
}

void
purple_xfer_end(PurpleXfer *xfer)
{
	PurpleXferClass *klass = NULL;
	PurpleXferPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	klass = PURPLE_XFER_GET_CLASS(xfer);
	priv = purple_xfer_get_instance_private(xfer);

	/* See if we are actually trying to cancel this. */
	if (!purple_xfer_is_completed(xfer)) {
		purple_xfer_cancel_local(xfer);
		return;
	}

	priv->end_time = g_get_monotonic_time();

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_END_TIME]);

	if (klass && klass->end != NULL) {
		klass->end(xfer);
	}

	if (priv->watcher != 0) {
		purple_input_remove(priv->watcher);
		purple_xfer_set_watcher(xfer, 0);
	}

	if (priv->fd != -1) {
		if (close(priv->fd)) {
			purple_debug_error("xfer", "closing file descr in purple_xfer_end() failed: %s",
					g_strerror(errno));
		}
	}

	if (priv->dest_fp != NULL) {
		if (fclose(priv->dest_fp)) {
			purple_debug_error("xfer", "closing dest file in purple_xfer_end() failed: %s",
					g_strerror(errno));
		}
		priv->dest_fp = NULL;
	}

	g_object_unref(xfer);
}

void
purple_xfer_cancel_local(PurpleXfer *xfer)
{
	PurpleXferClass *klass = NULL;
	PurpleXferPrivate *priv = NULL;
	char *msg = NULL;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	klass = PURPLE_XFER_GET_CLASS(xfer);
	priv = purple_xfer_get_instance_private(xfer);

	/* TODO: We definitely want to close any open request dialogs associated
	   with this transfer.  However, in some cases the request dialog might
	   own a reference on the xfer.  This happens at least with the "%s wants
	   to send you %s" dialog from purple_xfer_ask_recv().  In these cases
	   the ref count will not be decremented when the request dialog is
	   closed, so the ref count will never reach 0 and the xfer will never
	   be freed.  This is a memleak and should be fixed.  It's not clear what
	   the correct fix is.  Probably requests should have a destroy function
	   that is called when the request is destroyed.  But also, ref counting
	   xfer objects makes this code REALLY complicated.  An alternate fix is
	   to not ref count and instead just make sure the object still exists
	   when we try to use it. */
	purple_request_close_with_handle(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	priv->end_time = g_get_monotonic_time();

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_END_TIME]);

	if (purple_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("You cancelled the transfer of %s"),
							  purple_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup(_("File transfer cancelled"));
	}
	purple_xfer_conversation_write(xfer, msg, FALSE);
	g_free(msg);

	if (priv->type == PURPLE_XFER_TYPE_SEND)
	{
		if (klass && klass->cancel_send) {
			klass->cancel_send(xfer);
		}
	} else {
		if (klass && klass->cancel_recv) {
			klass->cancel_recv(xfer);
		}
	}

	if (priv->watcher != 0) {
		purple_input_remove(priv->watcher);
		purple_xfer_set_watcher(xfer, 0);
	}

	if (priv->fd != -1) {
		close(priv->fd);
	}

	if (priv->dest_fp != NULL) {
		fclose(priv->dest_fp);
		priv->dest_fp = NULL;
	}

	g_object_unref(xfer);
}

void
purple_xfer_cancel_remote(PurpleXfer *xfer)
{
	PurpleXferClass *klass = NULL;
	PurpleXferPrivate *priv = NULL;
	gchar *msg;
	PurpleAccount *account;
	PurpleBuddy *buddy;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	klass = PURPLE_XFER_GET_CLASS(xfer);
	priv = purple_xfer_get_instance_private(xfer);

	purple_request_close_with_handle(xfer);
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
	priv->end_time = g_get_monotonic_time();

	g_object_notify_by_pspec(G_OBJECT(xfer), properties[PROP_END_TIME]);

	account = purple_xfer_get_account(xfer);
	buddy = purple_blist_find_buddy(account, priv->who);

	if (purple_xfer_get_filename(xfer) != NULL)
	{
		msg = g_strdup_printf(_("%s cancelled the transfer of %s"),
				buddy ? purple_buddy_get_alias(buddy) : priv->who, purple_xfer_get_filename(xfer));
	}
	else
	{
		msg = g_strdup_printf(_("%s cancelled the file transfer"),
				buddy ? purple_buddy_get_alias(buddy) : priv->who);
	}
	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(purple_xfer_get_xfer_type(xfer), account, priv->who, msg);
	g_free(msg);

	if (priv->type == PURPLE_XFER_TYPE_SEND) {
		if (klass && klass->cancel_send) {
			klass->cancel_send(xfer);
		}
	} else if(priv->type == PURPLE_XFER_TYPE_RECEIVE) {
		if (klass && klass->cancel_recv) {
			klass->cancel_recv(xfer);
		}
	}

	if (priv->watcher != 0) {
		purple_input_remove(priv->watcher);
		purple_xfer_set_watcher(xfer, 0);
	}

	if (priv->fd != -1)
		close(priv->fd);

	if (priv->dest_fp != NULL) {
		fclose(priv->dest_fp);
		priv->dest_fp = NULL;
	}

	g_object_unref(xfer);
}

void
purple_xfer_error(PurpleXferType type, PurpleAccount *account, const gchar *who, const gchar *msg)
{
	gchar *title = NULL;

	g_return_if_fail(msg  != NULL);

	if (account) {
		PurpleBuddy *buddy;
		buddy = purple_blist_find_buddy(account, who);
		if (buddy)
			who = purple_buddy_get_alias(buddy);
	}

	if (type == PURPLE_XFER_TYPE_SEND) {
		title = g_strdup_printf(_("File transfer to %s failed."), who);
	} else if (type == PURPLE_XFER_TYPE_RECEIVE) {
		title = g_strdup_printf(_("File transfer from %s failed."), who);
	}

	purple_notify_error(NULL, NULL, title, msg,
		purple_request_cpar_from_account(account));

	g_free(title);
}

gconstpointer
purple_xfer_get_thumbnail(PurpleXfer *xfer, gsize *len)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);

	if (len) {
		*len = priv->thumbnail_size;
	}

	return priv->thumbnail_data;
}

const gchar *
purple_xfer_get_thumbnail_mimetype(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_XFER(xfer), NULL);

	priv = purple_xfer_get_instance_private(xfer);

	return priv->thumbnail_mimetype;
}

void
purple_xfer_set_thumbnail(PurpleXfer *xfer, gconstpointer thumbnail,
	gsize size, const gchar *mimetype)
{
	PurpleXferPrivate *priv = NULL;
	gpointer old_thumbnail_data;
	gchar *old_mimetype;

	g_return_if_fail(PURPLE_IS_XFER(xfer));

	priv = purple_xfer_get_instance_private(xfer);

	/* Hold onto these in case they are equal to passed-in pointers */
	old_thumbnail_data = priv->thumbnail_data;
	old_mimetype = priv->thumbnail_mimetype;

	if (thumbnail && size > 0) {
		priv->thumbnail_data = g_memdup(thumbnail, size);
		priv->thumbnail_size = size;
		priv->thumbnail_mimetype = g_strdup(mimetype);
	} else {
		priv->thumbnail_data = NULL;
		priv->thumbnail_size = 0;
		priv->thumbnail_mimetype = NULL;
	}

	/* Now it's safe to free the pointers */
	g_free(old_thumbnail_data);
	g_free(old_mimetype);
}

void
purple_xfer_prepare_thumbnail(PurpleXfer *xfer, const gchar *formats)
{
	g_return_if_fail(PURPLE_IS_XFER(xfer));

	g_signal_emit(xfer, signals[SIG_ADD_THUMBNAIL], 0, formats, NULL);
}

/**************************************************************************
 * GObject code
 **************************************************************************/
static void
purple_xfer_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleXfer *xfer = PURPLE_XFER(obj);
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	switch (param_id) {
		case PROP_TYPE:
			priv->type = g_value_get_enum(value);
			break;
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_REMOTE_USER:
			purple_xfer_set_remote_user(xfer, g_value_get_string(value));
			break;
		case PROP_MESSAGE:
			purple_xfer_set_message(xfer, g_value_get_string(value));
			break;
		case PROP_FILENAME:
			purple_xfer_set_filename(xfer, g_value_get_string(value));
			break;
		case PROP_LOCAL_FILENAME:
			purple_xfer_set_local_filename(xfer, g_value_get_string(value));
			break;
		case PROP_FILE_SIZE:
			purple_xfer_set_size(xfer, g_value_get_int64(value));
			break;
		case PROP_LOCAL_PORT:
			purple_xfer_set_local_port(xfer, g_value_get_int(value));
			break;
		case PROP_FD:
			purple_xfer_set_fd(xfer, g_value_get_int(value));
			break;
		case PROP_WATCHER:
			purple_xfer_set_watcher(xfer, g_value_get_int(value));
			break;
		case PROP_BYTES_SENT:
			purple_xfer_set_bytes_sent(xfer, g_value_get_int64(value));
			break;
		case PROP_STATUS:
			purple_xfer_set_status(xfer, g_value_get_enum(value));
			break;
		case PROP_VISIBLE:
			purple_xfer_set_visible(xfer, g_value_get_boolean(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_xfer_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleXfer *xfer = PURPLE_XFER(obj);

	switch (param_id) {
		case PROP_TYPE:
			g_value_set_enum(value, purple_xfer_get_xfer_type(xfer));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_xfer_get_account(xfer));
			break;
		case PROP_REMOTE_USER:
			g_value_set_string(value, purple_xfer_get_remote_user(xfer));
			break;
		case PROP_MESSAGE:
			g_value_set_string(value, purple_xfer_get_message(xfer));
			break;
		case PROP_FILENAME:
			g_value_set_string(value, purple_xfer_get_filename(xfer));
			break;
		case PROP_LOCAL_FILENAME:
			g_value_set_string(value, purple_xfer_get_local_filename(xfer));
			break;
		case PROP_FILE_SIZE:
			g_value_set_int64(value, purple_xfer_get_size(xfer));
			break;
		case PROP_REMOTE_IP:
			g_value_set_string(value, purple_xfer_get_remote_ip(xfer));
			break;
		case PROP_LOCAL_PORT:
			g_value_set_int(value, purple_xfer_get_local_port(xfer));
			break;
		case PROP_REMOTE_PORT:
			g_value_set_int(value, purple_xfer_get_remote_port(xfer));
			break;
		case PROP_FD:
			g_value_set_int(value, purple_xfer_get_fd(xfer));
			break;
		case PROP_WATCHER:
			g_value_set_int(value, purple_xfer_get_watcher(xfer));
			break;
		case PROP_BYTES_SENT:
			g_value_set_int64(value, purple_xfer_get_bytes_sent(xfer));
			break;
		case PROP_START_TIME:
			g_value_set_int64(value, purple_xfer_get_start_time(xfer));
			break;
		case PROP_END_TIME:
			g_value_set_int64(value, purple_xfer_get_end_time(xfer));
			break;
		case PROP_STATUS:
			g_value_set_enum(value, purple_xfer_get_status(xfer));
			break;
		case PROP_PROGRESS:
			g_value_set_double(value, purple_xfer_get_progress(xfer));
			break;
		case PROP_VISIBLE:
			g_value_set_boolean(value, purple_xfer_get_visible(xfer));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_xfer_init(PurpleXfer *xfer)
{
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	priv->ui_ops = purple_xfers_get_ui_ops();
	priv->current_buffer_size = FT_INITIAL_BUFFER_SIZE;
	priv->fd = -1;
	priv->ready = PURPLE_XFER_READY_NONE;
}

static void
purple_xfer_constructed(GObject *object)
{
	PurpleXfer *xfer = PURPLE_XFER(object);
	PurpleXferUiOps *ui_ops;

	G_OBJECT_CLASS(purple_xfer_parent_class)->constructed(object);

	ui_ops = purple_xfers_get_ui_ops();

	if (ui_ops != NULL && ui_ops->new_xfer != NULL) {
		ui_ops->new_xfer(xfer);
	}

	xfers = g_list_prepend(xfers, xfer);
}

static void
purple_xfer_finalize(GObject *object)
{
	PurpleXfer *xfer = PURPLE_XFER(object);
	PurpleXferPrivate *priv = purple_xfer_get_instance_private(xfer);

	/* Close the file browser, if it's open */
	purple_request_close_with_handle(xfer);

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_STARTED) {
		purple_xfer_cancel_local(xfer);
	}

	xfers = g_list_remove(xfers, xfer);

	g_free(priv->who);
	g_free(priv->filename);
	g_free(priv->remote_ip);
	g_free(priv->local_filename);

	if (priv->buffer) {
		g_byte_array_free(priv->buffer, TRUE);
	}

	g_free(priv->thumbnail_data);
	g_free(priv->thumbnail_mimetype);

	G_OBJECT_CLASS(purple_xfer_parent_class)->finalize(object);
}

/* Class initializer function */
static void
purple_xfer_class_init(PurpleXferClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_xfer_finalize;
	obj_class->constructed = purple_xfer_constructed;
	obj_class->get_property = purple_xfer_get_property;
	obj_class->set_property = purple_xfer_set_property;

	klass->write = do_write;
	klass->read = do_read;

	klass->open_local = do_open_local;
	klass->query_local = do_query_local;
	klass->read_local = do_read_local;
	klass->write_local = do_write_local;
	klass->data_not_sent = do_data_not_sent;

	/* Properties */

	properties[PROP_TYPE] = g_param_spec_enum("type", "Transfer type",
				"The type of file transfer.", PURPLE_TYPE_XFER_TYPE,
				PURPLE_XFER_TYPE_UNKNOWN,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	properties[PROP_ACCOUNT] = g_param_spec_object("account", "Account",
				"The account sending or receiving the file.",
				PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	properties[PROP_REMOTE_USER] = g_param_spec_string("remote-user",
				"Remote user",
				"The name of the remote user.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_MESSAGE] = g_param_spec_string("message", "Message",
				"The message for the file transfer.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_FILENAME] = g_param_spec_string("filename", "Filename",
				"The filename for the file transfer.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_LOCAL_FILENAME] = g_param_spec_string("local-filename",
				"Local filename",
				"The local filename for the file transfer.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_FILE_SIZE] = g_param_spec_int64("file-size", "File size",
				"Size of the file in a file transfer.",
				G_MININT64, G_MAXINT64, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_REMOTE_IP] = g_param_spec_string("remote-ip", "Remote IP",
				"The remote IP address in the file transfer.", NULL,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_LOCAL_PORT] = g_param_spec_int("local-port", "Local port",
				"The local port number in the file transfer.",
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_REMOTE_PORT] = g_param_spec_int("remote-port",
				"Remote port",
				"The remote port number in the file transfer.",
				G_MININT, G_MAXINT, 0,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_FD] = g_param_spec_int("fd", "Socket FD",
				"The socket file descriptor.",
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_WATCHER] = g_param_spec_int("watcher", "Watcher",
				"The watcher for the file transfer.",
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_BYTES_SENT] = g_param_spec_int64("bytes-sent", "Bytes sent",
				"The number of bytes sent (or received) so far.",
				G_MININT64, G_MAXINT64, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_START_TIME] = g_param_spec_int64(
	        "start-time", "Start time",
	        "The monotonic time the transfer of a file started.",
	        G_MININT64, G_MAXINT64, 0,
	        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_END_TIME] = g_param_spec_int64(
	        "end-time", "End time",
	        "The monotonic time the transfer of a file ended.", G_MININT64,
	        G_MAXINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_STATUS] = g_param_spec_enum("status", "Status",
				"The current status for the file transfer.",
				PURPLE_TYPE_XFER_STATUS, PURPLE_XFER_STATUS_UNKNOWN,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_PROGRESS] = g_param_spec_double(
	        "progress", "Progress",
	        "The current progress of the file transfer.", -1.0, 1.0, -1.0,
	        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_VISIBLE] = g_param_spec_boolean(
	        "visible", "Visible",
	        "Hint for UIs whether this transfer should be visible.", FALSE,
	        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);

	/* Signals */

	/**
	 * PurpleXfer::open-local:
	 *
	 * Open a file locally for a file transfer.
	 *
	 * The default class handler will open a file using standard library
	 * functions. If you connect to this signal, you should connect to
	 * PurpleXfer::query-local, PurpleXfer::read-local, PurpleXfer::write-local
	 * and PurpleXfer::data-not-sent as well.
	 *
	 * Returns: %TRUE if the file was opened successfully, or %FALSE otherwise,
	 *          and the transfer should be cancelled (libpurple will cancel).
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_OPEN_LOCAL] = g_signal_new(
	        "open-local", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
	        G_STRUCT_OFFSET(PurpleXferClass, open_local),
	        g_signal_accumulator_first_wins, NULL, NULL, G_TYPE_BOOLEAN, 0);

	/**
	 * PurpleXfer::query-local:
	 * @filename: The filename of the transfer.
	 *
	 * Query a file's properties locally.
	 *
	 * The default class handler will query a file using standard library
	 * functions, and set the transfer's size. If you connect to this signal,
	 * you should try to do the same, but it is not necessarily an error if you
	 * cannot. If you connect to this signal, you must connect to
	 * PurpleXfer::open-local, PurpleXfer::read-local, PurpleXfer::write-local
	 * and PurpleXfer::data-not-sent as well.
	 *
	 * Returns: %TRUE if the properties were queried successfully, or %FALSE
	 *          otherwise, and the transfer should be cancelled (libpurple will
	 *          cancel).
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_QUERY_LOCAL] = g_signal_new(
	        "query-local", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
	        G_STRUCT_OFFSET(PurpleXferClass, query_local),
	        g_signal_accumulator_first_wins, NULL, NULL, G_TYPE_BOOLEAN, 1,
	        G_TYPE_STRING);

	/**
	 * PurpleXfer::read-local:
	 * @buffer: (out): A pointer to a buffer to fill.
	 * @size: The maximum amount of data to put in the buffer.
	 *
	 * Read data locally to send to the protocol for a file transfer.
	 *
	 * The default class handler will read from a file using standard library
	 * functions. If you connect to this signal, you must connect to
	 * PurpleXfer::open-local, PurpleXfer::query-local, PurpleXfer::write-local
	 * and PurpleXfer::data-not-sent as well.
	 *
	 * Returns: The amount of data in the buffer, 0 if nothing is available,
	 *          and a negative value if an error occurred and the transfer
	 *          should be cancelled (libpurple will cancel).
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_READ_LOCAL] = g_signal_new(
	        "read-local", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
	        G_STRUCT_OFFSET(PurpleXferClass, read_local),
	        g_signal_accumulator_first_wins, NULL, NULL, G_TYPE_LONG, 2,
	        G_TYPE_POINTER, G_TYPE_LONG);

	/**
	 * PurpleXfer::write-local:
	 * @buffer: The buffer to write.
	 * @size: The size of the buffer.
	 *
	 * Write data received from the protocol locally. The signal handler must
	 * deal with the entire buffer and return size, or it is treated as an
	 * error.
	 *
	 * The default class handler will write to a file using standard library
	 * functions. If you connect to this signal, you must connect to
	 * PurpleXfer::open-local, PurpleXfer::query-local, PurpleXfer::read-local
	 * and PurpleXfer::data-not-sent as well.
	 *
	 * Returns: @size if the write was successful, or a value between 0 and
	 *          @size on error.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_WRITE_LOCAL] = g_signal_new(
	        "write-local", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
	        G_STRUCT_OFFSET(PurpleXferClass, write_local),
	        g_signal_accumulator_first_wins, NULL, NULL, G_TYPE_LONG, 2,
	        G_TYPE_POINTER, G_TYPE_LONG);

	/**
	 * PurpleXfer::data-not-sent:
	 * @buffer: A pointer to the beginning of the unwritten data.
	 * @size: The amount of unwritten data.
	 *
	 * Notify the UI that not all the data read in was written. The UI should
	 * re-enqueue this data and return it the next time read is called.
	 *
	 * If you connect to this signal, you must connect to
	 * PurpleXfer::open-local, PurpleXfer::query-local, PurpleXfer::read-local
	 * and PurpleXfer::write-local as well.
	 *
	 * Returns: %TRUE if the data was re-enqueued successfully, or %FALSE
	 *          otherwise, and the transfer should be cancelled (libpurple
	 *          will cancel).
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_DATA_NOT_SENT] = g_signal_new(
	        "data-not-sent", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
	        G_STRUCT_OFFSET(PurpleXferClass, data_not_sent),
	        g_signal_accumulator_first_wins, NULL, NULL, G_TYPE_BOOLEAN, 2,
	        G_TYPE_POINTER, G_TYPE_ULONG);

	/**
	 * PurpleXfer::add-thumbnail:
	 * @formats: A comma-separated string of allowed image formats.
	 *
	 * Request that a thumbnail be added to a file transfer.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_ADD_THUMBNAIL] = g_signal_new(
	        "add-thumbnail", G_TYPE_FROM_CLASS(klass), G_SIGNAL_ACTION, 0, NULL,
	        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}

PurpleXfer *
purple_xfer_new(PurpleAccount *account, PurpleXferType type, const char *who)
{
	PurpleXfer *xfer;
	PurpleProtocol *protocol;

	g_return_val_if_fail(type != PURPLE_XFER_TYPE_UNKNOWN, NULL);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(who != NULL, NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(account));
	if (PURPLE_IS_PROTOCOL_XFER(protocol)) {
		PurpleConnection *connection = purple_account_get_connection(account);

		xfer = purple_protocol_xfer_new_xfer(
			PURPLE_PROTOCOL_XFER(protocol),
			connection,
			/* TODO: this should support the type */
			who
		);
	} else {
		xfer = g_object_new(PURPLE_TYPE_XFER,
			"account",     account,
			"type",        type,
			"remote-user", who,
			NULL
		);
	}

	return xfer;
}

/**************************************************************************
 * File Transfer Subsystem API
 **************************************************************************/
GList *
purple_xfers_get_all()
{
	return xfers;
}

void *
purple_xfers_get_handle(void) {
	static int handle = 0;

	return &handle;
}

void
purple_xfers_init(void) {
	void *handle = purple_xfers_get_handle();

	/* register signals */
	purple_signal_register(handle, "file-recv-request",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_XFER);
}

void
purple_xfers_uninit(void)
{
	void *handle = purple_xfers_get_handle();

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);
}

void
purple_xfers_set_ui_ops(PurpleXferUiOps *ops) {
	xfer_ui_ops = ops;
}

PurpleXferUiOps *
purple_xfers_get_ui_ops(void) {
	return xfer_ui_ops;
}

/**************************************************************************
 * GBoxed code
 **************************************************************************/
static PurpleXferUiOps *
purple_xfer_ui_ops_copy(PurpleXferUiOps *ops)
{
	PurpleXferUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleXferUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_xfer_ui_ops_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleXferUiOps",
				(GBoxedCopyFunc)purple_xfer_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

/**************************************************************************
 * PurpleXferProtocolInterface
 **************************************************************************/
G_DEFINE_INTERFACE(PurpleProtocolXfer, purple_protocol_xfer, G_TYPE_INVALID);

static void
purple_protocol_xfer_default_init(PurpleProtocolXferInterface *face) {
}

gboolean
purple_protocol_xfer_can_receive(PurpleProtocolXfer *prplxfer,
                                 PurpleConnection *connection,
                                 const gchar *who
) {
	PurpleProtocolXferInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_XFER(prplxfer), FALSE);
	g_return_val_if_fail(PURPLE_IS_CONNECTION(connection), FALSE);
	g_return_val_if_fail(who, FALSE);

	iface = PURPLE_PROTOCOL_XFER_GET_IFACE(prplxfer);
	if(iface &&  iface->can_receive)
		return iface->can_receive(prplxfer, connection, who);

	/* If the PurpleProtocolXfer doesn't implement this function, we assume
	 * there are no conditions where we can't send a file to the given user.
	 */
	return TRUE;
}

void
purple_protocol_xfer_send_file(PurpleProtocolXfer *prplxfer,
                               PurpleConnection *connection,
                               const gchar *who,
                               const gchar *filename
) {
	PurpleProtocolXferInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_PROTOCOL_XFER(prplxfer));
	g_return_if_fail(PURPLE_IS_CONNECTION(connection));
	g_return_if_fail(who);

	iface = PURPLE_PROTOCOL_XFER_GET_IFACE(prplxfer);
	if(iface && iface->send_file)
		iface->send_file(prplxfer, connection, who, filename);
}

PurpleXfer *
purple_protocol_xfer_new_xfer(PurpleProtocolXfer *prplxfer,
                              PurpleConnection *connection,
                              const gchar *who
) {
	PurpleProtocolXferInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_XFER(prplxfer), FALSE);
	g_return_val_if_fail(PURPLE_IS_CONNECTION(connection), FALSE);
	g_return_val_if_fail(who, FALSE);

	iface = PURPLE_PROTOCOL_XFER_GET_IFACE(prplxfer);
	if(iface && iface->new_xfer)
		return iface->new_xfer(prplxfer, connection, who);

	return NULL;
}
