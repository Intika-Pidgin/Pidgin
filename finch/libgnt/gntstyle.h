/**
 * @file gntstyle.h Style API
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include "gnt.h"
#include "gntwm.h"

typedef enum
{
	GNT_STYLE_SHADOW = 0,
	GNT_STYLE_COLOR = 1,
	GNT_STYLE_MOUSE = 2,
	GNT_STYLE_WM = 3,
	GNT_STYLE_REMPOS = 4,
	GNT_STYLES
} GntStyle;

/**
 * 
 * @param filename
 */
void gnt_style_read_configure_file(const char *filename);

const char *gnt_style_get(GntStyle style);

/**
 * Get the value of a preference in ~/.gntrc.
 *
 * @param group   The name of the group in the keyfile. If @c NULL, the prgname
 *                will be used first, if available. Otherwise, "general" will be used.
 * @param key     The key
 *
 * @return  The value of the setting as a string, or @c NULL
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
char *gnt_style_get_from_name(const char *group, const char *key);

/**
 * Parse a boolean preference. For example, if 'value' is "false" (ignoring case)
 * or "0", the return value will be @c FALSE, otherwise @c TRUE.
 *
 * @param value   The value of the boolean setting as a string
 * @return    The boolean value
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
gboolean gnt_style_parse_bool(const char *value);

/**
 * 
 * @param style
 * @param def
 *
 * @return
 */
gboolean gnt_style_get_bool(GntStyle style, gboolean def);

/* This should be called only once for the each type */
/**
 * 
 * @param type
 * @param hash
 */
void gnt_styles_get_keyremaps(GType type, GHashTable *hash);

/**
 * 
 * @param type
 * @param klass
 */
void gnt_style_read_actions(GType type, GntBindableClass *klass);

/*
 * Read menu-accels from ~/.gntrc
 *
 * @param name  The name of the window.
 * @param table The hastable to store the accel information.
 *
 * @return  @c TRUE if some accels were read, @c FALSE otherwise.
 */
gboolean gnt_style_read_menu_accels(const char *name, GHashTable *table);

void gnt_style_read_workspaces(GntWM *wm);

/**
 * 
 */
void gnt_init_styles(void);

/**
 * 
 */
void gnt_uninit_styles(void);

