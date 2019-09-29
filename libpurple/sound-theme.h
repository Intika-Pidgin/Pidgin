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

#ifndef PURPLE_SOUND_THEME_H
#define PURPLE_SOUND_THEME_H
/**
 * SECTION:sound-theme
 * @section_id: libpurple-sound-theme
 * @short_description: <filename>sound-theme.h</filename>
 * @title: Sound Theme Class
 */

#include <glib.h>
#include <glib-object.h>
#include "theme.h"
#include "sound.h"

#define PURPLE_TYPE_SOUND_THEME  purple_sound_theme_get_type()

/**************************************************************************/
/* Purple Sound Theme API                                                 */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * purple_sound_theme_get_type:
 *
 * Returns: The #GType for a sound theme.
 */
G_DECLARE_FINAL_TYPE(PurpleSoundTheme, purple_sound_theme, PURPLE, SOUND_THEME,
		PurpleTheme)

/**
 * purple_sound_theme_get_file:
 * @theme: The theme.
 * @event: The purple sound event to look up.
 *
 * Returns a copy of the filename for the sound event.
 *
 * Returns: The filename of the sound event.
 */
const gchar *purple_sound_theme_get_file(PurpleSoundTheme *theme,
		const gchar *event);

/**
 * purple_sound_theme_get_file_full:
 * @theme: The theme.
 * @event: The purple sound event to look up
 *
 * Returns a copy of the directory and filename for the sound event
 *
 * Returns: The directory + '/' + filename of the sound event.  This is
 *          a newly allocated string that should be freed with g_free.
 */
gchar *purple_sound_theme_get_file_full(PurpleSoundTheme *theme,
		const gchar *event);

/**
 * purple_sound_theme_set_file:
 * @theme:    The theme.
 * @event:    the purple sound event to look up
 * @filename: the name of the file to be used for the event
 *
 * Sets the filename for a given sound event
 */
void purple_sound_theme_set_file(PurpleSoundTheme *theme,
		const gchar *event,
		const gchar *filename);

G_END_DECLS

#endif /* PURPLE_SOUND_THEME_H */
