/**
 * @file blist.h Buddy List API
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
#ifndef _PURPLE_BLIST_H_
#define _PURPLE_BLIST_H_

/* I can't believe I let ChipX86 inspire me to write good code. -Sean */

#include <glib.h>

#define PURPLE_TYPE_BLIST_NODE             (purple_blist_node_get_type())
#define PURPLE_BLIST_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BLIST_NODE, PurpleBlistNode))
#define PURPLE_BLIST_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BLIST_NODE, PurpleBlistNodeClass))
#define PURPLE_IS_BLIST_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BLIST_NODE))
#define PURPLE_IS_BLIST_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BLIST_NODE))
#define PURPLE_BLIST_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BLIST_NODE, PurpleBlistNodeClass))

/** @copydoc _PurpleBlistNode */
typedef struct _PurpleBlistNode PurpleBlistNode;
/** @copydoc _PurpleBlistNodeClass */
typedef struct _PurpleBlistNodeClass PurpleBlistNodeClass;

#define PURPLE_TYPE_COUNTING_NODE             (purple_counting_node_get_type())
#define PURPLE_COUNTING_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNode))
#define PURPLE_COUNTING_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNodeClass))
#define PURPLE_IS_COUNTING_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_COUNTING_NODE))
#define PURPLE_IS_COUNTING_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_COUNTING_NODE))
#define PURPLE_COUNTING_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNodeClass))

/** @copydoc _PurpleCountingNode */
typedef struct _PurpleCountingNode PurpleCountingNode;
/** @copydoc _PurpleCountingNodeClass */
typedef struct _PurpleCountingNodeClass PurpleCountingNodeClass;

#define PURPLE_TYPE_BUDDY             (purple_buddy_get_type())
#define PURPLE_BUDDY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BUDDY, PurpleBuddy))
#define PURPLE_BUDDY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BUDDY, PurpleBuddyClass))
#define PURPLE_IS_BUDDY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BUDDY))
#define PURPLE_IS_BUDDY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BUDDY))
#define PURPLE_BUDDY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BUDDY, PurpleBuddyClass))

/** @copydoc _PurpleBuddy */
typedef struct _PurpleBuddy PurpleBuddy;
/** @copydoc _PurpleBuddyClass */
typedef struct _PurpleBuddyClass PurpleBuddyClass;

#define PURPLE_TYPE_CONTACT             (purple_contact_get_type())
#define PURPLE_CONTACT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CONTACT, PurpleContact))
#define PURPLE_CONTACT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CONTACT, PurpleContactClass))
#define PURPLE_IS_CONTACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CONTACT))
#define PURPLE_IS_CONTACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CONTACT))
#define PURPLE_CONTACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CONTACT, PurpleContactClass))

/** @copydoc _PurpleContact */
typedef struct _PurpleContact PurpleContact;
/** @copydoc _PurpleContactClass */
typedef struct _PurpleContactClass PurpleContactClass;

#define PURPLE_TYPE_GROUP             (purple_group_get_type())
#define PURPLE_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_GROUP, PurpleGroup))
#define PURPLE_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_GROUP, PurpleGroupClass))
#define PURPLE_IS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_GROUP))
#define PURPLE_IS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_GROUP))
#define PURPLE_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_GROUP, PurpleGroupClass))

/** @copydoc _PurpleGroup */
typedef struct _PurpleGroup PurpleGroup;
/** @copydoc _PurpleGroupClass */
typedef struct _PurpleGroupClass PurpleGroupClass;

#define PURPLE_TYPE_CHAT             (purple_chat_get_type())
#define PURPLE_CHAT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT, PurpleChat))
#define PURPLE_CHAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT, PurpleChatClass))
#define PURPLE_IS_CHAT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT))
#define PURPLE_IS_CHAT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT))
#define PURPLE_CHAT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT, PurpleChatClass))

/** @copydoc _PurpleChat */
typedef struct _PurpleChat PurpleChat;
/** @copydoc _PurpleChatClass */
typedef struct _PurpleChatClass PurpleChatClass;

#define PURPLE_TYPE_BUDDY_LIST             (purple_buddy_list_get_type())
#define PURPLE_BUDDY_LIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyList))
#define PURPLE_BUDDY_LIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyListClass))
#define PURPLE_IS_BUDDY_LIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BUDDY_LIST))
#define PURPLE_IS_BUDDY_LIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BUDDY_LIST))
#define PURPLE_BUDDY_LIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyListClass))

/** @copydoc _PurpleBuddyList */
typedef struct _PurpleBuddyList PurpleBuddyList;
/** @copydoc _PurpleBuddyListClass */
typedef struct _PurpleBuddyListClass PurpleBuddyListClass;

/** @copydoc _PurpleBlistUiOps */
typedef struct _PurpleBlistUiOps PurpleBlistUiOps;

#define PURPLE_IS_BUDDY_ONLINE(b) \
	(PURPLE_IS_BUDDY(b) \
	&& purple_account_is_connected(purple_buddy_get_account(PURPLE_BUDDY(b))) \
	&& purple_presence_is_online(purple_buddy_get_presence(PURPLE_BUDDY(b))))

#define PURPLE_BLIST_NODE_NAME(n) \
	(PURPLE_IS_CHAT(n) ? purple_chat_get_name(PURPLE_CHAT(n)) : \
	PURPLE_IS_BUDDY(n) ? purple_buddy_get_name(PURPLE_BUDDY(n)) : NULL)

#include "account.h"
#include "buddyicon.h"
#include "media.h"
#include "status.h"

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/

#if !(defined PURPLE_HIDE_STRUCTS) || (defined _PURPLE_BLIST_C_)

/**
 * A Buddy list node.  This can represent a group, a buddy, or anything else.
 * This is a base class for PurpleBuddy, PurpleContact, PurpleGroup, and for
 * anything else that wants to put itself in the buddy list. */
struct _PurpleBlistNode {
	/*< private >*/
	GObject gparent;

	/** The UI data associated with this account. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/** The base class for all #PurpleBlistNode's. */
struct _PurpleBlistNodeClass {
	/*< private >*/
	GObjectClass gparent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A Buddy list node that keeps a count of the number of children it has.
 */
struct _PurpleCountingNode {
	/** The Buddy list node that this counting node inherits from */
	PurpleBlistNode node;
};

/** The base class for all #PurpleCountingNode's. */
struct _PurpleCountingNodeClass {
	/*< private >*/
	PurpleBlistNodeClass node_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A buddy.  This contains everything Purple will ever need to know about
 * someone on the buddy list.  Everything.
 */
struct _PurpleBuddy {
	/** The node that this buddy inherits from */
	PurpleBlistNode node;
};

/** The base class for all #PurpleBuddy's. */
struct _PurpleBuddyClass {
	/*< private >*/
	PurpleBlistNodeClass node_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A contact.  This contains everything Purple will ever need to know about a
 * contact.
 */
struct _PurpleContact {
	/** The counting node that keeps a count of the number of buddies in this
	 *  contact
	 */
	PurpleCountingNode cnode;
};

/** The base class for all #PurpleContact's. */
struct _PurpleContactClass {
	/*< private >*/
	PurpleCountingNodeClass cnode_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A group.  This contains everything Purple will ever need to know about a
 * group.
 */
struct _PurpleGroup {
	/** The counting node that keeps a count of the number of chats and contacts
	 *  in this group
	 */
	PurpleCountingNode cnode;
};

/** The base class for all #PurpleGroup's. */
struct _PurpleGroupClass {
	/*< private >*/
	PurpleCountingNodeClass cnode_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A chat.  This contains everything Purple needs to put a chat room in the
 * buddy list.
 */
struct _PurpleChat {
	/** The node that this chat inherits from */
	PurpleBlistNode node;
};

/** The base class for all #PurpleChat's. */
struct _PurpleChatClass {
	/*< private >*/
	PurpleBlistNodeClass node_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * The Buddy List
 */
struct _PurpleBuddyList {
	/*< private >*/
	GObject gparent;

	/** The UI data associated with this account. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/** The base class for all #PurpleBuddyList's. */
struct _PurpleBuddyListClass {
	/*< private >*/
	GObjectClass gparent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

#endif /* PURPLE_HIDE_STRUCTS && PURPLE_BLIST_STRUCTS */

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
	 * @param node    The node which has been modified.
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
	 * @param node  The node which has been modified.
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
	 * @param account  The account whose data to save. If NULL, save all data
	 *                  for all accounts.
	 */
	void (*save_account)(PurpleAccount *account);

	void (*_purple_reserved1)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Buddy List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Returns the main buddy list.
 *
 * @return The main buddy list.
 */
PurpleBuddyList *purple_get_blist(void);

/**
 * Returns the root node of the main buddy list.
 *
 * @return The root node.
 */
PurpleBlistNode *purple_blist_get_root(void);

/**
 * Returns a list of every buddy in the list.  Use of this function is
 * discouraged if you do not actually need every buddy in the list.  Use
 * purple_find_buddies instead.
 *
 * @return A list of every buddy in the list. Caller is responsible for
 *         freeing the list.
 *
 * @see purple_find_buddies
 */
GSList *purple_blist_get_buddies(void);

/**
 * Returns the UI data for the list.
 *
 * @return The UI data for the list.
 */
gpointer purple_blist_get_ui_data(void);

/**
 * Sets the UI data for the list.
 *
 * @param ui_data The UI data for the list.
 */
void purple_blist_set_ui_data(gpointer ui_data);

/**
 * Returns the next node of a given node. This function is to be used to iterate
 * over the tree returned by purple_get_blist.
 *
 * @param node		A node.
 * @param offline	Whether to include nodes for offline accounts
 * @return	The next node
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_get_sibling_prev
 */
PurpleBlistNode *purple_blist_node_next(PurpleBlistNode *node, gboolean offline);

/**
 * Returns the parent node of a given node.
 *
 * @param node A node.
 * @return  The parent node.
 *
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_get_sibling_prev
 * @see purple_blist_node_next
 */
PurpleBlistNode *purple_blist_node_get_parent(PurpleBlistNode *node);

/**
 * Returns the the first child node of a given node.
 *
 * @param node A node.
 * @return  The child node.
 *
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_get_sibling_prev
 * @see purple_blist_node_next
 */
PurpleBlistNode *purple_blist_node_get_first_child(PurpleBlistNode *node);

/**
 * Returns the sibling node of a given node.
 *
 * @param node A node.
 * @return  The sibling node.
 *
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_prev
 * @see purple_blist_node_next
 */
PurpleBlistNode *purple_blist_node_get_sibling_next(PurpleBlistNode *node);

/**
 * Returns the previous sibling node of a given node.
 *
 * @param node A node.
 * @return  The sibling node.
 *
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_next
 */
PurpleBlistNode *purple_blist_node_get_sibling_prev(PurpleBlistNode *node);

/**
 * Returns the UI data of a given node.
 *
 * @param node The node.
 * @return The UI data.
 */
gpointer purple_blist_node_get_ui_data(const PurpleBlistNode *node);

/**
 * Sets the UI data of a given node.
 *
 * @param node The node.
 * @param ui_data The UI data.
 */
void purple_blist_node_set_ui_data(PurpleBlistNode *node, gpointer ui_data);

/**
 * Shows the buddy list, creating a new one if necessary.
 */
void purple_blist_show(void);


/**
 * Destroys the buddy list window.
 *
 * @deprecated The UI is responsible for cleaning up the
 *             PurpleBuddyList->ui_data. purple_blist_uninit() will free the
 *             PurpleBuddyList* itself.
 */
void purple_blist_destroy(void);

/**
 * Hides or unhides the buddy list.
 *
 * @param show   Whether or not to show the buddy list
 */
void purple_blist_set_visible(gboolean show);

/**
 * Updates a buddy's status.
 *
 * This should only be called from within Purple.
 *
 * @param buddy      The buddy whose status has changed.
 * @param old_status The status from which we are changing.
 */
void purple_blist_update_buddy_status(PurpleBuddy *buddy, PurpleStatus *old_status);

/**
 * Updates a node's custom icon.
 *
 * @param node  The PurpleBlistNode whose custom icon has changed.
 */
void purple_blist_update_node_icon(PurpleBlistNode *node);

/**
 * Renames a buddy in the buddy list.
 *
 * @param buddy  The buddy whose name will be changed.
 * @param name   The new name of the buddy.
 */
void purple_blist_rename_buddy(PurpleBuddy *buddy, const char *name);

/**
 * Aliases a contact in the buddy list.
 *
 * @param contact The contact whose alias will be changed.
 * @param alias   The contact's alias.
 */
void purple_blist_alias_contact(PurpleContact *contact, const char *alias);

/**
 * Aliases a buddy in the buddy list.
 *
 * @param buddy  The buddy whose alias will be changed.
 * @param alias  The buddy's alias.
 */
void purple_blist_alias_buddy(PurpleBuddy *buddy, const char *alias);

/**
 * Sets the server-sent alias of a buddy in the buddy list.
 * PRPLs should call serv_got_alias() instead of this.
 *
 * @param buddy  The buddy whose alias will be changed.
 * @param alias  The buddy's "official" alias.
 */
void purple_blist_server_alias_buddy(PurpleBuddy *buddy, const char *alias);

/**
 * Aliases a chat in the buddy list.
 *
 * @param chat  The chat whose alias will be changed.
 * @param alias The chat's new alias.
 */
void purple_blist_alias_chat(PurpleChat *chat, const char *alias);

/**
 * Renames a group
 *
 * @param group  The group to rename
 * @param name   The new name
 */
void purple_blist_rename_group(PurpleGroup *group, const char *name);

/**
 * Creates a new chat for the buddy list
 *
 * @param account    The account this chat will get added to
 * @param alias      The alias of the new chat
 * @param components The info the prpl needs to join the chat.  The
 *                   hash function should be g_str_hash() and the
 *                   equal function should be g_str_equal().
 * @return           A newly allocated chat
 */
PurpleChat *purple_chat_new(PurpleAccount *account, const char *alias, GHashTable *components);

/**
 * Destroys a chat
 *
 * @param chat       The chat to destroy
 */
void purple_chat_destroy(PurpleChat *chat);

/**
 * Adds a new chat to the buddy list.
 *
 * The chat will be inserted right after node or appended to the end
 * of group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Chats" group.
 *
 * @param chat  The new chat who gets added
 * @param group  The group to add the new chat to.
 * @param node   The insertion point
 */
void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group, PurpleBlistNode *node);

/**
 * Creates a new buddy.
 *
 * This function only creates the PurpleBuddy. Use purple_blist_add_buddy
 * to add the buddy to the list and purple_account_add_buddy to sync up
 * with the server.
 *
 * @param account    The account this buddy will get added to
 * @param name       The name of the new buddy
 * @param alias      The alias of the new buddy (or NULL if unaliased)
 * @return           A newly allocated buddy
 *
 * @see purple_account_add_buddy
 * @see purple_blist_add_buddy
 */
PurpleBuddy *purple_buddy_new(PurpleAccount *account, const char *name, const char *alias);

/**
 * Destroys a buddy
 *
 * @param buddy     The buddy to destroy
 */
void purple_buddy_destroy(PurpleBuddy *buddy);

/**
 * Sets a buddy's icon.
 *
 * This should only be called from within Purple. You probably want to
 * call purple_buddy_icon_set_data().
 *
 * @param buddy The buddy.
 * @param icon  The buddy icon.
 *
 * @see purple_buddy_icon_set_data()
 */
void purple_buddy_set_icon(PurpleBuddy *buddy, PurpleBuddyIcon *icon);

/**
 * Returns a buddy's account.
 *
 * @param buddy The buddy.
 *
 * @return The account
 */
PurpleAccount *purple_buddy_get_account(const PurpleBuddy *buddy);

/**
 * Returns a buddy's name
 *
 * @param buddy The buddy.
 *
 * @return The name.
 */
const char *purple_buddy_get_name(const PurpleBuddy *buddy);

/**
 * Returns a buddy's icon.
 *
 * @param buddy The buddy.
 *
 * @return The buddy icon.
 */
PurpleBuddyIcon *purple_buddy_get_icon(const PurpleBuddy *buddy);

/**
 * Returns a buddy's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param buddy The buddy.
 * @return      The protocol data.
 *
 * @see purple_buddy_set_protocol_data()
 */
gpointer purple_buddy_get_protocol_data(const PurpleBuddy *buddy);

/**
 * Sets a buddy's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param buddy The buddy.
 * @param data  The data.
 *
 * @see purple_buddy_get_protocol_data()
 */
void purple_buddy_set_protocol_data(PurpleBuddy *buddy, gpointer data);

/**
 * Returns a buddy's contact.
 *
 * @param buddy The buddy.
 *
 * @return The buddy's contact.
 */
PurpleContact *purple_buddy_get_contact(PurpleBuddy *buddy);

/**
 * Returns a buddy's presence.
 *
 * @param buddy The buddy.
 *
 * @return The buddy's presence.
 */
PurplePresence *purple_buddy_get_presence(const PurpleBuddy *buddy);

/**
 * Gets the media caps from a buddy.
 *
 * @param buddy The buddy.
 * @return      The media caps.
 */
PurpleMediaCaps purple_buddy_get_media_caps(const PurpleBuddy *buddy);

/**
 * Sets the media caps for a buddy.
 *
 * @param buddy      The PurpleBuddy.
 * @param media_caps The PurpleMediaCaps.
 */
void purple_buddy_set_media_caps(PurpleBuddy *buddy, PurpleMediaCaps media_caps);

/**
 * Adds a new buddy to the buddy list.
 *
 * The buddy will be inserted right after node or prepended to the
 * group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Buddies" group.
 *
 * @param buddy   The new buddy who gets added
 * @param contact The optional contact to place the buddy in.
 * @param group   The group to add the new buddy to.
 * @param node    The insertion point.  Pass in NULL to add the node as
 *                the first child in the given group.
 */
void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node);

/**
 * Creates a new group
 *
 * You can't have more than one group with the same name.  Sorry.  If you pass
 * this the name of a group that already exists, it will return that group.
 *
 * @param name   The name of the new group
 * @return       A new group struct
*/
PurpleGroup *purple_group_new(const char *name);

/**
 * Destroys a group
 *
 * @param group  The group to destroy
*/
void purple_group_destroy(PurpleGroup *group);

/**
 * Adds a new group to the buddy list.
 *
 * The new group will be inserted after insert or prepended to the list if
 * node is NULL.
 *
 * @param group  The group
 * @param node   The insertion point
 */
void purple_blist_add_group(PurpleGroup *group, PurpleBlistNode *node);

/**
 * Creates a new contact
 *
 * @return       A new contact struct
 */
PurpleContact *purple_contact_new(void);

/**
 * Destroys a contact
 *
 * @param contact  The contact to destroy
 */
void purple_contact_destroy(PurpleContact *contact);

/**
 * Gets the PurpleGroup from a PurpleContact
 *
 * @param contact  The contact
 * @return         The group
 */
PurpleGroup *purple_contact_get_group(const PurpleContact *contact);

/**
 * Adds a new contact to the buddy list.
 *
 * The new contact will be inserted after insert or prepended to the list if
 * node is NULL.
 *
 * @param contact The contact
 * @param group   The group to add the contact to
 * @param node    The insertion point
 */
void purple_blist_add_contact(PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node);

/**
 * Merges two contacts
 *
 * All of the buddies from source will be moved to target
 *
 * @param source  The contact to merge
 * @param node    The place to merge to (a buddy or contact)
 */
void purple_blist_merge_contact(PurpleContact *source, PurpleBlistNode *node);

/**
 * Returns the highest priority buddy for a given contact.
 *
 * @param contact  The contact
 * @return The highest priority buddy
 */
PurpleBuddy *purple_contact_get_priority_buddy(PurpleContact *contact);

/**
 * Gets the alias for a contact.
 *
 * @param contact  The contact
 * @return  The alias, or NULL if it is not set.
 */
const char *purple_contact_get_alias(PurpleContact *contact);

/**
 * Determines whether an account owns any buddies in a given contact
 *
 * @param contact  The contact to search through.
 * @param account  The account.
 *
 * @return TRUE if there are any buddies from account in the contact, or FALSE otherwise.
 */
gboolean purple_contact_on_account(PurpleContact *contact, PurpleAccount *account);

/**
 * Invalidates the priority buddy so that the next call to
 * purple_contact_get_priority_buddy recomputes it.
 *
 * @param contact  The contact
 */
void purple_contact_invalidate_priority_buddy(PurpleContact *contact);

/**
 * Determines the total size of a contact.
 *
 * @param contact	The contact
 * @param offline	Count buddies in offline accounts
 * @return The number of buddies in the contact
 */
int purple_contact_get_contact_size(PurpleContact *contact, gboolean offline);

/**
 * Removes a buddy from the buddy list and frees the memory allocated to it.
 * This doesn't actually try to remove the buddy from the server list.
 *
 * @param buddy   The buddy to be removed
 *
 * @see purple_account_remove_buddy
 */
void purple_blist_remove_buddy(PurpleBuddy *buddy);

/**
 * Removes a contact, and any buddies it contains, and frees the memory
 * allocated to it. This calls purple_blist_remove_buddy and therefore
 * doesn't remove the buddies from the server list.
 *
 * @param contact The contact to be removed
 *
 * @see purple_blist_remove_buddy
 */
void purple_blist_remove_contact(PurpleContact *contact);

/**
 * Removes a chat from the buddy list and frees the memory allocated to it.
 *
 * @param chat   The chat to be removed
 */
void purple_blist_remove_chat(PurpleChat *chat);

/**
 * Removes a group from the buddy list and frees the memory allocated to it and to
 * its children
 *
 * @param group   The group to be removed
 */
void purple_blist_remove_group(PurpleGroup *group);

/**
 * Returns the alias of a buddy.
 *
 * @param buddy   The buddy whose name will be returned.
 * @return        The alias (if set), server alias (if set),
 *                or NULL.
 */
const char *purple_buddy_get_alias_only(PurpleBuddy *buddy);

/**
 * Gets the server alias for a buddy.
 *
 * @param buddy  The buddy whose name will be returned
 * @return  The server alias, or NULL if it is not set.
 */
const char *purple_buddy_get_server_alias(PurpleBuddy *buddy);

/**
 * Returns the correct name to display for a buddy, taking the contact alias
 * into account. In order of precedence: the buddy's alias; the buddy's
 * contact alias; the buddy's server alias; the buddy's user name.
 *
 * @param buddy  The buddy whose name will be returned
 * @return       The appropriate name or alias, or NULL.
 *
 */
const char *purple_buddy_get_contact_alias(PurpleBuddy *buddy);

/**
 * Returns the correct name to display for a buddy. In order of precedence:
 * the buddy's alias; the buddy's server alias; the buddy's contact alias;
 * the buddy's user name.
 *
 * @param buddy   The buddy whose name will be returned.
 * @return        The appropriate name or alias, or NULL
 */
const char *purple_buddy_get_alias(PurpleBuddy *buddy);

/**
 * Returns the local alias for the buddy, or @c NULL if none exists.
 *
 * @param buddy  The buddy
 * @return       The local alias for the buddy
 */
const char *purple_buddy_get_local_buddy_alias(PurpleBuddy *buddy);

/**
 * Returns the correct name to display for a blist chat.
 *
 * @param chat   The chat whose name will be returned.
 * @return       The alias (if set), or first component value.
 */
const char *purple_chat_get_name(PurpleChat *chat);

/**
 * Finds the buddy struct given a name and an account
 *
 * @param account The account this buddy belongs to
 * @param name    The buddy's name
 * @return        The buddy or NULL if the buddy does not exist
 */
PurpleBuddy *purple_find_buddy(PurpleAccount *account, const char *name);

/**
 * Finds the buddy struct given a name, an account, and a group
 *
 * @param account The account this buddy belongs to
 * @param name    The buddy's name
 * @param group   The group to look in
 * @return        The buddy or NULL if the buddy does not exist in the group
 */
PurpleBuddy *purple_find_buddy_in_group(PurpleAccount *account, const char *name,
		PurpleGroup *group);

/**
 * Finds all PurpleBuddy structs given a name and an account
 *
 * @param account The account this buddy belongs to
 * @param name    The buddy's name (or NULL to return all buddies for the account)
 *
 * @return        NULL if the buddy doesn't exist, or a GSList of
 *                PurpleBuddy structs.  You must free the GSList using
 *                g_slist_free.  Do not free the PurpleBuddy structs that
 *                the list points to.
 */
GSList *purple_find_buddies(PurpleAccount *account, const char *name);


/**
 * Finds a group by name
 *
 * @param name    The group's name
 * @return        The group or NULL if the group does not exist
 */
PurpleGroup *purple_find_group(const char *name);

/**
 * Finds a chat by name.
 *
 * @param account The chat's account.
 * @param name    The chat's name.
 *
 * @return The chat, or @c NULL if the chat does not exist.
 */
PurpleChat *purple_blist_find_chat(PurpleAccount *account, const char *name);

/**
 * Returns the group of which the chat is a member.
 *
 * @param chat The chat.
 *
 * @return The parent group, or @c NULL if the chat is not in a group.
 */
PurpleGroup *purple_chat_get_group(PurpleChat *chat);

/**
 * Returns the account the chat belongs to.
 *
 * @param chat  The chat.
 *
 * @return  The account the chat belongs to.
 */
PurpleAccount *purple_chat_get_account(PurpleChat *chat);

/**
 * Get a hashtable containing information about a chat.
 *
 * @param chat  The chat.
 *
 * @constreturn  The hashtable.
 */
GHashTable *purple_chat_get_components(PurpleChat *chat);

/**
 * Returns the group of which the buddy is a member.
 *
 * @param buddy   The buddy
 * @return        The group or NULL if the buddy is not in a group
 */
PurpleGroup *purple_buddy_get_group(PurpleBuddy *buddy);


/**
 * Returns a list of accounts that have buddies in this group
 *
 * @param g The group
 *
 * @return A GSList of accounts (which must be freed), or NULL if the group
 *         has no accounts.
 */
GSList *purple_group_get_accounts(PurpleGroup *g);

/**
 * Determines whether an account owns any buddies in a given group
 *
 * @param g       The group to search through.
 * @param account The account.
 *
 * @return TRUE if there are any buddies in the group, or FALSE otherwise.
 */
gboolean purple_group_on_account(PurpleGroup *g, PurpleAccount *account);

/**
 * Returns the name of a group.
 *
 * @param group The group.
 *
 * @return The name of the group.
 */
const char *purple_group_get_name(PurpleGroup *group);

/**
 * Called when an account connects.  Tells the UI to update all the
 * buddies.
 *
 * @param account   The account
 */
void purple_blist_add_account(PurpleAccount *account);


/**
 * Called when an account disconnects.  Sets the presence of all the buddies to 0
 * and tells the UI to update them.
 *
 * @param account   The account
 */
void purple_blist_remove_account(PurpleAccount *account);


/**
 * Determines the total size of a group
 *
 * @param group  The group
 * @param offline Count buddies in offline accounts
 * @return The number of buddies in the group
 */
int purple_blist_get_group_size(PurpleGroup *group, gboolean offline);

/**
 * Determines the number of online buddies in a group
 *
 * @param group The group
 * @return The number of online buddies in the group, or 0 if the group is NULL
 */
int purple_blist_get_group_online_count(PurpleGroup *group);

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
 * @param account  The account the buddy is added to.
 * @param username The username of the buddy.
 * @param group    The name of the group to place the buddy in.
 * @param alias    The optional alias for the buddy.
 */
void purple_blist_request_add_buddy(PurpleAccount *account, const char *username,
								  const char *group, const char *alias);

/**
 * Requests from the user information needed to add a chat to the
 * buddy list.
 *
 * @param account The account the buddy is added to.
 * @param group   The optional group to add the chat to.
 * @param alias   The optional alias for the chat.
 * @param name    The required chat name.
 */
void purple_blist_request_add_chat(PurpleAccount *account, PurpleGroup *group,
								 const char *alias, const char *name);

/**
 * Requests from the user information needed to add a group to the
 * buddy list.
 */
void purple_blist_request_add_group(void);

/**
 * Checks whether a named setting exists for a node in the buddy list
 *
 * @param node  The node to check from which to check settings
 * @param key   The identifier of the data
 *
 * @return TRUE if a value exists, or FALSE if there is no setting
 */
gboolean purple_blist_node_has_setting(PurpleBlistNode *node, const char *key);

/**
 * Associates a boolean with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void purple_blist_node_set_bool(PurpleBlistNode *node, const char *key, gboolean value);

/**
 * Retrieves a named boolean setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or FALSE if there is no setting
 */
gboolean purple_blist_node_get_bool(PurpleBlistNode *node, const char *key);

/**
 * Associates an integer with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void purple_blist_node_set_int(PurpleBlistNode *node, const char *key, int value);

/**
 * Retrieves a named integer setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or 0 if there is no setting
 */
int purple_blist_node_get_int(PurpleBlistNode *node, const char *key);

/**
 * Associates a string with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void purple_blist_node_set_string(PurpleBlistNode *node, const char *key,
		const char *value);

/**
 * Retrieves a named string setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or NULL if there is no setting
 */
const char *purple_blist_node_get_string(PurpleBlistNode *node, const char *key);

/**
 * Removes a named setting from a blist node
 *
 * @param node  The node from which to remove the setting
 * @param key   The name of the setting
 */
void purple_blist_node_remove_setting(PurpleBlistNode *node, const char *key);

/**
 * Set the flags for the given node.  Setting a node's flags will overwrite
 * the old flags, so if you want to save them, you must first call
 * purple_blist_node_get_flags and modify that appropriately.
 *
 * @param node  The node on which to set the flags.
 * @param flags The flags to set.  This is a bitmask.
 */
void purple_blist_node_set_flags(PurpleBlistNode *node, PurpleBlistNodeFlags flags);

/**
 * Get the current flags on a given node.
 *
 * @param node The node from which to get the flags.
 *
 * @return The flags on the node.  This is a bitmask.
 */
PurpleBlistNodeFlags purple_blist_node_get_flags(PurpleBlistNode *node);

/**
 * Get the type of a given node.
 *
 * @param node The node.
 *
 * @return The type of the node.
 */
PurpleBlistNodeType purple_blist_node_get_type(PurpleBlistNode *node);

/*@}*/

/**
 * Retrieves the extended menu items for a buddy list node.
 * @param n The blist node for which to obtain the extended menu items.
 * @return  A list of PurpleMenuAction items, as harvested by the
 *          blist-node-extended-menu signal.
 */
GList *purple_blist_node_get_extended_menu(PurpleBlistNode *n);

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for the buddy list.
 *
 * @param ops The ops struct.
 */
void purple_blist_set_ui_ops(PurpleBlistUiOps *ops);

/**
 * Returns the UI operations structure to be used for the buddy list.
 *
 * @return The UI operations structure.
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
 * @return The buddy list subsystem handle.
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

#endif /* _PURPLE_BLIST_H_ */
