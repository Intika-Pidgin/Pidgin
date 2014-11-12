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

#ifndef _PURPLE_ACCOUNTS_H_
#define _PURPLE_ACCOUNTS_H_
/**
 * SECTION:accounts
 * @section_id: libpurple-accounts
 * @short_description: <filename>accounts.h</filename>
 * @title: Accounts Subsystem API
 * @see_also: <link linkend="chapter-signals-account">Account signals</link>
 */

#include "account.h"
#include "status.h"

#define PURPLE_TYPE_ACCOUNT_UI_OPS (purple_account_ui_ops_get_type())

typedef struct _PurpleAccountUiOps  PurpleAccountUiOps;

/**
 * PurpleAccountUiOps:
 * @notify_added:          A buddy who is already on this account's buddy list
 *                         added this account to their buddy list.
 * @status_changed:        This account's status changed.
 * @request_add:           Someone we don't have on our list added us; prompt
 *                         to add them.
 * @request_authorize:     Prompt for authorization when someone adds this
 *                         account to their buddy list.  To authorize them to
 *                         see this account's presence, call
 *                         @authorize_cb (@message, @user_data) otherwise call
 *                         @deny_cb (@message, @user_data).
 *                         <sbr/>Returns: A UI-specific handle, as passed to
 *                         @close_account_request.
 * @close_account_request: Close a pending request for authorization.
 *                         @ui_handle is a handle as returned by
 *                         @request_authorize.
 *
 * Account UI operations, used to notify the user of status changes and when
 * buddies add this account to their buddy lists.
 */
struct _PurpleAccountUiOps
{
	void (*notify_added)(PurpleAccount *account,
	                     const char *remote_user,
	                     const char *id,
	                     const char *alias,
	                     const char *message);

	void (*status_changed)(PurpleAccount *account,
	                       PurpleStatus *status);

	void (*request_add)(PurpleAccount *account,
	                    const char *remote_user,
	                    const char *id,
	                    const char *alias,
	                    const char *message);

	void *(*request_authorize)(PurpleAccount *account,
	                           const char *remote_user,
	                           const char *id,
	                           const char *alias,
	                           const char *message,
	                           gboolean on_list,
	                           PurpleAccountRequestAuthorizationCb authorize_cb,
	                           PurpleAccountRequestAuthorizationCb deny_cb,
	                           void *user_data);

	void (*close_account_request)(void *ui_handle);

	void (*permit_added)(PurpleAccount *account, const char *name);
	void (*permit_removed)(PurpleAccount *account, const char *name);
	void (*deny_added)(PurpleAccount *account, const char *name);
	void (*deny_removed)(PurpleAccount *account, const char *name);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* Accounts API                                                           */
/**************************************************************************/

/**
 * purple_accounts_add:
 * @account: The account.
 *
 * Adds an account to the list of accounts.
 */
void purple_accounts_add(PurpleAccount *account);

/**
 * purple_accounts_remove:
 * @account: The account.
 *
 * Removes an account from the list of accounts.
 */
void purple_accounts_remove(PurpleAccount *account);

/**
 * purple_accounts_delete:
 * @account: The account.
 *
 * Deletes an account.
 *
 * This will remove any buddies from the buddy list that belong to this
 * account, buddy pounces that belong to this account, and will also
 * destroy @account.
 */
void purple_accounts_delete(PurpleAccount *account);

/**
 * purple_accounts_reorder:
 * @account:   The account to reorder.
 * @new_index: The new index for the account.
 *
 * Reorders an account.
 */
void purple_accounts_reorder(PurpleAccount *account, guint new_index);

/**
 * purple_accounts_get_all:
 *
 * Returns a list of all accounts.
 *
 * Returns: (transfer none): A list of all accounts.
 */
GList *purple_accounts_get_all(void);

/**
 * purple_accounts_get_all_active:
 *
 * Returns a list of all enabled accounts
 *
 * Returns: A list of all enabled accounts. The list is owned
 *         by the caller, and must be g_list_free()d to avoid
 *         leaking the nodes.
 */
GList *purple_accounts_get_all_active(void);

/**
 * purple_accounts_find:
 * @name:     The account username.
 * @protocol: The account protocol ID.
 *
 * Finds an account with the specified name and protocol id.
 *
 * Returns: The account, if found, or %FALSE otherwise.
 */
PurpleAccount *purple_accounts_find(const char *name, const char *protocol);

/**
 * purple_accounts_restore_current_statuses:
 *
 * This is called by the core after all subsystems and what
 * not have been initialized.  It sets all enabled accounts
 * to their startup status by signing them on, setting them
 * away, etc.
 *
 * You probably shouldn't call this unless you really know
 * what you're doing.
 */
void purple_accounts_restore_current_statuses(void);


/**************************************************************************/
/* UI Registration Functions                                              */
/**************************************************************************/

/**
 * purple_account_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleAccountUiOps boxed structure.
 */
GType purple_account_ui_ops_get_type(void);

/**
 * purple_accounts_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used for accounts.
 */
void purple_accounts_set_ui_ops(PurpleAccountUiOps *ops);

/**
 * purple_accounts_get_ui_ops:
 *
 * Returns the UI operations structure used for accounts.
 *
 * Returns: The UI operations structure in use.
 */
PurpleAccountUiOps *purple_accounts_get_ui_ops(void);


/**************************************************************************/
/* Accounts Subsystem                                                     */
/**************************************************************************/

/**
 * purple_accounts_get_handle:
 *
 * Returns the accounts subsystem handle.
 *
 * Returns: The accounts subsystem handle.
 */
void *purple_accounts_get_handle(void);

/**
 * purple_accounts_init:
 *
 * Initializes the accounts subsystem.
 */
void purple_accounts_init(void);

/**
 * purple_accounts_uninit:
 *
 * Uninitializes the accounts subsystem.
 */
void purple_accounts_uninit(void);

/**
 * purple_accounts_schedule_save:
 *
 * Schedules saving of accounts
 */
void purple_accounts_schedule_save(void);

G_END_DECLS

#endif /* _PURPLE_ACCOUNTS_H_ */
