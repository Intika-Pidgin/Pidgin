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

#ifndef PURPLE_ACCOUNT_USER_SPLIT_H
#define PURPLE_ACCOUNT_USER_SPLIT_H

/**
 * SECTION:purpleaccountusersplit
 * @section_id: libpurple-account-user-split
 * @short_description: Username splitting
 * @title: Account Username Splitting API
 */

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_ACCOUNT_USER_SPLIT (purple_account_user_split_get_type())

/**
 * PurpleAccountUserSplit:
 *
 * A username split.
 *
 * This is used by some protocols to separate the fields of the username
 * into more human-readable components.
 */
typedef struct _PurpleAccountUserSplit	PurpleAccountUserSplit;

G_BEGIN_DECLS

GType purple_account_user_split_get_type(void);

/**
 * purple_account_user_split_new:
 * @text:          The text of the option.
 * @default_value: The default value.
 * @sep:           The field separator.
 *
 * Creates a new account username split.
 *
 * Returns: The new user split.
 */
PurpleAccountUserSplit *purple_account_user_split_new(const gchar *text, const gchar *default_value, gchar sep);

/**
 * purple_account_user_split_copy:
 * @split: The split to copy.
 *
 * Creates a copy of @split.
 *
 * Returns: (transfer full): A copy of @split.
 *
 * Since: 3.0.0
 */
PurpleAccountUserSplit *purple_account_user_split_copy(PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_destroy:
 * @split: The split to destroy.
 *
 * Destroys an account username split.
 */
void purple_account_user_split_destroy(PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_text:
 * @split: The account username split.
 *
 * Returns the text for an account username split.
 *
 * Returns: The account username split's text.
 */
const gchar *purple_account_user_split_get_text(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_default_value:
 * @split: The account username split.
 *
 * Returns the default string value for an account split.
 *
 * Returns: The default string.
 */
const gchar *purple_account_user_split_get_default_value(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_separator:
 * @split: The account username split.
 *
 * Returns the field separator for an account split.
 *
 * Returns: The field separator.
 */
gchar purple_account_user_split_get_separator(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_get_reverse:
 * @split: The account username split.
 *
 * Returns the 'reverse' value for an account split.
 *
 * Returns: The 'reverse' value.
 */
gboolean purple_account_user_split_get_reverse(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_set_reverse:
 * @split:   The account username split.
 * @reverse: The 'reverse' value
 *
 * Sets the 'reverse' value for an account split.
 */
void purple_account_user_split_set_reverse(PurpleAccountUserSplit *split, gboolean reverse);

/**
 * purple_account_user_split_is_constant:
 * @split: The account username split.
 *
 * Returns the constant parameter for an account split.
 *
 * When split is constant, it does not need to be displayed
 * in configuration dialog.
 *
 * Returns: %TRUE, if the split is constant.
 */
gboolean purple_account_user_split_is_constant(const PurpleAccountUserSplit *split);

/**
 * purple_account_user_split_set_constant:
 * @split:    The account username split.
 * @constant: %TRUE, if the split is a constant part.
 *
 * Sets the constant parameter of account split.
 */
void purple_account_user_split_set_constant(PurpleAccountUserSplit *split, gboolean constant);

G_END_DECLS

#endif /* PURPLE_ACCOUNT_USER_SPLIT_H */
