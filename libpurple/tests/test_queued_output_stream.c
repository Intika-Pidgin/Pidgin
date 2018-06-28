/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include <glib.h>
#include <string.h>

#include <queuedoutputstream.h>

static const gsize test_bytes_data_len = 5;
static const guint8 test_bytes_data[] = "12345";

static const gsize test_bytes_data_len2 = 4;
static const guint8 test_bytes_data2[] = "6789";

static const gsize test_bytes_data_len3 = 12;
static const guint8 test_bytes_data3[] = "101112131415";

static void
test_queued_output_stream_new(void) {
	GOutputStream *output;
	PurpleQueuedOutputStream *queued;
	GError *err = NULL;

	output = g_memory_output_stream_new_resizable();
	g_assert_nonnull(output);

	queued = purple_queued_output_stream_new(output);
	g_assert_true(PURPLE_IS_QUEUED_OUTPUT_STREAM(queued));

	g_assert_true(g_output_stream_close(
			G_OUTPUT_STREAM(queued), NULL, &err));
	g_assert_no_error(err);

	g_clear_object(&queued);
	g_clear_object(&output);
}

static void
test_queued_output_stream_push_bytes_async_cb(GObject *source,
		GAsyncResult *res, gpointer user_data)
{
	PurpleQueuedOutputStream *queued = PURPLE_QUEUED_OUTPUT_STREAM(source);
	gboolean *done = user_data;
	GError *err = NULL;

	g_assert_true(purple_queued_output_stream_push_bytes_finish(queued,
			res, &err));
	g_assert_no_error(err);

	*done = TRUE;
}

static void
test_queued_output_stream_push_bytes_async(void) {
	GMemoryOutputStream *output;
	PurpleQueuedOutputStream *queued;
	GBytes *bytes;
	GError *err = NULL;
	gboolean done = FALSE;

	output = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
	g_assert_nonnull(output);

	queued = purple_queued_output_stream_new(G_OUTPUT_STREAM(output));
	g_assert_true(PURPLE_IS_QUEUED_OUTPUT_STREAM(queued));

	bytes = g_bytes_new_static(test_bytes_data, test_bytes_data_len);
	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, NULL,
			test_queued_output_stream_push_bytes_async_cb, &done);
	g_bytes_unref(bytes);

	while (!done) {
		g_main_context_iteration(NULL, TRUE);
	}

	g_assert_cmpmem(g_memory_output_stream_get_data(output),
			g_memory_output_stream_get_data_size(output),
			test_bytes_data, test_bytes_data_len);

	g_assert_true(g_output_stream_close(
			G_OUTPUT_STREAM(queued), NULL, &err));
	g_assert_no_error(err);

	g_clear_object(&queued);
	g_clear_object(&output);
}

static void
test_queued_output_stream_push_bytes_async_multiple_cb(GObject *source,
		GAsyncResult *res, gpointer user_data)
{
	PurpleQueuedOutputStream *queued = PURPLE_QUEUED_OUTPUT_STREAM(source);
	gint *done = user_data;
	GError *err = NULL;

	g_assert_true(purple_queued_output_stream_push_bytes_finish(queued,
			res, &err));
	g_assert_no_error(err);

	--*done;
}

static void
test_queued_output_stream_push_bytes_async_multiple(void) {
	GMemoryOutputStream *output;
	PurpleQueuedOutputStream *queued;
	GBytes *bytes;
	gchar *all_test_bytes_data;
	GError *err = NULL;
	int done = 3;

	output = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
	g_assert_nonnull(output);

	queued = purple_queued_output_stream_new(G_OUTPUT_STREAM(output));
	g_assert_true(PURPLE_IS_QUEUED_OUTPUT_STREAM(queued));

	bytes = g_bytes_new_static(test_bytes_data, test_bytes_data_len);
	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, NULL,
			test_queued_output_stream_push_bytes_async_multiple_cb,
			&done);
	g_bytes_unref(bytes);

	bytes = g_bytes_new_static(test_bytes_data2, test_bytes_data_len2);
	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, NULL,
			test_queued_output_stream_push_bytes_async_multiple_cb,
			&done);
	g_bytes_unref(bytes);

	bytes = g_bytes_new_static(test_bytes_data3, test_bytes_data_len3);
	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, NULL,
			test_queued_output_stream_push_bytes_async_multiple_cb,
			&done);
	g_bytes_unref(bytes);

	while (done > 0) {
		g_main_context_iteration(NULL, TRUE);
	}

	g_assert_cmpint(done, ==, 0);

	all_test_bytes_data = g_strconcat((const gchar *)test_bytes_data,
			test_bytes_data2, test_bytes_data3, NULL);

	g_assert_cmpmem(g_memory_output_stream_get_data(output),
			g_memory_output_stream_get_data_size(output),
			all_test_bytes_data, strlen(all_test_bytes_data));

	g_free(all_test_bytes_data);

	g_assert_true(g_output_stream_close(
			G_OUTPUT_STREAM(queued), NULL, &err));
	g_assert_no_error(err);

	g_clear_object(&queued);
	g_clear_object(&output);
}

static void
test_queued_output_stream_push_bytes_async_error_cb(GObject *source,
		GAsyncResult *res, gpointer user_data)
{
	PurpleQueuedOutputStream *queued = PURPLE_QUEUED_OUTPUT_STREAM(source);
	gint *done = user_data;
	GError *err = NULL;

	g_assert_false(purple_queued_output_stream_push_bytes_finish(queued,
			res, &err));

	g_assert_error(err, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error(&err);

	--*done;
}

static void
test_queued_output_stream_push_bytes_async_error(void) {
	GMemoryOutputStream *output;
	PurpleQueuedOutputStream *queued;
	GBytes *bytes;
	GCancellable *cancellable;
	GError *err = NULL;
	gint done = 3;

	output = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
	g_assert_nonnull(output);

	queued = purple_queued_output_stream_new(G_OUTPUT_STREAM(output));
	g_assert_true(PURPLE_IS_QUEUED_OUTPUT_STREAM(queued));

	cancellable = g_cancellable_new();
	g_assert_nonnull(cancellable);

	g_cancellable_cancel(cancellable);
	g_assert_true(g_cancellable_is_cancelled(cancellable));

	bytes = g_bytes_new_static(test_bytes_data, test_bytes_data_len);

	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, cancellable,
			test_queued_output_stream_push_bytes_async_error_cb,
			&done);

	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, cancellable,
			test_queued_output_stream_push_bytes_async_error_cb,
			&done);

	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, cancellable,
			test_queued_output_stream_push_bytes_async_error_cb,
			&done);

	g_bytes_unref(bytes);

	while (done > 0) {
		g_main_context_iteration(NULL, TRUE);
	}

	g_assert_cmpint(done, ==, 0);

	g_assert_cmpmem(g_memory_output_stream_get_data(output),
			g_memory_output_stream_get_data_size(output),
			NULL, 0);

	g_assert_true(g_output_stream_close(
			G_OUTPUT_STREAM(queued), NULL, &err));
	g_assert_no_error(err);

	g_clear_object(&queued);
	g_clear_object(&output);
}

static void
test_queued_output_stream_clear_queue_on_error_cb(GObject *source,
		GAsyncResult *res, gpointer user_data)
{
	PurpleQueuedOutputStream *queued = PURPLE_QUEUED_OUTPUT_STREAM(source);
	gint *done = user_data;
	GError *err = NULL;

	g_assert_false(purple_queued_output_stream_push_bytes_finish(queued,
			res, &err));

	g_assert_error(err, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_clear_error(&err);

	purple_queued_output_stream_clear_queue(queued);

	--*done;
}

static void
test_queued_output_stream_clear_queue_on_error(void) {
	GMemoryOutputStream *output;
	PurpleQueuedOutputStream *queued;
	GBytes *bytes;
	GCancellable *cancellable;
	GError *err = NULL;
	gint done = 3;

	output = G_MEMORY_OUTPUT_STREAM(g_memory_output_stream_new_resizable());
	g_assert_nonnull(output);

	queued = purple_queued_output_stream_new(G_OUTPUT_STREAM(output));
	g_assert_true(PURPLE_IS_QUEUED_OUTPUT_STREAM(queued));

	cancellable = g_cancellable_new();
	g_assert_nonnull(cancellable);

	g_cancellable_cancel(cancellable);
	g_assert_true(g_cancellable_is_cancelled(cancellable));

	bytes = g_bytes_new_static(test_bytes_data, test_bytes_data_len);

	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, cancellable,
			test_queued_output_stream_clear_queue_on_error_cb,
			&done);

	/* Don't set cancellable on these */
	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, NULL,
			test_queued_output_stream_clear_queue_on_error_cb,
			&done);

	purple_queued_output_stream_push_bytes_async(queued, bytes,
			G_PRIORITY_DEFAULT, NULL,
			test_queued_output_stream_clear_queue_on_error_cb,
			&done);

	g_bytes_unref(bytes);

	while (done > 0) {
		g_main_context_iteration(NULL, TRUE);
	}

	g_assert_cmpint(done, ==, 0);

	g_assert_cmpmem(g_memory_output_stream_get_data(output),
			g_memory_output_stream_get_data_size(output),
			NULL, 0);

	g_assert_true(g_output_stream_close(
			G_OUTPUT_STREAM(queued), NULL, &err));
	g_assert_no_error(err);

	g_clear_object(&queued);
	g_clear_object(&output);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_set_nonfatal_assertions();

	g_test_add_func("/queued-output-stream/new",
			test_queued_output_stream_new);
	g_test_add_func("/queued-output-stream/push-bytes-async",
			test_queued_output_stream_push_bytes_async);
	g_test_add_func("/queued-output-stream/push-bytes-async-multiple",
			test_queued_output_stream_push_bytes_async_multiple);
	g_test_add_func("/queued-output-stream/push-bytes-async-error",
			test_queued_output_stream_push_bytes_async_error);
	g_test_add_func("/queued-output-stream/clear-queue-on-error",
			test_queued_output_stream_clear_queue_on_error);

	return g_test_run();
}
