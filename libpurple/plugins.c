/*
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

#include "core.h"
#include "debug.h"
#include "enums.h"
#include "plugins.h"

typedef struct _PurplePluginInfoPrivate  PurplePluginInfoPrivate;

/**************************************************************************
 * Plugin info private data
 **************************************************************************/
struct _PurplePluginInfoPrivate {
	char *ui_requirement;  /* ID of UI that is required to load the plugin */
	char *error;           /* Why a plugin is not loadable                 */

	PurplePluginInfoFlags flags; /* Flags for the plugin */

	/* Callback that returns a list of actions the plugin can perform */
	PurplePluginActionsCb actions_cb;

	/* Callback that returns extra information about a plugin */
	PurplePluginExtraCb extra_cb;

	/* Callback that returns a preferences frame for a plugin */
	PurplePluginPrefFrameCb pref_frame_cb;

	/* Callback that returns a preferences request handle for a plugin */
	PurplePluginPrefRequestCb pref_request_cb;

	/* TRUE if a plugin has been unloaded at least once. Auto-load
	 * plugins that have been unloaded once will not be auto-loaded again. */
	gboolean unloaded;
};

enum
{
	PROP_0,
	PROP_UI_REQUIREMENT,
	PROP_ACTIONS_CB,
	PROP_EXTRA_CB,
	PROP_PREF_FRAME_CB,
	PROP_PREF_REQUEST_CB,
	PROP_FLAGS,
	PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE(PurplePluginInfo, purple_plugin_info,
		GPLUGIN_TYPE_PLUGIN_INFO);

/**************************************************************************
 * Globals
 **************************************************************************/
static GList *loaded_plugins     = NULL;
static GList *plugins_to_disable = NULL;

/**************************************************************************
 * Plugin API
 **************************************************************************/
static gboolean
plugin_loading_cb(GObject *manager, PurplePlugin *plugin, GError **error,
                  gpointer data)
{
	PurplePluginInfo *info;
	PurplePluginInfoPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_PLUGIN(plugin), FALSE);

	info = purple_plugin_get_info(plugin);
	if (!info)
		return TRUE; /* a GPlugin internal plugin */

	priv = purple_plugin_info_get_instance_private(info);

	if (priv->error) {
		gchar *filename = gplugin_plugin_get_filename(plugin);
		purple_debug_error("plugins", "Failed to load plugin %s: %s",
		                   filename,
		                   priv->error);

		g_set_error(error, PURPLE_PLUGINS_DOMAIN, 0,
				    "Plugin is not loadable: %s", priv->error);

		g_free(filename);
		return FALSE;
	}

	return TRUE;
}

static void
plugin_loaded_cb(GObject *manager, PurplePlugin *plugin)
{
	PurplePluginInfo *info;
	gchar *filename;

	g_return_if_fail(PURPLE_IS_PLUGIN(plugin));

	info = purple_plugin_get_info(plugin);
	if (!info)
		return; /* a GPlugin internal plugin */

	loaded_plugins = g_list_prepend(loaded_plugins, plugin);
	filename = gplugin_plugin_get_filename(plugin);

	purple_debug_info("plugins", "Loaded plugin %s\n", filename);

	purple_signal_emit(purple_plugins_get_handle(), "plugin-load", plugin);
	g_free(filename);
}

static gboolean
plugin_unloading_cb(GObject *manager, PurplePlugin *plugin, GError **error,
                    gpointer data)
{
	PurplePluginInfo *info;
	gchar *filename;

	g_return_val_if_fail(PURPLE_IS_PLUGIN(plugin), FALSE);

	info = purple_plugin_get_info(plugin);
	if (info) {
		filename = gplugin_plugin_get_filename(plugin);
		purple_debug_info("plugins", "Unloading plugin %s\n",
		                  filename);
		g_free(filename);
	}

	return TRUE;
}

static void
plugin_unloaded_cb(GObject *manager, PurplePlugin *plugin)
{
	PurplePluginInfo *info;
	PurplePluginInfoPrivate *priv;

	g_return_if_fail(PURPLE_IS_PLUGIN(plugin));

	info = purple_plugin_get_info(plugin);
	if (!info)
		return; /* a GPlugin internal plugin */

	priv = purple_plugin_info_get_instance_private(info);

	/* cancel any pending dialogs the plugin has */
	purple_request_close_with_handle(plugin);
	purple_notify_close_with_handle(plugin);

	purple_signals_disconnect_by_handle(plugin);
	purple_signals_unregister_by_instance(plugin);

	priv->unloaded = TRUE;

	loaded_plugins     = g_list_remove(loaded_plugins, plugin);
	plugins_to_disable = g_list_remove(plugins_to_disable, plugin);

	purple_signal_emit(purple_plugins_get_handle(), "plugin-unload", plugin);

	purple_prefs_disconnect_by_handle(plugin);
}

gboolean
purple_plugin_load(PurplePlugin *plugin, GError **error)
{
	GError *err = NULL;
	gchar *filename;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (purple_plugin_is_loaded(plugin))
		return TRUE;

	if (!gplugin_manager_load_plugin(plugin, &err)) {
	        filename = gplugin_plugin_get_filename(plugin);
		purple_debug_error("plugins", "Failed to load plugin %s: %s",
		                   filename,
		                   err ? err->message : "Unknown reason");

		if (error)
			*error = g_error_copy(err);
		g_error_free(err);
		g_free(filename);
		return FALSE;
	}

	return TRUE;
}

gboolean
purple_plugin_unload(PurplePlugin *plugin, GError **error)
{
	GError *err = NULL;
	gchar *filename;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (!purple_plugin_is_loaded(plugin))
		return TRUE;

	if (!gplugin_manager_unload_plugin(plugin, &err)) {
	        filename = gplugin_plugin_get_filename(plugin);
		purple_debug_error("plugins", "Failed to unload plugin %s: %s",
		                   filename,
		                   err ? err->message : "Unknown reason");

		if (error)
			*error = g_error_copy(err);
		g_error_free(err);
		g_free(filename);

		return FALSE;
	}

	return TRUE;
}

gboolean
purple_plugin_is_loaded(PurplePlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	return (gplugin_plugin_get_state(plugin) == GPLUGIN_PLUGIN_STATE_LOADED);
}

PurplePluginInfo *
purple_plugin_get_info(PurplePlugin *plugin)
{
	GPluginPluginInfo *info;

	g_return_val_if_fail(plugin != NULL, NULL);

	info = gplugin_plugin_get_info(plugin);

	/* GPlugin refs the plugin info object before returning it. This workaround
	 * is to avoid managing the reference counts everywhere in our codebase
	 * where we use the plugin info. The plugin info instance is guaranteed to
	 * exist as long as the plugin exists. */
	g_object_unref(info);

	if (PURPLE_IS_PLUGIN_INFO(info))
		return PURPLE_PLUGIN_INFO(info);
	else
		return NULL;
}

void
purple_plugin_disable(PurplePlugin *plugin)
{
	g_return_if_fail(plugin != NULL);

	if (!g_list_find(plugins_to_disable, plugin))
		plugins_to_disable = g_list_prepend(plugins_to_disable, plugin);
}

gboolean
purple_plugin_is_internal(PurplePlugin *plugin)
{
	PurplePluginInfo *info;

	g_return_val_if_fail(plugin != NULL, FALSE);

	info = purple_plugin_get_info(plugin);
	if (!info)
		return TRUE;

	return (purple_plugin_info_get_flags(info) &
	        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL);
}

GSList *
purple_plugin_get_dependent_plugins(PurplePlugin *plugin)
{
#warning TODO: Implement this when GPlugin can return dependent plugins.
	return NULL;
}

/**************************************************************************
 * GObject code for PurplePluginInfo
 **************************************************************************/
/* GObject initialization function */
static void
purple_plugin_info_init(PurplePluginInfo *info)
{
}

/* Set method for GObject properties */
static void
purple_plugin_info_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(obj);
	PurplePluginInfoPrivate *priv =
			purple_plugin_info_get_instance_private(info);

	switch (param_id) {
		case PROP_UI_REQUIREMENT:
			priv->ui_requirement = g_value_dup_string(value);
			break;
		case PROP_ACTIONS_CB:
			priv->actions_cb = g_value_get_pointer(value);
			break;
		case PROP_EXTRA_CB:
			priv->extra_cb = g_value_get_pointer(value);
			break;
		case PROP_PREF_FRAME_CB:
			priv->pref_frame_cb = g_value_get_pointer(value);
			break;
		case PROP_PREF_REQUEST_CB:
			priv->pref_request_cb = g_value_get_pointer(value);
			break;
		case PROP_FLAGS:
			priv->flags = g_value_get_flags(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_plugin_info_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_ACTIONS_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_actions_cb(info));
			break;
		case PROP_EXTRA_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_extra_cb(info));
			break;
		case PROP_PREF_FRAME_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_pref_frame_cb(info));
			break;
		case PROP_PREF_REQUEST_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_pref_request_cb(info));
			break;
		case PROP_FLAGS:
			g_value_set_flags(value, purple_plugin_info_get_flags(info));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_plugin_info_constructed(GObject *object)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(object);
	GPluginPluginInfo *ginfo = GPLUGIN_PLUGIN_INFO(info);
	PurplePluginInfoPrivate *priv =
			purple_plugin_info_get_instance_private(info);
	const char *id = gplugin_plugin_info_get_id(ginfo);
	guint32 version;

	G_OBJECT_CLASS(purple_plugin_info_parent_class)->constructed(object);

	if (id == NULL || *id == '\0')
		priv->error = g_strdup(_("This plugin has not defined an ID."));

	if (priv->ui_requirement && !purple_strequal(priv->ui_requirement, purple_core_get_ui()))
	{
		priv->error = g_strdup_printf(_("You are using %s, but this plugin requires %s."),
				purple_core_get_ui(), priv->ui_requirement);
		purple_debug_error("plugins", "%s is not loadable: The UI requirement is not met. (%s)\n",
				id, priv->error);
	}

	version = gplugin_plugin_info_get_abi_version(ginfo);
	if (PURPLE_PLUGIN_ABI_MAJOR_VERSION(version) != PURPLE_MAJOR_VERSION ||
		PURPLE_PLUGIN_ABI_MINOR_VERSION(version) > PURPLE_MINOR_VERSION)
	{
		priv->error = g_strdup_printf(_("Your libpurple version is %d.%d.x (need %d.%d.x)"),
				PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION,
				PURPLE_PLUGIN_ABI_MAJOR_VERSION(version),
				PURPLE_PLUGIN_ABI_MINOR_VERSION(version));
		purple_debug_error("plugins", "%s is not loadable: libpurple version is %d.%d.x (need %d.%d.x)\n",
				id, PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION,
				PURPLE_PLUGIN_ABI_MAJOR_VERSION(version),
				PURPLE_PLUGIN_ABI_MINOR_VERSION(version));
	}
}

/* GObject finalize function */
static void
purple_plugin_info_finalize(GObject *object)
{
	PurplePluginInfoPrivate *priv =
			purple_plugin_info_get_instance_private(
					PURPLE_PLUGIN_INFO(object));

	g_free(priv->ui_requirement);
	g_free(priv->error);

	G_OBJECT_CLASS(purple_plugin_info_parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_plugin_info_class_init(PurplePluginInfoClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->constructed = purple_plugin_info_constructed;
	obj_class->finalize    = purple_plugin_info_finalize;

	/* Setup properties */
	obj_class->get_property = purple_plugin_info_get_property;
	obj_class->set_property = purple_plugin_info_set_property;

	g_object_class_install_property(obj_class, PROP_UI_REQUIREMENT,
		g_param_spec_string("ui-requirement",
		                  "UI Requirement",
		                  "ID of UI that is required by this plugin", NULL,
		                  G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(obj_class, PROP_ACTIONS_CB,
		g_param_spec_pointer("actions-cb",
		                  "Plugin actions",
		                  "Callback that returns list of plugin's actions",
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(obj_class, PROP_EXTRA_CB,
		g_param_spec_pointer("extra-cb",
		                  "Extra info callback",
		                  "Callback that returns extra info about the plugin",
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(obj_class, PROP_PREF_FRAME_CB,
		g_param_spec_pointer("pref-frame-cb",
		                  "Preferences frame callback",
		                  "The callback that returns the preferences frame",
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(obj_class, PROP_PREF_REQUEST_CB,
		g_param_spec_pointer("pref-request-cb",
		                  "Preferences request callback",
		                  "Callback that returns preferences request handle",
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                  G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(obj_class, PROP_FLAGS,
		g_param_spec_flags("flags",
		                  "Plugin flags",
		                  "The flags for the plugin",
		                  PURPLE_TYPE_PLUGIN_INFO_FLAGS, 0,
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                  G_PARAM_STATIC_STRINGS));
}

/**************************************************************************
 * PluginInfo API
 **************************************************************************/
PurplePluginInfo *
purple_plugin_info_new(const char *first_property, ...)
{
	GObject *info;
	va_list var_args;

	/* at least ID is required */
	if (!first_property)
		return NULL;

	va_start(var_args, first_property);
	info = g_object_new_valist(PURPLE_TYPE_PLUGIN_INFO, first_property,
	                           var_args);
	va_end(var_args);

	return PURPLE_PLUGIN_INFO(info);
}

PurplePluginActionsCb
purple_plugin_info_get_actions_cb(PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);
	return priv->actions_cb;
}

PurplePluginExtraCb
purple_plugin_info_get_extra_cb(PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);
	return priv->extra_cb;
}

PurplePluginPrefFrameCb
purple_plugin_info_get_pref_frame_cb(PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);
	return priv->pref_frame_cb;
}

PurplePluginPrefRequestCb
purple_plugin_info_get_pref_request_cb(PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);
	return priv->pref_request_cb;
}

PurplePluginInfoFlags
purple_plugin_info_get_flags(PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), 0);

	priv = purple_plugin_info_get_instance_private(info);
	return priv->flags;
}

const gchar *
purple_plugin_info_get_error(PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);
	return priv->error;
}

void
purple_plugin_info_set_ui_data(PurplePluginInfo *info, gpointer ui_data)
{
	g_return_if_fail(PURPLE_IS_PLUGIN_INFO(info));

	info->ui_data = ui_data;
}

gpointer
purple_plugin_info_get_ui_data(PurplePluginInfo *info)
{
	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	return info->ui_data;
}

/**************************************************************************
 * PluginAction API
 **************************************************************************/
PurplePluginAction *
purple_plugin_action_new(const char* label, PurplePluginActionCb callback)
{
	PurplePluginAction *action;

	g_return_val_if_fail(label != NULL && callback != NULL, NULL);

	action = g_new0(PurplePluginAction, 1);

	action->label    = g_strdup(label);
	action->callback = callback;

	return action;
}

void
purple_plugin_action_free(PurplePluginAction *action)
{
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_free(action);
}

static PurplePluginAction *
purple_plugin_action_copy(PurplePluginAction *action)
{
	g_return_val_if_fail(action != NULL, NULL);

	return purple_plugin_action_new(action->label, action->callback);
}

G_DEFINE_BOXED_TYPE(PurplePluginAction, purple_plugin_action,
                    purple_plugin_action_copy, purple_plugin_action_free)

/**************************************************************************
 * Plugins API
 **************************************************************************/
GList *
purple_plugins_find_all(void)
{
	GList *ret = NULL, *ids, *l;
	GSList *plugins, *ll;

	ids = gplugin_manager_list_plugins();

	for (l = ids; l; l = l->next) {
		plugins = gplugin_manager_find_plugins(l->data);

		for (ll = plugins; ll; ll = ll->next) {
			PurplePlugin *plugin = PURPLE_PLUGIN(ll->data);
			if (purple_plugin_get_info(plugin))
				ret = g_list_append(ret, plugin);
		}

		gplugin_manager_free_plugin_list(plugins);
	}
	g_list_free(ids);

	return ret;
}

GList *
purple_plugins_get_loaded(void)
{
	return loaded_plugins;
}

void
purple_plugins_add_search_path(const gchar *path)
{
	gplugin_manager_append_path(path);
}

void
purple_plugins_refresh(void)
{
	GList *plugins, *l;

	gplugin_manager_refresh();

	plugins = purple_plugins_find_all();
	for (l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);
		PurplePluginInfo *info;
		PurplePluginInfoPrivate *priv;

		if (purple_plugin_is_loaded(plugin))
			continue;

		info = purple_plugin_get_info(plugin);
		priv = purple_plugin_info_get_instance_private(info);

		if (!priv->unloaded && purple_plugin_info_get_flags(info) &
				PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD) {
			gchar *filename = gplugin_plugin_get_filename(plugin);
			purple_debug_info("plugins", "Auto-loading plugin %s\n",
			                  filename);
			purple_plugin_load(plugin, NULL);
			g_free(filename);
		}
	}

	g_list_free(plugins);
}

PurplePlugin *
purple_plugins_find_plugin(const gchar *id)
{
	PurplePlugin *plugin;

	g_return_val_if_fail(id != NULL && *id != '\0', NULL);

	plugin = gplugin_manager_find_plugin(id);

	if (!plugin)
		return NULL;

	/* GPlugin refs the plugin object before returning it. This workaround is
	 * to avoid managing the reference counts everywhere in our codebase where
	 * we use plugin instances. A plugin object will exist till the plugins
	 * subsystem is uninitialized. */
	g_object_unref(plugin);

	return plugin;
}

PurplePlugin *
purple_plugins_find_by_filename(const char *filename)
{
	GList *plugins, *l;

	g_return_val_if_fail(filename != NULL && *filename != '\0', NULL);

	plugins = purple_plugins_find_all();

	for (l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);
		gchar *plugin_filename = gplugin_plugin_get_filename(plugin);

		if (purple_strequal(plugin_filename, filename)) {
			g_list_free(plugins);
			g_free(plugin_filename);
			return plugin;
		}
		g_free(plugin_filename);
	}
	g_list_free(plugins);

	return NULL;
}

void
purple_plugins_save_loaded(const char *key)
{
	GList *pl;
	GList *files = NULL;

	g_return_if_fail(key != NULL && *key != '\0');

	for (pl = purple_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(pl->data);
		PurplePluginInfo *info = purple_plugin_get_info(plugin);
		if (!info)
			continue;

		if (purple_plugin_info_get_flags(info) &
				PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD)
			continue;

		if (!g_list_find(plugins_to_disable, plugin))
			files = g_list_append(
			        files,
			        (gchar *)gplugin_plugin_get_filename(plugin));
	}

	purple_prefs_set_path_list(key, files);
	g_list_free_full(files, g_free);
}

void
purple_plugins_load_saved(const char *key)
{
	GList *l, *files;

	g_return_if_fail(key != NULL && *key != '\0');

	files = purple_prefs_get_path_list(key);

	for (l = files; l; l = l->next)
	{
		char *file;
		PurplePlugin *plugin;

		if (l->data == NULL)
			continue;

		file = l->data;
		plugin = purple_plugins_find_by_filename(file);

		if (plugin) {
			purple_debug_info("plugins", "Loading saved plugin %s\n", file);
			purple_plugin_load(plugin, NULL);
		} else {
			purple_debug_error("plugins", "Unable to find saved plugin %s\n", file);
		}

		g_free(l->data);
	}

	g_list_free(files);
}

/**************************************************************************
 * Plugins Subsystem API
 **************************************************************************/
void *
purple_plugins_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_plugins_init(void)
{
	void *handle = purple_plugins_get_handle();
	const gchar *search_path;

	purple_signal_register(handle, "plugin-load",
	                       purple_marshal_VOID__POINTER,
	                       G_TYPE_NONE, 1, PURPLE_TYPE_PLUGIN);
	purple_signal_register(handle, "plugin-unload",
	                       purple_marshal_VOID__POINTER,
	                       G_TYPE_NONE, 1, PURPLE_TYPE_PLUGIN);

	gplugin_init();

	search_path = g_getenv("PURPLE_PLUGIN_PATH");
	if (search_path) {
		gchar **paths;
		int i;

		paths = g_strsplit(search_path, G_SEARCHPATH_SEPARATOR_S, 0);
		for (i = 0; paths[i]; ++i) {
			purple_plugins_add_search_path(paths[i]);
		}

		g_strfreev(paths);
	}

	gplugin_manager_add_default_paths();

	if(!g_getenv("PURPLE_PLUGINS_SKIP")) {
		purple_plugins_add_search_path(PURPLE_LIBDIR);
	} else {
		purple_debug_info("plugins", "PURPLE_PLUGINS_SKIP environment variable set, skipping normal plugin paths");
	}

	g_signal_connect(gplugin_manager_get_instance(), "loading-plugin",
	                 G_CALLBACK(plugin_loading_cb), NULL);
	g_signal_connect(gplugin_manager_get_instance(), "loaded-plugin",
	                 G_CALLBACK(plugin_loaded_cb), NULL);
	g_signal_connect(gplugin_manager_get_instance(), "unloading-plugin",
	                 G_CALLBACK(plugin_unloading_cb), NULL);
	g_signal_connect(gplugin_manager_get_instance(), "unloaded-plugin",
	                 G_CALLBACK(plugin_unloaded_cb), NULL);

	purple_plugins_refresh();
}

void
purple_plugins_uninit(void)
{
	void *handle = purple_plugins_get_handle();

	purple_debug_info("plugins", "Unloading all plugins\n");
	while (loaded_plugins != NULL)
		purple_plugin_unload(loaded_plugins->data, NULL);

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);

	gplugin_uninit();
}
