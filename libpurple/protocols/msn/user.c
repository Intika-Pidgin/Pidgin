/**
 * @file user.c User functions
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
#include "user.h"
#include "slp.h"

/*new a user object*/
MsnUser *
msn_user_new(MsnUserList *userlist, const char *passport,
			 const char *friendly_name)
{
	MsnUser *user;

	user = g_new0(MsnUser, 1);

	user->userlist = userlist;

	msn_user_set_passport(user, passport);
	msn_user_set_friendly_name(user, friendly_name);

	return user;
}

/*destroy a user object*/
void
msn_user_destroy(MsnUser *user)
{
	g_return_if_fail(user != NULL);

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	if (user->group_ids != NULL)
	{
		GList *l;
		for (l = user->group_ids; l != NULL; l = l->next)
		{
			g_free(l->data);
		}
		g_list_free(user->group_ids);
	}

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	g_free(user->passport);
	g_free(user->friendly_name);
	g_free(user->uid);
	g_free(user->phone.home);
	g_free(user->phone.work);
	g_free(user->phone.mobile);
	g_free(user->media.artist);
	g_free(user->media.title);
	g_free(user->media.album);
	g_free(user->statusline);

	g_free(user);
}

void
msn_user_update(MsnUser *user)
{
	PurpleAccount *account;
	gboolean offline;

	g_return_if_fail(user != NULL);

	account = user->userlist->session->account;

	offline = (user->status == NULL);

	if (!offline) {
		purple_prpl_got_user_status(account, user->passport, user->status,
				"message", user->statusline, NULL);
	} else {
		if (user->mobile) {
			purple_prpl_got_user_status(account, user->passport, "mobile", NULL);
			purple_prpl_got_user_status(account, user->passport, "available", NULL);
		} else {
			purple_prpl_got_user_status(account, user->passport, "offline", NULL);
		}
	}

	if (!offline || !user->mobile) {
		purple_prpl_got_user_status_deactive(account, user->passport, "mobile");
	}

	if (!offline && user->media.type != CURRENT_MEDIA_UNKNOWN) {
		if (user->media.type == CURRENT_MEDIA_MUSIC) {
			purple_prpl_got_user_status(account, user->passport, "tune",
			                            PURPLE_TUNE_ARTIST, user->media.artist,
			                            PURPLE_TUNE_ALBUM, user->media.album,
			                            PURPLE_TUNE_TITLE, user->media.title,
			                            NULL);
		} else if (user->media.type == CURRENT_MEDIA_GAMES) {
			purple_prpl_got_user_status(account, user->passport, "tune",
			                            "game", user->media.title,
			                            NULL);
		} else if (user->media.type == CURRENT_MEDIA_OFFICE) {
			purple_prpl_got_user_status(account, user->passport, "tune",
			                            "office", user->media.title,
			                            NULL);
		} else {
			purple_debug_warning("msn", "Got CurrentMedia with unknown type %d.\n",
			                     user->media.type);
		}
	} else {
		purple_prpl_got_user_status_deactive(account, user->passport, "tune");
	}

	if (user->idle)
		purple_prpl_got_user_idle(account, user->passport, TRUE, -1);
	else
		purple_prpl_got_user_idle(account, user->passport, FALSE, 0);
}

void
msn_user_set_state(MsnUser *user, const char *state)
{
	const char *status;

	g_return_if_fail(user != NULL);

	if (state == NULL) {
		user->status = NULL;
		return;
	}

	if (!g_ascii_strcasecmp(state, "BSY"))
		status = "busy";
	else if (!g_ascii_strcasecmp(state, "BRB"))
		status = "brb";
	else if (!g_ascii_strcasecmp(state, "AWY"))
		status = "away";
	else if (!g_ascii_strcasecmp(state, "PHN"))
		status = "phone";
	else if (!g_ascii_strcasecmp(state, "LUN"))
		status = "lunch";
	else
		status = "available";

	if (!g_ascii_strcasecmp(state, "IDL"))
		user->idle = TRUE;
	else
		user->idle = FALSE;

	user->status = status;
}

void
msn_user_set_passport(MsnUser *user, const char *passport)
{
	g_return_if_fail(user != NULL);

	g_free(user->passport);
	user->passport = g_strdup(passport);
}

gboolean
msn_user_set_friendly_name(MsnUser *user, const char *name)
{
	g_return_val_if_fail(user != NULL, FALSE);

	if (user->friendly_name && name && !strcmp(user->friendly_name, name))
		return FALSE;

	g_free(user->friendly_name);
	user->friendly_name = g_strdup(name);

	return TRUE;
}

void
msn_user_set_statusline(MsnUser *user, const char *statusline)
{
	g_return_if_fail(user != NULL);

	g_free(user->statusline);
	user->statusline = g_strdup(statusline);
}

void
msn_user_set_currentmedia(MsnUser *user, const CurrentMedia *media)
{
	g_return_if_fail(user != NULL);

	g_free(user->media.title);
	g_free(user->media.album);
	g_free(user->media.artist);

	user->media.type   = media ? media->type : CURRENT_MEDIA_UNKNOWN;
	user->media.title  = media ? g_strdup(media->title) : NULL;
	user->media.artist = media ? g_strdup(media->artist) : NULL;
	user->media.album  = media ? g_strdup(media->album) : NULL;
}

void
msn_user_set_uid(MsnUser *user, const char *uid)
{
	g_return_if_fail(user != NULL);

	g_free(user->uid);
	user->uid = g_strdup(uid);
}

void
msn_user_set_op(MsnUser *user, int list_op)
{
	g_return_if_fail(user != NULL);

	user->list_op |= list_op;
}

void
msn_user_unset_op(MsnUser *user, int list_op)
{
	g_return_if_fail(user != NULL);

	user->list_op &= ~list_op;
}

void
msn_user_set_buddy_icon(MsnUser *user, PurpleStoredImage *img)
{
	MsnObject *msnobj;

	g_return_if_fail(user != NULL);

	msnobj = msn_object_new_from_image(img, "TFR2C2.tmp",
			user->passport, MSN_OBJECT_USERTILE);

	if (!msnobj)
		purple_debug_error("msn", "Unable to open buddy icon from %s!\n", user->passport);

	msn_user_set_object(user, msnobj);
}

/*add group id to User object*/
void
msn_user_add_group_id(MsnUser *user, const char* group_id)
{
	MsnUserList *userlist;
	PurpleAccount *account;
	PurpleBuddy *b;
	PurpleGroup *g;
	const char *passport;
	const char *group_name;

	g_return_if_fail(user != NULL);
	g_return_if_fail(group_id != NULL);

	user->group_ids = g_list_append(user->group_ids, g_strdup(group_id));

	userlist = user->userlist;
	account = userlist->session->account;
	passport = msn_user_get_passport(user);

	group_name = msn_userlist_find_group_name(userlist, group_id);

	purple_debug_info("msn", "User: group id:%s,name:%s,user:%s\n", group_id, group_name, passport);

	g = purple_find_group(group_name);

	if ((group_id == NULL) && (g == NULL))
	{
		g = purple_group_new(group_name);
		purple_blist_add_group(g, NULL);
	}

	b = purple_find_buddy_in_group(account, passport, g);
	if (b == NULL)
	{
		b = purple_buddy_new(account, passport, NULL);
		purple_blist_add_buddy(b, NULL, g, NULL);
	}
	b->proto_data = user;
	/*Update the blist Node info*/
//	purple_blist_node_set_string(&(b->node), "", "");
}

/*check if the msn user is online*/
gboolean
msn_user_is_online(PurpleAccount *account, const char *name)
{
	PurpleBuddy *buddy;

	buddy = purple_find_buddy(account, name);
	return PURPLE_BUDDY_IS_ONLINE(buddy);
}

gboolean
msn_user_is_yahoo(PurpleAccount *account, const char *name)
{
	MsnSession *session = NULL;
	MsnUser *user;
	PurpleConnection *gc;

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		session = gc->proto_data;

	if ((session != NULL) && (user = msn_userlist_find_user(session->userlist, name)) != NULL)
	{
		return (user->networkid == MSN_NETWORK_YAHOO);
	}
	return (strstr(name,"@yahoo.") != NULL);
}

void
msn_user_remove_group_id(MsnUser *user, const char *id)
{
	GList *l;

	g_return_if_fail(user != NULL);
	g_return_if_fail(id != NULL);

	l = g_list_find_custom(user->group_ids, id, (GCompareFunc)strcmp);

	if (l == NULL)
		return;

	g_free(l->data);
	user->group_ids = g_list_delete_link(user->group_ids, l);
}

void
msn_user_set_pending_group(MsnUser *user, const char *group)
{
	user->pending_group = g_strdup(group);
}

char *
msn_user_remove_pending_group(MsnUser *user)
{
	char *group = user->pending_group;
	user->pending_group = NULL;
	return group;
}

void
msn_user_set_home_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	g_free(user->phone.home);
	user->phone.home = g_strdup(number);
}

void
msn_user_set_work_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	g_free(user->phone.work);
	user->phone.work = g_strdup(number);
}

void
msn_user_set_mobile_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	g_free(user->phone.mobile);
	user->phone.mobile = g_strdup(number);
}

void
msn_user_set_clientid(MsnUser *user, guint clientid)
{
	g_return_if_fail(user != NULL);

	user->clientid = clientid;
}

void
msn_user_set_network(MsnUser *user, MsnNetwork network)
{
	g_return_if_fail(user != NULL);

	user->networkid = network;
}

void
msn_user_set_object(MsnUser *user, MsnObject *obj)
{
	g_return_if_fail(user != NULL);

	if (user->msnobj != NULL)
		msn_object_destroy(user->msnobj);

	user->msnobj = obj;

	if (user->list_op & MSN_LIST_FL_OP)
		msn_queue_buddy_icon_request(user);
}

void
msn_user_set_client_caps(MsnUser *user, GHashTable *info)
{
	g_return_if_fail(user != NULL);
	g_return_if_fail(info != NULL);

	if (user->clientcaps != NULL)
		g_hash_table_destroy(user->clientcaps);

	user->clientcaps = info;
}

const char *
msn_user_get_passport(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->passport;
}

const char *
msn_user_get_friendly_name(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->friendly_name;
}

const char *
msn_user_get_home_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.home;
}

const char *
msn_user_get_work_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.work;
}

const char *
msn_user_get_mobile_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.mobile;
}

guint
msn_user_get_clientid(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, 0);

	return user->clientid;
}

MsnObject *
msn_user_get_object(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->msnobj;
}

GHashTable *
msn_user_get_client_caps(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->clientcaps;
}
