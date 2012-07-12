#include "roster.h"

#include "gg.h"
#include "xml.h"
#include "utils.h"

#include <debug.h>

#define GGP_ROSTER_SYNC_SETT "gg-synchronized"
#define GGP_ROSTER_ID_SETT "gg-id"
#define GGP_ROSTER_DEBUG 1
#define GGP_ROSTER_GROUP_DEFAULT _("Buddies")
#define GGP_ROSTER_GROUPID_DEFAULT "00000000-0000-0000-0000-000000000000"

/*
 TODO:

- remove_group
- auto-sync at startup
- group rename (w obie strony; rename_group)
- buddy removal

*/

typedef struct
{
	xmlnode *xml;
	
	xmlnode *groups_node, *contacts_node;
	
	/**
	 * Key: (uin_t) user identifier
	 * Value: (xmlnode*) xml node for contact
	 */
	GHashTable *contact_nodes;
	
	/**
	 * Key: (gchar*) group id
	 * Value: (xmlnode*) xml node for group
	 */
	GHashTable *group_nodes;

	gboolean needs_update;
} ggp_roster_content;

typedef struct
{
	enum
	{
		GGP_ROSTER_CHANGE_CONTACT_UPDATE,
		GGP_ROSTER_CHANGE_CONTACT_REMOVE,
	} type;
	union
	{
		uin_t uin;
	} data;
} ggp_roster_change;

static void ggp_roster_content_free(ggp_roster_content *content);
static void ggp_roster_change_free(gpointer change);

static gboolean ggp_roster_is_synchronized(PurpleBuddy *buddy);
static void ggp_roster_set_synchronized(PurpleConnection *gc, PurpleBuddy *buddy, gboolean synchronized);
static const gchar * ggp_roster_add_group(ggp_roster_content *content, PurpleGroup *group);

static gboolean ggp_roster_timer_cb(gpointer _gc);

static void ggp_roster_reply_ack(PurpleConnection *gc, uint32_t version);
static void ggp_roster_reply_list(PurpleConnection *gc, uint32_t version, const char *reply);
static void ggp_roster_send_update(PurpleConnection *gc);

#if GGP_ROSTER_DEBUG
static void ggp_roster_dump(ggp_roster_content *content);
#endif

static void ggp_roster_set_not_synchronized(PurpleConnection *gc, const char *who);

/********/

static inline ggp_roster_session_data *
ggp_roster_get_rdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return &accdata->roster_data;
} 

void ggp_roster_setup(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	rdata->version = 0;
	rdata->content = NULL;
	rdata->sent_updates = NULL;
	rdata->pending_updates = NULL;
	
	rdata->timer = purple_timeout_add_seconds(1, ggp_roster_timer_cb, gc); //TODO: 10s / check for value in original gg
}

void ggp_roster_cleanup(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	purple_timeout_remove(rdata->timer);
	ggp_roster_content_free(rdata->content);
	g_list_free_full(rdata->sent_updates, ggp_roster_change_free);
	g_list_free_full(rdata->pending_updates, ggp_roster_change_free);
}

static void ggp_roster_content_free(ggp_roster_content *content)
{
	if (content == NULL)
		return;
	if (content->xml)
		xmlnode_free(content->xml);
	if (content->contact_nodes)
		g_hash_table_destroy(content->contact_nodes);
	if (content->group_nodes)
		g_hash_table_destroy(content->group_nodes);
	g_free(content);
}

static void ggp_roster_change_free(gpointer _change)
{
	ggp_roster_change *change = _change;
	g_free(change);
}

static void ggp_roster_set_synchronized(PurpleConnection *gc, PurpleBuddy *buddy, gboolean synchronized)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
	ggp_roster_change *change;
	
	purple_debug_info("gg", "ggp_roster_set_synchronized [uin=%u, sync=%d]\n", uin, synchronized);
	
	purple_blist_node_set_bool(PURPLE_BLIST_NODE(buddy), GGP_ROSTER_SYNC_SETT, synchronized);
	if (!synchronized)
	{
		change = g_new(ggp_roster_change, 1);
		change->type = GGP_ROSTER_CHANGE_CONTACT_UPDATE;
		change->data.uin = uin;
		rdata->pending_updates = g_list_append(rdata->pending_updates, change);
	}
}

static gboolean ggp_roster_is_synchronized(PurpleBuddy *buddy)
{
	gboolean ret = purple_blist_node_get_bool(PURPLE_BLIST_NODE(buddy), GGP_ROSTER_SYNC_SETT);
	purple_debug_info("gg", "ggp_roster_is_synchronized [uin=%s, sync=%d]\n", purple_buddy_get_name(buddy), ret);
	return ret;
}

static gboolean ggp_roster_timer_cb(gpointer _gc)
{
	PurpleConnection *gc = _gc;
	
	g_return_val_if_fail(PURPLE_CONNECTION_IS_VALID(gc), FALSE);
	
	ggp_roster_send_update(gc);
	
	return TRUE;
}

void ggp_roster_update(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	
	purple_debug_info("gg", "ggp_roster_update [local: %u]\n", rdata->version);
	
	if (!gg_libgadu_check_feature(GG_LIBGADU_FEATURE_USERLIST100))
	{
		purple_debug_error("gg", "ggp_roster_update - feature disabled\n");
		return;
	}
	
	gg_userlist100_request(accdata->session, GG_USERLIST100_GET, rdata->version, GG_USERLIST100_FORMAT_TYPE_GG100, NULL);
}

void ggp_roster_reply(PurpleConnection *gc, struct gg_event_userlist100_reply *reply)
{
	purple_debug_info("gg", "ggp_roster_reply [type=%x, version=%u, format_type=%x]\n",
		reply->type, reply->version, reply->format_type);

	if (GG_USERLIST100_FORMAT_TYPE_GG100 != reply->format_type)
	{
		purple_debug_warning("gg", "ggp_roster_reply: unsupported format type (%x)\n", reply->format_type);
		return;
	}
	
	if (reply->type == GG_USERLIST100_REPLY_LIST)
		ggp_roster_reply_list(gc, reply->version, reply->reply);
	else if (reply->type == 0x01) // list up to date (TODO: push to libgadu)
		purple_debug_info("gg", "ggp_roster_reply: list up to date\n");
	else if (reply->type == GG_USERLIST100_REPLY_ACK)
		ggp_roster_reply_ack(gc, reply->version);
	else if (reply->type == GG_USERLIST100_REPLY_REJECT)
		purple_debug_error("gg", "ggp_roster_reply: not implemented (reject)\n");
	else
		purple_debug_error("gg", "ggp_roster_reply: unsupported reply (%x)\n", reply->type);
}

void ggp_roster_version(PurpleConnection *gc, struct gg_event_userlist100_version *version)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	int local_version = rdata->version;
	int remote_version = version->version;

	purple_debug_info("gg", "ggp_roster_version [local=%u, remote=%u]\n", local_version, remote_version);
	
	if (local_version < remote_version)
		ggp_roster_update(gc);
}

static void ggp_roster_reply_ack(PurpleConnection *gc, uint32_t version)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	ggp_roster_content *content = rdata->content;
	
	// set synchronization flag for all buddies, that were updated at roster
	GList *updates_it = g_list_first(rdata->sent_updates);
	while (updates_it)
	{
		ggp_roster_change *change = updates_it->data;
		PurpleBuddy *buddy;
		updates_it = g_list_next(updates_it);
		
		if (change->type != GGP_ROSTER_CHANGE_CONTACT_UPDATE)
			continue;
		
		buddy = purple_find_buddy(account, ggp_uin_to_str(change->data.uin));
		if (buddy)
			ggp_roster_set_synchronized(gc, buddy, TRUE);
	}
	
	// we need to remove "synchronized" flag for all contacts, that have
	// beed modified between roster update start and now
	updates_it = g_list_first(rdata->pending_updates);
	while (updates_it)
	{
		ggp_roster_change *change = updates_it->data;
		PurpleBuddy *buddy;
		updates_it = g_list_next(updates_it);
		
		if (change->type != GGP_ROSTER_CHANGE_CONTACT_UPDATE)
			continue;
		
		buddy = purple_find_buddy(account, ggp_uin_to_str(change->data.uin));
		if (buddy && ggp_roster_is_synchronized(buddy))
			ggp_roster_set_synchronized(gc, buddy, FALSE);
	}
	
	g_list_free_full(rdata->sent_updates, ggp_roster_change_free);
	rdata->sent_updates = NULL;
	
	// bump roster version or update it, if needed
	g_return_if_fail(content != NULL);
	if (content->needs_update)
	{
		ggp_roster_content_free(rdata->content);
		rdata->content = NULL;
		// we have to wait for gg_event_userlist100_version
		//ggp_roster_update(gc);
	}
	else
		rdata->version = version;
}

static void ggp_roster_reply_list(PurpleConnection *gc, uint32_t version, const char *data)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	xmlnode *xml, *curr_elem;
	GHashTable *groups;
	PurpleAccount *account;
	GSList *buddies;
	GHashTable *remove_buddies;
	GList *remove_buddies_list, *remove_buddies_it;
	GList *update_buddies = NULL, *update_buddies_it;
	ggp_roster_content *content;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(data != NULL);

	account = purple_connection_get_account(gc);

	xml = xmlnode_from_str(data, -1);
	if (xml == NULL)
	{
		purple_debug_warning("gg", "ggp_roster_reply_list: invalid xml\n");
		return;
	}

	ggp_roster_content_free(rdata->content);
	rdata->content = NULL;
	content = g_new0(ggp_roster_content, 1);
	content->xml = xml;
	content->contact_nodes = g_hash_table_new(NULL, NULL);
	content->group_nodes = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

#if GGP_ROSTER_DEBUG
	ggp_roster_dump(content);
#endif

	// reading groups

	content->groups_node = xmlnode_get_child(xml, "Groups");
	if (content->groups_node == NULL)
	{
		ggp_roster_content_free(content);
		g_return_if_reached();
	}

	groups = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	curr_elem = xmlnode_get_child(content->groups_node, "Group");
	while (curr_elem != NULL)
	{
		char *name, *id;
		gboolean removable;
		gboolean succ = TRUE;
		
		succ &= ggp_xml_get_string(curr_elem, "Id", &id);
		succ &= ggp_xml_get_string(curr_elem, "Name", &name);
		succ &= ggp_xml_get_bool(curr_elem, "IsRemovable", &removable);
		
		if (!succ)
		{
			g_free(id);
			g_free(name);
			g_hash_table_destroy(groups);
			ggp_roster_content_free(content);
			g_return_if_reached();
		}
		
		if (removable)
		{
			PurpleGroup *local_group;
			purple_debug_misc("gg", "ggp_roster_reply_list: group %s [id=%s]\n", name, id);
			
			//TODO: group rename - first find by id and maybe rename local; if not found, do the following
			local_group = purple_find_group(name);
			if (local_group)
				purple_blist_node_set_string(PURPLE_BLIST_NODE(local_group), GGP_ROSTER_ID_SETT, id);
			
			g_hash_table_insert(content->group_nodes, g_strdup(id), curr_elem);
			g_hash_table_insert(groups, id, name);
		}
		else
		{
			g_free(id);
			g_free(name);
		}
		
		curr_elem = xmlnode_get_next_twin(curr_elem);
	}
	
	// dumping current buddy list
	// we will:
	// - remove synchronized ones, if not found in list at server
	// - upload not synchronized ones
	
	buddies = purple_find_buddies(account, NULL);
	remove_buddies = g_hash_table_new(g_direct_hash, g_direct_equal);
	while (buddies)
	{
		PurpleBuddy *buddy = buddies->data;
		uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
		
		if (uin)
		{
			if (ggp_roster_is_synchronized(buddy))
				g_hash_table_insert(remove_buddies, GINT_TO_POINTER(uin), buddy);
			else
				update_buddies = g_list_append(update_buddies, buddy);
		}

		buddies = g_slist_delete_link(buddies, buddies);
	}
	
	// reading buddies
	
	content->contacts_node = xmlnode_get_child(xml, "Contacts");
	if (content->contacts_node == NULL)
	{
		g_hash_table_destroy(groups);
		g_hash_table_destroy(remove_buddies);
		g_list_free(update_buddies);
		ggp_roster_content_free(content);
		g_return_if_reached();
	}
	
	curr_elem = xmlnode_get_child(content->contacts_node, "Contact");
	while (curr_elem != NULL)
	{
		gchar *alias, *group_name;
		uin_t uin;
		gboolean isbot;
		gboolean succ = TRUE;
		xmlnode *group_list, *group_elem;
		PurpleBuddy *buddy = NULL;
		PurpleGroup *group = NULL;
		
		succ &= ggp_xml_get_string(curr_elem, "ShowName", &alias);
		succ &= ggp_xml_get_uint(curr_elem, "GGNumber", &uin);
		
		group_list = xmlnode_get_child(curr_elem, "Groups");
		succ &= (group_list != NULL);
		
		if (!succ)
		{
			g_free(alias);
			g_hash_table_destroy(groups);
			g_hash_table_destroy(remove_buddies);
			g_list_free(update_buddies);
			ggp_roster_content_free(content);
			g_return_if_reached();
		}
		
		g_hash_table_insert(content->contact_nodes, GINT_TO_POINTER(uin), curr_elem);
		
		// looking up for group name
		group_elem = xmlnode_get_child(group_list, "GroupId");
		while (group_elem != NULL)
		{
			gchar *id;
			if (!ggp_xml_get_string(group_elem, NULL, &id))
				continue;
			group_name = g_hash_table_lookup(groups, id);
			isbot = (strcmp(id, "0b345af6-0001-0000-0000-000000000004") == 0 ||
				g_strcmp0(group_name, "Pomocnicy") == 0);
			g_free(id);
			
			if (isbot)
				break;
			
			if (group_name != NULL)
				break;
			
			group_elem = xmlnode_get_next_twin(group_elem);
		}
		
		// we don't want to import bots;
		// they are inserted to roster by default
		if (isbot)
		{
			g_free(alias);
			curr_elem = xmlnode_get_next_twin(curr_elem);
			continue;
		}
		
		if (strlen(alias) == 0 ||
			strcmp(alias, ggp_uin_to_str(uin)) == 0)
		{
			g_free(alias);
			alias = NULL;
		}
		
		purple_debug_misc("gg", "ggp_roster_reply_list: user [uin=%u, alias=\"%s\", group=\"%s\"]\n",
			uin, alias, group_name);
		
		if (group_name)
		{
			group = purple_find_group(group_name);
			if (!group)
			{
				group = purple_group_new(group_name);
				purple_blist_add_group(group, NULL);
			}
		}
		
		buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
		g_hash_table_remove(remove_buddies, GINT_TO_POINTER(uin));
		if (buddy)
		{
			PurpleGroup *currentGroup;
			
			// local list has priority
			if (!ggp_roster_is_synchronized(buddy))
			{
				purple_debug_info("gg", "ggp_roster_reply_list: ignoring not synchronized %s\n", purple_buddy_get_name(buddy));
				g_free(alias);
				curr_elem = xmlnode_get_next_twin(curr_elem);
				continue;
			}
			
			purple_debug_misc("gg", "ggp_roster_reply_list: updating %s\n", purple_buddy_get_name(buddy));
			purple_blist_alias_buddy(buddy, alias);
			currentGroup = purple_buddy_get_group(buddy);
			if (currentGroup != group)
				purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		else
		{
			purple_debug_info("gg", "ggp_roster_reply_list: adding %u to buddy list\n", uin);
			buddy = purple_buddy_new(account, ggp_uin_to_str(uin), alias);
			purple_blist_add_buddy(buddy, NULL, group, NULL);
			ggp_roster_set_synchronized(gc, buddy, TRUE);
		}
		
		g_free(alias);
		curr_elem = xmlnode_get_next_twin(curr_elem);
	}
	
	g_hash_table_destroy(groups);
	
	// removing buddies, which are not present in roster
	remove_buddies_list = g_hash_table_get_values(remove_buddies);
	remove_buddies_it = g_list_first(remove_buddies_list);
	while (remove_buddies_it)
	{
		PurpleBuddy *buddy = remove_buddies_it->data;
		if (ggp_roster_is_synchronized(buddy))
		{
			purple_debug_info("gg", "ggp_roster_reply_list: removing %s from buddy list\n", purple_buddy_get_name(buddy));
			purple_blist_remove_buddy(buddy);
		}
		remove_buddies_it = g_list_next(remove_buddies_it);
	}
	g_list_free(remove_buddies_list);
	g_hash_table_destroy(remove_buddies);
	
	update_buddies_it = g_list_first(update_buddies);
	while (update_buddies_it)
	{
		PurpleBuddy *buddy = update_buddies_it->data;
		uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
		ggp_roster_change *change;
		
		g_assert(uin > 0);
		
		purple_debug_info("gg", "ggp_roster_reply_list: adding change of %u for roster\n", uin);
		change = g_new(ggp_roster_change, 1);
		change->type = GGP_ROSTER_CHANGE_CONTACT_UPDATE;
		change->data.uin = uin;
		rdata->pending_updates = g_list_append(rdata->pending_updates, change);
		
		update_buddies_it = g_list_next(update_buddies_it);
	}
	g_list_free(update_buddies);

	rdata->content = content;
	rdata->version = version;
}

static const gchar * ggp_roster_add_group(ggp_roster_content *content, PurpleGroup *group)
{
	gchar *id_dyn;
	const char *id_existing;
	static gchar id[40];
	xmlnode *group_node;
	gboolean succ = TRUE;

	if (group && 0 == strcmp(GGP_ROSTER_GROUP_DEFAULT, purple_group_get_name(group)))
		group = NULL;
	if (group)
		id_existing = purple_blist_node_get_string(PURPLE_BLIST_NODE(group), GGP_ROSTER_ID_SETT);
	else
		id_existing = GGP_ROSTER_GROUPID_DEFAULT;
	if (id_existing)
		return id_existing;

	id_dyn = purple_uuid_random();
	g_snprintf(id, sizeof(id), "%s", id_dyn);
	g_free(id_dyn);
	
	group_node = xmlnode_new_child(content->groups_node, "Group");
	succ &= ggp_xml_set_string(group_node, "Id", id);
	succ &= ggp_xml_set_string(group_node, "Name", purple_group_get_name(group));
	succ &= ggp_xml_set_string(group_node, "IsExpanded", "true");
	succ &= ggp_xml_set_string(group_node, "IsRemovable", "true");
	content->needs_update = TRUE;
	
	g_return_val_if_fail(succ, NULL);

	return id;
}

static void ggp_roster_send_update(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	ggp_roster_content *content = rdata->content;
	GList *updates_it;
	gchar *str;
	int len;
	
	if (rdata->sent_updates)
	{
		purple_debug_misc("gg", "ggp_roster_send_update: an update is running now\n");
		return;
	}

	if (!rdata->pending_updates)
	{
		purple_debug_misc("gg", "ggp_roster_send_update: no pending updates found\n");
		return;
	}
	
	if (!content)
	{
		purple_debug_misc("gg", "ggp_roster_send_update: not initialized\n");
		return;
	}
	
	purple_debug_info("gg", "ggp_roster_send_update: pending updates found\n");
	
	rdata->sent_updates = rdata->pending_updates;
	rdata->pending_updates = NULL;
	
	updates_it = g_list_first(rdata->sent_updates);
	while (updates_it)
	{
		ggp_roster_change *change = updates_it->data;
		uin_t uin = change->data.uin;
		PurpleBuddy *buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
		xmlnode *buddy_node = g_hash_table_lookup(content->contact_nodes, GINT_TO_POINTER(uin));
		updates_it = g_list_next(updates_it);
		
		if (change->type == GGP_ROSTER_CHANGE_CONTACT_UPDATE && !buddy)
			continue;
		
		if (change->type == GGP_ROSTER_CHANGE_CONTACT_UPDATE && buddy_node)
		{ // update existing
			xmlnode *contact_groups;
			gboolean succ = TRUE;
			
			purple_debug_misc("gg", "ggp_roster_send_update: updating %u...\n", uin);
			
			succ &= ggp_xml_set_string(buddy_node, "ShowName", purple_buddy_get_alias(buddy));
			
			contact_groups = xmlnode_get_child(buddy_node, "Groups");
			g_assert(contact_groups);
			ggp_xmlnode_remove_children(contact_groups);
			succ &= ggp_xml_set_string(contact_groups, "GroupId", ggp_roster_add_group(content, purple_buddy_get_group(buddy)));
		
			g_return_if_fail(succ);
			
			continue;
		}
		
		if (change->type == GGP_ROSTER_CHANGE_CONTACT_UPDATE && !buddy_node)
		{ // add new
			xmlnode *contact_groups;
			gboolean succ = TRUE;
		
			buddy_node = xmlnode_new_child(content->contacts_node, "Contact");
			succ &= ggp_xml_set_string(buddy_node, "Guid", purple_uuid_random());
			succ &= ggp_xml_set_uint(buddy_node, "GGNumber", uin);
			succ &= ggp_xml_set_string(buddy_node, "ShowName", purple_buddy_get_alias(buddy));
			
			contact_groups = xmlnode_new_child(buddy_node, "Groups");
			g_assert(contact_groups);
			succ &= ggp_xml_set_string(contact_groups, "GroupId", ggp_roster_add_group(content, purple_buddy_get_group(buddy)));
			
			xmlnode_new_child(buddy_node, "Avatars");
			succ &= ggp_xml_set_bool(buddy_node, "FlagBuddy", TRUE);
			succ &= ggp_xml_set_bool(buddy_node, "FlagNormal", TRUE);
			succ &= ggp_xml_set_bool(buddy_node, "FlagFriend", TRUE);
			
			// we don't use Guid, so update is not needed
			//content->needs_update = TRUE;
			
			g_hash_table_insert(content->contact_nodes, GINT_TO_POINTER(uin), buddy_node);
			
			g_return_if_fail(succ);
			
			continue;
		}

		if (change->type == GGP_ROSTER_CHANGE_CONTACT_REMOVE && buddy)
		{
			purple_debug_info("gg", "ggp_roster_send_update: contact %u re-added\n", uin);
			continue;
		}

		if (change->type == GGP_ROSTER_CHANGE_CONTACT_REMOVE && !buddy_node)
			continue; // already removed
		
		if (change->type == GGP_ROSTER_CHANGE_CONTACT_REMOVE && buddy_node)
		{
			purple_debug_error("gg", "ggp_roster_send_update: TODO: not implemented (remove)\n");
			continue;
		}
		
		purple_debug_fatal("gg", "ggp_roster_send_update: not handled\n");
	}
	
	ggp_roster_dump(content);
	
	str = xmlnode_to_str(content->xml, &len);
	gg_userlist100_request(accdata->session, GG_USERLIST100_PUT, rdata->version, GG_USERLIST100_FORMAT_TYPE_GG100, str);
	g_free(str);
}

#if GGP_ROSTER_DEBUG
static void ggp_roster_dump(ggp_roster_content *content)
{
	char *str;
	int len;
	
	g_return_if_fail(content != NULL);
	g_return_if_fail(content->xml != NULL);
	
	str = xmlnode_to_formatted_str(content->xml, &len);
	purple_debug_misc("gg", "ggp_roster_reply_list: [%s]\n", str);
	g_free(str);
}
#endif

static void ggp_roster_set_not_synchronized(PurpleConnection *gc, const char *who)
{
	PurpleBuddy *buddy;
	
	g_return_if_fail(who != NULL);
	
	buddy = purple_find_buddy(purple_connection_get_account(gc), who);
	g_return_if_fail(buddy != NULL);
	
	ggp_roster_set_synchronized(gc, buddy, FALSE);
}

void ggp_roster_alias_buddy(PurpleConnection *gc, const char *who, const char *alias)
{
	ggp_roster_set_not_synchronized(gc, who);
}

void ggp_roster_group_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group)
{
	ggp_roster_set_not_synchronized(gc, who);
}
