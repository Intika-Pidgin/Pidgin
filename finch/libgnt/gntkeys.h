/**
 * @file gntkeys.h Keys API
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

#ifndef GNT_KEYS_H
#define GNT_KEYS_H

#include <curses.h>
#include <term.h>

/**
 * terminfo/termcap doesn't provide all the information that I want to use, eg.
 * ctrl-up, ctrl-down etc. So I am going to hard-code some of the information
 * for some popular $TERMs
 */
extern char *gnt_key_cup;
extern char *gnt_key_cdown;
extern char *gnt_key_cleft;
extern char *gnt_key_cright;

#define SAFE(x)   ((x) ? (x) : "")

#define GNT_KEY_POPUP   SAFE(key_f16)   /* Apparently */

/* Arrow keys */
#define GNT_KEY_LEFT   SAFE(key_left)
#define GNT_KEY_RIGHT  SAFE(key_right)
#define GNT_KEY_UP     SAFE(key_up)
#define GNT_KEY_DOWN   SAFE(key_down)

#define GNT_KEY_CTRL_UP     SAFE(gnt_key_cup)
#define GNT_KEY_CTRL_DOWN   SAFE(gnt_key_cdown)
#define GNT_KEY_CTRL_RIGHT  SAFE(gnt_key_cright)
#define GNT_KEY_CTRL_LEFT   SAFE(gnt_key_cleft)

#define GNT_KEY_PGUP   SAFE(key_ppage)
#define GNT_KEY_PGDOWN SAFE(key_npage)
#define GNT_KEY_HOME   SAFE(key_home)
#define GNT_KEY_END    SAFE(key_end)

#define GNT_KEY_ENTER  carriage_return

#define GNT_KEY_BACKSPACE SAFE(key_backspace)
#define GNT_KEY_DEL    SAFE(key_dc)
#define GNT_KEY_INS    SAFE(key_ic)
#define GNT_KEY_BACK_TAB (back_tab ? back_tab : SAFE(key_btab))

#define GNT_KEY_CTRL_A     "\001"
#define GNT_KEY_CTRL_B     "\002"
#define GNT_KEY_CTRL_D     "\004"
#define GNT_KEY_CTRL_E     "\005"
#define GNT_KEY_CTRL_F     "\006"
#define GNT_KEY_CTRL_G     "\007"
#define GNT_KEY_CTRL_H     "\010"
#define GNT_KEY_CTRL_I     "\011"
#define GNT_KEY_CTRL_J     "\012"
#define GNT_KEY_CTRL_K     "\013"
#define GNT_KEY_CTRL_L     "\014"
#define GNT_KEY_CTRL_M     "\012"
#define GNT_KEY_CTRL_N     "\016"
#define GNT_KEY_CTRL_O     "\017"
#define GNT_KEY_CTRL_P     "\020"
#define GNT_KEY_CTRL_R     "\022"
#define GNT_KEY_CTRL_T     "\024"
#define GNT_KEY_CTRL_U     "\025"
#define GNT_KEY_CTRL_V     "\026"
#define GNT_KEY_CTRL_W     "\027"
#define GNT_KEY_CTRL_X     "\030"
#define GNT_KEY_CTRL_Y     "\031"

#define GNT_KEY_F1         SAFE(key_f1)
#define GNT_KEY_F2         SAFE(key_f2)
#define GNT_KEY_F3         SAFE(key_f3)
#define GNT_KEY_F4         SAFE(key_f4)
#define GNT_KEY_F5         SAFE(key_f5)
#define GNT_KEY_F6         SAFE(key_f6)
#define GNT_KEY_F7         SAFE(key_f7)
#define GNT_KEY_F8         SAFE(key_f8)
#define GNT_KEY_F9         SAFE(key_f9)
#define GNT_KEY_F10        SAFE(key_f10)
#define GNT_KEY_F11        SAFE(key_f11)
#define GNT_KEY_F12        SAFE(key_f12)

/**
 * Initialize the keys.
 */
void gnt_init_keys(void);

/**
 * Refine input text. This usually looks at what the terminal claims it is,
 * and tries to change the text to work around some oft-broken terminfo entries.
 *
 * @param text  The input text to refine.
 */
void gnt_keys_refine(char *text);

/**
 * Translate a user-readable representation of an input to a machine-readable representation.
 *
 * @param name   The user-readable representation of an input (eg.: c-t)
 *
 * @return  A machine-readable representation of the input.
 */
const char *gnt_key_translate(const char *name);

/**
 * Translate a machine-readable representation of an input to a user-readable representation.
 *
 * @param key  The machine-readable representation of an input.
 *
 * @return  A user-readable representation of the input (eg.: c-t).
 */
const char *gnt_key_lookup(const char *key);

/**
 * Add a key combination to the internal key-tree.
 *
 * @param key  The key to add
 */
void gnt_keys_add_combination(const char *key);

/**
 * Remove a key combination from the internal key-tree.
 *
 * @param key The key to remove.
 */
void gnt_keys_del_combination(const char *key);

/**
 * Find a combination from the given string.
 *
 * @param key  The input string.
 *
 * @return The number of bytes in the combination that starts at the beginning
 *         of key (can be 0).
 */
int gnt_keys_find_combination(const char *key);

/* A lot of commonly used variable names are defined in <term.h>.
 * #undef them to make life easier for everyone. */

#undef columns
#undef lines
#undef buttons
#undef newline
#undef set_clock

#endif
