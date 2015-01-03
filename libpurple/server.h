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

#ifndef _PURPLE_SERVER_H_
#define _PURPLE_SERVER_H_
/**
 * SECTION:server
 * @section_id: libpurple-server
 * @short_description: <filename>server.h</filename>
 * @title: Server API
 */

#include "accounts.h"
#include "conversations.h"
#include "message.h"
#include "prpl.h"

G_BEGIN_DECLS

/**
 * purple_serv_send_typing:
 * @gc:    The connection over which to send the typing notification.
 * @name:  The user to send the typing notification to.
 * @state: One of PURPLE_IM_TYPING, PURPLE_IM_TYPED, or PURPLE_IM_NOT_TYPING.
 *
 * Send a typing message to a given user over a given connection.
 *
 * Returns: A quiet-period, specified in seconds, where Purple will not
 *         send any additional typing notification messages.  Most
 *         protocols should return 0, which means that no additional
 *         PURPLE_IM_TYPING messages need to be sent.  If this is 5, for
 *         example, then Purple will wait five seconds, and if the Purple
 *         user is still typing then Purple will send another PURPLE_IM_TYPING
 *         message.
 */
/* TODO Could probably move this into the conversation API. */
unsigned int purple_serv_send_typing(PurpleConnection *gc, const char *name, PurpleIMTypingState state);

void purple_serv_move_buddy(PurpleBuddy *, PurpleGroup *, PurpleGroup *);
int  purple_serv_send_im(PurpleConnection *, PurpleMessage *msg);

/**
 * purple_get_attention_type_from_code:
 *
 * Get information about an account's attention commands, from the protocol.
 *
 * Returns: The attention command numbered 'code' from the protocol's attention_types, or NULL.
 */
PurpleAttentionType *purple_get_attention_type_from_code(PurpleAccount *account, guint type_code);

void purple_serv_get_info(PurpleConnection *, const char *);
void purple_serv_set_info(PurpleConnection *, const char *);

void purple_serv_add_permit(PurpleConnection *, const char *);
void purple_serv_add_deny(PurpleConnection *, const char *);
void purple_serv_rem_permit(PurpleConnection *, const char *);
void purple_serv_rem_deny(PurpleConnection *, const char *);
void purple_serv_set_permit_deny(PurpleConnection *);
void purple_serv_chat_invite(PurpleConnection *, int, const char *, const char *);
void purple_serv_chat_leave(PurpleConnection *, int);
int  purple_serv_chat_send(PurpleConnection *, int, PurpleMessage *);
void purple_serv_alias_buddy(PurpleBuddy *);
void purple_serv_got_alias(PurpleConnection *gc, const char *who, const char *alias);

/**
 * purple_serv_got_private_alias:
 * @gc: The connection on which the alias was received.
 * @who: The name of the buddy whose alias was received.
 * @alias: The alias that was received.
 *
 * A protocol should call this when it retrieves a private alias from
 * the server.  Private aliases are the aliases the user sets, while public
 * aliases are the aliases or display names that buddies set for themselves.
 */
void purple_serv_got_private_alias(PurpleConnection *gc, const char *who, const char *alias);


/**
 * purple_serv_got_typing:
 * @gc:      The connection on which the typing message was received.
 * @name:    The name of the remote user.
 * @timeout: If this is a number greater than 0, then
 *        Purple will wait this number of seconds and then
 *        set this buddy to the PURPLE_IM_NOT_TYPING state.  This
 *        is used by protocols that send repeated typing messages
 *        while the user is composing the message.
 * @state:   The typing state received
 *
 * Receive a typing message from a remote user.  Either PURPLE_IM_TYPING
 * or PURPLE_IM_TYPED.  If the user has stopped typing then use
 * purple_serv_got_typing_stopped instead.
 *
 * @todo Could probably move this into the conversation API.
 */
void purple_serv_got_typing(PurpleConnection *gc, const char *name, int timeout,
					 PurpleIMTypingState state);

/**
 * purple_serv_got_typing_stopped:
 *
 * @todo Could probably move this into the conversation API.
 */
void purple_serv_got_typing_stopped(PurpleConnection *gc, const char *name);

void purple_serv_got_im(PurpleConnection *gc, const char *who, const char *msg,
				 PurpleMessageFlags flags, time_t mtime);

/**
 * purple_serv_join_chat:
 * @data: The hash function should be g_str_hash() and the equal
 *             function should be g_str_equal().
 */
void purple_serv_join_chat(PurpleConnection *, GHashTable *data);

/**
 * purple_serv_reject_chat:
 * @data: The hash function should be g_str_hash() and the equal
 *             function should be g_str_equal().
 */
void purple_serv_reject_chat(PurpleConnection *, GHashTable *data);

/**
 * purple_serv_got_chat_invite:
 * @gc:      The connection on which the invite arrived.
 * @name:    The name of the chat you're being invited to.
 * @who:     The username of the person inviting the account.
 * @message: The optional invite message.
 * @data:    The components necessary if you want to call purple_serv_join_chat().
 *                The hash function should be g_str_hash() and the equal
 *                function should be g_str_equal().
 *
 * Called by a protocol when an account is invited into a chat.
 */
void purple_serv_got_chat_invite(PurpleConnection *gc, const char *name,
						  const char *who, const char *message,
						  GHashTable *data);

/**
 * purple_serv_got_joined_chat:
 * @gc:   The connection on which the chat was joined.
 * @id:   The id of the chat, assigned by the protocol.
 * @name: The name of the chat.
 *
 * Called by a protocol when an account has joined a chat.
 *
 * Returns:     The resulting conversation
 */
PurpleChatConversation *purple_serv_got_joined_chat(PurpleConnection *gc,
									   int id, const char *name);
/**
 * purple_serv_got_join_chat_failed:
 * @gc:      The connection on which chat joining failed
 * @data:    The components passed to purple_serv_join_chat() originally.
 *                The hash function should be g_str_hash() and the equal
 *                function should be g_str_equal().
 *
 * Called by a protocol when an attempt to join a chat via purple_serv_join_chat()
 * fails.
 */
void purple_serv_got_join_chat_failed(PurpleConnection *gc, GHashTable *data);

/**
 * purple_serv_got_chat_left:
 * @g:  The connection on which the chat was left.
 * @id: The id of the chat, as assigned by the protocol.
 *
 * Called by a protocol when an account has left a chat.
 */
void purple_serv_got_chat_left(PurpleConnection *g, int id);

/**
 * purple_serv_got_chat_in:
 * @g:       The connection on which the message was received.
 * @id:      The id of the chat, as assigned by the protocol.
 * @who:     The name of the user who sent the message.
 * @flags:   The flags of the message.
 * @message: The message received in the chat.
 * @mtime:   The time when the message was received.
 *
 * Called by a protocol when a message has been received in a chat.
 */
void purple_serv_got_chat_in(PurpleConnection *g, int id, const char *who,
					  PurpleMessageFlags flags, const char *message, time_t mtime);
void purple_serv_send_file(PurpleConnection *gc, const char *who, const char *file);

G_END_DECLS

#endif /* _PURPLE_SERVER_H_ */

