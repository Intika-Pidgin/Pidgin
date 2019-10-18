/*
 * pidgin
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

#include "pidginpluginsdialog.h"

#include <glib/gi18n.h>

struct _PidginPluginsDialog {
	GtkDialog parent;

	GtkWidget *close_button;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_plugins_dialog_close(GtkWidget *b, gpointer data) {
	gtk_widget_destroy(GTK_WIDGET(data));
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginPluginsDialog, pidgin_plugins_dialog, GTK_TYPE_DIALOG);

static void
pidgin_plugins_dialog_class_init(PidginPluginsDialogClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/Plugins/dialog.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginPluginsDialog, close_button);
}

static void
pidgin_plugins_dialog_init(PidginPluginsDialog *dialog) {
	gtk_widget_init_template(GTK_WIDGET(dialog));

	/* wire up the close button */
	g_signal_connect(
		dialog->close_button,
		"clicked",
		G_CALLBACK(pidgin_plugins_dialog_close),
		dialog
	);
}

GtkWidget *
pidgin_plugins_dialog_new(void) {
	return g_object_new(
		PIDGIN_TYPE_PLUGINS_DIALOG,
		"title", _("Plugins"),
		NULL
	);
}

