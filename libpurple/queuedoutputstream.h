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

#ifndef PURPLE_QUEUED_OUTPUT_STREAM_H
#define PURPLE_QUEUED_OUTPUT_STREAM_H
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

#define PURPLE_TYPE_QUEUED_OUTPUT_STREAM  purple_queued_output_stream_get_type()

G_DECLARE_FINAL_TYPE(PurpleQueuedOutputStream,
		purple_queued_output_stream, PURPLE,
		QUEUED_OUTPUT_STREAM, GFilterOutputStream)

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

#endif /* PURPLE_QUEUED_OUTPUT_STREAM_H */
