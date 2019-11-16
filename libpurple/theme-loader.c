/*
 * ThemeLoaders for libpurple
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
#include "theme-loader.h"

void purple_theme_loader_set_type_string(PurpleThemeLoader *loader, const gchar *type);

/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *type;
} PurpleThemeLoaderPrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/

enum {
	PROP_ZERO = 0,
	PROP_TYPE,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/

static GParamSpec *properties[PROP_LAST];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(PurpleThemeLoader, purple_theme_loader,
		G_TYPE_OBJECT);

/******************************************************************************
 * GObject Stuff                                                              *
 *****************************************************************************/

static void
purple_theme_loader_get_property(GObject *obj, guint param_id, GValue *value,
						 GParamSpec *psec)
{
	PurpleThemeLoader *theme_loader = PURPLE_THEME_LOADER(obj);

	switch (param_id) {
		case PROP_TYPE:
			g_value_set_string(value, purple_theme_loader_get_type_string(theme_loader));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
purple_theme_loader_set_property(GObject *obj, guint param_id, const GValue *value,
						 GParamSpec *psec)
{
	PurpleThemeLoader *loader = PURPLE_THEME_LOADER(obj);

	switch (param_id) {
		case PROP_TYPE:
			purple_theme_loader_set_type_string(loader, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static gboolean
purple_theme_loader_probe_directory(PurpleThemeLoader *loader, const gchar *dir)
{
	const gchar *type = purple_theme_loader_get_type_string(loader);
	char *themedir;
	gboolean result;

	/* Checks for directory as $root/purple/$type */
	themedir = g_build_filename(dir, "purple", type, NULL);
	result = g_file_test(themedir, G_FILE_TEST_IS_DIR);
	g_free(themedir);

	return result;
}

static void
purple_theme_loader_init(PurpleThemeLoader *loader)
{
}

static void
purple_theme_loader_finalize(GObject *obj)
{
	PurpleThemeLoader *loader = PURPLE_THEME_LOADER(obj);
	PurpleThemeLoaderPrivate *priv =
			purple_theme_loader_get_instance_private(loader);

	g_free(priv->type);

	G_OBJECT_CLASS(purple_theme_loader_parent_class)->finalize(obj);
}

static void
purple_theme_loader_class_init(PurpleThemeLoaderClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_theme_loader_get_property;
	obj_class->set_property = purple_theme_loader_set_property;
	obj_class->finalize = purple_theme_loader_finalize;

	/* TYPE STRING (read only) */
	properties[PROP_TYPE] = g_param_spec_string("type", "Type",
				    "The string representing the type of the theme",
				    NULL,
				    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				    G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

const gchar *
purple_theme_loader_get_type_string(PurpleThemeLoader *theme_loader)
{
	PurpleThemeLoaderPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME_LOADER(theme_loader), NULL);

	priv = purple_theme_loader_get_instance_private(theme_loader);
	return priv->type;
}

/* < private > */
void
purple_theme_loader_set_type_string(PurpleThemeLoader *loader, const gchar *type)
{
	PurpleThemeLoaderPrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME_LOADER(loader));

	priv = purple_theme_loader_get_instance_private(loader);

	g_free(priv->type);
	priv->type = g_strdup(type);

	g_object_notify_by_pspec(G_OBJECT(loader), properties[PROP_TYPE]);
}

PurpleTheme *
purple_theme_loader_build(PurpleThemeLoader *loader, const gchar *dir)
{
	return PURPLE_THEME_LOADER_GET_CLASS(loader)->purple_theme_loader_build(dir);
}

gboolean
purple_theme_loader_probe(PurpleThemeLoader *loader, const gchar *dir)
{
	if (PURPLE_THEME_LOADER_GET_CLASS(loader)->probe_directory != NULL)
		return PURPLE_THEME_LOADER_GET_CLASS(loader)->probe_directory(dir);
	else
		return purple_theme_loader_probe_directory(loader, dir);
}

