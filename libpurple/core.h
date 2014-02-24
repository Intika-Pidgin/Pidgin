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

#ifndef _PURPLE_CORE_H_
#define _PURPLE_CORE_H_
/**
 * SECTION:core
 * @section_id: libpurple-core
 * @short_description: <filename>core.h</filename>
 * @title: Startup and Shutdown of libpurple
 * @see_also: <link linkend="chapter-signals-core">Core signals</link>
 */

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_CORE_UI_OPS (purple_core_ui_ops_get_type())

typedef struct PurpleCore PurpleCore;
typedef struct _PurpleCoreUiOps PurpleCoreUiOps;

/**
 * PurpleCoreUiOps:
 * @ui_prefs_init: Called just after the preferences subsystem is initialized;
 *                 the UI could use this callback to add some preferences it
 *                 needs to be in place when other subsystems are initialized.
 * @debug_ui_init: Called just after the debug subsystem is initialized, but
 *                 before just about every other component's initialization. The
 *                 UI should use this hook to call purple_debug_set_ui_ops() so
 *                 that debugging information for other components can be logged
 *                 during their initialization.
 * @ui_init:       Called after all of libpurple has been initialized. The UI
 *                 should use this hook to set all other necessary
 *    <link linkend="chapter-ui-ops"><literal>UiOps structures</literal></link>.
 * @quit:          Called after most of libpurple has been uninitialized.
 * @get_ui_info:   Called by purple_core_get_ui_info(); should return the
 *                 information documented there.
 *
 * Callbacks that fire at different points of the initialization and teardown
 * of libpurple, along with a hook to return descriptive information about the
 * UI.
 */
struct _PurpleCoreUiOps
{
	void (*ui_prefs_init)(void);
	void (*debug_ui_init)(void);
	void (*ui_init)(void);

	void (*quit)(void);

	GHashTable* (*get_ui_info)(void);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_core_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleCoreUiOps boxed structure.
 */
GType purple_core_ui_ops_get_type(void);

/**
 * purple_core_init:
 * @ui: The ID of the UI using the core. This should be a
 *           unique ID, registered with the purple team.
 *
 * Initializes the core of purple.
 *
 * This will setup preferences for all the core subsystems.
 *
 * Returns: %TRUE if successful, or %FALSE otherwise.
 */
gboolean purple_core_init(const char *ui);

/**
 * purple_core_quit:
 *
 * Quits the core of purple, which, depending on the UI, may quit the
 * application using the purple core.
 */
void purple_core_quit(void);

/**
 * purple_core_quit_cb:
 *
 * Calls purple_core_quit().  This can be used as the function
 * passed to purple_timeout_add() when you want to shutdown Purple
 * in a specified amount of time.  When shutting down Purple
 * from a plugin, you must use this instead of purple_core_quit();
 * for an immediate exit, use a timeout value of 0:
 *
 * <programlisting>
 * purple_timeout_add(0, purple_core_quitcb, NULL)
 * </programlisting>
 *
 * This is ensures that code from your plugin is not being
 * executed when purple_core_quit() is called.  If the plugin
 * called purple_core_quit() directly, you would get a core dump
 * after purple_core_quit() executes and control returns to your
 * plugin because purple_core_quit() frees all plugins.
 */
gboolean purple_core_quit_cb(gpointer unused);

/**
 * purple_core_get_version:
 *
 * Returns the version of the core library.
 *
 * Returns: The version of the core library.
 */
const char *purple_core_get_version(void);

/**
 * purple_core_get_ui:
 *
 * Returns the ID of the UI that is using the core, as passed to
 * purple_core_init().
 *
 * Returns: The ID of the UI that is currently using the core.
 */
const char *purple_core_get_ui(void);

/**
 * purple_get_core:
 *
 * This is used to connect to
 * <link linkend="chapter-signals-core">core signals</link>.
 *
 * Returns: A handle to the purple core.
 */
PurpleCore *purple_get_core(void);

/**
 * purple_core_set_ui_ops:
 * @ops: A UI ops structure for the core.
 *
 * Sets the UI ops for the core.
 */
void purple_core_set_ui_ops(PurpleCoreUiOps *ops);

/**
 * purple_core_get_ui_ops:
 *
 * Returns the UI ops for the core.
 *
 * Returns: The core's UI ops structure.
 */
PurpleCoreUiOps *purple_core_get_ui_ops(void);

/**
 * purple_core_ensure_single_instance:
 *
 * Ensures that only one instance is running.  If libpurple is built with D-Bus
 * support, this checks if another process owns the libpurple bus name and if
 * so whether that process is using the same configuration directory as this
 * process.
 *
 * Returns: %TRUE if this is the first instance of libpurple running;
 *          %FALSE if there is another instance running.
 */
gboolean purple_core_ensure_single_instance(void);

/**
 * purple_core_get_ui_info:
 *
 * Returns a hash table containing various information about the UI.  The
 * following well-known entries may be in the table (along with any others the
 * UI might choose to include):
 *
 * <informaltable frame='none'>
 *   <tgroup cols='2'><tbody>
 *   <row>
 *     <entry><literal>name</literal></entry>
 *     <entry>the user-readable name for the UI.</entry>
 *   </row>
 *   <row>
 *     <entry><literal>version</literal></entry>
 *     <entry>a user-readable description of the current version of the UI.</entry>
 *   </row>
 *   <row>
 *     <entry><literal>website</literal></entry>
 *     <entry>the UI's website, such as https://pidgin.im.</entry>
 *   </row>
 *   <row>
 *     <entry><literal>dev_website</literal></entry>
 *     <entry>the UI's development/support website, such as
 *       https://developer.pidgin.im.</entry>
 *   </row>
 *   <row>
 *     <entry><literal>client_type</literal></entry>
 *     <entry>the type of UI. Possible values include 'pc', 'console', 'phone',
 *       'handheld', 'web', and 'bot'. These values are compared
 *       programmatically and should not be localized.</entry>
 *   </row>
 *   </tbody></tgroup>
 * </informaltable>
 *
 * Returns: A GHashTable with strings for keys and values.  This
 * hash table must not be freed and should not be modified.
 *
 */
GHashTable* purple_core_get_ui_info(void);

G_END_DECLS

#endif /* _PURPLE_CORE_H_ */

/*

                                                  /===-
                                                `//"\\   """"`---.___.-""
             ______-==|                         | |  \\           _-"`
       __--"""  ,-/-==\\                        | |   `\        ,'
    _-"       /'    |  \\            ___         / /      \      /
  .'        /       |   \\         /"   "\    /' /        \   /'
 /  ____  /         |    \`\.__/-""  D O   \_/'  /          \/'
/-'"    """""---__  |     "-/"   O G     R   /'        _--"`
                  \_|      /   R    __--_  t ),   __--""
                    '""--_/  T   _-"_>--<_\ h '-" \
                   {\__--_/}    / \\__>--<__\ e B  \
                   /'   (_/  _-"  | |__>--<__|   U  |
                  |   _/) )-"     | |__>--<__|  R   |
                  / /" ,_/       / /__>---<__/ N    |
                 o-o _//        /-"_>---<__-" I    /
                 (^("          /"_>---<__-  N   _-"
                ,/|           /__>--<__/  A  _-"
             ,//('(          |__>--<__|  T  /                  .----_
            ( ( '))          |__>--<__|    |                 /' _---_"\
         `-)) )) (           |__>--<__|  O |               /'  /     "\`\
        ,/,'//( (             \__>--<__\  R \            /'  //        ||
      ,( ( ((, ))              "-__>--<_"-_  "--____---"' _/'/        /'
    `"/  )` ) ,/|                 "-_">--<_/-__       __-" _/
  ._-"//( )/ )) `                    ""-'_/_/ /"""""""__--"
   ;'( ')/ ,)(                              """"""""""
  ' ') '( (/
    '   '  `

*/
