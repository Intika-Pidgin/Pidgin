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
 */
#include "internal.h"
#include "glibcompat.h"

#include "buddylist.h"
#include "cmds.h"
#include "conversation.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "enums.h"
#include "notify.h"
#include "prefs.h"
#include "protocol.h"
#include "request.h"
#include "signals.h"
#include "smiley-list.h"
#include "util.h"

#define PURPLE_CONVERSATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CONVERSATION, PurpleConversationPrivate))

typedef struct _PurpleConversationPrivate  PurpleConversationPrivate;

/* General private data for a conversation */
struct _PurpleConversationPrivate
{
	PurpleAccount *account;           /* The user using this conversation. */

	char *name;                       /* The name of the conversation.     */
	char *title;                      /* The window title.                 */

	gboolean logging;                 /* The status of logging.            */

	GList *logs;                      /* This conversation's logs          */

	PurpleConversationUiOps *ui_ops;  /* UI-specific operations.           */

	PurpleConnectionFlags features;   /* The supported features            */
	GList *message_history; /* Message history, as a GList of PurpleMessages */

	PurpleE2eeState *e2ee_state;      /* End-to-end encryption state.      */

	/* The list of remote smileys. This should be per-buddy (PurpleBuddy),
	 * but we don't have any class for people not on our buddy
	 * list (PurpleDude?). So, if we have one, we should switch to it. */
	PurpleSmileyList *remote_smileys;
};

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_ACCOUNT,
	PROP_NAME,
	PROP_TITLE,
	PROP_LOGGING,
	PROP_FEATURES,
	PROP_LAST
};

static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];

static void
common_send(PurpleConversation *conv, const char *message, PurpleMessageFlags msgflags)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	PurpleMessage *msg;
	char *displayed = NULL;
	const char *sent;
	int err = 0;

	g_return_if_fail(priv != NULL);

	if (*message == '\0')
		return;

	account = purple_conversation_get_account(conv);
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	gc = purple_account_get_connection(account);
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	/* Always linkfy the text for display, unless we're
	 * explicitly asked to do otheriwse*/
	if (!(msgflags & PURPLE_MESSAGE_INVISIBLE)) {
		if(msgflags & PURPLE_MESSAGE_NO_LINKIFY)
			displayed = g_strdup(message);
		else
			displayed = purple_markup_linkify(message);
	}

	if (displayed && (priv->features & PURPLE_CONNECTION_FLAG_HTML) &&
		!(msgflags & PURPLE_MESSAGE_RAW)) {
		sent = displayed;
	} else
		sent = message;

	msgflags |= PURPLE_MESSAGE_SEND;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		msg = purple_message_new_outgoing(
			purple_conversation_get_name(conv), sent, msgflags);

		purple_signal_emit(purple_conversations_get_handle(), "sending-im-msg",
			account, msg);

		if (!purple_message_is_empty(msg)) {

			err = purple_serv_send_im(gc, msg);

			if ((err > 0) && (displayed != NULL)) {
				/* revert the contents in case sending-im-msg changed it */
				purple_message_set_contents(msg, displayed);
				purple_conversation_write_message(conv, msg);
			}

			purple_signal_emit(purple_conversations_get_handle(),
				"sent-im-msg", account, msg);
		}
	}
	else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		int id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));

		msg = purple_message_new_outgoing(NULL, sent, msgflags);

		purple_signal_emit(purple_conversations_get_handle(),
			"sending-chat-msg", account, msg, id);

		if (!purple_message_is_empty(msg)) {
			err = purple_serv_chat_send(gc, id, msg);

			purple_signal_emit(purple_conversations_get_handle(),
				"sent-chat-msg", account, msg, id);
		}
	}

	if (err < 0) {
		const char *who;
		const char *msg;

		who = purple_conversation_get_name(conv);

		if (err == -E2BIG) {
			msg = _("Unable to send message: The message is too large.");

			if (!purple_conversation_present_error(who, account, msg)) {
				char *msg2 = g_strdup_printf(_("Unable to send message to %s."), who);
				purple_notify_error(gc, NULL, msg2,
					_("The message is too large."),
					purple_request_cpar_from_connection(gc));
				g_free(msg2);
			}
		}
		else if (err == -ENOTCONN) {
			purple_debug(PURPLE_DEBUG_ERROR, "conversation",
					   "Not yet connected.\n");
		}
		else {
			msg = _("Unable to send message.");

			if (!purple_conversation_present_error(who, account, msg)) {
				char *msg2 = g_strdup_printf(_("Unable to send message to %s."), who);
				purple_notify_error(gc, NULL, msg2, NULL,
					purple_request_cpar_from_connection(gc));
				g_free(msg2);
			}
		}
	}

	g_free(displayed);
}

static void
open_log(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	priv->logs = g_list_append(NULL, purple_log_new(PURPLE_IS_CHAT_CONVERSATION(conv) ? PURPLE_LOG_CHAT :
							   PURPLE_LOG_IM, priv->name, priv->account,
							   conv, time(NULL), NULL));
}

/* Functions that deal with PurpleMessage history */

static void
add_message_to_history(PurpleConversation *conv, PurpleMessage *msg)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(msg != NULL);

	g_object_ref(msg);
	priv->message_history = g_list_prepend(priv->message_history, msg);
}

/**************************************************************************
 * Conversation API
 **************************************************************************/
void
purple_conversation_present(PurpleConversation *conv) {
	PurpleConversationUiOps *ops;

	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));

	ops = purple_conversation_get_ui_ops(conv);
	if(ops && ops->present)
		ops->present(conv);
}

void
purple_conversation_set_features(PurpleConversation *conv, PurpleConnectionFlags features)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	priv->features = features;

	g_object_notify_by_pspec(G_OBJECT(conv), properties[PROP_FEATURES]);

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_FEATURES);
}

PurpleConnectionFlags
purple_conversation_get_features(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->features;
}

static PurpleConversationUiOps *
purple_conversation_ui_ops_copy(PurpleConversationUiOps *ops)
{
	PurpleConversationUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleConversationUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_conversation_ui_ops_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleConversationUiOps",
				(GBoxedCopyFunc)purple_conversation_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

void
purple_conversation_set_ui_ops(PurpleConversation *conv,
							 PurpleConversationUiOps *ops)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (priv->ui_ops == ops)
		return;

	if (priv->ui_ops != NULL && priv->ui_ops->destroy_conversation != NULL)
		priv->ui_ops->destroy_conversation(conv);

	priv->ui_ops = ops;
}

PurpleConversationUiOps *
purple_conversation_get_ui_ops(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->ui_ops;
}

void
purple_conversation_set_account(PurpleConversation *conv, PurpleAccount *account)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (account == purple_conversation_get_account(conv))
		return;

	_purple_conversations_update_cache(conv, NULL, account);
	priv->account = account;

	g_object_notify_by_pspec(G_OBJECT(conv), properties[PROP_ACCOUNT]);

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_ACCOUNT);
}

PurpleAccount *
purple_conversation_get_account(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

PurpleConnection *
purple_conversation_get_connection(const PurpleConversation *conv)
{
	PurpleAccount *account;

	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conv), NULL);

	account = purple_conversation_get_account(conv);

	if (account == NULL)
		return NULL;

	return purple_account_get_connection(account);
}

void
purple_conversation_set_title(PurpleConversation *conv, const char *title)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv  != NULL);
	g_return_if_fail(title != NULL);

	g_free(priv->title);
	priv->title = g_strdup(title);

	if (!g_object_get_data(G_OBJECT(conv), "is-finalizing"))
		g_object_notify_by_pspec(G_OBJECT(conv), properties[PROP_TITLE]);

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_TITLE);
}

const char *
purple_conversation_get_title(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->title;
}

void
purple_conversation_autoset_title(PurpleConversation *conv)
{
	PurpleAccount *account;
	PurpleBuddy *b;
	PurpleChat *chat;
	const char *text = NULL, *name;

	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (account && ((b = purple_blist_find_buddy(account, name)) != NULL))
			text = purple_buddy_get_contact_alias(b);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		if (account && ((chat = purple_blist_find_chat(account, name)) != NULL))
			text = purple_chat_get_name(chat);
	}

	if(text == NULL)
		text = name;

	purple_conversation_set_title(conv, text);
}

void
purple_conversation_set_name(PurpleConversation *conv, const char *name)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	_purple_conversations_update_cache(conv, name, NULL);

	g_free(priv->name);
	priv->name = g_strdup(name);

	g_object_notify_by_pspec(G_OBJECT(conv), properties[PROP_NAME]);

	purple_conversation_autoset_title(conv);
	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_NAME);
}

const char *
purple_conversation_get_name(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

void
purple_conversation_set_e2ee_state(PurpleConversation *conv,
	PurpleE2eeState *state)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (state != NULL && purple_e2ee_state_get_provider(state) !=
		purple_e2ee_provider_get_main())
	{
		purple_debug_error("conversation",
			"This is not the main e2ee provider");

		return;
	}

	if (state == priv->e2ee_state)
		return;

	if (state)
		purple_e2ee_state_ref(state);
	purple_e2ee_state_unref(priv->e2ee_state);
	priv->e2ee_state = state;

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_E2EE);
}

PurpleE2eeState *
purple_conversation_get_e2ee_state(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	PurpleE2eeProvider *provider;

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->e2ee_state == NULL)
		return NULL;

	provider = purple_e2ee_provider_get_main();
	if (provider == NULL)
		return NULL;

	if (purple_e2ee_state_get_provider(priv->e2ee_state) != provider) {
		purple_debug_warning("conversation",
			"e2ee state has invalid provider set");
		return NULL;
	}

	return priv->e2ee_state;
}

void
purple_conversation_set_logging(PurpleConversation *conv, gboolean log)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	if (priv->logging != log)
	{
		priv->logging = log;
		if (log && priv->logs == NULL)
			open_log(conv);

		g_object_notify_by_pspec(G_OBJECT(conv), properties[PROP_LOGGING]);

		purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_LOGGING);
	}
}

gboolean
purple_conversation_is_logging(const PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->logging;
}

void
purple_conversation_close_logs(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	g_list_foreach(priv->logs, (GFunc)purple_log_free, NULL);
	g_list_free(priv->logs);
	priv->logs = NULL;
}

void
_purple_conversation_write_common(PurpleConversation *conv, PurpleMessage *pmsg)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc = NULL;
	PurpleAccount *account;
	PurpleConversationUiOps *ops;
	PurpleBuddy *b;
	int plugin_return;
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	/* int logging_font_options = 0; */

	g_return_if_fail(priv != NULL);
	g_return_if_fail(pmsg != NULL);

	ops = purple_conversation_get_ui_ops(conv);

	account = purple_conversation_get_account(conv);

	if (account != NULL)
		gc = purple_account_get_connection(account);

	if (PURPLE_IS_CHAT_CONVERSATION(conv) &&
		(gc != NULL && !g_slist_find(purple_connection_get_active_chats(gc), conv)))
		return;

	if (PURPLE_IS_IM_CONVERSATION(conv) &&
		!g_list_find(purple_conversations_get_all(), conv))
		return;

	plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
		purple_conversations_get_handle(),
		(PURPLE_IS_IM_CONVERSATION(conv) ? "writing-im-msg" : "writing-chat-msg"),
		conv, pmsg));

	if (purple_message_is_empty(pmsg))
		return;

	if (plugin_return)
		return;

	if (account != NULL) {
		protocol = purple_protocols_find(purple_account_get_protocol_id(account));

		if (PURPLE_IS_IM_CONVERSATION(conv) ||
			!(purple_protocol_get_options(protocol) & OPT_PROTO_UNIQUE_CHATNAME)) {

			if (purple_message_get_flags(pmsg) & PURPLE_MESSAGE_SEND) {
				const gchar *alias;

				b = purple_blist_find_buddy(account,
					purple_account_get_username(account));

				if (purple_account_get_private_alias(account) != NULL)
					alias = purple_account_get_private_alias(account);
				else if (b != NULL && !purple_strequal(purple_buddy_get_name(b),
					purple_buddy_get_contact_alias(b)))
				{
					alias = purple_buddy_get_contact_alias(b);
				} else if (purple_connection_get_display_name(gc) != NULL)
					alias = purple_connection_get_display_name(gc);
				else
					alias = purple_account_get_username(account);

				purple_message_set_author_alias(pmsg, alias);
			}
			else if (purple_message_get_flags(pmsg) & PURPLE_MESSAGE_RECV)
			{
				/* TODO: PurpleDude - folks not on the buddy list */
				b = purple_blist_find_buddy(account,
					purple_message_get_author(pmsg));

				if (b != NULL) {
					purple_message_set_author_alias(pmsg,
						purple_buddy_get_contact_alias(b));
				}
			}
		}
	}

	if (!(purple_message_get_flags(pmsg) & PURPLE_MESSAGE_NO_LOG) && purple_conversation_is_logging(conv)) {
		GList *log;

		log = priv->logs;
		while (log != NULL) {
			purple_log_write((PurpleLog *)log->data,
				purple_message_get_flags(pmsg),
				purple_message_get_author_alias(pmsg),
				purple_message_get_time(pmsg),
				purple_message_get_contents(pmsg));
			log = log->next;
		}
	}

	if (ops) {
		if (PURPLE_IS_CHAT_CONVERSATION(conv) && ops->write_chat)
			ops->write_chat(PURPLE_CHAT_CONVERSATION(conv), pmsg);
		else if (PURPLE_IS_IM_CONVERSATION(conv) && ops->write_im)
			ops->write_im(PURPLE_IM_CONVERSATION(conv), pmsg);
		else if (ops->write_conv)
			ops->write_conv(conv, pmsg);
	}

	add_message_to_history(conv, pmsg);

	purple_signal_emit(purple_conversations_get_handle(),
		(PURPLE_IS_IM_CONVERSATION(conv) ? "wrote-im-msg" : "wrote-chat-msg"),
		conv, pmsg);
}

void
purple_conversation_write_message(PurpleConversation *conv, PurpleMessage *msg)
{
	PurpleConversationClass *klass = NULL;

	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));

	klass = PURPLE_CONVERSATION_GET_CLASS(conv);

	if (klass && klass->write_message)
		klass->write_message(conv, msg);
}

void purple_conversation_write_system_message(PurpleConversation *conv,
	const gchar *message, PurpleMessageFlags flags)
{
	_purple_conversation_write_common(conv,
		purple_message_new_system(message, flags));
}

void
purple_conversation_send(PurpleConversation *conv, const char *message)
{
	purple_conversation_send_with_flags(conv, message, 0);
}

void
purple_conversation_send_with_flags(PurpleConversation *conv, const char *message,
		PurpleMessageFlags flags)
{
	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));
	g_return_if_fail(message != NULL);

	common_send(conv, message, flags);
}

gboolean
purple_conversation_has_focus(PurpleConversation *conv)
{
	gboolean ret = FALSE;
	PurpleConversationUiOps *ops;

	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conv), FALSE);

	ops = purple_conversation_get_ui_ops(conv);

	if (ops != NULL && ops->has_focus != NULL)
		ret = ops->has_focus(conv);

	return ret;
}

/*
 * TODO: Need to make sure calls to this function happen in the core
 * instead of the UI.  That way UIs have less work to do, and the
 * core/UI split is cleaner.  Also need to make sure this is called
 * when chats are added/removed from the blist.
 */
void
purple_conversation_update(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-updated", conv, type);
}

gboolean purple_conversation_present_error(const char *who, PurpleAccount *account, const char *what)
{
	PurpleConversation *conv;

	g_return_val_if_fail(who != NULL, FALSE);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(what != NULL, FALSE);

	conv = purple_conversations_find_with_account(who, account);
	if (conv != NULL)
		purple_conversation_write_system_message(conv, what, PURPLE_MESSAGE_ERROR);
	else
		return FALSE;

	return TRUE;
}

static void
purple_conversation_send_confirm_cb(gpointer *data)
{
	PurpleConversation *conv = data[0];
	char *message = data[1];

	g_free(data);
	common_send(conv, message, 0);
}

void
purple_conversation_send_confirm(PurpleConversation *conv, const char *message)
{
	char *text;
	gpointer *data;
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(message != NULL);

	if (priv->ui_ops != NULL && priv->ui_ops->send_confirm != NULL)
	{
		priv->ui_ops->send_confirm(conv, message);
		return;
	}

	text = g_strdup_printf("You are about to send the following message:\n%s", message);
	data = g_new0(gpointer, 2);
	data[0] = conv;
	data[1] = (gpointer)message;

	purple_request_action(conv, NULL, _("Send Message"), text, 0,
		purple_request_cpar_from_account(
			purple_conversation_get_account(conv)),
		data, 2, _("_Send Message"),
		G_CALLBACK(purple_conversation_send_confirm_cb), _("Cancel"), NULL);
}

GList *
purple_conversation_get_extended_menu(PurpleConversation *conv)
{
	GList *menu = NULL;

	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conv), NULL);

	purple_signal_emit(purple_conversations_get_handle(),
			"conversation-extended-menu", conv, &menu);
	return menu;
}

void purple_conversation_clear_message_history(PurpleConversation *conv)
{
	GList *list;
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_if_fail(priv != NULL);

	list = priv->message_history;
	g_list_free_full(list, g_object_unref);
	priv->message_history = NULL;

	purple_signal_emit(purple_conversations_get_handle(),
			"cleared-message-history", conv);
}

GList *purple_conversation_get_message_history(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->message_history;
}

void purple_conversation_set_ui_data(PurpleConversation *conv, gpointer ui_data)
{
	g_return_if_fail(PURPLE_IS_CONVERSATION(conv));

	conv->ui_data = ui_data;
}

gpointer purple_conversation_get_ui_data(const PurpleConversation *conv)
{
	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conv), NULL);

	return conv->ui_data;
}

gboolean
purple_conversation_do_command(PurpleConversation *conv, const gchar *cmdline,
				const gchar *markup, gchar **error)
{
	char *mark = (markup && *markup) ? NULL : g_markup_escape_text(cmdline, -1), *err = NULL;
	PurpleCmdStatus status = purple_cmd_do_command(conv, cmdline, mark ? mark : markup, error ? error : &err);
	g_free(mark);
	g_free(err);
	return (status == PURPLE_CMD_STATUS_OK);
}

gssize
purple_conversation_get_max_message_size(PurpleConversation *conv)
{
	PurpleProtocol *protocol;

	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conv), 0);

	protocol = purple_connection_get_protocol(
		purple_conversation_get_connection(conv));

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), 0);

	return purple_protocol_client_iface_get_max_message_size(protocol, conv);
}

PurpleSmiley *
purple_conversation_add_remote_smiley(PurpleConversation *conv,
	const gchar *shortcut)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	PurpleSmiley *smiley;

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(shortcut != NULL, NULL);
	g_return_val_if_fail(shortcut[0] != '\0', NULL);

	if (priv->remote_smileys == NULL) {
		priv->remote_smileys = purple_smiley_list_new();
		g_object_set(priv->remote_smileys,
			"drop-failed-remotes", TRUE, NULL);
	}

	smiley = purple_smiley_list_get_by_shortcut(
		priv->remote_smileys, shortcut);

	/* smiley was already added */
	if (smiley)
		return NULL;

	smiley = purple_smiley_new_remote(shortcut);

	if (!purple_smiley_list_add(priv->remote_smileys, smiley)) {
		purple_debug_error("conversation", "failed adding remote "
			"smiley to the list");
		g_object_unref(smiley);
		return NULL;
	}

	/* priv->remote_smileys holds the only one ref */
	g_object_unref(smiley);
	return smiley;
}

PurpleSmiley *
purple_conversation_get_remote_smiley(PurpleConversation *conv,
	const gchar *shortcut)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(shortcut != NULL, NULL);
	g_return_val_if_fail(shortcut[0] != '\0', NULL);

	if (priv->remote_smileys == NULL)
		return NULL;

	return purple_smiley_list_get_by_shortcut(
		priv->remote_smileys, shortcut);
}

PurpleSmileyList *
purple_conversation_get_remote_smileys(PurpleConversation *conv)
{
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->remote_smileys;
}


/**************************************************************************
 * GObject code
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(obj);
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);

	switch (param_id) {
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_NAME:
			g_free(priv->name);
			priv->name = g_strdup(g_value_get_string(value));
			break;
		case PROP_TITLE:
			g_free(priv->title);
			priv->title = g_strdup(g_value_get_string(value));
			break;
		case PROP_LOGGING:
			purple_conversation_set_logging(conv, g_value_get_boolean(value));
			break;
		case PROP_FEATURES:
			purple_conversation_set_features(conv, g_value_get_flags(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_conversation_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(obj);

	switch (param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_conversation_get_account(conv));
			break;
		case PROP_NAME:
			g_value_set_string(value, purple_conversation_get_name(conv));
			break;
		case PROP_TITLE:
			g_value_set_string(value, purple_conversation_get_title(conv));
			break;
		case PROP_LOGGING:
			g_value_set_boolean(value, purple_conversation_is_logging(conv));
			break;
		case PROP_FEATURES:
			g_value_set_flags(value, purple_conversation_get_features(conv));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_conversation_constructed(GObject *object)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(object);
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleConversationUiOps *ops;

	parent_class->constructed(object);

	g_object_get(object, "account", &account, NULL);
	gc = purple_account_get_connection(account);

	/* copy features from the connection. */
	purple_conversation_set_features(conv, purple_connection_get_flags(gc));

	/* add the conversation to the appropriate lists */
	purple_conversations_add(conv);

	/* Auto-set the title. */
	purple_conversation_autoset_title(conv);

	/* Don't move this.. it needs to be one of the last things done otherwise
	 * it causes mysterious crashes on my system.
	 *  -- Gary
	 */
	ops  = purple_conversations_get_ui_ops();
	purple_conversation_set_ui_ops(conv, ops);
	if (ops != NULL && ops->create_conversation != NULL)
		ops->create_conversation(conv);

	purple_signal_emit(purple_conversations_get_handle(),
					 "conversation-created", conv);

	g_object_unref(account);
}

static void
purple_conversation_dispose(GObject *object)
{
	g_object_set_data(object, "is-finalizing", GINT_TO_POINTER(TRUE));
}

/* GObject finalize function */
static void
purple_conversation_finalize(GObject *object)
{
	PurpleConversation *conv = PURPLE_CONVERSATION(object);
	PurpleConversationPrivate *priv = PURPLE_CONVERSATION_GET_PRIVATE(conv);
	PurpleConversationUiOps *ops  = purple_conversation_get_ui_ops(conv);

	purple_request_close_with_handle(conv);

	purple_e2ee_state_unref(priv->e2ee_state);
	priv->e2ee_state = NULL;

	/* remove from conversations and im/chats lists prior to emit */
	purple_conversations_remove(conv);

	purple_signal_emit(purple_conversations_get_handle(),
					 "deleting-conversation", conv);

	purple_conversation_close_logs(conv);
	purple_conversation_clear_message_history(conv);

	if (ops != NULL && ops->destroy_conversation != NULL)
		ops->destroy_conversation(conv);

	g_free(priv->name);
	g_free(priv->title);

	priv->name = NULL;
	priv->title = NULL;

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_conversation_class_init(PurpleConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_conversation_dispose;
	obj_class->finalize = purple_conversation_finalize;
	obj_class->constructed = purple_conversation_constructed;

	/* Setup properties */
	obj_class->get_property = purple_conversation_get_property;
	obj_class->set_property = purple_conversation_set_property;

	g_type_class_add_private(klass, sizeof(PurpleConversationPrivate));

	properties[PROP_ACCOUNT] = g_param_spec_object("account", "Account",
				"The account for the conversation.", PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_NAME] = g_param_spec_string("name", "Name",
				"The name of the conversation.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_TITLE] = g_param_spec_string("title", "Title",
				"The title of the conversation.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_LOGGING] = g_param_spec_boolean("logging", "Logging status",
				"Whether logging is enabled or not.", FALSE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_FEATURES] = g_param_spec_flags("features",
				"Connection features",
				"The connection features of the conversation.",
				PURPLE_TYPE_CONNECTION_FLAGS, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
purple_conversation_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleConversationClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_conversation_class_init,
			NULL,
			NULL,
			sizeof(PurpleConversation),
			0,
			NULL,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleConversation",
				&info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}
