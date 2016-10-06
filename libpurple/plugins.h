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

#ifndef _PURPLE_PLUGINS_H_
#define _PURPLE_PLUGINS_H_
/**
 * SECTION:plugins
 * @section_id: libpurple-plugins
 * @short_description: <filename>plugins.h</filename>
 * @title: Plugin API
 * @see_also: <link linkend="chapter-signals-plugin">Plugin signals</link>,
 *     <link linkend="chapter-plugin-ids">Plugin IDs</link>,
 *     <link linkend="chapter-plugin-i18n">Third Party Plugin Translation</link>
 */

#ifdef PURPLE_PLUGINS
#include <gplugin.h>
#include <gplugin-native.h>
#else
#include <glib.h>
#include <glib-object.h>
#endif

#include "version.h"

#define PURPLE_PLUGINS_DOMAIN          (g_quark_from_static_string("plugins"))

#ifdef PURPLE_PLUGINS

#define PURPLE_TYPE_PLUGIN             GPLUGIN_TYPE_PLUGIN
#define PURPLE_PLUGIN(obj)             GPLUGIN_PLUGIN(obj)
#define PURPLE_PLUGIN_CLASS(klass)     GPLUGIN_PLUGIN_CLASS(klass)
#define PURPLE_IS_PLUGIN(obj)          GPLUGIN_IS_PLUGIN(obj)
#define PURPLE_IS_PLUGIN_CLASS(klass)  GPLUGIN_IS_PLUGIN_CLASS(klass)
#define PURPLE_PLUGIN_GET_CLASS(obj)   GPLUGIN_PLUGIN_GET_CLASS(obj)

/**
 * PurplePlugin:
 *
 * Represents a plugin handle.
 * This type is an alias for GPluginPlugin.
 */
typedef GPluginPlugin PurplePlugin;

typedef GPluginPluginClass PurplePluginClass;

#else /* !defined(PURPLE_PLUGINS) */

#define PURPLE_TYPE_PLUGIN             G_TYPE_OBJECT
#define PURPLE_PLUGIN(obj)             G_OBJECT(obj)
#define PURPLE_PLUGIN_CLASS(klass)     G_OBJECT_CLASS(klass)
#define PURPLE_IS_PLUGIN(obj)          G_IS_OBJECT(obj)
#define PURPLE_IS_PLUGIN_CLASS(klass)  G_IS_OBJECT_CLASS(klass)
#define PURPLE_PLUGIN_GET_CLASS(obj)   G_OBJECT_GET_CLASS(obj)

typedef GObject PurplePlugin;
typedef GObjectClass PurplePluginClass;

#endif /* PURPLE_PLUGINS */

#define PURPLE_TYPE_PLUGIN_INFO             (purple_plugin_info_get_type())
#define PURPLE_PLUGIN_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfo))
#define PURPLE_PLUGIN_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfoClass))
#define PURPLE_IS_PLUGIN_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PLUGIN_INFO))
#define PURPLE_IS_PLUGIN_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_PLUGIN_INFO))
#define PURPLE_PLUGIN_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfoClass))

typedef struct _PurplePluginInfo PurplePluginInfo;
typedef struct _PurplePluginInfoClass PurplePluginInfoClass;

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
#ifdef PURPLE_PLUGINS
	GPluginPluginInfo parent;
#else
	GObject parent;
#endif

	/*< public >*/
	gpointer ui_data;
};

struct _PurplePluginInfoClass {
#ifdef PURPLE_PLUGINS
	GPluginPluginInfoClass parent_class;
#else
	GObjectClass parent_class;
#endif

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurplePluginAction:
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
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_PLUGIN_INIT(pluginname,pluginquery,pluginload,pluginunload) \
	PurplePluginInfo * pluginname##_plugin_query(void); \
	PurplePluginInfo * pluginname##_plugin_query(void) { \
		return pluginquery(NULL); \
	} \
	gboolean pluginname##_plugin_load(void); \
	gboolean pluginname##_plugin_load(void) { \
		GError *e = NULL; \
		gboolean loaded = pluginload(NULL, &e); \
		if (e) g_error_free(e); \
		return loaded; \
	} \
	gboolean pluginname##_plugin_unload(void); \
	gboolean pluginname##_plugin_unload(void) { \
		GError *e = NULL; \
		gboolean unloaded = pluginunload(NULL, &e); \
		if (e) g_error_free(e); \
		return unloaded; \
	}
#else /* PURPLE_PLUGINS && !PURPLE_STATIC_PRPL */
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
#endif

/**
 * PURPLE_DEFINE_TYPE:
 * @TN:   The name of the new type, in Camel case.
 * @t_n:  The name of the new type, in lowercase, words separated by '_'.
 * @T_P:  The #GType of the parent type.
 * 
 * A convenience macro for type implementations, which defines a *_get_type()
 * function; and a *_register_type() function for use in your plugin's load
 * function. You must define an instance initialization function *_init()
 * and a class initialization function *_class_init() for the type.
 *
 * The type will be registered statically if used in a static protocol or if
 * plugins support is disabled.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_DEFINE_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_STATIC_TYPE(TN, t_n, T_P)
#else
#define PURPLE_DEFINE_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_DYNAMIC_TYPE(TN, t_n, T_P)
#endif

/**
 * PURPLE_DEFINE_TYPE_EXTENDED:
 * @TN:     The name of the new type, in Camel case.
 * @t_n:    The name of the new type, in lowercase, words separated by '_'.
 * @T_P:    The #GType of the parent type.
 * @flags:  #GTypeFlags to register the type with.
 * @CODE:   Custom code that gets inserted in *_get_type().
 *
 * A more general version of PURPLE_DEFINE_TYPE() which allows you to
 * specify #GTypeFlags and custom code.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_DEFINE_TYPE_EXTENDED \
	PURPLE_DEFINE_STATIC_TYPE_EXTENDED
#else
#define PURPLE_DEFINE_TYPE_EXTENDED \
	PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED
#endif

/**
 * PURPLE_IMPLEMENT_INTERFACE_STATIC:
 * @TYPE_IFACE:  The #GType of the interface to add.
 * @iface_init:  The interface init function.
 *
 * A convenience macro to ease static interface addition in the CODE section
 * of PURPLE_DEFINE_TYPE_EXTENDED(). You should use this macro if the
 * interface is a part of the libpurple core.
 */
#define PURPLE_IMPLEMENT_INTERFACE_STATIC(TYPE_IFACE, iface_init) { \
	const GInterfaceInfo interface_info = { \
		(GInterfaceInitFunc) iface_init, NULL, NULL \
	}; \
	g_type_add_interface_static(type_id, TYPE_IFACE, &interface_info); \
}

/**
 * PURPLE_IMPLEMENT_INTERFACE:
 * @TYPE_IFACE:  The #GType of the interface to add.
 * @iface_init:  The interface init function.
 *
 * A convenience macro to ease interface addition in the CODE section
 * of PURPLE_DEFINE_TYPE_EXTENDED(). You should use this macro if the
 * interface lives in the plugin.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
	PURPLE_IMPLEMENT_INTERFACE_STATIC(TYPE_IFACE, iface_init)
#else
#define PURPLE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
	PURPLE_IMPLEMENT_INTERFACE_DYNAMIC(TYPE_IFACE, iface_init)
#endif

/**
 * PURPLE_DEFINE_DYNAMIC_TYPE:
 *
 * A convenience macro for dynamic type implementations.
 */
#define PURPLE_DEFINE_DYNAMIC_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED (TN, t_n, T_P, 0, {})

/**
 * PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED:
 *
 * A more general version of PURPLE_DEFINE_DYNAMIC_TYPE().
 */
#define PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, CODE) \
static GType type_name##_type_id = 0; \
G_MODULE_EXPORT GType type_name##_get_type(void) { \
	return type_name##_type_id; \
} \
void type_name##_register_type(PurplePlugin *); \
void type_name##_register_type(PurplePlugin *plugin) { \
	GType type_id; \
	const GTypeInfo type_info = { \
		sizeof (TypeName##Class), \
		(GBaseInitFunc) NULL, \
		(GBaseFinalizeFunc) NULL, \
		(GClassInitFunc) type_name##_class_init, \
		(GClassFinalizeFunc) NULL, \
		NULL, \
		sizeof (TypeName), \
		0, \
		(GInstanceInitFunc) type_name##_init, \
		NULL \
	}; \
	type_id = purple_plugin_register_type(plugin, TYPE_PARENT, #TypeName, \
	                                      &type_info, (GTypeFlags) flags); \
	type_name##_type_id = type_id; \
	{ CODE ; } \
}

/**
 * PURPLE_IMPLEMENT_INTERFACE_DYNAMIC:
 *
 * A convenience macro to ease dynamic interface addition.
 */
#define PURPLE_IMPLEMENT_INTERFACE_DYNAMIC(TYPE_IFACE, iface_init) { \
	const GInterfaceInfo interface_info = { \
		(GInterfaceInitFunc) iface_init, NULL, NULL \
	}; \
	purple_plugin_add_interface(plugin, type_id, TYPE_IFACE, &interface_info); \
}

/**
 * PURPLE_DEFINE_STATIC_TYPE:
 *
 * A convenience macro for static type implementations.
 */
#define PURPLE_DEFINE_STATIC_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_STATIC_TYPE_EXTENDED (TN, t_n, T_P, 0, {})

/**
 * PURPLE_DEFINE_STATIC_TYPE_EXTENDED:
 *
 * A more general version of PURPLE_DEFINE_STATIC_TYPE().
 */
#define PURPLE_DEFINE_STATIC_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, CODE) \
static GType type_name##_type_id = 0; \
GType type_name##_get_type(void) { \
	if (G_UNLIKELY(type_name##_type_id == 0)) { \
		GType type_id; \
		const GTypeInfo type_info = { \
			sizeof (TypeName##Class), \
			(GBaseInitFunc) NULL, \
			(GBaseFinalizeFunc) NULL, \
			(GClassInitFunc) type_name##_class_init, \
			(GClassFinalizeFunc) NULL, \
			NULL, \
			sizeof (TypeName), \
			0, \
			(GInstanceInitFunc) type_name##_init, \
			NULL \
		}; \
		type_id = g_type_register_static(TYPE_PARENT, #TypeName, &type_info, \
		                                 (GTypeFlags) flags); \
		type_name##_type_id = type_id; \
		{ CODE ; } \
	} \
	return type_name##_type_id; \
} \
void type_name##_register_type(PurplePlugin *); \
void type_name##_register_type(PurplePlugin *plugin) { }

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
gboolean purple_plugin_is_loaded(const PurplePlugin *plugin);

/**
 * purple_plugin_get_filename:
 * @plugin: The plugin.
 *
 * Returns a plugin's filename, along with the path.
 *
 * Returns: The plugin's filename.
 */
const gchar *purple_plugin_get_filename(const PurplePlugin *plugin);

/**
 * purple_plugin_get_info:
 * @plugin: The plugin.
 *
 * Returns a plugin's #PurplePluginInfo instance.
 *
 * Returns: (transfer none) The plugin's #PurplePluginInfo instance.
 * GPlugin refs the plugin info object before returning it. This workaround
 * is to avoid managing the reference counts everywhere in our codebase
 * where we use the plugin info. The plugin info instance is guaranteed to
 * exist as long as the plugin exists.
 */
PurplePluginInfo *purple_plugin_get_info(const PurplePlugin *plugin);

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
 * purple_plugin_register_type:
 * @plugin:  The plugin that is registering the type.
 * @parent:  Type from which this type will be derived.
 * @name:    Name of the new type.
 * @info:    Information to initialize and destroy a type's classes and
 *           instances.
 * @flags:   Bitwise combination of values that determines the nature
 *           (e.g. abstract or not) of the type.
 *
 * Registers a new dynamic type.
 *
 * Returns: The new GType, or %G_TYPE_INVALID if registration failed.
 */
GType purple_plugin_register_type(PurplePlugin *plugin, GType parent,
                                  const gchar *name, const GTypeInfo *info,
                                  GTypeFlags flags);

/**
 * purple_plugin_add_interface:
 * @plugin:          The plugin that is adding the interface type.
 * @instance_type:   The GType of the instantiable type.
 * @interface_type:  The GType of the interface type.
 * @interface_info:  Information used to manage the interface type.
 *
 * Adds a dynamic interface type to an instantiable type.
 */
void purple_plugin_add_interface(PurplePlugin *plugin, GType instance_type,
                                 GType interface_type,
                                 const GInterfaceInfo *interface_info);

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
gboolean purple_plugin_is_internal(const PurplePlugin *plugin);

/**
 * purple_plugin_get_dependent_plugins:
 * @plugin: The plugin whose dependent plugins are returned.
 *
 * Returns a list of plugins that depend on a particular plugin.
 *
 * Returns: (element-type PurplePlugin) (transfer none): The list of a plugins that depend on the specified
 *                           plugin.
 */
GSList *purple_plugin_get_dependent_plugins(const PurplePlugin *plugin);

/**************************************************************************/
/* PluginInfo API                                                         */
/**************************************************************************/

/**
 * purple_plugin_info_get_type:
 *
 * Returns: The #GType for the #PurplePluginInfo object.
 */
GType purple_plugin_info_get_type(void);

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
 * purple_plugin_info_get_id:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's ID.
 *
 * Returns: The plugin's ID.
 */
const gchar *purple_plugin_info_get_id(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_name:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's translated name.
 *
 * Returns: The name of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_name(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_version:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's version.
 *
 * Returns: The version of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_version(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_category:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's primary category.
 *
 * Returns: The primary category of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_category(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_summary:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's summary.
 *
 * Returns: The summary of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_summary(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_description:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's description.
 *
 * Returns: The description of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_description(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_authors:
 * @info: The plugin's info instance.
 *
 * Returns a NULL-terminated list of the plugin's authors.
 *
 * Returns: The authors of the plugin, or %NULL.
 */
const gchar * const *
purple_plugin_info_get_authors(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_website:
 * @info: The plugin's info instance.
 *
 * Returns a plugin's website.
 *
 * Returns: The website of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_website(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_icon:
 * @info: The plugin's info instance.
 *
 * Returns the path to a plugin's icon.
 *
 * Returns: The path to the plugin's icon, or %NULL.
 */
const gchar *purple_plugin_info_get_icon(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_license_id:
 * @info: The plugin's info instance.
 *
 * Returns a short name of the plugin's license.
 *
 * Returns: The license name of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_license_id(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_license_text:
 * @info: The plugin's info instance.
 *
 * Returns the text of a plugin's license.
 *
 * Returns: The license text of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_license_text(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_license_url:
 * @info: The plugin's info instance.
 *
 * Returns the URL of a plugin's license.
 *
 * Returns: The license URL of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_license_url(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_dependencies:
 * @info: The plugin's info instance.
 *
 * Returns a NULL-terminated list of IDs of plugins required by a plugin.
 *
 * Returns: The dependencies of the plugin, or %NULL.
 */
const gchar * const *
purple_plugin_info_get_dependencies(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_abi_version:
 * @info: The plugin's info instance.
 *
 * Returns the required purple ABI version for a plugin.
 *
 * Returns: The required purple ABI version for the plugin.
 */
guint32 purple_plugin_info_get_abi_version(const PurplePluginInfo *info);

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
purple_plugin_info_get_actions_cb(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_extra_cb:
 * @info: The plugin info to get extra information from.
 *
 * Returns a callback that gives extra information about a plugin. You must
 * free the string returned by this callback.
 *
 * Returns: The callback that returns extra information about a plugin.
 */
PurplePluginExtraCb
purple_plugin_info_get_extra_cb(const PurplePluginInfo *info);

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
purple_plugin_info_get_pref_frame_cb(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_pref_request_cb:
 * @info: The plugin info to get the callback from.
 *
 * Returns the callback that retrieves the preferences request handle for a
 * plugin, set via the "pref-request-cb" property of the plugin info.
 *
 * Returns: The callback that returns the preferences request handle.
 */
PurplePluginPrefRequestCb
purple_plugin_info_get_pref_request_cb(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_flags:
 * @info: The plugin's info instance.
 *
 * Returns the plugin's flags.
 *
 * Returns: The flags of the plugin.
 */
PurplePluginInfoFlags
purple_plugin_info_get_flags(const PurplePluginInfo *info);

/**
 * purple_plugin_info_get_error:
 * @info: The plugin info.
 *
 * Returns an error in the plugin info that would prevent the plugin from being
 * loaded.
 *
 * Returns: The plugin info error, or %NULL.
 */
const gchar *purple_plugin_info_get_error(const PurplePluginInfo *info);

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
gpointer purple_plugin_info_get_ui_data(const PurplePluginInfo *info);

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

#endif /* _PURPLE_PLUGINS_H_ */
