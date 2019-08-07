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

#include "group.h"
#include "internal.h" /* TODO: we need to kill this */

typedef struct _PurpleGroupPrivate      PurpleGroupPrivate;

/******************************************************************************
 * Private Data
 *****************************************************************************/
struct _PurpleGroupPrivate {
	char *name;               /* The name of this group.                    */
	gboolean is_constructed;  /* Indicates if the group has finished being
	                             constructed.                               */
};

/* Group property enums */
enum
{
	GROUP_PROP_0,
	GROUP_PROP_NAME,
	GROUP_PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GParamSpec *properties[GROUP_PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE(PurpleGroup, purple_group,
		PURPLE_TYPE_COUNTING_NODE);

/******************************************************************************
 * Group API
 *****************************************************************************/
GSList *purple_group_get_accounts(PurpleGroup *group) {
	GSList *l = NULL;
	PurpleBlistNode *gnode, *cnode, *bnode;

	gnode = (PurpleBlistNode *)group;

	for (cnode = gnode->child;  cnode; cnode = cnode->next) {
		if (PURPLE_IS_CHAT(cnode)) {
			if (!g_slist_find(l, purple_chat_get_account(PURPLE_CHAT(cnode))))
				l = g_slist_append(l, purple_chat_get_account(PURPLE_CHAT(cnode)));
		} else if (PURPLE_IS_CONTACT(cnode)) {
			for (bnode = cnode->child; bnode; bnode = bnode->next) {
				if (PURPLE_IS_BUDDY(bnode)) {
					if (!g_slist_find(l, purple_buddy_get_account(PURPLE_BUDDY(bnode))))
						l = g_slist_append(l, purple_buddy_get_account(PURPLE_BUDDY(bnode)));
				}
			}
		}
	}

	return l;
}

gboolean purple_group_on_account(PurpleGroup *g, PurpleAccount *account) {
	PurpleBlistNode *cnode;
	for (cnode = ((PurpleBlistNode *)g)->child; cnode; cnode = cnode->next) {
		if (PURPLE_IS_CONTACT(cnode)) {
			if(purple_contact_on_account((PurpleContact *) cnode, account))
				return TRUE;
		} else if (PURPLE_IS_CHAT(cnode)) {
			PurpleChat *chat = (PurpleChat *)cnode;
			if ((!account && purple_account_is_connected(purple_chat_get_account(chat)))
					|| purple_chat_get_account(chat) == account)
				return TRUE;
		}
	}
	return FALSE;
}

/*
 * TODO: If merging, prompt the user if they want to merge.
 */
void purple_group_set_name(PurpleGroup *source, const char *name) {
	PurpleGroupPrivate *priv = NULL;
	PurpleGroup *dest;
	gchar *old_name;
	gchar *new_name;
	GList *moved_buddies = NULL;
	GSList *accts;

	g_return_if_fail(PURPLE_IS_GROUP(source));
	g_return_if_fail(name != NULL);

	priv = purple_group_get_instance_private(source);

	new_name = purple_utf8_strip_unprintables(name);

	if (*new_name == '\0' || purple_strequal(new_name, priv->name)) {
		g_free(new_name);
		return;
	}

	dest = purple_blist_find_group(new_name);
	if (dest != NULL && purple_utf8_strcasecmp(priv->name,
			purple_group_get_name(dest)) != 0) {
		/* We're merging two groups */
		PurpleBlistNode *prev, *child, *next;

		prev = _purple_blist_get_last_child((PurpleBlistNode*)dest);
		child = PURPLE_BLIST_NODE(source)->child;

		/*
		 * TODO: This seems like a dumb way to do this... why not just
		 * append all children from the old group to the end of the new
		 * one?  Protocols might be expecting to receive an add_buddy() for
		 * each moved buddy...
		 */
		while (child)
		{
			next = child->next;
			if (PURPLE_IS_CONTACT(child)) {
				PurpleBlistNode *bnode;
				purple_blist_add_contact((PurpleContact *)child, dest, prev);
				for (bnode = child->child; bnode != NULL; bnode = bnode->next) {
					purple_blist_add_buddy((PurpleBuddy *)bnode, (PurpleContact *)child,
							NULL, bnode->prev);
					moved_buddies = g_list_append(moved_buddies, bnode);
				}
				prev = child;
			} else if (PURPLE_IS_CHAT(child)) {
				purple_blist_add_chat((PurpleChat *)child, dest, prev);
				prev = child;
			} else {
				purple_debug(PURPLE_DEBUG_ERROR, "blistnodetypes",
						"Unknown child type in group %s\n", priv->name);
			}
			child = next;
		}

		/* Make a copy of the old group name and then delete the old group */
		old_name = g_strdup(priv->name);
		purple_blist_remove_group(source);
		source = dest;
		g_free(new_name);
	} else {
		/* A simple rename */
		PurpleBlistNode *cnode, *bnode;

		/* Build a GList of all buddies in this group */
		for (cnode = PURPLE_BLIST_NODE(source)->child; cnode != NULL; cnode = cnode->next) {
			if (PURPLE_IS_CONTACT(cnode))
				for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
					moved_buddies = g_list_append(moved_buddies, bnode);
		}

		purple_blist_update_groups_cache(source, new_name);

		old_name = priv->name;
		priv->name = new_name;

		g_object_notify_by_pspec(G_OBJECT(source), properties[GROUP_PROP_NAME]);
	}

	/* Save our changes */
	purple_blist_save_node(purple_blist_get_default(),
	                       PURPLE_BLIST_NODE(source));

	/* Update the UI */
	purple_blist_update_node(purple_blist_get_default(),
	                         PURPLE_BLIST_NODE(source));

	/* Notify all protocols */
	/* TODO: Is this condition needed?  Seems like it would always be TRUE */
	if(old_name && !purple_strequal(priv->name, old_name)) {
		for (accts = purple_group_get_accounts(source); accts; accts = g_slist_remove(accts, accts->data)) {
			PurpleAccount *account = accts->data;
			PurpleConnection *gc = NULL;
			PurpleProtocol *protocol = NULL;
			GList *l = NULL, *buddies = NULL;

			gc = purple_account_get_connection(account);

			if(gc)
				protocol = purple_connection_get_protocol(gc);

			if(!protocol)
				continue;

			for(l = moved_buddies; l; l = l->next) {
				PurpleBuddy *buddy = PURPLE_BUDDY(l->data);

				if(buddy && purple_buddy_get_account(buddy) == account)
					buddies = g_list_append(buddies, (PurpleBlistNode *)buddy);
			}

			if(PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, rename_group)) {
				purple_protocol_server_iface_rename_group(protocol, gc, old_name, source, buddies);
			} else {
				GList *cur, *groups = NULL;

				/* Make a list of what the groups each buddy is in */
				for(cur = buddies; cur; cur = cur->next) {
					PurpleBlistNode *node = (PurpleBlistNode *)cur->data;
					groups = g_list_prepend(groups, node->parent->parent);
				}

				purple_account_remove_buddies(account, buddies, groups);
				g_list_free(groups);
				purple_account_add_buddies(account, buddies, NULL);
			}

			g_list_free(buddies);
		}
	}
	g_list_free(moved_buddies);
	g_free(old_name);

	g_object_notify_by_pspec(G_OBJECT(source), properties[GROUP_PROP_NAME]);
}

const char *purple_group_get_name(PurpleGroup *group) {
	PurpleGroupPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_GROUP(group), NULL);

	priv = purple_group_get_instance_private(group);
	return priv->name;
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
/* Set method for GObject properties */
static void
purple_group_set_property(GObject *obj, guint param_id, const GValue *value,
                          GParamSpec *pspec)
{
	PurpleGroup *group = PURPLE_GROUP(obj);
	PurpleGroupPrivate *priv = purple_group_get_instance_private(group);

	switch (param_id) {
		case GROUP_PROP_NAME:
			if (priv->is_constructed)
				purple_group_set_name(group, g_value_get_string(value));
			else
				priv->name =
					purple_utf8_strip_unprintables(g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_group_get_property(GObject *obj, guint param_id, GValue *value,
                          GParamSpec *pspec)
{
	PurpleGroup *group = PURPLE_GROUP(obj);

	switch (param_id) {
		case GROUP_PROP_NAME:
			g_value_set_string(value, purple_group_get_name(group));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_group_init(PurpleGroup *group) {
}

/* Called when done constructing */
static void
purple_group_constructed(GObject *object) {
	PurpleGroup *group = PURPLE_GROUP(object);
	PurpleGroupPrivate *priv = purple_group_get_instance_private(group);

	G_OBJECT_CLASS(purple_group_parent_class)->constructed(object);

	purple_blist_new_node(purple_blist_get_default(),
	                      PURPLE_BLIST_NODE(group));

	priv->is_constructed = TRUE;
}

/* GObject finalize function */
static void
purple_group_finalize(GObject *object) {
	PurpleGroupPrivate *priv = purple_group_get_instance_private(
			PURPLE_GROUP(object));

	g_free(priv->name);

	G_OBJECT_CLASS(purple_group_parent_class)->finalize(object);
}

/* Class initializer function */
static void
purple_group_class_init(PurpleGroupClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_group_finalize;
	obj_class->constructed = purple_group_constructed;

	/* Setup properties */
	obj_class->get_property = purple_group_get_property;
	obj_class->set_property = purple_group_set_property;

	properties[GROUP_PROP_NAME] = g_param_spec_string(
		"name",
		"Name",
		"Name of the group.",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS
	);

	g_object_class_install_properties(obj_class, GROUP_PROP_LAST, properties);
}

PurpleGroup *
purple_group_new(const char *name) {
	PurpleGroup *group;

	if (name == NULL || name[0] == '\0')
		name = PURPLE_BLIST_DEFAULT_GROUP_NAME;
	if (g_strcmp0(name, "Buddies") == 0)
		name = PURPLE_BLIST_DEFAULT_GROUP_NAME;
	if (g_strcmp0(name, _purple_blist_get_localized_default_group_name()) == 0)
		name = PURPLE_BLIST_DEFAULT_GROUP_NAME;

	group = purple_blist_find_group(name);
	if (group != NULL)
		return group;

	return g_object_new(PURPLE_TYPE_GROUP, "name", name, NULL);
}

