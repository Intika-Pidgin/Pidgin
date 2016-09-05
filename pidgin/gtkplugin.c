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
#include "gtkplugin.h"
#include "gtkpluginpref.h"
#include "gtkutils.h"
#include "debug.h"
#include "prefs.h"
#include "request.h"
#include "pidgintooltip.h"

#include <string.h>

#include "gtk3compat.h"

#define PIDGIN_PLUGIN_INFO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_PLUGIN_INFO, PidginPluginInfoPrivate))

#define PIDGIN_RESPONSE_CONFIGURE 98121

typedef struct
{
	PidginPluginConfigFrameCb config_frame_cb;
} PidginPluginInfoPrivate;

enum
{
	PROP_0,
	PROP_GTK_CONFIG_FRAME_CB,
	PROP_LAST
};

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

static void plugin_toggled_stage_two(PurplePlugin *plug, GtkTreeModel *model,
                                  GtkTreeIter *iter, GError *error, gboolean unload);

static GtkWidget *expander = NULL;
static GtkWidget *plugin_dialog = NULL;

static GtkLabel *plugin_name = NULL;
static GtkTextBuffer *plugin_desc = NULL;
static GtkLabel *plugin_error = NULL;
static GtkLabel *plugin_authors = NULL;
static GtkLabel *plugin_website = NULL;
static gchar *plugin_website_uri = NULL;
static GtkLabel *plugin_filename = NULL;

static GtkWidget *pref_button = NULL;

/* Set method for GObject properties */
static void
pidgin_plugin_info_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PidginPluginInfoPrivate *priv = PIDGIN_PLUGIN_INFO_GET_PRIVATE(obj);

	switch (param_id) {
		case PROP_GTK_CONFIG_FRAME_CB:
			priv->config_frame_cb = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
pidgin_plugin_info_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PidginPluginInfoPrivate *priv = PIDGIN_PLUGIN_INFO_GET_PRIVATE(obj);

	switch (param_id) {
		case PROP_GTK_CONFIG_FRAME_CB:
			g_value_set_pointer(value, priv->config_frame_cb);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Class initializer function */
static void pidgin_plugin_info_class_init(PidginPluginInfoClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(PidginPluginInfoPrivate));

	/* Setup properties */
	obj_class->get_property = pidgin_plugin_info_get_property;
	obj_class->set_property = pidgin_plugin_info_set_property;

	g_object_class_install_property(obj_class, PROP_GTK_CONFIG_FRAME_CB,
		g_param_spec_pointer("gtk-config-frame-cb",
		                     "GTK configuration frame callback",
		                     "Callback that returns a GTK configuration frame",
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                     G_PARAM_STATIC_STRINGS));
}

GType
pidgin_plugin_info_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PidginPluginInfoClass),
			.class_init = (GClassInitFunc)pidgin_plugin_info_class_init,
			.instance_size = sizeof(PidginPluginInfo),
		};

		type = g_type_register_static(PURPLE_TYPE_PLUGIN_INFO,
		                              "PidginPluginInfo", &info, 0);
	}

	return type;
}

PidginPluginInfo *
pidgin_plugin_info_new(const char *first_property, ...)
{
	GObject *info;
	va_list var_args;

	/* at least ID is required */
	if (!first_property)
		return NULL;

	va_start(var_args, first_property);
	info = g_object_new_valist(PIDGIN_TYPE_PLUGIN_INFO, first_property,
	                           var_args);
	va_end(var_args);

	g_object_set(info, "ui-requirement", PIDGIN_UI, NULL);

	return PIDGIN_PLUGIN_INFO(info);
}

static gboolean
pidgin_plugin_has_prefs(PurplePlugin *plugin)
{
	PurplePluginInfo *info = purple_plugin_get_info(plugin);
	PidginPluginInfoPrivate *priv = NULL;
	gboolean ret;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (!purple_plugin_is_loaded(plugin))
		return FALSE;

	if (PIDGIN_IS_PLUGIN_INFO(info))
		priv = PIDGIN_PLUGIN_INFO_GET_PRIVATE(info);

	ret = ((priv && priv->config_frame_cb) ||
			purple_plugin_info_get_pref_frame_cb(info) ||
			purple_plugin_info_get_pref_request_cb(info));

	return ret;
}

static GtkWidget *
pidgin_plugin_get_config_frame(PurplePlugin *plugin,
	PurplePluginPrefFrame **purple_pref_frame)
{
	GtkWidget *config = NULL;
	PurplePluginInfo *info;
	PurplePluginPrefFrameCb pref_frame_cb = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN(plugin), NULL);

	info = purple_plugin_get_info(plugin);
	if(!PURPLE_IS_PLUGIN_INFO(info))
		return NULL;

	pref_frame_cb = purple_plugin_info_get_pref_frame_cb(info);
	if(pref_frame_cb) {
		PurplePluginPrefFrame *frame = pref_frame_cb(plugin);

		if(frame) {
			config = pidgin_plugin_pref_create_frame(frame);

			*purple_pref_frame = frame;
		}
	}

	return config;
}

static void
pref_dialog_close(PurplePlugin *plugin)
{
	PurplePluginInfo *info;
	PidginPluginUiData *ui_data;

	g_return_if_fail(plugin != NULL);

	info = purple_plugin_get_info(plugin);

	ui_data = purple_plugin_info_get_ui_data(info);
	if (ui_data == NULL)
		return;

	if (ui_data->type == PIDGIN_PLUGIN_UI_DATA_TYPE_REQUEST) {
		purple_request_close(PURPLE_REQUEST_FIELDS,
			ui_data->u.request_handle);
		return;
	}

	g_return_if_fail(ui_data->type == PIDGIN_PLUGIN_UI_DATA_TYPE_FRAME);

	gtk_widget_destroy(ui_data->u.frame.dialog);

	if (ui_data->u.frame.pref_frame)
		purple_plugin_pref_frame_destroy(ui_data->u.frame.pref_frame);

	g_free(ui_data);
	purple_plugin_info_set_ui_data(info, NULL);
}


static void
pref_dialog_response_cb(GtkWidget *dialog, int response, PurplePlugin *plugin)
{
	if (response == GTK_RESPONSE_CLOSE ||
		response == GTK_RESPONSE_DELETE_EVENT)
	{
		pref_dialog_close(plugin);
	}
}

static void
pidgin_plugin_open_config(PurplePlugin *plugin, GtkWindow *parent)
{
	PurplePluginInfo *info;
	PidginPluginInfoPrivate *priv = NULL;
	PidginPluginUiData *ui_data;
	PurplePluginPrefFrameCb pref_frame_cb;
	PurplePluginPrefRequestCb pref_request_cb;
	PidginPluginConfigFrameCb get_pidgin_frame = NULL;
	gint prefs_count;

	g_return_if_fail(plugin != NULL);

	info = purple_plugin_get_info(plugin);

	if (!pidgin_plugin_has_prefs(plugin)) {
		purple_debug_warning("gtkplugin", "Plugin has no prefs");
		return;
	}

	if (purple_plugin_info_get_ui_data(info))
		return;

	if (PIDGIN_IS_PLUGIN_INFO(info))
		priv = PIDGIN_PLUGIN_INFO_GET_PRIVATE(info);

	pref_frame_cb = purple_plugin_info_get_pref_frame_cb(info);
	pref_request_cb = purple_plugin_info_get_pref_request_cb(info);

	if (priv)
		get_pidgin_frame = priv->config_frame_cb;

	prefs_count = 0;
	if (pref_frame_cb)
		prefs_count++;
	if (pref_request_cb)
		prefs_count++;
	if (get_pidgin_frame)
		prefs_count++;

	if (prefs_count > 1) {
		purple_debug_warning("gtkplugin", "Plugin %s contains more than"
			" one prefs callback, some will be ignored.",
			purple_plugin_info_get_name(info));
	}
	g_return_if_fail(prefs_count > 0);

	ui_data = g_new0(PidginPluginUiData, 1);
	purple_plugin_info_set_ui_data(info, ui_data);

	/* Priority: pidgin frame > purple request > purple frame
	 * Purple frame could be replaced with purple request some day.
	 */
	if (pref_request_cb && !get_pidgin_frame) {
		ui_data->type = PIDGIN_PLUGIN_UI_DATA_TYPE_REQUEST;
		ui_data->u.request_handle = pref_request_cb(plugin);
		purple_request_add_close_notify(ui_data->u.request_handle,
			purple_callback_set_zero, &info->ui_data);
		purple_request_add_close_notify(ui_data->u.request_handle,
			g_free, ui_data);
	} else {
		GtkWidget *box, *dialog;

		ui_data->type = PIDGIN_PLUGIN_UI_DATA_TYPE_FRAME;

		box = pidgin_plugin_get_config_frame(plugin,
			&ui_data->u.frame.pref_frame);
		if (box == NULL) {
			purple_debug_error("gtkplugin",
				"Failed to display prefs frame");
			g_free(ui_data);
			purple_plugin_info_set_ui_data(info, NULL);
			return;
		}
		gtk_widget_set_vexpand(box, TRUE);

		ui_data->u.frame.dialog = dialog = gtk_dialog_new_with_buttons(
			PIDGIN_ALERT_TITLE, parent,
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE,
			GTK_RESPONSE_CLOSE, NULL);

		g_signal_connect(G_OBJECT(dialog), "response",
			G_CALLBACK(pref_dialog_response_cb), plugin);

		gtk_container_add(GTK_CONTAINER(
			gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
			pidgin_make_scrollable(box, GTK_POLICY_NEVER,
				GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, 400, 400));

		gtk_window_set_role(GTK_WINDOW(dialog), "plugin_config");
		gtk_window_set_title(GTK_WINDOW(dialog),
			_(purple_plugin_info_get_name(info)));
		gtk_widget_show_all(dialog);
	}
}

void
pidgin_plugins_save(void)
{
	purple_plugins_save_loaded(PIDGIN_PREFS_ROOT "/plugins/loaded");
}

static void
update_plugin_list(void *data)
{
	GtkListStore *ls = GTK_LIST_STORE(data);
	GtkTreeIter iter;
	GList *plugins, *l;
	PurplePlugin *plug;
	PurplePluginInfo *info;

	gtk_list_store_clear(ls);
	purple_plugins_refresh();

	plugins = purple_plugins_find_all();

	for (l = plugins; l != NULL; l = l->next)
	{
		char *name;
		char *version;
		char *summary;
		char *desc;
		plug = PURPLE_PLUGIN(l->data);
		info = purple_plugin_get_info(plug);

		if (purple_plugin_is_internal(plug))
			continue;

		gtk_list_store_append (ls, &iter);

		if (purple_plugin_info_get_name(info)) {
			name = g_markup_escape_text(_(purple_plugin_info_get_name(info)), -1);
		} else {
			char *tmp = g_path_get_basename(purple_plugin_get_filename(plug));
			name = g_markup_escape_text(tmp, -1);
			g_free(tmp);
		}
		version = g_markup_escape_text(purple_plugin_info_get_version(info), -1);
		summary = g_markup_escape_text(purple_plugin_info_get_summary(info), -1);

		desc = g_strdup_printf("<b>%s</b> %s\n%s", name,
				       version,
				       summary);
		g_free(name);
		g_free(version);
		g_free(summary);

		gtk_list_store_set(ls, &iter,
				   0, purple_plugin_is_loaded(plug),
				   1, desc,
				   2, plug,
				   3, purple_plugin_info_get_error(info),
				   -1);
		g_free(desc);
	}

	g_list_free(plugins);
}

static gboolean
check_if_loaded(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	PurplePlugin *plugin;
	gtk_tree_model_get(model, iter, 2, &plugin, -1);
	gtk_list_store_set(GTK_LIST_STORE(model), iter,
					           0, purple_plugin_is_loaded(plugin),
					           -1);
	return FALSE;
}

static void
update_loaded_plugins(GtkTreeModel *model)
{
	gtk_tree_model_foreach(model, check_if_loaded, NULL);
}

static void plugin_loading_common(PurplePlugin *plugin, GtkTreeView *view, gboolean loaded)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(view);

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			PurplePlugin *plug;
			GtkTreeSelection *sel;

			gtk_tree_model_get(model, &iter, 2, &plug, -1);

			if (plug != plugin)
				continue;

			gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, loaded, -1);

			/* If the loaded/unloaded plugin is the selected row,
			 * update the pref_button. */
			sel = gtk_tree_view_get_selection(view);
			if (gtk_tree_selection_get_selected(sel, &model, &iter))
			{
				gtk_tree_model_get(model, &iter, 2, &plug, -1);
				if (plug == plugin)
				{
					gtk_widget_set_sensitive(pref_button,
						pidgin_plugin_has_prefs(plug));
				}
			}

			break;
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}

static void plugin_load_cb(PurplePlugin *plugin, gpointer data)
{
	GtkTreeView *view = (GtkTreeView *)data;
	plugin_loading_common(plugin, view, TRUE);
}

static void plugin_unload_cb(PurplePlugin *plugin, gpointer data)
{
	GtkTreeView *view = (GtkTreeView *)data;
	plugin_loading_common(plugin, view, FALSE);
}

static void plugin_unload_confirm_cb(gpointer *data)
{
	PurplePlugin *plugin = (PurplePlugin *)data[0];
	GtkTreeModel *model = (GtkTreeModel *)data[1];
	GtkTreeIter *iter = (GtkTreeIter *)data[2];

	plugin_toggled_stage_two(plugin, model, iter, NULL, TRUE);

	g_free(data);
}

static void plugin_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter *iter = g_new(GtkTreeIter, 1);
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	PurplePlugin *plug;
	GError *error = NULL;

	gtk_tree_model_get_iter(model, iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, iter, 2, &plug, -1);

	if (!purple_plugin_is_loaded(plug))
	{
		pidgin_set_cursor(plugin_dialog, GDK_WATCH);

		purple_plugin_load(plug, &error);
		plugin_toggled_stage_two(plug, model, iter, error, FALSE);

		pidgin_clear_cursor(plugin_dialog);
	}
	else
	{
		pref_dialog_close(plug);

		if (purple_plugin_get_dependent_plugins(plug) != NULL)
		{
			GString *tmp = g_string_new(_("The following plugins will be unloaded."));
			GSList *l;
			gpointer *cb_data;

			for (l = purple_plugin_get_dependent_plugins(plug); l != NULL ; l = l->next)
			{
				const char *dep_name = (const char *)l->data;
				PurplePlugin *dep_plugin = purple_plugins_find_plugin(dep_name);
				PurplePluginInfo *dep_info;

				g_return_if_fail(dep_plugin != NULL);

				dep_info = purple_plugin_get_info(dep_plugin);
				g_string_append_printf(tmp, "\n\t%s\n", purple_plugin_info_get_name(dep_info));
			}

			cb_data = g_new(gpointer, 3);
			cb_data[0] = plug;
			cb_data[1] = model;
			cb_data[2] = iter;

			purple_request_action(plugin_dialog, NULL,
				_("Multiple plugins will be unloaded."),
				tmp->str, 0, NULL, cb_data, 2,
				_("Unload Plugins"),
				G_CALLBACK(plugin_unload_confirm_cb),
				_("Cancel"), g_free);
			g_string_free(tmp, TRUE);
		}
		else
			plugin_toggled_stage_two(plug, model, iter, NULL, TRUE);
	}
}

static void plugin_toggled_stage_two(PurplePlugin *plug, GtkTreeModel *model, GtkTreeIter *iter, GError *error, gboolean unload)
{
	PurplePluginInfo *info = purple_plugin_get_info(plug);

	if (unload)
	{
		pidgin_set_cursor(plugin_dialog, GDK_WATCH);

		if (!purple_plugin_unload(plug, &error))
		{
			const char *primary = _("Could not unload plugin");
			const char *reload = _("The plugin could not be unloaded now, but will be disabled at the next startup.");

			char *tmp = g_strdup_printf("%s\n\n%s", reload, error->message);
			purple_notify_warning(NULL, NULL, primary, tmp, NULL);
			g_free(tmp);

			purple_plugin_disable(plug);
		}

		pidgin_clear_cursor(plugin_dialog);
	}
	else if (!unload && error)
	{
		purple_notify_warning(NULL, NULL, _("Could not load plugin"), error->message, NULL);
	}

	gtk_widget_set_sensitive(pref_button, pidgin_plugin_has_prefs(plug));

	if (error != NULL)
	{
		gchar *name = g_markup_escape_text(purple_plugin_info_get_name(info), -1);

		gchar *disp_error = g_markup_escape_text(error->message, -1);
		gchar *text;

		text = g_strdup_printf(
			"<b>%s</b> %s\n<span weight=\"bold\" color=\"red\"%s</span>",
			purple_plugin_info_get_name(info),
			purple_plugin_info_get_version(info), disp_error);
		gtk_list_store_set(GTK_LIST_STORE (model), iter,
				   1, text,
				   -1);
		g_free(text);

		text = g_strdup_printf(
			"<span weight=\"bold\" color=\"red\">%s</span>",
			disp_error);
		gtk_label_set_markup(plugin_error, text);
		g_free(text);

		g_free(disp_error);
		g_free(name);

		g_error_free(error);
	}

	if ((unload && purple_plugin_get_dependent_plugins(plug)) ||
	   (!unload && purple_plugin_info_get_dependencies(info)))
		update_loaded_plugins(model);
	else
		gtk_list_store_set(GTK_LIST_STORE (model), iter,
			               0, purple_plugin_is_loaded(plug),
			               -1);
	g_free(iter);

	pidgin_plugins_save();
}

static gboolean ensure_plugin_visible(void *data)
{
	GtkTreeSelection *sel = GTK_TREE_SELECTION(data);
	GtkTreeView *tv = gtk_tree_selection_get_tree_view(sel);
	GtkTreeModel *model = gtk_tree_view_get_model(tv);
	GtkTreePath *path;
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected (sel, &model, &iter))
		return FALSE;
	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_scroll_to_cell(gtk_tree_selection_get_tree_view(sel), path, NULL, FALSE, 0, 0);
	gtk_tree_path_free(path);
	return FALSE;
}

static void prefs_plugin_sel (GtkTreeSelection *sel, GtkTreeModel *model)
{
	gchar *buf, *tmp, *name, *version;
	gchar *authors = NULL;
	const gchar * const *authorlist;
	GtkTreeIter  iter;
	GValue val;
	PurplePlugin *plug;
	PurplePluginInfo *info;

	if (!gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		gtk_widget_set_sensitive(pref_button, FALSE);

		/* Collapse and disable the expander widget */
		gtk_expander_set_expanded(GTK_EXPANDER(expander), FALSE);
		gtk_widget_set_sensitive(expander, FALSE);

		return;
	}

	gtk_widget_set_sensitive(expander, TRUE);

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 2, &val);
	plug = g_value_get_pointer(&val);
	info = purple_plugin_get_info(plug);

	name = g_markup_escape_text(purple_plugin_info_get_name(info), -1);
	version = g_markup_escape_text(purple_plugin_info_get_version(info), -1);
	buf = g_strdup_printf(
		"<span size=\"larger\" weight=\"bold\">%s</span> "
		"<span size=\"smaller\">%s</span>",
		name, version);
	gtk_label_set_markup(plugin_name, buf);
	g_free(name);
	g_free(version);
	g_free(buf);

	gtk_text_buffer_set_text(plugin_desc, purple_plugin_info_get_description(info), -1);

	authorlist = purple_plugin_info_get_authors(info);
	if (authorlist)
		authors = g_strjoinv(",\n", (gchar **)authorlist);
	gtk_label_set_text(plugin_authors, authors);
	g_free(authors);

	gtk_label_set_text(plugin_filename, purple_plugin_get_filename(plug));

	g_free(plugin_website_uri);
	plugin_website_uri = g_strdup(purple_plugin_info_get_website(info));

	if (plugin_website_uri)
	{
		tmp = g_markup_escape_text(plugin_website_uri, -1);
		buf = g_strdup_printf("<span underline=\"single\" "
			"foreground=\"blue\">%s</span>", tmp);
		gtk_label_set_markup(plugin_website, buf);
		g_free(tmp);
		g_free(buf);
	}
	else
	{
		gtk_label_set_text(plugin_website, NULL);
	}

	if (purple_plugin_info_get_error(info) == NULL)
	{
		gtk_label_set_text(plugin_error, NULL);
	}
	else
	{
		tmp = g_markup_escape_text(purple_plugin_info_get_error(info), -1);
		buf = g_strdup_printf(
			_("<span foreground=\"red\" weight=\"bold\">"
			  "Error: %s\n"
			  "Check the plugin website for an update."
			  "</span>"),
			tmp);
		gtk_label_set_markup(plugin_error, buf);
		g_free(buf);
		g_free(tmp);
	}

	gtk_widget_set_sensitive(pref_button, pidgin_plugin_has_prefs(plug));

	/* Make sure the selected plugin is still visible */
	g_idle_add(ensure_plugin_visible, sel);

	g_value_unset(&val);
}

static void plugin_dialog_response_cb(GtkWidget *d, int response, GtkTreeSelection *sel)
{
	PurplePlugin *plugin;
	GtkTreeModel *model;
	GValue val;
	GtkTreeIter iter;
	GList *list, *it;

	g_return_if_fail(d != NULL);

	switch (response) {
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		purple_request_close_with_handle(plugin_dialog);
		purple_signals_disconnect_by_handle(plugin_dialog);
		list = purple_plugins_find_all();
		for (it = list; it; it = g_list_next(it))
			pref_dialog_close(it->data);
		g_list_free(list);
		gtk_widget_destroy(d);
		plugin_dialog = NULL;
		break;
	case PIDGIN_RESPONSE_CONFIGURE:
		if (! gtk_tree_selection_get_selected (sel, &model, &iter))
			return;
		val.g_type = 0;
		gtk_tree_model_get_value(model, &iter, 2, &val);
		plugin = g_value_get_pointer(&val);
		g_value_unset(&val);
		if (plugin == NULL)
			break;

		pidgin_plugin_open_config(plugin, GTK_WINDOW(d));

		break;
	}
}

static void
show_plugin_prefs_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, GtkWidget *dialog)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	PurplePlugin *plugin;
	GtkTreeModel *model;

	sel = gtk_tree_view_get_selection(view);

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, 2, &plugin, -1);

	if (!purple_plugin_is_loaded(plugin))
		return;

	/* Now show the pref-dialog for the plugin */
	plugin_dialog_response_cb(dialog, PIDGIN_RESPONSE_CONFIGURE, sel);
}

static gboolean
pidgin_plugins_paint_tooltip(GtkWidget *tipwindow, cairo_t *cr, gpointer data)
{
	PangoLayout *layout = g_object_get_data(G_OBJECT(tipwindow), "tooltip-plugin");
	GtkStyleContext *context = gtk_widget_get_style_context(tipwindow);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_TOOLTIP);
	gtk_render_layout(context, cr, 6, 6, layout);
	return TRUE;
}

static gboolean
pidgin_plugins_create_tooltip(GtkWidget *tipwindow, GtkTreePath *path,
		gpointer data, int *w, int *h)
{
	GtkTreeIter iter;
	GtkTreeView *treeview = GTK_TREE_VIEW(data);
	PurplePlugin *plugin = NULL;
	PurplePluginInfo *info;
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	PangoLayout *layout;
	int width, height;
	const char * const *authorlist;
	char *markup, *name, *desc;
	char *authors = NULL, *pauthors = NULL;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return FALSE;

	gtk_tree_model_get(model, &iter, 2, &plugin, -1);
	info = purple_plugin_get_info(plugin);
	authorlist = purple_plugin_info_get_authors(info);

	if (authorlist)
		authors = g_strjoinv(", ", (gchar **)authorlist);
	if (authors)
		pauthors = g_markup_escape_text(authors, -1);

	markup = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>\n<b>%s:</b> %s\n<b>%s:</b> %s",
			name = g_markup_escape_text(purple_plugin_info_get_name(info), -1),
			_("Description"), desc = g_markup_escape_text(purple_plugin_info_get_description(info), -1),
			(g_strv_length((gchar **)authorlist) > 1 ? _("Authors") : _("Author")), pauthors);

	layout = gtk_widget_create_pango_layout(tipwindow, NULL);
	pango_layout_set_markup(layout, markup, -1);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_width(layout, 600000);
	pango_layout_get_size(layout, &width, &height);
	g_object_set_data_full(G_OBJECT(tipwindow), "tooltip-plugin", layout, g_object_unref);

	if (w)
		*w = PANGO_PIXELS(width) + 12;
	if (h)
		*h = PANGO_PIXELS(height) + 12;

	g_free(markup);
	g_free(name);
	g_free(desc);
	g_free(pauthors);
	g_free(authors);

	return TRUE;
}

static gboolean
website_button_motion_cb(GtkWidget *button, GdkEventCrossing *event,
                          gpointer unused)
{
	if (plugin_website_uri) {
		pidgin_set_cursor(button, GDK_HAND2);
		return TRUE;
	}
	return FALSE;
}

static gboolean
website_button_clicked_cb(GtkButton *button, GdkEventButton *event,
                          gpointer unused)
{
	if (plugin_website_uri) {
		purple_notify_uri(NULL, plugin_website_uri);
		return TRUE;
	}
	return FALSE;
}

static GtkWidget *
create_details()
{
	GtkBox *vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 3));
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkWidget *label, *view, *website_button;

	plugin_name = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_xalign(plugin_name, 0);
	gtk_label_set_yalign(plugin_name, 0);
	gtk_label_set_line_wrap(plugin_name, FALSE);
	gtk_label_set_selectable(plugin_name, TRUE);
	gtk_box_pack_start(vbox, GTK_WIDGET(plugin_name), FALSE, FALSE, 0);

	view = gtk_text_view_new();
	plugin_desc = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	g_object_set(view, "wrap-mode", GTK_WRAP_WORD,
	                   "editable",  FALSE,
	                   "left-margin",  PIDGIN_HIG_CAT_SPACE,
	                   "right-margin", PIDGIN_HIG_CAT_SPACE,
	                   NULL);
	gtk_box_pack_start(vbox, view, TRUE, TRUE, 0);

	plugin_error = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_xalign(plugin_error, 0);
	gtk_label_set_yalign(plugin_error, 0);
	gtk_label_set_line_wrap(plugin_error, FALSE);
	gtk_label_set_selectable(plugin_error, TRUE);
	gtk_box_pack_start(vbox, GTK_WIDGET(plugin_error), FALSE, FALSE, 0);

	plugin_authors = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_line_wrap(plugin_authors, FALSE);
	gtk_label_set_xalign(plugin_authors, 0);
	gtk_label_set_yalign(plugin_authors, 0);
	gtk_label_set_selectable(plugin_authors, TRUE);
	pidgin_add_widget_to_vbox(vbox, "", sg,
		GTK_WIDGET(plugin_authors), TRUE, &label);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Written by:</b>"));
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);

	website_button = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(website_button), FALSE);

	plugin_website = GTK_LABEL(gtk_label_new(NULL));
	g_object_set(G_OBJECT(plugin_website),
		"ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	gtk_label_set_xalign(plugin_website, 0);
	gtk_label_set_yalign(plugin_website, 0);
	gtk_container_add(GTK_CONTAINER(website_button),
		GTK_WIDGET(plugin_website));
	g_signal_connect(website_button, "button-release-event",
		G_CALLBACK(website_button_clicked_cb), NULL);
	g_signal_connect(website_button, "enter-notify-event",
		G_CALLBACK(website_button_motion_cb), NULL);
	g_signal_connect(website_button, "leave-notify-event",
		G_CALLBACK(pidgin_clear_cursor), NULL);

	pidgin_add_widget_to_vbox(vbox, "", sg, website_button, TRUE, &label);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Web site:</b>"));
	gtk_label_set_xalign(GTK_LABEL(label), 0);

	plugin_filename = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_line_wrap(plugin_filename, FALSE);
	gtk_label_set_xalign(plugin_filename, 0);
	gtk_label_set_yalign(plugin_filename, 0);
	gtk_label_set_selectable(plugin_filename, TRUE);
	pidgin_add_widget_to_vbox(vbox, "", sg,
		GTK_WIDGET(plugin_filename), TRUE, &label);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Filename:</b>"));
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);

	g_object_unref(sg);

	return GTK_WIDGET(vbox);
}


void pidgin_plugin_dialog_show()
{
	GtkWidget *event_view;
	GtkListStore *ls;
	GtkCellRenderer *rend, *rendt;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	if (plugin_dialog != NULL) {
		gtk_window_present(GTK_WINDOW(plugin_dialog));
		return;
	}

	plugin_dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(plugin_dialog), _("Plugins"));
	pref_button = gtk_dialog_add_button(GTK_DIALOG(plugin_dialog),
						_("Configure Pl_ugin"), PIDGIN_RESPONSE_CONFIGURE);
	gtk_dialog_add_button(GTK_DIALOG(plugin_dialog),
						GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_widget_set_sensitive(pref_button, FALSE);
	gtk_window_set_role(GTK_WINDOW(plugin_dialog), "plugins");

	ls = gtk_list_store_new(4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),
					     1, GTK_SORT_ASCENDING);

	update_plugin_list(ls);

	event_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(event_view), TRUE);

	g_signal_connect(G_OBJECT(event_view), "row-activated",
				G_CALLBACK(show_plugin_prefs_cb), plugin_dialog);

	purple_signal_connect(purple_plugins_get_handle(), "plugin-load", plugin_dialog,
	                    PURPLE_CALLBACK(plugin_load_cb), event_view);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload", plugin_dialog,
	                    PURPLE_CALLBACK(plugin_unload_cb), event_view);

	rend = gtk_cell_renderer_toggle_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));

	col = gtk_tree_view_column_new_with_attributes (_("Enabled"),
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 0);
	g_signal_connect(G_OBJECT(rend), "toggled",
			 G_CALLBACK(plugin_toggled), ls);

	rendt = gtk_cell_renderer_text_new();
	g_object_set(rendt,
		     "foreground", "#c0c0c0",
		     NULL);
	col = gtk_tree_view_column_new_with_attributes (_("Name"),
							rendt,
							"markup", 1,
							"foreground-set", 3,
							NULL);
	gtk_tree_view_column_set_expand (col, TRUE);
	g_object_set(rendt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_column_set_sort_column_id(col, 1);
	g_object_unref(G_OBJECT(ls));
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(plugin_dialog))),
		pidgin_make_scrollable(event_view, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1),
		TRUE, TRUE, 0);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(event_view), 1);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(event_view),
				pidgin_tree_view_search_equal_func, NULL, NULL);

	pidgin_tooltip_setup_for_treeview(event_view, event_view,
			pidgin_plugins_create_tooltip,
			pidgin_plugins_paint_tooltip);


	expander = gtk_expander_new(_("<b>Plugin Details</b>"));
	gtk_expander_set_use_markup(GTK_EXPANDER(expander), TRUE);
	gtk_widget_set_sensitive(expander, FALSE);
	gtk_container_add(GTK_CONTAINER(expander), create_details());
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(plugin_dialog))),
	                   expander, FALSE, FALSE, 0);


	g_signal_connect (G_OBJECT (sel), "changed", G_CALLBACK (prefs_plugin_sel), NULL);
	g_signal_connect(G_OBJECT(plugin_dialog), "response", G_CALLBACK(plugin_dialog_response_cb), sel);
	gtk_window_set_default_size(GTK_WINDOW(plugin_dialog), 430, 530);

	pidgin_auto_parent_window(plugin_dialog);

	gtk_widget_show_all(plugin_dialog);
}
