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

#ifndef _PURPLE_QUEUED_OUTPUT_STREAM_H
#define _PURPLE_QUEUED_OUTPUT_STREAM_H
/**
 * SECTION:queuedoutputstream
 * @section_id: libpurple-queuedoutputstream
 * @short_description: GOutputStream for queuing data to output
 * @title: GOutputStream class
 *
 * A #PurpleQueuedOutputStream is a #GOutputStream which allows data to be
 * queued for outputting. It differs from a #GBufferedOutputStream in that
 * it allows for data to be queued while other operations are in progress.
 */

#include <gio/gio.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_QUEUED_OUTPUT_STREAM		(purple_queued_output_stream_get_type())
#define PURPLE_QUEUED_OUTPUT_STREAM(o)			(G_TYPE_CHECK_INSTANCE_CAST((o), PURPLE_TYPE_QUEUED_OUTPUT_STREAM, PurpleQueuedOutputStream))
#define PURPLE_QUEUED_OUTPUT_STREAM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), PURPLE_TYPE_QUEUED_OUTPUT_STREAM, PurpleQueuedOutputStreamClass))
#define PURPLE_IS_QUEUED_OUTPUT_STREAM(o)		(G_TYPE_CHECK_INSTANCE_TYPE((o), PURPLE_TYPE_QUEUED_OUTPUT_STREAM))
#define PURPLE_IS_QUEUED_OUTPUT_STREAM_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE((k), PURPLE_TYPE_QUEUED_OUTPUT_STREAM))
#define PURPLE_IS_QUEUED_OUTPUT_STREAM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS((o), PURPLE_TYPE_QUEUED_OUTPUT_STREAM, PurpleQueuedOutputStreamClass))

/**
 * PurpleQueuedOutputStream:
 *
 * An implementation of #GFilterOutputStream which allows queuing data for
 * output. This allows data to be queued while other data is being output.
 * Therefore, data doesn't have to be manually stored while waiting for
 * stream operations to finish.
 *
 * To create a queued output stream, use #purple_queued_output_stream_new().
 *
 * To queue data, use #purple_queued_output_stream_push_bytes_async().
 *
 * If there's a fatal stream error, it's suggested to clear the remaining
 * bytes queued with #purple_queued_output_stream_clear_queue() to avoid
 * excessive errors returned in
 * #purple_queued_output_stream_push_bytes_async()'s async callback.
 */
typedef struct _PurpleQueuedOutputStream	PurpleQueuedOutputStream;
typedef struct _PurpleQueuedOutputStreamClass	PurpleQueuedOutputStreamClass;
typedef struct _PurpleQueuedOutputStreamPrivate	PurpleQueuedOutputStreamPrivate;

struct _PurpleQueuedOutputStream
{
	GFilterOutputStream parent_instance;

	/*< protected >*/
	PurpleQueuedOutputStreamPrivate *priv;
};

struct _PurpleQueuedOutputStreamClass
{
	GFilterOutputStreamClass parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_g_reserved1)(void);
	void (*_g_reserved2)(void);
};

GType purple_queued_output_stream_get_type(void) G_GNUC_CONST;

/*
 * purple_queued_output_stream_new
 * @base_stream: Base output stream to wrap with the queued stream
 *
 * Creates a new queued output stream for a base stream.
 */
PurpleQueuedOutputStream *purple_queued_output_stream_new(
		GOutputStream *base_stream);

/*
 * purple_queued_output_stream_push_bytes_async
 * @stream: #PurpleQueuedOutputStream to push bytes to
 * @bytes: Bytes to queue
 * @priority: IO priority of the request
 * @cancellable: (allow-none): Optional #GCancellable object, NULL to ignore
 * @callback: (scope async): Callback to call when the request is finished
 * @user_data: (closure): Data to pass to the callback function
 *
 * Asynchronously queues and then writes data to the output stream.
 * Once the data has been written, or an error occurs, the callback
 * will be called.
 *
 * Be careful such that if there's a fatal stream error, all remaining queued
 * operations will likely return this error. Use
 * #purple_queued_output_stream_clear_queue() to clear the queue on such
 * an error to only report it a single time.
 */
void purple_queued_output_stream_push_bytes_async(
		PurpleQueuedOutputStream *stream, GBytes *bytes,
		int io_priority, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data);

/*
 * purple_queued_output_stream_push_bytes_finish
 * @stream: #PurpleQueuedOutputStream bytes were pushed to
 * @result: The #GAsyncResult of this operation
 * @error: A GError location to store the error, or NULL to ignore
 *
 * Finishes pushing bytes asynchronously.
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 */
gboolean purple_queued_output_stream_push_bytes_finish(
		PurpleQueuedOutputStream *stream,
		GAsyncResult *result, GError **error);

/*
 * purple_queued_output_stream_clear_queue
 * @stream: #PurpleQueuedOutputStream to clear
 *
 * Clears the queue of any pending bytes. However, any bytes that are
 * in the process of being sent will finish their operation.
 *
 * This function is useful for clearing the queue in case of an IO error.
 * Call this in the async callback in order to clear the queue and avoid
 * having all #purple_queue_output_stream_push_bytes_async() calls on
 * this queue return errors if there's a fatal stream error.
 */
void purple_queued_output_stream_clear_queue(PurpleQueuedOutputStream *stream);

G_END_DECLS

#endif /* _PURPLE_QUEUED_OUTPUT_STREAM_H */
