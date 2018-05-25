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

#ifndef _PURPLE_INTERNAL_H_
#define _PURPLE_INTERNAL_H_
/*
 * SECTION:internal
 * @section_id: libpurple-internal
 * @short_description: <filename>internal.h</filename>
 * @title: Internal definitions and includes
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* for SIOCGIFCONF  in SKYOS */
#ifdef SKYOS
#include <net/sockios.h>
#endif
/*
 * If we're using NLS, make sure gettext works.  If not, then define
 * dummy macros in place of the normal gettext macros.
 *
 * Also, the perl XS config.h file sometimes defines _  So we need to
 * make sure _ isn't already defined before trying to define it.
 *
 * The Singular/Plural/Number ngettext dummy definition below was
 * taken from an email to the texinfo mailing list by Manuel Guerrero.
 * Thank you Manuel, and thank you Alex's good friend Google.
 */
#ifdef ENABLE_NLS
#  include <locale.h>
#  include <libintl.h>
#  undef printf
#  define _(String) ((const char *)dgettext(PACKAGE, String))
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  include <locale.h>
#  define N_(String) (String)
#  ifndef _
#    define _(String) ((const char *)String)
#  endif
#  define ngettext(Singular, Plural, Number) ((Number == 1) ? ((const char *)Singular) : ((const char *)Plural))
#  define dngettext(Domain, Singular, Plural, Number) ((Number == 1) ? ((const char *)Singular) : ((const char *)Plural))
#endif

#ifdef HAVE_ENDIAN_H
# include <endian.h>
#endif

#define MSG_LEN 2048
/* The above should normally be the same as BUF_LEN,
 * but just so we're explicitly asking for the max message
 * length. */
#define BUF_LEN MSG_LEN
#define BUF_LONG BUF_LEN * 2

#include <sys/types.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/time.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#ifndef _WIN32
# include <netinet/in.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <sys/un.h>
# include <sys/utsname.h>
# include <netdb.h>
# include <signal.h>
# include <unistd.h>
#endif

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

#include <glib.h>
#include <glib/gstdio.h>

#ifdef _WIN32
#include "win32/win32dep.h"
#endif

#ifdef HAVE_CONFIG_H
#if SIZEOF_TIME_T == 4
#	define PURPLE_TIME_T_MODIFIER "lu"
#elif SIZEOF_TIME_T == 8
#	define PURPLE_TIME_T_MODIFIER "zu"
#else
#error Unknown size of time_t
#endif
#endif

#define PURPLE_STATIC_ASSERT(condition, message) \
	{ typedef char static_assertion_failed_ ## message \
	[(condition) ? 1 : -1]; static_assertion_failed_ ## message dummy; \
	(void)dummy; }

/* This is meant to track use-after-free errors.
 * TODO: it should be disabled in released code. */
#define PURPLE_ASSERT_CONNECTION_IS_VALID(gc) \
	_purple_assert_connection_is_valid(gc, __FILE__, __LINE__)

#ifdef __clang__

#define PURPLE_BEGIN_IGNORE_CAST_ALIGN \
	_Pragma ("clang diagnostic push") \
	_Pragma ("clang diagnostic ignored \"-Wcast-align\"")

#define PURPLE_END_IGNORE_CAST_ALIGN \
	_Pragma ("clang diagnostic pop")

#else

#define PURPLE_BEGIN_IGNORE_CAST_ALIGN
#define PURPLE_END_IGNORE_CAST_ALIGN

#endif /* __clang__ */

#include <glib-object.h>

#ifdef __COVERITY__

/* avoid TAINTED_SCALAR warning */
#undef g_utf8_next_char
#define g_utf8_next_char(p) (char *)((p) + 1)

#endif

typedef union
{
	struct sockaddr sa;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
	struct sockaddr_storage storage;
} common_sockaddr_t;

#define PURPLE_WEBSITE "https://pidgin.im/"
#define PURPLE_DEVEL_WEBSITE "https://developer.pidgin.im/"


/* INTERNAL FUNCTIONS */

#include "accounts.h"
#include "connection.h"

/**
 * _purple_account_set_current_error:
 * @account:  The account to set the error for.
 * @new_err:  The #PurpleConnectionErrorInfo instance representing the
 *                 error.
 *
 * Sets an error for an account.
 */
void _purple_account_set_current_error(PurpleAccount *account,
                                       PurpleConnectionErrorInfo *new_err);

/**
 * _purple_account_to_xmlnode:
 * @account:  The account
 *
 * Get an XML description of an account.
 *
 * Returns:  The XML description of the account.
 */
PurpleXmlNode *_purple_account_to_xmlnode(PurpleAccount *account);

/**
 * _purple_blist_get_last_child:
 * @node:  The node whose last child is to be retrieved.
 *
 * Returns the last child of a particular node.
 *
 * Returns: The last child of the node.
 */
PurpleBlistNode *_purple_blist_get_last_child(PurpleBlistNode *node);

/* This is for the accounts code to notify the buddy icon code that
 * it's done loading.  We may want to replace this with a signal. */
void
_purple_buddy_icons_account_loaded_cb(void);

/* This is for the buddy list to notify the buddy icon code that
 * it's done loading.  We may want to replace this with a signal. */
void
_purple_buddy_icons_blist_loaded_cb(void);

/**
 * _purple_connection_new:
 * @account:  The account the connection should be connecting to.
 * @regist:   Whether we are registering a new account or just
 *                 trying to do a normal signon.
 * @password: The password to use.
 *
 * Creates a connection to the specified account and either connects
 * or attempts to register a new account.  If you are logging in,
 * the connection uses the current active status for this account.
 * So if you want to sign on as "away," for example, you need to
 * have called purple_account_set_status(account, "away").
 * (And this will call purple_account_connect() automatically).
 *
 * Note: This function should only be called by purple_account_connect()
 *       in account.c.  If you're trying to sign on an account, use that
 *       function instead.
 */
void _purple_connection_new(PurpleAccount *account, gboolean regist,
                            const char *password);
/**
 * _purple_connection_new_unregister:
 * @account:  The account to unregister
 * @password: The password to use.
 * @cb: Optional callback to be called when unregistration is complete
 * @user_data: user data to pass to the callback
 *
 * Tries to unregister the account on the server. If the account is not
 * connected, also creates a new connection.
 *
 * Note: This function should only be called by purple_account_unregister()
 *       in account.c.
 */
void _purple_connection_new_unregister(PurpleAccount *account, const char *password,
                                       PurpleAccountUnregistrationCb cb, void *user_data);
/**
 * _purple_connection_wants_to_die:
 * @gc:  The connection to check
 *
 * Checks if a connection is disconnecting, and should not attempt to reconnect.
 *
 * Note: This function should only be called by purple_account_set_enabled()
 *       in account.c.
 */
gboolean _purple_connection_wants_to_die(const PurpleConnection *gc);

/**
 * _purple_connection_add_active_chat:
 * @gc:    The connection
 * @chat:  The chat conversation to add
 *
 * Adds a chat to the active chats list of a connection
 *
 * Note: This function should only be called by purple_serv_got_joined_chat()
 *       in server.c.
 */
void _purple_connection_add_active_chat(PurpleConnection *gc,
                                        PurpleChatConversation *chat);
/**
 * _purple_connection_remove_active_chat:
 * @gc:    The connection
 * @chat:  The chat conversation to remove
 *
 * Removes a chat from the active chats list of a connection
 *
 * Note: This function should only be called by purple_serv_got_chat_left()
 *       in server.c.
 */
void _purple_connection_remove_active_chat(PurpleConnection *gc,
                                           PurpleChatConversation *chat);

/**
 * _purple_conversations_update_cache:
 * @conv:    The conversation.
 * @name:    The new name. If no change, use %NULL.
 * @account: The new account. If no change, use %NULL.
 *
 * Updates the conversation cache to use a new conversation name and/or
 * account. This function only updates the conversation cache. It is the
 * caller's responsibility to actually update the conversation.
 *
 * Note: This function should only be called by purple_conversation_set_name()
 *       and purple_conversation_set_account() in conversation.c.
 */
void _purple_conversations_update_cache(PurpleConversation *conv,
		const char *name, PurpleAccount *account);

/**
 * _purple_statuses_get_primitive_scores:
 *
 * Note: This function should only be called by
 *       purple_buddy_presence_compute_score() in presence.c.
 *
 * Returns: The primitive scores array from status.c.
 */
int *_purple_statuses_get_primitive_scores(void);

/**
 * _purple_blist_get_localized_default_group_name:
 *
 * Returns the name of default group for previously used non-English
 * localization. It's used for merging default group, in case when roster
 * contains localized name.
 *
 * Please note, prpls shouldn't save default group name depending on current
 * locale. So, this function is mostly for libpurple2 compatibility. And for
 * improperly written prpls.
 */
const gchar *
_purple_blist_get_localized_default_group_name(void);

/**
 * Sets most commonly used socket flags: O_NONBLOCK and FD_CLOEXEC.
 *
 * @param fd The file descriptor for the socket.
 *
 * @return TRUE if succeeded, FALSE otherwise.
 */
gboolean
_purple_network_set_common_socket_flags(int fd);

/**
 * A fstat alternative, like g_stat for stat.
 *
 * @param fd The file descriptor.
 * @param st The stat buffer.
 *
 * @return the result just like for fstat.
 */
int
_purple_fstat(int fd, GStatBuf *st);

/**
 * _purple_message_init: (skip)
 *
 * Initializes the #PurpleMessage subsystem.
 */
void
_purple_message_init(void);

/**
 * _purple_message_uninit: (skip)
 *
 * Uninitializes the #PurpleMessage subsystem.
 */
void
_purple_message_uninit(void);

void
_purple_assert_connection_is_valid(PurpleConnection *gc,
	const gchar *file, int line);

/**
 * _purple_conversation_write_common:
 * @conv:    The conversation.
 * @msg:     The message.
 *
 * Writes to a conversation window.
 *
 * This function should not be used to write IM or chat messages. Use
 * purple_conversation_write_message() instead. This function will
 * most likely call this anyway, but it may do it's own formatting,
 * sound playback, etc. depending on whether the conversation is a chat or an
 * IM.
 *
 * See purple_conversation_write_message().
 */
void
_purple_conversation_write_common(PurpleConversation *conv, PurpleMessage *msg);

#endif /* _PURPLE_INTERNAL_H_ */
