/*
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
 *
 */

/*
 * Note: When handling the list of struct yahoo_pair's from an incoming
 * packet the value might not be UTF-8. You should either validate that
 * it is UTF-8 using g_utf8_validate() or use yahoo_string_decode().
 */

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "buddylist.h"
#include "ciphers/md5hash.h"
#include "cmds.h"
#include "core.h"
#include "debug.h"
#include "http.h"
#include "network.h"
#include "notify.h"
#include "protocol.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "version.h"
#include "xmlnode.h"

#include "ymsg.h"
#include "yahoochat.h"
#include "yahoo_aliases.h"
#include "yahoo_doodle.h"
#include "yahoo_filexfer.h"
#include "yahoo_friend.h"
#include "yahoo_packet.h"
#include "yahoo_picture.h"
#include "ycht.h"

/* #define YAHOO_DEBUG */

/* It doesn't look like it is working (the previously used host is down, another
 * one doesn't send us back cookies).
 */
#define TRY_WEBMESSENGER_LOGIN 0

/* One hour */
#define PING_TIMEOUT 3600

/* One minute */
#define KEEPALIVE_TIMEOUT 60

#if TRY_WEBMESSENGER_LOGIN
static void
yahoo_login_page_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _unused);
#endif /* TRY_WEBMESSENGER_LOGIN */

static void yahoo_update_status(PurpleConnection *gc, const char *name, YahooFriend *f)
{
	char *status = NULL;

	if (!gc || !name || !f || !purple_blist_find_buddy(purple_connection_get_account(gc), name))
		return;

	switch (f->status) {
	case YAHOO_STATUS_OFFLINE:
		status = YAHOO_STATUS_TYPE_OFFLINE;
		break;
	case YAHOO_STATUS_AVAILABLE:
		status = YAHOO_STATUS_TYPE_AVAILABLE;
		break;
	case YAHOO_STATUS_BRB:
		status = YAHOO_STATUS_TYPE_BRB;
		break;
	case YAHOO_STATUS_BUSY:
		status = YAHOO_STATUS_TYPE_BUSY;
		break;
	case YAHOO_STATUS_NOTATHOME:
		status = YAHOO_STATUS_TYPE_NOTATHOME;
		break;
	case YAHOO_STATUS_NOTATDESK:
		status = YAHOO_STATUS_TYPE_NOTATDESK;
		break;
	case YAHOO_STATUS_NOTINOFFICE:
		status = YAHOO_STATUS_TYPE_NOTINOFFICE;
		break;
	case YAHOO_STATUS_ONPHONE:
		status = YAHOO_STATUS_TYPE_ONPHONE;
		break;
	case YAHOO_STATUS_ONVACATION:
		status = YAHOO_STATUS_TYPE_ONVACATION;
		break;
	case YAHOO_STATUS_OUTTOLUNCH:
		status = YAHOO_STATUS_TYPE_OUTTOLUNCH;
		break;
	case YAHOO_STATUS_STEPPEDOUT:
		status = YAHOO_STATUS_TYPE_STEPPEDOUT;
		break;
	case YAHOO_STATUS_INVISIBLE: /* this should never happen? */
		status = YAHOO_STATUS_TYPE_INVISIBLE;
		break;
	case YAHOO_STATUS_CUSTOM:
	case YAHOO_STATUS_IDLE:
		if (!f->away)
			status = YAHOO_STATUS_TYPE_AVAILABLE;
		else
			status = YAHOO_STATUS_TYPE_AWAY;
		break;
	default:
		purple_debug_warning("yahoo", "Warning, unknown status %d\n", f->status);
		break;
	}

	if (status) {
		if (f->status == YAHOO_STATUS_CUSTOM)
			purple_protocol_got_user_status(purple_connection_get_account(gc), name, status, "message",
			                          yahoo_friend_get_status_message(f), NULL);
		else
			purple_protocol_got_user_status(purple_connection_get_account(gc), name, status, NULL);
	}

	if (f->idle != 0)
		purple_protocol_got_user_idle(purple_connection_get_account(gc), name, TRUE, f->idle);
	else
		purple_protocol_got_user_idle(purple_connection_get_account(gc), name, FALSE, 0);

	if (f->sms)
		purple_protocol_got_user_status(purple_connection_get_account(gc), name, YAHOO_STATUS_TYPE_MOBILE, NULL);
	else
		purple_protocol_got_user_status_deactive(purple_connection_get_account(gc), name, YAHOO_STATUS_TYPE_MOBILE);
}

static void yahoo_process_status(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GSList *l = pkt->hash;
	YahooFriend *f = NULL;
	char *name = NULL;
	gboolean unicode = FALSE;
	char *message = NULL;

	if (pkt->service == YAHOO_SERVICE_LOGOFF && pkt->status == -1) {
		if (!purple_account_get_remember_password(account))
			purple_account_set_password(account, NULL, NULL, NULL);
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NAME_IN_USE,
			_("You have signed on from another location"));
		return;
	}

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 0: /* we won't actually do anything with this */
		case 1: /* we won't actually do anything with this */
			break;
		case 8: /* how many online buddies we have */
			break;
		case 7: /* the current buddy */
			/* update the previous buddy before changing the variables */
			if (f) {
				if (message)
					yahoo_friend_set_status_message(f, yahoo_string_decode(gc, message, unicode));
				if (name)
					yahoo_update_status(gc, name, f);
			}
			name = message = NULL;
			f = NULL;
			if (pair->value && g_utf8_validate(pair->value, -1, NULL)) {
				name = pair->value;
				f = yahoo_friend_find_or_new(gc, name);
			}
			break;
		case 10: /* state */
			if (!f)
				break;

			f->status = strtol(pair->value, NULL, 10);
			if ((f->status >= YAHOO_STATUS_BRB) && (f->status <= YAHOO_STATUS_STEPPEDOUT))
				f->away = 1;
			else
				f->away = 0;

			if (f->status == YAHOO_STATUS_IDLE) {
				/* Idle may have already been set in a more precise way in case 137 */
				if (f->idle == 0)
				{
					if(pkt->service == YAHOO_SERVICE_STATUS_15)
						f->idle = -1;
					else
						f->idle = time(NULL);
				}
			} else
				f->idle = 0;

			if (f->status != YAHOO_STATUS_CUSTOM)
				yahoo_friend_set_status_message(f, NULL);

			f->sms = 0;
			break;
		case 19: /* custom message */
			if (f)
				message = pair->value;
			break;
		case 11: /* this is the buddy's session id */
			if (f)
				f->session_id = strtol(pair->value, NULL, 10);
			break;
		case 17: /* in chat? */
			break;
		case 47: /* is custom status away or not? 2=idle*/
			if (!f)
				break;

			/* I have no idea what it means when this is
			 * set when someone's available, but it doesn't
			 * mean idle. */
			if (f->status == YAHOO_STATUS_AVAILABLE)
				break;

			f->away = strtol(pair->value, NULL, 10);
			if (f->away == 2) {
				/* Idle may have already been set in a more precise way in case 137 */
				if (f->idle == 0)
				{
					if(pkt->service == YAHOO_SERVICE_STATUS_15)
						f->idle = -1;
					else
						f->idle = time(NULL);
				}
			}

			break;
		case 138: /* when value is 1, either we're not idle, or we are but won't say how long */
			if (!f)
				break;

			if( (strtol(pair->value, NULL, 10) == 1) && (f->idle) )
				f->idle = -1;
			break;
		case 137: /* usually idle time in seconds, sometimes login time */
			if (!f)
				break;

			if (f->status != YAHOO_STATUS_AVAILABLE)
				f->idle = time(NULL) - strtol(pair->value, NULL, 10);
			break;
		case 13: /* bitmask, bit 0 = pager, bit 1 = chat, bit 2 = game */
			if (strtol(pair->value, NULL, 10) == 0) {
				if (f)
					f->status = YAHOO_STATUS_OFFLINE;
				if (name) {
					purple_protocol_got_user_status(account, name, "offline", NULL);
					purple_protocol_got_user_status_deactive(account, name, YAHOO_STATUS_TYPE_MOBILE);
				}
				break;
			}
			break;
		case 60: /* SMS */
			if (f) {
				f->sms = strtol(pair->value, NULL, 10);
				yahoo_update_status(gc, name, f);
			}
			break;
		case 197: /* Avatars */
		{
			guchar *decoded;
			char *tmp;
			gsize len;

			if (pair->value) {
				decoded = purple_base64_decode(pair->value, &len);
				if (decoded && len > 0) {
					tmp = purple_str_binary_to_ascii(decoded, len);
					purple_debug_info("yahoo", "Got key 197, value = %s\n", tmp);
					g_free(tmp);
				}
				g_free(decoded);
			}
			break;
		}
		case 192: /* Pictures, aka Buddy Icons, checksum */
		{
			/* FIXME: Please, if you know this protocol,
			 * FIXME: fix up the strtol() stuff if possible. */
			int cksum = strtol(pair->value, NULL, 10);
			const char *locksum = NULL;
			PurpleBuddy *b;

			if (!name)
				break;

			b = purple_blist_find_buddy(purple_connection_get_account(gc), name);

			if (!cksum || (cksum == -1)) {
				if (f)
					yahoo_friend_set_buddy_icon_need_request(f, TRUE);
				purple_buddy_icons_set_for_user(purple_connection_get_account(gc), name, NULL, 0, NULL);
				break;
			}

			if (!f)
				break;

			yahoo_friend_set_buddy_icon_need_request(f, FALSE);
			if (b) {
				locksum = purple_buddy_icons_get_checksum_for_user(b);
				if (!locksum || (cksum != strtol(locksum, NULL, 10)))
					yahoo_send_picture_request(gc, name);
			}

			break;
		}
		case 16: /* Custom error message */
			{
				char *tmp = yahoo_string_decode(gc, pair->value, TRUE);
				purple_notify_error(gc, NULL, tmp, NULL,
					purple_request_cpar_from_connection(gc));
				g_free(tmp);
			}
			break;
		case 97: /* Unicode status message */
			unicode = !strcmp(pair->value, "1");
			break;
		case 244: /* client version number. Yahoo Client Detection */
			if(f && strtol(pair->value, NULL, 10))
				f->version_id = strtol(pair->value, NULL, 10);
			break;
		default:
			purple_debug_warning("yahoo",
					   "Unknown status key %d\n", pair->key);
			break;
		}

		l = l->next;
	}

	if (f) {
		if (pkt->service == YAHOO_SERVICE_LOGOFF)
			f->status = YAHOO_STATUS_OFFLINE;
		if (message)
			yahoo_friend_set_status_message(f, yahoo_string_decode(gc, message, unicode));

		if (name) /* update the last buddy */
			yahoo_update_status(gc, name, f);
	}
}

static void yahoo_do_group_check(PurpleAccount *account, GHashTable *ht, const char *name, const char *group)
{
	PurpleBuddy *b;
	PurpleGroup *g;
	GSList *list, *i;
	gboolean onlist = FALSE;
	char *oname = NULL;

	if (g_hash_table_lookup_extended(ht, name, (gpointer *)&oname, (gpointer *)&list))
		g_hash_table_steal(ht, oname);
	else
		list = purple_blist_find_buddies(account, name);

	for (i = list; i; i = i->next) {
		b = i->data;
		g = purple_buddy_get_group(b);
		if (!purple_utf8_strcasecmp(group, purple_group_get_name(g))) {
			purple_debug_misc("yahoo",
				"Oh good, %s is in the right group (%s).\n", name, group);
			list = g_slist_delete_link(list, i);
			onlist = TRUE;
			break;
		}
	}

	if (!onlist) {
		purple_debug_misc("yahoo",
			"Uhoh, %s isn't on the list (or not in this group), adding him to group %s.\n", name, group);
		if (!(g = purple_blist_find_group(group))) {
			g = purple_group_new(group);
			purple_blist_add_group(g, NULL);
		}
		b = purple_buddy_new(account, name, NULL);
		purple_blist_add_buddy(b, NULL, g, NULL);
	}

	if (list) {
		if (!oname)
			oname = g_strdup(name);
		g_hash_table_insert(ht, oname, list);
	} else
		g_free(oname);
}

static void yahoo_do_group_cleanup(gpointer key, gpointer value, gpointer user_data)
{
	char *name = key;
	GSList *list = value, *i;
	PurpleBuddy *b;
	PurpleGroup *g;

	for (i = list; i; i = i->next) {
		b = i->data;
		g = purple_buddy_get_group(b);
		purple_debug_misc("yahoo", "Deleting Buddy %s from group %s.\n", name,
				purple_group_get_name(g));
		purple_blist_remove_buddy(b);
	}
}

static char *_getcookie(char *rawcookie)
{
	char *cookie = NULL;
	char *tmpcookie;
	char *cookieend;

	if (strlen(rawcookie) < 2)
		return NULL;
	tmpcookie = g_strdup(rawcookie+2);
	cookieend = strchr(tmpcookie, ';');

	if (cookieend)
		*cookieend = '\0';

	cookie = g_strdup(tmpcookie);
	g_free(tmpcookie);

	return cookie;
}

static void yahoo_process_cookie(YahooData *yd, char *c)
{
	if (c[0] == 'Y') {
		g_free(yd->cookie_y);
		yd->cookie_y = _getcookie(c);
	} else if (c[0] == 'T') {
		g_free(yd->cookie_t);
		yd->cookie_t = _getcookie(c);
	} else
		purple_debug_info("yahoo", "Unrecognized cookie '%c'\n", c[0]);
	yd->cookies = g_slist_prepend(yd->cookies, g_strdup(c));
}

static void yahoo_process_list_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;

	PurpleAccount *account = purple_connection_get_account(gc);
	YahooData *yd = purple_connection_get_protocol_data(gc);
	GHashTable *ht;
	char *norm_bud = NULL;
	char *temp = NULL;
	YahooFriend *f = NULL; /* It's your friends. They're going to want you to share your StarBursts. */
	                       /* But what if you had no friends? */
	int stealth = 0;

	ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_slist_free);

	while (l) {
		struct yahoo_pair *pair = l->data;
		l = l->next;

		switch (pair->key) {
		case 302:
			/* This is always 318 before a group, 319 before the first s/n in a group, 320 before any ignored s/n.
			 * It is not sent for s/n's in a group after the first.
			 * All ignored s/n's are listed last, so when we see a 320 we clear the group and begin marking the
			 * s/n's as ignored.  It is always followed by an identical 300 key.
			 */
			if (pair->value && !strcmp(pair->value, "320")) {
				/* No longer in any group; this indicates the start of the ignore list. */
				g_free(yd->current_list15_grp);
				yd->current_list15_grp = NULL;
			}

			break;
		case 301: /* This is 319 before all s/n's in a group after the first. It is followed by an identical 300. */
			if(temp != NULL) {
				norm_bud = g_strdup(temp);

				if (yd->current_list15_grp) {
					/* This buddy is in a group */
					f = yahoo_friend_find_or_new(gc, norm_bud);
					if (!purple_blist_find_buddy(account, norm_bud)) {
						PurpleBuddy *b;
						PurpleGroup *g;
						if (!(g = purple_blist_find_group(yd->current_list15_grp))) {
							g = purple_group_new(yd->current_list15_grp);
							purple_blist_add_group(g, NULL);
						}
						b = purple_buddy_new(account, norm_bud, NULL);
						purple_blist_add_buddy(b, NULL, g, NULL);
					}
					yahoo_do_group_check(account, ht, norm_bud, yd->current_list15_grp);

					if(stealth == 2)
						f->presence = YAHOO_PRESENCE_PERM_OFFLINE;

					yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_NOT_CONNECTED);
					f->p2p_packet_sent = 0;
				} else {
					/* This buddy is on the ignore list (and therefore in no group) */
					purple_debug_info("yahoo", "%s adding %s to the deny list because of the ignore list / no group was found\n", purple_account_get_username(account), norm_bud);
					purple_account_privacy_deny_add(account, norm_bud, 1);
				}

				g_free(norm_bud);
				norm_bud=NULL;
				stealth = 0;
				g_free(temp);
				temp = NULL;
			}
			break;
		case 300: /* This is 318 before a group, 319 before any s/n in a group, and 320 before any ignored s/n. */
			break;
		case 65: /* This is the group */
			g_free(yd->current_list15_grp);
			yd->current_list15_grp = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 7: /* buddy's s/n */
			if (g_utf8_validate(pair->value, -1, NULL)) {
				g_free(temp);
				temp = g_strdup(purple_normalize(account, pair->value));
			} else {
				purple_debug_warning("yahoo", "yahoo_process_list_15 "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 59: /* somebody told cookies come here too, but im not sure */
			if (g_utf8_validate(pair->value, -1, NULL)) {
				yahoo_process_cookie(yd, pair->value);
			} else {
				purple_debug_warning("yahoo", "yahoo_process_list_15 "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 317: /* Stealth Setting */
			stealth = strtol(pair->value, NULL, 10);
			break;
		}
	}

	g_hash_table_foreach(ht, yahoo_do_group_cleanup, NULL);

	/* The reporter of ticket #9745 determined that we weren't retrieving the
	 * aliases during buddy list retrieval, so we never updated aliases that
	 * changed while we were signed off. */
	yahoo_fetch_aliases(gc);

	/* Now that we have processed the buddy list, we can say yahoo has connected */
	purple_connection_set_display_name(gc, purple_normalize(account, purple_account_get_username(account)));
	yd->logged_in = TRUE;
	purple_debug_info("yahoo","Authentication: Connection established\n");
	purple_connection_set_state(gc, PURPLE_CONNECTION_CONNECTED);
	if (yd->picture_upload_todo) {
		yahoo_buddy_icon_upload(gc, yd->picture_upload_todo);
		yd->picture_upload_todo = NULL;
	}
	yahoo_set_status(account, purple_account_get_active_status(account));

	g_hash_table_destroy(ht);
	g_free(temp);
}

static void yahoo_process_list(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	gboolean got_serv_list = FALSE;
	YahooFriend *f = NULL;
	PurpleAccount *account = purple_connection_get_account(gc);
	YahooData *yd = purple_connection_get_protocol_data(gc);
	GHashTable *ht;

	char **lines;
	char **split;
	char **buddies;
	char **tmp, **bud, *norm_bud;
	char *grp = NULL;

	if (pkt->id)
		yd->session_id = pkt->id;

	while (l) {
		struct yahoo_pair *pair = l->data;
		l = l->next;

		switch (pair->key) {
		case 87:
			if (!yd->tmp_serv_blist)
				yd->tmp_serv_blist = g_string_new(pair->value);
			else
				g_string_append(yd->tmp_serv_blist, pair->value);
			break;
		case 88:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				if (!yd->tmp_serv_ilist)
					yd->tmp_serv_ilist = g_string_new(pair->value);
				else
					g_string_append(yd->tmp_serv_ilist, pair->value);
			} else {
				purple_debug_warning("yahoo", "yahoo_process_list "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 89:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				yd->profiles = g_strsplit(pair->value, ",", -1);
			} else {
				purple_debug_warning("yahoo", "yahoo_process_list "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 59: /* cookies, yum */
			if (g_utf8_validate(pair->value, -1, NULL)) {
				yahoo_process_cookie(yd, pair->value);
			} else {
				purple_debug_warning("yahoo", "yahoo_process_list "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case YAHOO_SERVICE_PRESENCE_PERM:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				if (!yd->tmp_serv_plist)
					yd->tmp_serv_plist = g_string_new(pair->value);
				else
					g_string_append(yd->tmp_serv_plist, pair->value);
			} else {
				purple_debug_warning("yahoo", "yahoo_process_list "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		}
	}

	if (pkt->status != 0)
		return;

	if (yd->tmp_serv_blist) {
		ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_slist_free);

		lines = g_strsplit(yd->tmp_serv_blist->str, "\n", -1);
		for (tmp = lines; *tmp; tmp++) {
			split = g_strsplit(*tmp, ":", 2);
			if (!split)
				continue;
			if (!split[0] || !split[1]) {
				g_strfreev(split);
				continue;
			}
			grp = yahoo_string_decode(gc, split[0], FALSE);
			buddies = g_strsplit(split[1], ",", -1);
			for (bud = buddies; bud && *bud; bud++) {
				if (!g_utf8_validate(*bud, -1, NULL)) {
					purple_debug_warning("yahoo", "yahoo_process_list "
							"got non-UTF-8 string for bud\n");
					continue;
				}

				norm_bud = g_strdup(purple_normalize(account, *bud));
				f = yahoo_friend_find_or_new(gc, norm_bud);

				if (!purple_blist_find_buddy(account, norm_bud)) {
					PurpleBuddy *b;
					PurpleGroup *g;
					if (!(g = purple_blist_find_group(grp))) {
						g = purple_group_new(grp);
						purple_blist_add_group(g, NULL);
					}
					b = purple_buddy_new(account, norm_bud, NULL);
					purple_blist_add_buddy(b, NULL, g, NULL);
				}

				yahoo_do_group_check(account, ht, norm_bud, grp);
				/* set p2p status not connected and no p2p packet sent */
				yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_NOT_CONNECTED);
				f->p2p_packet_sent = 0;

				g_free(norm_bud);
			}
			g_strfreev(buddies);
			g_strfreev(split);
			g_free(grp);
		}
		g_strfreev(lines);

		g_string_free(yd->tmp_serv_blist, TRUE);
		yd->tmp_serv_blist = NULL;
		g_hash_table_foreach(ht, yahoo_do_group_cleanup, NULL);
		g_hash_table_destroy(ht);
	}

	if (yd->tmp_serv_ilist) {
		buddies = g_strsplit(yd->tmp_serv_ilist->str, ",", -1);
		for (bud = buddies; bud && *bud; bud++) {
			/* The server is already ignoring the user */
			got_serv_list = TRUE;
			purple_account_privacy_deny_add(account, *bud, 1);
		}
		g_strfreev(buddies);

		g_string_free(yd->tmp_serv_ilist, TRUE);
		yd->tmp_serv_ilist = NULL;
	}

	if (got_serv_list &&
		((purple_account_get_privacy_type(account) != PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST) &&
		(purple_account_get_privacy_type(account) != PURPLE_ACCOUNT_PRIVACY_DENY_ALL) &&
		(purple_account_get_privacy_type(account) != PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS)))
	{
		purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_DENY_USERS);
		purple_debug_info("yahoo", "%s privacy defaulting to PURPLE_ACCOUNT_PRIVACY_DENY_USERS.\n",
				purple_account_get_username(account));
	}

	if (yd->tmp_serv_plist) {
		buddies = g_strsplit(yd->tmp_serv_plist->str, ",", -1);
		for (bud = buddies; bud && *bud; bud++) {
			f = yahoo_friend_find(gc, *bud);
			if (f) {
				purple_debug_info("yahoo", "%s setting presence for %s to PERM_OFFLINE\n",
						purple_account_get_username(account), *bud);
				f->presence = YAHOO_PRESENCE_PERM_OFFLINE;
			}
		}
		g_strfreev(buddies);
		g_string_free(yd->tmp_serv_plist, TRUE);
		yd->tmp_serv_plist = NULL;

	}
	/* Now that we've got the list, request aliases */
	yahoo_fetch_aliases(gc);
}

/* pkt_type is YAHOO_PKT_TYPE_SERVER if pkt arrives from yahoo server, YAHOO_PKT_TYPE_P2P if pkt arrives through p2p */
static void yahoo_process_notify(PurpleConnection *gc, struct yahoo_packet *pkt, yahoo_pkt_type pkt_type)
{
	PurpleAccount *account;
	char *msg = NULL;
	char *from = NULL;
	char *stat = NULL;
	char *game = NULL;
	YahooFriend *f = NULL;
	GSList *l = pkt->hash;
	gint val_11 = 0;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	account = purple_connection_get_account(gc);

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4 || pair->key == 1) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				from = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_notify "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 49) {
			msg = pair->value;
		} else if (pair->key == 13) {
			stat = pair->value;
		} else if (pair->key == 14) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				game = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_notify "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 11) {
			val_11 = strtol(pair->value, NULL, 10);
		}
		l = l->next;
	}

	if (!from || !msg)
		return;

	/* disconnect the peer if connected through p2p and sends wrong value for session id */
	if ((pkt_type == YAHOO_PKT_TYPE_P2P) && (val_11 != yd->session_id)) {
		purple_debug_warning("yahoo","p2p: %s sent us notify with wrong session id. Disconnecting p2p connection to peer\n", from);
		/* remove from p2p connection lists, also calls yahoo_p2p_disconnect_destroy_data */
		g_hash_table_remove(yd->peers, from);
		return;
	}

	if (!g_ascii_strncasecmp(msg, "TYPING", strlen("TYPING"))
		&& (purple_account_privacy_check(account, from)))
	{

		if (stat && *stat == '1')
			purple_serv_got_typing(gc, from, 0, PURPLE_IM_TYPING);
		else
			purple_serv_got_typing_stopped(gc, from);

	} else if (!g_ascii_strncasecmp(msg, "GAME", strlen("GAME"))) {
		PurpleBuddy *bud = purple_blist_find_buddy(account, from);

		if (!bud) {
			purple_debug_warning("yahoo",
					   "%s is playing a game, and doesn't want you to know.\n", from);
		}

		f = yahoo_friend_find(gc, from);
		if (!f)
			return; /* if they're not on the list, don't bother */

		yahoo_friend_set_game(f, NULL);

		if (stat && *stat == '1') {
			yahoo_friend_set_game(f, game);
			if (bud)
				yahoo_update_status(gc, from, f);
		}
	} else if (!g_ascii_strncasecmp(msg, "WEBCAMINVITE", strlen("WEBCAMINVITE"))) {
		PurpleIMConversation *im = purple_conversations_find_im_with_account(from, account);
		char *buf = g_strdup_printf(_("%s has sent you a webcam invite, which is not yet supported."), from);
		purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
			buf, PURPLE_MESSAGE_NOTIFY);
		g_free(buf);
	}
}


struct _yahoo_im {
	char *from;
	char *active_id;
	int time;
	int utf8;
	int buddy_icon;
	char *id;
	char *msg;
};

static void yahoo_process_sms_message(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account;
	GSList *l = pkt->hash;
	struct _yahoo_im *sms = NULL;
	YahooData *yd;
	char *server_msg = NULL;
	char *m;

	yd = purple_connection_get_protocol_data(gc);
	account = purple_connection_get_account(gc);

	while (l != NULL) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				sms = g_new0(struct _yahoo_im, 1);
				sms->from = g_strdup_printf("+%s", pair->value);
				sms->time = time(NULL);
				sms->utf8 = TRUE;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_sms_message "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 14) {
			if (sms)
				sms->msg = pair->value;
		} else if (pair->key == 68) {
			if(sms)
				g_hash_table_insert(yd->sms_carrier, g_strdup(sms->from), g_strdup(pair->value));
		} else if (pair->key == 16) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				server_msg = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_sms_message "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		}
		l = l->next;
	}

	if(!sms) {
		purple_debug_info("yahoo", "Received a malformed SMS packet!\n");
		return;
	}

	if ((int)pkt->status < 0)
		pkt->status = YAHOO_STATUS_DISCONNECTED;
	if (pkt->status == YAHOO_STATUS_DISCONNECTED) {
		if (server_msg) {
			PurpleIMConversation *im;
			im = purple_conversations_find_im_with_account(sms->from, account);
			if (im == NULL)
				im = purple_im_conversation_new(account, sms->from);
			purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
				server_msg, 0);
		}
		else {
			purple_notify_error(gc, NULL,
				_("Your SMS was not delivered"), NULL,
				purple_request_cpar_from_connection(gc));
		}

		g_free(sms->from);
		g_free(sms);
		return ;
	}

	if (!sms->from || !sms->msg) {
		g_free(sms);
		return;
	}

	m = yahoo_string_decode(gc, sms->msg, sms->utf8);
	purple_serv_got_im(gc, sms->from, m, 0, sms->time);

	g_free(m);
	g_free(sms->from);
	g_free(sms);
}

/* pkt_type is YAHOO_PKT_TYPE_SERVER if pkt arrives from yahoo server, YAHOO_PKT_TYPE_P2P if pkt arrives through p2p */
static void yahoo_process_message(PurpleConnection *gc, struct yahoo_packet *pkt, yahoo_pkt_type pkt_type)
{
	PurpleAccount *account;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	GSList *l = pkt->hash;
	GSList *list = NULL;
	struct _yahoo_im *im = NULL;

	account = purple_connection_get_account(gc);

	if (pkt->status <= 1 || pkt->status == 5 || pkt->status == YAHOO_STATUS_OFFLINE) {
	/* messages are received with status YAHOO_STATUS_OFFLINE in case of p2p */
		while (l != NULL) {
			struct yahoo_pair *pair = l->data;
			if (pair->key == 4 || pair->key == 1) {
				if (g_utf8_validate(pair->value, -1, NULL)) {
					im = g_new0(struct _yahoo_im, 1);
					list = g_slist_append(list, im);
					im->from = pair->value;
					im->time = time(NULL);
					im->utf8 = TRUE;
				} else {
					purple_debug_warning("yahoo", "yahoo_process_message "
							"got non-UTF-8 string for key %d\n", pair->key);
				}
			} else if (im && pair->key == 5) {
				im->active_id = pair->value;
			} else if (pair->key == 97) {
				if (im)
					im->utf8 = strtol(pair->value, NULL, 10);
			} else if (pair->key == 15) {
				if (im)
					im->time = strtol(pair->value, NULL, 10);
			} else if (pair->key == 206) {
				if (im)
					im->buddy_icon = strtol(pair->value, NULL, 10);
			} else if (pair->key == 14) {
				if (im)
					im->msg = pair->value;
			} else if (im && (pair->key == 11)) {
				/* peer session id */
				/* disconnect the peer if connected through p2p and sends wrong value for session id */
				if( (pkt_type == YAHOO_PKT_TYPE_P2P)
						&& (yd->session_id != strtol(pair->value, NULL, 10)) )
				{
					purple_debug_warning("yahoo","p2p: %s sent us message with wrong session id. Disconnecting p2p connection to peer\n", im->from);
					/* remove from p2p connection lists, also calls yahoo_p2p_disconnect_destroy_data */
					g_hash_table_remove(yd->peers, im->from);
					g_free(im);
					return; /* Not sure whether we should process remaining IMs in this packet */
				}

			} else if (im && pair->key == 63 && g_utf8_validate(pair->value, -1, NULL)) {
				/* IMV key */
				/* Check for the Doodle IMV */
				if (im->from != NULL)
				{
					g_hash_table_replace(yd->imvironments, g_strdup(im->from), g_strdup(pair->value));

					if (strstr(pair->value, "doodle;") != NULL)
					{
						PurpleWhiteboard *wb;

						if (!purple_account_privacy_check(account, im->from)) {
							purple_debug_info("yahoo", "Doodle request from %s dropped.\n",
												im->from);
							g_free(im);
							return;
						}
						/* I'm not sure the following ever happens -DAA */
						wb = purple_whiteboard_get_session(account, im->from);

						/* If a Doodle session doesn't exist between this user */
						if(wb == NULL)
						{
							doodle_session *ds;
							wb = purple_whiteboard_new(account, im->from,
											DOODLE_STATE_REQUESTED);
							ds = purple_whiteboard_get_protocol_data(wb);
							ds->imv_key = g_strdup(pair->value);

							yahoo_doodle_command_send_request(gc, im->from, pair->value);
							yahoo_doodle_command_send_ready(gc, im->from, pair->value);
						}
					}
				}
			} else if (pair->key == 429) {
				if (im)
					im->id = pair->value;
			}
			l = l->next;
		}
	} else if (pkt->status == 2) {
		purple_notify_error(gc, NULL,
			_("Your Yahoo! message did not get sent."), NULL,
			purple_request_cpar_from_connection(gc));
	}

	for (l = list; l; l = l->next) {
		char *m, *m2;
		im = l->data;

		if (!im->msg) {
			g_free(im);
			continue;
		}

		if (!purple_account_privacy_check(account, im->from)) {
			purple_debug_info("yahoo", "Message from %s dropped.\n", im->from);
			return;
		}

		/*
		 * TODO: Is there anything else we should check when determining whether
		 *       we should send an acknowledgement?
		 */
		if (im->id != NULL) {
			/* Send acknowledgement.  If we don't do this then the official
			 * Yahoo Messenger client for Windows will send us the same
			 * message 7 seconds later as an offline message.  This is true
			 * for at least version 9.0.0.2162 on Windows XP. */
			struct yahoo_packet *pkt2;
			pkt2 = yahoo_packet_new(YAHOO_SERVICE_MESSAGE_ACK,
					YAHOO_STATUS_AVAILABLE, pkt->id);
			yahoo_packet_hash(pkt2, "ssisii",
					1, im->active_id,  /* May not always be the connection's display name */
					5, im->from,
					302, 430,
					430, im->id,
					303, 430,
					450, 0);
			yahoo_packet_send_and_free(pkt2, yd);
		}

		m = yahoo_string_decode(gc, im->msg, im->utf8);
		/* This may actually not be necessary, but it appears
		 * that at least at one point some clients were sending
		 * "\r\n" as line delimiters, so we want to avoid double
		 * lines. */
		m2 = purple_strreplace(m, "\r\n", "\n");
		g_free(m);
		m = m2;
		purple_util_chrreplace(m, '\r', '\n');
		if (!strcmp(m, "<ding>")) {
			char *username;

			username = g_markup_escape_text(im->from, -1);
			purple_protocol_got_attention(gc, username, YAHOO_BUZZ);
			g_free(username);
			g_free(m);
			g_free(im);
			continue;
		}

		m2 = yahoo_codes_to_html(m);
		g_free(m);

		purple_serv_got_im(gc, im->from, m2, 0, im->time);
		g_free(m2);
		g_free(im);
	}

	g_slist_free(list);
}

static void yahoo_process_sysmessage(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *prim, *me = NULL, *msg = NULL;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 5) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				me = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_sysmessage "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 14) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				msg = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_sysmessage "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		}

		l = l->next;
	}

	if (!msg || !g_utf8_validate(msg, -1, NULL))
		return;

	prim = g_strdup_printf(_("Yahoo! system message for %s:"),
	                       me?me:purple_connection_get_display_name(gc));
	purple_notify_info(NULL, NULL, prim, msg,
		purple_request_cpar_from_connection(gc));
	g_free(prim);
}

struct yahoo_add_request {
	PurpleConnection *gc;
	char *id;
	char *who;
};

static void
yahoo_buddy_add_authorize_cb(const char *message, gpointer data)
{
	struct yahoo_add_request *add_req = data;
	struct yahoo_packet *pkt;
	YahooData *yd = purple_connection_get_protocol_data(add_req->gc);
	const char *who = add_req->who;

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH_REQ_15, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "ssii",
					  1, add_req->id,
					  5, who,
					  13, 1,
					  334, 0);

	yahoo_packet_send_and_free(pkt, yd);

	g_free(add_req->id);
	g_free(add_req->who);
	g_free(add_req);
}

static void
yahoo_buddy_add_deny_cb(const char *msg, gpointer data)
{
	struct yahoo_add_request *add_req = data;
	YahooData *yd = purple_connection_get_protocol_data(add_req->gc);
	struct yahoo_packet *pkt;
	char *encoded_msg = NULL;
	const char *who = add_req->who;

	if (msg && *msg)
		encoded_msg = yahoo_string_encode(add_req->gc, msg, FALSE);

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH_REQ_15,
			YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "ssiiis",
					  1, add_req->id,
					  5, who,
					  13, 2,
					  334, 0,
					  97, 1, /* UTF-8 */
					  14, encoded_msg ? encoded_msg : "");


	yahoo_packet_send_and_free(pkt, yd);

	g_free(encoded_msg);

	g_free(add_req->id);
	g_free(add_req->who);
	g_free(add_req);
}

static void yahoo_buddy_denied_our_add(PurpleConnection *gc, const char *who, const char *reason)
{
	char *notify_msg;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	if (who == NULL)
		return;

	if (reason != NULL) {
		char *msg2 = yahoo_string_decode(gc, reason, FALSE);
		notify_msg = g_strdup_printf(_("%s has (retroactively) denied your request to add them to your list for the following reason: %s."), who, msg2);
		g_free(msg2);
	} else
		notify_msg = g_strdup_printf(_("%s has (retroactively) denied your request to add them to your list."), who);

	purple_notify_info(gc, NULL, _("Add buddy rejected"), notify_msg,
		purple_request_cpar_from_connection(gc));
	g_free(notify_msg);

	g_hash_table_remove(yd->friends, who);
	purple_protocol_got_user_status(purple_connection_get_account(gc), who, "offline", NULL); /* FIXME: make this set not on list status instead */
	/* TODO: Shouldn't we remove the buddy from our local list? */
}

static void yahoo_buddy_auth_req_15(PurpleConnection *gc, struct yahoo_packet *pkt) {
	PurpleAccount *account;
	GSList *l = pkt->hash;
	const char *msg = NULL;

	account = purple_connection_get_account(gc);

	/* Buddy authorized/declined our addition */
	if (pkt->status == 1) {
		char *who = NULL;
		int response = 0;

		while (l) {
			struct yahoo_pair *pair = l->data;

			switch (pair->key) {
			case 4:
				break;
			case 13:
				response = strtol(pair->value, NULL, 10);
				break;
			case 14:
				msg = pair->value;
				break;
			}
			l = l->next;
		}

		if (response == 1) /* Authorized */
			purple_debug_info("yahoo", "Received authorization from buddy '%s'.\n", who ? who : "(Unknown Buddy)");
		else if (response == 2) { /* Declined */
			purple_debug_info("yahoo", "Received authorization decline from buddy '%s'.\n", who ? who : "(Unknown Buddy)");
			yahoo_buddy_denied_our_add(gc, who, msg);
		} else
			purple_debug_error("yahoo", "Received unknown authorization response of %d from buddy '%s'.\n", response, who ? who : "(Unknown Buddy)");
		g_free(who);
	}
	/* Buddy requested authorization to add us. */
	else if (pkt->status == 3) {
		struct yahoo_add_request *add_req;
		const char *firstname = NULL, *lastname = NULL;

		add_req = g_new0(struct yahoo_add_request, 1);
		add_req->gc = gc;

		while (l) {
			struct yahoo_pair *pair = l->data;

			switch (pair->key) {
			case 4:
				break;
			case 5:
				if (g_utf8_validate(pair->value, -1, NULL)) {
					add_req->id = g_strdup(pair->value);
				} else {
					purple_debug_warning("yahoo", "yahoo_buddy_auth_req_15 "
							"got non-UTF-8 string for key %d\n", pair->key);
				}
				break;
			case 14:
				msg = pair->value;
				break;
			case 216:
				if (g_utf8_validate(pair->value, -1, NULL)) {
					firstname = pair->value;
				} else {
					purple_debug_warning("yahoo", "yahoo_buddy_auth_req_15 "
							"got non-UTF-8 string for key %d\n", pair->key);
				}
				break;
			case 254:
				if (g_utf8_validate(pair->value, -1, NULL)) {
					lastname = pair->value;
				} else {
					purple_debug_warning("yahoo", "yahoo_buddy_auth_req_15 "
							"got non-UTF-8 string for key %d\n", pair->key);
				}
				break;

			}
			l = l->next;
		}

		if (add_req->id && add_req->who) {
			char *alias = NULL, *dec_msg = NULL;

			if (!purple_account_privacy_check(account, add_req->who))
			{
				purple_debug_misc("yahoo", "Auth. request from %s dropped and automatically denied due to privacy settings!\n",
						  add_req->who);
				yahoo_buddy_add_deny_cb(NULL, add_req);
				return;
			}

			if (msg)
				dec_msg = yahoo_string_decode(gc, msg, FALSE);

			if (firstname && lastname)
				alias = g_strdup_printf("%s %s", firstname, lastname);
			else if (firstname)
				alias = g_strdup(firstname);
			else if (lastname)
				alias = g_strdup(lastname);

			/* DONE! this is almost exactly the same as what MSN does,
			 * this should probably be moved to the core.
			 */
			 purple_account_request_authorization(account, add_req->who, add_req->id,
					alias, dec_msg,
					purple_blist_find_buddy(account, add_req->who) != NULL,
					yahoo_buddy_add_authorize_cb,
					yahoo_buddy_add_deny_cb,
					add_req);
			g_free(alias);
			g_free(dec_msg);
		} else {
			g_free(add_req->id);
			g_free(add_req->who);
			g_free(add_req);
		}
	} else {
		purple_debug_error("yahoo", "Received authorization of unknown status (%d).\n", pkt->status);
	}
}

/* I don't think this happens anymore in Version 15 */
static void yahoo_buddy_added_us(PurpleConnection *gc, struct yahoo_packet *pkt) {
	PurpleAccount *account;
	struct yahoo_add_request *add_req;
	char *msg = NULL;
	GSList *l = pkt->hash;

	account = purple_connection_get_account(gc);

	add_req = g_new0(struct yahoo_add_request, 1);
	add_req->gc = gc;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				add_req->id = g_strdup(pair->value);
			} else {
					purple_debug_warning("yahoo", "yahoo_buddy_added_us "
							"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 3:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				add_req->who = g_strdup(pair->value);
			} else {
					purple_debug_warning("yahoo", "yahoo_buddy_added_us "
							"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 15: /* time, for when they add us and we're offline */
			break;
		case 14:
			msg = pair->value;
			break;
		}
		l = l->next;
	}

	if (add_req->id && add_req->who) {
		char *dec_msg = NULL;

		if (!purple_account_privacy_check(account, add_req->who)) {
			purple_debug_misc("yahoo", "Auth. request from %s dropped and automatically denied due to privacy settings!\n",
					  add_req->who);
			yahoo_buddy_add_deny_cb(NULL, add_req);
			return;
		}

		if (msg)
			dec_msg = yahoo_string_decode(gc, msg, FALSE);

		/* DONE! this is almost exactly the same as what MSN does,
		 * this should probably be moved to the core.
		 */
		 purple_account_request_authorization(account, add_req->who, add_req->id,
				NULL, dec_msg,
				purple_blist_find_buddy(account,add_req->who) != NULL,
						yahoo_buddy_add_authorize_cb,
						yahoo_buddy_add_deny_cb, add_req);
		g_free(dec_msg);
	} else {
		g_free(add_req->id);
		g_free(add_req->who);
		g_free(add_req);
	}
}

/* I have no idea if this every gets called in version 15 */
static void yahoo_buddy_denied_our_add_old(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL;
	char *msg = NULL;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 3:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				who = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_buddy_denied_our_add_old "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 14:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				msg = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_buddy_denied_our_add_old "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		}
		l = l->next;
	}

	yahoo_buddy_denied_our_add(gc, who, msg);
}

static void yahoo_process_contact(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	switch (pkt->status) {
	case 1:
		yahoo_process_status(gc, pkt);
		return;
	case 3:
		yahoo_buddy_added_us(gc, pkt);
		break;
	case 7:
		yahoo_buddy_denied_our_add_old(gc, pkt);
		break;
	default:
		break;
	}
}

#define OUT_CHARSET "utf-8"

static char *yahoo_decode(const char *text)
{
	char *converted = NULL;
	char *n, *new;
	const char *end, *p;
	int i, k;

	n = new = g_malloc(strlen (text) + 1);
	end = text + strlen(text);

	for (p = text; p < end; p++, n++) {
		if (*p == '\\') {
			if (p[1] >= '0' && p[1] <= '7') {
				p += 1;
				for (i = 0, k = 0; k < 3; k += 1) {
					char c = p[k];
					if (c < '0' || c > '7') break;
					i *= 8;
					i += c - '0';
				}
				*n = i;
				p += k - 1;
			} else { /* bug 959248 */
				/* If we see a \ not followed by an octal number,
				 * it means that it is actually a \\ with one \
				 * already eaten by some unknown function.
				 * This is arguably broken.
				 *
				 * I think wing is wrong here, there is no function
				 * called that I see that could have done it. I guess
				 * it is just really sending single \'s. That's yahoo
				 * for you.
				 */
				*n = *p;
			}
		}
		else
			*n = *p;
	}

	*n = '\0';

	/* XXX: Is this related to Yahoo! Japan? If so, it should be removed. -mmcco */
	if (strstr(text, "\033$B"))
		converted = g_convert(new, n - new, OUT_CHARSET, "iso-2022-jp", NULL, NULL, NULL);
	if (!converted)
		converted = g_convert(new, n - new, OUT_CHARSET, "iso-8859-1", NULL, NULL, NULL);
	g_free(new);

	return converted;
}

static void yahoo_process_mail(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *who = NULL;
	const char *email = NULL;
	const char *subj = NULL;
	const char *yahoo_mail_url = YAHOO_MAIL_URL;
	int count = 0;
	GSList *l = pkt->hash;

	if (!purple_account_get_check_mail(account))
		return;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 9) {
			count = strtol(pair->value, NULL, 10);
		} else if (pair->key == 43) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				who = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_mail "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 42) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				email = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_mail "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 18) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				subj = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_mail "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		}
		l = l->next;
	}

	if (who && subj && email && *email) {
		char *dec_who = yahoo_decode(who);
		char *dec_subj = yahoo_decode(subj);
		char *from = g_strdup_printf("%s (%s)", dec_who, email);

		purple_notify_email(gc, dec_subj, from, purple_account_get_username(account),
						  yahoo_mail_url, NULL, NULL);

		g_free(dec_who);
		g_free(dec_subj);
		g_free(from);
	} else if (count > 0) {
		const char *tos[2] = { purple_account_get_username(account) };
		const char *urls[2] = { yahoo_mail_url };

		purple_notify_emails(gc, count, FALSE, NULL, NULL, tos, urls,
						   NULL, NULL);
	}
}

/* We use this structure once while we authenticate */
struct yahoo_auth_data
{
	PurpleConnection *gc;
	char *seed;
};

/* This is the y64 alphabet... it's like base64, but has a . and a _ */
static const char base64digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";

/* This is taken from Sylpheed by Hiroyuki Yamamoto. We have our own tobase64 function
 * in util.c, but it is different from the one yahoo uses */
static void to_y64(char *out, const unsigned char *in, gsize inlen)
     /* raw bytes in quasi-big-endian order to base 64 string (NUL-terminated) */
{
	for (; inlen >= 3; inlen -= 3)
		{
			*out++ = base64digits[in[0] >> 2];
			*out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
			*out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
			*out++ = base64digits[in[2] & 0x3f];
			in += 3;
		}
	if (inlen > 0)
		{
			unsigned char fragment;

			*out++ = base64digits[in[0] >> 2];
			fragment = (in[0] << 4) & 0x30;
			if (inlen > 1)
				fragment |= in[1] >> 4;
			*out++ = base64digits[fragment];
			*out++ = (inlen < 2) ? '-' : base64digits[(in[1] << 2) & 0x3c];
			*out++ = '-';
		}
	*out = '\0';
}

static void yahoo_auth16_stage3(PurpleConnection *gc, const char *crypt)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *name = purple_normalize(account, purple_account_get_username(account));
	PurpleHash *md5_hash;
	guchar md5_digest[16];
	gchar base64_string[25];
	struct yahoo_packet *pkt;

	purple_debug_info("yahoo","Authentication: In yahoo_auth16_stage3\n");

	g_return_if_fail(crypt != NULL);

	md5_hash = purple_md5_hash_new();
	purple_hash_append(md5_hash, (guchar *)crypt, strlen(crypt));
	purple_hash_digest(md5_hash, md5_digest, sizeof(md5_digest));

	to_y64(base64_string, md5_digest, 16);

	purple_debug_info("yahoo", "yahoo status: %d\n", yd->current_status);
	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP, yd->current_status, yd->session_id);

	if(yd->cookie_b) { /* send B cookie if we have it */
		yahoo_packet_hash(pkt, "ssssssssss",
					1, name,
					0, name,
					277, yd->cookie_y,
					278, yd->cookie_t,
					307, base64_string,
					244, YAHOO_CLIENT_VERSION_ID,
					2, name,
					2, "1",
					59, yd->cookie_b,
					98, purple_account_get_string(account, "room_list_locale", "us"),
					135, YAHOO_CLIENT_VERSION);
	} else { /* don't try to send an empty B cookie - the server will be mad */
		yahoo_packet_hash(pkt, "sssssssss",
					1, name,
					0, name,
					277, yd->cookie_y,
					278, yd->cookie_t,
					307, base64_string,
					244, YAHOO_CLIENT_VERSION_ID,
					2, name,
					2, "1",
					98, purple_account_get_string(account, "room_list_locale", "us"),
					135, YAHOO_CLIENT_VERSION);
	}

	if (yd->picture_checksum)
		yahoo_packet_hash_int(pkt, 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);

	g_object_unref(md5_hash);
}

static void yahoo_auth16_stage2(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _auth_data)
{
	struct yahoo_auth_data *auth_data = _auth_data;
	PurpleConnection *gc = auth_data->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	int i;
	gchar **splits;
	int response_no = -1;
	char *crumb = NULL;
	char *crypt = NULL;
	PurpleHttpCookieJar *cookiejar;

	purple_debug_info("yahoo","Authentication: In yahoo_auth16_stage2\n");

	if (!purple_http_response_is_successful(response)) {
		const gchar *error_message = purple_http_response_get_error(response);
		purple_debug_error("yahoo", "Login Failed, unable to retrieve stage 2 url: %s\n", error_message);
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_message);
		g_free(auth_data->seed);
		g_free(auth_data);
		return;
	}

	splits = g_strsplit(purple_http_response_get_data(response, NULL),
		"\r\n", -1);

	cookiejar = purple_http_conn_get_cookie_jar(http_conn);
	yd->cookie_b = purple_http_cookie_jar_get(cookiejar, "B");
	yd->cookie_t = purple_http_cookie_jar_get(cookiejar, "T");
	yd->cookie_y = purple_http_cookie_jar_get(cookiejar, "Y");

	i = 0;
	while (splits[i]) {
		/* I'm not exactly a fan of the magic numbers, but it's obvious,
		 * so no sense in wasting a bajillion vars or calls to strlen */

		if (i == 0 && g_ascii_isdigit(splits[i][0])) {
			response_no = strtol(splits[i], NULL, 10);
			purple_debug_info("yahoo", "Got auth16 stage 2 response code: %d\n",
				response_no);
		} else if (strncmp(splits[i], "crumb=", 6) == 0) {
			crumb = g_strdup(&splits[i][6]);

			if (purple_debug_is_unsafe())
				purple_debug_info("yahoo", "Got crumb: %s\n", crumb);
		}
		i++;
	}

	g_strfreev(splits);

	if (crumb == NULL)
		response_no = -1;

	if(response_no != 0) {
		/* Some error in the login process */
		PurpleConnectionError error;
		char *error_reason = NULL;

		switch (response_no) {
			case -1:
				/* Some error in the received stream */
				error_reason = g_strdup(_("Received invalid data"));
				error = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
				break;
			case 100:
				/* Unknown error */
				error_reason = g_strdup(_("Unknown error"));
				error = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
				break;
			default:
				/* if we have everything we need, why not try to login irrespective of response */
				if ((crumb != NULL) && (yd->cookie_y != NULL) && (yd->cookie_t != NULL)) {
#if 0
					try_login_on_error = TRUE;
#endif
					break;
				}
				error_reason = g_strdup(_("Unknown error"));
				error = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
				break;
		}
		if (error_reason) {
			purple_debug_error("yahoo", "Authentication error: %s. "
				"Code %d\n", error_reason, response_no);
			purple_connection_error(gc, error, error_reason);
			g_free(error_reason);
			g_free(crumb);
			g_free(auth_data->seed);
			g_free(auth_data);
			return;
		}
	}

	crypt = g_strconcat(crumb, auth_data->seed, NULL);
	yahoo_auth16_stage3(gc, crypt);
	g_free(crypt);
	g_free(crumb);
	g_free(auth_data->seed);
	g_free(auth_data);
}

static void yahoo_auth16_stage1_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _auth_data)
{
	struct yahoo_auth_data *auth_data = _auth_data;
	PurpleConnection *gc = auth_data->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	purple_debug_info("yahoo","Authentication: In yahoo_auth16_stage1_cb\n");

	if (!purple_http_response_is_successful(response)) {
		const gchar *error_message = purple_http_response_get_error(response);
		purple_debug_error("yahoo", "Login Failed, unable to retrieve login url: %s\n", error_message);
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_message);
		g_free(auth_data->seed);
		g_free(auth_data);
		return;
	} else {
		PurpleAccount *account = purple_connection_get_account(gc);
		gchar **split_data = g_strsplit(purple_http_response_get_data(
			response, NULL), "\r\n", -1);
		int totalelements = 0;
		int response_no = -1;
		char *token = NULL;

		totalelements = g_strv_length(split_data);

		if(totalelements == 1) { /* Received an error code */
			response_no = strtol(split_data[0], NULL, 10);
		} else if(totalelements == 2 || totalelements == 3 ) { /* received valid data */
			response_no = strtol(split_data[0], NULL, 10);
			token = g_strdup(split_data[1] + strlen("ymsgr="));
		} else { /* It looks like a transparent proxy has returned a document we don't want */
			response_no = -1;
		}

		g_strfreev(split_data);

		if(response_no != 0) {
			/* Some error in the login process */
			PurpleConnectionError error;
			char *error_reason;

			switch(response_no) {
				case -1:
					/* Some error in the received stream */
					error_reason = g_strdup(_("Received invalid data"));
					error = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
					break;
				case 1212:
					/* Password incorrect */
					/* Set password to NULL. Avoids account locking. Brings dialog to enter password if clicked on Re-enable account */
					if (!purple_account_get_remember_password(account))
						purple_account_set_password(account, NULL, NULL, NULL);
					error_reason = g_strdup(_("Incorrect password"));
					error = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
					break;
				case 1213:
					/* security lock from too many failed login attempts */
					error_reason = g_strdup(_("Account locked: Too many failed login "
								"attempts.  Logging into the Yahoo! website may fix this."));
					error = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
					break;
				case 1235:
					/* the username does not exist */
					error_reason = g_strdup(_("Username does not exist"));
					error = PURPLE_CONNECTION_ERROR_INVALID_USERNAME;
					break;
				case 1214:
					/* indicates a lock of some description */
					error_reason = g_strdup(_("Account locked: Unknown reason.  Logging "
								"into the Yahoo! website may fix this."));
					error = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
					break;
				case 1236:
					/* indicates a lock due to logging in too frequently */
					error_reason = g_strdup(_("Account locked: You have been logging in too "
								"frequently.  Wait a few minutes before trying to connect "
								"again.  Logging into the Yahoo! website may help."));
					error = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
					break;
				case 100:
					/* username or password missing */
					error_reason = g_strdup(_("Username or password missing"));
					error = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
					break;
				default:
					/* Unknown error! */
					error_reason = g_strdup_printf(_("Unknown error (%d)"), response_no);
					error = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
					break;
			}
			purple_debug_error("yahoo", "Authentication error: %s. Code %d\n",
			                   error_reason, response_no);
			purple_connection_error(gc, error, error_reason);
			g_free(error_reason);
			g_free(auth_data->seed);
			g_free(auth_data);
			g_free(token);
		}
		else {
			PurpleHttpRequest *req;

			req = purple_http_request_new(NULL);
			purple_http_request_set_url_printf(req, YAHOO_LOGIN_URL, token);
			purple_http_request_header_set(req, "User-Agent",
				YAHOO_CLIENT_USERAGENT);
			purple_http_connection_set_add(yd->http_reqs,
				purple_http_request(gc, req,
					yahoo_auth16_stage2, auth_data));
			purple_http_request_unref(req);

			g_free(token);
		}
	}
}

static void yahoo_auth16_stage1(PurpleConnection *gc, const char *seed)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleHttpRequest *req;
	struct yahoo_auth_data *auth_data = NULL;
	char *encoded_username;
	char *encoded_password;

	purple_debug_info("yahoo", "Authentication: In yahoo_auth16_stage1\n");

	auth_data = g_new0(struct yahoo_auth_data, 1);
	auth_data->gc = gc;
	auth_data->seed = g_strdup(seed);

	encoded_username = g_strdup(purple_url_encode(purple_account_get_username(purple_connection_get_account(gc))));
	encoded_password = g_strdup(purple_url_encode(purple_connection_get_password(gc)));

	req = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(req, YAHOO_TOKEN_URL,
		encoded_username, encoded_password, purple_url_encode(seed));
	purple_http_request_header_set(req, "User-Agent", YAHOO_CLIENT_USERAGENT);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_auth16_stage1_cb, auth_data));
	purple_http_request_unref(req);

	purple_str_wipe(encoded_password);
	g_free(encoded_username);
}

static void yahoo_process_auth(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *seed = NULL;
	GSList *l = pkt->hash;
	int m = 0;
	gchar *buf;

	while (l) {
		struct yahoo_pair *pair = l->data;
		/* (pair->key == 1) -> sn */
		if (pair->key == 94) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				seed = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_auth "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		} else if (pair->key == 13) {
			m = atoi(pair->value);
		}
		l = l->next;
	}

	if (seed) {
		switch (m) {
		case 0:
			/* used to be for really old auth routine, dont support now */
		case 1:
		case 2: /* Yahoo ver 16 authentication */
			yahoo_auth16_stage1(gc, seed);
			break;
		default:
			{
				GHashTable *ui_info = purple_core_get_ui_info();

				buf = g_strdup_printf(_("The Yahoo server has requested the use of an unrecognized "
							"authentication method.  You will probably not be able "
							"to successfully sign on to Yahoo.  Check %s for updates."),
							((ui_info && g_hash_table_lookup(ui_info, "website")) ? (char *)g_hash_table_lookup(ui_info, "website") : PURPLE_WEBSITE));
				purple_notify_error(gc, "",
					_("Failed Yahoo! Authentication"), buf,
					purple_request_cpar_from_connection(gc));
				g_free(buf);
				yahoo_auth16_stage1(gc, seed); /* Can't hurt to try it anyway. */
				break;
			}
		}
	}
}

static void ignore_buddy(PurpleBuddy *buddy) {
	PurpleGroup *group;
	PurpleAccount *account;
	gchar *name;

	if (!buddy)
		return;

	group = purple_buddy_get_group(buddy);
	name = g_strdup(purple_buddy_get_name(buddy));
	account = purple_buddy_get_account(buddy);

	purple_debug_info("yahoo", "blist: Removing '%s' from buddy list.\n", name);
	purple_account_remove_buddy(account, buddy, group);
	purple_blist_remove_buddy(buddy);

	purple_serv_add_deny(purple_account_get_connection(account), name);

	g_free(name);
}

static void keep_buddy(PurpleBuddy *b)
{
	purple_account_privacy_deny_remove(purple_buddy_get_account(b),
			purple_buddy_get_name(b), 1);
}

static void yahoo_process_ignore(PurpleConnection *gc, struct yahoo_packet *pkt) {
	PurpleBuddy *b;
	GSList *l;
	gchar *who = NULL;
	gchar buf[BUF_LONG];
	gboolean ignore = TRUE;
	gint status = 0;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;
		switch (pair->key) {
		case 0:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				who = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_ignore "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		/* 1 -> me */
		case 13:
			/* 1 == ignore, 2 == unignore */
			ignore = (strtol(pair->value, NULL, 10) == 1);
			break;
		case 66:
			status = strtol(pair->value, NULL, 10);
			break;
		default:
			break;
		}
	}

	/*
	 * status
	 * 0  - ok
	 * 2  - already in ignore list, could not add
	 * 3  - not in ignore list, could not delete
	 * 12 - is a buddy, could not add (and possibly also a not-in-ignore list condition?)
	 */
	switch (status) {
		case 12:
			purple_debug_info("yahoo", "Server reported \"is a buddy\" for %s while %s",
							  who, (ignore ? "ignoring" : "unignoring"));

			if (ignore) {
				b = purple_blist_find_buddy(purple_connection_get_account(gc), who);
				g_snprintf(buf, sizeof(buf), _("You have tried to ignore %s, but the "
											   "user is on your buddy list.  Clicking \"Yes\" "
											   "will remove and ignore the buddy."), who);
				purple_request_yes_no(gc, NULL,
					_("Ignore buddy?"), buf, 0,
					purple_request_cpar_from_connection(gc),
					b, G_CALLBACK(ignore_buddy),
					G_CALLBACK(keep_buddy));
				break;
			}
		case 2:
			purple_debug_info("yahoo", "Server reported that %s is already in the ignore list.\n",
							  who);
			break;
		case 3:
			purple_debug_info("yahoo", "Server reported that %s is not in the ignore list; could not delete\n",
							  who);
		case 0:
		default:
			break;
	}
}

#if TRY_WEBMESSENGER_LOGIN

static gboolean
yahoo_try_webmessenger_login(PurpleConnection *gc)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleHttpRequest *req;

	if (yd->wm)
		return FALSE;

	yd->wm = TRUE;
	if (yd->fd >= 0)
		close(yd->fd);
	if (yd->inpa) {
		purple_input_remove(yd->inpa);
		yd->inpa = 0;
	}

	req = purple_http_request_new(WEBMESSENGER_URL);
	purple_http_request_header_set(req, "User-Agent", "Purple/" VERSION);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_login_page_cb, NULL));
	purple_http_request_unref(req);

	return TRUE;
}

#endif /* TRY_WEBMESSENGER_LOGIN */

static void yahoo_process_authresp(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	int err = 0;
	char *msg;
	char *url = NULL;
	char *fullmsg;
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleConnectionError reason = PURPLE_CONNECTION_ERROR_OTHER_ERROR;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 66) {
			err = strtol(pair->value, NULL, 10);
		} else if (pair->key == 20) {
			if (g_utf8_validate(pair->value, -1, NULL)) {
				url = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_authresp "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
		}

		l = l->next;
	}

	switch (err) {
	case 0:
		msg = g_strdup(_("Unknown error"));
		reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
		break;
	case 3:
		msg = g_strdup(_("Username does not exist"));
		reason = PURPLE_CONNECTION_ERROR_INVALID_USERNAME;
		break;
	case 13:
#if TRY_WEBMESSENGER_LOGIN
		if (yahoo_try_webmessenger_login(gc))
			return;
#else
		purple_debug_info("yahoo", "Web messenger login is disabled\n");
#endif /* TRY_WEBMESSENGER_LOGIN */
		if (!purple_account_get_remember_password(account))
			purple_account_set_password(account, NULL, NULL, NULL);

		msg = g_strdup(_("Invalid username or password"));
		reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
		break;
	case 14:
		msg = g_strdup(_("Your account has been locked due to too many failed login attempts."
					"  Please try logging into the Yahoo! website."));
		reason = PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED;
		break;
	case 52:
		/* See #9660. As much as we know, reconnecting shouldn't hurt */
		purple_debug_info("yahoo", "Got error 52, Set to autoreconnect\n");
		msg = g_strdup(_("Unknown error 52.  Reconnecting should fix this."));
		reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
		break;
	case 1013:
		msg = g_strdup(_("Error 1013: The username you have entered is invalid."
					"  The most common cause of this error is entering your email"
					" address instead of your Yahoo! ID."));
		reason = PURPLE_CONNECTION_ERROR_INVALID_USERNAME;
		break;
	default:
		msg = g_strdup_printf(_("Unknown error number %d. Logging into the Yahoo! website may fix this."), err);
	}

	if (url)
		fullmsg = g_strdup_printf("%s\n%s", msg, url);
	else
		fullmsg = g_strdup(msg);

	purple_connection_error(gc, reason, fullmsg);
	g_free(msg);
	g_free(fullmsg);
}

static void yahoo_process_addbuddy(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	int err = 0;
	char *who = NULL;
	char *temp = NULL;
	char *group = NULL;
	char *decoded_group;
	char *buf;
	YahooFriend *f;
	GSList *l = pkt->hash;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 66:
			err = strtol(pair->value, NULL, 10);
			break;
		case 7:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				temp = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_addbuddy "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 65:
			group = pair->value;
			break;
		}

		l = l->next;
	}

	if (!temp)
		return;
	if (!group)
		group = "";

	who = g_strdup(temp);

	if (!err || (err == 2)) { /* 0 = ok, 2 = already on serv list */
		f = yahoo_friend_find_or_new(gc, who);
		yahoo_update_status(gc, who, f);

		if( !g_hash_table_lookup(yd->peers, who) ) {
			/* we are not connected as client, so set friend to not connected */
			yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_NOT_CONNECTED);
			f->p2p_packet_sent = 0;
		}
		else	/* we are already connected. set friend to YAHOO_P2PSTATUS_WE_ARE_CLIENT */
			yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_WE_ARE_CLIENT);
		g_free(who);
		return;
	}

	decoded_group = yahoo_string_decode(gc, group, FALSE);
	buf = g_strdup_printf(_("Unable to add buddy %s to group %s to the server list on account %s."),
				who, decoded_group, purple_connection_get_display_name(gc));
	if (!purple_conversation_present_error(who, purple_connection_get_account(gc), buf))
		purple_notify_error(gc, NULL, _("Unable to add buddy to server list"), buf,
			purple_request_cpar_from_connection(gc));
	g_free(buf);
	g_free(decoded_group);
	g_free(who);
}

/* write pkt to the source */
static void yahoo_p2p_write_pkt(gint source, struct yahoo_packet *pkt)
{
	size_t pkt_len;
	gssize written;
	guchar *raw_packet;

	/*build the raw packet and send it to the host*/
	pkt_len = yahoo_packet_build(pkt, 0, 0, &raw_packet);
	written = write(source, raw_packet, pkt_len);
	if (written < 0 || (gsize)written != pkt_len)
		purple_debug_warning("yahoo","p2p: couldn't write to the source\n");
	g_free(raw_packet);
}

static void yahoo_p2p_keepalive_cb(gpointer key, gpointer value, gpointer user_data)
{
	struct yahoo_p2p_data *p2p_data = value;
	PurpleConnection *gc = user_data;
	struct yahoo_packet *pkt_to_send;
	PurpleAccount *account;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	account = purple_connection_get_account(gc);

	pkt_to_send = yahoo_packet_new(YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt_to_send, "ssisi",
		4, purple_normalize(account, purple_account_get_username(account)),
		5, p2p_data->host_username,
		241, 0,		/* Protocol identifier */
		49, "PEERTOPEER",
		13, 7);
	yahoo_p2p_write_pkt(p2p_data->source, pkt_to_send);

	yahoo_packet_free(pkt_to_send);
}

static gboolean yahoo_p2p_keepalive(gpointer data)
{
	PurpleConnection *gc = data;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	g_hash_table_foreach(yd->peers, yahoo_p2p_keepalive_cb, gc);

	return TRUE;
}

/* destroy p2p_data associated with a peer and close p2p connection.
 * g_hash_table_remove() calls this function to destroy p2p_data associated with the peer,
 * call g_hash_table_remove() instead of this fucntion if peer has an entry in the table */
static void yahoo_p2p_disconnect_destroy_data(gpointer data)
{
	struct yahoo_p2p_data *p2p_data;
	YahooFriend *f;

	if(!(p2p_data = data))
		return ;

	/* If friend, set him not connected */
	f = yahoo_friend_find(p2p_data->gc, p2p_data->host_username);
	if (f)
		yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_NOT_CONNECTED);

	if(p2p_data->source >= 0)
		close(p2p_data->source);
	if (p2p_data->input_event > 0)
		purple_input_remove(p2p_data->input_event);
	g_free(p2p_data->host_ip);
	g_free(p2p_data->host_username);
	g_free(p2p_data);
}

/* exchange of initial p2pfilexfer packets, service type YAHOO_SERVICE_P2PFILEXFER */
static void yahoo_p2p_process_p2pfilexfer(gpointer data, gint source, struct yahoo_packet *pkt)
{
	struct yahoo_p2p_data *p2p_data;
	char *who = NULL;
	GSList *l = pkt->hash;
	struct yahoo_packet *pkt_to_send;
	PurpleAccount *account;
	int val_13_to_send = 0;
	YahooData *yd;
	YahooFriend *f;

	if(!(p2p_data = data))
		return ;

	yd = purple_connection_get_protocol_data(p2p_data->gc);

	/* lets see whats in the packet */
	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				who = pair->value;
				if(strncmp(who, p2p_data->host_username, strlen(p2p_data->host_username)) != 0) {
					/* from whom are we receiving the packets ?? */
					purple_debug_warning("yahoo","p2p: received data from wrong user\n");
					return;
				}
			} else {
				purple_debug_warning("yahoo", "yahoo_p2p_process_p2pfilexfer "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 13:
			p2p_data->val_13 = strtol(pair->value, NULL, 10);	/* Value should be 5-7 */
			break;
		/* case 5, 49 look laters, no use right now */
		}
		l = l->next;
	}

	account = purple_connection_get_account(p2p_data->gc);

	/* key_13: sort of a counter.
	 * WHEN WE ARE CLIENT: yahoo server sends val_13 = 0, we send to peer val_13 = 1, receive back val_13 = 5,
	 * we send val_13=6, receive val_13=7, we send val_13=7, HALT. Keep sending val_13 = 7 as keep alive.
	 * WHEN WE ARE SERVER: we send val_13 = 0 to yahoo server, peer sends us val_13 = 1, we send val_13 = 5,
	 * receive val_13 = 6, send val_13 = 7, receive val_13 = 7. HALT. Keep sending val_13 = 7 as keep alive. */

	switch(p2p_data->val_13) {
		case 1 : val_13_to_send = 5; break;
		case 5 : val_13_to_send = 6; break;
		case 6 : val_13_to_send = 7; break;
		case 7 : if( g_hash_table_lookup(yd->peers, p2p_data->host_username) )
				return;
			 val_13_to_send = 7; break;
		default: purple_debug_warning("yahoo","p2p:Unknown value for key 13\n");
			 return;
		}

	/* Build the yahoo packet */
	pkt_to_send = yahoo_packet_new(YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt_to_send, "ssisi",
		4, purple_normalize(account, purple_account_get_username(account)),
		5, p2p_data->host_username,
		241, 0,		/* Protocol identifier */
		49, "PEERTOPEER",
		13, val_13_to_send);

	/* build the raw packet and send it to the host */
	yahoo_p2p_write_pkt(source, pkt_to_send);
	yahoo_packet_free(pkt_to_send);

	if( val_13_to_send == 7 )
		if( !g_hash_table_lookup(yd->peers, p2p_data->host_username) ) {
			g_hash_table_insert(yd->peers, g_strdup(p2p_data->host_username), p2p_data);
			/* If the peer is a friend, set him connected */
			f = yahoo_friend_find(p2p_data->gc, p2p_data->host_username);
			if (f) {
				if(p2p_data->connection_type == YAHOO_P2P_WE_ARE_SERVER) {
					p2p_data->session_id = f->session_id;
					yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_WE_ARE_SERVER);
				}
				else
					yahoo_friend_set_p2p_status(f, YAHOO_P2PSTATUS_WE_ARE_CLIENT);
			}
		}
}

/* callback function associated with receiving of data, not considering receipt of multiple YMSG packets in a single TCP packet */
static void yahoo_p2p_read_pkt_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	guchar buf[1024];	/* is it safe to assume a fixed array length of 1024 ?? */
	int len;
	int pos = 0;
	int pktlen;
	struct yahoo_packet *pkt;
	guchar *start;
	struct yahoo_p2p_data *p2p_data;
	YahooData *yd;

	if(!(p2p_data = data))
		return ;
	yd = purple_connection_get_protocol_data(p2p_data->gc);

	len = read(source, buf, sizeof(buf));
	if ((len < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
		return ; /* No Worries*/
	else if (len <= 0)
	{
		purple_debug_warning("yahoo","p2p: Error in connection, or host disconnected\n");
		/* remove from p2p connection lists, also calls yahoo_p2p_disconnect_destroy_data */
		if( g_hash_table_lookup(yd->peers, p2p_data->host_username) )
			g_hash_table_remove(yd->peers,p2p_data->host_username);
		else
			yahoo_p2p_disconnect_destroy_data(data);
		return;
	}

	/* TODO: It looks like there's a bug here (and above) where an incorrect
	 * assumtion is being made that the buffer will be added to when this
	 * is next called, but that's not really the case! */
	if(len < YAHOO_PACKET_HDRLEN)
		return;

	if(strncmp((char *)buf, "YMSG", 4) != 0) {
		/* Not a YMSG packet */
		purple_debug_warning("yahoo", "p2p: Got something other than YMSG packet\n");

		start = (guchar *) g_strstr_len((char *) buf + 1, len - 1 ,"YMSG");
		if (start == NULL) {
			/* remove from p2p connection lists, also calls yahoo_p2p_disconnect_destroy_data */
			if (g_hash_table_lookup(yd->peers, p2p_data->host_username))
				g_hash_table_remove(yd->peers, p2p_data->host_username);
			else
				yahoo_p2p_disconnect_destroy_data(data);
			return;
		}
		purple_debug_warning("yahoo","p2p: Got something other than YMSG packet\n");

		len -= (start - buf);
		g_memmove(buf, start, len);
	}

	pos += 4;	/* YMSG */
	pos += 2;
	pos += 2;

	pktlen = yahoo_get16(buf + pos); pos += 2;
	if (len < (YAHOO_PACKET_HDRLEN + pktlen)) {
		purple_debug_error("yahoo", "p2p: packet length(%d) > buffer length(%d)\n",
				pktlen, (len - pos)); 
		/* remove from p2p connection lists, also calls yahoo_p2p_disconnect_destroy_data */
		if (g_hash_table_lookup(yd->peers, p2p_data->host_username))
			g_hash_table_remove(yd->peers, p2p_data->host_username);
		else
			yahoo_p2p_disconnect_destroy_data(data);
		return;
	} else
		purple_debug_misc("yahoo", "p2p: %d bytes to read\n", pktlen);

	pkt = yahoo_packet_new(0, 0, 0);
	pkt->service = yahoo_get16(buf + pos); pos += 2;
	pkt->status = yahoo_get32(buf + pos); pos += 4;
	pkt->id = yahoo_get32(buf + pos); pos += 4;

	purple_debug_misc("yahoo", "p2p: Yahoo Service: 0x%02x Status: %d\n",pkt->service, pkt->status);
	yahoo_packet_read(pkt, buf + pos, pktlen);

	/* packet processing */
	switch(pkt->service) {
		case YAHOO_SERVICE_P2PFILEXFER:
			yahoo_p2p_process_p2pfilexfer(data, source, pkt);
			break;
		case YAHOO_SERVICE_MESSAGE:
			yahoo_process_message(p2p_data->gc, pkt, YAHOO_PKT_TYPE_P2P);
			break;
		case YAHOO_SERVICE_NOTIFY:
			yahoo_process_notify(p2p_data->gc, pkt, YAHOO_PKT_TYPE_P2P);
			break;
		default:
			purple_debug_warning("yahoo","p2p: p2p service %d Unhandled\n",pkt->service);
	}

	yahoo_packet_free(pkt);
}

static void yahoo_p2p_server_send_connected_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	int acceptfd;
	struct yahoo_p2p_data *p2p_data;
	YahooData *yd;

	if(!(p2p_data = data))
		return ;
	yd = purple_connection_get_protocol_data(p2p_data->gc);

	acceptfd = accept(source, NULL, 0);
	if(acceptfd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	else if(acceptfd == -1) {
		purple_debug_warning("yahoo","yahoo_p2p_server_send_connected_cb: accept: %s\n", g_strerror(errno));
		yahoo_p2p_disconnect_destroy_data(data);
		return;
	}

	/* remove timeout */
	if (yd->yahoo_p2p_server_timeout_handle) {
		purple_timeout_remove(yd->yahoo_p2p_server_timeout_handle);
		yd->yahoo_p2p_server_timeout_handle = 0;
	}

	/* remove watcher and close p2p server */
	if (yd->yahoo_p2p_server_watcher) {
		purple_input_remove(yd->yahoo_p2p_server_watcher);
		yd->yahoo_p2p_server_watcher = 0;
	}
	if (yd->yahoo_local_p2p_server_fd >= 0) {
		close(yd->yahoo_local_p2p_server_fd);
		yd->yahoo_local_p2p_server_fd = -1;
	}

	/* Add an Input Read event to the file descriptor */
	p2p_data->input_event = purple_input_add(acceptfd, PURPLE_INPUT_READ, yahoo_p2p_read_pkt_cb, data);
	p2p_data->source = acceptfd;
}

static gboolean yahoo_cancel_p2p_server_listen_cb(gpointer data)
{
	struct yahoo_p2p_data *p2p_data;
	YahooData *yd;

	if(!(p2p_data = data))
		return FALSE;

	yd = purple_connection_get_protocol_data(p2p_data->gc);

	purple_debug_warning("yahoo","yahoo p2p server timeout, peer failed to connect\n");
	yahoo_p2p_disconnect_destroy_data(data);
	purple_input_remove(yd->yahoo_p2p_server_watcher);
	yd->yahoo_p2p_server_watcher = 0;
	close(yd->yahoo_local_p2p_server_fd);
	yd->yahoo_local_p2p_server_fd = -1;
	yd->yahoo_p2p_server_timeout_handle = 0;

	return FALSE;
}

static void yahoo_p2p_server_listen_cb(int listenfd, gpointer data)
{
	struct yahoo_p2p_data *p2p_data;
	YahooData *yd;

	if(!(p2p_data = data))
		return ;

	yd = purple_connection_get_protocol_data(p2p_data->gc);
	yd->listen_data = NULL;

	if(listenfd == -1) {
		purple_debug_warning("yahoo","p2p: error starting p2p server\n");
		yahoo_p2p_disconnect_destroy_data(data);
		return;
	}

	/* Add an Input Read event to the file descriptor */
	yd->yahoo_local_p2p_server_fd = listenfd;
	yd->yahoo_p2p_server_watcher = purple_input_add(listenfd, PURPLE_INPUT_READ, yahoo_p2p_server_send_connected_cb,data);

	/* add timeout */
	yd->yahoo_p2p_server_timeout_handle = purple_timeout_add_seconds(YAHOO_P2P_SERVER_TIMEOUT, yahoo_cancel_p2p_server_listen_cb, data);
}

/* send p2p pkt containing our encoded ip, asking peer to connect to us */
void yahoo_send_p2p_pkt(PurpleConnection *gc, const char *who, int val_13)
{
	const char *public_ip;
	guint32 temp[4];
	guint32 ip;
	char temp_str[100];
	gchar *base64_ip = NULL;
	YahooFriend *f;
	struct yahoo_packet *pkt;
	PurpleAccount *account;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_p2p_data *p2p_data;
	const char *norm_username;

	f = yahoo_friend_find(gc, who);
	account = purple_connection_get_account(gc);

	/* Do not send invitation if already listening for other connection */
	if(yd->yahoo_local_p2p_server_fd >= 0)
		return;

	/* One shouldn't try to connect to self */
	if( strcmp(purple_normalize(account, purple_account_get_username(account)), who) == 0)
		return;

	/* send packet to only those friends who arent p2p connected and to whom we havent already sent. Do not send if this condition doesn't hold good */
	if( !( f && (yahoo_friend_get_p2p_status(f) == YAHOO_P2PSTATUS_NOT_CONNECTED) && (f->p2p_packet_sent == 0)) )
		return;

	/* Finally, don't try to connect to buddies not online or on sms */
	if( (f->status == YAHOO_STATUS_OFFLINE) || f->sms )
		return;

	public_ip = purple_network_get_public_ip();
	if( (sscanf(public_ip, "%u.%u.%u.%u", &temp[0], &temp[1], &temp[2], &temp[3])) !=4 )
		return ;

	ip = (temp[3] << 24) | (temp[2] <<16) | (temp[1] << 8) | temp[0];
	sprintf(temp_str, "%d", ip);
	base64_ip = purple_base64_encode( (guchar *)temp_str, strlen(temp_str) );

	norm_username = purple_normalize(account, purple_account_get_username(account));
	pkt = yahoo_packet_new(YAHOO_SERVICE_PEERTOPEER, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sssissis",
		1, norm_username,
		4, norm_username,
		12, base64_ip,	/* base64 encode ip */
		61, 0,		/* To-do : figure out what is 61 for?? */
		2, "",
		5, who,
		13, val_13,
		49, "PEERTOPEER");
	yahoo_packet_send_and_free(pkt, yd);

	f->p2p_packet_sent = 1;	/* set p2p_packet_sent to sent */

	p2p_data = g_new0(struct yahoo_p2p_data, 1);

	p2p_data->gc = gc;
	p2p_data->host_ip = NULL;
	p2p_data->host_username = g_strdup(who);
	p2p_data->val_13 = val_13;
	p2p_data->connection_type = YAHOO_P2P_WE_ARE_SERVER;
	p2p_data->source = -1;

	/* FIXME: If the port is already used, purple_network_listener returns NULL and old listener won't be canceled
	 * in yahoo_close function. */
	if (yd->listen_data)
		purple_debug_warning("yahoo","p2p: Failed to create p2p server - server already exists\n");
	else {
		yd->listen_data = purple_network_listen(YAHOO_PAGER_PORT_P2P, AF_UNSPEC, SOCK_STREAM, TRUE, yahoo_p2p_server_listen_cb, p2p_data);
		if (yd->listen_data == NULL)
			purple_debug_warning("yahoo","p2p: Failed to created p2p server\n");
	}

	g_free(base64_ip);
}

/* function called when connection to p2p host is setup */
static void yahoo_p2p_init_cb(gpointer data, gint source, const gchar *error_message)
{
	struct yahoo_p2p_data *p2p_data;
	struct yahoo_packet *pkt_to_send;
	PurpleAccount *account;
	YahooData *yd;

	p2p_data = data;
	yd = purple_connection_get_protocol_data(p2p_data->gc);

	if(error_message != NULL) {
		purple_debug_warning("yahoo","p2p: %s\n",error_message);
		yahoo_send_p2p_pkt(p2p_data->gc, p2p_data->host_username, 2);/* send p2p init packet with val_13=2 */

		yahoo_p2p_disconnect_destroy_data(p2p_data);
		return;
	}

	/* Add an Input Read event to the file descriptor */
	p2p_data->input_event = purple_input_add(source, PURPLE_INPUT_READ, yahoo_p2p_read_pkt_cb, data);
	p2p_data->source = source;

	account = purple_connection_get_account(p2p_data->gc);

	/* Build the yahoo packet */
	pkt_to_send = yahoo_packet_new(YAHOO_SERVICE_P2PFILEXFER, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt_to_send, "ssisi",
		4, purple_normalize(account, purple_account_get_username(account)),
		5, p2p_data->host_username,
		241, 0,		/* Protocol identifier */
		49, "PEERTOPEER",
		13, 1);		/* we receive key13= 0 or 2, we send key13=1 */

	yahoo_p2p_write_pkt(source, pkt_to_send);	/* build raw packet and send */
	yahoo_packet_free(pkt_to_send);
}

static void yahoo_process_p2p(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *who = NULL;
	char *base64 = NULL;
	guchar *decoded;
	gsize len;
	gint val_13 = 0;
	gint val_11 = 0;
	PurpleAccount *account;
	YahooFriend *f;

	/* if status is not YAHOO_STATUS_BRB or YAHOO_STATUS_P2P, the packet bounced back,
	 * so it contains our own ip */
	if(pkt->status != YAHOO_STATUS_BRB && pkt->status != YAHOO_STATUS_P2P)
		return ;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 5:
			/* our identity */
			break;
		case 4:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				who = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_p2p "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 1:
			/* who again, the master identity this time? */
			break;
		case 12:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				base64 = pair->value;
				/* so, this is an ip address. in base64. decoded it's in ascii.
				   after strtol, it's in reversed byte order. Who thought this up?*/
			} else {
				purple_debug_warning("yahoo", "yahoo_process_p2p "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 13:
			val_13 = strtol(pair->value, NULL, 10);
			break;
		case 11:
			val_11 = strtol(pair->value, NULL, 10);		/* session id of peer */
			if( (f = yahoo_friend_find(gc, who)) )
				f->session_id = val_11;
			break;
		/*
			TODO: figure these out
			yahoo: Key: 61          Value: 0
			yahoo: Key: 2   Value:
			yahoo: Key: 13          Value: 0	packet count ??
			yahoo: Key: 49          Value: PEERTOPEER
			yahoo: Key: 140         Value: 1
		*/

		}

		l = l->next;
	}

	if (base64) {
		guint32 ip;
		YahooFriend *f;
		char *host_ip, *tmp;
		struct yahoo_p2p_data *p2p_data;

		decoded = purple_base64_decode(base64, &len);
		if (decoded == NULL) {
			purple_debug_info("yahoo","p2p: Unable to decode base64 IP (%s) \n", base64);
			return;
		}
		tmp = purple_str_binary_to_ascii(decoded, len);
		purple_debug_info("yahoo", "Got P2P service packet (from server): who = %s, ip = %s\n", who, tmp);
		g_free(tmp);

		ip = strtol((gchar *)decoded, NULL, 10);
		g_free(decoded);
		host_ip = g_strdup_printf("%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff,
		                       (ip >> 24) & 0xff);
		f = yahoo_friend_find(gc, who);
		if (f)
			yahoo_friend_set_ip(f, host_ip);
		purple_debug_info("yahoo", "IP : %s\n", host_ip);

		account = purple_connection_get_account(gc);

		if(val_11==0) {
			if(!f)
				return;
			else
				val_11 = f->session_id;
		}

		p2p_data = g_new0(struct yahoo_p2p_data, 1);
		p2p_data->host_username = g_strdup(who);
		p2p_data->val_13 = val_13;
		p2p_data->session_id = val_11;
		p2p_data->host_ip = host_ip;
		p2p_data->gc = gc;
		p2p_data->connection_type = YAHOO_P2P_WE_ARE_CLIENT;
		p2p_data->source = -1;

		/* connect to host */
		if((purple_proxy_connect(gc, account, host_ip, YAHOO_PAGER_PORT_P2P, yahoo_p2p_init_cb, p2p_data))==NULL) {
			purple_debug_info("yahoo","p2p: Connection to %s failed\n", host_ip);
			g_free(p2p_data->host_ip);
			g_free(p2p_data->host_username);
			g_free(p2p_data);
		}
	}
}

static void yahoo_process_audible(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account;
	char *who = NULL, *msg = NULL, *id = NULL;
	GSList *l = pkt->hash;

	account = purple_connection_get_account(gc);

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			if (g_utf8_validate(pair->value, -1, NULL)) {
				who = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_audible "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 5:
			/* us */
			break;
		case 230:
			/* the audible, in foo.locale.bar.baz format
			   eg: base.tw.smiley.smiley43 */
			if (g_utf8_validate(pair->value, -1, NULL)) {
				id = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_audible "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 231:
			/* the text of the audible */
			if (g_utf8_validate(pair->value, -1, NULL)) {
				msg = pair->value;
			} else {
				purple_debug_warning("yahoo", "yahoo_process_audible "
						"got non-UTF-8 string for key %d\n", pair->key);
			}
			break;
		case 232:
			/* SHA-1 hash of audible SWF file (eg: 4e8691499d9c0fb8374478ff9720f4a9ea4a4915) */
			break;
		}

		l = l->next;
	}

	if (!msg)
		msg = id;
	if (!who || !msg)
		return;
	if (!g_utf8_validate(msg, -1, NULL)) {
		purple_debug_misc("yahoo", "Warning, nonutf8 audible, ignoring!\n");
		return;
	}
	if (!purple_account_privacy_check(account, who)) {
		purple_debug_misc("yahoo", "Audible message from %s for %s dropped!\n",
				purple_account_get_username(account), who);
		return;
	}
	if (id) {
		/* "http://l.yimg.com/pu/dl/aud/"+locale+"/"+id+".swf" */
		char **audible_locale = g_strsplit(id, ".", 0);
		char *buf = g_strdup_printf(_("[ Audible %s/%s/%s.swf ] %s"), YAHOO_AUDIBLE_URL, audible_locale[1], id, msg);
		g_strfreev(audible_locale);

		purple_serv_got_im(gc, who, buf, 0, time(NULL));
		g_free(buf);
	} else
		purple_serv_got_im(gc, who, msg, 0, time(NULL));
}

static void yahoo_packet_process(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	switch (pkt->service) {
	case YAHOO_SERVICE_LOGON:
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
	case YAHOO_SERVICE_GAMELOGON:
	case YAHOO_SERVICE_GAMELOGOFF:
	case YAHOO_SERVICE_CHATLOGON:
	case YAHOO_SERVICE_CHATLOGOFF:
	case YAHOO_SERVICE_Y6_STATUS_UPDATE:
	case YAHOO_SERVICE_STATUS_15:
		yahoo_process_status(gc, pkt);
		break;
	case YAHOO_SERVICE_NOTIFY:
		yahoo_process_notify(gc, pkt, YAHOO_PKT_TYPE_SERVER);
		break;
	case YAHOO_SERVICE_MESSAGE:
	case YAHOO_SERVICE_GAMEMSG:
	case YAHOO_SERVICE_CHATMSG:
		yahoo_process_message(gc, pkt, YAHOO_PKT_TYPE_SERVER);
		break;
	case YAHOO_SERVICE_SYSMESSAGE:
		yahoo_process_sysmessage(gc, pkt);
			break;
	case YAHOO_SERVICE_NEWMAIL:
		yahoo_process_mail(gc, pkt);
		break;
	case YAHOO_SERVICE_NEWCONTACT:
		yahoo_process_contact(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTHRESP:
		yahoo_process_authresp(gc, pkt);
		break;
	case YAHOO_SERVICE_LIST:
		yahoo_process_list(gc, pkt);
		break;
	case YAHOO_SERVICE_LIST_15:
		yahoo_process_list_15(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTH:
		yahoo_process_auth(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTH_REQ_15:
		yahoo_buddy_auth_req_15(gc, pkt);
		break;
	case YAHOO_SERVICE_ADDBUDDY:
		yahoo_process_addbuddy(gc, pkt);
		break;
	case YAHOO_SERVICE_IGNORECONTACT:
		yahoo_process_ignore(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFINVITE:
	case YAHOO_SERVICE_CONFADDINVITE:
		yahoo_process_conference_invite(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFDECLINE:
		yahoo_process_conference_decline(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFLOGON:
		yahoo_process_conference_logon(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFLOGOFF:
		yahoo_process_conference_logoff(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFMSG:
		yahoo_process_conference_message(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATONLINE:
		yahoo_process_chat_online(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATLOGOUT:
		yahoo_process_chat_logout(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATGOTO:
		yahoo_process_chat_goto(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATJOIN:
		yahoo_process_chat_join(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATLEAVE: /* XXX is this right? */
	case YAHOO_SERVICE_CHATEXIT:
		yahoo_process_chat_exit(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATINVITE: /* XXX never seen this one, might not do it right */
	case YAHOO_SERVICE_CHATADDINVITE:
		yahoo_process_chat_addinvite(gc, pkt);
		break;
	case YAHOO_SERVICE_COMMENT:
		yahoo_process_chat_message(gc, pkt);
		break;
	case YAHOO_SERVICE_PRESENCE_PERM:
	case YAHOO_SERVICE_PRESENCE_SESSION:
		yahoo_process_presence(gc, pkt);
		break;
	case YAHOO_SERVICE_P2PFILEXFER:
		/* This case had no break and continued; thus keeping it this way.*/
		yahoo_process_p2p(gc, pkt);	/* P2PFILEXFER handled the same way as process_p2p */
		yahoo_process_p2pfilexfer(gc, pkt);	/* redundant ??, need to have a break now */
	case YAHOO_SERVICE_FILETRANSFER:
		purple_debug_error("yahoo", "Legacy file transfers are not "
			"supported anymore.\n");
		break;
	case YAHOO_SERVICE_PEERTOPEER:
		yahoo_process_p2p(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE:
		yahoo_process_picture(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_CHECKSUM:
		yahoo_process_picture_checksum(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_UPLOAD:
		yahoo_process_picture_upload(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_UPDATE:
	case YAHOO_SERVICE_AVATAR_UPDATE:
		yahoo_process_avatar_update(gc, pkt);
		break;
	case YAHOO_SERVICE_AUDIBLE:
		yahoo_process_audible(gc, pkt);
		break;
	case YAHOO_SERVICE_CONTACT_DETAILS:
		yahoo_process_contact_details(gc, pkt);
		break;
	case YAHOO_SERVICE_FILETRANS_15:
		yahoo_process_filetrans_15(gc, pkt);
		break;
	case YAHOO_SERVICE_FILETRANS_INFO_15:
		yahoo_process_filetrans_info_15(gc, pkt);
		break;
	case YAHOO_SERVICE_FILETRANS_ACC_15:
		yahoo_process_filetrans_acc_15(gc, pkt);
		break;
	case YAHOO_SERVICE_SMS_MSG:
		yahoo_process_sms_message(gc, pkt);
		break;

	default:
		purple_debug_error("yahoo", "Unhandled service 0x%02x\n", pkt->service);
		break;
	}
}

static void yahoo_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc = data;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	char buf[1024];
	int len;

	len = read(yd->fd, buf, sizeof(buf));

	if (len < 0) {
		gchar *tmp;

		if (errno == EAGAIN)
			/* No worries */
			return;

		tmp = g_strdup_printf(_("Lost connection with server: %s"),
				g_strerror(errno));
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	} else if (len == 0) {
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Server closed the connection"));
		return;
	}
	purple_connection_update_last_received(gc);
	yd->rxqueue = g_realloc(yd->rxqueue, len + yd->rxlen);
	memcpy(yd->rxqueue + yd->rxlen, buf, len);
	yd->rxlen += len;

	while (1) {
		struct yahoo_packet *pkt;
		int pos = 0;
		int pktlen;

		if (yd->rxlen < YAHOO_PACKET_HDRLEN)
			return;

		if (strncmp((char *)yd->rxqueue, "YMSG", MIN(4, yd->rxlen)) != 0) {
			/* HEY! This isn't even a YMSG packet. What
			 * are you trying to pull? */
			guchar *start;

			purple_debug_warning("yahoo", "Error in YMSG stream, got something not a YMSG packet!\n");

			start = memchr(yd->rxqueue + 1, 'Y', yd->rxlen - 1);
			if (start) {
				g_memmove(yd->rxqueue, start, yd->rxlen - (start - yd->rxqueue));
				yd->rxlen -= start - yd->rxqueue;
				continue;
			} else {
				g_free(yd->rxqueue);
				yd->rxqueue = NULL;
				yd->rxlen = 0;
				return;
			}
		}

		pos += 4; /* YMSG */
		pos += 2;
		pos += 2;

		pktlen = yahoo_get16(yd->rxqueue + pos); pos += 2;
		purple_debug_misc("yahoo", "%d bytes to read, rxlen is %d\n", pktlen, yd->rxlen);

		if (yd->rxlen < (YAHOO_PACKET_HDRLEN + pktlen))
			return;

		yahoo_packet_dump(yd->rxqueue, YAHOO_PACKET_HDRLEN + pktlen);

		pkt = yahoo_packet_new(0, 0, 0);

		pkt->service = yahoo_get16(yd->rxqueue + pos); pos += 2;
		pkt->status = yahoo_get32(yd->rxqueue + pos); pos += 4;
		purple_debug_misc("yahoo", "Yahoo Service: 0x%02x Status: %d\n",
				   pkt->service, pkt->status);
		pkt->id = yahoo_get32(yd->rxqueue + pos); pos += 4;

		yahoo_packet_read(pkt, yd->rxqueue + pos, pktlen);

		yd->rxlen -= YAHOO_PACKET_HDRLEN + pktlen;
		if (yd->rxlen) {
			guchar *tmp = g_memdup(yd->rxqueue + YAHOO_PACKET_HDRLEN + pktlen, yd->rxlen);
			g_free(yd->rxqueue);
			yd->rxqueue = tmp;
		} else {
			g_free(yd->rxqueue);
			yd->rxqueue = NULL;
		}

		yahoo_packet_process(gc, pkt);

		yahoo_packet_free(pkt);
	}
}

static void yahoo_got_connected(gpointer data, gint source, const gchar *error_message)
{
	PurpleConnection *gc = data;
	YahooData *yd;
	struct yahoo_packet *pkt;

	if (source < 0) {
		gchar *tmp;
		tmp = g_strdup_printf(_("Unable to connect: %s"), error_message);
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	yd = purple_connection_get_protocol_data(gc);
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH, yd->current_status, yd->session_id);

	yahoo_packet_hash_str(pkt, 1, purple_normalize(purple_connection_get_account(gc), purple_account_get_username(purple_connection_get_account(gc))));
	yahoo_packet_send_and_free(pkt, yd);

	yd->inpa = purple_input_add(yd->fd, PURPLE_INPUT_READ, yahoo_pending, gc);
}

#if TRY_WEBMESSENGER_LOGIN

static void yahoo_got_web_connected(gpointer data, gint source, const gchar *error_message)
{
	PurpleConnection *gc = data;
	YahooData *yd;
	struct yahoo_packet *pkt;

	if (source < 0) {
		gchar *tmp;
		tmp = g_strdup_printf(_("Unable to connect: %s"), error_message);
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	yd = purple_connection_get_protocol_data(gc);
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_WEBLOGIN, YAHOO_STATUS_WEBLOGIN, yd->session_id);

	yahoo_packet_hash(pkt, "sss", 0,
	                  purple_normalize(purple_connection_get_account(gc), purple_account_get_username(purple_connection_get_account(gc))),
	                  1, purple_normalize(purple_connection_get_account(gc), purple_account_get_username(purple_connection_get_account(gc))),
	                  6, yd->auth);
	yahoo_packet_send_and_free(pkt, yd);

	g_free(yd->auth);
	yd->inpa = purple_input_add(yd->fd, PURPLE_INPUT_READ, yahoo_pending, gc);
}

static void
yahoo_login_page_got(PurpleHttpConnection *hc, PurpleHttpResponse *resp,
	gpointer _unused)
{
	PurpleConnection *gc = purple_http_conn_get_purple_connection(hc);
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleHttpCookieJar *cjar;
	GString *auth_s;
	gchar *cookie;

	if (purple_http_response_get_code(resp) != 302) {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to connect"));
		return;
	}

	auth_s = g_string_new(NULL);
	cjar = purple_http_conn_get_cookie_jar(hc);
	cookie = purple_http_cookie_jar_get(cjar, "B");
	if (cookie) {
		g_string_append_printf(auth_s, "B=%s; ", cookie);
		g_free(cookie);
	}
	cookie = purple_http_cookie_jar_get(cjar, "T");
	if (cookie) {
		g_string_append_printf(auth_s, "T=%s; ", cookie);
		g_free(cookie);
	}
	cookie = purple_http_cookie_jar_get(cjar, "Y");
	if (cookie) {
		g_string_append_printf(auth_s, "Y=%s; ", cookie);
		g_free(cookie);
	}

	yd->auth = g_string_free(auth_s, FALSE);
	/* Now we have our cookies to login with.  I'll go get the milk. */

	/* XXX: wcs2.msg.dcn.yahoo.com is down, so I used
	 * YAHOO_PAGER_HOST_FALLBACK, but I'm not sure, if it is the correct
	 * host.
	 */
	if (purple_proxy_connect(gc, account, YAHOO_PAGER_HOST_FALLBACK,
		purple_account_get_int(account, "port", YAHOO_PAGER_PORT),
		yahoo_got_web_connected, gc) == NULL)
	{
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to connect"));
		return;
	}
}

static void yahoo_login_page_hash_iter(const char *key, const char *val, GString *url)
{
	if (!strcmp(key, "passwd") || !strcmp(key, "login"))
		return;
	g_string_append_c(url, '&');
	g_string_append(url, key);
	g_string_append_c(url, '=');
	if (!strcmp(key, ".save") || !strcmp(key, ".js"))
		g_string_append_c(url, '1');
	else if (!strcmp(key, ".challenge"))
		g_string_append(url, val);
	else
		g_string_append(url, purple_url_encode(val));
}

static GHashTable *yahoo_login_page_hash(const char *buf, size_t len)
{
	GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	const char *c = buf;
	char *d;
	char name[64], value[64];
	int count;
	int input_len = strlen("<input ");
	int name_len = strlen("name=\"");
	int value_len = strlen("value=\"");
	while ((len > ((c - buf) + input_len))
			&& (c = strstr(c, "<input "))) {
		if (!(c = g_strstr_len(c, len - (c - buf), "name=\"")))
			continue;
		c += name_len;
		count = sizeof(name)-1;
		for (d = name; (len > ((c - buf) + 1)) && *c!='"'
				&& count; c++, d++, count--)
			*d = *c;
		*d = '\0';
		count = sizeof(value)-1;
		if (!(d = g_strstr_len(c, len - (c - buf), "value=\"")))
			continue;
		d += value_len;
		if (strchr(c, '>') < d)
			break;
		for (c = d, d = value; (len > ((c - buf) + 1))
				&& *c!='"' && count; c++, d++, count--)
			*d = *c;
		*d = '\0';
		g_hash_table_insert(hash, g_strdup(name), g_strdup(value));
	}
	return hash;
}

static void
yahoo_login_page_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _unused)
{
	PurpleConnection *gc = purple_http_conn_get_purple_connection(http_conn);
	PurpleAccount *account = purple_connection_get_account(gc);
	YahooData *yd = purple_connection_get_protocol_data(gc);
	const char *pass = purple_connection_get_password(gc);
	size_t len;
	const gchar *got_data;
	GHashTable *hash;
	GString *url;
	char md5[33], *hashp = md5, *chal;
	int i;
	PurpleCipher *cipher;
	guchar digest[16];
	PurpleHttpRequest *req;

	if (!purple_http_response_is_successful(response))
	{
		purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			purple_http_response_get_error(response));
		return;
	}

	got_data = purple_http_response_get_data(response, &len);
	hash = yahoo_login_page_hash(got_data, len);

	cipher = purple_md5_cipher_new();

	purple_cipher_append(cipher, (const guchar *)pass, strlen(pass));
	purple_cipher_digest(cipher, digest, sizeof(digest));
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", digest[i]);
		hashp += 2;
	}

	chal = g_strconcat(md5, g_hash_table_lookup(hash, ".challenge"), NULL);
	purple_cipher_reset(cipher);
	purple_cipher_append(cipher, (const guchar *)chal, strlen(chal));
	purple_cipher_digest(cipher, digest, sizeof(digest));
	hashp = md5;
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", digest[i]);
		hashp += 2;
	}
	/*
	 * I dunno why this is here and commented out.. but in case it's needed
	 * I updated it..

	purple_cipher_reset(cipher);
	purple_cipher_append(cipher, md5, strlen(md5));
	purple_cipher_digest(cipher, sizeof(digest), digest, NULL);
	hashp = md5;
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", digest[i]);
		hashp += 2;
	}
	*/
	g_free(chal);

	url = g_string_new(NULL);
	g_string_printf(url, "http://login.yahoo.com/config/login?login=%s&passwd=%s", purple_account_get_username(account), md5);
	g_hash_table_foreach(hash, (GHFunc)yahoo_login_page_hash_iter, url);
	url = g_string_append(url, "&.hash=1&.md5=1");

	g_hash_table_destroy(hash);
	g_object_unref(cipher);

	req = purple_http_request_new(g_string_free(url, FALSE));
	purple_http_request_set_max_redirects(req, 0);
	purple_http_connection_set_add(yd->http_reqs,
		purple_http_request(gc, req, yahoo_login_page_got, NULL));
	purple_http_request_unref(req);
}

#endif /* TRY_WEBMESSENGER_LOGIN */

static void yahoo_picture_check(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleImage *img = purple_buddy_icons_find_account_icon(account);

	yahoo_set_buddy_icon(gc, img);

	if (img)
		g_object_unref(img);
}

static int get_yahoo_status_from_purple_status(PurpleStatus *status)
{
	PurplePresence *presence;
	const char *status_id;
	const char *msg;

	presence = purple_status_get_presence(status);
	status_id = purple_status_get_id(status);
	msg = purple_status_get_attr_string(status, "message");

	if ((msg != NULL) && (*msg != '\0')) {
		return YAHOO_STATUS_CUSTOM;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_AVAILABLE)) {
		return YAHOO_STATUS_AVAILABLE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_BRB)) {
		return YAHOO_STATUS_BRB;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_BUSY)) {
		return YAHOO_STATUS_BUSY;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_NOTATHOME)) {
		return YAHOO_STATUS_NOTATHOME;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_NOTATDESK)) {
		return YAHOO_STATUS_NOTATDESK;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_NOTINOFFICE)) {
		return YAHOO_STATUS_NOTINOFFICE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_ONPHONE)) {
		return YAHOO_STATUS_ONPHONE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_ONVACATION)) {
		return YAHOO_STATUS_ONVACATION;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_OUTTOLUNCH)) {
		return YAHOO_STATUS_OUTTOLUNCH;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_STEPPEDOUT)) {
		return YAHOO_STATUS_STEPPEDOUT;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_INVISIBLE)) {
		return YAHOO_STATUS_INVISIBLE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_AWAY)) {
		return YAHOO_STATUS_CUSTOM;
	} else if (purple_presence_is_idle(presence)) {
		return YAHOO_STATUS_IDLE;
	} else {
		purple_debug_error("yahoo", "Unexpected PurpleStatus!\n");
		return YAHOO_STATUS_AVAILABLE;
	}
}

static void yahoo_got_pager_server(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _yd)
{
	YahooData *yd = _yd;
	PurpleConnection *gc = yd->gc;
	PurpleAccount *a = purple_connection_get_account(gc);
	gchar **strings = NULL, *cs_server = NULL;
	int port = purple_account_get_int(a, "port", YAHOO_PAGER_PORT);
	int stringslen = 0;
	const gchar *got_data;

	if (!purple_http_response_is_successful(response)) {
		purple_debug_error("yahoo", "Unable to retrieve server info: %s\n",
			purple_http_response_get_error(response));

		if(purple_proxy_connect(gc, a, YAHOO_PAGER_HOST_FALLBACK, port,
				yahoo_got_connected, gc) == NULL) {
			purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Unable to connect"));
		}
	} else {
		got_data = purple_http_response_get_data(response, NULL);
		strings = g_strsplit(got_data, "\r\n", -1);

		if((stringslen = g_strv_length(strings)) > 1) {
			int i;

			for(i = 0; i < stringslen; i++) {
				if(g_ascii_strncasecmp(strings[i], "COLO_CAPACITY=", 14) == 0) {
					purple_debug_info("yahoo", "Got COLO Capacity: %s\n", &(strings[i][14]));
				} else if(g_ascii_strncasecmp(strings[i], "CS_IP_ADDRESS=", 14) == 0) {
					cs_server = g_strdup(&strings[i][14]);
					purple_debug_info("yahoo", "Got CS IP address: %s\n", cs_server);
				}
			}
		}

		if(cs_server) { /* got an address; get on with connecting */
			if(purple_proxy_connect(gc, a, cs_server, port, yahoo_got_connected, gc) == NULL)
				purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
								_("Unable to connect"));
		} else {
			purple_debug_error("yahoo", "No CS address retrieved!  Server "
					"response:\n%s\n", got_data);

			if(purple_proxy_connect(gc, a, YAHOO_PAGER_HOST_FALLBACK, port,
					yahoo_got_connected, gc) == NULL) {
				purple_connection_error(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Unable to connect"));
			}
		}
	}

	g_strfreev(strings);
	g_free(cs_server);
}

void yahoo_login(PurpleAccount *account) {
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleHttpRequest *req;
	YahooData *yd = g_new0(YahooData, 1);
	PurpleStatus *status = purple_account_get_active_status(account);

	purple_connection_set_protocol_data(gc, yd);
	purple_connection_set_flags(gc, PURPLE_CONNECTION_FLAG_HTML |
		PURPLE_CONNECTION_FLAG_NO_BGCOLOR |
		PURPLE_CONNECTION_FLAG_NO_URLDESC |
		PURPLE_CONNECTION_FLAG_NO_IMAGES);

	purple_connection_update_progress(gc, _("Connecting"), 1, 2);

	purple_connection_set_display_name(gc, purple_account_get_username(account));

	yd->gc = gc;
	yd->yahoo_local_p2p_server_fd = -1;
	yd->fd = -1;
	yd->txhandler = 0;
	/* TODO: Is there a good grow size for the buffer? */
	yd->txbuf = purple_circular_buffer_new(0);
	yd->http_reqs = purple_http_connection_set_new();
	yd->friends = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, yahoo_friend_free);
	yd->imvironments = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	yd->xfer_peer_idstring_map = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	yd->peers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					yahoo_p2p_disconnect_destroy_data);
	yd->sms_carrier = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	yd->yahoo_p2p_timer = purple_timeout_add_seconds(YAHOO_P2P_KEEPALIVE_SECS,
					yahoo_p2p_keepalive, gc);
	yd->confs = NULL;
	yd->conf_id = 2;
	yd->last_keepalive = yd->last_ping = time(NULL);

	yd->current_status = get_yahoo_status_from_purple_status(status);

	yahoo_picture_check(account);

	/* Get the pager server.  Actually start connecting in the callback since we
	 * must have the contents of the HTTP response to proceed. */
	req = purple_http_request_new(YAHOO_PAGER_HOST_REQ_URL);
	purple_http_request_header_set(req, "User-Agent", YAHOO_CLIENT_USERAGENT);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_got_pager_server, yd));
	purple_http_request_unref(req);

	return;
}

void yahoo_close(PurpleConnection *gc) {
	YahooData *yd = purple_connection_get_protocol_data(gc);
	GSList *l;

	if (yd->inpa) {
		purple_input_remove(yd->inpa);
		yd->inpa = 0;
	}

	purple_http_connection_set_destroy(yd->http_reqs);
	yd->http_reqs = NULL;

	for (l = yd->confs; l; l = l->next) {
		PurpleChatConversation *conv = l->data;
		GList *users;

		users = purple_chat_conversation_get_users(conv);
		yahoo_conf_leave(yd,
			purple_conversation_get_name(PURPLE_CONVERSATION(conv)),
			purple_connection_get_display_name(gc), users);
		g_list_free(users);
	}
	g_slist_free(yd->confs);

	g_slist_free_full(yd->cookies, g_free);

	yd->chat_online = FALSE;
	if (yd->in_chat)
		yahoo_c_leave(gc, 1); /* 1 = YAHOO_CHAT_ID */

	purple_timeout_remove(yd->yahoo_p2p_timer);
	if(yd->yahoo_p2p_server_timeout_handle != 0) {
		purple_timeout_remove(yd->yahoo_p2p_server_timeout_handle);
		yd->yahoo_p2p_server_timeout_handle = 0;
	}

	/* close p2p server if it is waiting for a peer to connect */
	if (yd->yahoo_p2p_server_watcher) {
		purple_input_remove(yd->yahoo_p2p_server_watcher);
		yd->yahoo_p2p_server_watcher = 0;
	}
	if (yd->yahoo_local_p2p_server_fd >= 0) {
		close(yd->yahoo_local_p2p_server_fd);
		yd->yahoo_local_p2p_server_fd = -1;
	}

	g_hash_table_destroy(yd->sms_carrier);
	g_hash_table_destroy(yd->peers);
	g_hash_table_destroy(yd->friends);
	g_hash_table_destroy(yd->imvironments);
	g_hash_table_destroy(yd->xfer_peer_idstring_map);
	g_free(yd->chat_name);

	g_free(yd->cookie_y);
	g_free(yd->cookie_t);
	g_free(yd->cookie_b);

	if (yd->txhandler)
		purple_input_remove(yd->txhandler);

	g_object_unref(G_OBJECT(yd->txbuf));

	if (yd->fd >= 0)
		close(yd->fd);

	g_free(yd->rxqueue);
	yd->rxlen = 0;
	g_free(yd->picture_url);

	purple_http_conn_cancel(yd->picture_upload_hc);
	if (yd->picture_upload_todo)
		yahoo_buddy_icon_upload_data_free(yd->picture_upload_todo);
	if (yd->ycht)
		ycht_connection_close(yd->ycht);
	if (yd->listen_data != NULL)
		purple_network_listen_cancel(yd->listen_data);

	g_free(yd->pending_chat_room);
	g_free(yd->pending_chat_id);
	g_free(yd->pending_chat_topic);
	g_free(yd->pending_chat_goto);
	g_strfreev(yd->profiles);

	yahoo_personal_details_reset(&yd->ypd, TRUE);

	g_free(yd->current_list15_grp);

	g_free(yd);
	purple_connection_set_protocol_data(gc, NULL);
}

const char *yahoo_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "yahoo";
}

const char *yahoo_list_emblem(PurpleBuddy *b)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	YahooFriend *f;
	PurplePresence *presence;

	if (!b || !(account = purple_buddy_get_account(b)) ||
			!(gc = purple_account_get_connection(account)) ||
			!purple_connection_get_protocol_data(gc))
		return NULL;

	f = yahoo_friend_find(gc, purple_buddy_get_name(b));
	if (!f) {
		return "not-authorized";
	}

	presence = purple_buddy_get_presence(b);

	if (purple_presence_is_online(presence)) {
		if (yahoo_friend_get_game(f))
			return "game";
	}
	return NULL;
}

static const char *yahoo_get_status_string(enum yahoo_status a)
{
	switch (a) {
	case YAHOO_STATUS_BRB:
		return _("Be Right Back");
	case YAHOO_STATUS_BUSY:
		return _("Busy");
	case YAHOO_STATUS_NOTATHOME:
		return _("Not at Home");
	case YAHOO_STATUS_NOTATDESK:
		return _("Not at Desk");
	case YAHOO_STATUS_NOTINOFFICE:
		return _("Not in Office");
	case YAHOO_STATUS_ONPHONE:
		return _("On the Phone");
	case YAHOO_STATUS_ONVACATION:
		return _("On Vacation");
	case YAHOO_STATUS_OUTTOLUNCH:
		return _("Out to Lunch");
	case YAHOO_STATUS_STEPPEDOUT:
		return _("Stepped Out");
	case YAHOO_STATUS_INVISIBLE:
		return _("Invisible");
	case YAHOO_STATUS_IDLE:
		return _("Idle");
	case YAHOO_STATUS_OFFLINE:
		return _("Offline");
	default:
		return _("Available");
	}
}

static void yahoo_initiate_conference(PurpleBlistNode *node, gpointer data) {

	PurpleBuddy *buddy;
	PurpleConnection *gc;

	GHashTable *components;
	YahooData *yd;
	int id;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));
	yd = purple_connection_get_protocol_data(gc);
	id = yd->conf_id;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"),
		g_strdup_printf("%s-%d", purple_connection_get_display_name(gc), id));
	g_hash_table_replace(components, g_strdup("topic"), g_strdup("Join my conference..."));
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	yahoo_c_join(gc, components);
	g_hash_table_destroy(components);

	yahoo_c_invite(gc, id, "Join my conference...", purple_buddy_get_name(buddy));
}

static void yahoo_presence_settings(PurpleBlistNode *node, gpointer data) {
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	int presence_val = GPOINTER_TO_INT(data);

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));

	yahoo_friend_update_presence(gc, purple_buddy_get_name(buddy), presence_val);
}

static void yahoo_game(PurpleBlistNode *node, gpointer data) {

	PurpleBuddy *buddy;
	PurpleConnection *gc;

	const char *game;
	char *game2;
	char *t;
	char url[256];
	YahooFriend *f;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));

	f = yahoo_friend_find(gc, purple_buddy_get_name(buddy));
	if (!f)
		return;

	game = yahoo_friend_get_game(f);
	if (!game)
		return;

	t = game2 = g_strdup(strstr(game, "ante?room="));
	while (*t && *t != '\t')
		t++;
	*t = 0;
	g_snprintf(url, sizeof url, "http://games.yahoo.com/games/%s", game2);
	purple_notify_uri(gc, url);
	g_free(game2);
}

char *yahoo_status_text(PurpleBuddy *b)
{
	YahooFriend *f = NULL;
	const char *msg;
	char *msg2;
	PurpleAccount *account;
	PurpleConnection *gc;

	account = purple_buddy_get_account(b);
	gc = purple_account_get_connection(account);
	if (!gc || !purple_connection_get_protocol_data(gc))
		return NULL;

	f = yahoo_friend_find(gc, purple_buddy_get_name(b));
	if (!f)
		return g_strdup(_("Not on server list"));

	switch (f->status) {
	case YAHOO_STATUS_AVAILABLE:
		return NULL;
	case YAHOO_STATUS_IDLE:
		if (f->idle == -1)
			return g_strdup(yahoo_get_status_string(f->status));
		return NULL;
	case YAHOO_STATUS_CUSTOM:
		if (!(msg = yahoo_friend_get_status_message(f)))
			return NULL;
		msg2 = g_markup_escape_text(msg, strlen(msg));
		purple_util_chrreplace(msg2, '\n', ' ');
		return msg2;

	default:
		return g_strdup(yahoo_get_status_string(f->status));
	}
}

void yahoo_tooltip_text(PurpleBuddy *b, PurpleNotifyUserInfo *user_info, gboolean full)
{
	YahooFriend *f;
	char *status = NULL;
	const char *presence = NULL;
	PurpleAccount *account;

	account = purple_buddy_get_account(b);
	f = yahoo_friend_find(purple_account_get_connection(account), purple_buddy_get_name(b));
	if (!f)
		status = g_strdup_printf("\n%s", _("Not on server list"));
	else {
		switch (f->status) {
		case YAHOO_STATUS_CUSTOM:
			if (!yahoo_friend_get_status_message(f))
				return;
			status = g_strdup(yahoo_friend_get_status_message(f));
			break;
		case YAHOO_STATUS_OFFLINE:
			break;
		default:
			status = g_strdup(yahoo_get_status_string(f->status));
			break;
		}

		switch (f->presence) {
			case YAHOO_PRESENCE_ONLINE:
				presence = _("Appear Online");
				break;
			case YAHOO_PRESENCE_PERM_OFFLINE:
				presence = _("Appear Permanently Offline");
				break;
			case YAHOO_PRESENCE_DEFAULT:
				break;
			default:
				purple_debug_error("yahoo", "Unknown presence in yahoo_tooltip_text\n");
				break;
		}
	}

	if (status != NULL) {
		purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), status);
		g_free(status);
	}

	if (presence != NULL)
		purple_notify_user_info_add_pair_plaintext(user_info, _("Presence"), presence);

	if (f && full) {
		YahooPersonalDetails *ypd = &f->ypd;
		if (ypd->phone.home && *ypd->phone.home)
			purple_notify_user_info_add_pair_plaintext(user_info, _("Home Phone Number"), ypd->phone.home);
		if (ypd->phone.work && *ypd->phone.work)
			purple_notify_user_info_add_pair_plaintext(user_info, _("Work Phone Number"), ypd->phone.work);
		if (ypd->phone.mobile && *ypd->phone.mobile)
			purple_notify_user_info_add_pair_plaintext(user_info, _("Mobile Phone Number"), ypd->phone.mobile);
	}
}

static void yahoo_addbuddyfrommenu_cb(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));

	yahoo_add_buddy(gc, buddy, NULL, NULL);
}


static void yahoo_chat_goto_menu(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));

	yahoo_chat_goto(gc, purple_buddy_get_name(buddy));
}

static GList *build_presence_submenu(YahooFriend *f, PurpleConnection *gc) {
	GList *m = NULL;
	PurpleMenuAction *act;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	if (yd->current_status == YAHOO_STATUS_INVISIBLE) {
		if (f->presence != YAHOO_PRESENCE_ONLINE) {
			act = purple_menu_action_new(_("Appear Online"),
			                           PURPLE_CALLBACK(yahoo_presence_settings),
			                           GINT_TO_POINTER(YAHOO_PRESENCE_ONLINE),
			                           NULL);
			m = g_list_append(m, act);
		} else if (f->presence != YAHOO_PRESENCE_DEFAULT) {
			act = purple_menu_action_new(_("Appear Offline"),
			                           PURPLE_CALLBACK(yahoo_presence_settings),
			                           GINT_TO_POINTER(YAHOO_PRESENCE_DEFAULT),
			                           NULL);
			m = g_list_append(m, act);
		}
	}

	if (f->presence == YAHOO_PRESENCE_PERM_OFFLINE) {
		act = purple_menu_action_new(_("Don't Appear Permanently Offline"),
		                           PURPLE_CALLBACK(yahoo_presence_settings),
		                           GINT_TO_POINTER(YAHOO_PRESENCE_DEFAULT),
		                           NULL);
		m = g_list_append(m, act);
	} else {
		act = purple_menu_action_new(_("Appear Permanently Offline"),
		                           PURPLE_CALLBACK(yahoo_presence_settings),
		                           GINT_TO_POINTER(YAHOO_PRESENCE_PERM_OFFLINE),
		                           NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static void yahoo_doodle_blist_node(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *b = (PurpleBuddy *)node;
	PurpleAccount *account = purple_buddy_get_account(b);
	PurpleConnection *gc = purple_account_get_connection(account);

	yahoo_doodle_initiate(gc, purple_buddy_get_name(b));
}

#if 0
/* XXX: it doesn't seems to work */
static void
yahoo_userinfo_blist_node(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *b = (PurpleBuddy *)node;
	PurpleAccount *account = purple_buddy_get_account(b);
	PurpleConnection *gc = purple_account_get_connection(account);

	yahoo_set_userinfo_for_buddy(gc, b);
}
#endif

static GList *yahoo_buddy_menu(PurpleBuddy *buddy)
{
	GList *m = NULL;
	PurpleMenuAction *act;

	PurpleConnection *gc = purple_account_get_connection(purple_buddy_get_account(buddy));
	YahooData *yd = purple_connection_get_protocol_data(gc);
	static char buf2[1024];
	YahooFriend *f;

	f = yahoo_friend_find(gc, purple_buddy_get_name(buddy));

	if (!f && !yd->wm) {
		act = purple_menu_action_new(_("Add Buddy"),
		                           PURPLE_CALLBACK(yahoo_addbuddyfrommenu_cb),
		                           NULL, NULL);
		m = g_list_append(m, act);

		return m;

	}

	if (f && f->status != YAHOO_STATUS_OFFLINE) {
		if (!yd->wm) {
			act = purple_menu_action_new(_("Join in Chat"),
			                           PURPLE_CALLBACK(yahoo_chat_goto_menu),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}

		act = purple_menu_action_new(_("Initiate Conference"),
		                           PURPLE_CALLBACK(yahoo_initiate_conference),
		                           NULL, NULL);
		m = g_list_append(m, act);

		if (yahoo_friend_get_game(f)) {
			const char *game = yahoo_friend_get_game(f);
			char *room;
			char *t;

			if ((room = strstr(game, "&follow="))) {/* skip ahead to the url */
				while (*room && *room != '\t')          /* skip to the tab */
					room++;
				t = room++;                             /* room as now at the name */
				while (*t != '\n')
					t++;                            /* replace the \n with a space */
				*t = ' ';
				g_snprintf(buf2, sizeof buf2, "%s", room);

				act = purple_menu_action_new(buf2,
				                           PURPLE_CALLBACK(yahoo_game),
				                           NULL, NULL);
				m = g_list_append(m, act);
			}
		}
	}

	if (f) {
		act = purple_menu_action_new(_("Presence Settings"), NULL, NULL,
		                           build_presence_submenu(f, gc));
		m = g_list_append(m, act);

		act = purple_menu_action_new(_("Start Doodling"),
				PURPLE_CALLBACK(yahoo_doodle_blist_node),
				NULL, NULL);
		m = g_list_append(m, act);

#if 0
		/* XXX: it doesn't seems to work */
		act = purple_menu_action_new(_("Set User Info..."),
		                           PURPLE_CALLBACK(yahoo_userinfo_blist_node),
		                           NULL, NULL);
		m = g_list_append(m, act);
#endif
	}

	return m;
}

GList *yahoo_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_IS_BUDDY(node)) {
		return yahoo_buddy_menu((PurpleBuddy *) node);
	} else {
		return NULL;
	}
}

static void yahoo_act_id(PurpleConnection *gc, PurpleRequestFields *fields)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	const char *name = yd->profiles[GPOINTER_TO_INT(purple_request_fields_get_choice(fields, "id"))];

	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_IDACT, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash_str(pkt, 3, name);
	yahoo_packet_send_and_free(pkt, yd);

	purple_connection_set_display_name(gc, name);
}

static void
yahoo_get_inbox_token_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _unused)
{
	PurpleConnection *gc =
		purple_http_conn_get_purple_connection(http_conn);
	gchar *url;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);

	if (!purple_http_response_is_successful(response)) {
		purple_debug_error("yahoo",
			"Requesting mail login token failed: %s\n",
			purple_http_response_get_error(response));
		url = g_strdup(YAHOO_MAIL_URL);
	} else {
		/* Should we not be hardcoding the rd url? */
		gchar *token;
		token = g_strdup(purple_http_response_get_data(response, NULL));
		g_strstrip(token);
		url = g_strdup_printf(
			"http://login.yahoo.com/config/reset_cookies_token?"
			".token=%s"
			"&.done=http://us.rd.yahoo.com/messenger/client/%%3f"
			"http://mail.yahoo.com/", token);
		purple_str_wipe(token);
	}

	/* Open the mailbox with the parsed url data */
	purple_notify_uri(gc, url);

	g_free(url);
}


static void yahoo_show_inbox(PurpleProtocolAction *action)
{
	/* Setup a cookie that can be used by the browser */

	PurpleConnection *gc = action->connection;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cookiejar;

	req = purple_http_request_new(
		"https://login.yahoo.com/config/cookie_token");
	purple_http_request_set_method(req, "POST");
	purple_http_request_header_set(req, "User-Agent",
		YAHOO_CLIENT_USERAGENT);
	cookiejar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cookiejar, "T", yd->cookie_t);
	purple_http_cookie_jar_set(cookiejar, "Y", yd->cookie_y);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_get_inbox_token_cb, NULL));
	purple_http_request_unref(req);
}

#if 0
/* XXX: it doesn't seems to work */
static void
yahoo_set_userinfo_fn(PurpleProtocolAction *action)
{
	yahoo_set_userinfo(action->connection);
}
#endif

static void yahoo_show_act_id(PurpleProtocolAction *action)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleConnection *gc = (PurpleConnection *) action->connection;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	const char *name = purple_connection_get_display_name(gc);
	int iter;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_choice_new("id", _("Activate which ID?"), 0);
	purple_request_field_group_add_field(group, field);

	for (iter = 0; yd->profiles[iter]; iter++) {
		purple_request_field_choice_add(field, yd->profiles[iter], GINT_TO_POINTER(iter));
		if (purple_strequal(yd->profiles[iter], name))
			purple_request_field_choice_set_default_value(field, GINT_TO_POINTER(iter));
	}

	purple_request_fields(gc, NULL, _("Select the ID you want to activate"), NULL,
					   fields,
					   _("OK"), G_CALLBACK(yahoo_act_id),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_connection(gc), gc);
}

static void yahoo_show_chat_goto(PurpleProtocolAction *action)
{
	PurpleConnection *gc = action->connection;
	purple_request_input(gc, NULL, _("Join whom in chat?"), NULL,
					   "", FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(yahoo_chat_goto),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_connection(gc),
					   gc);
}

GList *yahoo_get_actions(PurpleConnection *gc) {
	GList *m = NULL;
	PurpleProtocolAction *act;

#if 0
	/* XXX: it doesn't seems to work */
	act = purple_protocol_action_new(_("Set User Info..."),
			yahoo_set_userinfo_fn);
	m = g_list_append(m, act);
#endif

	act = purple_protocol_action_new(_("Activate ID..."),
			yahoo_show_act_id);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Join User in Chat..."),
			yahoo_show_chat_goto);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);
	act = purple_protocol_action_new(_("Open Inbox"),
			yahoo_show_inbox);
	m = g_list_append(m, act);

	return m;
}

struct yahoo_sms_carrier_cb_data	{
	PurpleConnection *gc;
	char *who;
	char *what;
};

static void yahoo_get_sms_carrier_cb(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _sms_cb_data)
{
	struct yahoo_sms_carrier_cb_data *sms_cb_data = _sms_cb_data;
	PurpleConnection *gc = sms_cb_data->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	char *status = NULL;
	char *carrier = NULL;
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleIMConversation *im = purple_conversations_find_im_with_account(sms_cb_data->who, account);

	if (!purple_http_response_is_successful(response)) {
		purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
			_("Can't send SMS. Unable to obtain mobile carrier."), 0);

		g_free(sms_cb_data->who);
		g_free(sms_cb_data->what);
		g_free(sms_cb_data);
		return ;
	} else {
		const gchar *got_data = purple_http_response_get_data(response, NULL);
		PurpleXmlNode *validate_data_root = purple_xmlnode_from_str(got_data, -1);
		PurpleXmlNode *validate_data_child = purple_xmlnode_get_child(validate_data_root, "mobile_no");
		const char *mobile_no = purple_xmlnode_get_attrib(validate_data_child, "msisdn");

		validate_data_root = purple_xmlnode_copy(validate_data_child);
		validate_data_child = purple_xmlnode_get_child(validate_data_root, "status");
		status = purple_xmlnode_get_data(validate_data_child);

		validate_data_child = purple_xmlnode_get_child(validate_data_root, "carrier");
		carrier = purple_xmlnode_get_data(validate_data_child);

		purple_debug_info("yahoo", "SMS validate data: %s\n", got_data);

		if (status && g_str_equal(status, "Valid")) {
			g_hash_table_insert(yd->sms_carrier,
					g_strdup_printf("+%s", mobile_no), g_strdup(carrier));
			yahoo_send_im(sms_cb_data->gc, purple_message_new_outgoing(
				sms_cb_data->who, sms_cb_data->what, 0));
		} else {
			g_hash_table_insert(yd->sms_carrier,
					g_strdup_printf("+%s", mobile_no), g_strdup("Unknown"));
			purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
				_("Can't send SMS. Unknown mobile carrier."), 0);
		}

		purple_xmlnode_free(validate_data_child);
		purple_xmlnode_free(validate_data_root);
		g_free(sms_cb_data->who);
		g_free(sms_cb_data->what);
		g_free(sms_cb_data);
		g_free(status);
		g_free(carrier);
	}
}

static void yahoo_get_sms_carrier(PurpleConnection *gc, gpointer data)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cookiejar;
	struct yahoo_sms_carrier_cb_data *sms_cb_data;
	char *validate_request_str = NULL;
	PurpleXmlNode *validate_request_root = NULL;
	PurpleXmlNode *validate_request_child = NULL;

	if(!(sms_cb_data = data))
		return;

	validate_request_root = purple_xmlnode_new("validate");
	purple_xmlnode_set_attrib(validate_request_root, "intl", "us");
	purple_xmlnode_set_attrib(validate_request_root, "version", YAHOO_CLIENT_VERSION);
	purple_xmlnode_set_attrib(validate_request_root, "qos", "0");

	validate_request_child = purple_xmlnode_new_child(validate_request_root, "mobile_no");
	purple_xmlnode_set_attrib(validate_request_child, "msisdn", sms_cb_data->who + 1);

	validate_request_str = purple_xmlnode_to_str(validate_request_root, NULL);

	purple_xmlnode_free(validate_request_child);
	purple_xmlnode_free(validate_request_root);

	req = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(req, "http://validate.msg.yahoo.com"
		"/mobileno?intl=us&version=%s", YAHOO_CLIENT_VERSION);
	purple_http_request_set_method(req, "POST");
	purple_http_request_header_set(req, "User-Agent",
		YAHOO_CLIENT_USERAGENT);
	cookiejar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cookiejar, "T", yd->cookie_t);
	purple_http_cookie_jar_set(cookiejar, "Y", yd->cookie_y);
	purple_http_request_set_contents(req, validate_request_str, -1);
	purple_http_connection_set_add(yd->http_reqs, purple_http_request(gc,
		req, yahoo_get_sms_carrier_cb, data));
	purple_http_request_unref(req);

	g_free(validate_request_str);
}

int yahoo_send_im(PurpleConnection *gc, PurpleMessage *pmsg)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt = NULL;
	char *msg = yahoo_html_to_codes(purple_message_get_contents(pmsg));
	char *msg2;
	PurpleWhiteboard *wb;
	int ret = 1;
	gsize lenb = 0;
	glong lenc = 0;
	struct yahoo_p2p_data *p2p_data;
	const gchar *rcpt = purple_message_get_recipient(pmsg);

	msg2 = yahoo_string_encode(gc, msg, TRUE);

	if(msg2) {
		lenb = strlen(msg2);
		lenc = g_utf8_strlen(msg2, -1);

		if(lenb > YAHOO_MAX_MESSAGE_LENGTH_BYTES || lenc > YAHOO_MAX_MESSAGE_LENGTH_CHARS) {
			purple_debug_info("yahoo", "Message too big.  Length is %" G_GSIZE_FORMAT
					" bytes, %ld characters.  Max is %d bytes, %d chars."
					"  Message is '%s'.\n", lenb, lenc, YAHOO_MAX_MESSAGE_LENGTH_BYTES,
					YAHOO_MAX_MESSAGE_LENGTH_CHARS, msg2);
			g_free(msg);
			g_free(msg2);
			return -E2BIG;
		}
	}

	if (rcpt[0] == '+') {
		/* we have an sms to be sent */
		gchar *carrier = NULL;
		const char *alias = NULL;
		PurpleAccount *account = purple_connection_get_account(gc);
		PurpleIMConversation *im = purple_conversations_find_im_with_account(rcpt, account);

		carrier = g_hash_table_lookup(yd->sms_carrier, rcpt);
		if (!carrier) {
			struct yahoo_sms_carrier_cb_data *sms_cb_data;
			sms_cb_data = g_malloc(sizeof(struct yahoo_sms_carrier_cb_data));
			sms_cb_data->gc = gc;
			sms_cb_data->who = g_strdup(rcpt);
			sms_cb_data->what = g_strdup(purple_message_get_contents(pmsg));

			purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
				_("Getting mobile carrier to send the SMS."), 0);

			yahoo_get_sms_carrier(gc, sms_cb_data);

			g_free(msg);
			g_free(msg2);
			return ret;
		}
		else if( strcmp(carrier,"Unknown") == 0 ) {
			purple_conversation_write_system_message(PURPLE_CONVERSATION(im),
				_("Can't send SMS. Unknown mobile carrier."), 0);

			g_free(msg);
			g_free(msg2);
			return -1;
		}

		alias = purple_account_get_private_alias(account);
		pkt = yahoo_packet_new(YAHOO_SERVICE_SMS_MSG, YAHOO_STATUS_AVAILABLE, yd->session_id);
		yahoo_packet_hash(pkt, "sssss",
			1, purple_connection_get_display_name(gc),
			69, alias,
			5, rcpt + 1,
			68, carrier,
			14, msg2);
		yahoo_packet_send_and_free(pkt, yd);

		g_free(msg);
		g_free(msg2);

		return ret;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_OFFLINE, yd->session_id);
	yahoo_packet_hash(pkt, "ss", 1, purple_connection_get_display_name(gc), 5, rcpt);

	yahoo_packet_hash_str(pkt, 97, "1"); /* UTF-8 */
	yahoo_packet_hash_str(pkt, 14, msg2);

	/*
	 * IMVironment.
	 *
	 * If this message is to a user who is also Doodling with the local user,
	 * format the chat packet with the correct IMV information (thanks Yahoo!)
	 *
	 * Otherwise attempt to use the same IMVironment as the remote user,
	 * just so that we don't inadvertantly reset their IMVironment back
	 * to nothing.
	 *
	 * If they have not set an IMVironment, then use the default.
	 */
	wb = purple_whiteboard_get_session(purple_connection_get_account(gc), rcpt);
	if (wb)
		yahoo_packet_hash_str(pkt, 63, DOODLE_IMV_KEY);
	else
	{
		const char *imv;
		imv = g_hash_table_lookup(yd->imvironments, rcpt);
		if (imv != NULL)
			yahoo_packet_hash_str(pkt, 63, imv);
		else
			yahoo_packet_hash_str(pkt, 63, ";0");
	}

	yahoo_packet_hash_str(pkt,   64, "0"); /* no idea */
	yahoo_packet_hash_str(pkt, 1002, "1"); /* no idea, Yahoo 6 or later only it seems */
	if (!yd->picture_url)
		yahoo_packet_hash_str(pkt, 206, "0"); /* 0 = no picture, 2 = picture, maybe 1 = avatar? */
	else
		yahoo_packet_hash_str(pkt, 206, "2");

	/* We may need to not send any packets over 2000 bytes, but I'm not sure yet. */
	if ((YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt)) <= 2000) {
		/* if p2p link exists, send through it. To-do: key 15, time value to be sent in case of p2p */
		if( (p2p_data = g_hash_table_lookup(yd->peers, rcpt))) {
			yahoo_packet_hash_int(pkt, 11, p2p_data->session_id);
			yahoo_p2p_write_pkt(p2p_data->source, pkt);
		}
		else	{
			yahoo_packet_send(pkt, yd);
			yahoo_send_p2p_pkt(gc, rcpt, 0);		/* send p2p packet, with val_13=0 */
		}
	}
	else
		ret = -E2BIG;

	yahoo_packet_free(pkt);

	g_free(msg);
	g_free(msg2);

	return ret;
}

unsigned int yahoo_send_typing(PurpleConnection *gc, const char *who, PurpleIMTypingState state)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_p2p_data *p2p_data;
	struct yahoo_packet *pkt = NULL;

	/* Don't do anything if sms is being typed */
	if( strncmp(who, "+", 1) == 0 )
		return 0;

	pkt = yahoo_packet_new(YAHOO_SERVICE_NOTIFY, YAHOO_STATUS_TYPING, yd->session_id);

	/* check to see if p2p link exists, send through it */
	if( (p2p_data = g_hash_table_lookup(yd->peers, who))) {
		yahoo_packet_hash(pkt, "sssssis", 49, "TYPING", 1, purple_connection_get_display_name(gc),
	                  14, " ", 13, state == PURPLE_IM_TYPING ? "1" : "0",
	                  5, who, 11, p2p_data->session_id, 1002, "1");	/* To-do: key 15 to be sent in case of p2p */
		yahoo_p2p_write_pkt(p2p_data->source, pkt);
		yahoo_packet_free(pkt);
	}
	else {	/* send through yahoo server */

		yahoo_packet_hash(pkt, "ssssss", 49, "TYPING", 1, purple_connection_get_display_name(gc),
                  14, " ", 13, state == PURPLE_IM_TYPING ? "1" : "0",
                  5, who, 1002, "1");
		yahoo_packet_send_and_free(pkt, yd);
	}

	return 0;
}

static void yahoo_session_presence_remove(gpointer key, gpointer value, gpointer data)
{
	YahooFriend *f = value;
	if (f && f->presence == YAHOO_PRESENCE_ONLINE)
		f->presence = YAHOO_PRESENCE_DEFAULT;
}

void yahoo_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	PurplePresence *presence;
	YahooData *yd;
	struct yahoo_packet *pkt;
	int old_status;
	const char *msg = NULL;
	char *tmp = NULL;
	char *conv_msg = NULL;

	if (!purple_status_is_active(status))
		return;

	gc = purple_account_get_connection(account);
	presence = purple_status_get_presence(status);
	yd = purple_connection_get_protocol_data(gc);
	old_status = yd->current_status;

	yd->current_status = get_yahoo_status_from_purple_status(status);

	if (yd->current_status == YAHOO_STATUS_CUSTOM)
	{
		msg = purple_status_get_attr_string(status, "message");

		if (purple_status_is_available(status)) {
			tmp = yahoo_string_encode(gc, msg, TRUE);
			conv_msg = purple_markup_strip_html(tmp);
			g_free(tmp);
		} else {
			if ((msg == NULL) || (*msg == '\0'))
				msg = _("Away");
			tmp = yahoo_string_encode(gc, msg, TRUE);
			conv_msg = purple_markup_strip_html(tmp);
			g_free(tmp);
		}
	}

	if (yd->current_status == YAHOO_STATUS_INVISIBLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_VISIBLE_TOGGLE, YAHOO_STATUS_AVAILABLE, yd->session_id);
		yahoo_packet_hash_str(pkt, 13, "2");
		yahoo_packet_send_and_free(pkt, yd);

		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_STATUS_UPDATE, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash_int(pkt, 10, yd->current_status);

	if (yd->current_status == YAHOO_STATUS_CUSTOM) {
		yahoo_packet_hash_str(pkt, 97, "1"); /* UTF-8 */
		yahoo_packet_hash_str(pkt, 19, conv_msg);
	} else {
		yahoo_packet_hash_str(pkt, 19, "");
	}

	g_free(conv_msg);

	if (purple_presence_is_idle(presence))
		yahoo_packet_hash_str(pkt, 47, "2");
	else	{
		if (!purple_status_is_available(status))
			yahoo_packet_hash_str(pkt, 47, "1");
		else
			yahoo_packet_hash_str(pkt, 47, "0");
	}

	yahoo_packet_send_and_free(pkt, yd);

	if (old_status == YAHOO_STATUS_INVISIBLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_VISIBLE_TOGGLE, YAHOO_STATUS_AVAILABLE, yd->session_id);
		yahoo_packet_hash_str(pkt, 13, "1");
		yahoo_packet_send_and_free(pkt, yd);

		/* Any per-session presence settings are removed */
		g_hash_table_foreach(yd->friends, yahoo_session_presence_remove, NULL);

	}
}

void yahoo_set_idle(PurpleConnection *gc, int idle)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt = NULL;
	char *msg = NULL, *msg2 = NULL;
	PurpleStatus *status = NULL;
	gboolean invisible = FALSE;

	if (idle && yd->current_status != YAHOO_STATUS_CUSTOM)
		yd->current_status = YAHOO_STATUS_IDLE;
	else if (!idle && yd->current_status == YAHOO_STATUS_IDLE) {
		status = purple_presence_get_active_status(purple_account_get_presence(purple_connection_get_account(gc)));
		yd->current_status = get_yahoo_status_from_purple_status(status);
	}

	invisible = (yd->current_status == YAHOO_STATUS_INVISIBLE);

	pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_STATUS_UPDATE, YAHOO_STATUS_AVAILABLE, yd->session_id);

	if (!idle && invisible)
		yahoo_packet_hash_int(pkt, 10, YAHOO_STATUS_AVAILABLE);
	else
		yahoo_packet_hash_int(pkt, 10, yd->current_status);

	if (yd->current_status == YAHOO_STATUS_CUSTOM) {
		const char *tmp;
		if (status == NULL)
			status = purple_presence_get_active_status(purple_account_get_presence(purple_connection_get_account(gc)));
		tmp = purple_status_get_attr_string(status, "message");
		if (tmp != NULL) {
			msg = yahoo_string_encode(gc, tmp, TRUE);
			msg2 = purple_markup_strip_html(msg);
			yahoo_packet_hash_str(pkt, 97, "1"); /* UTF-8 */
			yahoo_packet_hash_str(pkt, 19, msg2);
		} else {
			/* get_yahoo_status_from_purple_status() returns YAHOO_STATUS_CUSTOM for
			 * the generic away state (YAHOO_STATUS_TYPE_AWAY) with no message */
			yahoo_packet_hash_str(pkt, 19, _("Away"));
		}
	} else {
		yahoo_packet_hash_str(pkt, 19, "");
	}

	if (idle)
		yahoo_packet_hash_str(pkt, 47, "2");
	else if (yd->current_status == YAHOO_STATUS_CUSTOM &&
			!purple_status_is_available(status))
		/* We are still unavailable in this case.
		 * Make sure Yahoo knows that */
		yahoo_packet_hash_str(pkt, 47, "1");

	yahoo_packet_send_and_free(pkt, yd);

	g_free(msg);
	g_free(msg2);
}

GList *yahoo_status_types(PurpleAccount *account)
{
	PurpleStatusType *type;
	GList *types = NULL;

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, YAHOO_STATUS_TYPE_AVAILABLE,
	                                       NULL, TRUE, TRUE, FALSE,
	                                       "message", _("Message"),
	                                       purple_value_new(G_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_AWAY,
	                                       NULL, TRUE, TRUE, FALSE,
	                                       "message", _("Message"),
	                                       purple_value_new(G_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_BRB, _("Be Right Back"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE, YAHOO_STATUS_TYPE_BUSY,
	                                       _("Busy"), TRUE, TRUE, FALSE,
	                                       "message", _("Message"),
	                                       purple_value_new(G_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_NOTATHOME, _("Not at Home"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_NOTATDESK, _("Not at Desk"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_NOTINOFFICE, _("Not in Office"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_UNAVAILABLE, YAHOO_STATUS_TYPE_ONPHONE, _("On the Phone"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_EXTENDED_AWAY, YAHOO_STATUS_TYPE_ONVACATION, _("On Vacation"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_OUTTOLUNCH, _("Out to Lunch"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_STEPPEDOUT, _("Stepped Out"), TRUE);
	types = g_list_append(types, type);


	type = purple_status_type_new(PURPLE_STATUS_INVISIBLE, YAHOO_STATUS_TYPE_INVISIBLE, NULL, TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_OFFLINE, YAHOO_STATUS_TYPE_OFFLINE, NULL, TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new_full(PURPLE_STATUS_MOBILE, YAHOO_STATUS_TYPE_MOBILE, NULL, FALSE, FALSE, TRUE);
	types = g_list_append(types, type);

	return types;
}

void yahoo_keepalive(PurpleConnection *gc)
{
	struct yahoo_packet *pkt;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	time_t now = time(NULL);

	/* We're only allowed to send a ping once an hour or the servers will boot us */
	if ((now - yd->last_ping) >= PING_TIMEOUT) {
		yd->last_ping = now;

		/* The native client will only send PING or CHATPING */
		if (yd->chat_online) {
			if (yd->wm) {
				ycht_chat_send_keepalive(yd->ycht);
			} else {
				pkt = yahoo_packet_new(YAHOO_SERVICE_CHATPING, YAHOO_STATUS_AVAILABLE, yd->session_id);
				yahoo_packet_hash_str(pkt, 109, purple_connection_get_display_name(gc));
				yahoo_packet_send_and_free(pkt, yd);
			}
		} else {
			pkt = yahoo_packet_new(YAHOO_SERVICE_PING, YAHOO_STATUS_AVAILABLE, yd->session_id);
			yahoo_packet_send_and_free(pkt, yd);
		}
	}

	if ((now - yd->last_keepalive) >= KEEPALIVE_TIMEOUT) {
		yd->last_keepalive = now;
		pkt = yahoo_packet_new(YAHOO_SERVICE_KEEPALIVE, YAHOO_STATUS_AVAILABLE, yd->session_id);
		yahoo_packet_hash_str(pkt, 0, purple_connection_get_display_name(gc));
		yahoo_packet_send_and_free(pkt, yd);
	}

}

void yahoo_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *g, const char *message)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	const char *group = NULL;
	char *group2;
	const char *bname = purple_buddy_get_name(buddy);

	if (!yd->logged_in)
		return;

	if (!purple_account_privacy_check(purple_connection_get_account(gc), bname))
		return;

	group = purple_group_get_name(purple_buddy_get_group(buddy));

	group2 = yahoo_string_encode(gc, group, FALSE);
	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "ssssssssss",
					  14, "",
					  65, group2,
					  97, "1", /* UTF-8 */
					  1, purple_connection_get_display_name(gc),
					  302, "319",
					  300, "319",
					  7, bname,
					  334, "0",
					  301, "319",
					  303, "319");

	yahoo_packet_send_and_free(pkt, yd);
	g_free(group2);
}

void yahoo_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	GSList *buddies, *l;
	PurpleGroup *g;
	gboolean remove = TRUE;
	char *cg;
	const char *bname, *gname;
	YahooFriend *f = NULL;

	bname = purple_buddy_get_name(buddy);
	f = yahoo_friend_find(gc, bname);
	if (!f)
		return;

	gname = purple_group_get_name(group);
	buddies = purple_blist_find_buddies(purple_connection_get_account(gc), bname);
	for (l = buddies; l; l = l->next) {
		g = purple_buddy_get_group(l->data);
		if (purple_utf8_strcasecmp(gname, purple_group_get_name(g))) {
			remove = FALSE;
			break;
		}
	}

	g_slist_free(buddies);

	if (remove) {
		g_hash_table_remove(yd->friends, bname);
		f = NULL; /* f no longer valid - Just making it clear */
	}

	cg = yahoo_string_encode(gc, gname, FALSE);
	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc),
	                  7, bname, 65, cg);
	yahoo_packet_send_and_free(pkt, yd);
	g_free(cg);
}

void yahoo_add_deny(PurpleConnection *gc, const char *who) {
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc), 7, who, 13, "1");

	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_rem_deny(PurpleConnection *gc, const char *who) {
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc), 7, who, 13, "2");

	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_set_permit_deny(PurpleConnection *gc)
{
	PurpleAccount *account;
	GSList *deny;

	account = purple_connection_get_account(gc);

	switch (purple_account_get_privacy_type(account))
	{
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			for (deny = purple_account_privacy_get_denied(account); deny; deny = deny->next)
				yahoo_rem_deny(gc, deny->data);
			break;

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			for (deny = purple_account_privacy_get_denied(account); deny; deny = deny->next)
				yahoo_add_deny(gc, deny->data);
			break;
	}
}

void yahoo_change_buddys_group(PurpleConnection *gc, const char *who,
				   const char *old_group, const char *new_group)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	char *gpn, *gpo;
	YahooFriend *f = yahoo_friend_find(gc, who);
	const char *temp = NULL;

	/* Step 0:  If they aren't on the server list anyway,
	 *          don't bother letting the server know.
	 */
	if (!f)
		return;

	temp = who;

	/* If old and new are the same, we would probably
	 * end up deleting the buddy, which would be bad.
	 * This might happen because of the charset conversation.
	 */
	gpn = yahoo_string_encode(gc, new_group, FALSE);
	gpo = yahoo_string_encode(gc, old_group, FALSE);
	if (!strcmp(gpn, gpo)) {
		g_free(gpn);
		g_free(gpo);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHGRP_15, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "ssssssss", 1, purple_connection_get_display_name(gc),
				  302, "240", 300, "240", 7, temp, 224, gpo, 264, gpn, 301,
				  "240", 303, "240");
	yahoo_packet_send_and_free(pkt, yd);

	g_free(gpn);
	g_free(gpo);
}

void yahoo_rename_group(PurpleConnection *gc, const char *old_name,
							   PurpleGroup *group, GList *moved_buddies)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	char *gpn, *gpo;

	gpn = yahoo_string_encode(gc, purple_group_get_name(group), FALSE);
	gpo = yahoo_string_encode(gc, old_name, FALSE);
	if (!strcmp(gpn, gpo)) {
		g_free(gpn);
		g_free(gpo);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_GROUPRENAME, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc),
	                  65, gpo, 67, gpn);
	yahoo_packet_send_and_free(pkt, yd);
	g_free(gpn);
	g_free(gpo);
}

/********************************* Commands **********************************/

PurpleCmdRet
yahoopurple_cmd_buzz(PurpleConversation *c, const gchar *cmd, gchar **args, gchar **error, void *data) {
	PurpleAccount *account = purple_conversation_get_account(c);

	if (*args && args[0])
		return PURPLE_CMD_RET_FAILED;

	purple_protocol_send_attention(purple_account_get_connection(account), purple_conversation_get_name(c), YAHOO_BUZZ);

	return PURPLE_CMD_RET_OK;
}

PurpleCmdRet
yahoopurple_cmd_chat_join(PurpleConversation *conv, const char *cmd,
                        char **args, char **error, void *data)
{
	GHashTable *comp;
	PurpleConnection *gc;

	if (!args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	gc = purple_conversation_get_connection(conv);
	purple_debug_info("yahoo", "Trying to join %s \n", args[0]);

	comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(comp, g_strdup("room"), g_ascii_strdown(args[0], -1));
	g_hash_table_replace(comp, g_strdup("type"), g_strdup("Chat"));

	yahoo_c_join(gc, comp);

	g_hash_table_destroy(comp);
	return PURPLE_CMD_RET_OK;
}

PurpleCmdRet
yahoopurple_cmd_chat_list(PurpleConversation *conv, const char *cmd,
                        char **args, char **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	if (*args && args[0])
		return PURPLE_CMD_RET_FAILED;
	purple_roomlist_show_with_account(account);
	return PURPLE_CMD_RET_OK;
}

gboolean yahoo_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

gboolean yahoo_send_attention(PurpleConnection *gc, const char *username, guint type)
{
	PurpleIMConversation *im;

	im = purple_conversations_find_im_with_account(username,
			purple_connection_get_account(gc));

	g_return_val_if_fail(im != NULL, FALSE);

	purple_debug_info("yahoo", "Sending <ding> on account %s to buddy %s.\n",
			username, purple_conversation_get_name(PURPLE_CONVERSATION(im)));
	purple_conversation_send_with_flags(PURPLE_CONVERSATION(im), "<ding>", PURPLE_MESSAGE_INVISIBLE);

	return TRUE;
}

GList *yahoo_attention_types(PurpleAccount *account)
{
	static GList *list = NULL;

	if (!list) {
		/* Yahoo only supports one attention command: the 'buzz'. */
		/* This is index number YAHOO_BUZZ. */
		list = g_list_append(list, purple_attention_type_new("Buzz", _("Buzz"),
				_("%s has buzzed you!"), _("Buzzing %s...")));
	}

	return list;
}

gssize
yahoo_get_max_message_size(PurpleConversation *conv)
{
	return YAHOO_MAX_MESSAGE_LENGTH_CHARS;
}
