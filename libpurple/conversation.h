/**
 * @file conversation.h Conversation API
 * @ingroup core
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
#ifndef _PURPLE_CONVERSATION_H_
#define _PURPLE_CONVERSATION_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/

/** @copydoc _PurpleConversation */
typedef struct _PurpleConversation           PurpleConversation;
/** @copydoc _PurpleConversation */
typedef struct _PurpleConversationClass      PurpleConversationClass;

/** @copydoc _PurpleConversationUiOps */
typedef struct _PurpleConversationUiOps      PurpleConversationUiOps;
/** @copydoc _PurpleConversationMessage */
typedef struct _PurpleConversationMessage    PurpleConversationMessage;

/**
 * Conversation update type.
 */
typedef enum
{
	PURPLE_CONVERSATION_UPDATE_ADD = 0, /**< The buddy associated with the conversation
	                                         was added.   */
	PURPLE_CONVERSATION_UPDATE_REMOVE,  /**< The buddy associated with the conversation
	                                         was removed. */
	PURPLE_CONVERSATION_UPDATE_ACCOUNT, /**< The purple_account was changed. */
	PURPLE_CONVERSATION_UPDATE_TYPING,  /**< The typing state was updated. */
	PURPLE_CONVERSATION_UPDATE_UNSEEN,  /**< The unseen state was updated. */
	PURPLE_CONVERSATION_UPDATE_LOGGING, /**< Logging for this conversation was
	                                         enabled or disabled. */
	PURPLE_CONVERSATION_UPDATE_TOPIC,   /**< The topic for a chat was updated. */
	/*
	 * XXX These need to go when we implement a more generic core/UI event
	 * system.
	 */
	PURPLE_CONVERSATION_ACCOUNT_ONLINE,  /**< One of the user's accounts went online.  */
	PURPLE_CONVERSATION_ACCOUNT_OFFLINE, /**< One of the user's accounts went offline. */
	PURPLE_CONVERSATION_UPDATE_AWAY,     /**< The other user went away.                */
	PURPLE_CONVERSATION_UPDATE_ICON,     /**< The other user's buddy icon changed.     */
	PURPLE_CONVERSATION_UPDATE_TITLE,
	PURPLE_CONVERSATION_UPDATE_CHATLEFT,

	PURPLE_CONVERSATION_UPDATE_FEATURES  /**< The features for a chat have changed */

} PurpleConversationUpdateType;

/**
 * Flags applicable to a message. Most will have send, recv or system.
 */
typedef enum /*< flags >*/
{
	PURPLE_CONVERSATION_MESSAGE_SEND        = 0x0001, /**< Outgoing message.        */
	PURPLE_CONVERSATION_MESSAGE_RECV        = 0x0002, /**< Incoming message.        */
	PURPLE_CONVERSATION_MESSAGE_SYSTEM      = 0x0004, /**< System message.          */
	PURPLE_CONVERSATION_MESSAGE_AUTO_RESP   = 0x0008, /**< Auto response.           */
	PURPLE_CONVERSATION_MESSAGE_ACTIVE_ONLY = 0x0010,  /**< Hint to the UI that this
	                                                        message should not be
	                                                        shown in conversations
	                                                        which are only open for
	                                                        internal UI purposes
	                                                        (e.g. for contact-aware
	                                                        conversations).         */
	PURPLE_CONVERSATION_MESSAGE_NICK        = 0x0020, /**< Contains your nick.      */
	PURPLE_CONVERSATION_MESSAGE_NO_LOG      = 0x0040, /**< Do not log.              */
	PURPLE_CONVERSATION_MESSAGE_WHISPER     = 0x0080, /**< Whispered message.       */
	PURPLE_CONVERSATION_MESSAGE_ERROR       = 0x0200, /**< Error message.           */
	PURPLE_CONVERSATION_MESSAGE_DELAYED     = 0x0400, /**< Delayed message.         */
	PURPLE_CONVERSATION_MESSAGE_RAW         = 0x0800, /**< "Raw" message - don't
	                                                        apply formatting        */
	PURPLE_CONVERSATION_MESSAGE_IMAGES      = 0x1000, /**< Message contains images  */
	PURPLE_CONVERSATION_MESSAGE_NOTIFY      = 0x2000, /**< Message is a notification */
	PURPLE_CONVERSATION_MESSAGE_NO_LINKIFY  = 0x4000, /**< Message should not be auto-
										                   linkified */
	PURPLE_CONVERSATION_MESSAGE_INVISIBLE   = 0x8000  /**< Message should not be displayed */
} PurpleConversationMessageFlags;

/**
 * Flags applicable to users in Chats.
 */
typedef enum /*< flags >*/
{
	PURPLE_CHAT_CONVERSATION_BUDDY_NONE     = 0x0000, /**< No flags                     */
	PURPLE_CHAT_CONVERSATION_BUDDY_VOICE    = 0x0001, /**< Voiced user or "Participant" */
	PURPLE_CHAT_CONVERSATION_BUDDY_HALFOP   = 0x0002, /**< Half-op                      */
	PURPLE_CHAT_CONVERSATION_BUDDY_OP       = 0x0004, /**< Channel Op or Moderator      */
	PURPLE_CHAT_CONVERSATION_BUDDY_FOUNDER  = 0x0008, /**< Channel Founder              */
	PURPLE_CHAT_CONVERSATION_BUDDY_TYPING   = 0x0010, /**< Currently typing             */
	PURPLE_CHAT_CONVERSATION_BUDDY_AWAY     = 0x0020  /**< Currently away.              */

} PurpleChatConversationBuddyFlags;

#include "account.h"
#include "buddyicon.h"
#include "log.h"

/**************************************************************************/
/** PurpleConversation                                                    */
/**************************************************************************/
/** Structure representing a conversation instance. */
struct _PurpleConversation
{
	/*< private >*/
	GObject gparent;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/** Base class for all #PurpleConversation's */
struct _PurpleConversationClass {
	/*< private >*/
	GObjectClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** PurpleConversationUiOps                                               */
/**************************************************************************/
/**
 * Conversation operations and events.
 *
 * Any UI representing a conversation must assign a filled-out
 * PurpleConversationUiOps structure to the PurpleConversation.
 */
struct _PurpleConversationUiOps
{
	/** Called when @a conv is created (but before the @ref
	 *  conversation-created signal is emitted).
	 */
	void (*create_conversation)(PurpleConversation *conv);

	/** Called just before @a conv is freed. */
	void (*destroy_conversation)(PurpleConversation *conv);
	/** Write a message to a chat.  If this field is @c NULL, libpurple will
	 *  fall back to using #write_conv.
	 *  @see purple_chat_conversation_write()
	 */
	void (*write_chat)(PurpleConversation *conv, const char *who,
	                  const char *message, PurpleConversationMessageFlags flags,
	                  time_t mtime);
	/** Write a message to an IM conversation.  If this field is @c NULL,
	 *  libpurple will fall back to using #write_conv.
	 *  @see purple_im_conversation_write()
	 */
	void (*write_im)(PurpleConversation *conv, const char *who,
	                 const char *message, PurpleConversationMessageFlags flags,
	                 time_t mtime);
	/** Write a message to a conversation.  This is used rather than the
	 *  chat- or im-specific ops for errors, system messages (such as "x is
	 *  now know as y"), and as the fallback if #write_im and #write_chat
	 *  are not implemented.  It should be implemented, or the UI will miss
	 *  conversation error messages and your users will hate you.
	 *
	 *  @see purple_conversation_write()
	 */
	void (*write_conv)(PurpleConversation *conv,
	                   const char *name,
	                   const char *alias,
	                   const char *message,
	                   PurpleConversationMessageFlags flags,
	                   time_t mtime);

	/** Add @a cbuddies to a chat.
	 *  @param cbuddies      A @c GList of #PurpleChatConversationBuddy structs.
	 *  @param new_arrivals  Whether join notices should be shown.
	 *                       (Join notices are actually written to the
	 *                       conversation by
	 *                       #purple_chat_conversation_add_users().)
	 */
	void (*chat_add_users)(PurpleConversation *conv,
	                       GList *cbuddies,
	                       gboolean new_arrivals);
	/** Rename the user in this chat named @a old_name to @a new_name.  (The
	 *  rename message is written to the conversation by libpurple.)
	 *  @param new_alias  @a new_name's new alias, if they have one.
	 *  @see purple_chat_conversation_add_users()
	 */
	void (*chat_rename_user)(PurpleConversation *conv, const char *old_name,
	                         const char *new_name, const char *new_alias);
	/** Remove @a users from a chat.
	 *  @param users    A @c GList of <tt>const char *</tt>s.
	 *  @see purple_chat_conversation_rename_user()
	 */
	void (*chat_remove_users)(PurpleConversation *conv, GList *users);
	/** Called when a user's flags are changed.
	 *  @see purple_chat_conversation_user_set_flags()
	 */
	void (*chat_update_user)(PurpleConversation *conv, const char *user);

	/** Present this conversation to the user; for example, by displaying
	 *  the IM dialog.
	 */
	void (*present)(PurpleConversation *conv);

	/** If this UI has a concept of focus (as in a windowing system) and
	 *  this conversation has the focus, return @c TRUE; otherwise, return
	 *  @c FALSE.
	 */
	gboolean (*has_focus)(PurpleConversation *conv);

	/* Custom Smileys */
	gboolean (*custom_smiley_add)(PurpleConversation *conv, const char *smile,
	                            gboolean remote);
	void (*custom_smiley_write)(PurpleConversation *conv, const char *smile,
	                            const guchar *data, gsize size);
	void (*custom_smiley_close)(PurpleConversation *conv, const char *smile);

	/** Prompt the user for confirmation to send @a message.  This function
	 *  should arrange for the message to be sent if the user accepts.  If
	 *  this field is @c NULL, libpurple will fall back to using
	 *  #purple_request_action().
	 */
	void (*send_confirm)(PurpleConversation *conv, const char *message);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Conversation API                                                */
/**************************************************************************/
/*@{*/

/** TODO GObjectify
 * Creates a new conversation of the specified type.
 *
 * @param type    The type of conversation.
 * @param account The account opening the conversation window on the purple
 *                user's end.
 * @param name    The name of the conversation. For PURPLE_CONVERSATION_TYPE_IM,
 *                this is the name of the buddy.
 *
 * @return The new conversation.
 */
PurpleConversation *purple_conversation_new(PurpleAccount *account,
										const char *name);

/** TODO dispose/fnalize
 * Destroys the specified conversation and removes it from the parent
 * window.
 *
 * If this conversation is the only one contained in the parent window,
 * that window is also destroyed.
 *
 * @param conv The conversation to destroy.
 */
void purple_conversation_destroy(PurpleConversation *conv);


/**
 * Present a conversation to the user. This allows core code to initiate a
 * conversation by displaying the IM dialog.
 * @param conv The conversation to present
 */
void purple_conversation_present(PurpleConversation *conv);


/** TODO REMOVE, return the GObject GType
 * Returns the specified conversation's type.
 *
 * @param conv The conversation.
 *
 * @return The conversation's type.
 */
/*PurpleConversationType purple_conversation_get_type(const PurpleConversation *conv);*/

/**
 * Sets the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 * @param ops  The UI conversation operations structure.
 */
void purple_conversation_set_ui_ops(PurpleConversation *conv,
								  PurpleConversationUiOps *ops);

/**
 * Returns the specified conversation's UI operations structure.
 *
 * @param conv The conversation.
 *
 * @return The operations structure.
 */
PurpleConversationUiOps *purple_conversation_get_ui_ops(const PurpleConversation *conv);

/**
 * Sets the specified conversation's purple_account.
 *
 * This purple_account represents the user using purple, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 * @param account The purple_account.
 */
void purple_conversation_set_account(PurpleConversation *conv,
                                   PurpleAccount *account);

/**
 * Returns the specified conversation's purple_account.
 *
 * This purple_account represents the user using purple, not the person the user
 * is having a conversation/chat/flame with.
 *
 * @param conv The conversation.
 *
 * @return The conversation's purple_account.
 */
PurpleAccount *purple_conversation_get_account(const PurpleConversation *conv);

/**
 * Returns the specified conversation's purple_connection.
 *
 * @param conv The conversation.
 *
 * @return The conversation's purple_connection.
 */
PurpleConnection *purple_conversation_get_connection(const PurpleConversation *conv);

/**
 * Sets the specified conversation's title.
 *
 * @param conv  The conversation.
 * @param title The title.
 */
void purple_conversation_set_title(PurpleConversation *conv, const char *title);

/**
 * Returns the specified conversation's title.
 *
 * @param conv The conversation.
 *
 * @return The title.
 */
const char *purple_conversation_get_title(const PurpleConversation *conv);

/**
 * Automatically sets the specified conversation's title.
 *
 * This function takes OPT_IM_ALIAS_TAB into account, as well as the
 * user's alias.
 *
 * @param conv The conversation.
 */
void purple_conversation_autoset_title(PurpleConversation *conv);

/**
 * Sets the specified conversation's name.
 *
 * @param conv The conversation.
 * @param name The conversation's name.
 */
void purple_conversation_set_name(PurpleConversation *conv, const char *name);

/**
 * Returns the specified conversation's name.
 *
 * @param conv The conversation.
 *
 * @return The conversation's name. If the conversation is an IM with a PurpleBuddy,
 *         then it's the name of the PurpleBuddy.
 */
const char *purple_conversation_get_name(const PurpleConversation *conv);

/**
 * Enables or disables logging for this conversation.
 *
 * @param conv The conversation.
 * @param log  @c TRUE if logging should be enabled, or @c FALSE otherwise.
 */
void purple_conversation_set_logging(PurpleConversation *conv, gboolean log);

/**
 * Returns whether or not logging is enabled for this conversation.
 *
 * @param conv The conversation.
 *
 * @return @c TRUE if logging is enabled, or @c FALSE otherwise.
 */
gboolean purple_conversation_is_logging(const PurpleConversation *conv);

/**
 * Closes any open logs for this conversation.
 *
 * Note that new logs will be opened as necessary (e.g. upon receipt of a
 * message, if the conversation has logging enabled. To disable logging for
 * the remainder of the conversation, use purple_conversation_set_logging().
 *
 * @param conv The conversation.
 */
void purple_conversation_close_logs(PurpleConversation *conv);

/**
 * Sets extra data for a conversation.
 *
 * @param conv The conversation.
 * @param key  The unique key.
 * @param data The data to assign.
 */
void purple_conversation_set_data(PurpleConversation *conv, const char *key,
								gpointer data);

/**
 * Returns extra data in a conversation.
 *
 * @param conv The conversation.
 * @param key  The unqiue key.
 *
 * @return The data associated with the key.
 */
gpointer purple_conversation_get_data(PurpleConversation *conv, const char *key);

/**
 * Writes to a conversation window.
 *
 * This function should not be used to write IM or chat messages. Use
 * purple_im_conversation_write() and purple_chat_conversation_write() instead. Those functions will
 * most likely call this anyway, but they may do their own formatting,
 * sound playback, etc.
 *
 * This can be used to write generic messages, such as "so and so closed
 * the conversation window."
 *
 * @param conv    The conversation.
 * @param who     The user who sent the message.
 * @param message The message.
 * @param flags   The message flags.
 * @param mtime   The time the message was sent.
 *
 * @see purple_im_conversation_write()
 * @see purple_chat_conversation_write()
 */
void purple_conversation_write(PurpleConversation *conv, const char *who,
		const char *message, PurpleConversationMessageFlags flags,
		time_t mtime);

/**
	Set the features as supported for the given conversation.
	@param conv      The conversation
	@param features  Bitset defining supported features
*/
void purple_conversation_set_features(PurpleConversation *conv,
		PurpleConnectionFlags features);


/**
	Get the features supported by the given conversation.
	@param conv  The conversation
*/
PurpleConnectionFlags purple_conversation_get_features(PurpleConversation *conv);

/**
 * Determines if a conversation has focus
 *
 * @param conv    The conversation.
 *
 * @return @c TRUE if the conversation has focus, @c FALSE if
 * it does not or the UI does not have a concept of conversation focus
 */
gboolean purple_conversation_has_focus(PurpleConversation *conv);

/**
 * Updates the visual status and UI of a conversation.
 *
 * @param conv The conversation.
 * @param type The update type.
 */
void purple_conversation_update(PurpleConversation *conv, PurpleConversationUpdateType type);

/**
 * Retrieve the message history of a conversation.
 *
 * @param conv   The conversation
 *
 * @return  A GList of PurpleConversationMessage's. The must not modify the list or the data within.
 *          The list contains the newest message at the beginning, and the oldest message at
 *          the end.
 */
GList *purple_conversation_get_message_history(PurpleConversation *conv);

/**
 * Clear the message history of a conversation.
 *
 * @param conv  The conversation
 */
void purple_conversation_clear_message_history(PurpleConversation *conv);

/**
 * Set the UI data associated with this conversation.
 *
 * @param conv			The conversation.
 * @param ui_data		A pointer to associate with this conversation.
 */
void purple_conversation_set_ui_data(PurpleConversation *conv, gpointer ui_data);

/**
 * Get the UI data associated with this conversation.
 *
 * @param conv			The conversation.
 *
 * @return The UI data associated with this conversation.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_conversation_get_ui_data(const PurpleConversation *conv);

/**
 * Sends a message to a conversation after confirming with
 * the user.
 *
 * This function is intended for use in cases where the user
 * hasn't explicitly and knowingly caused a message to be sent.
 * The confirmation ensures that the user isn't sending a
 * message by mistake.
 *
 * @param conv    The conversation.
 * @param message The message to send.
 */
void purple_conversation_send_confirm(PurpleConversation *conv, const char *message);

/**
 * Adds a smiley to the conversation's smiley tree. If this returns
 * @c TRUE you should call purple_conversation_custom_smiley_write() one or more
 * times, and then purple_conversation_custom_smiley_close(). If this returns
 * @c FALSE, either the conv or smile were invalid, or the icon was
 * found in the cache. In either case, calling write or close would
 * be an error.
 *
 * @param conv The conversation to associate the smiley with.
 * @param smile The text associated with the smiley
 * @param cksum_type The type of checksum.
 * @param chksum The checksum, as a NUL terminated base64 string.
 * @param remote @c TRUE if the custom smiley is set by the remote user (buddy).
 * @return      @c TRUE if an icon is expected, else FALSE. Note that
 *              it is an error to never call purple_conversation_custom_smiley_close if
 *              this function returns @c TRUE, but an error to call it if
 *              @c FALSE is returned.
 */

gboolean purple_conversation_custom_smiley_add(PurpleConversation *conv, const char *smile,
                                      const char *cksum_type, const char *chksum,
									  gboolean remote);

/**
 * Updates the image associated with the current smiley.
 *
 * @param conv The conversation associated with the smiley.
 * @param smile The text associated with the smiley.
 * @param data The actual image data.
 * @param size The length of the data.
 */

void purple_conversation_custom_smiley_write(PurpleConversation *conv,
                                   const char *smile,
                                   const guchar *data,
                                   gsize size);

/**
 * Close the custom smiley, all data has been written with
 * purple_conversation_custom_smiley_write, and it is no longer valid
 * to call that function on that smiley.
 *
 * @param conv The purple conversation associated with the smiley.
 * @param smile The text associated with the smiley
 */

void purple_conversation_custom_smiley_close(PurpleConversation *conv, const char *smile);

/**
 * Retrieves the extended menu items for the conversation.
 *
 * @param conv The conversation.
 *
 * @return  A list of PurpleMenuAction items, harvested by the
 *          chat-extended-menu signal. The list and the menuaction
 *          items should be freed by the caller.
 */
GList * purple_conversation_get_extended_menu(PurpleConversation *conv);

/**
 * Perform a command in a conversation. Similar to @see purple_cmd_do_command
 *
 * @param conv    The conversation.
 * @param cmdline The entire command including the arguments.
 * @param markup  @c NULL, or the formatted command line.
 * @param error   If the command failed errormsg is filled in with the appropriate error
 *                message, if not @c NULL. It must be freed by the caller with g_free().
 *
 * @return  @c TRUE if the command was executed successfully, @c FALSE otherwise.
 */
gboolean purple_conversation_do_command(PurpleConversation *conv,
		const gchar *cmdline, const gchar *markup, gchar **error);

/*@}*/

/**************************************************************************/
/** @name Conversation Helper API                                         */
/**************************************************************************/
/*@{*/

/**
 * Presents an IM-error to the user
 *
 * This is a helper function to find a conversation, write an error to it, and
 * raise the window.  If a conversation with this user doesn't already exist,
 * the function will return FALSE and the calling function can attempt to present
 * the error another way (purple_notify_error, most likely)
 *
 * @param who     The user this error is about
 * @param account The account this error is on
 * @param what    The error
 * @return        TRUE if the error was presented, else FALSE
 */
gboolean purple_conversation_helper_present_error(const char *who, PurpleAccount *account, const char *what);

/*@}*/

/**************************************************************************/
/** @name Conversation Message API                                        */
/**************************************************************************/
/*@{*/

/**
 * Get the sender from a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The name of the sender of the message
 */
const char *purple_conversation_message_get_sender(const PurpleConversationMessage *msg);

/**
 * Get the message from a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The name of the sender of the message
 */
const char *purple_conversation_message_get_message(const PurpleConversationMessage *msg);

/**
 * Get the message-flags of a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The message flags
 */
PurpleConversationMessageFlags purple_conversation_message_get_flags(const PurpleConversationMessage *msg);

/**
 * Get the timestamp of a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The timestamp of the message
 */
time_t purple_conversation_message_get_timestamp(const PurpleConversationMessage *msg);

/**
 * Get the alias from a PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The alias of the sender of the message
 */
const char *purple_conversation_message_get_alias(const PurpleConversationMessage *msg);

/**
 * Get the conversation associated with the PurpleConversationMessage
 *
 * @param msg   A PurpleConversationMessage
 *
 * @return   The conversation
 */
PurpleConversation *purple_conversation_message_get_conv(const PurpleConversationMessage *msg);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_CONVERSATION_H_ */
