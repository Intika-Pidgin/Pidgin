/**
 * @file buddylist.h Buddy List API
 * @ingroup core
 * @see @ref blist-signals
 */

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
#ifndef _PURPLE_BUDDY_LIST_H_
#define _PURPLE_BUDDY_LIST_H_

/* I can't believe I let ChipX86 inspire me to write good code. -Sean */

#include "blistnodetypes.h"

#define PURPLE_TYPE_BUDDY_LIST             (purple_buddy_list_get_type())
#define PURPLE_BUDDY_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyList))
#define PURPLE_BUDDY_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyListClass))
#define PURPLE_IS_BUDDY_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BUDDY_LIST))
#define PURPLE_IS_BUDDY_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BUDDY_LIST))
#define PURPLE_BUDDY_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyListClass))

/** @copydoc _PurpleBuddyList */
typedef struct _PurpleBuddyList       PurpleBuddyList;
/** @copydoc _PurpleBuddyList */
typedef struct _PurpleBuddyListClass  PurpleBuddyListClass;

/** @copydoc _PurpleBlistUiOps */
typedef struct _PurpleBlistUiOps PurpleBlistUiOps;

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/
/**
 * The Buddy List
 */
struct _PurpleBuddyList {
	GObject gparent;

	/** The first node in the buddy list */
	PurpleBlistNode *root;

	/** The UI data associated with this buddy list. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/** The base class for all #PurpleBuddyList's. */
struct _PurpleBuddyListClass {
	GObjectClass gparent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * Buddy list UI operations.
 *
 * Any UI representing a buddy list must assign a filled-out PurpleBlistUiOps
 * structure to the buddy list core.
 */
struct _PurpleBlistUiOps
{
	void (*new_list)(PurpleBuddyList *list); /**< Sets UI-specific data on a buddy list. */
	void (*new_node)(PurpleBlistNode *node); /**< Sets UI-specific data on a node. */
	void (*show)(PurpleBuddyList *list);     /**< The core will call this when it's finished doing its core stuff */
	void (*update)(PurpleBuddyList *list,
		       PurpleBlistNode *node);       /**< This will update a node in the buddy list. */
	void (*remove)(PurpleBuddyList *list,
		       PurpleBlistNode *node);       /**< This removes a node from the list */
	void (*destroy)(PurpleBuddyList *list);  /**< When the list is destroyed, this is called to destroy the UI. */
	void (*set_visible)(PurpleBuddyList *list,
			    gboolean show);            /**< Hides or unhides the buddy list */
	void (*request_add_buddy)(PurpleAccount *account, const char *username,
							  const char *group, const char *alias);
	void (*request_add_chat)(PurpleAccount *account, PurpleGroup *group,
							 const char *alias, const char *name);
	void (*request_add_group)(void);

	/**
	 * This is called when a node has been modified and should be saved.
	 *
	 * Implementation of this UI op is OPTIONAL. If not implemented, it will
	 * be set to a fallback function that saves data to blist.xml like in
	 * previous libpurple versions.
	 *
	 * @node:    The node which has been modified.
	 */
	void (*save_node)(PurpleBlistNode *node);

	/**
	 * Called when a node is about to be removed from the buddy list.
	 * The UI op should update the relevant data structures to remove this
	 * node (for example, removing a buddy from the group this node is in).
	 *
	 * Implementation of this UI op is OPTIONAL. If not implemented, it will
	 * be set to a fallback function that saves data to blist.xml like in
	 * previous libpurple versions.
	 *
	 * @node:  The node which has been modified.
	 */
	void (*remove_node)(PurpleBlistNode *node);

	/**
	 * Called to save all the data for an account. If the UI sets this,
	 * the callback must save the privacy and buddy list data for an account.
	 * If the account is NULL, save the data for all accounts.
	 *
	 * Implementation of this UI op is OPTIONAL. If not implemented, it will
	 * be set to a fallback function that saves data to blist.xml like in
	 * previous libpurple versions.
	 *
	 * @account:  The account whose data to save. If NULL, save all data
	 *                  for all accounts.
	 */
	void (*save_account)(PurpleAccount *account);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Buddy List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleBuddyList object.
 */
GType purple_buddy_list_get_type(void);

/**
 * Returns the main buddy list.
 *
 * Returns: The main buddy list.
 */
PurpleBuddyList *purple_blist_get_buddy_list(void);

/**
 * Returns the root node of the main buddy list.
 *
 * Returns: The root node.
 */
PurpleBlistNode *purple_blist_get_root(void);

/**
 * Returns a list of every buddy in the list.  Use of this function is
 * discouraged if you do not actually need every buddy in the list.  Use
 * purple_blist_find_buddies instead.
 *
 * Returns: A list of every buddy in the list. Caller is responsible for
 *         freeing the list.
 *
 * @see purple_blist_find_buddies
 */
GSList *purple_blist_get_buddies(void);

/**
 * Returns the UI data for the list.
 *
 * Returns: The UI data for the list.
 */
gpointer purple_blist_get_ui_data(void);

/**
 * Sets the UI data for the list.
 *
 * @ui_data: The UI data for the list.
 */
void purple_blist_set_ui_data(gpointer ui_data);

/**
 * Shows the buddy list, creating a new one if necessary.
 */
void purple_blist_show(void);

/**
 * Hides or unhides the buddy list.
 *
 * @show:   Whether or not to show the buddy list
 */
void purple_blist_set_visible(gboolean show);

/**
 * Updates the buddies hash table when a buddy has been renamed. This only
 * updates the cache, the caller is responsible for the actual renaming of
 * the buddy after updating the cache.
 *
 * @buddy:  The buddy whose name will be changed.
 * @name:   The new name of the buddy.
 */
void purple_blist_update_buddies_cache(PurpleBuddy *buddy, const char *new_name);

/**
 * Updates the groups hash table when a group has been renamed. This only
 * updates the cache, the caller is responsible for the actual renaming of
 * the group after updating the cache.
 *
 * @group:  The group whose name will be changed.
 * @name:   The new name of the group.
 */
void purple_blist_update_groups_cache(PurpleGroup *group, const char *new_name);

/**
 * Adds a new chat to the buddy list.
 *
 * The chat will be inserted right after node or appended to the end
 * of group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Chats" group.
 *
 * @chat:  The new chat who gets added
 * @group:  The group to add the new chat to.
 * @node:   The insertion point
 */
void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group, PurpleBlistNode *node);

/**
 * Adds a new buddy to the buddy list.
 *
 * The buddy will be inserted right after node or prepended to the
 * group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Buddies" group.
 *
 * @buddy:   The new buddy who gets added
 * @contact: The optional contact to place the buddy in.
 * @group:   The group to add the new buddy to.
 * @node:    The insertion point.  Pass in NULL to add the node as
 *                the first child in the given group.
 */
void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node);

/**
 * Adds a new group to the buddy list.
 *
 * The new group will be inserted after insert or prepended to the list if
 * node is NULL.
 *
 * @group:  The group
 * @node:   The insertion point
 */
void purple_blist_add_group(PurpleGroup *group, PurpleBlistNode *node);

/**
 * Adds a new contact to the buddy list.
 *
 * The new contact will be inserted after insert or prepended to the list if
 * node is NULL.
 *
 * @contact: The contact
 * @group:   The group to add the contact to
 * @node:    The insertion point
 */
void purple_blist_add_contact(PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node);

/**
 * Removes a buddy from the buddy list and frees the memory allocated to it.
 * This doesn't actually try to remove the buddy from the server list.
 *
 * @buddy:   The buddy to be removed
 *
 * @see purple_account_remove_buddy
 */
void purple_blist_remove_buddy(PurpleBuddy *buddy);

/**
 * Removes a contact, and any buddies it contains, and frees the memory
 * allocated to it. This calls purple_blist_remove_buddy and therefore
 * doesn't remove the buddies from the server list.
 *
 * @contact: The contact to be removed
 *
 * @see purple_blist_remove_buddy
 */
void purple_blist_remove_contact(PurpleContact *contact);

/**
 * Removes a chat from the buddy list and frees the memory allocated to it.
 *
 * @chat:   The chat to be removed
 */
void purple_blist_remove_chat(PurpleChat *chat);

/**
 * Removes a group from the buddy list and frees the memory allocated to it and to
 * its children
 *
 * @group:   The group to be removed
 */
void purple_blist_remove_group(PurpleGroup *group);

/**
 * Finds the buddy struct given a name and an account
 *
 * @account: The account this buddy belongs to
 * @name:    The buddy's name
 * Returns:        The buddy or NULL if the buddy does not exist
 */
PurpleBuddy *purple_blist_find_buddy(PurpleAccount *account, const char *name);

/**
 * Finds the buddy struct given a name, an account, and a group
 *
 * @account: The account this buddy belongs to
 * @name:    The buddy's name
 * @group:   The group to look in
 * Returns:        The buddy or NULL if the buddy does not exist in the group
 */
PurpleBuddy *purple_blist_find_buddy_in_group(PurpleAccount *account, const char *name,
		PurpleGroup *group);

/**
 * Finds all PurpleBuddy structs given a name and an account
 *
 * @account: The account this buddy belongs to
 * @name:    The buddy's name (or NULL to return all buddies for the account)
 *
 * Returns:        NULL if the buddy doesn't exist, or a GSList of
 *                PurpleBuddy structs.  You must free the GSList using
 *                g_slist_free.  Do not free the PurpleBuddy structs that
 *                the list points to.
 */
GSList *purple_blist_find_buddies(PurpleAccount *account, const char *name);

/**
 * Finds a group by name
 *
 * @name:    The group's name
 * Returns:        The group or NULL if the group does not exist
 */
PurpleGroup *purple_blist_find_group(const char *name);

/**
 * Finds a chat by name.
 *
 * @account: The chat's account.
 * @name:    The chat's name.
 *
 * Returns: The chat, or %NULL if the chat does not exist.
 */
PurpleChat *purple_blist_find_chat(PurpleAccount *account, const char *name);

/**
 * Called when an account connects.  Tells the UI to update all the
 * buddies.
 *
 * @account:   The account
 */
void purple_blist_add_account(PurpleAccount *account);

/**
 * Called when an account disconnects.  Sets the presence of all the buddies to 0
 * and tells the UI to update them.
 *
 * @account:   The account
 */
void purple_blist_remove_account(PurpleAccount *account);

/*@}*/

/****************************************************************************************/
/** @name Buddy list file management API                                                */
/****************************************************************************************/

/**
 * Schedule a save of the blist.xml file.  This is used by the privacy
 * API whenever the privacy settings are changed.  If you make a change
 * to blist.xml using one of the functions in the buddy list API, then
 * the buddy list is saved automatically, so you should not need to
 * call this.
 */
void purple_blist_schedule_save(void);

/**
 * Requests from the user information needed to add a buddy to the
 * buddy list.
 *
 * @account:  The account the buddy is added to.
 * @username: The username of the buddy.
 * @group:    The name of the group to place the buddy in.
 * @alias:    The optional alias for the buddy.
 */
void purple_blist_request_add_buddy(PurpleAccount *account, const char *username,
								  const char *group, const char *alias);

/**
 * Requests from the user information needed to add a chat to the
 * buddy list.
 *
 * @account: The account the buddy is added to.
 * @group:   The optional group to add the chat to.
 * @alias:   The optional alias for the chat.
 * @name:    The required chat name.
 */
void purple_blist_request_add_chat(PurpleAccount *account, PurpleGroup *group,
								 const char *alias, const char *name);

/**
 * Requests from the user information needed to add a group to the
 * buddy list.
 */
void purple_blist_request_add_group(void);

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for the buddy list.
 *
 * @ops: The ops struct.
 */
void purple_blist_set_ui_ops(PurpleBlistUiOps *ops);

/**
 * Returns the UI operations structure to be used for the buddy list.
 *
 * Returns: The UI operations structure.
 */
PurpleBlistUiOps *purple_blist_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Buddy List Subsystem                                            */
/**************************************************************************/
/*@{*/

/**
 * Returns the handle for the buddy list subsystem.
 *
 * Returns: The buddy list subsystem handle.
 */
void *purple_blist_get_handle(void);

/**
 * Initializes the buddy list subsystem.
 */
void purple_blist_init(void);

/**
 * Loads the buddy list.
 *
 * You shouldn't call this. purple_core_init() will do it for you.
 */
void purple_blist_boot(void);

/**
 * Uninitializes the buddy list subsystem.
 */
void purple_blist_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_BUDDY_LIST_H_ */
