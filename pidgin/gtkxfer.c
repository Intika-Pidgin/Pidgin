/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#include "pidgin.h"

#include "debug.h"
#include "notify.h"
#include "xfer.h"
#include "protocol.h"
#include "util.h"

#include "gtkxfer.h"
#include "prefs.h"
#include "pidginstock.h"
#include "gtkutils.h"

#include "gtk3compat.h"

#ifdef _WIN32
#  include <shellapi.h>
#endif

/* the maximum size of files we will try to make a thumbnail for */
#define PIDGIN_XFER_MAX_SIZE_IMAGE_THUMBNAIL 10 * 1024 * 1024

struct _PidginXferDialog
{
	gint box_count;

	gboolean keep_open;
	gboolean auto_clear;

	gint num_transfers;

	PurpleXfer *selected_xfer;

	GtkWidget *window;
	GtkWidget *tree;
	GtkListStore *model;

	GtkWidget *expander;

	GtkWidget *grid;

	GtkWidget *local_user_desc_label;
	GtkWidget *local_user_label;
	GtkWidget *remote_user_desc_label;
	GtkWidget *remote_user_label;
	GtkWidget *protocol_label;
	GtkWidget *filename_label;
	GtkWidget *localfile_label;
	GtkWidget *status_label;
	GtkWidget *speed_label;
	GtkWidget *time_elapsed_label;
	GtkWidget *time_remaining_label;

	GtkWidget *progress;

	/* Buttons */
	GtkWidget *open_button;
	GtkWidget *remove_button;
	GtkWidget *stop_button;
	GtkWidget *close_button;
};

typedef struct
{
	GtkTreeIter iter;
	time_t last_updated_time;
	gboolean in_list;

	char *name;

} PidginXferUiData;

static PidginXferDialog *xfer_dialog = NULL;

enum
{
	COLUMN_STATUS = 0,
	COLUMN_PROGRESS,
	COLUMN_FILENAME,
	COLUMN_SIZE,
	COLUMN_REMAINING,
	COLUMN_DATA,
	NUM_COLUMNS
};


/**************************************************************************
 * Utility Functions
 **************************************************************************/
static void
get_xfer_info_strings(PurpleXfer *xfer, char **kbsec, char **time_elapsed,
					  char **time_remaining)
{
	double kb_sent, kb_rem;
	double kbps = 0.0;
	time_t elapsed, now;

	now = purple_xfer_get_end_time(xfer);
	if (now == 0)
		now = time(NULL);

	kb_sent = purple_xfer_get_bytes_sent(xfer) / 1024.0;
	kb_rem  = purple_xfer_get_bytes_remaining(xfer) / 1024.0;
	elapsed = purple_xfer_get_start_time(xfer);
	if (elapsed > 0)
		elapsed = now - elapsed;
	else
		elapsed = 0;
	kbps    = (elapsed > 0 ? (kb_sent / elapsed) : 0);

	if (kbsec != NULL) {
		*kbsec = g_strdup_printf(_("%.2f KiB/s"), kbps);
	}

	if (time_elapsed != NULL)
	{
		int h, m, s;
		int secs_elapsed;

		if (purple_xfer_get_start_time(xfer) > 0)
		{
			secs_elapsed = now - purple_xfer_get_start_time(xfer);

			h = secs_elapsed / 3600;
			m = (secs_elapsed % 3600) / 60;
			s = secs_elapsed % 60;

			*time_elapsed = g_strdup_printf("%d:%02d:%02d", h, m, s);
		}
		else
		{
			*time_elapsed = g_strdup(_("Not started"));
		}
	}

	if (time_remaining != NULL) {
		if (purple_xfer_is_completed(xfer)) {
			*time_remaining = g_strdup(_("Finished"));
		}
		else if (purple_xfer_is_cancelled(xfer)) {
			*time_remaining = g_strdup(_("Cancelled"));
		}
		else if (purple_xfer_get_size(xfer) == 0 || (kb_sent > 0 && kbps < 0.001)) {
			*time_remaining = g_strdup(_("Unknown"));
		}
		else if (kb_sent <= 0) {
			*time_remaining = g_strdup(_("Waiting for transfer to begin"));
		}
		else {
			int h, m, s;
			int secs_remaining;

			secs_remaining = (int)(kb_rem / kbps);

			h = secs_remaining / 3600;
			m = (secs_remaining % 3600) / 60;
			s = secs_remaining % 60;

			*time_remaining = g_strdup_printf("%d:%02d:%02d", h, m, s);
		}
	}
}

static void
update_title_progress(PidginXferDialog *dialog)
{
	gboolean valid;
	GtkTreeIter iter;
	int num_active_xfers = 0;
	guint64 total_bytes_xferred = 0;
	guint64 total_file_size = 0;

	if (dialog->window == NULL)
		return;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->model), &iter);

	/* Find all active transfers */
	while (valid) {
		GValue val;
		PurpleXfer *xfer = NULL;

		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model),
				&iter, COLUMN_DATA, &val);

		xfer = g_value_get_pointer(&val);
		if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_STARTED) {
			num_active_xfers++;
			total_bytes_xferred += purple_xfer_get_bytes_sent(xfer);
			total_file_size += purple_xfer_get_size(xfer);
		}

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->model), &iter);
	}

	/* Update the title */
	if (num_active_xfers > 0)
	{
		gchar *title;
		int total_pct = 0;

		if (total_file_size > 0) {
			total_pct = 100 * total_bytes_xferred / total_file_size;
		}

		title = g_strdup_printf(ngettext("File Transfers - %d%% of %d file",
						 "File Transfers - %d%% of %d files",
						 num_active_xfers),
					total_pct, num_active_xfers);
		gtk_window_set_title(GTK_WINDOW(dialog->window), title);
		g_free(title);
	} else {
		gtk_window_set_title(GTK_WINDOW(dialog->window), _("File Transfers"));
	}
}

static void
update_detailed_info(PidginXferDialog *dialog, PurpleXfer *xfer)
{
	PidginXferUiData *data;
	char *kbsec, *time_elapsed, *time_remaining;
	char *status, *utf8;

	if (dialog == NULL || xfer == NULL)
		return;

	data = purple_xfer_get_ui_data(xfer);

	get_xfer_info_strings(xfer, &kbsec, &time_elapsed, &time_remaining);

	status = g_strdup_printf("%d%% (%" G_GOFFSET_FORMAT " of %" G_GOFFSET_FORMAT " bytes)",
							 (int)(purple_xfer_get_progress(xfer)*100),
							 purple_xfer_get_bytes_sent(xfer),
							 purple_xfer_get_size(xfer));

	if (purple_xfer_is_completed(xfer)) {

		GdkPixbuf *pixbuf = NULL;

		pixbuf = gtk_widget_render_icon(xfer_dialog->window,
										PIDGIN_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(GTK_LIST_STORE(xfer_dialog->model), &data->iter,
						   COLUMN_STATUS, pixbuf,
						   -1);

		g_object_unref(pixbuf);
	}

	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE) {
		gtk_label_set_markup(GTK_LABEL(dialog->local_user_desc_label),
							 _("<b>Receiving As:</b>"));
		gtk_label_set_markup(GTK_LABEL(dialog->remote_user_desc_label),
							 _("<b>Receiving From:</b>"));
	}
	else {
		gtk_label_set_markup(GTK_LABEL(dialog->remote_user_desc_label),
							 _("<b>Sending To:</b>"));
		gtk_label_set_markup(GTK_LABEL(dialog->local_user_desc_label),
							 _("<b>Sending As:</b>"));
	}

	gtk_label_set_text(GTK_LABEL(dialog->local_user_label),
								 purple_account_get_username(purple_xfer_get_account(xfer)));
	gtk_label_set_text(GTK_LABEL(dialog->remote_user_label), purple_xfer_get_remote_user(xfer));
	gtk_label_set_text(GTK_LABEL(dialog->protocol_label),
								 purple_account_get_protocol_name(purple_xfer_get_account(xfer)));

	if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE) {
		gtk_label_set_text(GTK_LABEL(dialog->filename_label),
					   purple_xfer_get_filename(xfer));
	} else {
		char *tmp;

		tmp = g_path_get_basename(purple_xfer_get_local_filename(xfer));
		utf8 = g_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
		g_free(tmp);

		gtk_label_set_text(GTK_LABEL(dialog->filename_label), utf8);
		g_free(utf8);
	}

	utf8 = g_filename_to_utf8((purple_xfer_get_local_filename(xfer)), -1, NULL, NULL, NULL);
	gtk_label_set_text(GTK_LABEL(dialog->localfile_label), utf8);
	g_free(utf8);

	gtk_label_set_text(GTK_LABEL(dialog->status_label), status);

	gtk_label_set_text(GTK_LABEL(dialog->speed_label), kbsec);
	gtk_label_set_text(GTK_LABEL(dialog->time_elapsed_label), time_elapsed);
	gtk_label_set_text(GTK_LABEL(dialog->time_remaining_label),
					   time_remaining);

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress),
								  purple_xfer_get_progress(xfer));

	g_free(kbsec);
	g_free(time_elapsed);
	g_free(time_remaining);
	g_free(status);
}

static void
update_buttons(PidginXferDialog *dialog, PurpleXfer *xfer)
{
	if (dialog->selected_xfer == NULL) {
		gtk_widget_set_sensitive(dialog->expander, FALSE);
		gtk_widget_set_sensitive(dialog->open_button, FALSE);
		gtk_widget_set_sensitive(dialog->stop_button, FALSE);

		gtk_widget_show(dialog->stop_button);
		gtk_widget_hide(dialog->remove_button);

		return;
	}

	if (dialog->selected_xfer != xfer)
		return;

	if (purple_xfer_is_completed(xfer)) {
		gtk_widget_hide(dialog->stop_button);
		gtk_widget_show(dialog->remove_button);

#ifdef _WIN32
		/* If using Win32... */
		if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE) {
			gtk_widget_set_sensitive(dialog->open_button, TRUE);
		} else {
			gtk_widget_set_sensitive(dialog->open_button, FALSE);
		}
#else
		if (purple_xfer_get_xfer_type(xfer) == PURPLE_XFER_TYPE_RECEIVE) {
			gtk_widget_set_sensitive(dialog->open_button, TRUE);
		} else {
			gtk_widget_set_sensitive (dialog->open_button, FALSE);
		}
#endif

		gtk_widget_set_sensitive(dialog->remove_button, TRUE);
	} else if (purple_xfer_is_cancelled(xfer)) {
		gtk_widget_hide(dialog->stop_button);
		gtk_widget_show(dialog->remove_button);

		gtk_widget_set_sensitive(dialog->open_button,  FALSE);

		gtk_widget_set_sensitive(dialog->remove_button, TRUE);
	} else {
		gtk_widget_show(dialog->stop_button);
		gtk_widget_hide(dialog->remove_button);

		gtk_widget_set_sensitive(dialog->open_button,  FALSE);
		gtk_widget_set_sensitive(dialog->stop_button,   TRUE);
	}
}

static void
ensure_row_selected(PidginXferDialog *dialog)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		return;

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->model), &iter))
		gtk_tree_selection_select_iter(selection, &iter);
}

/**************************************************************************
 * Callbacks
 **************************************************************************/
static gint
delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginXferDialog *dialog;

	dialog = (PidginXferDialog *)d;

	pidgin_xfer_dialog_hide(dialog);

	return TRUE;
}

static void
toggle_keep_open_cb(GtkWidget *w, PidginXferDialog *dialog)
{
	dialog->keep_open = !dialog->keep_open;
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/filetransfer/keep_open",
						dialog->keep_open);
}

static void
toggle_clear_finished_cb(GtkWidget *w, PidginXferDialog *dialog)
{
	dialog->auto_clear = !dialog->auto_clear;
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/filetransfer/clear_finished",
						dialog->auto_clear);
}

static void
selection_changed_cb(GtkTreeSelection *selection, PidginXferDialog *dialog)
{
	GtkTreeIter iter;
	PurpleXfer *xfer = NULL;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		GValue val;

		gtk_widget_set_sensitive(dialog->expander, TRUE);

		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model),
								 &iter, COLUMN_DATA, &val);

		xfer = g_value_get_pointer(&val);

		update_detailed_info(dialog, xfer);

		dialog->selected_xfer = xfer;
	}
	else {
		gtk_expander_set_expanded(GTK_EXPANDER(dialog->expander),
									 FALSE);

		gtk_widget_set_sensitive(dialog->expander, FALSE);

		dialog->selected_xfer = NULL;
	}

	update_buttons(dialog, xfer);
}

static void
open_button_cb(GtkButton *button, PidginXferDialog *dialog)
{
#ifdef _WIN32
	/* If using Win32... */
	int code;
	wchar_t *wc_filename = g_utf8_to_utf16(
			purple_xfer_get_local_filename(
				dialog->selected_xfer),
			-1, NULL, NULL, NULL);

	code = (int) ShellExecuteW(NULL, NULL, wc_filename, NULL, NULL,
			SW_SHOW);

	g_free(wc_filename);

	if (code == SE_ERR_ASSOCINCOMPLETE || code == SE_ERR_NOASSOC)
	{
		purple_notify_error(dialog, NULL,
				_("There is no application configured to open this type of file."),
				NULL, NULL);
	}
	else if (code < 32)
	{
		purple_notify_error(dialog, NULL,
				_("An error occurred while opening the file."), NULL, NULL);
		purple_debug_warning("xfer", "filename: %s; code: %d\n",
				purple_xfer_get_local_filename(dialog->selected_xfer), code);
	}
#else
	const char *filename = purple_xfer_get_local_filename(dialog->selected_xfer);
	char *command = NULL;
	char *tmp = NULL;
	GError *error = NULL;

	if (purple_running_gnome())
	{
		char *escaped = g_shell_quote(filename);
		command = g_strdup_printf("gnome-open %s", escaped);
		g_free(escaped);
	}
	else if (purple_running_kde())
	{
		char *escaped = g_shell_quote(filename);

		if (purple_str_has_suffix(filename, ".desktop"))
			command = g_strdup_printf("kfmclient openURL %s 'text/plain'", escaped);
		else
			command = g_strdup_printf("kfmclient openURL %s", escaped);
		g_free(escaped);
	}
	else
	{
		gchar *uri = g_strdup_printf("file://%s", filename);
		purple_notify_uri(NULL, uri);
		g_free(uri);
		return;
	}

	if (purple_program_is_valid(command))
	{
		gint exit_status;
		if (!g_spawn_command_line_sync(command, NULL, NULL, &exit_status, &error))
		{
			tmp = g_strdup_printf(_("Error launching %s: %s"),
							purple_xfer_get_local_filename(dialog->selected_xfer),
							error->message);
			purple_notify_error(dialog, NULL, _("Unable to open file."), tmp, NULL);
			g_free(tmp);
			g_error_free(error);
		}
		if (exit_status != 0)
		{
			char *primary = g_strdup_printf(_("Error running %s"), command);
			char *secondary = g_strdup_printf(_("Process returned error code %d"),
									exit_status);
			purple_notify_error(dialog, NULL, primary, secondary, NULL);
			g_free(tmp);
		}
	}
#endif
}

static void
remove_button_cb(GtkButton *button, PidginXferDialog *dialog)
{
	pidgin_xfer_dialog_remove_xfer(dialog, dialog->selected_xfer);
}

static void
stop_button_cb(GtkButton *button, PidginXferDialog *dialog)
{
	purple_xfer_cancel_local(dialog->selected_xfer);
}

static void
close_button_cb(GtkButton *button, PidginXferDialog *dialog)
{
	pidgin_xfer_dialog_hide(dialog);
}


/**************************************************************************
 * Dialog Building Functions
 **************************************************************************/
static GtkWidget *
setup_tree(PidginXferDialog *dialog)
{
	GtkWidget *tree;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	/* Build the tree model */
	/* Transfer type, Progress Bar, Filename, Size, Remaining */
	model = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_INT,
							   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
							   G_TYPE_POINTER);
	dialog->model = model;

	/* Create the treeview */
	dialog->tree = tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	/* gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE); */

	gtk_widget_show(tree);

	g_signal_connect(G_OBJECT(selection), "changed",
					 G_CALLBACK(selection_changed_cb), dialog);

	g_object_unref(G_OBJECT(model));


	/* Columns */

	/* Transfer Type column */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
				"pixbuf", COLUMN_STATUS, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
									GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 25);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Progress bar column */
	renderer = gtk_cell_renderer_progress_new();
	column = gtk_tree_view_column_new_with_attributes(_("Progress"), renderer,
				"value", COLUMN_PROGRESS, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Filename column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"), renderer,
				"text", COLUMN_FILENAME, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* File Size column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Size"), renderer,
				"text", COLUMN_SIZE, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Bytes Remaining column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Remaining"),
				renderer, "text", COLUMN_REMAINING, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(tree));

	gtk_widget_show(tree);

	return tree;
}

static GtkWidget *
make_info_grid(PidginXferDialog *dialog)
{
	GtkWidget *grid;
	GtkWidget *label;
	gsize i;

	struct
	{
		GtkWidget **desc_label;
		GtkWidget **val_label;
		const char *desc;

	} labels[] =
	{
		{ &dialog->local_user_desc_label, &dialog->local_user_label, NULL },
		{ &dialog->remote_user_desc_label, &dialog->remote_user_label, NULL },
		{ &label, &dialog->protocol_label,       _("Protocol:") },
		{ &label, &dialog->filename_label,       _("Filename:") },
		{ &label, &dialog->localfile_label,      _("Local File:") },
		{ &label, &dialog->status_label,         _("Status:") },
		{ &label, &dialog->speed_label,          _("Speed:") },
		{ &label, &dialog->time_elapsed_label,   _("Time Elapsed:") },
		{ &label, &dialog->time_remaining_label, _("Time Remaining:") }
	};

	/* Setup the initial grid */
	dialog->grid = grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), PIDGIN_HIG_BOX_SPACE);
	gtk_grid_set_column_spacing(GTK_GRID(grid), PIDGIN_HIG_BOX_SPACE);

	/* Setup the labels */
	for (i = 0; i < G_N_ELEMENTS(labels); i++) {
		GtkWidget *label;
		char buf[256];

		g_snprintf(buf, sizeof(buf), "<b>%s</b>",
			   labels[i].desc != NULL ? labels[i].desc : "");

		*labels[i].desc_label = label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label), buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		gtk_grid_attach(GTK_GRID(grid), label, 0, i, 1, 1);
		gtk_widget_set_valign(label, GTK_ALIGN_CENTER);

		gtk_widget_show(label);

		*labels[i].val_label = label = gtk_label_new(NULL);
		gtk_label_set_xalign(GTK_LABEL(label), 0);
		gtk_grid_attach(GTK_GRID(grid), label, 1, i, 1, 1);
		gtk_widget_set_hexpand(label, TRUE);
		gtk_widget_set_valign(label, GTK_ALIGN_CENTER);

		gtk_widget_show(label);
	}

	/* Setup the progress bar */
	dialog->progress = gtk_progress_bar_new();
	gtk_grid_attach(GTK_GRID(grid), dialog->progress,
		0, G_N_ELEMENTS(labels), 2, 1);

	gtk_widget_show(dialog->progress);

	return grid;
}

PidginXferDialog *
pidgin_xfer_dialog_new(void)
{
	PidginXferDialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *expander;
#if !GTK_CHECK_VERSION(3,14,0)
	GtkWidget *alignment;
#endif
	GtkWidget *grid;
	GtkWidget *checkbox;
	GtkWidget *bbox;

	dialog = g_new0(PidginXferDialog, 1);
	dialog->keep_open =
		purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/filetransfer/keep_open");
	dialog->auto_clear =
		purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/filetransfer/clear_finished");

	/* Create the window. */
	dialog->window = window = pidgin_create_dialog(_("File Transfers"), 0, "file transfer", TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 450, 250);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the main vbox for top half of the window. */
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(window), FALSE, PIDGIN_HIG_BORDER);

	/* Setup the listbox */
	gtk_box_pack_start(GTK_BOX(vbox),
		pidgin_make_scrollable(setup_tree(dialog), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, 140),
		TRUE, TRUE, 0);

	/* "Close this window when all transfers finish" */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("Close this window when all transfers _finish"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
								 !dialog->keep_open);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_keep_open_cb), dialog);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
	gtk_widget_show(checkbox);

	/* "Clear finished transfers" */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("C_lear finished transfers"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
								 dialog->auto_clear);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_clear_finished_cb), dialog);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
	gtk_widget_show(checkbox);

	/* "Download Details" arrow */
	expander = gtk_expander_new_with_mnemonic(_("File transfer _details"));
	dialog->expander = expander;
	gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);
	gtk_widget_show(expander);

	gtk_widget_set_sensitive(expander, FALSE);

#if GTK_CHECK_VERSION(3,14,0)
	/* The grid of information. */
	grid = make_info_grid(dialog);
	gtk_container_add(GTK_CONTAINER(expander), grid);
	gtk_widget_show(grid);

	/* Small indent make grid fall under GtkExpander's label */
	gtk_widget_set_margin_start(grid, 20);
#else
	/* Small indent make grid fall under GtkExpander's label */
	alignment = gtk_alignment_new(1, 0, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 20, 0);
	gtk_container_add(GTK_CONTAINER(expander), alignment);
	gtk_widget_show(alignment);

	/* The grid of information. */
	grid = make_info_grid(dialog);
	gtk_container_add(GTK_CONTAINER(alignment), grid);
	gtk_widget_show(grid);
#endif

	bbox = pidgin_dialog_get_action_area(GTK_DIALOG(window));

#define ADD_BUTTON(b, label, callback, callbackdata) do { \
		GtkWidget *button = gtk_button_new_from_stock(label); \
		gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0); \
		g_signal_connect(G_OBJECT(button), "clicked", callback, callbackdata); \
		gtk_widget_show(button); \
		b = button; \
	} while (0)

	/* Open button */
	ADD_BUTTON(dialog->open_button, GTK_STOCK_OPEN, G_CALLBACK(open_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->open_button, FALSE);

	/* Remove button */
	ADD_BUTTON(dialog->remove_button, GTK_STOCK_REMOVE, G_CALLBACK(remove_button_cb), dialog);
	gtk_widget_hide(dialog->remove_button);

	/* Stop button */
	ADD_BUTTON(dialog->stop_button, GTK_STOCK_STOP, G_CALLBACK(stop_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->stop_button, FALSE);

	/* Close button */
	ADD_BUTTON(dialog->close_button, GTK_STOCK_CLOSE, G_CALLBACK(close_button_cb), dialog);

#undef ADD_BUTTON

#ifdef _WIN32
	g_signal_connect(G_OBJECT(dialog->window), "show",
		G_CALLBACK(winpidgin_ensure_onscreen), dialog->window);
#endif

	return dialog;
}

void
pidgin_xfer_dialog_destroy(PidginXferDialog *dialog)
{
	g_return_if_fail(dialog != NULL);

	purple_notify_close_with_handle(dialog);

	gtk_widget_destroy(dialog->window);

	g_free(dialog);
}

void
pidgin_xfer_dialog_show(PidginXferDialog *dialog)
{
	PidginXferDialog *tmp;

	if (dialog == NULL) {
		tmp = pidgin_get_xfer_dialog();

		if (tmp == NULL) {
			tmp = pidgin_xfer_dialog_new();
			pidgin_set_xfer_dialog(tmp);
		}

		gtk_widget_show(tmp->window);
	} else {
		gtk_window_present(GTK_WINDOW(dialog->window));
	}
}

void
pidgin_xfer_dialog_hide(PidginXferDialog *dialog)
{
	g_return_if_fail(dialog != NULL);

	purple_notify_close_with_handle(dialog);

	gtk_widget_hide(dialog->window);
}

void
pidgin_xfer_dialog_add_xfer(PidginXferDialog *dialog, PurpleXfer *xfer)
{
	PidginXferUiData *data;
	PurpleXferType type;
	GdkPixbuf *pixbuf;
	char *size_str, *remaining_str;
	char *lfilename, *utf8;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	g_object_ref(xfer);

	data = purple_xfer_get_ui_data(xfer);
	data->in_list = TRUE;

	pidgin_xfer_dialog_show(dialog);

	data->last_updated_time = 0;

	type = purple_xfer_get_xfer_type(xfer);

	size_str      = purple_str_size_to_units(purple_xfer_get_size(xfer));
	remaining_str = purple_str_size_to_units(purple_xfer_get_bytes_remaining(xfer));

	pixbuf = gtk_widget_render_icon(dialog->window,
									(type == PURPLE_XFER_TYPE_RECEIVE
									 ? PIDGIN_STOCK_DOWNLOAD
									 : PIDGIN_STOCK_UPLOAD),
									GTK_ICON_SIZE_MENU, NULL);

	gtk_list_store_append(dialog->model, &data->iter);
	lfilename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
	utf8 = g_filename_to_utf8(lfilename, -1, NULL, NULL, NULL);
	g_free(lfilename);
	lfilename = utf8;
	gtk_list_store_set(dialog->model, &data->iter,
					   COLUMN_STATUS, pixbuf,
					   COLUMN_PROGRESS, 0,
					   COLUMN_FILENAME, (type == PURPLE_XFER_TYPE_RECEIVE)
					                     ? purple_xfer_get_filename(xfer)
							     : lfilename,
					   COLUMN_SIZE, size_str,
					   COLUMN_REMAINING, _("Waiting for transfer to begin"),
					   COLUMN_DATA, xfer,
					   -1);
	g_free(lfilename);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dialog->tree));

	g_object_unref(pixbuf);

	g_free(size_str);
	g_free(remaining_str);

	dialog->num_transfers++;

	ensure_row_selected(dialog);
	update_title_progress(dialog);
}

void
pidgin_xfer_dialog_remove_xfer(PidginXferDialog *dialog,
								PurpleXfer *xfer)
{
	PidginXferUiData *data;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = purple_xfer_get_ui_data(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	data->in_list = FALSE;

	gtk_list_store_remove(GTK_LIST_STORE(dialog->model), &data->iter);

	dialog->num_transfers--;

	ensure_row_selected(dialog);

	update_title_progress(dialog);
	g_object_unref(xfer);
}

void
pidgin_xfer_dialog_cancel_xfer(PidginXferDialog *dialog,
								PurpleXfer *xfer)
{
	PidginXferUiData *data;
	GdkPixbuf *pixbuf;
	const gchar *status;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = purple_xfer_get_ui_data(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	if ((purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) && (dialog->auto_clear)) {
		pidgin_xfer_dialog_remove_xfer(dialog, xfer);
		return;
	}

	data = purple_xfer_get_ui_data(xfer);

	update_detailed_info(dialog, xfer);
	update_title_progress(dialog);

	pixbuf = gtk_widget_render_icon(dialog->window,
									PIDGIN_STOCK_FILE_CANCELLED,
									GTK_ICON_SIZE_MENU, NULL);

	if (purple_xfer_is_cancelled(xfer))
		status = _("Cancelled");
	else
		status = _("Failed");

	gtk_list_store_set(dialog->model, &data->iter,
	                   COLUMN_STATUS, pixbuf,
	                   COLUMN_REMAINING, status,
	                   -1);

	g_object_unref(pixbuf);

	update_buttons(dialog, xfer);
}

void
pidgin_xfer_dialog_update_xfer(PidginXferDialog *dialog,
								PurpleXfer *xfer)
{
	PidginXferUiData *data;
	char *size_str, *remaining_str;
	time_t current_time;
	GtkTreeIter iter;
	gboolean valid;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	if ((data = purple_xfer_get_ui_data(xfer)) == NULL)
		return;

	if (data->in_list == FALSE)
		return;

	current_time = time(NULL);
	if (((current_time - data->last_updated_time) == 0) &&
		(!purple_xfer_is_completed(xfer)))
	{
		/* Don't update the window more than once per second */
		return;
	}
	data->last_updated_time = current_time;

	size_str      = purple_str_size_to_units(purple_xfer_get_size(xfer));
	remaining_str = purple_str_size_to_units(purple_xfer_get_bytes_remaining(xfer));

	gtk_list_store_set(xfer_dialog->model, &data->iter,
					   COLUMN_PROGRESS, (gint)(purple_xfer_get_progress(xfer) * 100),
					   COLUMN_SIZE, size_str,
					   COLUMN_REMAINING, remaining_str,
					   -1);

	g_free(size_str);
	g_free(remaining_str);

	if (purple_xfer_is_completed(xfer))
	{
		GdkPixbuf *pixbuf;

		pixbuf = gtk_widget_render_icon(dialog->window,
										PIDGIN_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(GTK_LIST_STORE(xfer_dialog->model), &data->iter,
						   COLUMN_STATUS, pixbuf,
						   COLUMN_REMAINING, _("Finished"),
						   -1);

		g_object_unref(pixbuf);
	}

	update_title_progress(dialog);
	if (xfer == dialog->selected_xfer)
		update_detailed_info(xfer_dialog, xfer);

	if (purple_xfer_is_completed(xfer) && dialog->auto_clear)
		pidgin_xfer_dialog_remove_xfer(dialog, xfer);
	else
		update_buttons(dialog, xfer);

	/*
	 * If all transfers are finished, and the pref is set, then
	 * close the dialog.  Otherwise just exit this function.
	 */
	if (dialog->keep_open)
		return;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->model), &iter);
	while (valid)
	{
		GValue val;
		PurpleXfer *next;

		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model),
				&iter, COLUMN_DATA, &val);

		next = g_value_get_pointer(&val);
		if (!purple_xfer_is_completed(next))
			return;

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->model), &iter);
	}

	/* If we got to this point then we know everything is finished */
	pidgin_xfer_dialog_hide(dialog);
}

/**************************************************************************
 * PidginXferDialog GBoxed code
 **************************************************************************/
static PidginXferDialog *
pidgin_xfer_dialog_ref(PidginXferDialog *dialog)
{
	g_return_val_if_fail(dialog != NULL, NULL);

	dialog->box_count++;

	return dialog;
}

static void
pidgin_xfer_dialog_unref(PidginXferDialog *dialog)
{
	g_return_if_fail(dialog != NULL);
	g_return_if_fail(dialog->box_count >= 0);

	if (!dialog->box_count--)
		pidgin_xfer_dialog_destroy(dialog);
}

GType
pidgin_xfer_dialog_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PidginXferDialog",
				(GBoxedCopyFunc)pidgin_xfer_dialog_ref,
				(GBoxedFreeFunc)pidgin_xfer_dialog_unref);
	}

	return type;
}

/**************************************************************************
 * File Transfer UI Ops
 **************************************************************************/
static void
pidgin_xfer_new_xfer(PurpleXfer *xfer)
{
	PidginXferUiData *data;

	/* This is where we're setting xfer's "ui_data" for the first time. */
	data = g_new0(PidginXferUiData, 1);
	purple_xfer_set_ui_data(xfer, data);
}

static void
pidgin_xfer_destroy(PurpleXfer *xfer)
{
	PidginXferUiData *data;

	data = purple_xfer_get_ui_data(xfer);
	if (data) {
		g_free(data->name);
		g_free(data);
		purple_xfer_set_ui_data(xfer, NULL);
	}
}

static void
pidgin_xfer_add_xfer(PurpleXfer *xfer)
{
	if (xfer_dialog == NULL)
		xfer_dialog = pidgin_xfer_dialog_new();

	pidgin_xfer_dialog_add_xfer(xfer_dialog, xfer);
}

static void
pidgin_xfer_update_progress(PurpleXfer *xfer, double percent)
{
	pidgin_xfer_dialog_update_xfer(xfer_dialog, xfer);
}

static void
pidgin_xfer_cancel_local(PurpleXfer *xfer)
{
	if (xfer_dialog)
		pidgin_xfer_dialog_cancel_xfer(xfer_dialog, xfer);
}

static void
pidgin_xfer_cancel_remote(PurpleXfer *xfer)
{
	if (xfer_dialog)
		pidgin_xfer_dialog_cancel_xfer(xfer_dialog, xfer);
}

static void
pidgin_xfer_add_thumbnail(PurpleXfer *xfer, const gchar *formats)
{
	purple_debug_info("xfer", "creating thumbnail for transfer\n");

	if (purple_xfer_get_size(xfer) <= PIDGIN_XFER_MAX_SIZE_IMAGE_THUMBNAIL) {
		GdkPixbuf *thumbnail =
			pidgin_pixbuf_new_from_file_at_size(
					purple_xfer_get_local_filename(xfer), 128, 128);

		if (thumbnail) {
			gchar **formats_split = g_strsplit(formats, ",", 0);
			gchar *buffer = NULL;
			gsize size;
			char *option_keys[2] = {NULL, NULL};
			char *option_values[2] = {NULL, NULL};
			int i;
			gchar *format = NULL;

			for (i = 0; formats_split[i]; i++) {
				if (purple_strequal(formats_split[i], "jpeg")) {
					purple_debug_info("xfer", "creating JPEG thumbnail\n");
					option_keys[0] = "quality";
					option_values[0] = "90";
					format = "jpeg";
					break;
				} else if (purple_strequal(formats_split[i], "png")) {
					purple_debug_info("xfer", "creating PNG thumbnail\n");
					option_keys[0] = "compression";
					option_values[0] = "9";
					format = "png";
					break;
				}
			}

			/* Try the first format given by the protocol without options */
			if (format == NULL) {
				purple_debug_info("xfer",
				    "creating thumbnail of format %s as demanded by protocol\n",
				    formats_split[0]);
				format = formats_split[0];
			}

			gdk_pixbuf_save_to_bufferv(thumbnail, &buffer, &size, format,
				option_keys, option_values, NULL);

			if (buffer) {
				gchar *mimetype = g_strdup_printf("image/%s", format);
				purple_debug_info("xfer",
				                  "created thumbnail of %" G_GSIZE_FORMAT " bytes\n",
					size);
				purple_xfer_set_thumbnail(xfer, buffer, size, mimetype);
				g_free(buffer);
				g_free(mimetype);
			}
			g_object_unref(thumbnail);
			g_strfreev(formats_split);
		}
	}
}

static PurpleXferUiOps ops =
{
	pidgin_xfer_new_xfer,
	pidgin_xfer_destroy,
	pidgin_xfer_add_xfer,
	pidgin_xfer_update_progress,
	pidgin_xfer_cancel_local,
	pidgin_xfer_cancel_remote,
	NULL,
	NULL,
	NULL,
	pidgin_xfer_add_thumbnail
};

/**************************************************************************
 * GTK+ File Transfer API
 **************************************************************************/
void
pidgin_xfers_init(void)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/filetransfer");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/filetransfer/clear_finished", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/filetransfer/keep_open", FALSE);
}

void
pidgin_xfers_uninit(void)
{
	if (xfer_dialog != NULL)
		pidgin_xfer_dialog_destroy(xfer_dialog);
}

void
pidgin_set_xfer_dialog(PidginXferDialog *dialog)
{
	xfer_dialog = dialog;
}

PidginXferDialog *
pidgin_get_xfer_dialog(void)
{
	return xfer_dialog;
}

PurpleXferUiOps *
pidgin_xfers_get_ui_ops(void)
{
	return &ops;
}
