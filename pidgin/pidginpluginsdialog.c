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

#include <gplugin.h>
#include <gplugin-gtk.h>

#include <purple.h>

#include "gtkpluginpref.h"

struct _PidginPluginsDialog {
	GtkDialog parent;

	GtkWidget *tree_view;
	GtkWidget *configure_plugin_button;
	GtkWidget *close_button;
	GtkWidget *plugin_info;

	GtkListStore *plugin_store;
};

/* this has a short life left to it... */
typedef struct
{
	enum
	{
		PIDGIN_PLUGIN_UI_DATA_TYPE_FRAME,
		PIDGIN_PLUGIN_UI_DATA_TYPE_REQUEST
	} type;

	union
	{
		struct
		{
			GtkWidget *dialog;
			PurplePluginPrefFrame *pref_frame;
		} frame;

		gpointer request_handle;
	} u;
} PidginPluginUiData;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gboolean
pidgin_plugins_dialog_plugin_has_config(GPluginPlugin *plugin) {
	GPluginPluginInfo *ginfo = gplugin_plugin_get_info(plugin);
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(ginfo);
	GPluginPluginState state;

	g_return_val_if_fail(GPLUGIN_IS_PLUGIN(plugin), FALSE);

	state = gplugin_plugin_get_state(plugin);

	if (state != GPLUGIN_PLUGIN_STATE_LOADED) {
		return FALSE;
	}

	return (purple_plugin_info_get_pref_frame_cb(info) ||
	       purple_plugin_info_get_pref_request_cb(info));
}

static GPluginPlugin *
pidgin_plugins_dialog_get_selected(PidginPluginsDialog *dialog) {
	GPluginPlugin *plugin = NULL;
	GtkTreeSelection *selection = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	g_return_val_if_fail(PIDGIN_IS_PLUGINS_DIALOG(dialog), NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree_view));
	/* not sure if this is necessary, but playing defense. - grim 20191112 */
	if(selection == NULL) {
		return NULL;
	}

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter,
		                   GPLUGIN_GTK_STORE_PLUGIN_COLUMN, &plugin,
		                   -1);
	}

	return plugin;
}

static void
pidgin_plugins_dialog_pref_dialog_close(GPluginPlugin *plugin) {
	GPluginPluginInfo *ginfo = gplugin_plugin_get_info(plugin);
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(ginfo);
	PidginPluginUiData *ui_data;

	ui_data = purple_plugin_info_get_ui_data(info);
	if (ui_data == NULL) {
		purple_debug_info("PidginPluginsDialog", "failed to find uidata\n");
		return;
	}

	if (ui_data->type == PIDGIN_PLUGIN_UI_DATA_TYPE_REQUEST) {
		purple_request_close(PURPLE_REQUEST_FIELDS,
			ui_data->u.request_handle);
		return;
	}

	g_return_if_fail(ui_data->type == PIDGIN_PLUGIN_UI_DATA_TYPE_FRAME);

	gtk_widget_destroy(ui_data->u.frame.dialog);

	if (ui_data->u.frame.pref_frame) {
		purple_plugin_pref_frame_destroy(ui_data->u.frame.pref_frame);
	}

	g_free(ui_data);
	purple_plugin_info_set_ui_data(info, NULL);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_plugins_dialog_close(GtkWidget *b, gpointer data) {
	gtk_widget_destroy(GTK_WIDGET(data));
}

static void
pidgin_plugins_dialog_selection_cb(GtkTreeSelection *sel, gpointer data) {
	PidginPluginsDialog *dialog = PIDGIN_PLUGINS_DIALOG(data);
	GPluginPlugin *plugin = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_get_selected(sel, &model, &iter)) {
		gtk_tree_model_get(model, &iter,
		                   GPLUGIN_GTK_STORE_PLUGIN_COLUMN, &plugin,
		                   -1);
	}

	gplugin_gtk_plugin_info_set_plugin(
		GPLUGIN_GTK_PLUGIN_INFO(dialog->plugin_info),
		plugin
	);

	gtk_widget_set_sensitive(
		GTK_WIDGET(dialog->configure_plugin_button),
		pidgin_plugins_dialog_plugin_has_config(plugin)
	);
}

static void
pidgin_plugins_dialog_pref_dialog_response_cb(GtkWidget *dialog, int response,
                                              gpointer data)
{
	if (response == GTK_RESPONSE_CLOSE ||
		response == GTK_RESPONSE_DELETE_EVENT)
	{
		pidgin_plugins_dialog_pref_dialog_close(GPLUGIN_PLUGIN(data));
	}
}

static void
pidgin_plugins_dialog_config_plugin_cb(GtkWidget *button, gpointer data) {
	PidginPluginsDialog *dialog = PIDGIN_PLUGINS_DIALOG(data);
	PidginPluginUiData *ui_data;
	PurplePluginInfo *info;
	PurplePluginPrefFrameCb pref_frame_cb;
	PurplePluginPrefRequestCb pref_request_cb;
	GPluginPlugin *plugin = NULL;
	GPluginPluginInfo *ginfo = NULL;
	gint prefs_count;

	plugin = pidgin_plugins_dialog_get_selected(dialog);
	if(!GPLUGIN_IS_PLUGIN(plugin)) {
		return;
	}

	ginfo = gplugin_plugin_get_info(plugin);
	info = PURPLE_PLUGIN_INFO(ginfo);

	if(purple_plugin_info_get_ui_data(info)) {
		return;
	}

	pref_frame_cb = purple_plugin_info_get_pref_frame_cb(info);
	pref_request_cb = purple_plugin_info_get_pref_request_cb(info);

	ui_data = g_new0(PidginPluginUiData, 1);
	purple_plugin_info_set_ui_data(info, ui_data);

	prefs_count = 0;
	if (pref_frame_cb) {
		prefs_count++;

		ui_data->u.frame.pref_frame = pref_frame_cb(plugin);
	}

	if (pref_request_cb)
		prefs_count++;

	if (prefs_count > 1) {
		purple_debug_warning("gtkplugin",
		                     "Plugin %s contains more than one prefs "
		                     "callback, some will be ignored.",
		                     gplugin_plugin_info_get_name(ginfo));
	}
	g_return_if_fail(prefs_count > 0);


	/* Priority: pidgin frame > purple request > purple frame
	 * Purple frame could be replaced with purple request some day.
	 */
	if (pref_request_cb) {
		ui_data->type = PIDGIN_PLUGIN_UI_DATA_TYPE_REQUEST;
		ui_data->u.request_handle = pref_request_cb(plugin);
		purple_request_add_close_notify(ui_data->u.request_handle,
			purple_callback_set_zero, &info->ui_data);
		purple_request_add_close_notify(ui_data->u.request_handle,
			g_free, ui_data);
	} else {
		GtkWidget *box, *pdialog, *content, *sw;

		ui_data->type = PIDGIN_PLUGIN_UI_DATA_TYPE_FRAME;

		box = pidgin_plugin_pref_create_frame(ui_data->u.frame.pref_frame);
		if (box == NULL) {
			purple_debug_error("gtkplugin",
				"Failed to display prefs frame");
			g_free(ui_data);
			purple_plugin_info_set_ui_data(info, NULL);
			return;
		}
		gtk_widget_set_vexpand(box, TRUE);

		ui_data->u.frame.dialog = pdialog = gtk_dialog_new_with_buttons(
			PIDGIN_ALERT_TITLE, GTK_WINDOW(dialog),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			_("Close"), GTK_RESPONSE_CLOSE,
			NULL);

		g_signal_connect(G_OBJECT(pdialog), "response",
			G_CALLBACK(pidgin_plugins_dialog_pref_dialog_response_cb), plugin);

		content = gtk_dialog_get_content_area(GTK_DIALOG(pdialog));

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(content), sw);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
		                               GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
		                                    GTK_SHADOW_IN);
		gtk_widget_set_size_request(sw, 400, 400);

		gtk_container_add(GTK_CONTAINER(sw), box);

		gtk_window_set_role(GTK_WINDOW(pdialog), "plugin_config");
		gtk_window_set_title(GTK_WINDOW(pdialog),
		                     _(gplugin_plugin_info_get_name(
		                             GPLUGIN_PLUGIN_INFO(info))));
		gtk_widget_show_all(pdialog);
	}
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

	gtk_widget_class_bind_template_child(widget_class, PidginPluginsDialog, tree_view);
	gtk_widget_class_bind_template_child(widget_class, PidginPluginsDialog, configure_plugin_button);
	gtk_widget_class_bind_template_child(widget_class, PidginPluginsDialog, close_button);
	gtk_widget_class_bind_template_child(widget_class, PidginPluginsDialog, plugin_info);
	gtk_widget_class_bind_template_child(widget_class, PidginPluginsDialog, plugin_store);

	gtk_widget_class_bind_template_callback(widget_class, pidgin_plugins_dialog_selection_cb);
	gtk_widget_class_bind_template_callback(widget_class, pidgin_plugins_dialog_config_plugin_cb);
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

	/* set the sort column for the plugin_store */
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(dialog->plugin_store),
		GPLUGIN_GTK_STORE_MARKUP_COLUMN,
		GTK_SORT_ASCENDING
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

