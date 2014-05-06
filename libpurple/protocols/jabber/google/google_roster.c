/**
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
#include "google_roster.h"
#include "jabber.h"
#include "presence.h"
#include "debug.h"
#include "xmlnode.h"
#include "roster.h"

void jabber_google_roster_outgoing(JabberStream *js, PurpleXmlNode *query, PurpleXmlNode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	GSList *list = purple_account_privacy_get_denied(account);
	const char *jid = purple_xmlnode_get_attrib(item, "jid");
	char *jid_norm = (char *)jabber_normalize(account, jid);

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			purple_xmlnode_set_attrib(query, "xmlns:gr", NS_GOOGLE_ROSTER);
			purple_xmlnode_set_attrib(query, "gr:ext", "2");
			purple_xmlnode_set_attrib(item, "gr:t", "B");
			return;
		}
		list = list->next;
	}
}

gboolean jabber_google_roster_incoming(JabberStream *js, PurpleXmlNode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	const char *jid = purple_xmlnode_get_attrib(item, "jid");
	gboolean on_block_list = FALSE;

	char *jid_norm;

	const char *grt = purple_xmlnode_get_attrib_with_namespace(item, "t", NS_GOOGLE_ROSTER);
	const char *subscription = purple_xmlnode_get_attrib(item, "subscription");
	const char *ask = purple_xmlnode_get_attrib(item, "ask");

	if ((!subscription || !strcmp(subscription, "none")) && !ask) {
		/* The Google Talk servers will automatically add people from your Gmail address book
		 * with subscription=none. If we see someone with subscription=none, ignore them.
		 */
		return FALSE;
	}

	jid_norm = g_strdup(jabber_normalize(account, jid));

	on_block_list = NULL != g_slist_find_custom(purple_account_privacy_get_denied(account),
	                                            jid_norm, (GCompareFunc)strcmp);

	if (grt && (*grt == 'H' || *grt == 'h')) {
		/* Hidden; don't show this buddy. */
		GSList *buddies = purple_blist_find_buddies(account, jid_norm);
		if (buddies) {
			purple_debug_info("jabber", "Removing %s from local buddy list\n",
			                  jid_norm);

			do {
				purple_blist_remove_buddy(buddies->data);
				buddies = g_slist_delete_link(buddies, buddies);
			} while (buddies);
		}

		g_free(jid_norm);
		return FALSE;
	}

	if (!on_block_list && (grt && (*grt == 'B' || *grt == 'b'))) {
		purple_debug_info("jabber", "Blocking %s\n", jid_norm);
		purple_account_privacy_deny_add(account, jid_norm, TRUE);
	} else if (on_block_list && (!grt || (*grt != 'B' && *grt != 'b' ))){
		purple_debug_info("jabber", "Unblocking %s\n", jid_norm);
		purple_account_privacy_deny_remove(account, jid_norm, TRUE);
	}

	g_free(jid_norm);
	return TRUE;
}

void jabber_google_roster_add_deny(JabberStream *js, const char *who)
{
	PurpleAccount *account;
	GSList *buddies;
	JabberIq *iq;
	PurpleXmlNode *query;
	PurpleXmlNode *item;
	PurpleXmlNode *group;
	PurpleBuddy *b;
	JabberBuddy *jb;
	const char *balias;

	jb = jabber_buddy_find(js, who, TRUE);

	account = purple_connection_get_account(js->gc);
	buddies = purple_blist_find_buddies(account, who);
	if(!buddies)
		return;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = purple_xmlnode_get_child(iq->node, "query");
	item = purple_xmlnode_new_child(query, "item");

	do {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = purple_xmlnode_new_child(item, "group");
		purple_xmlnode_insert_data(group,
			jabber_roster_group_get_global_name(g), -1);

		buddies = g_slist_delete_link(buddies, buddies);
	} while (buddies);

	balias = purple_buddy_get_local_alias(b);
	purple_xmlnode_set_attrib(item, "jid", who);
	purple_xmlnode_set_attrib(item, "name", balias ? balias : "");
	purple_xmlnode_set_attrib(item, "gr:t", "B");
	purple_xmlnode_set_attrib(query, "xmlns:gr", NS_GOOGLE_ROSTER);
	purple_xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* Synthesize a sign-off */
	if (jb) {
		JabberBuddyResource *jbr;
		GList *l = jb->resources;
		while (l) {
			jbr = l->data;
			if (jbr && jbr->name)
			{
				purple_debug_misc("jabber", "Removing resource %s\n", jbr->name);
				jabber_buddy_remove_resource(jb, jbr->name);
			}
			l = l->next;
		}
	}

	purple_prpl_got_user_status(account, who, "offline", NULL);
}

void jabber_google_roster_rem_deny(JabberStream *js, const char *who)
{
	GSList *buddies;
	JabberIq *iq;
	PurpleXmlNode *query;
	PurpleXmlNode *item;
	PurpleXmlNode *group;
	PurpleBuddy *b;
	const char *balias;

	buddies = purple_blist_find_buddies(purple_connection_get_account(js->gc), who);
	if(!buddies)
		return;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = purple_xmlnode_get_child(iq->node, "query");
	item = purple_xmlnode_new_child(query, "item");

	do {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = purple_xmlnode_new_child(item, "group");
		purple_xmlnode_insert_data(group,
			jabber_roster_group_get_global_name(g), -1);

		buddies = g_slist_delete_link(buddies, buddies);
	} while (buddies);

	balias = purple_buddy_get_local_alias(b);
	purple_xmlnode_set_attrib(item, "jid", who);
	purple_xmlnode_set_attrib(item, "name", balias ? balias : "");
	purple_xmlnode_set_attrib(query, "xmlns:gr", NS_GOOGLE_ROSTER);
	purple_xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* See if he's online */
	jabber_presence_subscription_set(js, who, "probe");
}

