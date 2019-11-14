/* purple
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

#ifndef PURPLE_PLUGINS_H
#define PURPLE_PLUGINS_H
/**
 * SECTION:plugins
 * @section_id: libpurple-plugins
 * @short_description: <filename>plugins.h</filename>
 * @title: Plugin API
 * @see_also: <link linkend="chapter-signals-plugin">Plugin signals</link>,
 *     <link linkend="chapter-plugin-ids">Plugin IDs</link>,
 *     <link linkend="chapter-plugin-i18n">Third Party Plugin Translation</link>
 */

#include <gplugin.h>
#include <gplugin-native.h>

#include "version.h"

#define PURPLE_PLUGINS_DOMAIN          (g_quark_from_static_string("plugins"))

#define PURPLE_TYPE_PLUGIN             GPLUGIN_TYPE_PLUGIN
#define PURPLE_PLUGIN(obj)             GPLUGIN_PLUGIN(obj)
#define PURPLE_IS_PLUGIN(obj)          GPLUGIN_IS_PLUGIN(obj)
#define PURPLE_PLUGIN_GET_IFACE(obj)   GPLUGIN_PLUGIN_GET_IFACE(obj)

/**
 * PurplePlugin:
 *
 * Represents a plugin handle.
 * This type is an alias for GPluginPlugin.
 */
typedef GPluginPlugin PurplePlugin;

typedef GPluginPluginInterface PurplePluginInterface;

#define PURPLE_TYPE_PLUGIN_INFO             (purple_plugin_info_get_type())
typedef struct _PurplePluginInfo PurplePluginInfo;

#define PURPLE_TYPE_PLUGIN_ACTION  (purple_plugin_action_get_type())
typedef struct _PurplePluginAction PurplePluginAction;

#include "pluginpref.h"

/**
 * PurplePluginActionCb:
 * @action: the action information.
 *
 * A function called when the related Action Menu is activated.
 */
typedef void (*PurplePluginActionCb)(PurplePluginAction *action);

/**
 * PurplePluginActionsCb:
 * @plugin: the plugin associated with this callback.
 *
 * Returns a list of actions the plugin can perform.
 *
 * Returns: (transfer none): A list of actions the plugin can perform.
 */
typedef GList *(*PurplePluginActionsCb)(PurplePlugin *plugin);

/**
 * PurplePluginExtraCb:
 * @plugin: the plugin associated with this callback.
 *
 * Gives extra information about the plguin.
 *
 * Returns: a newly allocated string denoting extra information
 * about a plugin.
 */
typedef gchar *(*PurplePluginExtraCb)(PurplePlugin *plugin);

/**
 * PurplePluginPrefFrameCb:
 * @plugin: the plugin associated with this callback.
 *
 * Returns the preferences frame for the plugin.
 *
 * Returns: Preference frame.
 */
typedef PurplePluginPrefFrame *(*PurplePluginPrefFrameCb)(PurplePlugin *plugin);

/**
 * PurplePrefRequestCb:
 *
 * Returns the preferences request handle for a plugin.
 *
 * Returns: Preferences request handle.
 */
typedef gpointer (*PurplePluginPrefRequestCb)(PurplePlugin *plugin);

/**
 * PurplePluginInfoFlags:
 * @PURPLE_PLUGIN_INFO_FLAGS_INTERNAL:  Plugin is not shown in UI lists
 * @PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD: Auto-load the plugin
 *
 * Flags that can be used to treat plugins differently.
 */
typedef enum /*< flags >*/
{
	PURPLE_PLUGIN_INFO_FLAGS_INTERNAL  = 1 << 1,
	PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD = 1 << 2,

} PurplePluginInfoFlags;

/**
 * PurplePluginInfo:
 * @ui_data: The UI data associated with the plugin. This is a convenience
 *           field provided to the UIs -- it is not used by the libpurple core.
 *
 * Holds information about a plugin.
 */
struct _PurplePluginInfo {
	GPluginPluginInfo parent;

	/*< public >*/
	gpointer ui_data;
};

/**
 * PurplePluginAction:
 * @label: The label to display in the user interface.
 * @callback: The function to call when the user wants to perform this action.
 * @plugin: The plugin that this action belongs to.
 * @user_data: User data to pass to @callback.
 *
 * Represents an action that the plugin can perform. This shows up in the Tools
 * menu, under a submenu with the name of the plugin.
 */
struct _PurplePluginAction {
	char *label;
	PurplePluginActionCb callback;
	PurplePlugin *plugin;
	gpointer user_data;
};

/**
 * PURPLE_PLUGIN_ABI_VERSION:
 *
 * Note: The lower six nibbles represent the ABI version for libpurple, the
 *       rest are required by GPlugin.
 *
 * Returns: An ABI version to set in plugins using major and minor versions.
 */
#define PURPLE_PLUGIN_ABI_VERSION(major,minor) \
	(0x01000000 | ((major) << 16) | (minor))

/**
 * PURPLE_PLUGIN_ABI_MAJOR_VERSION:
 *
 * Returns: The major version from an ABI version
 */
#define PURPLE_PLUGIN_ABI_MAJOR_VERSION(abi) \
	((abi >> 16) & 0xff)

/**
 * PURPLE_PLUGIN_ABI_MINOR_VERSION:
 *
 * Returns: The minor version from an ABI version
 */
#define PURPLE_PLUGIN_ABI_MINOR_VERSION(abi) \
	(abi & 0xffff)

/**
 * PURPLE_ABI_VERSION:
 *
 * A convenience‎ macro that returns an ABI version using PURPLE_MAJOR_VERSION
 * and PURPLE_MINOR_VERSION
 */
#define PURPLE_ABI_VERSION PURPLE_PLUGIN_ABI_VERSION(PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION)

/**
 * PURPLE_PLUGIN_INIT:
 *
 * Defines the plugin's entry points.
 */
#define PURPLE_PLUGIN_INIT(pluginname,pluginquery,pluginload,pluginunload) \
	G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **e); \
	G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **e) { \
		return GPLUGIN_PLUGIN_INFO(pluginquery(e)); \
	} \
	G_MODULE_EXPORT gboolean gplugin_load(GPluginNativePlugin *p, GError **e); \
	G_MODULE_EXPORT gboolean gplugin_load(GPluginNativePlugin *p, GError **e) { \
		return pluginload(PURPLE_PLUGIN(p), e); \
	} \
	G_MODULE_EXPORT gboolean gplugin_unload(GPluginNativePlugin *p, GError **e); \
	G_MODULE_EXPORT gboolean gplugin_unload(GPluginNativePlugin *p, GError **e) { \
		return pluginunload(PURPLE_PLUGIN(p), e); \
	}

G_BEGIN_DECLS

/**************************************************************************/
/* Plugin API                                                             */
/**************************************************************************/

/**
 * purple_plugin_load:
 * @plugin: The plugin to load.
 * @error:  Return location for a #GError or %NULL. If provided, this
 *               will be set to the reason if the load fails.
 *
 * Attempts to load a plugin.
 *
 * Also see purple_plugin_unload().
 *
 * Returns: %TRUE if successful or already loaded, %FALSE otherwise.
 */
gboolean purple_plugin_load(PurplePlugin *plugin, GError **error);

/**
 * purple_plugin_unload:
 * @plugin: The plugin handle.
 * @error:  Return location for a #GError or %NULL. If provided, this
 *               will be set to the reason if the unload fails.
 *
 * Unloads the specified plugin.
 *
 * Also see purple_plugin_load().
 *
 * Returns: %TRUE if successful or not loaded, %FALSE otherwise.
 */
gboolean purple_plugin_unload(PurplePlugin *plugin, GError **error);

/**
 * purple_plugin_is_loaded:
 * @plugin: The plugin.
 *
 * Returns whether or not a plugin is currently loaded.
 *
 * Returns: %TRUE if loaded, or %FALSE otherwise.
 */
gboolean purple_plugin_is_loaded(PurplePlugin *plugin);

/**
 * purple_plugin_get_info:
 * @plugin: The plugin.
 *
 * Returns a plugin's #PurplePluginInfo instance.
 *
 * Returns: (transfer none): The plugin's #PurplePluginInfo instance.
 * GPlugin refs the plugin info object before returning it. This workaround
 * is to avoid managing the reference counts everywhere in our codebase
 * where we use the plugin info. The plugin info instance is guaranteed to
 * exist as long as the plugin exists.
 */
PurplePluginInfo *purple_plugin_get_info(PurplePlugin *plugin);

/**
 * purple_plugin_disable:
 *
 * Disable a plugin.
 *
 * This function adds the plugin to a list of plugins to "disable at the next
 * startup" by excluding said plugins from the list of plugins to save.  The
 * UI needs to call purple_plugins_save_loaded() after calling this for it
 * to have any effect.
 */
void purple_plugin_disable(PurplePlugin *plugin);

/**
 * purple_plugin_is_internal:
 * @plugin: The plugin.
 *
 * Returns whether a plugin is an internal plugin. Internal plugins provide
 * required additional functionality to the libpurple core. These plugins must
 * not be shown in plugin lists. Examples of such plugins are in-tree protocol
 * plugins, loaders etc.
 *
 * Returns: %TRUE if the plugin is an internal plugin, %FALSE otherwise.
 */
gboolean purple_plugin_is_internal(PurplePlugin *plugin);

/**
 * purple_plugin_get_dependent_plugins:
 * @plugin: The plugin whose dependent plugins are returned.
 *
 * Returns a list of plugins that depend on a particular plugin.
 *
 * Returns: (element-type PurplePlugin) (transfer none): The list of a plugins that depend on the specified
 *                           plugin.
 */
GSList *purple_plugin_get_dependent_plugins(PurplePlugin *plugin);

/**************************************************************************/
/* PluginInfo API                                                         */
/**************************************************************************/

/**
 * purple_plugin_info_get_type:
 *
 * Returns: The #GType for the #PurplePluginInfo object.
 */
G_DECLARE_FINAL_TYPE(PurplePluginInfo, purple_plugin_info, PURPLE, PLUGIN_INFO,
                     GPluginPluginInfo)

/**
 * purple_plugin_info_new:
 * @first_property:  The first property name
 * @...:  The value of the first property, followed optionally by more
 *             name/value pairs, followed by %NULL
 *
 * Creates a new #PurplePluginInfo instance to be returned from
 * #plugin_query of a plugin, using the provided name/value pairs.
 *
 * All properties except <literal>"id"</literal> and
 * <literal>"purple-abi"</literal> are optional.
 *
 * Valid property names are:
 * <informaltable frame='none'>
 *   <tgroup cols='2'><tbody>
 *   <row><entry><literal>"id"</literal></entry>
 *     <entry>(string) The ID of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"abi-version"</literal></entry>
 *     <entry>(<type>guint32</type>) The ABI version required by the
 *       plugin.</entry>
 *   </row>
 *   <row><entry><literal>"name"</literal></entry>
 *     <entry>(string) The translated name of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"version"</literal></entry>
 *     <entry>(string) Version of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"category"</literal></entry>
 *     <entry>(string) Primary category of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"summary"</literal></entry>
 *     <entry>(string) Brief summary of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"description"</literal></entry>
 *     <entry>(string) Full description of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"authors"</literal></entry>
 *     <entry>(<type>const gchar * const *</type>) A %NULL-terminated list of
 *       plugin authors. format: First Last &lt;user\@domain.com&gt;</entry>
 *   </row>
 *   <row><entry><literal>"website"</literal></entry>
 *     <entry>(string) Website of the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"icon"</literal></entry>
 *     <entry>(string) Path to a plugin's icon.</entry>
 *   </row>
 *   <row><entry><literal>"license-id"</literal></entry>
 *     <entry>(string) Short name of the plugin's license. This should
 *       either be an identifier of the license from
 *       <ulink url="http://dep.debian.net/deps/dep5/#license-specification">
 *       DEP5</ulink> or "Other" for custom licenses.</entry>
 *   </row>
 *   <row><entry><literal>"license-text"</literal></entry>
 *     <entry>(string) The text of the plugin's license, if unlisted on
 *       DEP5.</entry>
 *   </row>
 *   <row><entry><literal>"license-url"</literal></entry>
 *     <entry>(string) The plugin's license URL, if unlisted on DEP5.</entry>
 *   </row>
 *   <row><entry><literal>"dependencies"</literal></entry>
 *     <entry>(<type>const gchar * const *</type>) A %NULL-terminated list of
 *       plugin IDs required by the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"actions-cb"</literal></entry>
 *     <entry>(#PurplePluginActionsCb) Callback that returns a list of
 *       actions the plugin can perform.</entry>
 *   </row>
 *   <row><entry><literal>"extra-cb"</literal></entry>
 *     <entry>(#PurplePluginExtraCb) Callback that returns a newly
 *       allocated string denoting extra information about a plugin.</entry>
 *   </row>
 *   <row><entry><literal>"pref-frame-cb"</literal></entry>
 *     <entry>(#PurplePluginPrefFrameCb) Callback that returns a
 *       preferences frame for the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"pref-request-cb"</literal></entry>
 *     <entry>(#PurplePluginPrefRequestCb) Callback that returns a
 *       preferences request handle for the plugin.</entry>
 *   </row>
 *   <row><entry><literal>"flags"</literal></entry>
 *     <entry>(#PurplePluginInfoFlags) The flags for a plugin.</entry>
 *   </row>
 *   </tbody></tgroup>
 * </informaltable>
 *
 * See #PURPLE_PLUGIN_ABI_VERSION,
 *     <link linkend="chapter-plugin-ids">Plugin IDs</link>.
 *
 * Returns: A new #PurplePluginInfo instance.
 */
PurplePluginInfo *purple_plugin_info_new(const char *first_property, ...)
                  G_GNUC_NULL_TERMINATED;

/**
 * purple_plugin_info_get_actions_cb:
 * @info: The plugin info to get the callback from.
 *
 * Returns the callback that retrieves the list of actions a plugin can perform
 * at that moment.
 *
 * Returns: The callback that returns a list of #PurplePluginAction
 *          instances corresponding to the actions a plugin can perform.
 */
PurplePluginActionsCb
purple_plugin_info_get_actions_cb(PurplePluginInfo *info);

/**
 * purple_plugin_info_get_extra_cb:
 * @info: The plugin info to get extra information from.
 *
 * Returns a callback that gives extra information about a plugin. You must
 * free the string returned by this callback.
 *
 * Returns: (transfer none): The callback that returns extra information about a plugin.
 */
PurplePluginExtraCb
purple_plugin_info_get_extra_cb(PurplePluginInfo *info);

/**
 * purple_plugin_info_get_pref_frame_cb:
 * @info: The plugin info to get the callback from.
 *
 * Returns the callback that retrieves the preferences frame for a plugin, set
 * via the "pref-frame-cb" property of the plugin info.
 *
 * Returns: The callback that returns the preferences frame.
 */
PurplePluginPrefFrameCb
purple_plugin_info_get_pref_frame_cb(PurplePluginInfo *info);

/**
 * purple_plugin_info_get_pref_request_cb:
 * @info: The plugin info to get the callback from.
 *
 * Returns the callback that retrieves the preferences request handle for a
 * plugin, set via the "pref-request-cb" property of the plugin info.
 *
 * Returns: (transfer none): The callback that returns the preferences request handle.
 */
PurplePluginPrefRequestCb
purple_plugin_info_get_pref_request_cb(PurplePluginInfo *info);

/**
 * purple_plugin_info_get_flags:
 * @info: The plugin's info instance.
 *
 * Returns the plugin's flags.
 *
 * Returns: The flags of the plugin.
 */
PurplePluginInfoFlags
purple_plugin_info_get_flags(PurplePluginInfo *info);

/**
 * purple_plugin_info_get_error:
 * @info: The plugin info.
 *
 * Returns an error in the plugin info that would prevent the plugin from being
 * loaded.
 *
 * Returns: The plugin info error, or %NULL.
 */
const gchar *purple_plugin_info_get_error(PurplePluginInfo *info);

/**
 * purple_plugin_info_set_ui_data:
 * @info: The plugin's info instance.
 * @ui_data: A pointer to associate with this object.
 *
 * Set the UI data associated with a plugin.
 */
void purple_plugin_info_set_ui_data(PurplePluginInfo *info, gpointer ui_data);

/**
 * purple_plugin_info_get_ui_data:
 * @info: The plugin's info instance.
 *
 * Returns the UI data associated with a plugin.
 *
 * Returns: The UI data associated with this plugin.  This is a
 *          convenience field provided to the UIs--it is not
 *          used by the libpurple core.
 */
gpointer purple_plugin_info_get_ui_data(PurplePluginInfo *info);

/**************************************************************************/
/* PluginAction API                                                       */
/**************************************************************************/

/**
 * purple_plugin_action_get_type:
 *
 * Returns: The #GType for the #PurplePluginAction boxed structure.
 */
GType purple_plugin_action_get_type(void);

/**
 * purple_plugin_action_new:
 * @label:    The description of the action to show to the user.
 * @callback: (scope call): The callback to call when the user selects this
 *            action.
 *
 * Allocates and returns a new PurplePluginAction. Use this to add actions in a
 * list in the "actions-cb" callback for your plugin.
 */
PurplePluginAction *purple_plugin_action_new(const char* label,
		PurplePluginActionCb callback);

/**
 * purple_plugin_action_free:
 * @action: The PurplePluginAction to free.
 *
 * Frees a PurplePluginAction
 */
void purple_plugin_action_free(PurplePluginAction *action);

/**************************************************************************/
/* Plugins API                                                            */
/**************************************************************************/

/**
 * purple_plugins_find_all:
 *
 * Returns a list of all plugins, whether loaded or not.
 *
 * Returns: (element-type PurplePlugin) (transfer full): A list of all plugins.
 * 	       The list is owned by the caller, and must be
 *         g_list_free()d to avoid leaking the nodes.
 */
GList *purple_plugins_find_all(void);

/**
 * purple_plugins_get_loaded:
 *
 * Returns a list of all loaded plugins.
 *
 * Returns: (element-type PurplePlugin) (transfer none): A list of all loaded plugins.
 */
GList *purple_plugins_get_loaded(void);

/**
 * purple_plugins_add_search_path:
 * @path: The new search path.
 *
 * Add a new directory to search for plugins
 */
void purple_plugins_add_search_path(const gchar *path);

/**
 * purple_plugins_refresh:
 *
 * Forces a refresh of all plugins found in the search paths, and loads plugins
 * that are to be auto-loaded.
 *
 * See purple_plugins_add_search_path().
 */
void purple_plugins_refresh(void);

/**
 * purple_plugins_find_plugin:
 * @id: The plugin ID.
 *
 * Finds a plugin with the specified plugin ID.
 *
 * Returns: (transfer none): The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_plugin(const gchar *id);

/**
 * purple_plugins_find_by_filename:
 * @filename: The plugin filename.
 *
 * Finds a plugin with the specified filename (filename with a path).
 *
 * Returns: (transfer none): The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_by_filename(const char *filename);

/**
 * purple_plugins_save_loaded:
 * @key: The preference key to save the list of plugins to.
 *
 * Saves the list of loaded plugins to the specified preference key.
 * Plugins that are set to auto-load are not saved.
 */
void purple_plugins_save_loaded(const char *key);

/**
 * purple_plugins_load_saved:
 * @key: The preference key containing the list of plugins.
 *
 * Attempts to load all the plugins in the specified preference key
 * that were loaded when purple last quit.
 */
void purple_plugins_load_saved(const char *key);

/**************************************************************************/
/* Plugins Subsystem API                                                  */
/**************************************************************************/

/**
 * purple_plugins_get_handle:
 *
 * Returns the plugin subsystem handle.
 *
 * Returns: (transfer none): The plugin sybsystem handle.
 */
void *purple_plugins_get_handle(void);

/**
 * purple_plugins_init:
 *
 * Initializes the plugin subsystem
 */
void purple_plugins_init(void);

/**
 * purple_plugins_uninit:
 *
 * Uninitializes the plugin subsystem
 */
void purple_plugins_uninit(void);

G_END_DECLS

#endif /* PURPLE_PLUGINS_H */
