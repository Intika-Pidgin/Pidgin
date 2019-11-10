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

#include "internal.h"
#include "queuedoutputstream.h"

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
struct _PurpleQueuedOutputStream
{
	GFilterOutputStream parent;
};

typedef struct
{
	GAsyncQueue *queue;
	gboolean pending_queued;
} PurpleQueuedOutputStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(PurpleQueuedOutputStream,
		purple_queued_output_stream, G_TYPE_FILTER_OUTPUT_STREAM)

/******************************************************************************
 * Helpers
 *****************************************************************************/

static void purple_queued_output_stream_start_push_bytes_async(GTask *task);

static void
purple_queued_output_stream_push_bytes_async_cb(GObject *source,
		GAsyncResult *res, gpointer user_data)
{
	GTask *task = G_TASK(user_data);
	PurpleQueuedOutputStream *stream = g_task_get_source_object(task);
	PurpleQueuedOutputStreamPrivate *priv = purple_queued_output_stream_get_instance_private(stream);
	gssize written;
	GBytes *bytes;
	gsize size;
	GError *error = NULL;

	written = g_output_stream_write_bytes_finish(G_OUTPUT_STREAM(source),
			res, &error);

	bytes = g_task_get_task_data(task);
	size = g_bytes_get_size(bytes);

	if (written < 0) {
		/* Error occurred, return error */
		g_task_return_error(task, error);
		g_clear_object(&task);
	} else if (size > written) {
		/* Partial write, prepare to send remaining data */
		bytes = g_bytes_new_from_bytes(bytes, written, size - written);
		g_task_set_task_data(task, bytes,
				(GDestroyNotify)g_bytes_unref);
	} else {
		/* Full write, this task is finished */
		g_task_return_boolean(task, TRUE);
		g_clear_object(&task);
	}

	/* If g_task_return_* was called in this function, the callback
	 * may have cleared the stream. If so, there will be no remaining
	 * tasks to process here.
	 */

	if (task == NULL) {
		/* Any queued data left? */
		task = g_async_queue_try_pop(priv->queue);
	}

	if (task != NULL) {
		/* More to process */
		purple_queued_output_stream_start_push_bytes_async(task);
	} else {
		/* All done */
		priv->pending_queued = FALSE;
		g_output_stream_clear_pending(G_OUTPUT_STREAM(stream));
	}
}

static void
purple_queued_output_stream_start_push_bytes_async(GTask *task)
{
	PurpleQueuedOutputStream *stream = g_task_get_source_object(task);
	GOutputStream *base_stream;

	base_stream = g_filter_output_stream_get_base_stream(
			G_FILTER_OUTPUT_STREAM(stream));

	g_output_stream_write_bytes_async(base_stream,
			g_task_get_task_data(task),
			g_task_get_priority(task),
			g_task_get_cancellable(task),
			purple_queued_output_stream_push_bytes_async_cb,
			task);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/

static void
purple_queued_output_stream_dispose(GObject *object)
{
	PurpleQueuedOutputStream *stream = PURPLE_QUEUED_OUTPUT_STREAM(object);
	PurpleQueuedOutputStreamPrivate *priv = purple_queued_output_stream_get_instance_private(stream);

	g_clear_pointer(&priv->queue, g_async_queue_unref);

	G_OBJECT_CLASS(purple_queued_output_stream_parent_class)->dispose(object);
}

static void
purple_queued_output_stream_class_init(PurpleQueuedOutputStreamClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->dispose = purple_queued_output_stream_dispose;
}

static void
purple_queued_output_stream_init(PurpleQueuedOutputStream *stream)
{
	PurpleQueuedOutputStreamPrivate *priv = purple_queued_output_stream_get_instance_private(stream);
	priv->queue = g_async_queue_new_full((GDestroyNotify)g_bytes_unref);
	priv->pending_queued = FALSE;
}

/******************************************************************************
 * Public API
 *****************************************************************************/

PurpleQueuedOutputStream *
purple_queued_output_stream_new(GOutputStream *base_stream)
{
	PurpleQueuedOutputStream *stream;

	g_return_val_if_fail(G_IS_OUTPUT_STREAM(base_stream), NULL);

	stream = g_object_new(PURPLE_TYPE_QUEUED_OUTPUT_STREAM,
			"base-stream", base_stream,
			NULL);

	return stream;
}

void
purple_queued_output_stream_push_bytes_async(PurpleQueuedOutputStream *stream,
		GBytes *bytes, int io_priority, GCancellable *cancellable,
		GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;
	gboolean set_pending;
	GError *error = NULL;
	PurpleQueuedOutputStreamPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_QUEUED_OUTPUT_STREAM(stream));
	g_return_if_fail(bytes != NULL);

	priv = purple_queued_output_stream_get_instance_private(stream);

	task = g_task_new(stream, cancellable, callback, user_data);
	g_task_set_task_data(task, g_bytes_ref(bytes),
			(GDestroyNotify)g_bytes_unref);
	g_task_set_source_tag(task,
			purple_queued_output_stream_push_bytes_async);
	g_task_set_priority(task, io_priority);

	set_pending = g_output_stream_set_pending(
			G_OUTPUT_STREAM(stream), &error);

	/* Since we're allowing queuing requests without blocking,
	 * it's not an error to be pending while processing queued operations.
	 */
	if (!set_pending && (!g_error_matches(error,
			G_IO_ERROR, G_IO_ERROR_PENDING) ||
			!priv->pending_queued)) {
		g_task_return_error(task, error);
		g_object_unref(task);
		return;
	}

	g_clear_error (&error);
	priv->pending_queued = TRUE;

	if (set_pending) {
		/* Start processing if there were no pending operations */
		purple_queued_output_stream_start_push_bytes_async(task);
	} else {
		/* Otherwise queue the data */
		g_async_queue_push(priv->queue, task);
	}
}

gboolean
purple_queued_output_stream_push_bytes_finish(PurpleQueuedOutputStream *stream,
		GAsyncResult *result, GError **error)
{
	g_return_val_if_fail(PURPLE_IS_QUEUED_OUTPUT_STREAM(stream), FALSE);
	g_return_val_if_fail(g_task_is_valid(result, stream), FALSE);
	g_return_val_if_fail(g_async_result_is_tagged(result,
			purple_queued_output_stream_push_bytes_async), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

void
purple_queued_output_stream_clear_queue(PurpleQueuedOutputStream *stream)
{
	GTask *task;
	PurpleQueuedOutputStreamPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_QUEUED_OUTPUT_STREAM(stream));

	priv = purple_queued_output_stream_get_instance_private(stream);

	while ((task = g_async_queue_try_pop(priv->queue)) != NULL) {
		g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
				"PurpleQueuedOutputStream queue cleared");
		g_object_unref(task);
	}
}
