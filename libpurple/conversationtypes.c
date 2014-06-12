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
#include "conversationtypes.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "enums.h"

#define SEND_TYPED_TIMEOUT_SECONDS 5

#define PURPLE_CHAT_CONVERSATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT_CONVERSATION, PurpleChatConversationPrivate))

typedef struct _PurpleChatConversationPrivate     PurpleChatConversationPrivate;

#define PURPLE_IM_CONVERSATION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_IM_CONVERSATION, PurpleIMConversationPrivate))

typedef struct _PurpleIMConversationPrivate       PurpleIMConversationPrivate;

#define PURPLE_CHAT_USER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CHAT_USER, PurpleChatUserPrivate))

typedef struct _PurpleChatUserPrivate  PurpleChatUserPrivate;

/*
 * Data specific to Chats.
 */
struct _PurpleChatConversationPrivate
{
	GList *ignored;     /* Ignored users.                            */
	char  *who;         /* The person who set the topic.             */
	char  *topic;       /* The topic.                                */
	int    id;          /* The chat ID.                              */
	char *nick;         /* Your nick in this chat.                   */
	gboolean left;      /* We left the chat and kept the window open */
	GHashTable *users;  /* Hash table of the users in the room.      */
};

/* Chat Property enums */
enum {
	CHAT_PROP_0,
	CHAT_PROP_TOPIC_WHO,
	CHAT_PROP_TOPIC,
	CHAT_PROP_ID,
	CHAT_PROP_NICK,
	CHAT_PROP_LEFT,
	CHAT_PROP_LAST
};

/*
 * Data specific to Instant Messages.
 */
struct _PurpleIMConversationPrivate
{
	PurpleIMTypingState typing_state;  /* The current typing state.    */
	guint  typing_timeout;             /* The typing timer handle.     */
	time_t type_again;                 /* The type again time.         */
	guint  send_typed_timeout;         /* The type again timer handle. */
	PurpleBuddyIcon *icon;             /* The buddy icon.              */
};

/* IM Property enums */
enum {
	IM_PROP_0,
	IM_PROP_TYPING_STATE,
	IM_PROP_ICON,
	IM_PROP_LAST
};

/*
 * Data for "Chat Buddies"
 */
struct _PurpleChatUserPrivate
{
	PurpleChatConversation *chat;  /* The chat                              */
	char *name;                    /* The chat participant's name in the
	                                  chat.                                 */
	char *alias;                   /* The chat participant's alias, if known;
	                                  NULL otherwise.                       */
	char *alias_key;               /* A string by which this user will be
	                                  sorted, or @c NULL if the user should be
	                                  sorted by its @name.
	                                  (This is currently always NULL.       */
	gboolean buddy;                /* TRUE if this chat participant is on
	                                  the buddy list; FALSE otherwise.      */
	PurpleChatUserFlags flags;     /* A bitwise OR of flags for this
	                                  participant, such as whether they
	                                  are a channel operator.               */
};

/* Chat User Property enums */
enum {
	CU_PROP_0,
	CU_PROP_CHAT,
	CU_PROP_NAME,
	CU_PROP_ALIAS,
	CU_PROP_FLAGS,
	CU_PROP_LAST
};

static PurpleConversationClass *parent_class;
static GObjectClass            *cb_parent_class;

static GParamSpec *chat_properties[CHAT_PROP_LAST];
static GParamSpec *im_properties[IM_PROP_LAST];
static GParamSpec *cu_properties[CU_PROP_LAST];

static int purple_chat_user_compare(PurpleChatUser *a,
		PurpleChatUser *b);

/**************************************************************************
 * IM Conversation API
 **************************************************************************/
static gboolean
reset_typing_cb(gpointer data)
{
	PurpleIMConversation *im = (PurpleIMConversation *)data;

	purple_im_conversation_set_typing_state(im, PURPLE_IM_NOT_TYPING);
	purple_im_conversation_stop_typing_timeout(im);

	return FALSE;
}

static gboolean
send_typed_cb(gpointer data)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(data);
	PurpleConnection *gc;
	const char *name;

	g_return_val_if_fail(im != NULL, FALSE);

	gc   = purple_conversation_get_connection(PURPLE_CONVERSATION(im));
	name = purple_conversation_get_name(PURPLE_CONVERSATION(im));

	if (gc != NULL && name != NULL) {
		/* We set this to 1 so that PURPLE_IM_TYPING will be sent
		 * if the Purple user types anything else.
		 */
		purple_im_conversation_set_type_again(im, 1);

		purple_serv_send_typing(gc, name, PURPLE_IM_TYPED);

		purple_debug(PURPLE_DEBUG_MISC, "conversationtypes", "typed...\n");
	}

	return FALSE;
}

void
purple_im_conversation_set_icon(PurpleIMConversation *im, PurpleBuddyIcon *icon)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->icon != icon)
	{
		purple_buddy_icon_unref(priv->icon);

		priv->icon = (icon == NULL ? NULL : purple_buddy_icon_ref(icon));

		g_object_notify_by_pspec(G_OBJECT(im), im_properties[IM_PROP_ICON]);
	}

	purple_conversation_update(PURPLE_CONVERSATION(im),
							 PURPLE_CONVERSATION_UPDATE_ICON);
}

PurpleBuddyIcon *
purple_im_conversation_get_icon(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->icon;
}

void
purple_im_conversation_set_typing_state(PurpleIMConversation *im, PurpleIMTypingState state)
{
	PurpleAccount *account;
	const char *name;
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	name = purple_conversation_get_name(PURPLE_CONVERSATION(im));
	account = purple_conversation_get_account(PURPLE_CONVERSATION(im));

	if (priv->typing_state != state)
	{
		priv->typing_state = state;

		g_object_notify_by_pspec(G_OBJECT(im),
				im_properties[IM_PROP_TYPING_STATE]);

		switch (state)
		{
			case PURPLE_IM_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing", account, name);
				break;
			case PURPLE_IM_TYPED:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typed", account, name);
				break;
			case PURPLE_IM_NOT_TYPING:
				purple_signal_emit(purple_conversations_get_handle(),
								   "buddy-typing-stopped", account, name);
				break;
		}

		purple_im_conversation_update_typing(im);
	}
}

PurpleIMTypingState
purple_im_conversation_get_typing_state(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->typing_state;
}

void
purple_im_conversation_start_typing_timeout(PurpleIMConversation *im, int timeout)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->typing_timeout > 0)
		purple_im_conversation_stop_typing_timeout(im);

	priv->typing_timeout = purple_timeout_add_seconds(timeout, reset_typing_cb, im);
}

void
purple_im_conversation_stop_typing_timeout(PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->typing_timeout == 0)
		return;

	purple_timeout_remove(priv->typing_timeout);
	priv->typing_timeout = 0;
}

guint
purple_im_conversation_get_typing_timeout(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->typing_timeout;
}

void
purple_im_conversation_set_type_again(PurpleIMConversation *im, unsigned int val)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (val == 0)
		priv->type_again = 0;
	else
		priv->type_again = time(NULL) + val;
}

time_t
purple_im_conversation_get_type_again(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->type_again;
}

void
purple_im_conversation_start_send_typed_timeout(PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	priv->send_typed_timeout = purple_timeout_add_seconds(SEND_TYPED_TIMEOUT_SECONDS,
	                                                    send_typed_cb, im);
}

void
purple_im_conversation_stop_send_typed_timeout(PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_if_fail(priv != NULL);

	if (priv->send_typed_timeout == 0)
		return;

	purple_timeout_remove(priv->send_typed_timeout);
	priv->send_typed_timeout = 0;
}

guint
purple_im_conversation_get_send_typed_timeout(const PurpleIMConversation *im)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(im);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->send_typed_timeout;
}

void
purple_im_conversation_update_typing(PurpleIMConversation *im)
{
	g_return_if_fail(PURPLE_IS_IM_CONVERSATION(im));

	purple_conversation_update(PURPLE_CONVERSATION(im),
							 PURPLE_CONVERSATION_UPDATE_TYPING);
}

static void
im_conversation_write_message(PurpleConversation *conv, PurpleMessage *msg)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(conv);
	gboolean is_recv;

	g_return_if_fail(im != NULL);
	g_return_if_fail(msg != NULL);

	is_recv = (purple_message_get_flags(msg) & PURPLE_MESSAGE_RECV);

	if (is_recv)
		purple_im_conversation_set_typing_state(im, PURPLE_IM_NOT_TYPING);

	_purple_conversation_write_common(conv, msg);
}

/**************************************************************************
 * GObject code for IMs
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_im_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(obj);

	switch (param_id) {
		case IM_PROP_TYPING_STATE:
			purple_im_conversation_set_typing_state(im, g_value_get_enum(value));
			break;
		case IM_PROP_ICON:
			purple_im_conversation_set_icon(im, g_value_get_pointer(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_im_conversation_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(obj);

	switch (param_id) {
		case IM_PROP_TYPING_STATE:
			g_value_set_enum(value, purple_im_conversation_get_typing_state(im));
			break;
		case IM_PROP_ICON:
			g_value_set_pointer(value, purple_im_conversation_get_icon(im));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void purple_im_conversation_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_IM_CONVERSATION(instance),
			PurpleIMConversation);
}

/* Called when done constructing */
static void
purple_im_conversation_constructed(GObject *object)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(object);
	PurpleAccount *account;
	PurpleBuddyIcon *icon;
	gchar *name;

	G_OBJECT_CLASS(parent_class)->constructed(object);

	g_object_get(object,
			"account", &account,
			"name",    &name,
			NULL);

	if ((icon = purple_buddy_icons_find(account, name)))
	{
		purple_im_conversation_set_icon(im, icon);
		/* purple_im_conversation_set_icon refs the icon. */
		purple_buddy_icon_unref(icon);
	}

	if (purple_prefs_get_bool("/purple/logging/log_ims"))
		purple_conversation_set_logging(PURPLE_CONVERSATION(im), TRUE);

	g_object_unref(account);
	g_free(name);
}

/* GObject dispose function */
static void
purple_im_conversation_dispose(GObject *object)
{
	PurpleIMConversationPrivate *priv = PURPLE_IM_CONVERSATION_GET_PRIVATE(object);

	if (priv->icon) {
		purple_buddy_icon_unref(priv->icon);
		priv->icon = NULL;
	}

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_im_conversation_finalize(GObject *object)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(object);
	PurpleConnection *gc = purple_conversation_get_connection(PURPLE_CONVERSATION(im));
	PurplePluginProtocolInfo *prpl_info = NULL;
	const char *name = purple_conversation_get_name(PURPLE_CONVERSATION(im));

	if (gc != NULL)
	{
		/* Still connected */
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));

		if (purple_prefs_get_bool("/purple/conversations/im/send_typing"))
			purple_serv_send_typing(gc, name, PURPLE_IM_NOT_TYPING);

		if (gc && prpl_info->convo_closed != NULL)
			prpl_info->convo_closed(gc, name);
	}

	purple_im_conversation_stop_typing_timeout(im);
	purple_im_conversation_stop_send_typed_timeout(im);

	PURPLE_DBUS_UNREGISTER_POINTER(im);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_im_conversation_class_init(PurpleIMConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleConversationClass *conv_class = PURPLE_CONVERSATION_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_im_conversation_dispose;
	obj_class->finalize = purple_im_conversation_finalize;
	obj_class->constructed = purple_im_conversation_constructed;

	/* Setup properties */
	obj_class->get_property = purple_im_conversation_get_property;
	obj_class->set_property = purple_im_conversation_set_property;

	conv_class->write_message = im_conversation_write_message;

	g_type_class_add_private(klass, sizeof(PurpleIMConversationPrivate));

	im_properties[IM_PROP_TYPING_STATE] = g_param_spec_enum("typing-state",
				"Typing state",
				"Status of the user's typing of a message.",
				PURPLE_TYPE_IM_TYPING_STATE, PURPLE_IM_NOT_TYPING,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
				G_PARAM_STATIC_STRINGS);

	im_properties[IM_PROP_ICON] = g_param_spec_pointer("icon",
				"Buddy icon",
				"The buddy icon for the IM.",
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
				G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, IM_PROP_LAST, im_properties);
}

GType
purple_im_conversation_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleIMConversationClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_im_conversation_class_init,
			NULL,
			NULL,
			sizeof(PurpleIMConversation),
			0,
			(GInstanceInitFunc)purple_im_conversation_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CONVERSATION,
				"PurpleIMConversation",
				&info, 0);
	}

	return type;
}

PurpleIMConversation *
purple_im_conversation_new(PurpleAccount *account, const char *name)
{
	PurpleIMConversation *im;
	PurpleConnection *gc;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((im = purple_conversations_find_im_with_account(name, account)) != NULL)
		return im;

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	im = g_object_new(PURPLE_TYPE_IM_CONVERSATION,
			"account", account,
			"name",    name,
			"title",   name,
			NULL);

	return im;
}

/**************************************************************************
 * Chat Conversation API
 **************************************************************************/
static guint
_purple_conversation_user_hash(gconstpointer data)
{
	const gchar *name = data;
	gchar *collated;
	guint hash;

	collated = g_utf8_collate_key(name, -1);
	hash     = g_str_hash(collated);
	g_free(collated);
	return hash;
}

static gboolean
_purple_conversation_user_equal(gconstpointer a, gconstpointer b)
{
	return !g_utf8_collate(a, b);
}

GList *
purple_chat_conversation_get_users(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return g_hash_table_get_values(priv->users);
}

guint
purple_chat_conversation_get_users_count(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, 0);

	return g_hash_table_size(priv->users);
}

void
purple_chat_conversation_ignore(PurpleChatConversation *chat, const char *name)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user isn't already ignored. */
	if (purple_chat_conversation_is_ignored_user(chat, name))
		return;

	purple_chat_conversation_set_ignored(chat,
		g_list_append(priv->ignored, g_strdup(name)));
}

void
purple_chat_conversation_unignore(PurpleChatConversation *chat, const char *name)
{
	GList *item;
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(name != NULL);

	/* Make sure the user is actually ignored. */
	if (!purple_chat_conversation_is_ignored_user(chat, name))
		return;

	item = g_list_find(purple_chat_conversation_get_ignored(chat),
					   purple_chat_conversation_get_ignored_user(chat, name));

	purple_chat_conversation_set_ignored(chat,
		g_list_remove_link(priv->ignored, item));

	g_free(item->data);
	g_list_free_1(item);
}

GList *
purple_chat_conversation_set_ignored(PurpleChatConversation *chat, GList *ignored)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	priv->ignored = ignored;
	return ignored;
}

GList *
purple_chat_conversation_get_ignored(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->ignored;
}

const char *
purple_chat_conversation_get_ignored_user(const PurpleChatConversation *chat, const char *user)
{
	GList *ignored;

	g_return_val_if_fail(PURPLE_IS_CHAT_CONVERSATION(chat), NULL);
	g_return_val_if_fail(user != NULL, NULL);

	for (ignored = purple_chat_conversation_get_ignored(chat);
		 ignored != NULL;
		 ignored = ignored->next) {

		const char *ign = (const char *)ignored->data;

		if (!purple_utf8_strcasecmp(user, ign) ||
			((*ign == '+' || *ign == '%') && !purple_utf8_strcasecmp(user, ign + 1)))
			return ign;

		if (*ign == '@') {
			ign++;

			if ((*ign == '+' && !purple_utf8_strcasecmp(user, ign + 1)) ||
				(*ign != '+' && !purple_utf8_strcasecmp(user, ign)))
				return ign;
		}
	}

	return NULL;
}

gboolean
purple_chat_conversation_is_ignored_user(const PurpleChatConversation *chat, const char *user)
{
	g_return_val_if_fail(PURPLE_IS_CHAT_CONVERSATION(chat), FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (purple_chat_conversation_get_ignored_user(chat, user) != NULL);
}

void
purple_chat_conversation_set_topic(PurpleChatConversation *chat, const char *who, const char *topic)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);
	GObject *obj;

	g_return_if_fail(priv != NULL);

	g_free(priv->who);
	g_free(priv->topic);

	priv->who   = g_strdup(who);
	priv->topic = g_strdup(topic);

	obj = G_OBJECT(chat);
	g_object_freeze_notify(obj);
	g_object_notify_by_pspec(obj, chat_properties[CHAT_PROP_TOPIC_WHO]);
	g_object_notify_by_pspec(obj, chat_properties[CHAT_PROP_TOPIC]);
	g_object_thaw_notify(obj);

	purple_conversation_update(PURPLE_CONVERSATION(chat),
							 PURPLE_CONVERSATION_UPDATE_TOPIC);

	purple_signal_emit(purple_conversations_get_handle(), "chat-topic-changed",
					 chat, priv->who, priv->topic);
}

const char *
purple_chat_conversation_get_topic(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->topic;
}

const char *
purple_chat_conversation_get_topic_who(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->who;
}

void
purple_chat_conversation_set_id(PurpleChatConversation *chat, int id)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	priv->id = id;

	g_object_notify_by_pspec(G_OBJECT(chat), chat_properties[CHAT_PROP_ID]);
}

int
purple_chat_conversation_get_id(const PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->id;
}

static void
chat_conversation_write_message(PurpleConversation *conv, PurpleMessage *msg)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv);
	PurpleMessageFlags flags;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(msg != NULL);

	/* Don't display this if the person who wrote it is ignored. */
	if (purple_message_get_author(msg) && purple_chat_conversation_is_ignored_user(
		PURPLE_CHAT_CONVERSATION(conv), purple_message_get_author(msg)))
	{
		return;
	}

#if 0
	PurpleAccount *account = purple_conversation_get_account(conv);
	/* XXX: this should not be necessary */
	if (purple_strequal(purple_normalize(account, who), priv->nick)) {
		flags |= PURPLE_MESSAGE_SEND;
	}
#endif

	flags = purple_message_get_flags(msg);
	if (flags & PURPLE_MESSAGE_RECV) {
		if (purple_utf8_has_word(purple_message_get_contents(msg), priv->nick)) {
			flags |= PURPLE_MESSAGE_NICK;
			purple_message_set_flags(msg, flags);
		}
	}

	_purple_conversation_write_common(conv, msg);
}

void
purple_chat_conversation_add_user(PurpleChatConversation *chat, const char *user,
						const char *extra_msg, PurpleChatUserFlags flags,
						gboolean new_arrival)
{
	GList *users = g_list_append(NULL, (char *)user);
	GList *extra_msgs = g_list_append(NULL, (char *)extra_msg);
	GList *flags2 = g_list_append(NULL, GINT_TO_POINTER(flags));

	purple_chat_conversation_add_users(chat, users, extra_msgs, flags2, new_arrival);

	g_list_free(users);
	g_list_free(extra_msgs);
	g_list_free(flags2);
}

void
purple_chat_conversation_add_users(PurpleChatConversation *chat, GList *users, GList *extra_msgs,
						 GList *flags, gboolean new_arrivals)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	PurpleChatUser *chatuser;
	PurpleChatConversationPrivate *priv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	GList *ul, *fl;
	GList *cbuddies = NULL;

	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv  != NULL);
	g_return_if_fail(users != NULL);

	conv = PURPLE_CONVERSATION(chat);
	ops  = purple_conversation_get_ui_ops(conv);

	account = purple_conversation_get_account(conv);
	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	ul = users;
	fl = flags;
	while ((ul != NULL) && (fl != NULL)) {
		const char *user = (const char *)ul->data;
		const char *alias = user;
		gboolean quiet;
		PurpleChatUserFlags flag = GPOINTER_TO_INT(fl->data);
		const char *extra_msg = (extra_msgs ? extra_msgs->data : NULL);

		if(!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			if (purple_strequal(priv->nick, purple_normalize(account, user))) {
				const char *alias2 = purple_account_get_private_alias(account);
				if (alias2 != NULL)
					alias = alias2;
				else
				{
					const char *display_name = purple_connection_get_display_name(gc);
					if (display_name != NULL)
						alias = display_name;
				}
			} else {
				PurpleBuddy *buddy;
				if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), user)) != NULL)
					alias = purple_buddy_get_contact_alias(buddy);
			}
		}

		quiet = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_conversations_get_handle(),
						 "chat-user-joining", chat, user, flag)) ||
				purple_chat_conversation_is_ignored_user(chat, user);

		chatuser = purple_chat_user_new(chat, user, alias, flag);

		g_hash_table_replace(priv->users,
			g_strdup(purple_chat_user_get_name(chatuser)),
			chatuser);

		cbuddies = g_list_prepend(cbuddies, chatuser);

		if (!quiet && new_arrivals) {
			char *alias_esc = g_markup_escape_text(alias, -1);
			char *tmp;

			if (extra_msg == NULL)
				tmp = g_strdup_printf(_("%s entered the room."), alias_esc);
			else {
				char *extra_msg_esc = g_markup_escape_text(extra_msg, -1);
				tmp = g_strdup_printf(_("%s [<I>%s</I>] entered the room."),
				                      alias_esc, extra_msg_esc);
				g_free(extra_msg_esc);
			}
			g_free(alias_esc);

			purple_conversation_write_system_message(
				conv, tmp, PURPLE_MESSAGE_NO_LINKIFY);
			g_free(tmp);
		}

		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-user-joined", chat, user, flag, new_arrivals);
		ul = ul->next;
		fl = fl->next;
		if (extra_msgs != NULL)
			extra_msgs = extra_msgs->next;
	}

	cbuddies = g_list_sort(cbuddies, (GCompareFunc)purple_chat_user_compare);

	if (ops != NULL && ops->chat_add_users != NULL)
		ops->chat_add_users(chat, cbuddies, new_arrivals);

	g_list_free(cbuddies);
}

void
purple_chat_conversation_rename_user(PurpleChatConversation *chat, const char *old_user,
						   const char *new_user)
{
	PurpleConversation *conv;
	PurpleConversationUiOps *ops;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	PurpleChatUser *cb;
	PurpleChatUserFlags flags;
	PurpleChatConversationPrivate *priv;
	const char *new_alias = new_user;
	char tmp[BUF_LONG];
	gboolean is_me = FALSE;

	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(old_user != NULL);
	g_return_if_fail(new_user != NULL);

	conv    = PURPLE_CONVERSATION(chat);
	ops     = purple_conversation_get_ui_ops(conv);
	account = purple_conversation_get_account(conv);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	if (purple_strequal(priv->nick, purple_normalize(account, old_user))) {
		const char *alias;

		/* Note this for later. */
		is_me = TRUE;

		if(!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
			alias = purple_account_get_private_alias(account);
			if (alias != NULL)
				new_alias = alias;
			else
			{
				const char *display_name = purple_connection_get_display_name(gc);
				if (display_name != NULL)
					new_alias = display_name;
			}
		}
	} else if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
		PurpleBuddy *buddy;
		if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), new_user)) != NULL)
			new_alias = purple_buddy_get_contact_alias(buddy);
	}

	flags = purple_chat_user_get_flags(purple_chat_conversation_find_user(chat, old_user));
	cb = purple_chat_user_new(chat, new_user, new_alias, flags);

	g_hash_table_replace(priv->users,
		g_strdup(purple_chat_user_get_name(cb)), cb);

	if (ops != NULL && ops->chat_rename_user != NULL)
		ops->chat_rename_user(chat, old_user, new_user, new_alias);

	cb = purple_chat_conversation_find_user(chat, old_user);

	if (cb)
		g_hash_table_remove(priv->users, purple_chat_user_get_name(cb));

	if (purple_chat_conversation_is_ignored_user(chat, old_user)) {
		purple_chat_conversation_unignore(chat, old_user);
		purple_chat_conversation_ignore(chat, new_user);
	}
	else if (purple_chat_conversation_is_ignored_user(chat, new_user))
		purple_chat_conversation_unignore(chat, new_user);

	if (is_me)
		purple_chat_conversation_set_nick(chat, new_user);

	if (purple_prefs_get_bool("/purple/conversations/chat/show_nick_change") &&
	    !purple_chat_conversation_is_ignored_user(chat, new_user)) {

		if (is_me) {
			char *escaped = g_markup_escape_text(new_user, -1);
			g_snprintf(tmp, sizeof(tmp),
					_("You are now known as %s"), escaped);
			g_free(escaped);
		} else {
			const char *old_alias = old_user;
			const char *new_alias = new_user;
			char *escaped;
			char *escaped2;

			if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
				PurpleBuddy *buddy;

				if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), old_user)) != NULL)
					old_alias = purple_buddy_get_contact_alias(buddy);
				if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), new_user)) != NULL)
					new_alias = purple_buddy_get_contact_alias(buddy);
			}

			escaped = g_markup_escape_text(old_alias, -1);
			escaped2 = g_markup_escape_text(new_alias, -1);
			g_snprintf(tmp, sizeof(tmp),
					_("%s is now known as %s"), escaped, escaped2);
			g_free(escaped);
			g_free(escaped2);
		}

		purple_conversation_write_system_message(conv,
			tmp, PURPLE_MESSAGE_NO_LINKIFY);
	}
}

void
purple_chat_conversation_remove_user(PurpleChatConversation *chat, const char *user, const char *reason)
{
	GList *users = g_list_append(NULL, (char *)user);

	purple_chat_conversation_remove_users(chat, users, reason);

	g_list_free(users);
}

void
purple_chat_conversation_remove_users(PurpleChatConversation *chat, GList *users, const char *reason)
{
	PurpleConversation *conv;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info;
	PurpleConversationUiOps *ops;
	PurpleChatUser *cb;
	PurpleChatConversationPrivate *priv;
	GList *l;
	gboolean quiet;

	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv  != NULL);
	g_return_if_fail(users != NULL);

	conv = PURPLE_CONVERSATION(chat);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
	g_return_if_fail(prpl_info != NULL);

	ops  = purple_conversation_get_ui_ops(conv);

	for (l = users; l != NULL; l = l->next) {
		const char *user = (const char *)l->data;
		quiet = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_conversations_get_handle(),
					"chat-user-leaving", chat, user, reason)) |
				purple_chat_conversation_is_ignored_user(chat, user);

		cb = purple_chat_conversation_find_user(chat, user);

		if (cb) {
			g_hash_table_remove(priv->users,
				purple_chat_user_get_name(cb));
		}

		/* NOTE: Don't remove them from ignored in case they re-enter. */

		if (!quiet) {
			const char *alias = user;
			char *alias_esc;
			char *tmp;

			if (!(prpl_info->options & OPT_PROTO_UNIQUE_CHATNAME)) {
				PurpleBuddy *buddy;

				if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), user)) != NULL)
					alias = purple_buddy_get_contact_alias(buddy);
			}

			alias_esc = g_markup_escape_text(alias, -1);

			if (reason == NULL || !*reason)
				tmp = g_strdup_printf(_("%s left the room."), alias_esc);
			else {
				char *reason_esc = g_markup_escape_text(reason, -1);
				tmp = g_strdup_printf(_("%s left the room (%s)."),
				                      alias_esc, reason_esc);
				g_free(reason_esc);
			}
			g_free(alias_esc);

			purple_conversation_write_system_message(conv,
				tmp, PURPLE_MESSAGE_NO_LINKIFY);
			g_free(tmp);
		}

		purple_signal_emit(purple_conversations_get_handle(), "chat-user-left",
						 conv, user, reason);
	}

	if (ops != NULL && ops->chat_remove_users != NULL)
		ops->chat_remove_users(chat, users);
}

void
purple_chat_conversation_clear_users(PurpleChatConversation *chat)
{
	PurpleConversationUiOps *ops;
	GHashTableIter it;
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);
	gchar *name;

	g_return_if_fail(priv != NULL);

	ops = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(chat));

	if (ops != NULL && ops->chat_remove_users != NULL) {
		GList *names = NULL;

		g_hash_table_iter_init(&it, priv->users);
		while (g_hash_table_iter_next(&it, (gpointer*)&name, NULL))
			names = g_list_prepend(names, name);

		ops->chat_remove_users(chat, names);
		g_list_free(names);
	}

	g_hash_table_iter_init(&it, priv->users);
	while (g_hash_table_iter_next(&it, (gpointer*)&name, NULL)) {
		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-user-leaving", chat, name, NULL);
		purple_signal_emit(purple_conversations_get_handle(),
						 "chat-user-left", chat, name, NULL);
	}

	g_hash_table_remove_all(priv->users);
}

void purple_chat_conversation_set_nick(PurpleChatConversation *chat, const char *nick) {
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	g_free(priv->nick);
	priv->nick = g_strdup(purple_normalize(
			purple_conversation_get_account(PURPLE_CONVERSATION(chat)), nick));

	g_object_notify_by_pspec(G_OBJECT(chat), chat_properties[CHAT_PROP_NICK]);
}

const char *purple_chat_conversation_get_nick(PurpleChatConversation *chat) {
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->nick;
}

static void
invite_user_to_chat(gpointer data, PurpleRequestFields *fields)
{
	PurpleConversation *conv;
	PurpleChatConversationPrivate *priv;
	const char *user, *message;

	conv = data;
	priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(conv);
	user = purple_request_fields_get_string(fields, "screenname");
	message = purple_request_fields_get_string(fields, "message");

	purple_serv_chat_invite(purple_conversation_get_connection(conv), priv->id, message, user);
}

void purple_chat_conversation_invite_user(PurpleChatConversation *chat, const char *user,
		const char *message, gboolean confirm)
{
	PurpleAccount *account;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	g_return_if_fail(PURPLE_IS_CHAT_CONVERSATION(chat));

	if (!user || !*user || !message || !*message)
		confirm = TRUE;

	account = purple_conversation_get_account(PURPLE_CONVERSATION(chat));

	if (!confirm) {
		purple_serv_chat_invite(purple_account_get_connection(account),
				purple_chat_conversation_get_id(chat), message, user);
		return;
	}

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(_("Invite to chat"));
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("Buddy"), user, FALSE);
	purple_request_field_group_add_field(group, field);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_set_type_hint(field, "screenname");

	field = purple_request_field_string_new("message", _("Message"), message, FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(chat, _("Invite to chat"), NULL,
			_("Please enter the name of the user you wish to invite, "
				"along with an optional invite message."),
			fields,
			_("Invite"), G_CALLBACK(invite_user_to_chat),
			_("Cancel"), NULL,
			purple_request_cpar_from_conversation(PURPLE_CONVERSATION(chat)),
			chat);
}

gboolean
purple_chat_conversation_has_user(PurpleChatConversation *chat, const char *user)
{
	g_return_val_if_fail(PURPLE_IS_CHAT_CONVERSATION(chat), FALSE);
	g_return_val_if_fail(user != NULL, FALSE);

	return (purple_chat_conversation_find_user(chat, user) != NULL);
}

void
purple_chat_conversation_leave(PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	priv->left = TRUE;

	if (!g_object_get_data(G_OBJECT(chat), "is-finalizing"))
		g_object_notify_by_pspec(G_OBJECT(chat), chat_properties[CHAT_PROP_LEFT]);

	purple_conversation_update(PURPLE_CONVERSATION(chat), PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

gboolean
purple_chat_conversation_has_left(PurpleChatConversation *chat)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, TRUE);

	return priv->left;
}

static void
chat_conversation_cleanup_for_rejoin(PurpleChatConversation *chat)
{
	const char *disp;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	account = purple_conversation_get_account(conv);

	purple_conversation_close_logs(conv);
	purple_conversation_set_logging(conv, TRUE);

	gc = purple_account_get_connection(account);

	if ((disp = purple_connection_get_display_name(gc)) != NULL)
		purple_chat_conversation_set_nick(chat, disp);
	else
	{
		purple_chat_conversation_set_nick(chat,
								purple_account_get_username(account));
	}

	purple_chat_conversation_clear_users(chat);
	purple_chat_conversation_set_topic(chat, NULL, NULL);
	priv->left = FALSE;

	g_object_notify_by_pspec(G_OBJECT(chat), chat_properties[CHAT_PROP_LEFT]);

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_CHATLEFT);
}

PurpleChatUser *
purple_chat_conversation_find_user(PurpleChatConversation *chat, const char *name)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return g_hash_table_lookup(priv->users, name);
}

/**************************************************************************
 * GObject code for chats
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_chat_conversation_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(obj);

	switch (param_id) {
		case CHAT_PROP_ID:
			purple_chat_conversation_set_id(chat, g_value_get_int(value));
			break;
		case CHAT_PROP_NICK:
			purple_chat_conversation_set_nick(chat, g_value_get_string(value));
			break;
		case CHAT_PROP_LEFT:
			{
				gboolean left = g_value_get_boolean(value);
				if (left)
					purple_chat_conversation_leave(chat);
			}
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_chat_conversation_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(obj);

	switch (param_id) {
		case CHAT_PROP_TOPIC_WHO:
			g_value_set_string(value, purple_chat_conversation_get_topic_who(chat));
			break;
		case CHAT_PROP_TOPIC:
			g_value_set_string(value, purple_chat_conversation_get_topic(chat));
			break;
		case CHAT_PROP_ID:
			g_value_set_int(value, purple_chat_conversation_get_id(chat));
			break;
		case CHAT_PROP_NICK:
			g_value_set_string(value, purple_chat_conversation_get_nick(chat));
			break;
		case CHAT_PROP_LEFT:
			g_value_set_boolean(value, purple_chat_conversation_has_left(chat));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void purple_chat_conversation_init(GTypeInstance *instance, gpointer klass)
{
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(instance);

	PURPLE_DBUS_REGISTER_POINTER(PURPLE_CHAT_CONVERSATION(instance),
			PurpleChatConversation);

	priv->users = g_hash_table_new_full(_purple_conversation_user_hash,
		_purple_conversation_user_equal, g_free, g_object_unref);
}

/* Called when done constructing */
static void
purple_chat_conversation_constructed(GObject *object)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(object);
	PurpleAccount *account;
	const char *disp;

	G_OBJECT_CLASS(parent_class)->constructed(object);

	g_object_get(object, "account", &account, NULL);

	if ((disp = purple_connection_get_display_name(purple_account_get_connection(account))))
		purple_chat_conversation_set_nick(chat, disp);
	else
		purple_chat_conversation_set_nick(chat,
								purple_account_get_username(account));

	if (purple_prefs_get_bool("/purple/logging/log_chats"))
		purple_conversation_set_logging(PURPLE_CONVERSATION(chat), TRUE);

	g_object_unref(account);
}

/* GObject dispose function */
static void
purple_chat_conversation_dispose(GObject *object)
{
	PurpleChatConversationPrivate *priv =
			PURPLE_CHAT_CONVERSATION_GET_PRIVATE(object);

	g_hash_table_remove_all(priv->users);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_chat_conversation_finalize(GObject *object)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(object);
	PurpleConnection *gc = purple_conversation_get_connection(PURPLE_CONVERSATION(chat));
	PurpleChatConversationPrivate *priv = PURPLE_CHAT_CONVERSATION_GET_PRIVATE(chat);

	if (gc != NULL)
	{
		/* Still connected */
		int chat_id = purple_chat_conversation_get_id(chat);
#if 0
		/*
		 * This is unfortunately necessary, because calling
		 * purple_serv_chat_leave() calls this purple_conversation_destroy(),
		 * which leads to two calls here.. We can't just return after
		 * this, because then it'll return on the next pass. So, since
		 * purple_serv_got_chat_left(), which is eventually called from the
		 * prpl that purple_serv_chat_leave() calls, removes this conversation
		 * from the gc's buddy_chats list, we're going to check to see
		 * if this exists in the list. If so, we want to return after
		 * calling this, because it'll be called again. If not, fall
		 * through, because it'll have already been removed, and we'd
		 * be on the 2nd pass.
		 *
		 * Long paragraph. <-- Short sentence.
		 *
		 *   -- ChipX86
		 */

		if (gc && g_slist_find(gc->buddy_chats, conv) != NULL) {
			purple_serv_chat_leave(gc, chat_id);

			return;
		}
#endif
		/*
		 * Instead of all of that, lets just close the window when
		 * the user tells us to, and let the prpl deal with the
		 * internals on it's own time. Don't do this if the prpl already
		 * knows it left the chat.
		 */
		if (!purple_chat_conversation_has_left(chat))
			purple_serv_chat_leave(gc, chat_id);

		/*
		 * If they didn't call purple_serv_got_chat_left by now, it's too late.
		 * So we better do it for them before we destroy the thing.
		 */
		if (!purple_chat_conversation_has_left(chat))
			purple_serv_got_chat_left(gc, chat_id);
	}

	g_hash_table_destroy(priv->users);
	priv->users = NULL;

	g_list_foreach(priv->ignored, (GFunc)g_free, NULL);
	g_list_free(priv->ignored);
	priv->ignored = NULL;

	g_free(priv->who);
	g_free(priv->topic);
	g_free(priv->nick);

	priv->who = NULL;
	priv->topic = NULL;
	priv->nick = NULL;

	PURPLE_DBUS_UNREGISTER_POINTER(chat);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_chat_conversation_class_init(PurpleChatConversationClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleConversationClass *conv_class = PURPLE_CONVERSATION_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_chat_conversation_dispose;
	obj_class->finalize = purple_chat_conversation_finalize;
	obj_class->constructed = purple_chat_conversation_constructed;

	/* Setup properties */
	obj_class->get_property = purple_chat_conversation_get_property;
	obj_class->set_property = purple_chat_conversation_set_property;

	conv_class->write_message = chat_conversation_write_message;

	g_type_class_add_private(klass, sizeof(PurpleChatConversationPrivate));

	chat_properties[CHAT_PROP_TOPIC_WHO] = g_param_spec_string("topic-who",
				"Who set topic",
				"Who set the chat topic.", NULL,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	chat_properties[CHAT_PROP_TOPIC] = g_param_spec_string("topic",
				"Topic",
				"Topic of the chat.", NULL,
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	chat_properties[CHAT_PROP_ID] = g_param_spec_int("chat-id",
				"Chat ID",
				"The ID of the chat.", G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	chat_properties[CHAT_PROP_NICK] = g_param_spec_string("nick",
				"Nickname",
				"The nickname of the user in a chat.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	chat_properties[CHAT_PROP_LEFT] = g_param_spec_boolean("left",
				"Left the chat",
				"Whether the user has left the chat.", FALSE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, CHAT_PROP_LAST,
				chat_properties);
}

GType
purple_chat_conversation_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleChatConversationClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_chat_conversation_class_init,
			NULL,
			NULL,
			sizeof(PurpleChatConversation),
			0,
			(GInstanceInitFunc)purple_chat_conversation_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_CONVERSATION,
				"PurpleChatConversation",
				&info, 0);
	}

	return type;
}

PurpleChatConversation *
purple_chat_conversation_new(PurpleAccount *account, const char *name)
{
	PurpleChatConversation *chat;
	PurpleConnection *gc;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Check if this conversation already exists. */
	if ((chat = purple_conversations_find_chat_with_account(name, account)) != NULL)
	{
		if (!purple_chat_conversation_has_left(chat)) {
			purple_debug_warning("conversationtypes", "Trying to create "
					"multiple chats (%s) with the same name is deprecated and "
					"will be removed in libpurple 3.0.0", name);
		} else {
			/*
			 * This hack is necessary because some prpls (MSN) have unnamed chats
			 * that all use the same name.  A PurpleConversation for one of those
			 * is only ever re-used if the user has left, so calls to
			 * purple_conversation_new need to fall-through to creating a new
			 * chat.
			 * TODO 3.0.0: Remove this workaround and mandate unique names.
			 */

			chat_conversation_cleanup_for_rejoin(chat);
			return chat;
		}
	}

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	chat = g_object_new(PURPLE_TYPE_CHAT_CONVERSATION,
			"account", account,
			"name",    name,
			"title",   name,
			NULL);

	return chat;
}

/**************************************************************************
 * Chat Conversation User API
 **************************************************************************/
static int
purple_chat_user_compare(PurpleChatUser *a, PurpleChatUser *b)
{
	PurpleChatUserFlags f1 = 0, f2 = 0;
	PurpleChatUserPrivate *priva, *privb;
	char *user1 = NULL, *user2 = NULL;
	gint ret = 0;

	priva = PURPLE_CHAT_USER_GET_PRIVATE(a);
	privb = PURPLE_CHAT_USER_GET_PRIVATE(b);

	if (priva) {
		f1 = priva->flags;
		if (priva->alias_key)
			user1 = priva->alias_key;
		else if (priva->name)
			user1 = priva->name;
	}

	if (privb) {
		f2 = privb->flags;
		if (privb->alias_key)
			user2 = privb->alias_key;
		else if (privb->name)
			user2 = privb->name;
	}

	if (user1 == NULL || user2 == NULL) {
		if (!(user1 == NULL && user2 == NULL))
			ret = (user1 == NULL) ? -1: 1;
	} else if (f1 != f2) {
		/* sort more important users first */
		ret = (f1 > f2) ? -1 : 1;
	} else if (priva->buddy != privb->buddy) {
		ret = priva->buddy ? -1 : 1;
	} else {
		ret = purple_utf8_strcasecmp(user1, user2);
	}

	return ret;
}

const char *
purple_chat_user_get_alias(const PurpleChatUser *cb)
{
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->alias;
}

const char *
purple_chat_user_get_name(const PurpleChatUser *cb)
{
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->name;
}

void
purple_chat_user_set_flags(PurpleChatUser *cb,
							  PurpleChatUserFlags flags)
{
	PurpleConversationUiOps *ops;
	PurpleChatUserFlags oldflags;
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);

	if (flags == priv->flags)
		return;

	oldflags = priv->flags;
	priv->flags = flags;

	g_object_notify_by_pspec(G_OBJECT(cb), cu_properties[CU_PROP_FLAGS]);

	ops = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(priv->chat));

	if (ops != NULL && ops->chat_update_user != NULL)
		ops->chat_update_user(cb);

	purple_signal_emit(purple_conversations_get_handle(),
					 "chat-user-flags", cb, oldflags, flags);
}

PurpleChatUserFlags
purple_chat_user_get_flags(const PurpleChatUser *cb)
{
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, PURPLE_CHAT_USER_NONE);

	return priv->flags;
}

void
purple_chat_user_set_ui_data(PurpleChatUser *cb, gpointer ui_data)
{
	g_return_if_fail(PURPLE_IS_CHAT_USER(cb));

	cb->ui_data = ui_data;
}

gpointer
purple_chat_user_get_ui_data(const PurpleChatUser *cb)
{
	g_return_val_if_fail(PURPLE_IS_CHAT_USER(cb), NULL);

	return cb->ui_data;
}

void
purple_chat_user_set_chat(PurpleChatUser *cb,
		PurpleChatConversation *chat)
{
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_if_fail(priv != NULL);

	priv->chat = chat;

	g_object_notify_by_pspec(G_OBJECT(cb), cu_properties[CU_PROP_CHAT]);
}

PurpleChatConversation *
purple_chat_user_get_chat(const PurpleChatUser *cb)
{
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->chat;
}

gboolean
purple_chat_user_is_buddy(const PurpleChatUser *cb)
{
	PurpleChatUserPrivate *priv;
	priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->buddy;
}

/**************************************************************************
 * GObject code for chat user
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_chat_user_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleChatUser *cb = PURPLE_CHAT_USER(obj);
	PurpleChatUserPrivate *priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	switch (param_id) {
		case CU_PROP_CHAT:
			priv->chat = g_value_get_object(value);
			break;
		case CU_PROP_NAME:
			g_free(priv->name);
			priv->name = g_strdup(g_value_get_string(value));
			break;
		case CU_PROP_ALIAS:
			g_free(priv->alias);
			priv->alias = g_strdup(g_value_get_string(value));
			break;
		case CU_PROP_FLAGS:
			priv->flags = g_value_get_flags(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_chat_user_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleChatUser *cb = PURPLE_CHAT_USER(obj);

	switch (param_id) {
		case CU_PROP_CHAT:
			g_value_set_object(value, purple_chat_user_get_chat(cb));
			break;
		case CU_PROP_NAME:
			g_value_set_string(value, purple_chat_user_get_name(cb));
			break;
		case CU_PROP_ALIAS:
			g_value_set_string(value, purple_chat_user_get_alias(cb));
			break;
		case CU_PROP_FLAGS:
			g_value_set_flags(value, purple_chat_user_get_flags(cb));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_chat_user_init(GTypeInstance *instance, gpointer klass)
{
	PURPLE_DBUS_REGISTER_POINTER(PURPLE_CHAT_USER(instance), PurpleChatUser);
}

/* Called when done constructing */
static void
purple_chat_user_constructed(GObject *object)
{
	PurpleChatUserPrivate *priv = PURPLE_CHAT_USER_GET_PRIVATE(object);
	PurpleAccount *account;

	cb_parent_class->constructed(object);

	account = purple_conversation_get_account(PURPLE_CONVERSATION(priv->chat));

	if (purple_blist_find_buddy(account, priv->name) != NULL)
		priv->buddy = TRUE;
}

/* GObject finalize function */
static void
purple_chat_user_finalize(GObject *object)
{
	PurpleChatUser *cb = PURPLE_CHAT_USER(object);
	PurpleChatUserPrivate *priv = PURPLE_CHAT_USER_GET_PRIVATE(cb);

	purple_signal_emit(purple_conversations_get_handle(),
			"deleting-chat-user", cb);

	g_free(priv->alias);
	g_free(priv->alias_key);
	g_free(priv->name);

	PURPLE_DBUS_UNREGISTER_POINTER(cb);

	cb_parent_class->finalize(object);
}

/* Class initializer function */
static void purple_chat_user_class_init(PurpleChatUserClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	cb_parent_class = g_type_class_peek_parent(klass);

	obj_class->constructed = purple_chat_user_constructed;
	obj_class->finalize = purple_chat_user_finalize;

	/* Setup properties */
	obj_class->get_property = purple_chat_user_get_property;
	obj_class->set_property = purple_chat_user_set_property;

	g_type_class_add_private(klass, sizeof(PurpleChatUserPrivate));

	cu_properties[CU_PROP_CHAT] = g_param_spec_object("chat", "Chat",
				"The chat the buddy belongs to.", PURPLE_TYPE_CHAT_CONVERSATION,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	cu_properties[CU_PROP_NAME] = g_param_spec_string("name", "Name",
				"Name of the chat user.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	cu_properties[CU_PROP_ALIAS] = g_param_spec_string("alias", "Alias",
				"Alias of the chat user.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	cu_properties[CU_PROP_FLAGS] = g_param_spec_flags("flags", "Buddy flags",
				"The flags for the chat user.",
				PURPLE_TYPE_CHAT_USER_FLAGS, PURPLE_CHAT_USER_NONE,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, CU_PROP_LAST, cu_properties);
}

GType
purple_chat_user_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleChatUserClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_chat_user_class_init,
			NULL,
			NULL,
			sizeof(PurpleChatUser),
			0,
			(GInstanceInitFunc)purple_chat_user_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleChatUser",
				&info, 0);
	}

	return type;
}

PurpleChatUser *
purple_chat_user_new(PurpleChatConversation *chat, const char *name,
		const char *alias, PurpleChatUserFlags flags)
{
	PurpleChatUser *cb;

	g_return_val_if_fail(PURPLE_IS_CHAT_CONVERSATION(chat), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	cb = g_object_new(PURPLE_TYPE_CHAT_USER,
			"chat",  chat,
			"name",  name,
			"alias", alias,
			"flags", flags,
			NULL);

	return cb;
}
