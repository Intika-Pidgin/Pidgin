/**
 * @file protocols.h Protocols API
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

#ifndef _PURPLE_PROTOCOLS_H_
#define _PURPLE_PROTOCOLS_H_

#define PURPLE_PROTOCOLS_DOMAIN  (g_quark_from_static_string("protocols"))

#define PURPLE_TYPE_PROTOCOL_ACTION  (purple_protocol_action_get_type())

/** @copydoc _PurpleProtocolAction */
typedef struct _PurpleProtocolAction PurpleProtocolAction;
typedef void (*PurpleProtocolActionCallback)(PurpleProtocolAction *);

#define PURPLE_TYPE_ATTENTION_TYPE  (purple_attention_type_get_type())

/** Represents "nudges" and "buzzes" that you may send to a buddy to attract
 *  their attention (or vice-versa).
 */
typedef struct _PurpleAttentionType PurpleAttentionType;

/**************************************************************************/
/** @name Basic Protocol Information                                      */
/**************************************************************************/

typedef enum  /*< flags >*/
{
	PURPLE_ICON_SCALE_DISPLAY = 0x01,		/**< We scale the icon when we display it */
	PURPLE_ICON_SCALE_SEND = 0x02			/**< We scale the icon before we send it to the server */
} PurpleIconScaleRules;

/**
 * Represents an entry containing information that must be supplied by the
 * user when joining a chat.
 */
typedef struct _PurpleProtocolChatEntry PurpleProtocolChatEntry;

/**
 * Protocol options
 *
 * These should all be stuff that some protocols can do and others can't.
 */
typedef enum  /*< flags >*/
{
	/**
	 * User names are unique to a chat and are not shared between rooms.
	 *
	 * XMPP lets you choose what name you want in chats, so it shouldn't
	 * be pulling the aliases from the buddy list for the chat list;
	 * it gets annoying.
	 */
	OPT_PROTO_UNIQUE_CHATNAME = 0x00000004,

	/**
	 * Chat rooms have topics.
	 *
	 * IRC and XMPP support this.
	 */
	OPT_PROTO_CHAT_TOPIC = 0x00000008,

	/**
	 * Don't require passwords for sign-in.
	 *
	 * Zephyr doesn't require passwords, so there's no
	 * need for a password prompt.
	 */
	OPT_PROTO_NO_PASSWORD = 0x00000010,

	/**
	 * Notify on new mail.
	 *
	 * MSN and Yahoo notify you when you have new mail.
	 */
	OPT_PROTO_MAIL_CHECK = 0x00000020,

	/**
	 * Images in IMs.
	 *
	 * Oscar lets you send images in direct IMs.
	 */
	OPT_PROTO_IM_IMAGE = 0x00000040,

	/**
	 * Allow passwords to be optional.
	 *
	 * Passwords in IRC are optional, and are needed for certain
	 * functionality.
	 */
	OPT_PROTO_PASSWORD_OPTIONAL = 0x00000080,

	/**
	 * Allows font size to be specified in sane point size
	 *
	 * Probably just XMPP and Y!M
	 */
	OPT_PROTO_USE_POINTSIZE = 0x00000100,

	/**
	 * Set the Register button active even when the username has not
	 * been specified.
	 *
	 * Gadu-Gadu doesn't need a username to register new account (because
	 * usernames are assigned by the server).
	 */
	OPT_PROTO_REGISTER_NOSCREENNAME = 0x00000200,

	/**
	 * Indicates that slash commands are native to this protocol.
	 * Used as a hint that unknown commands should not be sent as messages.
	 */
	OPT_PROTO_SLASH_COMMANDS_NATIVE = 0x00000400,

	/**
	 * Indicates that this protocol supports sending a user-supplied message
	 * along with an invitation.
	 */
	OPT_PROTO_INVITE_MESSAGE = 0x00000800,

	/**
	 * Indicates that this protocol supports sending a user-supplied message
	 * along with an authorization acceptance.
	 */
	OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE = 0x00001000,

	/**
	 * Indicates that this protocol supports sending a user-supplied message
	 * along with an authorization denial.
	 */
	OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE = 0x00002000

} PurpleProtocolOptions;

#include "media.h"
#include "protocol.h"
#include "status.h"

#define PURPLE_TYPE_PROTOCOL_CHAT_ENTRY  (purple_protocol_chat_entry_get_type())

/** @copydoc PurpleProtocolChatEntry */
struct _PurpleProtocolChatEntry {
	const char *label;       /**< User-friendly name of the entry */
	const char *identifier;  /**< Used by the protocol to identify the option */
	gboolean required;       /**< True if it's required */
	gboolean is_int;         /**< True if the entry expects an integer */
	int min;                 /**< Minimum value in case of integer */
	int max;                 /**< Maximum value in case of integer */
	gboolean secret;         /**< True if the entry is secret (password) */
};

/**
 * Represents an action that the protocol can perform. This shows up in the
 * Accounts menu, under a submenu with the name of the account.
 */
struct _PurpleProtocolAction {
	char *label;
	PurpleProtocolActionCallback callback;
	PurpleConnection *connection;
	gpointer user_data;
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Attention Type API                                              */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the #PurpleAttentionType boxed structure.
 */
GType purple_attention_type_get_type(void);

/**
 * Creates a new #PurpleAttentionType object and sets its mandatory parameters.
 *
 * @param ulname A non-localized string that can be used by UIs in need of such
 *               non-localized strings.  This should be the same as @a name,
 *               without localization.
 * @param name A localized string that the UI may display for the event. This
 *             should be the same string as @a ulname, with localization.
 * @param inc_desc A localized description shown when the event is received.
 * @param out_desc A localized description shown when the event is sent.
 *
 * @return A pointer to the new object.
 */
PurpleAttentionType *purple_attention_type_new(const char *ulname, const char *name,
								const char *inc_desc, const char *out_desc);

/**
 * Sets the displayed name of the attention-demanding event.
 *
 * @param type The attention type.
 * @param name The localized name that will be displayed by UIs. This should be
 *             the same string given as the unlocalized name, but with
 *             localization.
 */
void purple_attention_type_set_name(PurpleAttentionType *type, const char *name);

/**
 * Sets the description of the attention-demanding event shown in  conversations
 * when the event is received.
 *
 * @param type The attention type.
 * @param desc The localized description for incoming events.
 */
void purple_attention_type_set_incoming_desc(PurpleAttentionType *type, const char *desc);

/**
 * Sets the description of the attention-demanding event shown in conversations
 * when the event is sent.
 *
 * @param type The attention type.
 * @param desc The localized description for outgoing events.
 */
void purple_attention_type_set_outgoing_desc(PurpleAttentionType *type, const char *desc);

/**
 * Sets the name of the icon to display for the attention event; this is optional.
 *
 * @param type The attention type.
 * @param name The icon's name.
 * @note Icons are optional for attention events.
 */
void purple_attention_type_set_icon_name(PurpleAttentionType *type, const char *name);

/**
 * Sets the unlocalized name of the attention event; some UIs may need this,
 * thus it is required.
 *
 * @param type The attention type.
 * @param ulname The unlocalized name.  This should be the same string given as
 *               the localized name, but without localization.
 */
void purple_attention_type_set_unlocalized_name(PurpleAttentionType *type, const char *ulname);

/**
 * Get the attention type's name as displayed by the UI.
 *
 * @param type The attention type.
 *
 * @return The name.
 */
const char *purple_attention_type_get_name(const PurpleAttentionType *type);

/**
 * Get the attention type's description shown when the event is received.
 *
 * @param type The attention type.
 * @return The description.
 */
const char *purple_attention_type_get_incoming_desc(const PurpleAttentionType *type);

/**
 * Get the attention type's description shown when the event is sent.
 *
 * @param type The attention type.
 * @return The description.
 */
const char *purple_attention_type_get_outgoing_desc(const PurpleAttentionType *type);

/**
 * Get the attention type's icon name.
 *
 * @param type The attention type.
 * @return The icon name or @c NULL if unset/empty.
 * @note Icons are optional for attention events.
 */
const char *purple_attention_type_get_icon_name(const PurpleAttentionType *type);

/**
 * Get the attention type's unlocalized name; this is useful for some UIs.
 *
 * @param type The attention type
 * @return The unlocalized name.
 */
const char *purple_attention_type_get_unlocalized_name(const PurpleAttentionType *type);

/*@}*/

/**************************************************************************/
/** @name Protocol Action API                                             */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the #PurpleProtocolAction boxed structure.
 */
GType purple_protocol_action_get_type(void);

/**
 * Allocates and returns a new PurpleProtocolAction. Use this to add actions in
 * a list in the get_actions function of the protocol.
 *
 * @param label    The description of the action to show to the user.
 * @param callback The callback to call when the user selects this action.
 */
PurpleProtocolAction *purple_protocol_action_new(const char* label,
		PurpleProtocolActionCallback callback);

/**
 * Frees a PurpleProtocolAction
 *
 * @param action The PurpleProtocolAction to free.
 */
void purple_protocol_action_free(PurpleProtocolAction *action);

/*@}*/

/**************************************************************************/
/** @name Protocol Chat Entry API                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the #PurpleProtocolChatEntry boxed structure.
 */
GType purple_protocol_chat_entry_get_type(void);

/*@}*/

/**************************************************************************/
/** @name Protocol API                                                    */
/**************************************************************************/
/*@{*/

/**
 * Notifies Purple that our account's idle state and time have changed.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account.
 * @param idle      The user's idle state.
 * @param idle_time The user's idle time.
 */
void purple_protocol_got_account_idle(PurpleAccount *account, gboolean idle,
                                      time_t idle_time);

/**
 * Notifies Purple of our account's log-in time.
 *
 * This is meant to be called from protocols.
 *
 * @param account    The account the user is on.
 * @param login_time The user's log-in time.
 */
void purple_protocol_got_account_login_time(PurpleAccount *account,
                                            time_t login_time);

/**
 * Notifies Purple that our account's status has changed.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param status_id The status ID.
 * @param ...       A NULL-terminated list of attribute IDs and values,
 *                  beginning with the value for @a attr_id.
 */
void purple_protocol_got_account_status(PurpleAccount *account,
                                        const char *status_id, ...)
                                        G_GNUC_NULL_TERMINATED;

/**
 * Notifies Purple that our account's actions have changed. This is only
 * called after the initial connection. Emits the account-actions-changed
 * signal.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account.
 *
 * @see account-actions-changed
 */
void purple_protocol_got_account_actions(PurpleAccount *account);

/**
 * Notifies Purple that a buddy's idle state and time have changed.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param name      The name of the buddy.
 * @param idle      The user's idle state.
 * @param idle_time The user's idle time.  This is the time at
 *                  which the user became idle, in seconds since
 *                  the epoch.  If the protocol does not know this value
 *                  then it should pass 0.
 */
void purple_protocol_got_user_idle(PurpleAccount *account, const char *name,
                                   gboolean idle, time_t idle_time);

/**
 * Notifies Purple of a buddy's log-in time.
 *
 * This is meant to be called from protocols.
 *
 * @param account    The account the user is on.
 * @param name       The name of the buddy.
 * @param login_time The user's log-in time.
 */
void purple_protocol_got_user_login_time(PurpleAccount *account,
                                         const char *name, time_t login_time);

/**
 * Notifies Purple that a buddy's status has been activated.
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param name      The name of the buddy.
 * @param status_id The status ID.
 * @param ...       A NULL-terminated list of attribute IDs and values,
 *                  beginning with the value for @a attr_id.
 */
void purple_protocol_got_user_status(PurpleAccount *account, const char *name,
                                     const char *status_id, ...)
                                     G_GNUC_NULL_TERMINATED;

/**
 * Notifies libpurple that a buddy's status has been deactivated
 *
 * This is meant to be called from protocols.
 *
 * @param account   The account the user is on.
 * @param name      The name of the buddy.
 * @param status_id The status ID.
 */
void purple_protocol_got_user_status_deactive(PurpleAccount *account,
                                              const char *name,
                                              const char *status_id);

/**
 * Informs the server that our account's status changed.
 *
 * @param account    The account the user is on.
 * @param old_status The previous status.
 * @param new_status The status that was activated, or deactivated
 *                   (in the case of independent statuses).
 */
void purple_protocol_change_account_status(PurpleAccount *account,
                                           PurpleStatus *old_status,
                                           PurpleStatus *new_status);

/**
 * Retrieves the list of stock status types from a protocol.
 *
 * @param account The account the user is on.
 * @param presence The presence for which we're going to get statuses
 *
 * @return List of statuses
 */
GList *purple_protocol_get_statuses(PurpleAccount *account,
                                    PurplePresence *presence);

/**
 * Send an attention request message.
 *
 * @param gc The connection to send the message on.
 * @param who Whose attention to request.
 * @param type_code An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send. 0 if protocol only defines one (for example, Yahoo and
 *                  MSN), but some protocols define more (MySpaceIM).
 *
 * Note that you can't send arbitrary PurpleAttentionType's, because there is
 * only a fixed set of attention commands.
 */
void purple_protocol_send_attention(PurpleConnection *gc, const char *who,
                                    guint type_code);

/**
 * Process an incoming attention message.
 *
 * @param gc The connection that received the attention message.
 * @param who Who requested your attention.
 * @param type_code An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send.
 */
void purple_protocol_got_attention(PurpleConnection *gc, const char *who,
                                   guint type_code);

/**
 * Process an incoming attention message in a chat.
 *
 * @param gc The connection that received the attention message.
 * @param id The chat id.
 * @param who Who requested your attention.
 * @param type_code An index into the protocol's attention_types list
 *                  determining the type of the attention request command to
 *                  send.
 */
void purple_protocol_got_attention_in_chat(PurpleConnection *gc, int id,
                                           const char *who, guint type_code);

/**
 * Determines if the contact supports the given media session type.
 *
 * @param account The account the user is on.
 * @param who The name of the contact to check capabilities for.
 *
 * @return The media caps the contact supports.
 */
PurpleMediaCaps purple_protocol_get_media_caps(PurpleAccount *account,
                                               const char *who);

/**
 * Initiates a media session with the given contact.
 *
 * @param account The account the user is on.
 * @param who The name of the contact to start a session with.
 * @param type The type of media session to start.
 *
 * @return TRUE if the call succeeded else FALSE. (Doesn't imply the media
 *         session or stream will be successfully created)
 */
gboolean purple_protocol_initiate_media(PurpleAccount *account,
                                        const char *who,
                                        PurpleMediaSessionType type);

/**
 * Signals that the protocol received capabilities for the given contact.
 *
 * This function is intended to be used only by protocols.
 *
 * @param account The account the user is on.
 * @param who The name of the contact for which capabilities have been received.
 */
void purple_protocol_got_media_caps(PurpleAccount *account, const char *who);

/*@}*/

/**************************************************************************/
/** @name Protocols API                                                   */
/**************************************************************************/
/*@{*/

/**
 * Finds a protocol by ID.
 *
 * @param id The protocol's ID.
 */
PurpleProtocol *purple_protocols_find(const char *id);

/**
 * Adds a protocol to the list of protocols.
 *
 * @param protocol_type  The type of the protocol to add.
 * @param error  Return location for a #GError or @c NULL. If provided, this
 *               will be set to the reason if adding fails.
 *
 * @return The protocol instance if the protocol was added, else @c NULL.
 */
PurpleProtocol *purple_protocols_add(GType protocol_type, GError **error);

/**
 * Removes a protocol from the list of protocols. This will disconnect all
 * connected accounts using this protocol, and free the protocol's user splits
 * and protocol options.
 *
 * @param protocol  The protocol to remove.
 * @param error  Return location for a #GError or @c NULL. If provided, this
 *               will be set to the reason if removing fails.
 *
 * @return TRUE if the protocol was removed, else FALSE.
 */
gboolean purple_protocols_remove(PurpleProtocol *protocol, GError **error);

/**
 * Returns a list of all loaded protocols.
 *
 * @return A list of all loaded protocols. The list is owned by the caller, and
 *         must be g_list_free()d to avoid leaking the nodes.
 */
GList *purple_protocols_get_all(void);

/*@}*/

/**************************************************************************/
/** @name Protocols Subsytem API                                          */
/**************************************************************************/
/*@{*/

/**
 * Initializes the protocols subsystem.
 */
void purple_protocols_init(void);

/**
 * Returns the protocols subsystem handle.
 *
 * @return The protocols subsystem handle.
 */
void *purple_protocols_get_handle(void);

/**
 * Uninitializes the protocols subsystem.
 */
void purple_protocols_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PROTOCOLS_H_ */
