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

#ifndef PURPLE_SOUND_THEME_LOADER_H
#define PURPLE_SOUND_THEME_LOADER_H
/**
 * SECTION:sound-theme-loader
 * @section_id: libpurple-sound-theme-loader
 * @short_description: <filename>sound-theme-loader.h</filename>
 * @title: Sound Theme Loader Class
 */

#include <glib.h>
#include <glib-object.h>
#include "theme-loader.h"

#define PURPLE_TYPE_SOUND_THEME_LOADER  purple_sound_theme_loader_get_type()

/**************************************************************************/
/* Purple Theme-Loader API                                                */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * purple_sound_theme_loader_get_type:
 *
 * Returns: The #GType for sound theme loader.
 */
G_DECLARE_FINAL_TYPE(PurpleSoundThemeLoader, purple_sound_theme_loader, PURPLE,
		SOUND_THEME_LOADER, PurpleThemeLoader)

G_END_DECLS
#endif /* PURPLE_SOUND_THEME_LOADER_H */
