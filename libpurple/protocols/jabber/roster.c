/*
 * purple - Jabber Protocol Plugin
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
#include "internal.h"
#include "debug.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
#include "chat.h"
#include "facebook_roster.h"
#include "google/google.h"
#include "google/google_roster.h"
#include "presence.h"
#include "roster.h"
#include "iq.h"

#include <string.h>

/* Take a list of strings and join them with a ", " separator */
static gchar *roster_groups_join(GSList *list)
{
	GString *out = g_string_new(NULL);
	for ( ; list; list = list->next) {
		out = g_string_append(out, (const char *)list->data);
		if (list->next)
			out = g_string_append(out, ", ");
	}

	return g_string_free(out, FALSE);
}

static void roster_request_cb(JabberStream *js, const char *from,
                              JabberIqType type, const char *id,
                              PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query;

	if (type == JABBER_IQ_ERROR) {
		/*
		 * This shouldn't happen in any real circumstances and
		 * likely constitutes a server-side misconfiguration (i.e.
		 * explicitly not loading mod_roster...)
		 */
		purple_debug_error("jabber", "Error retrieving roster!?\n");
		jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
		return;
	}

	query = purple_xmlnode_get_child(packet, "query");
	if (query == NULL) {
		jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
		return;
	}

	jabber_roster_parse(js, from, type, id, query);
	jabber_stream_set_state(js, JABBER_STREAM_CONNECTED);
}

void jabber_roster_request(JabberStream *js)
{
	JabberIq *iq;
	PurpleXmlNode *query;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:roster");
	query = purple_xmlnode_get_child(iq->node, "query");

	if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER) {
		purple_xmlnode_set_attrib(query, "xmlns:gr", NS_GOOGLE_ROSTER);
		purple_xmlnode_set_attrib(query, "gr:ext", "2");
	}

	jabber_iq_set_callback(iq, roster_request_cb, NULL);
	jabber_iq_send(iq);
}

static void remove_purple_buddies(JabberStream *js, const char *jid)
{
	GSList *buddies, *l;

	buddies = purple_blist_find_buddies(purple_connection_get_account(js->gc), jid);

	for(l = buddies; l; l = l->next)
		purple_blist_remove_buddy(l->data);

	g_slist_free(buddies);
}

static void add_purple_buddy_to_groups(JabberStream *js, const char *jid,
		const char *alias, GSList *groups)
{
	GSList *buddies, *l;
	PurpleAccount *account = purple_connection_get_account(js->gc);

	buddies = purple_blist_find_buddies(purple_connection_get_account(js->gc), jid);

	if(!groups) {
		if(!buddies) {
			groups = g_slist_append(groups, g_strdup(JABBER_ROSTER_DEFAULT_GROUP));
		} else {
			/* TODO: What should we do here? Removing the local buddies
			 * is wrong, but so is letting the group state get out of sync with
			 * the server.
			 */
			g_slist_free(buddies);
			return;
		}
	}

	while(buddies) {
		PurpleBuddy *b = buddies->data;
		PurpleGroup *g = purple_buddy_get_group(b);

		buddies = g_slist_delete_link(buddies, buddies);

		/* XMPP groups are case-sensitive, but libpurple groups are
		 * case-insensitive. We treat a buddy in both "Friends" and "friends"
		 * as only being in one group, but if we push changes about the buddy
		 * to the server, the buddy will be dropped from one of the groups.
		 * Not optimal, but better than the alternative, I think.
		 */
		l = g_slist_find_custom(groups, purple_group_get_name(g),
			(GCompareFunc)purple_utf8_strcasecmp);
		if (!l && g == purple_blist_get_default_group()) {
			l = g_slist_find_custom(groups, JABBER_ROSTER_DEFAULT_GROUP,
				(GCompareFunc)purple_utf8_strcasecmp);
		}
		if (!l && g == purple_blist_get_default_group()) {
			l = g_slist_find_custom(groups,
				_purple_blist_get_localized_default_group_name(),
				(GCompareFunc)purple_utf8_strcasecmp);
		}

		if (l) {
			/* The buddy is already on the local list. Update info. */
			const char *servernick, *balias;

			/* Previously stored serverside / buddy-supplied alias */
			if((servernick = purple_blist_node_get_string((PurpleBlistNode*)b, "servernick")))
				purple_serv_got_alias(js->gc, jid, servernick);

			/* Alias from our roster retrieval */
			balias = purple_buddy_get_local_alias(b);
			if(alias && !purple_strequal(alias, balias))
				purple_serv_got_private_alias(js->gc, jid, alias);
			g_free(l->data);
			groups = g_slist_delete_link(groups, l);
		} else {
			/* This buddy isn't in the group on the server anymore */
			purple_debug_info("jabber", "jabber_roster_parse(): "
				"Removing %s from group '%s' on the local list",
				purple_buddy_get_name(b),
				jabber_roster_group_get_global_name(g));
			purple_blist_remove_buddy(b);
		}
	}

	if (groups) {
		char *tmp = roster_groups_join(groups);
		purple_debug_info("jabber", "jabber_roster_parse(): Adding %s to "
		                  "groups: %s\n", jid, tmp);
		g_free(tmp);
	}

	while(groups) {
		PurpleGroup *g = purple_blist_find_group(groups->data);
		PurpleBuddy *b = purple_buddy_new(account, jid, alias);

		if(!g) {
			g = purple_group_new(groups->data);
			purple_blist_add_group(g, NULL);
		}

		purple_blist_add_buddy(b, NULL, g, NULL);
		purple_buddy_set_local_alias(b, alias);

		g_free(groups->data);
		groups = g_slist_delete_link(groups, groups);
	}

	g_slist_free(buddies);
}

void jabber_roster_parse(JabberStream *js, const char *from,
                         JabberIqType type, const char *id, PurpleXmlNode *query)
{
	PurpleXmlNode *item, *group;
#if 0
	const char *ver;
#endif

	if (!jabber_is_own_account(js, from)) {
		purple_debug_warning("jabber", "Received bogon roster push from %s\n",
		                     from);
		return;
	}

	js->currently_parsing_roster_push = TRUE;

	if (js->server_caps & JABBER_CAP_FACEBOOK)
		jabber_facebook_roster_cleanup(js, query);

	for(item = purple_xmlnode_get_child(query, "item"); item; item = purple_xmlnode_get_next_twin(item))
	{
		const char *jid, *name, *subscription, *ask;
		JabberBuddy *jb;

		if (js->server_caps & JABBER_CAP_FACEBOOK)
			if (!jabber_facebook_roster_incoming(js, item))
				continue;

		subscription = purple_xmlnode_get_attrib(item, "subscription");
		jid = purple_xmlnode_get_attrib(item, "jid");
		name = purple_xmlnode_get_attrib(item, "name");
		ask = purple_xmlnode_get_attrib(item, "ask");

		if(!jid)
			continue;

		if(!(jb = jabber_buddy_find(js, jid, TRUE)))
			continue;

		if(subscription) {
			if (g_str_equal(subscription, "remove"))
				jb->subscription = JABBER_SUB_REMOVE;
			else if (jb == js->user_jb)
				jb->subscription = JABBER_SUB_BOTH;
			else if (g_str_equal(subscription, "none"))
				jb->subscription = JABBER_SUB_NONE;
			else if (g_str_equal(subscription, "to"))
				jb->subscription = JABBER_SUB_TO;
			else if (g_str_equal(subscription, "from"))
				jb->subscription = JABBER_SUB_FROM;
			else if (g_str_equal(subscription, "both"))
				jb->subscription = JABBER_SUB_BOTH;
		}

		if(purple_strequal(ask, "subscribe"))
			jb->subscription |= JABBER_SUB_PENDING;
		else
			jb->subscription &= ~JABBER_SUB_PENDING;

		if(jb->subscription & JABBER_SUB_REMOVE) {
			remove_purple_buddies(js, jid);
		} else {
			GSList *groups = NULL;

			if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
				if (!jabber_google_roster_incoming(js, item))
					continue;

			for(group = purple_xmlnode_get_child(item, "group"); group; group = purple_xmlnode_get_next_twin(group)) {
				char *group_name = purple_xmlnode_get_data(group);

				if (group_name == NULL || *group_name == '\0')
					/* Changing this string?  Look in add_purple_buddy_to_groups */
					group_name = g_strdup(JABBER_ROSTER_DEFAULT_GROUP);

				/*
				 * See the note in add_purple_buddy_to_groups; the core handles
				 * names case-insensitively and this is required to not
				 * end up with duplicates if a buddy is in, e.g.,
				 * 'XMPP' and 'xmpp'
				 */
				if (g_slist_find_custom(groups, group_name, (GCompareFunc)purple_utf8_strcasecmp))
					g_free(group_name);
				else
					groups = g_slist_prepend(groups, group_name);
			}

			add_purple_buddy_to_groups(js, jid, name, groups);
			if (jb == js->user_jb)
				jabber_presence_fake_to_self(js, NULL);
		}
	}

#if 0
	ver = purple_xmlnode_get_attrib(query, "ver");
	if (ver) {
		 PurpleAccount *account = purple_connection_get_account(js->gc);
		 purple_account_set_string(account, "roster_ver", ver);
	}
#endif

	if (type == JABBER_IQ_SET) {
		JabberIq *ack = jabber_iq_new(js, JABBER_IQ_RESULT);
		jabber_iq_set_id(ack, id);
		jabber_iq_send(ack);
	}

	js->currently_parsing_roster_push = FALSE;
}

/* jabber_roster_update frees the GSList* passed in */
static void jabber_roster_update(JabberStream *js, const char *name,
		GSList *groups)
{
	PurpleBuddy *b;
	PurpleGroup *g;
	GSList *l;
	JabberIq *iq;
	PurpleXmlNode *query, *item, *group;
	const char *balias;

	if (js->currently_parsing_roster_push)
		return;

	if(!(b = purple_blist_find_buddy(purple_connection_get_account(js->gc), name)))
		return;

	if (groups) {
		char *tmp = roster_groups_join(groups);

		purple_debug_info("jabber", "jabber_roster_update(%s): [Source: "
		                  "groups]: groups: %s\n", name, tmp);
		g_free(tmp);
	} else {
		GSList *buddies = purple_blist_find_buddies(purple_connection_get_account(js->gc), name);
		char *tmp;

		if(!buddies)
			return;
		while(buddies) {
			b = buddies->data;
			g = purple_buddy_get_group(b);
			groups = g_slist_append(groups,
				(char *)jabber_roster_group_get_global_name(g));
			buddies = g_slist_remove(buddies, b);
		}

		tmp = roster_groups_join(groups);
		purple_debug_info("jabber", "jabber_roster_update(%s): [Source: local blist]: groups: %s\n",
		                  name, tmp);
		g_free(tmp);
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = purple_xmlnode_get_child(iq->node, "query");
	item = purple_xmlnode_new_child(query, "item");

	purple_xmlnode_set_attrib(item, "jid", name);

	balias = purple_buddy_get_local_alias(b);
	purple_xmlnode_set_attrib(item, "name", balias ? balias : "");

	for(l = groups; l; l = l->next) {
		group = purple_xmlnode_new_child(item, "group");
		purple_xmlnode_insert_data(group, l->data, -1);
	}

	g_slist_free(groups);

	if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER) {
		jabber_google_roster_outgoing(js, query, item);
		purple_xmlnode_set_attrib(query, "xmlns:gr", NS_GOOGLE_ROSTER);
		purple_xmlnode_set_attrib(query, "gr:ext", "2");
	}
	jabber_iq_send(iq);
}

void jabber_roster_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
		PurpleGroup *group, const char *message)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);
	char *who;
	JabberID *jid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	const char *name;

	/* If we haven't received the roster yet, ignore any adds */
	if (js->state != JABBER_STREAM_CONNECTED)
		return;

	name = purple_buddy_get_name(buddy);
	jid = jabber_id_new(name);
	if (jid == NULL) {
		/* TODO: Remove the buddy from the list? */
		return;
	}

	/* Adding a chat room or a chat user to the roster is *not* supported. */
	if (jid->node && jabber_chat_find(js, jid->node, jid->domain) != NULL) {
		/*
		 * This is the same thing Bonjour does. If it causes problems, move
		 * it to an idle callback.
		 */
		purple_debug_warning("jabber", "Cowardly refusing to add a MUC user "
		                     "to your buddy list and removing the buddy. "
		                     "Buddies can only be added by real (non-MUC) "
		                     "JID\n");
		purple_blist_remove_buddy(buddy);
		jabber_id_free(jid);
		return;
	}

	who = jabber_id_get_bare_jid(jid);
	if (jid->resource != NULL) {
		/*
		 * If the buddy name added contains a resource, strip that off and
		 * rename the buddy.
		 */
		purple_buddy_set_name(buddy, who);
	}

	jb = jabber_buddy_find(js, who, FALSE);

	purple_debug_info("jabber", "jabber_roster_add_buddy(): Adding %s\n", who);

	jabber_roster_update(js, who, NULL);

	if (jb == js->user_jb) {
		jabber_presence_fake_to_self(js, NULL);
	} else if(!jb || !(jb->subscription & JABBER_SUB_TO)) {
		jabber_presence_subscription_set(js, who, "subscribe");
	} else if((jbr =jabber_buddy_find_resource(jb, NULL))) {
		purple_prpl_got_user_status(purple_connection_get_account(gc), who,
				jabber_buddy_state_get_status_id(jbr->state),
				"priority", jbr->priority, jbr->status ? "message" : NULL, jbr->status, NULL);
	}

	g_free(who);
}

void jabber_roster_alias_change(PurpleConnection *gc, const char *name, const char *alias)
{
	PurpleBuddy *b = purple_blist_find_buddy(purple_connection_get_account(gc), name);

	if(b != NULL) {
		purple_buddy_set_local_alias(b, alias);

		purple_debug_info("jabber", "jabber_roster_alias_change(): Aliased %s to %s\n",
				name, alias ? alias : "(null)");

		jabber_roster_update(purple_connection_get_protocol_data(gc), name, NULL);
	}
}

void jabber_roster_group_change(PurpleConnection *gc, const char *name,
		const char *old_group, const char *new_group)
{
	GSList *buddies, *groups = NULL;
	PurpleBuddy *b;

	if(!old_group || !new_group || !strcmp(old_group, new_group))
		return;

	buddies = purple_blist_find_buddies(purple_connection_get_account(gc), name);
	while(buddies) {
		b = buddies->data;
		groups = g_slist_append(groups, (char*)new_group);
		buddies = g_slist_remove(buddies, b);
	}

	purple_debug_info("jabber", "jabber_roster_group_change(): Moving %s from %s to %s\n",
	                  name, old_group, new_group);

	jabber_roster_update(purple_connection_get_protocol_data(gc), name, groups);
}

void jabber_roster_group_rename(PurpleConnection *gc, const char *old_name,
		PurpleGroup *group, GList *moved_buddies)
{
	GList *l;
	const char *gname = jabber_roster_group_get_global_name(group);
	for(l = moved_buddies; l; l = l->next) {
		PurpleBuddy *buddy = l->data;
		jabber_roster_group_change(gc, purple_buddy_get_name(buddy), old_name, gname);
	}
}

void jabber_roster_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
		PurpleGroup *group) {
	const char *name = purple_buddy_get_name(buddy);
	GSList *buddies = purple_blist_find_buddies(purple_connection_get_account(gc), name);

	buddies = g_slist_remove(buddies, buddy);
	if(buddies != NULL) {
		PurpleBuddy *tmpbuddy;
		PurpleGroup *tmpgroup;
		GSList *groups = NULL;

		while(buddies) {
			tmpbuddy = buddies->data;
			tmpgroup = purple_buddy_get_group(tmpbuddy);
			groups = g_slist_append(groups,
				(char *)jabber_roster_group_get_global_name(tmpgroup));
			buddies = g_slist_remove(buddies, tmpbuddy);
		}

		purple_debug_info("jabber", "jabber_roster_remove_buddy(): "
			"Removing %s from %s", purple_buddy_get_name(buddy),
			jabber_roster_group_get_global_name(group));

		jabber_roster_update(purple_connection_get_protocol_data(gc), name, groups);
	} else {
		JabberIq *iq = jabber_iq_new_query(purple_connection_get_protocol_data(gc), JABBER_IQ_SET,
				"jabber:iq:roster");
		PurpleXmlNode *query = purple_xmlnode_get_child(iq->node, "query");
		PurpleXmlNode *item = purple_xmlnode_new_child(query, "item");

		purple_xmlnode_set_attrib(item, "jid", name);
		purple_xmlnode_set_attrib(item, "subscription", "remove");

		purple_debug_info("jabber", "jabber_roster_remove_buddy(): Removing %s\n",
		                  purple_buddy_get_name(buddy));

		jabber_iq_send(iq);
	}
}

const gchar *
jabber_roster_group_get_global_name(PurpleGroup *group)
{
	const gchar *name = NULL;

	if (group)
		name = purple_group_get_name(group);

	if (name == NULL)
		name = JABBER_ROSTER_DEFAULT_GROUP;
	else if (g_strcmp0(name, PURPLE_BLIST_DEFAULT_GROUP_NAME) == 0)
		name = JABBER_ROSTER_DEFAULT_GROUP;
	else if (g_strcmp0(name, _purple_blist_get_localized_default_group_name()) == 0)
		name = JABBER_ROSTER_DEFAULT_GROUP;

	return name;
}
