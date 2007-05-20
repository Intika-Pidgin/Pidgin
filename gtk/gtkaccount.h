/**
 * @file gtkaccount.h GTK+ Account Editor UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_GTKACCOUNT_H_
#define _GAIM_GTKACCOUNT_H_

#include "account.h"

typedef enum
{
	GAIM_GTK_ADD_ACCOUNT_DIALOG,
	GAIM_GTK_MODIFY_ACCOUNT_DIALOG

} GaimGtkAccountDialogType;


/**
 * Shows the accounts window.
 */
void gaim_gtk_accounts_window_show(void);

/**
 * Hides the accounts window.
 */
void gaim_gtk_accounts_window_hide(void);

/**
 * Shows an add/modify account dialog.
 *
 * @param type    The type of dialog.
 * @param account The associated account, or @c NULL for an Add dialog.
 */
void gaim_gtk_account_dialog_show(GaimGtkAccountDialogType type,
								  GaimAccount *account);

/**
 * Returns the GTK+ account UI ops
 *
 * @return The UI operations structure.
 */
GaimAccountUiOps *gaim_gtk_accounts_get_ui_ops(void);

/**
 * Returns the gtkaccounts handle
 *
 * @return The handle to the GTK+ account system
 */
void *gaim_gtk_account_get_handle(void);

/**
 * Initializes the GTK+ account system
 */
void gaim_gtk_account_init(void);

/**
 * Uninitializes the GTK+ account system
 */
void gaim_gtk_account_uninit(void);

#endif /* _GAIM_GTKACCOUNT_H_ */
