/* purple
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

#ifndef PURPLE_BUDDY_LIST_H
#define PURPLE_BUDDY_LIST_H

/**
 * SECTION:buddylist
 * @section_id: libpurple-buddylist
 * @short_description: <filename>buddylist.h</filename>
 * @title: Buddy List API
 * @see_also: <link linkend="chapter-signals-blist">Buddy List signals</link>
 */

/* I can't believe I let ChipX86 inspire me to write good code. -Sean */

#include "buddy.h"

#define PURPLE_TYPE_BUDDY_LIST (purple_buddy_list_get_type())
typedef struct _PurpleBuddyList PurpleBuddyList;

#define PURPLE_BLIST_DEFAULT_GROUP_NAME _("Buddies")

#include "chat.h"

/**
 * PurpleBlistWalkFunc:
 * @node: The node that's being iterated
 * @data: User supplied data.
 *
 * A callback function for purple_blist_walk.
 *
 * Since: 3.0.0
 */
typedef void (*PurpleBlistWalkFunc)(PurpleBlistNode *node, gpointer data);

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/
/**
 * PurpleBuddyList:
 *
 * The Buddy List
 */
/**
 * PurpleBuddyListClass:
 * @new_node:     Sets UI-specific data on a node.
 * @show:         The core will call this when it's finished doing its core
 *                stuff.
 * @update:       This will update a node in the buddy list.
 * @remove:       This removes a node from the list
 * @set_visible:  Hides or unhides the buddy list.
 * @request_add_buddy: Called when information is needed to add a buddy to the
 *                     buddy list. See purple_blist_request_add_buddy().
 * @request_add_chat: Called when information is needed to add a chat to the
 *                    buddy list. See purple_blist_request_add_chat().
 * @request_add_group: Called when information is needed to add a group to the
 *                     buddy list. See purple_blist_request_add_group().
 * @save_node:    This is called when a node has been modified and should be
 *                saved.
 *                <sbr/>Implementation of this method is
 *                <emphasis>OPTIONAL</emphasis>. If not implemented, it will be
 *                set to a fallback function that saves data to
 *                <filename>blist.xml</filename> like in previous libpurple
 *                versions.
 *                <sbr/>@node: The node which has been modified.
 * @remove_node:  Called when a node is about to be removed from the buddy list.
 *                The method should update the relevant data structures to
 *                remove this node (for example, removing a buddy from the
 *                group this node is in).
 *                <sbr/>Implementation of this method is
 *                <emphasis>OPTIONAL</emphasis>. If not implemented, it will be
 *                set to a fallback function that saves data to
 *                <filename>blist.xml</filename> like in previous libpurple
 *                versions.
 *                <sbr/>@node: The node which has been modified.
 * @save_account: Called to save all the data for an account. If the UI sets
 *                this, the callback must save the privacy and buddy list data
 *                for an account. If the account is %NULL, save the data for all
 *                accounts.
 *                <sbr/>Implementation of this method is
 *                <emphasis>OPTIONAL</emphasis>. If not implemented, it will be
 *                set to a fallback function that saves data to
 *                <filename>blist.xml</filename> like in previous
 *                libpurple versions.
 *                <sbr/>@account: The account whose data to save. If %NULL,
 *                                save all data for all accounts.
 *
 * Buddy list operations.
 *
 * Any UI representing a buddy list must derive a filled-out
 * @PurpleBuddyListClass and set the GType using purple_blist_set_ui() before a
 * buddy list is created.
 */
struct _PurpleBuddyListClass {
	/*< private >*/
	GObjectClass gparent_class;

	/*< public >*/
	void (*new_node)(PurpleBuddyList *list, PurpleBlistNode *node);
	void (*show)(PurpleBuddyList *list);
	void (*update)(PurpleBuddyList *list, PurpleBlistNode *node);
	void (*remove)(PurpleBuddyList *list, PurpleBlistNode *node);
	void (*set_visible)(PurpleBuddyList *list, gboolean show);

	void (*request_add_buddy)(PurpleBuddyList *list, PurpleAccount *account,
	                          const char *username, const char *group,
	                          const char *alias);

	void (*request_add_chat)(PurpleBuddyList *list, PurpleAccount *account,
	                         PurpleGroup *group, const char *alias,
	                         const char *name);

	void (*request_add_group)(PurpleBuddyList *list);

	void (*save_node)(PurpleBuddyList *list, PurpleBlistNode *node);
	void (*remove_node)(PurpleBuddyList *list, PurpleBlistNode *node);

	void (*save_account)(PurpleBuddyList *list, PurpleAccount *account);

	/*< private >*/
	gpointer reserved[4];
};

G_BEGIN_DECLS

/**************************************************************************/
/* Buddy List API                                                         */
/**************************************************************************/

/**
 * purple_buddy_list_get_type:
 *
 * Returns: The #GType for the #PurpleBuddyList object.
 */
G_DECLARE_DERIVABLE_TYPE(PurpleBuddyList, purple_buddy_list, PURPLE, BUDDY_LIST,
                         GObject)

/**
 * purple_blist_get_default:
 *
 * Returns the default buddy list.
 *
 * Returns: (transfer none): The default buddy list.
 *
 * Since: 3.0.0
 */
PurpleBuddyList *purple_blist_get_default(void);

/**
 * purple_blist_get_default_root:
 *
 * Returns the root node of the default buddy list.
 *
 * Returns: (transfer none): The root node.
 *
 * Since: 3.0.0
 */
PurpleBlistNode *purple_blist_get_default_root(void);

/**
 * purple_blist_get_root:
 * @list: The buddy list to query.
 *
 * Returns the root node of the specified buddy list.
 *
 * Returns: (transfer none): The root node.
 */
PurpleBlistNode *purple_blist_get_root(PurpleBuddyList *list);

/**
 * purple_blist_get_buddies:
 *
 * Returns a list of every buddy in the list.  Use of this function is
 * discouraged if you do not actually need every buddy in the list.  Use
 * purple_blist_find_buddies instead.
 *
 * See purple_blist_find_buddies().
 *
 * Returns: (element-type PurpleBlistNode) (transfer container): A list of every
 *          buddy in the list.
 */
GSList *purple_blist_get_buddies(void);

/**
 * purple_blist_show:
 *
 * Shows the buddy list, creating a new one if necessary.
 */
void purple_blist_show(void);

/**
 * purple_blist_set_visible:
 * @show:   Whether or not to show the buddy list
 *
 * Hides or unhides the buddy list.
 */
void purple_blist_set_visible(gboolean show);

/**
 * purple_blist_update_buddies_cache:
 * @buddy:    The buddy whose name will be changed.
 * @new_name: The new name of the buddy.
 *
 * Updates the buddies hash table when a buddy has been renamed. This only
 * updates the cache, the caller is responsible for the actual renaming of
 * the buddy after updating the cache.
 */
void purple_blist_update_buddies_cache(PurpleBuddy *buddy, const char *new_name);

/**
 * purple_blist_update_groups_cache:
 * @group:    The group whose name will be changed.
 * @new_name: The new name of the group.
 *
 * Updates the groups hash table when a group has been renamed. This only
 * updates the cache, the caller is responsible for the actual renaming of
 * the group after updating the cache.
 */
void purple_blist_update_groups_cache(PurpleGroup *group, const char *new_name);

/**
 * purple_blist_add_chat:
 * @chat:  The new chat who gets added
 * @group:  The group to add the new chat to.
 * @node:   The insertion point
 *
 * Adds a new chat to the buddy list.
 *
 * The chat will be inserted right after node or appended to the end
 * of group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Chats" group.
 */
void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group, PurpleBlistNode *node);

/**
 * purple_blist_add_buddy:
 * @buddy:   The new buddy who gets added
 * @contact: The optional contact to place the buddy in.
 * @group:   The group to add the new buddy to.
 * @node:    The insertion point.  Pass in NULL to add the node as
 *                the first child in the given group.
 *
 * Adds a new buddy to the buddy list.
 *
 * The buddy will be inserted right after node or prepended to the
 * group if node is NULL.  If both are NULL, the buddy will be added to
 * the default group.
 */
void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node);

/**
 * purple_blist_add_group:
 * @group:  The group
 * @node:   The insertion point
 *
 * Adds a new group to the buddy list.
 *
 * The new group will be inserted after insert or prepended to the list if
 * node is NULL.
 */
void purple_blist_add_group(PurpleGroup *group, PurpleBlistNode *node);

/**
 * purple_blist_add_contact:
 * @contact: The contact
 * @group:   The group to add the contact to
 * @node:    The insertion point
 *
 * Adds a new contact to the buddy list.
 *
 * The new contact will be inserted after insert or prepended to the list if
 * node is NULL.
 */
void purple_blist_add_contact(PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node);

/**
 * purple_blist_remove_buddy:
 * @buddy:   The buddy to be removed
 *
 * Removes a buddy from the buddy list and frees the memory allocated to it.
 * This doesn't actually try to remove the buddy from the server list.
 *
 * See purple_account_remove_buddy().
 */
void purple_blist_remove_buddy(PurpleBuddy *buddy);

/**
 * purple_blist_remove_contact:
 * @contact: The contact to be removed
 *
 * Removes a contact, and any buddies it contains, and frees the memory
 * allocated to it. This calls purple_blist_remove_buddy and therefore
 * doesn't remove the buddies from the server list.
 *
 * See purple_blist_remove_buddy().
 */
void purple_blist_remove_contact(PurpleContact *contact);

/**
 * purple_blist_remove_chat:
 * @chat:   The chat to be removed
 *
 * Removes a chat from the buddy list and frees the memory allocated to it.
 */
void purple_blist_remove_chat(PurpleChat *chat);

/**
 * purple_blist_remove_group:
 * @group:   The group to be removed
 *
 * Removes a group from the buddy list and frees the memory allocated to it and to
 * its children
 */
void purple_blist_remove_group(PurpleGroup *group);

/**
 * purple_blist_find_buddy:
 * @account: The account this buddy belongs to
 * @name:    The buddy's name
 *
 * Finds the buddy struct given a name and an account
 *
 * Returns: (transfer none): The buddy or %NULL if the buddy does not exist.
 */
PurpleBuddy *purple_blist_find_buddy(PurpleAccount *account, const char *name);

/**
 * purple_blist_find_buddy_in_group:
 * @account: The account this buddy belongs to
 * @name:    The buddy's name
 * @group:   The group to look in
 *
 * Finds the buddy struct given a name, an account, and a group
 *
 * Returns: (transfer none): The buddy or %NULL if the buddy does not exist in
 *          the group.
 */
PurpleBuddy *purple_blist_find_buddy_in_group(PurpleAccount *account, const char *name,
		PurpleGroup *group);

/**
 * purple_blist_find_buddies:
 * @account: The account this buddy belongs to
 * @name:    The buddy's name (or NULL to return all buddies for the account)
 *
 * Finds all PurpleBuddy structs given a name and an account
 *
 * Returns: (element-type PurpleBuddy) (transfer container): %NULL if the buddy
 *          doesn't exist, or a GSList of PurpleBuddy structs.
 */
GSList *purple_blist_find_buddies(PurpleAccount *account, const char *name);

/**
 * purple_blist_find_group:
 * @name:    The group's name
 *
 * Finds a group by name
 *
 * Returns: (transfer none): The group or %NULL if the group does not exist.
 */
PurpleGroup *purple_blist_find_group(const char *name);

/**
 * purple_blist_get_default_group:
 *
 * Finds or creates default group.
 *
 * Returns: (transfer none): The default group.
 */
PurpleGroup *purple_blist_get_default_group(void);

/**
 * purple_blist_find_chat:
 * @account: The chat's account.
 * @name:    The chat's name.
 *
 * Finds a chat by name.
 *
 * Returns: (transfer none): The chat, or %NULL if the chat does not exist.
 */
PurpleChat *purple_blist_find_chat(PurpleAccount *account, const char *name);

/**
 * purple_blist_add_account:
 * @account:   The account
 *
 * Called when an account connects.  Tells the UI to update all the
 * buddies.
 */
void purple_blist_add_account(PurpleAccount *account);

/**
 * purple_blist_remove_account:
 * @account:   The account
 *
 * Called when an account disconnects.  Sets the presence of all the buddies to 0
 * and tells the UI to update them.
 */
void purple_blist_remove_account(PurpleAccount *account);

/**
 * purple_blist_walk:
 * @group_func: (scope call): The callback for groups
 * @chat_func: (scope call): The callback for chats
 * @meta_contact_func: (scope call): The callback for meta-contacts
 * @contact_func: (scope call): The callback for contacts
 * @data: User supplied data.
 *
 * Walks the buddy list and calls the appropriate function for each node.  If
 * a callback function is omitted iteration will continue without it.
 *
 * Since: 3.0.0
 */
void purple_blist_walk(PurpleBlistWalkFunc group_func, PurpleBlistWalkFunc chat_func, PurpleBlistWalkFunc meta_contact_func, PurpleBlistWalkFunc contact_func, gpointer data);

/****************************************************************************************/
/* Buddy list file management API                                                       */
/****************************************************************************************/

/**
 * purple_blist_schedule_save:
 *
 * Schedule a save of the <filename>blist.xml</filename> file.  This is used by
 * the account API whenever the privacy settings are changed.  If you make a
 * change to <filename>blist.xml</filename> using one of the functions in the
 * buddy list API, then the buddy list is saved automatically, so you should not
 * need to call this.
 */
void purple_blist_schedule_save(void);

/**
 * purple_blist_request_add_buddy:
 * @account:  The account the buddy is added to.
 * @username: The username of the buddy.
 * @group:    The name of the group to place the buddy in.
 * @alias:    The optional alias for the buddy.
 *
 * Requests from the user information needed to add a buddy to the
 * buddy list.
 */
void purple_blist_request_add_buddy(PurpleAccount *account, const char *username,
								  const char *group, const char *alias);

/**
 * purple_blist_request_add_chat:
 * @account: The account the buddy is added to.
 * @group:   The optional group to add the chat to.
 * @alias:   The optional alias for the chat.
 * @name:    The required chat name.
 *
 * Requests from the user information needed to add a chat to the
 * buddy list.
 */
void purple_blist_request_add_chat(PurpleAccount *account, PurpleGroup *group,
								 const char *alias, const char *name);

/**
 * purple_blist_request_add_group:
 *
 * Requests from the user information needed to add a group to the
 * buddy list.
 */
void purple_blist_request_add_group(void);

/**
 * purple_blist_new_node:
 * @list: The list that contains the node.
 * @node: The node to initialize.
 *
 * Sets UI-specific data on a node.
 *
 * This should usually only be run when initializing a @PurpleBlistNode
 * instance.
 *
 * Since: 3.0.0
 */
void purple_blist_new_node(PurpleBuddyList *list, PurpleBlistNode *node);

/**
 * purple_blist_update_node:
 * @list: The buddy list to modify.
 * @node: The node to update.
 *
 * Update a node in the buddy list in the UI.
 *
 * Since: 3.0.0
 */
void purple_blist_update_node(PurpleBuddyList *list, PurpleBlistNode *node);

/**
 * purple_blist_save_node:
 * @list: The list that contains the node.
 * @node: The node which has been modified.
 *
 * This is called when a node has been modified and should be saved by the UI.
 *
 * If the UI does not implement a more specific method, it will be set to save
 * data to <filename>blist.xml</filename> like in previous libpurple versions.
 *
 * Since: 3.0.0
 */
void purple_blist_save_node(PurpleBuddyList *list, PurpleBlistNode *node);

/**
 * purple_blist_save_account:
 * @list: The list that contains the account.
 * @account: The account whose data to save. If %NULL, save all data for all
 *           accounts.
 *
 * Save all the data for an account.
 *
 * If the UI does not set a more specific method, it will be set to save data
 * to <filename>blist.xml</filename> like in previous libpurple versions.
 *
 * Since: 3.0.0
 */
void purple_blist_save_account(PurpleBuddyList *list, PurpleAccount *account);

/**************************************************************************/
/* Buddy List Subsystem                                                   */
/**************************************************************************/

/**
 * purple_blist_set_ui:
 * @type: The @GType of a derived UI implementation of @PurpleBuddyList.
 *
 * Set the UI implementation of the buddy list.
 *
 * This must be called before the buddy list is created or you will get the
 * default libpurple implementation.
 *
 * Since: 3.0.0
 */
void purple_blist_set_ui(GType type);

/**
 * purple_blist_get_handle:
 *
 * Returns the handle for the buddy list subsystem.
 *
 * Returns: The buddy list subsystem handle.
 */
void *purple_blist_get_handle(void);

/**
 * purple_blist_init:
 *
 * Initializes the buddy list subsystem.
 */
void purple_blist_init(void);

/**
 * purple_blist_boot:
 *
 * Loads the buddy list.
 *
 * You shouldn't call this. purple_core_init() will do it for you.
 */
void purple_blist_boot(void);

/**
 * purple_blist_uninit:
 *
 * Uninitializes the buddy list subsystem.
 */
void purple_blist_uninit(void);

G_END_DECLS

#endif /* PURPLE_BUDDY_LIST_H */
