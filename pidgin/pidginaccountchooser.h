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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#ifndef PIDGIN_ACCOUNT_CHOOSER_H
#define PIDGIN_ACCOUNT_CHOOSER_H
/**
 * SECTION:pidginaccountchooser
 * @section_id: pidgin-account-chooser
 * @short_description: A #GtkComboBox for choosing accounts
 * @title: Pidgin Account Chooser Combo Box Widget
 */

#include <gtk/gtk.h>

#include "pidgin.h"

#include "account.h"

G_BEGIN_DECLS

#define PIDGIN_TYPE_ACCOUNT_CHOOSER (pidgin_account_chooser_get_type())

G_DECLARE_FINAL_TYPE(PidginAccountChooser, pidgin_account_chooser, PIDGIN,
                     ACCOUNT_CHOOSER, GtkComboBox)

/**
 * pidgin_account_chooser_new:
 * @default_account: The account to select by default.
 * @show_all: Whether or not to show all accounts, or just active accounts.
 *
 * Creates a combo box filled with accounts.
 *
 * Returns: (transfer full): The account chooser combo box.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_account_chooser_new(PurpleAccount *default_account,
                                      gboolean show_all);

/**
 * pidgin_account_chooser_set_filter_func:
 * @chooser: The account chooser combo box.
 * @filter_func: (scope notified): A function for checking if an account should be shown. This
 *               can be %NULL to remove any filter.
 *
 * Set the filter function used to determine which accounts to show.
 *
 * Since: 3.0.0
 */
void pidgin_account_chooser_set_filter_func(
        PidginAccountChooser *chooser, PurpleFilterAccountFunc filter_func);

/**
 * pidgin_account_chooser_get_selected:
 * @chooser: The chooser created by pidgin_account_chooser_new().
 *
 * Gets the currently selected account from an account combo box.
 *
 * Returns: (transfer none): Returns the #PurpleAccount that is currently
 *          selected.
 *
 * Since: 3.0.0
 */
PurpleAccount *pidgin_account_chooser_get_selected(GtkWidget *chooser);

/**
 * pidgin_account_chooser_set_selected:
 * @chooser: The chooser created by pidgin_account_chooser_new().
 * @account: The #PurpleAccount to select.
 *
 * Sets the currently selected account for an account combo box.
 *
 * Since: 3.0.0
 */
void pidgin_account_chooser_set_selected(GtkWidget *chooser,
                                         PurpleAccount *account);

G_END_DECLS

#endif /* PIDGIN_ACCOUNT_CHOOSER_H */
