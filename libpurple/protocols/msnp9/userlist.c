/**
 * @file userlist.c MSN user list support
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
#include "userlist.h"

const char *lists[] = { "FL", "AL", "BL", "RL" };

typedef struct
{
	PurpleConnection *gc;
	char *who;
	char *friendly;

} MsnPermitAdd;

/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
msn_accept_add_cb(gpointer data)
{
	MsnPermitAdd *pa = data;
	MsnSession *session;
	MsnUserList *userlist;

	if (PURPLE_CONNECTION_IS_VALID(pa->gc)) {
		session = pa->gc->proto_data;
		userlist = session->userlist;

		msn_userlist_add_buddy(userlist, pa->who, MSN_LIST_AL, NULL);
	}

	g_free(pa->who);
	g_free(pa->friendly);
	g_free(pa);
}

static void
msn_cancel_add_cb(gpointer data)
{
	MsnPermitAdd *pa = data;
	MsnSession *session;
	MsnUserList *userlist;

	if (PURPLE_CONNECTION_IS_VALID(pa->gc)) {
		session = pa->gc->proto_data;
		userlist = session->userlist;

		msn_userlist_add_buddy(userlist, pa->who, MSN_LIST_BL, NULL);
	}

	g_free(pa->who);
	g_free(pa->friendly);
	g_free(pa);
}

static void
got_new_entry(PurpleConnection *gc, const char *passport, const char *friendly)
{
	MsnPermitAdd *pa;

	pa = g_new0(MsnPermitAdd, 1);
	pa->who = g_strdup(passport);
	pa->friendly = g_strdup(friendly);
	pa->gc = gc;
	
	purple_account_request_authorization(purple_connection_get_account(gc), passport, NULL, friendly, NULL,
					   purple_find_buddy(purple_connection_get_account(gc), passport) != NULL,
					   msn_accept_add_cb, msn_cancel_add_cb, pa);
}

/**************************************************************************
 * Utility functions
 **************************************************************************/

static gboolean
user_is_in_group(MsnUser *user, int group_id)
{
	if (user == NULL)
		return FALSE;

	if (group_id < 0)
		return FALSE;

	if (g_list_find(user->group_ids, GINT_TO_POINTER(group_id)))
		return TRUE;

	return FALSE;
}

static gboolean
user_is_there(MsnUser *user, int list_id, int group_id)
{
	int list_op;

	if (user == NULL)
		return FALSE;

	list_op = 1 << list_id;

	if (!(user->list_op & list_op))
		return FALSE;

	if (list_id == MSN_LIST_FL)
	{
		if (group_id >= 0)
			return user_is_in_group(user, group_id);
	}

	return TRUE;
}

static const char*
get_friendly_name(MsnUser *user)
{
	const char *friendly_name;

	g_return_val_if_fail(user != NULL, NULL);

	friendly_name = msn_user_get_friendly_name(user);

	if (friendly_name != NULL)
		friendly_name = purple_url_encode(friendly_name);
	else
		friendly_name = msn_user_get_passport(user);

	/* this might be a bit of a hack, but it should prevent notification server
	 * disconnections for people who have buddies with insane friendly names
	 * who added you to their buddy list from being disconnected. Stu. */
	/* Shx: What? Isn't the store_name obtained from the server, and hence it's
	 * below the BUDDY_ALIAS_MAXLEN ? */
	/* Stu: yeah, that's why it's a bit of a hack, as you pointed out, we're
	 * probably decoding the incoming store_name wrong, or something. bleh. */

	if (strlen(friendly_name) > BUDDY_ALIAS_MAXLEN)
		friendly_name = msn_user_get_passport(user);

	return friendly_name;
}

static void
msn_request_add_group(MsnUserList *userlist, const char *who,
					  const char *old_group_name, const char *new_group_name)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	MsnMoveBuddy *data;

	cmdproc = userlist->session->notification->cmdproc;
	data = g_new0(MsnMoveBuddy, 1);

	data->who = g_strdup(who);

	if (old_group_name)
		data->old_group_name = g_strdup(old_group_name);

	trans = msn_transaction_new(cmdproc, "ADG", "%s %d",
								purple_url_encode(new_group_name),
								0);

	msn_transaction_set_data(trans, data);

	msn_cmdproc_send_trans(cmdproc, trans);
}

/**************************************************************************
 * Server functions
 **************************************************************************/

MsnListId
msn_get_list_id(const char *list)
{
	if (list[0] == 'F')
		return MSN_LIST_FL;
	else if (list[0] == 'A')
		return MSN_LIST_AL;
	else if (list[0] == 'B')
		return MSN_LIST_BL;
	else if (list[0] == 'R')
		return MSN_LIST_RL;

	return -1;
}

void
msn_got_add_user(MsnSession *session, MsnUser *user,
				 MsnListId list_id, int group_id)
{
	PurpleAccount *account;
	const char *passport;
	const char *friendly;

	account = session->account;

	passport = msn_user_get_passport(user);
	friendly = msn_user_get_friendly_name(user);

	if (list_id == MSN_LIST_FL)
	{
		PurpleConnection *gc;

		gc = purple_account_get_connection(account);

		serv_got_alias(gc, passport, friendly);

		if (group_id >= 0)
		{
			msn_user_add_group_id(user, group_id);
		}
		else
		{
			/* session->sync->fl_users_count++; */
		}
	}
	else if (list_id == MSN_LIST_AL)
	{
		purple_privacy_permit_add(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_BL)
	{
		purple_privacy_deny_add(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_RL)
	{
		PurpleConnection *gc;
		PurpleConversation *convo;

		gc = purple_account_get_connection(account);

		purple_debug_info("msn",
						"%s has added you to his or her buddy list.\n",
						passport);

 		convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, passport, account);
 		if (convo) {
 			PurpleBuddy *buddy;
 			char *msg;
 
 			buddy = purple_find_buddy(account, passport);
 			msg = g_strdup_printf(
 				_("%s has added you to his or her buddy list."),
 				buddy ? purple_buddy_get_contact_alias(buddy) : passport);
 			purple_conv_im_write(PURPLE_CONV_IM(convo), passport, msg,
 				PURPLE_MESSAGE_SYSTEM, time(NULL));
 			g_free(msg);
 		}
 
		if (!(user->list_op & (MSN_LIST_AL_OP | MSN_LIST_BL_OP)))
		{
			/*
			 * TODO: The friendly name was NULL for me when I
			 *       looked at this.  Maybe we should use the store
			 *       name instead? --KingAnt
			 */
			got_new_entry(gc, passport, friendly);
		}
	}

	user->list_op |= (1 << list_id);
	/* purple_user_add_list_id (user, list_id); */
}

void
msn_got_rem_user(MsnSession *session, MsnUser *user,
				 MsnListId list_id, int group_id)
{
	PurpleAccount *account;
	const char *passport;

	account = session->account;

	passport = msn_user_get_passport(user);

	if (list_id == MSN_LIST_FL)
	{
		/* TODO: When is the user totally removed? */
		if (group_id >= 0)
		{
			msn_user_remove_group_id(user, group_id);
			return;
		}
		else
		{
			/* session->sync->fl_users_count--; */
		}
	}
	else if (list_id == MSN_LIST_AL)
	{
		purple_privacy_permit_remove(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_BL)
	{
		purple_privacy_deny_remove(account, passport, TRUE);
	}
	else if (list_id == MSN_LIST_RL)
	{
		PurpleConversation *convo;

		purple_debug_info("msn",
						"%s has removed you from his or her buddy list.\n",
						passport);

		convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, passport, account);
		if (convo) {
			PurpleBuddy *buddy;
			char *msg;

			buddy = purple_find_buddy(account, passport);
			msg = g_strdup_printf(
				_("%s has removed you from his or her buddy list."),
				buddy ? purple_buddy_get_contact_alias(buddy) : passport);
			purple_conv_im_write(PURPLE_CONV_IM(convo), passport, msg,
				PURPLE_MESSAGE_SYSTEM, time(NULL));
			g_free(msg);
		}
	}

	user->list_op &= ~(1 << list_id);
	/* purple_user_remove_list_id (user, list_id); */

	if (user->list_op == 0)
	{
		purple_debug_info("msn", "Buddy '%s' shall be deleted?.\n",
						passport);

	}
}

void
msn_got_lst_user(MsnSession *session, MsnUser *user,
				 int list_op, GSList *group_ids)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	const char *passport;
	const char *store;

	account = session->account;
	gc = purple_account_get_connection(account);

	passport = msn_user_get_passport(user);
	store = msn_user_get_friendly_name(user);

	if (list_op & MSN_LIST_FL_OP)
	{
		GSList *c;
		for (c = group_ids; c != NULL; c = g_slist_next(c))
		{
			int group_id;
			group_id = GPOINTER_TO_INT(c->data);
			msn_user_add_group_id(user, group_id);
		}

		/* FIXME: It might be a real alias */
		/* Umm, what? This might fix bug #1385130 */
		serv_got_alias(gc, passport, store);
	}

	if (list_op & MSN_LIST_AL_OP)
	{
		/* These are users who are allowed to see our status. */
		purple_privacy_deny_remove(account, passport, TRUE);
		purple_privacy_permit_add(account, passport, TRUE);
	}

	if (list_op & MSN_LIST_BL_OP)
	{
		/* These are users who are not allowed to see our status. */
		purple_privacy_permit_remove(account, passport, TRUE);
		purple_privacy_deny_add(account, passport, TRUE);
	}

	if (list_op & MSN_LIST_RL_OP)
	{
		/* These are users who have us on their buddy list. */
		/*
		 * TODO: What is store name set to when this happens?
		 *       For one of my accounts "something@hotmail.com"
		 *       the store name was "something."  Maybe we
		 *       should use the friendly name, instead? --KingAnt
		 */

		if (!(list_op & (MSN_LIST_AL_OP | MSN_LIST_BL_OP)))
		{
			got_new_entry(gc, passport, store);
		}
	}

	user->list_op = list_op;
}

/**************************************************************************
 * UserList functions
 **************************************************************************/

MsnUserList*
msn_userlist_new(MsnSession *session)
{
	MsnUserList *userlist;

	userlist = g_new0(MsnUserList, 1);

	userlist->session = session;
	userlist->buddy_icon_requests = g_queue_new();
	
	/* buddy_icon_window is the number of allowed simultaneous buddy icon requests.
	 * XXX With smarter rate limiting code, we could allow more at once... 5 was the limit set when
	 * we weren't retrieiving any more than 5 per MSN session. */
	userlist->buddy_icon_window = 1;

	return userlist;
}

void
msn_userlist_destroy(MsnUserList *userlist)
{
	GList *l;

	for (l = userlist->users; l != NULL; l = l->next)
	{
		msn_user_destroy(l->data);
	}

	g_list_free(userlist->users);

	for (l = userlist->groups; l != NULL; l = l->next)
	{
		msn_group_destroy(l->data);
	}

	g_list_free(userlist->groups);

	g_queue_free(userlist->buddy_icon_requests);

	if (userlist->buddy_icon_request_timer)
		purple_timeout_remove(userlist->buddy_icon_request_timer);

	g_free(userlist);
}

void
msn_userlist_add_user(MsnUserList *userlist, MsnUser *user)
{
	userlist->users = g_list_prepend(userlist->users, user);
}

void
msn_userlist_remove_user(MsnUserList *userlist, MsnUser *user)
{
	userlist->users = g_list_remove(userlist->users, user);
}

MsnUser *
msn_userlist_find_user(MsnUserList *userlist, const char *passport)
{
	GList *l;

	g_return_val_if_fail(passport != NULL, NULL);

	for (l = userlist->users; l != NULL; l = l->next)
	{
		MsnUser *user = (MsnUser *)l->data;

		g_return_val_if_fail(user->passport != NULL, NULL);

		if (!strcmp(passport, user->passport))
			return user;
	}

	return NULL;
}

void
msn_userlist_add_group(MsnUserList *userlist, MsnGroup *group)
{
	userlist->groups = g_list_append(userlist->groups, group);
}

void
msn_userlist_remove_group(MsnUserList *userlist, MsnGroup *group)
{
	userlist->groups = g_list_remove(userlist->groups, group);
}

MsnGroup *
msn_userlist_find_group_with_id(MsnUserList *userlist, int id)
{
	GList *l;

	g_return_val_if_fail(userlist != NULL, NULL);
	g_return_val_if_fail(id       >= 0,    NULL);

	for (l = userlist->groups; l != NULL; l = l->next)
	{
		MsnGroup *group = l->data;

		if (group->id == id)
			return group;
	}

	return NULL;
}

MsnGroup *
msn_userlist_find_group_with_name(MsnUserList *userlist, const char *name)
{
	GList *l;

	g_return_val_if_fail(userlist != NULL, NULL);
	g_return_val_if_fail(name     != NULL, NULL);

	for (l = userlist->groups; l != NULL; l = l->next)
	{
		MsnGroup *group = l->data;

		if ((group->name != NULL) && !g_ascii_strcasecmp(name, group->name))
			return group;
	}

	return NULL;
}

int
msn_userlist_find_group_id(MsnUserList *userlist, const char *group_name)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_name(userlist, group_name);

	if (group != NULL)
		return msn_group_get_id(group);
	else
		return -1;
}

const char *
msn_userlist_find_group_name(MsnUserList *userlist, int group_id)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_id(userlist, group_id);

	if (group != NULL)
		return msn_group_get_name(group);
	else
		return NULL;
}

void
msn_userlist_rename_group_id(MsnUserList *userlist, int group_id,
							 const char *new_name)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_id(userlist, group_id);

	if (group != NULL)
		msn_group_set_name(group, new_name);
}

void
msn_userlist_remove_group_id(MsnUserList *userlist, int group_id)
{
	MsnGroup *group;

	group = msn_userlist_find_group_with_id(userlist, group_id);

	if (group != NULL)
	{
		msn_userlist_remove_group(userlist, group);
		msn_group_destroy(group);
	}
}

void
msn_userlist_rem_buddy(MsnUserList *userlist,
					   const char *who, int list_id, const char *group_name)
{
	MsnUser *user;
	int group_id;
	const char *list;

	user = msn_userlist_find_user(userlist, who);
	group_id = -1;

	if (group_name != NULL)
	{
		group_id = msn_userlist_find_group_id(userlist, group_name);

		if (group_id < 0)
		{
			/* Whoa, there is no such group. */
			purple_debug_error("msn", "Group doesn't exist: %s\n", group_name);
			return;
		}
	}

	/* First we're going to check if not there. */
	if (!(user_is_there(user, list_id, group_id)))
	{
		list = lists[list_id];
		purple_debug_error("msn", "User '%s' is not there: %s\n",
						 who, list);
		return;
	}

	/* Then request the rem to the server. */
	list = lists[list_id];

	msn_notification_rem_buddy(userlist->session->notification, list, who, group_id);
}

void
msn_userlist_add_buddy(MsnUserList *userlist,
					   const char *who, int list_id,
					   const char *group_name)
{
	MsnUser *user;
	int group_id;
	const char *list;
	const char *friendly_name;

	group_id = -1;

	if (!purple_email_is_valid(who))
	{
		/* only notify the user about problems adding to the friends list
		 * maybe we should do something else for other lists, but it probably
		 * won't cause too many problems if we just ignore it */
		if (list_id == MSN_LIST_FL)
		{
			char *str = g_strdup_printf(_("Unable to add \"%s\"."), who);
			purple_notify_error(NULL, NULL, str,
							  _("The screen name specified is invalid."));
			g_free(str);
		}

		return;
	}

	if (group_name != NULL)
	{
		group_id = msn_userlist_find_group_id(userlist, group_name);

		if (group_id < 0)
		{
			/* Whoa, we must add that group first. */
			msn_request_add_group(userlist, who, NULL, group_name);
			return;
		}
	}

	user = msn_userlist_find_user(userlist, who);

	/* First we're going to check if it's already there. */
	if (user_is_there(user, list_id, group_id))
	{
		list = lists[list_id];
		purple_debug_error("msn", "User '%s' is already there: %s\n", who, list);
		return;
	}

	friendly_name = (user != NULL) ? get_friendly_name(user) : who;

	/* Then request the add to the server. */
	list = lists[list_id];

	msn_notification_add_buddy(userlist->session->notification, list, who,
							   friendly_name, group_id);
}

void
msn_userlist_move_buddy(MsnUserList *userlist, const char *who,
						const char *old_group_name, const char *new_group_name)
{
	int new_group_id;

	new_group_id = msn_userlist_find_group_id(userlist, new_group_name);

	if (new_group_id < 0)
	{
		msn_request_add_group(userlist, who, old_group_name, new_group_name);
		return;
	}

	msn_userlist_add_buddy(userlist, who, MSN_LIST_FL, new_group_name);
	msn_userlist_rem_buddy(userlist, who, MSN_LIST_FL, old_group_name);
}
