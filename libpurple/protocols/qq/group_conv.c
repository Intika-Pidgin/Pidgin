/**
 * @file group_conv.c
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

#include <glib.h>
#include "qq.h"

#include "conversation.h"

#include "buddy_status.h"
#include "group_conv.h"
#include "utils.h"

/* show group conversation window */
void qq_group_conv_show_window(PurpleConnection *gc, qq_group *group)
{
	PurpleConversation *conv;
	qq_data *qd;

	g_return_if_fail(group != NULL);
	qd = (qq_data *) gc->proto_data;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, 
			group->group_name_utf8, purple_connection_get_account(gc));
	if (conv == NULL)	/* show only one window per group */
		serv_got_joined_chat(gc, qd->channel++, group->group_name_utf8);
}

/* refresh online member in group conversation window */
void qq_group_conv_refresh_online_member(PurpleConnection *gc, qq_group *group)
{
	GList *names, *list, *flags;
	qq_buddy *member;
	gchar *member_name, *member_uid;
	PurpleConversation *conv;
	gint flag;
	g_return_if_fail(group != NULL);

	names = NULL;
	flags = NULL;
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
			group->group_name_utf8, purple_connection_get_account(gc));
	if (conv != NULL && group->members != NULL) {
		list = group->members;
		while (list != NULL) {
			member = (qq_buddy *) list->data;

			/* we need unique identifiers for everyone in the chat or else we'll
			 * run into problems with functions like get_cb_real_name from qq.c */
			member_name =   (member->nickname != NULL && *(member->nickname) != '\0') ?
					g_strdup_printf("%s (qq-%u)", member->nickname, member->uid) :
					g_strdup_printf("(qq-%u)", member->uid);
			member_uid = g_strdup_printf("(qq-%u)", member->uid);

			flag = 0;
			/* TYPING to put online above OP and FOUNDER */
			if (is_online(member->status))
				flag |= (PURPLE_CBFLAGS_TYPING | PURPLE_CBFLAGS_VOICE);
			if(1 == (member->role & 1)) flag |= PURPLE_CBFLAGS_OP;
			if(member->uid == group->creator_uid) flag |= PURPLE_CBFLAGS_FOUNDER;

			if (purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), member_name))
			{
				purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(conv),
						member_name,
						flag);
			} else if (purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), member_uid))
			{
				purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(conv),
						member_uid,
						flag);
				purple_conv_chat_rename_user(PURPLE_CONV_CHAT(conv), member_uid, member_name);
			} else {
				/* always put it even offline */
				names = g_list_append(names, member_name);
				flags = g_list_append(flags, GINT_TO_POINTER(flag));
			}
			g_free(member_uid);
			list = list->next;
		}

		purple_conv_chat_add_users(PURPLE_CONV_CHAT(conv), names, NULL, flags, FALSE);
	}
	/* clean up names */
	while (names != NULL) {
		member_name = (gchar *) names->data;
		names = g_list_remove(names, member_name);
		g_free(member_name);
	}
	g_list_free(flags);
}
