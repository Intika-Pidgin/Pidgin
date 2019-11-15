/*
 * Themes for libpurple
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
#include "theme.h"
#include "util.h"

void purple_theme_set_type_string(PurpleTheme *theme, const gchar *type);

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	gchar *name;
	gchar *description;
	gchar *author;
	gchar *type;
	gchar *dir;
	gchar *img;
} PurpleThemePrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/

enum {
	PROP_ZERO = 0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_AUTHOR,
	PROP_TYPE,
	PROP_DIR,
	PROP_IMAGE,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/

static GParamSpec *properties[PROP_LAST];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(PurpleTheme, purple_theme, G_TYPE_OBJECT);

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
purple_theme_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *psec)
{
	PurpleTheme *theme = PURPLE_THEME(obj);

	switch (param_id) {
		case PROP_NAME:
			g_value_set_string(value, purple_theme_get_name(theme));
			break;
		case PROP_DESCRIPTION:
			g_value_set_string(value, purple_theme_get_description(theme));
			break;
		case PROP_AUTHOR:
			g_value_set_string(value, purple_theme_get_author(theme));
			break;
		case PROP_TYPE:
			g_value_set_string(value, purple_theme_get_type_string(theme));
			break;
		case PROP_DIR:
			g_value_set_string(value, purple_theme_get_dir(theme));
			break;
		case PROP_IMAGE:
			g_value_set_string(value, purple_theme_get_image(theme));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
purple_theme_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *psec)
{
	PurpleTheme *theme = PURPLE_THEME(obj);

	switch (param_id) {
		case PROP_NAME:
			purple_theme_set_name(theme, g_value_get_string(value));
			break;
		case PROP_DESCRIPTION:
			purple_theme_set_description(theme, g_value_get_string(value));
			break;
		case PROP_AUTHOR:
			purple_theme_set_author(theme, g_value_get_string(value));
			break;
		case PROP_TYPE:
			purple_theme_set_type_string(theme, g_value_get_string(value));
			break;
		case PROP_DIR:
			purple_theme_set_dir(theme, g_value_get_string(value));
			break;
		case PROP_IMAGE:
			purple_theme_set_image(theme, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
purple_theme_init(PurpleTheme *theme)
{
}

static void
purple_theme_finalize(GObject *obj)
{
	PurpleTheme *theme = PURPLE_THEME(obj);
	PurpleThemePrivate *priv = purple_theme_get_instance_private(theme);

	g_free(priv->name);
	g_free(priv->description);
	g_free(priv->author);
	g_free(priv->type);
	g_free(priv->dir);
	g_free(priv->img);

	G_OBJECT_CLASS(purple_theme_parent_class)->finalize(obj);
}

static void
purple_theme_class_init(PurpleThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_theme_get_property;
	obj_class->set_property = purple_theme_set_property;
	obj_class->finalize = purple_theme_finalize;

	/* NAME */
	properties[PROP_NAME] = g_param_spec_string("name", "Name",
			"The name of the theme",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/* DESCRIPTION */
	properties[PROP_DESCRIPTION] = g_param_spec_string("description",
			"Description",
			"The description of the theme",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/* AUTHOR */
	properties[PROP_AUTHOR] = g_param_spec_string("author", "Author",
			"The author of the theme",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/* TYPE STRING (read only) */
	properties[PROP_TYPE] = g_param_spec_string("type", "Type",
			"The string representing the type of the theme",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS);

	/* DIRECTORY */
	properties[PROP_DIR] = g_param_spec_string("directory", "Directory",
			"The directory that contains the theme and all its files",
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/* PREVIEW IMAGE */
	properties[PROP_IMAGE] = g_param_spec_string("image", "Image",
			"A preview image of the theme",
			NULL,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

/******************************************************************************
 * Helper Functions
 *****************************************************************************/

static gchar *
theme_clean_text(const gchar *text)
{
	gchar *clean_text = NULL;
	if (text != NULL) {
		clean_text = g_markup_escape_text(text, -1);
		g_strdelimit(clean_text, "\n", ' ');
		purple_str_strip_char(clean_text, '\r');
	}
	return clean_text;
}

/*****************************************************************************
 * Public API function
 *****************************************************************************/

const gchar *
purple_theme_get_name(PurpleTheme *theme)
{
	PurpleThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = purple_theme_get_instance_private(theme);
	return priv->name;
}

void
purple_theme_set_name(PurpleTheme *theme, const gchar *name)
{
	PurpleThemePrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = purple_theme_get_instance_private(theme);

	g_free(priv->name);
	priv->name = theme_clean_text(name);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_NAME]);
}

const gchar *
purple_theme_get_description(PurpleTheme *theme)
{
	PurpleThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = purple_theme_get_instance_private(theme);
	return priv->description;
}

void
purple_theme_set_description(PurpleTheme *theme, const gchar *description)
{
	PurpleThemePrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = purple_theme_get_instance_private(theme);

	g_free(priv->description);
	priv->description = theme_clean_text(description);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_DESCRIPTION]);
}

const gchar *
purple_theme_get_author(PurpleTheme *theme)
{
	PurpleThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = purple_theme_get_instance_private(theme);
	return priv->author;
}

void
purple_theme_set_author(PurpleTheme *theme, const gchar *author)
{
	PurpleThemePrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = purple_theme_get_instance_private(theme);

	g_free(priv->author);
	priv->author = theme_clean_text(author);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_AUTHOR]);
}

const gchar *
purple_theme_get_type_string(PurpleTheme *theme)
{
	PurpleThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = purple_theme_get_instance_private(theme);
	return priv->type;
}

/* < private > */
void
purple_theme_set_type_string(PurpleTheme *theme, const gchar *type)
{
	PurpleThemePrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = purple_theme_get_instance_private(theme);

	g_free(priv->type);
	priv->type = g_strdup(type);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_TYPE]);
}

const gchar *
purple_theme_get_dir(PurpleTheme *theme)
{
	PurpleThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = purple_theme_get_instance_private(theme);
	return priv->dir;
}

void
purple_theme_set_dir(PurpleTheme *theme, const gchar *dir)
{
	PurpleThemePrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = purple_theme_get_instance_private(theme);

	g_free(priv->dir);
	priv->dir = g_strdup(dir);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_DIR]);
}

const gchar *
purple_theme_get_image(PurpleTheme *theme)
{
	PurpleThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_THEME(theme), NULL);

	priv = purple_theme_get_instance_private(theme);

	return priv->img;
}

gchar *
purple_theme_get_image_full(PurpleTheme *theme)
{
	const gchar *filename = purple_theme_get_image(theme);

	if (filename)
		return g_build_filename(purple_theme_get_dir(PURPLE_THEME(theme)), filename, NULL);
	else
		return NULL;
}

void
purple_theme_set_image(PurpleTheme *theme, const gchar *img)
{
	PurpleThemePrivate *priv;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	priv = purple_theme_get_instance_private(theme);

	g_free(priv->img);
	priv->img = g_strdup(img);

	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_IMAGE]);
}
