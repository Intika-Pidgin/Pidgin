/*
 * Sound Themes for libpurple
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
#include "sound-theme.h"

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	/* used to store filenames of diffrent sounds */
	GHashTable *sound_files;
} PurpleSoundThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

G_DEFINE_TYPE_WITH_PRIVATE(PurpleSoundTheme, purple_sound_theme,
		PURPLE_TYPE_THEME);

/******************************************************************************
 * Enums
 *****************************************************************************/

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
purple_sound_theme_init(PurpleSoundTheme *theme)
{
	PurpleSoundThemePrivate *priv;

	priv = purple_sound_theme_get_instance_private(theme);

	priv->sound_files = g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, g_free);
}

static void
purple_sound_theme_finalize(GObject *obj)
{
	PurpleSoundThemePrivate *priv;

	priv = purple_sound_theme_get_instance_private(PURPLE_SOUND_THEME(obj));

	g_hash_table_destroy(priv->sound_files);

	G_OBJECT_CLASS(purple_sound_theme_parent_class)->finalize(obj);
}

static void
purple_sound_theme_class_init(PurpleSoundThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_sound_theme_finalize;
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

const gchar *
purple_sound_theme_get_file(PurpleSoundTheme *theme,
		const gchar *event)
{
	PurpleSoundThemePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_SOUND_THEME(theme), NULL);

	priv = purple_sound_theme_get_instance_private(theme);

	return g_hash_table_lookup(priv->sound_files, event);
}

gchar *
purple_sound_theme_get_file_full(PurpleSoundTheme *theme,
		const gchar *event)
{
	const gchar *filename;

	g_return_val_if_fail(PURPLE_IS_SOUND_THEME(theme), NULL);

	filename = purple_sound_theme_get_file(theme, event);

	g_return_val_if_fail(filename, NULL);

	return g_build_filename(purple_theme_get_dir(PURPLE_THEME(theme)), filename, NULL);
}

void
purple_sound_theme_set_file(PurpleSoundTheme *theme,
		const gchar *event,
		const gchar *filename)
{
	PurpleSoundThemePrivate *priv;
	g_return_if_fail(PURPLE_IS_SOUND_THEME(theme));

	priv = purple_sound_theme_get_instance_private(theme);

	if (filename != NULL)
		g_hash_table_replace(priv->sound_files,
				g_strdup(event), g_strdup(filename));
	else
		g_hash_table_remove(priv->sound_files, event);
}
