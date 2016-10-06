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
 *
 */

#include "internal.h"
#include "core.h"
#include "pidgin.h"
#include "pidginstock.h"

#include "debug.h"
#include "notify.h"
#include "request.h"
#include "tls-certificate.h"
#include "tls-certificate-info.h"

#include "gtk3compat.h"
#include "gtkblist.h"
#include "gtkutils.h"

#include "gtkcertmgr.h"

/*****************************************************************************
 * X.509 certificate management interface                                    *
 *****************************************************************************/

typedef struct {
	GtkWidget *mgmt_widget;
	GtkTreeView *listview;
	GtkTreeSelection *listselect;
	GtkWidget *importbutton;
	GtkWidget *exportbutton;
	GtkWidget *infobutton;
	GtkWidget *deletebutton;
} tls_peers_mgmt_data;

tls_peers_mgmt_data *tpm_dat = NULL;

/* Columns
   See http://developer.gnome.org/doc/API/2.0/gtk/TreeWidget.html */
enum
{
	TPM_HOSTNAME_COLUMN,
	TPM_N_COLUMNS
};

static void
tls_peers_mgmt_destroy(GtkWidget *mgmt_widget, gpointer data)
{
	purple_debug_info("certmgr",
			  "tls peers self-destructs\n");

	purple_signals_disconnect_by_handle(tpm_dat);
	purple_request_close_with_handle(tpm_dat);
	g_free(tpm_dat); tpm_dat = NULL;
}

static void
tls_peers_mgmt_repopulate_list(void)
{
	GtkTreeView *listview = tpm_dat->listview;
	GList *idlist, *l;

	GtkListStore *store = GTK_LIST_STORE(
		gtk_tree_view_get_model(GTK_TREE_VIEW(listview)));

	/* First, delete everything in the list */
	gtk_list_store_clear(store);

	/* Grab the available certificates */
	idlist = purple_tls_certificate_list_ids();

	/* Populate the listview */
	for (l = idlist; l; l = l->next) {
		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);

		gtk_list_store_set(GTK_LIST_STORE(store), &iter,
				   TPM_HOSTNAME_COLUMN, l->data,
				   -1);
	}

	purple_tls_certificate_free_ids(idlist);
}

static void
tls_peers_mgmt_select_chg_cb(GtkTreeSelection *ignored, gpointer data)
{
	GtkTreeSelection *select = tpm_dat->listselect;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* See if things are selected */
	if (gtk_tree_selection_get_selected(select, &model, &iter)) {
		/* Enable buttons if something is selected */
		gtk_widget_set_sensitive(GTK_WIDGET(tpm_dat->exportbutton), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(tpm_dat->infobutton), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(tpm_dat->deletebutton), TRUE);
	} else {
		/* Otherwise, disable them */
		gtk_widget_set_sensitive(GTK_WIDGET(tpm_dat->exportbutton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(tpm_dat->infobutton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(tpm_dat->deletebutton), FALSE);

	}
}

static void
tls_peers_mgmt_import_ok2_cb(gpointer data, const char *result)
{
	GTlsCertificate *crt = data;
	GError *error = NULL;

	/* TODO: Perhaps prompt if you're overwriting a cert? */

	/* Trust the certificate */
	if (result && *result) {
		if(!purple_tls_certificate_trust(result, crt, &error)) {
			purple_debug_error("gtkcertmgr/tls_peers_mgmt",
					"Error trusting certificate '%s': %s",
					result, error->message);
			g_clear_error(&error);
		}

		tls_peers_mgmt_repopulate_list();
	}

	/* And this certificate is not needed any more */
	g_object_unref(crt);
}

static void
tls_peers_mgmt_import_cancel2_cb(gpointer data, const char *result)
{
	GTlsCertificate *crt = data;
	g_object_unref(crt);
}

static void
tls_peers_mgmt_import_ok_cb(gpointer data, const char *filename)
{
	GTlsCertificate *crt;
	GError *error = NULL;

	/* Now load the certificate from disk */
	crt = g_tls_certificate_new_from_file(filename, &error);

	/* Did it work? */
	if (crt != NULL) {
		gchar *default_hostname;
		PurpleTlsCertificateInfo *info;

		/* Get name to add trust as */
		/* Make a guess about what the hostname should be */
		info = purple_tls_certificate_get_info(crt);
		default_hostname = purple_tls_certificate_info_get_subject_name(info);
		purple_tls_certificate_info_free(info);

		/* TODO: Find a way to make sure that crt gets destroyed
		   if the window gets closed unusually, such as by handle
		   deletion */
		/* TODO: Display some more information on the certificate? */
		purple_request_input(tpm_dat,
				     _("Certificate Import"),
				     _("Specify a hostname"),
				     _("Type the host name for this certificate."),
				     default_hostname,
				     FALSE, /* Not multiline */
				     FALSE, /* Not masked? */
				     NULL,  /* No hints? */
				     _("OK"),
				     G_CALLBACK(tls_peers_mgmt_import_ok2_cb),
				     _("Cancel"),
				     G_CALLBACK(tls_peers_mgmt_import_cancel2_cb),
				     NULL,  /* No additional parameters */
				     crt    /* Pass cert instance to callback*/
				     );

		g_free(default_hostname);
	} else {
		/* Errors! Oh no! */
		/* TODO: Perhaps find a way to be specific about what just
		   went wrong? */
		gchar * secondary;

		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				"File %s couldn't be imported: %s",
				filename, error->message);
		g_clear_error(&error);

		secondary = g_strdup_printf(_("File %s could not be imported.\nMake sure that the file is readable and in PEM format.\n"), filename);
		purple_notify_error(NULL,
				    _("Certificate Import Error"),
				    _("X.509 certificate import failed"),
				    secondary, NULL);
		g_free(secondary);
	}
}

static void
tls_peers_mgmt_import_cb(GtkWidget *button, gpointer data)
{
	/* TODO: need to tell the user that we want a .PEM file! */
	purple_request_file(tpm_dat,
			    _("Select a PEM certificate"),
			    "certificate.pem",
			    FALSE, /* Not a save dialog */
			    G_CALLBACK(tls_peers_mgmt_import_ok_cb),
			    NULL,  /* Do nothing if cancelled */
			    NULL, NULL); /* No extra parameters */
}

static void
tls_peers_mgmt_export_ok_cb(gpointer data, const char *filename)
{
	GTlsCertificate *crt = data;
	gchar *pem = NULL;
	GError *error = NULL;

	g_assert(filename);

	g_object_get(crt, "certificate-pem", &pem, NULL);

	if (!g_file_set_contents(filename, pem, -1, &error)) {
		/* Errors! Oh no! */
		/* TODO: Perhaps find a way to be specific about what just
		   went wrong? */
		gchar * secondary;

		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				"File %s couldn't be exported: %s",
				filename, error->message);
		g_clear_error(&error);

		secondary = g_strdup_printf(_("Export to file %s failed.\nCheck that you have write permission to the target path\n"), filename);
		purple_notify_error(NULL,
				    _("Certificate Export Error"),
				    _("X.509 certificate export failed"),
				    secondary, NULL);
		g_free(secondary);
	} else {
		tls_peers_mgmt_repopulate_list();
	}

	g_free(pem);
	g_object_unref(crt);
}

static void
tls_peers_mgmt_export_cb(GtkWidget *button, gpointer data)
{
	GTlsCertificate *crt;
	GtkTreeSelection *select = tpm_dat->listselect;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *id;
	GError *error = NULL;

	/* See if things are selected */
	if (!gtk_tree_selection_get_selected(select, &model, &iter)) {
		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				     "Export clicked with no selection?\n");
		return;
	}

	/* Retrieve the selected hostname */
	gtk_tree_model_get(model, &iter, TPM_HOSTNAME_COLUMN, &id, -1);

	/* Extract the certificate from the pool now to make sure it doesn't
	   get deleted out from under us */
	crt = purple_tls_certificate_new_from_id(id, &error);

	if (NULL == crt) {
		purple_debug_error("gtkcertmgr/tls_peers_mgmt",
				   "Error fetching trusted cert '%s': %s\n",
				   id, error->message);
		g_clear_error(&error);
		g_free(id);
		return;
	}
	g_free(id);

	/* TODO: inform user that it will be a PEM? */
	purple_request_file(tpm_dat,
			    _("PEM X.509 Certificate Export"),
			    "certificate.pem",
			    TRUE, /* Is a save dialog */
			    G_CALLBACK(tls_peers_mgmt_export_ok_cb),
			    G_CALLBACK(g_object_unref),
			    NULL, /* No extra parameters */
			    crt); /* Pass the certificate on to the callback */
}

static void
tls_peers_mgmt_info_cb(GtkWidget *button, gpointer data)
{
	GtkTreeSelection *select = tpm_dat->listselect;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *id;
	GTlsCertificate *crt;
	gchar *title;
	GError *error = NULL;

	/* See if things are selected */
	if (!gtk_tree_selection_get_selected(select, &model, &iter)) {
		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				     "Info clicked with no selection?\n");
		return;
	}

	/* Retrieve the selected hostname */
	gtk_tree_model_get(model, &iter, TPM_HOSTNAME_COLUMN, &id, -1);

	/* Now retrieve the certificate */
	crt = purple_tls_certificate_new_from_id(id, &error);

	if (crt == NULL) {
		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				"Unable to fetch certificate '%s': %s",
				id, error ? error->message : "unknown error");
		g_clear_error(&error);
		g_free(id);
	}

	/* Fire the notification */
	title = g_strdup_printf(_("Certificate Information for %s"), id);
	purple_request_certificate(tpm_dat, title, NULL, NULL, crt,
	                           _("OK"), G_CALLBACK(g_object_unref),
	                           _("Cancel"), G_CALLBACK(g_object_unref),
	                           crt);

	g_free(id);
	g_free(title);
}

static void
tls_peers_mgmt_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer data)
{
	tls_peers_mgmt_info_cb(NULL, NULL);
}

static void
tls_peers_mgmt_delete_confirm_cb(gchar *id, gint choice)
{
	GError *error = NULL;

	if (1 == choice) {
		/* Yes, distrust was confirmed */
		/* Now distrust the thing */
		if (!purple_tls_certificate_distrust(id, &error)) {
			purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
					     "Deletion failed on id %s: %s\n",
					     id, error->message);
			g_clear_error(&error);
		} else {
			tls_peers_mgmt_repopulate_list();
		}
	}

	g_free(id);
}

static void
tls_peers_mgmt_delete_cb(GtkWidget *button, gpointer data)
{
	GtkTreeSelection *select = tpm_dat->listselect;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* See if things are selected */
	if (gtk_tree_selection_get_selected(select, &model, &iter)) {

		gchar *id;
		gchar *primary;

		/* Retrieve the selected hostname */
		gtk_tree_model_get(model, &iter, TPM_HOSTNAME_COLUMN, &id, -1);

		/* Prompt to confirm deletion */
		primary = g_strdup_printf(
			_("Really delete certificate for %s?"), id );

		purple_request_yes_no(tpm_dat, _("Confirm certificate delete"),
				      primary, NULL, /* Can this be NULL? */
				      0, /* "yes" is the default action */
				      NULL,
				      id, /* id ownership passed to callback */
				      tls_peers_mgmt_delete_confirm_cb,
				      tls_peers_mgmt_delete_confirm_cb );

		g_free(primary);

	} else {
		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				     "Delete clicked with no selection?\n");
		return;
	}
}

static GtkWidget *
tls_peers_mgmt_build(void)
{
	GtkWidget *bbox;
	GtkListStore *store;

	/* This block of variables will end up in tpm_dat */
	GtkTreeView *listview;
	GtkTreeSelection *select;
	GtkWidget *importbutton;
	GtkWidget *exportbutton;
	GtkWidget *infobutton;
	GtkWidget *deletebutton;
	/** Element to return to the Certmgr window to put in the Notebook */
	GtkWidget *mgmt_widget;

	/* Create a struct to store context information about this window */
	tpm_dat = g_new0(tls_peers_mgmt_data, 1);

	tpm_dat->mgmt_widget = mgmt_widget = gtk_box_new(
		GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(mgmt_widget),
		PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(mgmt_widget);

	/* Ensure that everything gets cleaned up when the dialog box
	   is closed */
	g_signal_connect(G_OBJECT(mgmt_widget), "destroy",
			 G_CALLBACK(tls_peers_mgmt_destroy), NULL);

	/* List view */
	store = gtk_list_store_new(TPM_N_COLUMNS, G_TYPE_STRING);

	tpm_dat->listview = listview =
		GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
	g_object_unref(G_OBJECT(store));

	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;

		/* Set up the display columns */
		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
			_("Hostname"),
			renderer,
			"text", TPM_HOSTNAME_COLUMN,
			NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
				TPM_HOSTNAME_COLUMN, GTK_SORT_ASCENDING);
	}

	/* Get the treeview selector into the struct */
	tpm_dat->listselect = select =
		gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));

	/* Force the selection mode */
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

	/* Use a callback to enable/disable the buttons based on whether
	   something is selected */
	g_signal_connect(G_OBJECT(select), "changed",
			 G_CALLBACK(tls_peers_mgmt_select_chg_cb), NULL);

	g_signal_connect(G_OBJECT(listview), "row-activated",
			 G_CALLBACK(tls_peers_mgmt_activated_cb), NULL);

	gtk_box_pack_start(GTK_BOX(mgmt_widget), 
			pidgin_make_scrollable(GTK_WIDGET(listview), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, -1),
			TRUE, TRUE, /* Take up lots of space */
			0);
	gtk_widget_show(GTK_WIDGET(listview));

	/* Fill the list for the first time */
	tls_peers_mgmt_repopulate_list();

	/* Right-hand side controls box */
	bbox = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_end(GTK_BOX(mgmt_widget), bbox,
			 FALSE, FALSE, /* Do not take up space */
			 0);
	gtk_box_set_spacing(GTK_BOX(bbox), PIDGIN_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
	gtk_widget_show(bbox);

	/* Import button */
	tpm_dat->importbutton = importbutton =
		gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start(GTK_BOX(bbox), importbutton, FALSE, FALSE, 0);
	gtk_widget_show(importbutton);
	g_signal_connect(G_OBJECT(importbutton), "clicked",
			 G_CALLBACK(tls_peers_mgmt_import_cb), NULL);


	/* Export button */
	tpm_dat->exportbutton = exportbutton =
		gtk_button_new_from_stock(GTK_STOCK_SAVE_AS);
	gtk_box_pack_start(GTK_BOX(bbox), exportbutton, FALSE, FALSE, 0);
	gtk_widget_show(exportbutton);
	g_signal_connect(G_OBJECT(exportbutton), "clicked",
			 G_CALLBACK(tls_peers_mgmt_export_cb), NULL);


	/* Info button */
	tpm_dat->infobutton = infobutton =
		gtk_button_new_from_stock(PIDGIN_STOCK_INFO);
	gtk_box_pack_start(GTK_BOX(bbox), infobutton, FALSE, FALSE, 0);
	gtk_widget_show(infobutton);
	g_signal_connect(G_OBJECT(infobutton), "clicked",
			 G_CALLBACK(tls_peers_mgmt_info_cb), NULL);


	/* Delete button */
	tpm_dat->deletebutton = deletebutton =
		gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(bbox), deletebutton, FALSE, FALSE, 0);
	gtk_widget_show(deletebutton);
	g_signal_connect(G_OBJECT(deletebutton), "clicked",
			 G_CALLBACK(tls_peers_mgmt_delete_cb), NULL);

	/* Call the "selection changed" callback, which will probably disable
	   all the buttons since nothing is selected yet */
	tls_peers_mgmt_select_chg_cb(select, NULL);

	return mgmt_widget;
}

const PidginCertificateManager tls_peers_mgmt = {
	tls_peers_mgmt_build, /* Widget creation function */
	N_("SSL Servers")
};

/*****************************************************************************
 * GTK+ main certificate manager                                             *
 *****************************************************************************/
typedef struct
{
	GtkWidget *window;
	GtkWidget *notebook;

	GtkWidget *closebutton;
} CertMgrDialog;

/* If a certificate manager window is open, this will point to it.
   So if it is set, don't open another one! */
CertMgrDialog *certmgr_dialog = NULL;

static gboolean
certmgr_close_cb(GtkWidget *w, CertMgrDialog *dlg)
{
	/* TODO: Ignoring the arguments to this function may not be ideal,
	   but there *should* only be "one dialog to rule them all" at a time*/
	pidgin_certmgr_hide();
	return FALSE;
}

void
pidgin_certmgr_show(void)
{
	CertMgrDialog *dlg;
	GtkWidget *win;
	GtkWidget *vbox;

	/* Enumerate all the certificates on file */
	{
		GList *idlist;
		GList *l;

		purple_debug_info("gtkcertmgr",
				  "Enumerating X.509 certificates:\n");

		idlist = purple_tls_certificate_list_ids();

		for (l=idlist; l; l = l->next) {
			purple_debug_info("gtkcertmgr",
					  "- %s\n",
					  l->data ? (gchar *) l->data : "(null)");
		} /* idlist */

		purple_tls_certificate_free_ids(idlist);
	}


	/* If the manager is already open, bring it to the front */
	if (certmgr_dialog != NULL) {
		gtk_window_present(GTK_WINDOW(certmgr_dialog->window));
		return;
	}

	/* Create the dialog, and set certmgr_dialog so we never create
	   more than one at a time */
	dlg = certmgr_dialog = g_new0(CertMgrDialog, 1);

	win = dlg->window =
		pidgin_create_dialog(_("Certificate Manager"),/* Title */
				     0, /*Window border*/
				     "certmgr",         /* Role */
				     TRUE); /* Allow resizing */
	g_signal_connect(G_OBJECT(win), "delete_event",
			 G_CALLBACK(certmgr_close_cb), dlg);


	/* TODO: Retrieve the user-set window size and use it */
	gtk_window_set_default_size(GTK_WINDOW(win), 400, 400);

	/* Main vbox */
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(win), FALSE, PIDGIN_HIG_BORDER);

	/* Notebook of various certificate managers */
	dlg->notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), dlg->notebook,
			   TRUE, TRUE, /* Notebook should take extra space */
			   0);
	gtk_widget_show(dlg->notebook);

	/* Close button */
	dlg->closebutton = pidgin_dialog_add_button(GTK_DIALOG(win), GTK_STOCK_CLOSE,
			G_CALLBACK(certmgr_close_cb), dlg);

	/* Add the defined certificate managers */
	/* TODO: Find a way of determining whether each is shown or not */
	/* TODO: Implement this correctly */
	gtk_notebook_append_page(GTK_NOTEBOOK (dlg->notebook),
				 (tls_peers_mgmt.build)(),
				 gtk_label_new(_(tls_peers_mgmt.label)) );

	gtk_widget_show(win);
}

void
pidgin_certmgr_hide(void)
{
	/* If it isn't open, do nothing */
	if (certmgr_dialog == NULL) {
		return;
	}

	purple_signals_disconnect_by_handle(certmgr_dialog);
	purple_prefs_disconnect_by_handle(certmgr_dialog);

	gtk_widget_destroy(certmgr_dialog->window);
	g_free(certmgr_dialog);
	certmgr_dialog = NULL;
}
