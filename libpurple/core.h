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

#ifndef PURPLE_CORE_H
#define PURPLE_CORE_H
/**
 * SECTION:core
 * @section_id: libpurple-core
 * @short_description: <filename>core.h</filename>
 * @title: Startup and Shutdown of libpurple
 * @see_also: <link linkend="chapter-signals-core">Core signals</link>
 */

#include <glib.h>
#include <glib-object.h>

#include <libpurple/purpleuiinfo.h>

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
 *                 UI should use this hook to call purple_debug_set_ui() so
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

	PurpleUiInfo *(*get_ui_info)(void);

	/*< private >*/
	gpointer reserved[4];
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
 * @unused: This argument is for consistency with a timeout callback. It is
 *          otherwise unused.
 *
 * Calls purple_core_quit().  This can be used as the function passed to
 * g_timeout_add() when you want to shutdown Purple in a specified amount of
 * time.  When shutting down Purple from a plugin, you must use this instead of
 * purple_core_quit(); for an immediate exit, use a timeout value of 0:
 *
 * <programlisting>
 * g_timeout_add(0, purple_core_quit_cb, NULL)
 * </programlisting>
 *
 * This ensures that code from your plugin is not being executed when
 * purple_core_quit() is called.  If the plugin called purple_core_quit()
 * directly, you would get a core dump after purple_core_quit() executes and
 * control returns to your plugin because purple_core_quit() frees all plugins.
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
 * Returns: (transfer none): A handle to the purple core.
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
 * purple_core_get_ui_info:
 *
 * Returns a #PurpleUiInfo that contains information about the user interface.
 *
 * Returns: (transfer full): A #PurpleUiInfo instance.
 */
PurpleUiInfo* purple_core_get_ui_info(void);

/**
 * purple_core_migrate_to_xdg_base_dirs:
 *
 * Migrates from legacy directory for libpurple to location following
 * XDG base dir spec. https://developer.pidgin.im/ticket/10029
 * NOTE This is not finished yet. Need to decide where other profile files
 * should be moved. Search for usages of purple_user_dir().
 *
 * Returns: TRUE if migrated successfully, FALSE otherwise. On failure,
 *         the application must display an error to the user and then exit.
 */
gboolean purple_core_migrate_to_xdg_base_dirs(void);

G_END_DECLS

#endif /* PURPLE_CORE_H */

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
