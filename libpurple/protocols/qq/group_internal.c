/**
 * @file group_internal.c
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

#include "internal.h"
#include "blist.h"
#include "debug.h"

#include "buddy_opt.h"
#include "group_free.h"
#include "group_internal.h"
#include "utils.h"

static gchar *_qq_group_set_my_status_desc(qq_group *group)
{
	const char *status_desc;
	g_return_val_if_fail(group != NULL, g_strdup(""));

	switch (group->my_status) {
	case QQ_GROUP_MEMBER_STATUS_NOT_MEMBER:
		status_desc = _("I am not a member");
		break;
	case QQ_GROUP_MEMBER_STATUS_IS_MEMBER:
		status_desc = _("I am a member");
		break;
	case QQ_GROUP_MEMBER_STATUS_APPLYING:
		status_desc = _("I am applying to join");
		break;
	case QQ_GROUP_MEMBER_STATUS_IS_ADMIN:
		status_desc = _("I am the admin");
		break;
	default:
		status_desc = _("Unknown status");
	}

	return g_strdup(status_desc);
}

static void _qq_group_add_to_blist(PurpleConnection *gc, qq_group *group)
{
	GHashTable *components;
	PurpleGroup *g;
	PurpleChat *chat;
	components = qq_group_to_hashtable(group);
	chat = purple_chat_new(purple_connection_get_account(gc), group->group_name_utf8, components);
	g = qq_get_purple_group(PURPLE_GROUP_QQ_QUN);
	purple_blist_add_chat(chat, g, NULL);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "You have added group \"%s\" to blist locally\n", group->group_name_utf8);
}

/* Create a dummy qq_group, which includes only internal_id, ext_id,
 * and potentially group_name_utf8, in case we need to call group_conv_show_window
 * right after creation. All other attributes are set to empty.
 * We need to send a get_group_info to the QQ server to update it right away */
qq_group *qq_group_create_internal_record(PurpleConnection *gc,
                guint32 internal_id, guint32 ext_id, gchar *group_name_utf8)
{
        qq_group *group;
        qq_data *qd;

        g_return_val_if_fail(internal_id > 0, NULL);
        qd = (qq_data *) gc->proto_data;

        group = g_new0(qq_group, 1);
        group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
        group->my_status_desc = _qq_group_set_my_status_desc(group);
        group->id = internal_id;
        group->ext_id = ext_id;
        group->type8 = 0x01;       /* assume permanent Qun */
        group->creator_uid = 10000;     /* assume by QQ admin */
        group->group_category = 0x01;
        group->auth_type = 0x02;        /* assume need auth */
        group->group_name_utf8 = g_strdup(group_name_utf8 == NULL ? "" : group_name_utf8);
        group->group_desc_utf8 = g_strdup("");
        group->notice_utf8 = g_strdup("");
        group->members = NULL;

        qd->groups = g_list_append(qd->groups, group);
        _qq_group_add_to_blist(gc, group);

        return group;
}

void qq_group_delete_internal_record(qq_data *qd, guint32 id)
{
        qq_group *group;
        GList *list;

        list = qd->groups;
        while (list != NULL) {
                group = (qq_group *) qd->groups->data;
                if (id == group->id) {
                        qd->groups = g_list_remove(qd->groups, group);
                        qq_group_free(group);
                        break;
                } else {
                        list = list->next;
                }
        }
}

/* convert a qq_group to hash-table, which could be component of PurpleChat */
GHashTable *qq_group_to_hashtable(qq_group *group)
{
	GHashTable *components;
	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_MEMBER_STATUS), g_strdup_printf("%d", group->my_status));
	group->my_status_desc = _qq_group_set_my_status_desc(group);

	g_hash_table_insert(components,
			    g_strdup(QQ_GROUP_KEY_INTERNAL_ID), g_strdup_printf("%d", group->id));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_EXTERNAL_ID),
			    g_strdup_printf("%d", group->ext_id));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_TYPE), g_strdup_printf("%d", group->type8));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_CREATOR_UID), g_strdup_printf("%d", group->creator_uid));
	g_hash_table_insert(components,
			    g_strdup(QQ_GROUP_KEY_GROUP_CATEGORY), g_strdup_printf("%d", group->group_category));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_AUTH_TYPE), g_strdup_printf("%d", group->auth_type));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_MEMBER_STATUS_DESC), g_strdup(group->my_status_desc));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_GROUP_NAME_UTF8), g_strdup(group->group_name_utf8));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_GROUP_DESC_UTF8), g_strdup(group->group_desc_utf8));
	return components;
}

/* create a qq_group from hashtable */
qq_group *qq_group_from_hashtable(PurpleConnection *gc, GHashTable *data)
{
	qq_data *qd;
	qq_group *group;

	g_return_val_if_fail(data != NULL, NULL);
	qd = (qq_data *) gc->proto_data;

	group = g_new0(qq_group, 1);
	group->my_status =
	    qq_string_to_dec_value
	    (NULL ==
	     g_hash_table_lookup(data,
				 QQ_GROUP_KEY_MEMBER_STATUS) ?
	     g_strdup_printf("%d", QQ_GROUP_MEMBER_STATUS_NOT_MEMBER) :
	     g_hash_table_lookup(data, QQ_GROUP_KEY_MEMBER_STATUS));
	group->id = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_INTERNAL_ID));
	group->ext_id = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_EXTERNAL_ID));
	group->type8 = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_TYPE));
	group->creator_uid = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_CREATOR_UID));
	group->group_category = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_CATEGORY));
	group->auth_type = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_AUTH_TYPE));
	group->group_name_utf8 = g_strdup(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_NAME_UTF8));
	group->group_desc_utf8 = g_strdup(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_DESC_UTF8));
	group->my_status_desc = _qq_group_set_my_status_desc(group);

	qd->groups = g_list_append(qd->groups, group);

	return group;
}

/* refresh group local subscription */
void qq_group_refresh(PurpleConnection *gc, qq_group *group)
{
	PurpleChat *chat;
	gchar *ext_id;
	g_return_if_fail(group != NULL);

	ext_id = g_strdup_printf("%d", group->ext_id);
	chat = purple_blist_find_chat(purple_connection_get_account(gc), ext_id);
	g_free(ext_id);
	if (chat == NULL && group->my_status != QQ_GROUP_MEMBER_STATUS_NOT_MEMBER) {
		_qq_group_add_to_blist(gc, group);
	} else if (chat != NULL) {	/* we have a local record, update its info */
		/* if there is group_name_utf8, we update the group name */
		if (group->group_name_utf8 != NULL && strlen(group->group_name_utf8) > 0)
			purple_blist_alias_chat(chat, group->group_name_utf8);
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_MEMBER_STATUS), g_strdup_printf("%d", group->my_status));
		group->my_status_desc = _qq_group_set_my_status_desc(group);
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_MEMBER_STATUS_DESC), g_strdup(group->my_status_desc));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_INTERNAL_ID),
				     g_strdup_printf("%d", group->id));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_EXTERNAL_ID),
				     g_strdup_printf("%d", group->ext_id));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_TYPE), g_strdup_printf("%d", group->type8));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_CREATOR_UID), g_strdup_printf("%d", group->creator_uid));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_CATEGORY),
				     g_strdup_printf("%d", group->group_category));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_AUTH_TYPE), g_strdup_printf("%d", group->auth_type));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_NAME_UTF8), g_strdup(group->group_name_utf8));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_DESC_UTF8), g_strdup(group->group_desc_utf8));
	}
}

/* NOTE: If we knew how to convert between an external and internal group id, as the official 
 * client seems to, the following would be unnecessary. That would be ideal. */

/* Use list to specify if id's alternate id is pending discovery. */
void qq_set_pending_id(GSList **list, guint32 id, gboolean pending)
{
	if (pending) 
		*list = g_slist_prepend(*list, GINT_TO_POINTER(id));
	else 
		*list = g_slist_remove(*list, GINT_TO_POINTER(id));
}

/* Return the location of id in list, or NULL if not found */
GSList *qq_get_pending_id(GSList *list, guint32 id)
{
        return g_slist_find(list, GINT_TO_POINTER(id));
}
