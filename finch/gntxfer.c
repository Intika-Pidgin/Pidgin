/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <internal.h>
#include "finch.h"

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntlabel.h>
#include <gnttree.h>

#include "debug.h"
#include "notify.h"
#include "xfer.h"
#include "protocol.h"
#include "util.h"

#include "gntxfer.h"
#include "prefs.h"

typedef struct
{
	gboolean keep_open;
	gboolean auto_clear;
	gint num_transfers;

	GntWidget *window;
	GntWidget *tree;

	GntWidget *remove_button;
	GntWidget *stop_button;
	GntWidget *close_button;
} PurpleGntXferDialog;

static PurpleGntXferDialog *xfer_dialog = NULL;

typedef struct
{
	time_t last_updated_time;
	gboolean in_list;

	char *name;
	gboolean notified;   /* Has the completion of the transfer been notified? */

} PurpleGntXferUiData;

enum
{
	COLUMN_PROGRESS = 0,
	COLUMN_FILENAME,
	COLUMN_SIZE,
	COLUMN_SPEED,
	COLUMN_REMAINING,
	COLUMN_STATUS,
	NUM_COLUMNS
};


/**************************************************************************
 * Utility Functions
 **************************************************************************/

static void
update_title_progress(void)
{
	GList *list;
	int num_active_xfers = 0;
	guint64 total_bytes_xferred = 0;
	guint64 total_file_size = 0;

	if (xfer_dialog == NULL || xfer_dialog->window == NULL)
		return;

	/* Find all active transfers */
	for (list = gnt_tree_get_rows(GNT_TREE(xfer_dialog->tree)); list; list = list->next) {
		PurpleXfer *xfer = (PurpleXfer *)list->data;

		if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_STARTED) {
			num_active_xfers++;
			total_bytes_xferred += purple_xfer_get_bytes_sent(xfer);
			total_file_size += purple_xfer_get_size(xfer);
		}
	}

	/* Update the title */
	if (num_active_xfers > 0) {
		gchar *title;
		int total_pct = 0;

		if (total_file_size > 0) {
			total_pct = 100 * total_bytes_xferred / total_file_size;
		}

		title = g_strdup_printf(ngettext("File Transfers - %d%% of %d file",
						 "File Transfers - %d%% of %d files",
						 num_active_xfers),
				total_pct, num_active_xfers);
		gnt_screen_rename_widget((xfer_dialog->window), title);
		g_free(title);
	} else {
		gnt_screen_rename_widget((xfer_dialog->window), _("File Transfers"));
	}
}


/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
toggle_keep_open_cb(GntWidget *w)
{
	xfer_dialog->keep_open = !xfer_dialog->keep_open;
	purple_prefs_set_bool("/finch/filetransfer/keep_open",
						xfer_dialog->keep_open);
}

static void
toggle_clear_finished_cb(GntWidget *w)
{
	xfer_dialog->auto_clear = !xfer_dialog->auto_clear;
	purple_prefs_set_bool("/finch/filetransfer/clear_finished",
						xfer_dialog->auto_clear);
	if (xfer_dialog->auto_clear) {
		GList *iter = purple_xfers_get_all();
		while (iter) {
			PurpleXfer *xfer = iter->data;
			iter = iter->next;
			if (purple_xfer_is_completed(xfer) || purple_xfer_is_cancelled(xfer))
			finch_xfer_dialog_remove_xfer(xfer);
		}
	}
}

static void
remove_button_cb(GntButton *button)
{
	PurpleXfer *selected_xfer = gnt_tree_get_selection_data(GNT_TREE(xfer_dialog->tree));
	if (selected_xfer && (purple_xfer_is_completed(selected_xfer) ||
				purple_xfer_is_cancelled(selected_xfer))) {
		finch_xfer_dialog_remove_xfer(selected_xfer);
	}
}

static void
stop_button_cb(GntButton *button)
{
	PurpleXfer *selected_xfer = gnt_tree_get_selection_data(GNT_TREE(xfer_dialog->tree));
	PurpleXferStatus status;

	if (!selected_xfer)
		return;

	status = purple_xfer_get_status(selected_xfer);
	if (status != PURPLE_XFER_STATUS_CANCEL_LOCAL &&
			status != PURPLE_XFER_STATUS_CANCEL_REMOTE &&
			status != PURPLE_XFER_STATUS_DONE)
		purple_xfer_cancel_local(selected_xfer);
}

/**************************************************************************
 * Dialog Building Functions
 **************************************************************************/


void
finch_xfer_dialog_new(void)
{
	GList *iter;
	GntWidget *window;
	GntWidget *bbox;
	GntWidget *button;
	GntWidget *checkbox;
	GntWidget *tree;
	int widths[] = {8, 12, 8, 8, 8, 8, -1};

	if (!xfer_dialog)
		xfer_dialog = g_new0(PurpleGntXferDialog, 1);

	xfer_dialog->keep_open =
		purple_prefs_get_bool("/finch/filetransfer/keep_open");
	xfer_dialog->auto_clear =
		purple_prefs_get_bool("/finch/filetransfer/clear_finished");

	/* Create the window. */
	xfer_dialog->window = window = gnt_vbox_new(FALSE);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(finch_xfer_dialog_destroy), NULL);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), _("File Transfers"));
	gnt_box_set_fill(GNT_BOX(window), TRUE);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	xfer_dialog->tree = tree = gnt_tree_new_with_columns(NUM_COLUMNS);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Progress"), _("Filename"), _("Size"), _("Speed"), _("Remaining"), _("Status"));
	gnt_tree_set_column_width_ratio(GNT_TREE(tree), widths);
	gnt_tree_set_column_resizable(GNT_TREE(tree), COLUMN_PROGRESS, FALSE);
	gnt_tree_set_column_resizable(GNT_TREE(tree), COLUMN_SIZE, FALSE);
	gnt_tree_set_column_resizable(GNT_TREE(tree), COLUMN_SPEED, FALSE);
	gnt_tree_set_column_resizable(GNT_TREE(tree), COLUMN_REMAINING, FALSE);
	gnt_widget_set_size(tree, 70, -1);
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_box_add_widget(GNT_BOX(window), tree);

	checkbox = gnt_check_box_new( _("Close this window when all transfers finish"));
	gnt_check_box_set_checked(GNT_CHECK_BOX(checkbox),
								 !xfer_dialog->keep_open);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_keep_open_cb), NULL);
	gnt_box_add_widget(GNT_BOX(window), checkbox);

	checkbox = gnt_check_box_new(_("Clear finished transfers"));
	gnt_check_box_set_checked(GNT_CHECK_BOX(checkbox),
								 xfer_dialog->auto_clear);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_clear_finished_cb), NULL);
	gnt_box_add_widget(GNT_BOX(window), checkbox);

	bbox = gnt_hbox_new(FALSE);

	xfer_dialog->remove_button = button = gnt_button_new(_("Remove"));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(remove_button_cb), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	xfer_dialog->stop_button = button = gnt_button_new(_("Stop"));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(stop_button_cb), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	xfer_dialog->close_button = button = gnt_button_new(_("Close"));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(finch_xfer_dialog_destroy), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	gnt_box_add_widget(GNT_BOX(window), bbox);

	for (iter = purple_xfers_get_all(); iter; iter = iter->next) {
		PurpleXfer *xfer = (PurpleXfer *)iter->data;
		PurpleGntXferUiData *data = purple_xfer_get_ui_data(xfer);
		if (data->in_list) {
			finch_xfer_dialog_add_xfer(xfer);
			finch_xfer_dialog_update_xfer(xfer);
			gnt_tree_set_selected(GNT_TREE(tree), xfer);
		}
	}
	gnt_widget_show(xfer_dialog->window);
}

void
finch_xfer_dialog_destroy()
{
	gnt_widget_destroy(xfer_dialog->window);
	g_free(xfer_dialog);
	xfer_dialog = NULL;
}

void
finch_xfer_dialog_show()
{
	if (xfer_dialog == NULL)
		finch_xfer_dialog_new();
	else
		gnt_window_present(xfer_dialog->window);
}

void
finch_xfer_dialog_add_xfer(PurpleXfer *xfer)
{
	PurpleGntXferUiData *data;
	PurpleXferType type;
	char *size_str, *remaining_str;
	char *lfilename, *utf8;

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	g_object_ref(xfer);

	data = purple_xfer_get_ui_data(xfer);
	data->in_list = TRUE;

	finch_xfer_dialog_show();

	data->last_updated_time = 0;

	type = purple_xfer_get_xfer_type(xfer);

	size_str      = purple_str_size_to_units(purple_xfer_get_size(xfer));
	remaining_str = purple_str_size_to_units(purple_xfer_get_bytes_remaining(xfer));

	lfilename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
	utf8 = g_filename_to_utf8(lfilename, -1, NULL, NULL, NULL);
	g_free(lfilename);
	lfilename = utf8;
	gnt_tree_add_row_last(GNT_TREE(xfer_dialog->tree), xfer,
		gnt_tree_create_row(GNT_TREE(xfer_dialog->tree),
			"0.0", (type == PURPLE_XFER_TYPE_RECEIVE) ? purple_xfer_get_filename(xfer) : lfilename,
			size_str, "0.0", "",_("Waiting for transfer to begin")), NULL);
	g_free(lfilename);

	g_free(size_str);
	g_free(remaining_str);

	xfer_dialog->num_transfers++;

	update_title_progress();
}

void
finch_xfer_dialog_remove_xfer(PurpleXfer *xfer)
{
	PurpleGntXferUiData *data;

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = purple_xfer_get_ui_data(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	data->in_list = FALSE;

	gnt_tree_remove(GNT_TREE(xfer_dialog->tree), xfer);

	xfer_dialog->num_transfers--;

	if (xfer_dialog->num_transfers == 0 && !xfer_dialog->keep_open)
		finch_xfer_dialog_destroy();
	else
		update_title_progress();
	g_object_unref(xfer);
}

void
finch_xfer_dialog_cancel_xfer(PurpleXfer *xfer)
{
	PurpleGntXferUiData *data;
	const gchar *status;

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = purple_xfer_get_ui_data(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	if ((purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) && (xfer_dialog->auto_clear)) {
		finch_xfer_dialog_remove_xfer(xfer);
		return;
	}

	update_title_progress();

	if (purple_xfer_is_cancelled(xfer))
		status = _("Cancelled");
	else
		status = _("Failed");

	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_STATUS, status);
}

void
finch_xfer_dialog_update_xfer(PurpleXfer *xfer)
{
	PurpleGntXferUiData *data;
	char *size_str, *remaining_str;
	time_t current_time;
	char prog_str[5];
	double kb_sent;
	double kbps = 0.0;
	time_t elapsed, now;
	char *kbsec;
	gboolean send;

	if ((now = purple_xfer_get_end_time(xfer)) == 0)
		now = time(NULL);

	kb_sent = purple_xfer_get_bytes_sent(xfer) / 1024.0;
	elapsed = (purple_xfer_get_start_time(xfer) > 0 ? now - purple_xfer_get_start_time(xfer) : 0);
	kbps    = (elapsed > 0 ? (kb_sent / elapsed) : 0);

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	if ((data = purple_xfer_get_ui_data(xfer)) == NULL)
		return;

	if (data->in_list == FALSE || data->notified)
		return;

	current_time = time(NULL);
	if (((current_time - data->last_updated_time) == 0) &&
		(!purple_xfer_is_completed(xfer))) {
		/* Don't update the window more than once per second */
		return;
	}
	data->last_updated_time = current_time;

	send = (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_SEND);
	size_str      = purple_str_size_to_units(purple_xfer_get_size(xfer));
	remaining_str = purple_str_size_to_units(purple_xfer_get_bytes_remaining(xfer));
	kbsec = g_strdup_printf(_("%.2f KiB/s"), kbps);

	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_PROGRESS,
			g_ascii_dtostr(prog_str, sizeof(prog_str), purple_xfer_get_progress(xfer) * 100.));
	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_SIZE, size_str);
	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_REMAINING, remaining_str);
	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_SPEED, kbsec);
	g_free(size_str);
	g_free(remaining_str);
	g_free(kbsec);
	if (purple_xfer_is_completed(xfer)) {
		gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_STATUS, send ? _("Sent") : _("Received"));
		gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_REMAINING, _("Finished"));
		if (!send) {
			char *msg = g_strdup_printf(_("The file was saved as %s."), purple_xfer_get_local_filename(xfer));
			purple_xfer_conversation_write(xfer, msg, FALSE);
			g_free(msg);
		}
		data->notified = TRUE;
	} else {
		gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_STATUS,
				send ? _("Sending") : _("Receiving"));
	}

	update_title_progress();

	if (purple_xfer_is_completed(xfer) && xfer_dialog->auto_clear)
		finch_xfer_dialog_remove_xfer(xfer);
}

/**************************************************************************
 * File Transfer UI Ops
 **************************************************************************/
static void
finch_xfer_new_xfer(PurpleXfer *xfer)
{
	PurpleGntXferUiData *data;

	/* This is where we're setting xfer's "ui_data" for the first time. */
	data = g_new0(PurpleGntXferUiData, 1);
	purple_xfer_set_ui_data(xfer, data);
}

static void
finch_xfer_destroy(PurpleXfer *xfer)
{
	PurpleGntXferUiData *data;

	data = purple_xfer_get_ui_data(xfer);
	if (data) {
		g_free(data->name);
		g_free(data);
		purple_xfer_set_ui_data(xfer, NULL);
	}
}

static void
finch_xfer_add_xfer(PurpleXfer *xfer)
{
	if (!xfer_dialog)
		finch_xfer_dialog_new();

	finch_xfer_dialog_add_xfer(xfer);
	gnt_tree_set_selected(GNT_TREE(xfer_dialog->tree), xfer);
}

static void
finch_xfer_update_progress(PurpleXfer *xfer, double percent)
{
	if (xfer_dialog)
		finch_xfer_dialog_update_xfer(xfer);
}

static void
finch_xfer_cancel_local(PurpleXfer *xfer)
{
	if (xfer_dialog)
		finch_xfer_dialog_cancel_xfer(xfer);
}

static void
finch_xfer_cancel_remote(PurpleXfer *xfer)
{
	if (xfer_dialog)
		finch_xfer_dialog_cancel_xfer(xfer);
}

static PurpleXferUiOps ops =
{
	finch_xfer_new_xfer,
	finch_xfer_destroy,
	finch_xfer_add_xfer,
	finch_xfer_update_progress,
	finch_xfer_cancel_local,
	finch_xfer_cancel_remote,
	NULL, /* ui_write */
	NULL, /* ui_read */
	NULL, /* data_not_sent */
	NULL  /* add_thumbnail */
};

/**************************************************************************
 * GNT File Transfer API
 **************************************************************************/
void
finch_xfers_init(void)
{
	purple_prefs_add_none("/finch/filetransfer");
	purple_prefs_add_bool("/finch/filetransfer/clear_finished", TRUE);
	purple_prefs_add_bool("/finch/filetransfer/keep_open", FALSE);
}

void
finch_xfers_uninit(void)
{
	if (xfer_dialog != NULL)
		finch_xfer_dialog_destroy();
}

PurpleXferUiOps *
finch_xfers_get_ui_ops(void)
{
	return &ops;
}
