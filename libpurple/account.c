/**
 * @file account.c Account API
 * @ingroup core
 */

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
#include "internal.h"
#include "accounts.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "keyring.h"
#include "network.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "status.h"
#include "util.h"

#define PURPLE_ACCOUNT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_ACCOUNT, PurpleAccountPrivate))

typedef struct
{
	char *username;             /**< The username.                          */
	char *alias;                /**< How you appear to yourself.            */
	char *password;             /**< The account password.                  */
	char *user_info;            /**< User information.                      */

	char *buddy_icon_path;      /**< The buddy icon's non-cached path.      */

	gboolean remember_pass;     /**< Remember the password.                 */

	char *protocol_id;          /**< The ID of the protocol.                */

	PurpleConnection *gc;         /**< The connection handle.               */
	gboolean disconnecting;     /**< The account is currently disconnecting */

	GHashTable *settings;       /**< Protocol-specific settings.            */
	GHashTable *ui_settings;    /**< UI-specific settings.                  */

	PurpleProxyInfo *proxy_info;  /**< Proxy information.  This will be set */
								/*   to NULL when the account inherits      */
								/*   proxy settings from global prefs.      */

	/*
	 * TODO: Supplementing the next two linked lists with hash tables
	 * should help performance a lot when these lists are long.  This
	 * matters quite a bit for protocols like MSN, where all your
	 * buddies are added to your permit list.  Currently we have to
	 * iterate through the entire list if we want to check if someone
	 * is permitted or denied.  We should do this for 3.0.0.
	 * Or maybe use a GTree.
	 */
	GSList *permit;             /**< Permit list.                           */
	GSList *deny;               /**< Deny list.                             */
	PurpleAccountPrivacyType perm_deny;  /**< The permit/deny setting.      */

	GList *status_types;        /**< Status types.                          */

	PurplePresence *presence;     /**< Presence.                            */
	PurpleLog *system_log;        /**< The system log                       */

	void *ui_data;              /**< The UI can put data here.              */
	PurpleAccountRegistrationCb registration_cb;
	void *registration_cb_user_data;

	PurpleConnectionErrorInfo *current_error;	/**< Errors */
} PurpleAccountPrivate;

#if 0
/* TODO Use GValue instead */
typedef struct
{
	PurplePrefType type;

	char *ui;

	union
	{
		int integer;
		char *string;
		gboolean boolean;

	} value;

} PurpleAccountSetting;
#endif

typedef struct
{
	PurpleAccountRequestType type;
	PurpleAccount *account;
	void *ui_handle;
	char *user;
	gpointer userdata;
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	guint ref;
} PurpleAccountRequestInfo;

typedef struct
{
	PurpleCallback cb;
	gpointer data;
} PurpleCallbackBundle;

static void set_current_error(PurpleAccount *account,
	PurpleConnectionErrorInfo *new_err);


PurpleAccount *
purple_account_new(const char *username, const char *protocol_id)
{
	PurpleAccount *account = NULL;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleStatusType *status_type;

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	account = purple_accounts_find(username, protocol_id);

	if (account != NULL)
		return account;

	account = g_new0(PurpleAccount, 1);
	PURPLE_DBUS_REGISTER_POINTER(account, PurpleAccount);

	purple_account_set_username(account, username);

	purple_account_set_protocol_id(account, protocol_id);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
	account->ui_settings = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, (GDestroyNotify)g_hash_table_destroy);
	account->system_log = NULL;
	/* 0 is not a valid privacy setting */
	account->perm_deny = PURPLE_PRIVACY_ALLOW_ALL;

	purple_signal_emit(purple_accounts_get_handle(), "account-created", account);

	prpl = purple_find_prpl(protocol_id);

	if (prpl == NULL)
		return account;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info != NULL && prpl_info->status_types != NULL)
		purple_account_set_status_types(account, prpl_info->status_types(account));

	account->presence = purple_presence_new_for_account(account);

	status_type = purple_account_get_status_type_with_primitive(account, PURPLE_STATUS_AVAILABLE);
	if (status_type != NULL)
		purple_presence_set_status_active(account->presence,
										purple_status_type_get_id(status_type),
										TRUE);
	else
		purple_presence_set_status_active(account->presence,
										"offline",
										TRUE);

	return account;
}

void
purple_account_destroy(PurpleAccount *account)
{
	GList *l;

	g_return_if_fail(account != NULL);

	purple_debug_info("account", "Destroying account %p\n", account);
	purple_signal_emit(purple_accounts_get_handle(), "account-destroying", account);

	for (l = purple_get_conversations(); l != NULL; l = l->next)
	{
		PurpleConversation *conv = (PurpleConversation *)l->data;

		if (purple_conversation_get_account(conv) == account)
			purple_conversation_set_account(conv, NULL);
	}

	g_free(account->username);
	g_free(account->alias);
	purple_str_wipe(account->password);
	g_free(account->user_info);
	g_free(account->buddy_icon_path);
	g_free(account->protocol_id);

	g_hash_table_destroy(account->settings);
	g_hash_table_destroy(account->ui_settings);

	if (account->proxy_info)
		purple_proxy_info_destroy(account->proxy_info);

	purple_account_set_status_types(account, NULL);

	if (account->presence)
		purple_presence_destroy(account->presence);

	if(account->system_log)
		purple_log_free(account->system_log);

	while (account->deny) {
		g_free(account->deny->data);
		account->deny = g_slist_delete_link(account->deny, account->deny);
	}

	while (account->permit) {
		g_free(account->permit->data);
		account->permit = g_slist_delete_link(account->permit, account->permit);
	}

	PURPLE_DBUS_UNREGISTER_POINTER(account->current_error);
	if (account->current_error) {
		g_free(account->current_error->description);
		g_free(account->current_error);
	}

	PURPLE_DBUS_UNREGISTER_POINTER(account);
	g_free(account);
}

void
purple_account_set_register_callback(PurpleAccount *account, PurpleAccountRegistrationCb cb, void *user_data)
{
	g_return_if_fail(account != NULL);

	account->registration_cb = cb;
	account->registration_cb_user_data = user_data;
}

static void
purple_account_register_got_password_cb(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	g_return_if_fail(account != NULL);

	_purple_connection_new(account, TRUE, password);
}

void
purple_account_register(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	purple_debug_info("account", "Registering account %s\n",
					purple_account_get_username(account));

	purple_keyring_get_password(account,
		purple_account_register_got_password_cb, NULL);
}

static void
purple_account_unregister_got_password_cb(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	PurpleCallbackBundle *cbb = data;
	PurpleAccountUnregistrationCb cb;

	cb = (PurpleAccountUnregistrationCb)cbb->cb;
	_purple_connection_new_unregister(account, password, cb, cbb->data);

	g_free(cbb);
}

void
purple_account_register_completed(PurpleAccount *account, gboolean succeeded)
{
	g_return_if_fail(account != NULL);

	if (account->registration_cb)
		(account->registration_cb)(account, succeeded, account->registration_cb_user_data);
}

void
purple_account_unregister(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data)
{
	PurpleCallbackBundle *cbb;

	g_return_if_fail(account != NULL);

	purple_debug_info("account", "Unregistering account %s\n",
					  purple_account_get_username(account));

	cbb = g_new0(PurpleCallbackBundle, 1);
	cbb->cb = PURPLE_CALLBACK(cb);
	cbb->data = user_data;

	purple_keyring_get_password(account,
		purple_account_unregister_got_password_cb, cbb);
}

static void
request_password_ok_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	const char *entry;
	gboolean remember;

	entry = purple_request_fields_get_string(fields, "password");
	remember = purple_request_fields_get_bool(fields, "remember");

	if (!entry || !*entry)
	{
		purple_notify_error(account, NULL, _("Password is required to sign on."), NULL);
		return;
	}

	purple_account_set_remember_password(account, remember);

	purple_account_set_password(account, entry, NULL, NULL);
	_purple_connection_new(account, FALSE, entry);
}

static void
request_password_cancel_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	/* Disable the account as the user has cancelled connecting */
	purple_account_set_enabled(account, purple_core_get_ui(), FALSE);
}


void
purple_account_request_password(PurpleAccount *account, GCallback ok_cb,
				GCallback cancel_cb, void *user_data)
{
	gchar *primary;
	const gchar *username;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleRequestFields *fields;

	/* Close any previous password request windows */
	purple_request_close_with_handle(account);

	username = purple_account_get_username(account);
	primary = g_strdup_printf(_("Enter password for %s (%s)"), username,
								  purple_account_get_protocol_name(account));

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password", _("Enter Password"), NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_bool_new("remember", _("Save password"), FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(account,
                        NULL,
                        primary,
                        NULL,
                        fields,
                        _("OK"), ok_cb,
                        _("Cancel"), cancel_cb,
						account, NULL, NULL,
                        user_data);
	g_free(primary);
}

static void
purple_account_connect_got_password_cb(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	PurplePluginProtocolInfo *prpl_info = data;

	if ((password == NULL || *password == '\0') &&
		!(prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
		!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_account_request_password(account,
			G_CALLBACK(request_password_ok_cb),
			G_CALLBACK(request_password_cancel_cb), account);
	else
		_purple_connection_new(account, FALSE, password);
}

void
purple_account_connect(PurpleAccount *account)
{
	PurplePlugin *prpl;
	const char *username;
	PurplePluginProtocolInfo *prpl_info;

	g_return_if_fail(account != NULL);

	username = purple_account_get_username(account);

	if (!purple_account_get_enabled(account, purple_core_get_ui())) {
		purple_debug_info("account",
				  "Account %s not enabled, not connecting.\n",
				  username);
		return;
	}

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));
	if (prpl == NULL) {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol plugin for %s"), username);
		purple_notify_error(account, _("Connection Error"), message, NULL);
		g_free(message);
		return;
	}

	purple_debug_info("account", "Connecting to account %s.\n", username);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (account->password != NULL) {
		purple_account_connect_got_password_cb(account,
			account->password, NULL, prpl_info);
	} else {
		purple_keyring_get_password(account,
			purple_account_connect_got_password_cb, prpl_info);
	}
}

void
purple_account_disconnect(PurpleAccount *account)
{
	PurpleConnection *gc;
	const char *username;

	g_return_if_fail(account != NULL);
	g_return_if_fail(!purple_account_is_disconnected(account));

	username = purple_account_get_username(account);
	purple_debug_info("account", "Disconnecting account %s (%p)\n",
	                  username ? username : "(null)", account);

	account->disconnecting = TRUE;

	gc = purple_account_get_connection(account);
	_purple_connection_destroy(gc);
	purple_account_set_connection(account, NULL);

	account->disconnecting = FALSE;
}

gboolean
purple_account_is_disconnecting(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, TRUE);
	
	return account->disconnecting;
}

void
purple_account_notify_added(PurpleAccount *account, const char *remote_user,
                          const char *id, const char *alias,
                          const char *message)
{
	PurpleAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = purple_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->notify_added != NULL)
		ui_ops->notify_added(account, remote_user, id, alias, message);
}

void
purple_account_request_add(PurpleAccount *account, const char *remote_user,
                         const char *id, const char *alias,
                         const char *message)
{
	PurpleAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = purple_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add != NULL)
		ui_ops->request_add(account, remote_user, id, alias, message);
}

static PurpleAccountRequestInfo *
purple_account_request_info_unref(PurpleAccountRequestInfo *info)
{
	if (--info->ref)
		return info;

	/* TODO: This will leak info->user_data, but there is no callback to just clean that up */
	g_free(info->user);
	g_free(info);
	return NULL;
}

static void
purple_account_request_close_info(PurpleAccountRequestInfo *info)
{
	PurpleAccountUiOps *ops;

	ops = purple_accounts_get_ui_ops();

	if (ops != NULL && ops->close_account_request != NULL)
		ops->close_account_request(info->ui_handle);

	purple_account_request_info_unref(info);
}

void
purple_account_request_close_with_account(PurpleAccount *account)
{
	GList *l, *l_next;

	g_return_if_fail(account != NULL);

	for (l = handles; l != NULL; l = l_next) {
		PurpleAccountRequestInfo *info = l->data;

		l_next = l->next;

		if (info->account == account) {
			handles = g_list_remove(handles, info);
			purple_account_request_close_info(info);
		}
	}
}

void
purple_account_request_close(void *ui_handle)
{
	GList *l, *l_next;

	g_return_if_fail(ui_handle != NULL);

	for (l = handles; l != NULL; l = l_next) {
		PurpleAccountRequestInfo *info = l->data;

		l_next = l->next;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);
			purple_account_request_close_info(info);
		}
	}
}

static void
request_auth_cb(const char *message, void *data)
{
	PurpleAccountRequestInfo *info = data;

	handles = g_list_remove(handles, info);

	if (info->auth_cb != NULL)
		info->auth_cb(message, info->userdata);

	purple_signal_emit(purple_accounts_get_handle(),
			"account-authorization-granted", info->account, info->user, message);

	purple_account_request_info_unref(info);
}

static void
request_deny_cb(const char *message, void *data)
{
	PurpleAccountRequestInfo *info = data;

	handles = g_list_remove(handles, info);

	if (info->deny_cb != NULL)
		info->deny_cb(message, info->userdata);

	purple_signal_emit(purple_accounts_get_handle(),
			"account-authorization-denied", info->account, info->user, message);

	purple_account_request_info_unref(info);
}

void *
purple_account_request_authorization(PurpleAccount *account, const char *remote_user,
				     const char *id, const char *alias, const char *message, gboolean on_list,
				     PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb, void *user_data)
{
	PurpleAccountUiOps *ui_ops;
	PurpleAccountRequestInfo *info;
	int plugin_return;
	char *response = NULL;

	g_return_val_if_fail(account     != NULL, NULL);
	g_return_val_if_fail(remote_user != NULL, NULL);

	ui_ops = purple_accounts_get_ui_ops();

	plugin_return = GPOINTER_TO_INT(
			purple_signal_emit_return_1(
				purple_accounts_get_handle(),
				"account-authorization-requested",
				account, remote_user, message, &response
			));

	switch (plugin_return)
	{
		case PURPLE_ACCOUNT_RESPONSE_IGNORE:
			g_free(response);
			return NULL;
		case PURPLE_ACCOUNT_RESPONSE_ACCEPT:
			if (auth_cb != NULL)
				auth_cb(response, user_data);
			g_free(response);
			return NULL;
		case PURPLE_ACCOUNT_RESPONSE_DENY:
			if (deny_cb != NULL)
				deny_cb(response, user_data);
			g_free(response);
			return NULL;
	}

	g_free(response);

	if (ui_ops != NULL && ui_ops->request_authorize != NULL) {
		info            = g_new0(PurpleAccountRequestInfo, 1);
		info->type      = PURPLE_ACCOUNT_REQUEST_AUTHORIZATION;
		info->account   = account;
		info->auth_cb   = auth_cb;
		info->deny_cb   = deny_cb;
		info->userdata  = user_data;
		info->user      = g_strdup(remote_user);
		info->ref       = 2;  /* We hold an extra ref to make sure info remains valid
		                         if any of the callbacks are called synchronously. We
		                         unref it after the function call */

		info->ui_handle = ui_ops->request_authorize(account, remote_user, id, alias, message,
							    on_list, request_auth_cb, request_deny_cb, info);

		info = purple_account_request_info_unref(info);
		if (info) {
			handles = g_list_append(handles, info);
			return info->ui_handle;
		}
	}

	return NULL;
}

static void
change_password_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	const char *orig_pass, *new_pass_1, *new_pass_2;

	orig_pass  = purple_request_fields_get_string(fields, "password");
	new_pass_1 = purple_request_fields_get_string(fields, "new_password_1");
	new_pass_2 = purple_request_fields_get_string(fields, "new_password_2");

	if (g_utf8_collate(new_pass_1, new_pass_2))
	{
		purple_notify_error(account, NULL,
						  _("New passwords do not match."), NULL);

		return;
	}

	if ((purple_request_fields_is_field_required(fields, "password") &&
			(orig_pass == NULL || *orig_pass == '\0')) ||
		(purple_request_fields_is_field_required(fields, "new_password_1") &&
			(new_pass_1 == NULL || *new_pass_1 == '\0')) ||
		(purple_request_fields_is_field_required(fields, "new_password_2") &&
			(new_pass_2 == NULL || *new_pass_2 == '\0')))
	{
		purple_notify_error(account, NULL,
						  _("Fill out all fields completely."), NULL);
		return;
	}

	purple_account_change_password(account, orig_pass, new_pass_1);
}

void
purple_account_request_change_password(PurpleAccount *account)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleConnection *gc;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;
	char primary[256];

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);
	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password", _("Original password"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!prpl_info || !(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("new_password_1",
										  _("New password"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!prpl_info || !(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("new_password_2",
										  _("New password (again)"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!prpl_info || !(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	g_snprintf(primary, sizeof(primary), _("Change password for %s"),
			   purple_account_get_username(account));

	/* I'm sticking this somewhere in the code: bologna */

	purple_request_fields(purple_account_get_connection(account),
						NULL,
						primary,
						_("Please enter your current password and your "
						  "new password."),
						fields,
						_("OK"), G_CALLBACK(change_password_cb),
						_("Cancel"), NULL,
						account, NULL, NULL,
						account);
}

static void
set_user_info_cb(PurpleAccount *account, const char *user_info)
{
	PurpleConnection *gc;

	purple_account_set_user_info(account, user_info);
	gc = purple_account_get_connection(account);
	serv_set_info(gc, user_info);
}

void
purple_account_request_change_user_info(PurpleAccount *account)
{
	PurpleConnection *gc;
	char primary[256];

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);

	g_snprintf(primary, sizeof(primary),
			   _("Change user information for %s"),
			   purple_account_get_username(account));

	purple_request_input(gc, _("Set User Info"), primary, NULL,
					   purple_account_get_user_info(account),
					   TRUE, FALSE, ((gc != NULL) &&
					   (purple_connection_get_flags(gc) & PURPLE_CONNECTION_HTML) ? "html" : NULL),
					   _("Save"), G_CALLBACK(set_user_info_cb),
					   _("Cancel"), NULL,
					   account, NULL, NULL,
					   account);
}

void
purple_account_set_username(PurpleAccount *account, const char *username)
{
	PurpleBlistUiOps *blist_ops;

	g_return_if_fail(account != NULL);

	g_free(account->username);
	account->username = g_strdup(username);

	schedule_accounts_save();

	/* if the name changes, we should re-write the buddy list
	 * to disk with the new name */
	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);
}

void 
purple_account_set_password(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	g_return_if_fail(account != NULL);

	purple_str_wipe(account->password);
	account->password = g_strdup(password);

	schedule_accounts_save();

	if (!purple_account_get_remember_password(account)) {
		purple_debug_info("account",
			"Password for %s set, not sent to keyring.\n",
			purple_account_get_username(account));

		if (cb != NULL)
			cb(account, NULL, data);
	} else {
		purple_keyring_set_password(account, password, cb, data);
	}
}

void
purple_account_set_alias(PurpleAccount *account, const char *alias)
{
	g_return_if_fail(account != NULL);

	/*
	 * Do nothing if alias and account->alias are both NULL.  Or if
	 * they're the exact same string.
	 */
	if (alias == account->alias)
		return;

	if ((!alias && account->alias) || (alias && !account->alias) ||
			g_utf8_collate(account->alias, alias))
	{
		char *old = account->alias;

		account->alias = g_strdup(alias);
		purple_signal_emit(purple_accounts_get_handle(), "account-alias-changed",
						 account, old);
		g_free(old);

		schedule_accounts_save();
	}
}

void
purple_account_set_user_info(PurpleAccount *account, const char *user_info)
{
	g_return_if_fail(account != NULL);

	g_free(account->user_info);
	account->user_info = g_strdup(user_info);

	schedule_accounts_save();
}

void purple_account_set_buddy_icon_path(PurpleAccount *account, const char *path)
{
	g_return_if_fail(account != NULL);

	g_free(account->buddy_icon_path);
	account->buddy_icon_path = g_strdup(path);

	schedule_accounts_save();
}

void
purple_account_set_protocol_id(PurpleAccount *account, const char *protocol_id)
{
	g_return_if_fail(account     != NULL);
	g_return_if_fail(protocol_id != NULL);

	g_free(account->protocol_id);
	account->protocol_id = g_strdup(protocol_id);

	schedule_accounts_save();
}

void
purple_account_set_connection(PurpleAccount *account, PurpleConnection *gc)
{
	g_return_if_fail(account != NULL);

	account->gc = gc;
}

void
purple_account_set_remember_password(PurpleAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	account->remember_pass = value;

	schedule_accounts_save();
}

void
purple_account_set_check_mail(PurpleAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	purple_account_set_bool(account, "check-mail", value);
}

void
purple_account_set_enabled(PurpleAccount *account, const char *ui,
			 gboolean value)
{
	PurpleConnection *gc;
	gboolean was_enabled = FALSE;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);

	was_enabled = purple_account_get_enabled(account, ui);

	purple_account_set_ui_bool(account, ui, "auto-login", value);
	gc = purple_account_get_connection(account);

	if(was_enabled && !value)
		purple_signal_emit(purple_accounts_get_handle(), "account-disabled", account);
	else if(!was_enabled && value)
		purple_signal_emit(purple_accounts_get_handle(), "account-enabled", account);

	if ((gc != NULL) && (gc->wants_to_die == TRUE))
		return;

	if (value && purple_presence_is_online(account->presence))
		purple_account_connect(account);
	else if (!value && !purple_account_is_disconnected(account))
		purple_account_disconnect(account);
}

void
purple_account_set_proxy_info(PurpleAccount *account, PurpleProxyInfo *info)
{
	g_return_if_fail(account != NULL);

	if (account->proxy_info != NULL)
		purple_proxy_info_destroy(account->proxy_info);

	account->proxy_info = info;

	schedule_accounts_save();
}

void
purple_account_set_privacy_type(PurpleAccount *account, PurpleAccountPrivacyType privacy_type)
{
	g_return_if_fail(account != NULL);

	account->perm_deny = privacy_type;
}

void
purple_account_set_status_types(PurpleAccount *account, GList *status_types)
{
	g_return_if_fail(account != NULL);

	/* Out with the old... */
	if (account->status_types != NULL)
	{
		g_list_foreach(account->status_types, (GFunc)purple_status_type_destroy, NULL);
		g_list_free(account->status_types);
	}

	/* In with the new... */
	account->status_types = status_types;
}

void
purple_account_set_status(PurpleAccount *account, const char *status_id,
						gboolean active, ...)
{
	GList *attrs = NULL;
	const gchar *id;
	gpointer data;
	va_list args;

	va_start(args, active);
	while ((id = va_arg(args, const char *)) != NULL)
	{
		attrs = g_list_append(attrs, (char *)id);
		data = va_arg(args, void *);
		attrs = g_list_append(attrs, data);
	}
	purple_account_set_status_list(account, status_id, active, attrs);
	g_list_free(attrs);
	va_end(args);
}

void
purple_account_set_status_list(PurpleAccount *account, const char *status_id,
							 gboolean active, GList *attrs)
{
	PurpleStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);

	status = purple_account_get_status(account, status_id);
	if (status == NULL)
	{
		purple_debug_error("account",
				   "Invalid status ID '%s' for account %s (%s)\n",
				   status_id, purple_account_get_username(account),
				   purple_account_get_protocol_id(account));
		return;
	}

	if (active || purple_status_is_independent(status))
		purple_status_set_active_with_attrs_list(status, active, attrs);

	/*
	 * Our current statuses are saved to accounts.xml (so that when we
	 * reconnect, we go back to the previous status).
	 */
	schedule_accounts_save();
}

struct public_alias_closure
{
	PurpleAccount *account;
	gpointer failure_cb;
};

static gboolean
set_public_alias_unsupported(gpointer data)
{
	struct public_alias_closure *closure = data;
	PurpleSetPublicAliasFailureCallback failure_cb = closure->failure_cb;

	failure_cb(closure->account,
	           _("This protocol does not support setting a public alias."));
	g_free(closure);

	return FALSE;
}

void
purple_account_set_public_alias(PurpleAccount *account,
		const char *alias, PurpleSetPublicAliasSuccessCallback success_cb,
		PurpleSetPublicAliasFailureCallback failure_cb)
{
	PurpleConnection *gc;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	prpl = purple_connection_get_prpl(gc);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, set_public_alias))
		prpl_info->set_public_alias(gc, alias, success_cb, failure_cb);
	else if (failure_cb) {
		struct public_alias_closure *closure =
				g_new0(struct public_alias_closure, 1);
		closure->account = account;
		closure->failure_cb = failure_cb;
		purple_timeout_add(0, set_public_alias_unsupported, closure);
	}
}

static gboolean
get_public_alias_unsupported(gpointer data)
{
	struct public_alias_closure *closure = data;
	PurpleGetPublicAliasFailureCallback failure_cb = closure->failure_cb;

	failure_cb(closure->account,
	           _("This protocol does not support fetching the public alias."));
	g_free(closure);

	return FALSE;
}

void
purple_account_get_public_alias(PurpleAccount *account,
		PurpleGetPublicAliasSuccessCallback success_cb,
		PurpleGetPublicAliasFailureCallback failure_cb)
{
	PurpleConnection *gc;
	PurplePlugin *prpl = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	prpl = purple_connection_get_prpl(gc);
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, get_public_alias))
		prpl_info->get_public_alias(gc, success_cb, failure_cb);
	else if (failure_cb) {
		struct public_alias_closure *closure =
				g_new0(struct public_alias_closure, 1);
		closure->account = account;
		closure->failure_cb = failure_cb;
		purple_timeout_add(0, get_public_alias_unsupported, closure);
	}
}

gboolean
purple_account_get_silence_suppression(const PurpleAccount *account)
{
	return purple_account_get_bool(account, "silence-suppression", FALSE);
}

void
purple_account_set_silence_suppression(PurpleAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	purple_account_set_bool(account, "silence-suppression", value);
}

void
purple_account_clear_settings(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	g_hash_table_destroy(account->settings);

	account->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
}

void
purple_account_remove_setting(PurpleAccount *account, const char *setting)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(setting != NULL);

	g_hash_table_remove(account->settings, setting);
}

void
purple_account_set_int(PurpleAccount *account, const char *name, int value)
{
	PurpleAccountSetting *setting;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->type          = PURPLE_PREF_INT;
	setting->value.integer = value;

	g_hash_table_insert(account->settings, g_strdup(name), setting);

	schedule_accounts_save();
}

void
purple_account_set_string(PurpleAccount *account, const char *name,
						const char *value)
{
	PurpleAccountSetting *setting;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->type         = PURPLE_PREF_STRING;
	setting->value.string = g_strdup(value);

	g_hash_table_insert(account->settings, g_strdup(name), setting);

	schedule_accounts_save();
}

void
purple_account_set_bool(PurpleAccount *account, const char *name, gboolean value)
{
	PurpleAccountSetting *setting;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->type       = PURPLE_PREF_BOOLEAN;
	setting->value.boolean = value;

	g_hash_table_insert(account->settings, g_strdup(name), setting);

	schedule_accounts_save();
}

static GHashTable *
get_ui_settings_table(PurpleAccount *account, const char *ui)
{
	GHashTable *table;

	table = g_hash_table_lookup(account->ui_settings, ui);

	if (table == NULL) {
		table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
									  delete_setting);
		g_hash_table_insert(account->ui_settings, g_strdup(ui), table);
	}

	return table;
}

void
purple_account_set_ui_int(PurpleAccount *account, const char *ui,
						const char *name, int value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->type          = PURPLE_PREF_INT;
	setting->ui            = g_strdup(ui);
	setting->value.integer = value;

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	schedule_accounts_save();
}

void
purple_account_set_ui_string(PurpleAccount *account, const char *ui,
						   const char *name, const char *value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->type         = PURPLE_PREF_STRING;
	setting->ui           = g_strdup(ui);
	setting->value.string = g_strdup(value);

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	schedule_accounts_save();
}

void
purple_account_set_ui_bool(PurpleAccount *account, const char *ui,
						 const char *name, gboolean value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->type       = PURPLE_PREF_BOOLEAN;
	setting->ui         = g_strdup(ui);
	setting->value.boolean = value;

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	schedule_accounts_save();
}

static PurpleConnectionState
purple_account_get_state(const PurpleAccount *account)
{
	PurpleConnection *gc;

	g_return_val_if_fail(account != NULL, PURPLE_DISCONNECTED);

	gc = purple_account_get_connection(account);
	if (!gc)
		return PURPLE_DISCONNECTED;

	return purple_connection_get_state(gc);
}

gboolean
purple_account_is_connected(const PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTED);
}

gboolean
purple_account_is_connecting(const PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTING);
}

gboolean
purple_account_is_disconnected(const PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_DISCONNECTED);
}

const char *
purple_account_get_username(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->username;
}

static void
purple_account_get_password_got(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	PurpleCallbackBundle *cbb = data;
	PurpleKeyringReadCallback cb;

	purple_debug_info("account",
		"Read password for account %s from async keyring.\n",
		purple_account_get_username(account));

	purple_str_wipe(account->password);
	account->password = g_strdup(password);

	cb = (PurpleKeyringReadCallback)cbb->cb;
	if (cb != NULL)
		cb(account, password, error, cbb->data);

	g_free(cbb);
}

void
purple_account_get_password(PurpleAccount *account,
	PurpleKeyringReadCallback cb, gpointer data)
{
	if (account == NULL) {
		cb(NULL, NULL, NULL, data);
		return;
	}

	if (account->password != NULL) {
		purple_debug_info("account",
			"Reading password for account %s from cache.\n",
			purple_account_get_username(account));
		cb(account, account->password, NULL, data);
	} else {
		PurpleCallbackBundle *cbb = g_new0(PurpleCallbackBundle, 1);
		cbb->cb = PURPLE_CALLBACK(cb);
		cbb->data = data;

		purple_debug_info("account",
			"Reading password for account %s from async keyring.\n",
			purple_account_get_username(account));
		purple_keyring_get_password(account, 
			purple_account_get_password_got, cbb);
	}
}

const char *
purple_account_get_alias(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->alias;
}

const char *
purple_account_get_user_info(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->user_info;
}

const char *
purple_account_get_buddy_icon_path(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->buddy_icon_path;
}

const char *
purple_account_get_protocol_id(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);
	return account->protocol_id;
}

const char *
purple_account_get_protocol_name(const PurpleAccount *account)
{
	PurplePlugin *p;

	g_return_val_if_fail(account != NULL, NULL);

	p = purple_find_prpl(purple_account_get_protocol_id(account));

	return ((p && p->info->name) ? _(p->info->name) : _("Unknown"));
}

PurpleConnection *
purple_account_get_connection(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->gc;
}

const gchar *
purple_account_get_name_for_display(const PurpleAccount *account)
{
	PurpleBuddy *self = NULL;
	PurpleConnection *gc = NULL;
	const gchar *name = NULL, *username = NULL, *displayname = NULL;

	name = purple_account_get_alias(account);

	if (name) {
		return name;
	}

	username = purple_account_get_username(account);
	self = purple_find_buddy((PurpleAccount *)account, username);

	if (self) {
		const gchar *calias= purple_buddy_get_contact_alias(self);

		/* We don't want to return the buddy name if the buddy/contact
		 * doesn't have an alias set. */
		if (!purple_strequal(username, calias)) {
			return calias;
		}
	}

	gc = purple_account_get_connection(account);
	displayname = purple_connection_get_display_name(gc);

	if (displayname) {
		return displayname;
	}

	return username;
}

gboolean
purple_account_get_remember_password(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return account->remember_pass;
}

gboolean
purple_account_get_check_mail(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return purple_account_get_bool(account, "check-mail", FALSE);
}

gboolean
purple_account_get_enabled(const PurpleAccount *account, const char *ui)
{
	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(ui      != NULL, FALSE);

	return purple_account_get_ui_bool(account, ui, "auto-login", FALSE);
}

PurpleProxyInfo *
purple_account_get_proxy_info(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->proxy_info;
}

PurpleAccountPrivacyType
purple_account_get_privacy_type(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, PURPLE_PRIVACY_ALLOW_ALL);

	return account->perm_deny;
}

gboolean
purple_account_privacy_permit_add(PurpleAccount *account, const char *who,
						gboolean local_only)
{
	GSList *l;
	char *name;
	PurpleBuddy *buddy;
	PurpleBlistUiOps *blist_ops;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	name = g_strdup(purple_normalize(account, who));

	for (l = account->permit; l != NULL; l = l->next) {
		if (g_str_equal(name, l->data))
			/* This buddy already exists */
			break;
	}

	if (l != NULL)
	{
		/* This buddy already exists, so bail out */
		g_free(name);
		return FALSE;
	}

	account->permit = g_slist_append(account->permit, name);

	if (!local_only && purple_account_is_connected(account))
		serv_add_permit(purple_account_get_connection(account), who);

	if (privacy_ops != NULL && privacy_ops->permit_added != NULL)
		privacy_ops->permit_added(account, who);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	/* This lets the UI know a buddy has had its privacy setting changed */
	buddy = purple_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
purple_account_privacy_permit_remove(PurpleAccount *account, const char *who,
						   gboolean local_only)
{
	GSList *l;
	const char *name;
	PurpleBuddy *buddy;
	char *del;
	PurpleBlistUiOps *blist_ops;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	name = purple_normalize(account, who);

	for (l = account->permit; l != NULL; l = l->next) {
		if (g_str_equal(name, l->data))
			/* We found the buddy we were looking for */
			break;
	}

	if (l == NULL)
		/* We didn't find the buddy we were looking for, so bail out */
		return FALSE;

	/* We should not free l->data just yet. There can be occasions where
	 * l->data == who. In such cases, freeing l->data here can cause crashes
	 * later when who is used. */
	del = l->data;
	account->permit = g_slist_delete_link(account->permit, l);

	if (!local_only && purple_account_is_connected(account))
		serv_rem_permit(purple_account_get_connection(account), who);

	if (privacy_ops != NULL && privacy_ops->permit_removed != NULL)
		privacy_ops->permit_removed(account, who);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	buddy = purple_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	g_free(del);
	return TRUE;
}

gboolean
purple_account_privacy_deny_add(PurpleAccount *account, const char *who,
					  gboolean local_only)
{
	GSList *l;
	char *name;
	PurpleBuddy *buddy;
	PurpleBlistUiOps *blist_ops;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	name = g_strdup(purple_normalize(account, who));

	for (l = account->deny; l != NULL; l = l->next) {
		if (g_str_equal(name, l->data))
			/* This buddy already exists */
			break;
	}

	if (l != NULL)
	{
		/* This buddy already exists, so bail out */
		g_free(name);
		return FALSE;
	}

	account->deny = g_slist_append(account->deny, name);

	if (!local_only && purple_account_is_connected(account))
		serv_add_deny(purple_account_get_connection(account), who);

	if (privacy_ops != NULL && privacy_ops->deny_added != NULL)
		privacy_ops->deny_added(account, who);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	buddy = purple_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
purple_account_privacy_deny_remove(PurpleAccount *account, const char *who,
						 gboolean local_only)
{
	GSList *l;
	const char *normalized;
	char *name;
	PurpleBuddy *buddy;
	PurpleBlistUiOps *blist_ops;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	normalized = purple_normalize(account, who);

	for (l = account->deny; l != NULL; l = l->next) {
		if (g_str_equal(normalized, l->data))
			/* We found the buddy we were looking for */
			break;
	}

	if (l == NULL)
		/* We didn't find the buddy we were looking for, so bail out */
		return FALSE;

	buddy = purple_find_buddy(account, normalized);

	name = l->data;
	account->deny = g_slist_delete_link(account->deny, l);

	if (!local_only && purple_account_is_connected(account))
		serv_rem_deny(purple_account_get_connection(account), name);

	if (privacy_ops != NULL && privacy_ops->deny_removed != NULL)
		privacy_ops->deny_removed(account, who);

	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}

	g_free(name);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	return TRUE;
}

/**
 * This makes sure your permit list contains all buddies from your
 * buddy list and ONLY buddies from your buddy list.
 */
static void
add_all_buddies_to_permit_list(PurpleAccount *account, gboolean local)
{
	GSList *list;

	/* Remove anyone in the permit list who is not in the buddylist */
	for (list = account->permit; list != NULL; ) {
		char *person = list->data;
		list = list->next;
		if (!purple_find_buddy(account, person))
			purple_account_privacy_permit_remove(account, person, local);
	}

	/* Now make sure everyone in the buddylist is in the permit list */
	list = purple_find_buddies(account, NULL);
	while (list != NULL)
	{
		PurpleBuddy *buddy = list->data;
		const gchar *name = purple_buddy_get_name(buddy);

		if (!g_slist_find_custom(account->permit, name, (GCompareFunc)g_utf8_collate))
			purple_account_privacy_permit_add(account, name, local);
		list = g_slist_delete_link(list, list);
	}
}

void
purple_account_privacy_allow(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivacyType type = purple_account_get_privacy_type(account);

	switch (type) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			return;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			purple_account_privacy_permit_add(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			purple_account_privacy_deny_remove(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			/* Empty the allow-list. */
			const char *norm = purple_normalize(account, who);
			for (list = account->permit; list != NULL;) {
				char *person = list->data;
				list = list->next;
				if (!purple_strequal(norm, person))
					purple_account_privacy_permit_remove(account, person, FALSE);
			}
			purple_account_privacy_permit_add(account, who, FALSE);
			purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			if (!purple_find_buddy(account, who)) {
				add_all_buddies_to_permit_list(account, FALSE);
				purple_account_privacy_permit_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		default:
			g_return_if_reached();
	}

	/* Notify the server if the privacy setting was changed */
	if (type != purple_account_get_privacy_type(account) && purple_account_is_connected(account))
		serv_set_permit_deny(purple_account_get_connection(account));
}

void
purple_account_privacy_deny(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivacyType type = purple_account_get_privacy_type(account);

	switch (type) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			/* Empty the deny-list. */
			const char *norm = purple_normalize(account, who);
			for (list = account->deny; list != NULL; ) {
				char *person = list->data;
				list = list->next;
				if (!purple_strequal(norm, person))
					purple_account_privacy_deny_remove(account, person, FALSE);
			}
			purple_account_privacy_deny_add(account, who, FALSE);
			purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_DENY_USERS);
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			purple_account_privacy_permit_remove(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			purple_account_privacy_deny_add(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			if (purple_find_buddy(account, who)) {
				add_all_buddies_to_permit_list(account, FALSE);
				purple_account_privacy_permit_remove(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		default:
			g_return_if_reached();
	}

	/* Notify the server if the privacy setting was changed */
	if (type != purple_account_get_privacy_type(account) && purple_account_is_connected(account))
		serv_set_permit_deny(purple_account_get_connection(account));
}

GSList *
purple_account_privacy_get_permitted(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	g_return_if_fail(priv != NULL);

	return priv->permit;
}

GSList *
purple_account_privacy_get_denied(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	g_return_if_fail(priv != NULL);

	return priv->deny;
}

gboolean
purple_account_privacy_check(PurpleAccount *account, const char *who)
{
	GSList *list;

	switch (purple_account_get_privacy_type(account)) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			return TRUE;

		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			return FALSE;

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			who = purple_normalize(account, who);
			for (list=account->permit; list!=NULL; list=list->next) {
				if (g_str_equal(who, list->data))
					return TRUE;
			}
			return FALSE;

		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			who = purple_normalize(account, who);
			for (list=account->deny; list!=NULL; list=list->next) {
				if (g_str_equal(who, list->data))
					return FALSE;
			}
			return TRUE;

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			return (purple_find_buddy(account, who) != NULL);

		default:
			g_return_val_if_reached(TRUE);
	}
}

PurpleStatus *
purple_account_get_active_status(const PurpleAccount *account)
{
	g_return_val_if_fail(account   != NULL, NULL);

	return purple_presence_get_active_status(account->presence);
}

PurpleStatus *
purple_account_get_status(const PurpleAccount *account, const char *status_id)
{
	g_return_val_if_fail(account   != NULL, NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	return purple_presence_get_status(account->presence, status_id);
}

PurpleStatusType *
purple_account_get_status_type(const PurpleAccount *account, const char *id)
{
	GList *l;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(id      != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (purple_strequal(purple_status_type_get_id(status_type), id))
			return status_type;
	}

	return NULL;
}

PurpleStatusType *
purple_account_get_status_type_with_primitive(const PurpleAccount *account, PurpleStatusPrimitive primitive)
{
	GList *l;

	g_return_val_if_fail(account != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (purple_status_type_get_primitive(status_type) == primitive)
			return status_type;
	}

	return NULL;
}

PurplePresence *
purple_account_get_presence(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->presence;
}

gboolean
purple_account_is_status_active(const PurpleAccount *account,
							  const char *status_id)
{
	g_return_val_if_fail(account   != NULL, FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	return purple_presence_is_status_active(account->presence, status_id);
}

GList *
purple_account_get_status_types(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->status_types;
}

int
purple_account_get_int(const PurpleAccount *account, const char *name,
					 int default_value)
{
	PurpleAccountSetting *setting;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	setting = g_hash_table_lookup(account->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == PURPLE_PREF_INT, default_value);

	return setting->value.integer;
}

const char *
purple_account_get_string(const PurpleAccount *account, const char *name,
						const char *default_value)
{
	PurpleAccountSetting *setting;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	setting = g_hash_table_lookup(account->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == PURPLE_PREF_STRING, default_value);

	return setting->value.string;
}

gboolean
purple_account_get_bool(const PurpleAccount *account, const char *name,
					  gboolean default_value)
{
	PurpleAccountSetting *setting;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	setting = g_hash_table_lookup(account->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == PURPLE_PREF_BOOLEAN, default_value);

	return setting->value.boolean;
}

int
purple_account_get_ui_int(const PurpleAccount *account, const char *ui,
						const char *name, int default_value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	if ((table = g_hash_table_lookup(account->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == PURPLE_PREF_INT, default_value);

	return setting->value.integer;
}

const char *
purple_account_get_ui_string(const PurpleAccount *account, const char *ui,
						   const char *name, const char *default_value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	if ((table = g_hash_table_lookup(account->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == PURPLE_PREF_STRING, default_value);

	return setting->value.string;
}

gboolean
purple_account_get_ui_bool(const PurpleAccount *account, const char *ui,
						 const char *name, gboolean default_value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	if ((table = g_hash_table_lookup(account->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(setting->type == PURPLE_PREF_BOOLEAN, default_value);

	return setting->value.boolean;
}

gpointer
purple_account_get_ui_data(const PurpleAccount *account)
{
        g_return_val_if_fail(account != NULL, NULL);

        return account->ui_data;
}

void
purple_account_set_ui_data(PurpleAccount *account,
                                 gpointer ui_data)
{
        g_return_if_fail(account != NULL);

        account->ui_data = ui_data;
}


PurpleLog *
purple_account_get_log(PurpleAccount *account, gboolean create)
{
	g_return_val_if_fail(account != NULL, NULL);

	if(!account->system_log && create){
		PurplePresence *presence;
		int login_time;

		presence = purple_account_get_presence(account);
		login_time = purple_presence_get_login_time(presence);

		account->system_log	 = purple_log_new(PURPLE_LOG_SYSTEM,
				purple_account_get_username(account), account, NULL,
				(login_time != 0) ? login_time : time(NULL), NULL);
	}

	return account->system_log;
}

void
purple_account_destroy_log(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	if(account->system_log){
		purple_log_free(account->system_log);
		account->system_log = NULL;
	}
}

void
purple_account_add_buddy(PurpleAccount *account, PurpleBuddy *buddy, const char *message)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;
	PurplePlugin *prpl = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(buddy != NULL);

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info != NULL) {
		if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, add_buddy))
			prpl_info->add_buddy(gc, buddy, purple_buddy_get_group(buddy), message);
	}
}

void
purple_account_add_buddies(PurpleAccount *account, GList *buddies, const char *message)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;

	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info) {
		GList *cur, *groups = NULL;

		/* Make a list of what group each buddy is in */
		for (cur = buddies; cur != NULL; cur = cur->next) {
			PurpleBuddy *buddy = cur->data;
			groups = g_list_append(groups, purple_buddy_get_group(buddy));
		}

		if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, add_buddies))
			prpl_info->add_buddies(gc, buddies, groups, message);
		else if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, add_buddy)) {
			GList *curb = buddies, *curg = groups;

			while ((curb != NULL) && (curg != NULL)) {
				prpl_info->add_buddy(gc, curb->data, curg->data, message);
				curb = curb->next;
				curg = curg->next;
			}
		}

		g_list_free(groups);
	}
}

void
purple_account_remove_buddy(PurpleAccount *account, PurpleBuddy *buddy,
		PurpleGroup *group)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;

	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && prpl_info->remove_buddy)
		prpl_info->remove_buddy(gc, buddy, group);
}

void
purple_account_remove_buddies(PurpleAccount *account, GList *buddies, GList *groups)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;

	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info) {
		if (prpl_info->remove_buddies)
			prpl_info->remove_buddies(gc, buddies, groups);
		else {
			GList *curb = buddies;
			GList *curg = groups;
			while ((curb != NULL) && (curg != NULL)) {
				purple_account_remove_buddy(account, curb->data, curg->data);
				curb = curb->next;
				curg = curg->next;
			}
		}
	}
}

void
purple_account_remove_group(PurpleAccount *account, PurpleGroup *group)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;

	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && prpl_info->remove_group)
		prpl_info->remove_group(gc, group);
}

void
purple_account_change_password(PurpleAccount *account, const char *orig_pw,
		const char *new_pw)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePlugin *prpl = NULL;

	purple_account_set_password(account, new_pw, NULL, NULL);

	if (gc != NULL)
		prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info && prpl_info->change_passwd)
		prpl_info->change_passwd(gc, orig_pw, new_pw);
}

gboolean purple_account_supports_offline_message(PurpleAccount *account, PurpleBuddy *buddy)
{
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurplePlugin *prpl = NULL;

	g_return_val_if_fail(account, FALSE);
	g_return_val_if_fail(buddy, FALSE);

	gc = purple_account_get_connection(account);
	if (gc == NULL)
		return FALSE;

	prpl = purple_connection_get_prpl(gc);

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (!prpl_info || !prpl_info->offline_message)
		return FALSE;
	return prpl_info->offline_message(buddy);
}

static void
signed_on_cb(PurpleConnection *gc,
             gpointer unused)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	purple_account_clear_current_error(account);

	purple_signal_emit(purple_accounts_get_handle(), "account-signed-on",
	                   account);
}

static void
signed_off_cb(PurpleConnection *gc,
              gpointer unused)
{
	PurpleAccount *account = purple_connection_get_account(gc);

	purple_signal_emit(purple_accounts_get_handle(), "account-signed-off",
	                   account);
}

static void
set_current_error(PurpleAccount *account, PurpleConnectionErrorInfo *new_err)
{
	PurpleConnectionErrorInfo *old_err;

	g_return_if_fail(account != NULL);

	old_err = account->current_error;

	if(new_err == old_err)
		return;

	account->current_error = new_err;

	purple_signal_emit(purple_accounts_get_handle(),
	                   "account-error-changed",
	                   account, old_err, new_err);
	schedule_accounts_save();

	if(old_err)
		g_free(old_err->description);

	PURPLE_DBUS_UNREGISTER_POINTER(old_err);
	g_free(old_err);
}

static void
connection_error_cb(PurpleConnection *gc,
                    PurpleConnectionError type,
                    const gchar *description,
                    gpointer unused)
{
	PurpleAccount *account;
	PurpleConnectionErrorInfo *err;

	account = purple_connection_get_account(gc);

	g_return_if_fail(account != NULL);

	err = g_new0(PurpleConnectionErrorInfo, 1);
	PURPLE_DBUS_REGISTER_POINTER(err, PurpleConnectionErrorInfo);

	err->type = type;
	err->description = g_strdup(description);

	set_current_error(account, err);

	purple_signal_emit(purple_accounts_get_handle(), "account-connection-error",
	                   account, type, description);
}

static void
password_migration_cb(PurpleAccount *account)
{
	/* account may be NULL (means: all) */

	schedule_accounts_save();
}

const PurpleConnectionErrorInfo *
purple_account_get_current_error(PurpleAccount *account)
{
	return account->current_error;
}

void
purple_account_clear_current_error(PurpleAccount *account)
{
	set_current_error(account, NULL);
}

GType
purple_account_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleAccountClass),
			NULL,
			NULL,
			NULL, /*TODO: (GClassInitFunc)purple_account_class_init,*/
			NULL,
			NULL,
			sizeof(PurpleAccount),
			0,
			NULL, /*TODO: (GInstanceInitFunc)purple_account_init,*/
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleAccount",
				&info, 0);
	}

	return type;
}
