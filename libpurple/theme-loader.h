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

#ifndef PURPLE_THEME_LOADER_H
#define PURPLE_THEME_LOADER_H
/**
 * SECTION:theme-loader
 * @section_id: libpurple-theme-loader
 * @short_description: <filename>theme-loader.h</filename>
 * @title: Theme Loader Abstact Class
 */

#include <glib.h>
#include <glib-object.h>
#include "theme.h"

typedef struct _PurpleThemeLoader        PurpleThemeLoader;
typedef struct _PurpleThemeLoaderClass   PurpleThemeLoaderClass;

#define PURPLE_TYPE_THEME_LOADER            (purple_theme_loader_get_type())
#define PURPLE_THEME_LOADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_THEME_LOADER, PurpleThemeLoader))
#define PURPLE_THEME_LOADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_THEME_LOADER, PurpleThemeLoaderClass))
#define PURPLE_IS_THEME_LOADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_THEME_LOADER))
#define PURPLE_IS_THEME_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_THEME_LOADER))
#define PURPLE_THEME_LOADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_THEME_LOADER, PurpleThemeLoaderClass))

/**
 * PurpleThemeLoader:
 *
 * A purple theme loader.
 * This is an abstract class for Purple to use with the Purple theme manager.
 * The loader is responsible for building each type of theme
 */
struct _PurpleThemeLoader
{
	GObject parent;
};

struct _PurpleThemeLoaderClass
{
	GObjectClass parent_class;

	PurpleTheme *(*purple_theme_loader_build)(const gchar*);
	gboolean (*probe_directory)(const gchar *);

	/*< private >*/
	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

/**************************************************************************/
/* Purple Theme-Loader API                                                */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * purple_theme_loader_get_type:
 *
 * Returns: The #GType for theme loader.
 */
GType purple_theme_loader_get_type(void);

/**
 * purple_theme_loader_get_type_string:
 * @self: The theme loader
 *
 * Returns the string representing the type of the theme loader
 *
 * Returns: The string representing this type
 */
const gchar *purple_theme_loader_get_type_string(PurpleThemeLoader *self);

/**
 * purple_theme_loader_build:
 * @loader: The theme loader
 * @dir:    The directory containing the theme
 *
 * Creates a new PurpleTheme
 *
 * Returns: A PurpleTheme containing the information from the directory
 */
PurpleTheme *purple_theme_loader_build(PurpleThemeLoader *loader, const gchar *dir);

/**
 * purple_theme_loader_probe:
 * @loader: The theme loader
 * @dir:    The directory that may contain the theme
 *
 * Probes a directory to see if it might possibly contain a theme
 *
 * This function might only check for obvious files or directory structure.
 * Loading of a theme may fail for other reasons.
 * The default prober checks for $dir/purple/$type.
 *
 * Returns: TRUE if the directory appears to contain a theme, FALSE otherwise.
 */
gboolean purple_theme_loader_probe(PurpleThemeLoader *loader, const gchar *dir);

G_END_DECLS

#endif /* PURPLE_THEME_LOADER_H */
