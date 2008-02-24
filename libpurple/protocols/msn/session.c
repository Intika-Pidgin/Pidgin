/**
 * @file session.c MSN session functions
 *
 * purple
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
#include "msn.h"
#include "session.h"
#include "notification.h"
#include "oim.h"

#include "dialog.h"

MsnSession *
msn_session_new(PurpleAccount *account)
{
	MsnSession *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(MsnSession, 1);

	session->account = account;
	session->notification = msn_notification_new(session);
	session->userlist = msn_userlist_new(session);

	session->user = msn_user_new(session->userlist,
								 purple_account_get_username(account), NULL);
	session->oim = msn_oim_new(session);

	/*if you want to chat with Yahoo Messenger*/
	//session->protocol_ver = WLM_YAHOO_PROT_VER;
	session->protocol_ver = WLM_PROT_VER;

	return session;
}

void
msn_session_destroy(MsnSession *session)
{
	g_return_if_fail(session != NULL);

	session->destroying = TRUE;

	if (session->connected)
		msn_session_disconnect(session);

	if (session->notification != NULL)
		msn_notification_destroy(session->notification);

	while (session->switches != NULL)
		msn_switchboard_destroy(session->switches->data);

	while (session->slplinks != NULL)
		msn_slplink_destroy(session->slplinks->data);

	msn_userlist_destroy(session->userlist);

	g_free(session->psm);
	g_free(session->passport_info.t);
	g_free(session->passport_info.p);
	g_free(session->passport_info.kv);
	g_free(session->passport_info.sid);
	g_free(session->passport_info.mspauth);
	g_free(session->passport_info.client_ip);

	if (session->passport_info.file != NULL)
	{
		g_unlink(session->passport_info.file);
		g_free(session->passport_info.file);
	}

	if (session->sync != NULL)
		msn_sync_destroy(session->sync);

	if (session->nexus != NULL)
		msn_nexus_destroy(session->nexus);

	if (session->contact != NULL)
		msn_contact_destroy(session->contact);
	if (session->oim != NULL)
		msn_oim_destroy(session->oim);

	if (session->user != NULL)
		msn_user_destroy(session->user);

	if (session->soap_table)
		g_hash_table_destroy(session->soap_table);

	if (session->soap_cleanup_handle)
		purple_timeout_remove(session->soap_cleanup_handle);

	g_free(session);
}

gboolean
msn_session_connect(MsnSession *session, const char *host, int port,
					gboolean http_method)
{
	g_return_val_if_fail(session != NULL, FALSE);
	g_return_val_if_fail(!session->connected, TRUE);

	session->connected = TRUE;
	session->http_method = http_method;

	if (session->notification == NULL)
	{
		purple_debug_error("msn", "This shouldn't happen\n");
		g_return_val_if_reached(FALSE);
	}

	if (msn_notification_connect(session->notification, host, port))
	{
		return TRUE;
	}

	return FALSE;
}

void
msn_session_disconnect(MsnSession *session)
{
	g_return_if_fail(session != NULL);

	if (!session->connected)
		return;

	session->connected = FALSE;

	while (session->switches != NULL)
		msn_switchboard_close(session->switches->data);

	if (session->notification != NULL)
		msn_notification_close(session->notification);
}

/* TODO: This must go away when conversation is redesigned */
MsnSwitchBoard *
msn_session_find_swboard(MsnSession *session, const char *username)
{
	GList *l;

	g_return_val_if_fail(session  != NULL, NULL);
	g_return_val_if_fail(username != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if ((swboard->im_user != NULL) && !strcmp(username, swboard->im_user))
			return swboard;
	}

	return NULL;
}

static PurpleConversation *
msn_session_get_conv(MsnSession *session,const char *passport)
{
	PurpleAccount *account;
	PurpleConversation * conv;

	g_return_val_if_fail(session != NULL, NULL);
	account = session->account;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
									passport, account);
	if(conv == NULL){
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, passport);
	}
	return conv;
}

/* put Message to User Conversation
 *
 * 	passport - the one want to talk to you
 */
void
msn_session_report_user(MsnSession *session,const char *passport,char *msg,PurpleMessageFlags flags)
{
	PurpleConversation * conv;

	if ((conv = msn_session_get_conv(session,passport)) != NULL){
		purple_conversation_write(conv, NULL, msg, flags, time(NULL));
	}
}

MsnSwitchBoard *
msn_session_find_swboard_with_conv(MsnSession *session, PurpleConversation *conv)
{
	GList *l;

	g_return_val_if_fail(session  != NULL, NULL);
	g_return_val_if_fail(conv != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if (swboard->conv == conv)
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_find_swboard_with_id(const MsnSession *session, int chat_id)
{
	GList *l;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(chat_id >= 0,    NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if (swboard->chat_id == chat_id)
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_get_swboard(MsnSession *session, const char *username,
						MsnSBFlag flag)
{
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session != NULL, NULL);

	swboard = msn_session_find_swboard(session, username);

	if (swboard == NULL)
	{
		swboard = msn_switchboard_new(session);
		swboard->im_user = g_strdup(username);
		msn_switchboard_request(swboard);
		msn_switchboard_request_add_user(swboard, username);
	}

	swboard->flag |= flag;

	return swboard;
}

static void
msn_session_sync_users(MsnSession *session)
{
	PurpleBlistNode *gnode, *cnode, *bnode;
	PurpleConnection *gc = purple_account_get_connection(session->account);

	g_return_if_fail(gc != NULL);

	/* The core used to use msn_add_buddy to add all buddies before
	 * being logged in. This no longer happens, so we manually iterate
	 * over the whole buddy list to identify sync issues.
	 */
	for (gnode = purple_get_blist()->root; gnode; gnode = gnode->next) {
		PurpleGroup *group = (PurpleGroup *)gnode;
		const char *group_name;
		if(!PURPLE_BLIST_NODE_IS_GROUP(gnode))
			continue;
		group_name = group->name;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				PurpleBuddy *b;
				if(!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				b = (PurpleBuddy *)bnode;
				if(purple_buddy_get_account(b) == purple_connection_get_account(gc)) {
					MsnUser *remote_user;
					gboolean found = FALSE;

					remote_user = msn_userlist_find_user(session->userlist, purple_buddy_get_name(b));

					if ((remote_user != NULL) && (remote_user->list_op & MSN_LIST_FL_OP))
					{
						GList *l;

						for (l = remote_user->group_ids; l != NULL; l = l->next)
						{
							const char *name = msn_userlist_find_group_name(remote_user->userlist, l->data);
							if (name && !g_strcasecmp(group_name, name))
							{
								found = TRUE;
								break;
							}
						}
					}

					if (!found)
					{
						/* The user was not on the server list or not in that group
						 * on the server list */
						msn_show_sync_issue(session, purple_buddy_get_name(b), group_name);
					}
				}
			}
		}
	}
}

void
msn_session_set_error(MsnSession *session, MsnErrorType error,
					  const char *info)
{
	PurpleConnection *gc;
	PurpleConnectionError reason;
	char *msg;

	gc = purple_account_get_connection(session->account);

	switch (error)
	{
		case MSN_ERROR_SERVCONN:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(info);
			break;
		case MSN_ERROR_UNSUPPORTED_PROTOCOL:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(_("Our protocol is not supported by the "
							 "server."));
			break;
		case MSN_ERROR_HTTP_MALFORMED:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(_("Error parsing HTTP."));
			break;
		case MSN_ERROR_SIGN_OTHER:
			reason = PURPLE_CONNECTION_ERROR_NAME_IN_USE;
			msg = g_strdup(_("You have signed on from another location."));
			if (!purple_account_get_remember_password(session->account))
				purple_account_set_password(session->account, NULL);
			break;
		case MSN_ERROR_SERV_UNAVAILABLE:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(_("The MSN servers are temporarily "
							 "unavailable. Please wait and try "
							 "again."));
			break;
		case MSN_ERROR_SERV_DOWN:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(_("The MSN servers are going down "
							 "temporarily."));
			break;
		case MSN_ERROR_AUTH:
			reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
			msg = g_strdup_printf(_("Unable to authenticate: %s"),
								  (info == NULL ) ?
								  _("Unknown error") : info);
			break;
		case MSN_ERROR_BAD_BLIST:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(_("Your MSN buddy list is temporarily "
							 "unavailable. Please wait and try "
							 "again."));
			break;
		default:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			msg = g_strdup(_("Unknown error."));
			break;
	}

	msn_session_disconnect(session);

	purple_connection_error_reason (gc, reason, msg);

	g_free(msg);
}

static const char *
get_login_step_text(MsnSession *session)
{
	const char *steps_text[] = {
		_("Connecting"),
		_("Handshaking"),
		_("Transferring"),
		_("Handshaking"),
		_("Starting authentication"),
		_("Getting cookie"),
		_("Authenticating"),
		_("Sending cookie"),
		_("Retrieving buddy list")
	};

	return steps_text[session->login_step];
}

void
msn_session_set_login_step(MsnSession *session, MsnLoginStep step)
{
	PurpleConnection *gc;

	/* Prevent the connection progress going backwards, eg. if we get
	 * transferred several times during login */
	if (session->login_step > step)
		return;

	/* If we're already logged in, we're probably here because of a
	 * mid-session XFR from the notification server, so we don't want to
	 * popup the connection progress dialog */
	if (session->logged_in)
		return;

	gc = session->account->gc;

	session->login_step = step;

	purple_connection_update_progress(gc, get_login_step_text(session), step,
									MSN_LOGIN_STEPS);
}

void
msn_session_finish_login(MsnSession *session)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleStoredImage *img;
	const char *passport;

	if (session->logged_in)
		return;

	account = session->account;
	gc = purple_account_get_connection(account);

	img = purple_buddy_icons_find_account_icon(session->account);
	msn_user_set_buddy_icon(session->user, img);
	purple_imgstore_unref(img);

	session->logged_in = TRUE;

	msn_change_status(session);

	purple_connection_set_state(gc, PURPLE_CONNECTED);

	/* Sync users */
	msn_session_sync_users(session);
	/* It seems that some accounts that haven't accessed hotmail for a while
	 * and @msn.com accounts don't automatically get the initial email
	 * notification so we always request it on login
	 */

	passport = purple_normalize(account, purple_account_get_username(account));

	if ((strstr(passport, "@hotmail.") != NULL) ||
		(strstr(passport, "@msn.com") != NULL))
	{
		msn_cmdproc_send(session->notification->cmdproc, "URL", "%s", "INBOX");
	}
}

