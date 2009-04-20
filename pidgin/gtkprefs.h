/**
 * @file gtkprefs.h GTK+ Preferences
 * @ingroup pidgin
 */

/* pidgin
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
 *
 */
#ifndef _PIDGINPREFS_H_
#define _PIDGINPREFS_H_

#include "prefs.h"

/**
 * Initializes all UI-specific preferences.
 */
void pidgin_prefs_init(void);

/**
 * Shows the preferences dialog.
 */
void pidgin_prefs_show(void);

/**
 * Add a new checkbox for a boolean preference
 *
 * @param title The text to be displayed as the checkbox label
 * @param key   The key of the purple bool pref that will be represented by the checkbox
 * @param page  The page to which the new checkbox will be added
 */
GtkWidget *pidgin_prefs_checkbox(const char *title, const char *key,
		GtkWidget *page);

/**
 * Add a new spin button representing an int preference
 *
 * @param page  The page to which the spin button will be added
 * @param title The text to be displayed as the spin button label
 * @param key   The key of the int pref that will be represented by the spin button
 * @param min   The minimum value of the spin button
 * @param max   The maximum value of the spin button
 * @param sg    If not NULL, the size group to which the spin button will be added
 * @return      An hbox containing both the label and the spinner.  Can be
 *              used to set the widgets to sensitive or insensitive based on the
 *              value of a checkbox.
 */
GtkWidget *pidgin_prefs_labeled_spin_button(GtkWidget *page,
		const gchar *title, const char *key, int min, int max, GtkSizeGroup *sg);

/**
 * Add a new entry representing a string preference
 *
 * @param page  The page to which the entry will be added
 * @param title The text to be displayed as the entry label
 * @param key   The key of the string pref that will be represented by the entry
 * @param sg    If not NULL, the size group to which the entry will be added
 *
 * @return      An hbox containing both the label and the entry.  Can be used to set
 *               the widgets to sensitive or insensitive based on the value of a
 *               checkbox.
 */
GtkWidget *pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
										const char *key, GtkSizeGroup *sg);

/**
 * Add a new entry representing a password (string) preference
 * The entry will use a password-style text entry (the text is substituded)
 *
 * @param page  The page to which the entry will be added
 * @param title The text to be displayed as the entry label
 * @param key   The key of the string pref that will be represented by the entry
 * @param sg    If not NULL, the size group to which the entry will be added
 *
 * @return      An hbox containing both the label and the entry.  Can be used to set
 *               the widgets to sensitive or insensitive based on the value of a
 *               checkbox.
 *
 * @since 2.6.0
 */
GtkWidget *pidgin_prefs_labeled_password(GtkWidget *page, const gchar *title,
										const char *key, GtkSizeGroup *sg);

/**
 * Add a new dropdown representing a preference of the specified type
 *
 * @param page  The page to which the dropdown will be added
 * @param title The text to be displayed as the dropdown label
 * @param type  The type of preference to be stored in the generated dropdown
 * @param key   The key of the pref that will be represented by the dropdown
 * @param ...   The choices to be added to the dropdown, choices should be
 *              paired as label/value
 */
GtkWidget *pidgin_prefs_dropdown(GtkWidget *page, const gchar *title,
		PurplePrefType type, const char *key, ...);

/**
 * Add a new dropdown representing a preference of the specified type
 *
 * @param page      The page to which the dropdown will be added
 * @param title     The text to be displayed as the dropdown label
 * @param type      The type of preference to be stored in the dropdown
 * @param key       The key of the pref that will be represented by the dropdown
 * @param menuitems The choices to be added to the dropdown, choices should
 *                  be paired as label/value
 */
GtkWidget *pidgin_prefs_dropdown_from_list(GtkWidget *page,
		const gchar * title, PurplePrefType type, const char *key,
		GList *menuitems);

/**
 * Rename legacy prefs and delete some that no longer exist.
 */
void pidgin_prefs_update_old(void);

#endif /* _PIDGINPREFS_H_ */
