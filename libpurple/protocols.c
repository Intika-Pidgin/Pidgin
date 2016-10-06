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
 *
 */
#include "internal.h"
#include "accountopt.h"
#include "conversation.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "protocol.h"
#include "request.h"
#include "util.h"

static GHashTable *protocols = NULL;

/**************************************************************************/
/* Attention Type API                                                     */
/**************************************************************************/

struct _PurpleAttentionType
{
	const char *name;                  /* Shown in GUI elements */
	const char *incoming_description;  /* Shown when sent */
	const char *outgoing_description;  /* Shown when receied */
	const char *icon_name;             /* Icon to display (optional) */
	const char *unlocalized_name;      /* Unlocalized name for UIs needing it */
};


PurpleAttentionType *
purple_attention_type_new(const char *ulname, const char *name,
						const char *inc_desc, const char *out_desc)
{
	PurpleAttentionType *attn = g_new0(PurpleAttentionType, 1);

	purple_attention_type_set_name(attn, name);
	purple_attention_type_set_incoming_desc(attn, inc_desc);
	purple_attention_type_set_outgoing_desc(attn, out_desc);
	purple_attention_type_set_unlocalized_name(attn, ulname);

	return attn;
}


void
purple_attention_type_set_name(PurpleAttentionType *type, const char *name)
{
	g_return_if_fail(type != NULL);

	type->name = name;
}

void
purple_attention_type_set_incoming_desc(PurpleAttentionType *type, const char *desc)
{
	g_return_if_fail(type != NULL);

	type->incoming_description = desc;
}

void
purple_attention_type_set_outgoing_desc(PurpleAttentionType *type, const char *desc)
{
	g_return_if_fail(type != NULL);

	type->outgoing_description = desc;
}

void
purple_attention_type_set_icon_name(PurpleAttentionType *type, const char *name)
{
	g_return_if_fail(type != NULL);

	type->icon_name = name;
}

void
purple_attention_type_set_unlocalized_name(PurpleAttentionType *type, const char *ulname)
{
	g_return_if_fail(type != NULL);

	type->unlocalized_name = ulname;
}

const char *
purple_attention_type_get_name(const PurpleAttentionType *type)
{
	g_return_val_if_fail(type != NULL, NULL);

	return type->name;
}

const char *
purple_attention_type_get_incoming_desc(const PurpleAttentionType *type)
{
	g_return_val_if_fail(type != NULL, NULL);

	return type->incoming_description;
}

const char *
purple_attention_type_get_outgoing_desc(const PurpleAttentionType *type)
{
	g_return_val_if_fail(type != NULL, NULL);

	return type->outgoing_description;
}

const char *
purple_attention_type_get_icon_name(const PurpleAttentionType *type)
{
	g_return_val_if_fail(type != NULL, NULL);

	if(type->icon_name == NULL || *(type->icon_name) == '\0')
		return NULL;

	return type->icon_name;
}

const char *
purple_attention_type_get_unlocalized_name(const PurpleAttentionType *type)
{
	g_return_val_if_fail(type != NULL, NULL);

	return type->unlocalized_name;
}

/**************************************************************************
 * GBoxed code for PurpleAttentionType
 **************************************************************************/

static PurpleAttentionType *
purple_attention_type_copy(PurpleAttentionType *attn)
{
	PurpleAttentionType *attn_copy;

	g_return_val_if_fail(attn != NULL, NULL);

	attn_copy  = g_new(PurpleAttentionType, 1);
	*attn_copy = *attn;

	return attn_copy;
}

GType
purple_attention_type_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleAttentionType",
				(GBoxedCopyFunc)purple_attention_type_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

/**************************************************************************
 * GBoxed code for PurpleProtocolChatEntry
 **************************************************************************/

static PurpleProtocolChatEntry *
purple_protocol_chat_entry_copy(PurpleProtocolChatEntry *pce)
{
	PurpleProtocolChatEntry *pce_copy;

	g_return_val_if_fail(pce != NULL, NULL);

	pce_copy  = g_new(PurpleProtocolChatEntry, 1);
	*pce_copy = *pce;

	return pce_copy;
}

GType
purple_protocol_chat_entry_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleProtocolChatEntry",
				(GBoxedCopyFunc)purple_protocol_chat_entry_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

/**************************************************************************/
/* Protocol API                                                           */
/**************************************************************************/
void
purple_protocol_got_account_idle(PurpleAccount *account, gboolean idle,
						   time_t idle_time)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	purple_presence_set_idle(purple_account_get_presence(account),
						   idle, idle_time);
}

void
purple_protocol_got_account_login_time(PurpleAccount *account, time_t login_time)
{
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	if (login_time == 0)
		login_time = time(NULL);

	presence = purple_account_get_presence(account);

	purple_presence_set_login_time(presence, login_time);
}

void
purple_protocol_got_account_status(PurpleAccount *account, const char *status_id, ...)
{
	PurplePresence *presence;
	PurpleStatus *status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	presence = purple_account_get_presence(account);
	status   = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	va_start(args, status_id);
	purple_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);
}

void
purple_protocol_got_account_actions(PurpleAccount *account)
{

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	purple_signal_emit(purple_accounts_get_handle(), "account-actions-changed",
	                   account);
}

void
purple_protocol_got_user_idle(PurpleAccount *account, const char *name,
		gboolean idle, time_t idle_time)
{
	PurplePresence *presence;
	GSList *list;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if ((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	while (list) {
		presence = purple_buddy_get_presence(list->data);
		list = g_slist_delete_link(list, list);
		purple_presence_set_idle(presence, idle, idle_time);
	}
}

void
purple_protocol_got_user_login_time(PurpleAccount *account, const char *name,
		time_t login_time)
{
	GSList *list;
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	if (login_time == 0)
		login_time = time(NULL);

	while (list) {
		PurpleBuddy *buddy = list->data;
		presence = purple_buddy_get_presence(buddy);
		list = g_slist_delete_link(list, list);

		if (purple_presence_get_login_time(presence) != login_time)
		{
			purple_presence_set_login_time(presence, login_time);

			purple_signal_emit(purple_blist_get_handle(), "buddy-got-login-time", buddy);
		}
	}
}

void
purple_protocol_got_user_status(PurpleAccount *account, const char *name,
		const char *status_id, ...)
{
	GSList *list, *l;
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;
	PurpleStatus *old_status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	for(l = list; l != NULL; l = l->next) {
		buddy = l->data;

		presence = purple_buddy_get_presence(buddy);
		status   = purple_presence_get_status(presence, status_id);

		if(NULL == status)
			/*
			 * TODO: This should never happen, right?  We should call
			 *       g_warning() or something.
			 */
			continue;

		old_status = purple_presence_get_active_status(presence);

		va_start(args, status_id);
		purple_status_set_active_with_attrs(status, TRUE, args);
		va_end(args);

		purple_buddy_update_status(buddy, old_status);
	}

	g_slist_free(list);

	/* The buddy is no longer online, they are therefore by definition not
	 * still typing to us. */
	if (!purple_status_is_online(status)) {
		purple_serv_got_typing_stopped(purple_account_get_connection(account), name);
		purple_protocol_got_media_caps(account, name);
	}
}

void purple_protocol_got_user_status_deactive(PurpleAccount *account, const char *name,
					const char *status_id)
{
	GSList *list, *l;
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	for(l = list; l != NULL; l = l->next) {
		buddy = l->data;

		presence = purple_buddy_get_presence(buddy);
		status   = purple_presence_get_status(presence, status_id);

		if(NULL == status)
			continue;

		if (purple_status_is_active(status)) {
			purple_status_set_active(status, FALSE);
			purple_buddy_update_status(buddy, status);
		}
	}

	g_slist_free(list);
}

static void
do_protocol_change_account_status(PurpleAccount *account,
								PurpleStatus *old_status, PurpleStatus *new_status)
{
	PurpleProtocol *protocol;

	if (purple_status_is_online(new_status) &&
		purple_account_is_disconnected(account) &&
		purple_network_is_available())
	{
		purple_account_connect(account);
		return;
	}

	if (!purple_status_is_online(new_status))
	{
		if (!purple_account_is_disconnected(account))
			purple_account_disconnect(account);
		/* Clear out the unsaved password if we switch to offline status */
		if (!purple_account_get_remember_password(account))
			purple_account_set_password(account, NULL, NULL, NULL);

		return;
	}

	if (purple_account_is_connecting(account))
		/*
		 * We don't need to call the set_status protocol function because
		 * the protocol will take care of setting its status during the
		 * connection process.
		 */
		return;

	protocol = purple_protocols_find(purple_account_get_protocol_id(account));

	if (protocol == NULL)
		return;

	if (!purple_account_is_disconnected(account))
		purple_protocol_server_iface_set_status(protocol, account, new_status);
}

void
purple_protocol_change_account_status(PurpleAccount *account,
								PurpleStatus *old_status, PurpleStatus *new_status)
{
	g_return_if_fail(account    != NULL);
	g_return_if_fail(new_status != NULL);
	g_return_if_fail(!purple_status_is_exclusive(new_status) || old_status != NULL);

	purple_signal_emit(purple_accounts_get_handle(), "account-status-changing",
					account, old_status, new_status);

	do_protocol_change_account_status(account, old_status, new_status);

	purple_signal_emit(purple_accounts_get_handle(), "account-status-changed",
					account, old_status, new_status);
}

GList *
purple_protocol_get_statuses(PurpleAccount *account, PurplePresence *presence)
{
	GList *statuses = NULL;
	GList *l;
	PurpleStatus *status;

	g_return_val_if_fail(account  != NULL, NULL);
	g_return_val_if_fail(presence != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		status = purple_status_new((PurpleStatusType *)l->data, presence);
		statuses = g_list_prepend(statuses, status);
	}

	statuses = g_list_reverse(statuses);

	return statuses;
}

static void
purple_protocol_attention(PurpleConversation *conv, const char *who,
	guint type, PurpleMessageFlags flags, time_t mtime)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	purple_signal_emit(purple_conversations_get_handle(),
		flags == PURPLE_MESSAGE_SEND ? "sent-attention" : "got-attention",
		account, who, conv, type);
}

void
purple_protocol_send_attention(PurpleConnection *gc, const char *who, guint type_code)
{
	PurpleAttentionType *attn;
	PurpleProtocol *protocol;
	PurpleIMConversation *im;
	PurpleBuddy *buddy;
	const char *alias;
	gchar *description;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(purple_connection_get_account(gc)));
	g_return_if_fail(PURPLE_PROTOCOL_IMPLEMENTS(protocol, ATTENTION_IFACE, send));

	attn = purple_get_attention_type_from_code(purple_connection_get_account(gc), type_code);

	if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), who)) != NULL)
		alias = purple_buddy_get_contact_alias(buddy);
	else
		alias = who;

	if (attn && purple_attention_type_get_outgoing_desc(attn)) {
		description = g_strdup_printf(purple_attention_type_get_outgoing_desc(attn), alias);
	} else {
		description = g_strdup_printf(_("Requesting %s's attention..."), alias);
	}

	purple_debug_info("server", "serv_send_attention: sending '%s' to %s\n",
			description, who);

	if (!purple_protocol_attention_iface_send(protocol, gc, who, type_code))
		return;

	im = purple_im_conversation_new(purple_connection_get_account(gc), who);
	purple_conversation_write_system_message(PURPLE_CONVERSATION(im), description, 0);
	purple_protocol_attention(PURPLE_CONVERSATION(im), who, type_code, PURPLE_MESSAGE_SEND, time(NULL));

	g_free(description);
}

static void
got_attention(PurpleConnection *gc, int id, const char *who, guint type_code)
{
	PurpleMessageFlags flags;
	PurpleAttentionType *attn;
	PurpleBuddy *buddy;
	const char *alias;
	gchar *description;
	time_t mtime;

	mtime = time(NULL);

	attn = purple_get_attention_type_from_code(purple_connection_get_account(gc), type_code);

	/* PURPLE_MESSAGE_NOTIFY is for attention messages. */
	flags = PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NOTIFY | PURPLE_MESSAGE_RECV;

	/* TODO: if (attn->icon_name) is non-null, use it to lookup an emoticon and display
	 * it next to the attention command. And if it is null, display a generic icon. */

	if ((buddy = purple_blist_find_buddy(purple_connection_get_account(gc), who)) != NULL)
		alias = purple_buddy_get_contact_alias(buddy);
	else
		alias = who;

	if (attn && purple_attention_type_get_incoming_desc(attn)) {
		description = g_strdup_printf(purple_attention_type_get_incoming_desc(attn), alias);
	} else {
		description = g_strdup_printf(_("%s has requested your attention!"), alias);
	}

	purple_debug_info("server", "got_attention: got '%s' from %s\n",
			description, who);

	if (id == -1)
		purple_serv_got_im(gc, who, description, flags, mtime);
	else
		purple_serv_got_chat_in(gc, id, who, flags, description, mtime);

	/* TODO: sounds (depending on PurpleAttentionType), shaking, etc. */

	g_free(description);
}

void
purple_protocol_got_attention(PurpleConnection *gc, const char *who, guint type_code)
{
	PurpleConversation *conv = NULL;
	PurpleAccount *account = purple_connection_get_account(gc);

	got_attention(gc, -1, who, type_code);
	conv =
		purple_conversations_find_with_account(who, account);
	if (conv)
		purple_protocol_attention(conv, who, type_code, PURPLE_MESSAGE_RECV,
			time(NULL));
}

void
purple_protocol_got_attention_in_chat(PurpleConnection *gc, int id, const char *who, guint type_code)
{
	got_attention(gc, id, who, type_code);
}

gboolean
purple_protocol_initiate_media(PurpleAccount *account,
			   const char *who,
			   PurpleMediaSessionType type)
{
#ifdef USE_VV
	PurpleConnection *gc = NULL;
	PurpleProtocol *protocol = NULL;

	if (account)
		gc = purple_account_get_connection(account);
	if (gc)
		protocol = purple_connection_get_protocol(gc);

	if (protocol) {
		/* should check that the protocol supports this media type here? */
		return purple_protocol_media_iface_initiate_session(protocol, account, who, type);
	} else
#endif
	return FALSE;
}

PurpleMediaCaps
purple_protocol_get_media_caps(PurpleAccount *account, const char *who)
{
#ifdef USE_VV
	PurpleConnection *gc = NULL;
	PurpleProtocol *protocol = NULL;

	if (account)
		gc = purple_account_get_connection(account);
	if (gc)
		protocol = purple_connection_get_protocol(gc);

	if (protocol)
		return purple_protocol_media_iface_get_caps(protocol, account, who);
#endif
	return PURPLE_MEDIA_CAPS_NONE;
}

void
purple_protocol_got_media_caps(PurpleAccount *account, const char *name)
{
#ifdef USE_VV
	GSList *list;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	while (list) {
		PurpleBuddy *buddy = list->data;
		PurpleMediaCaps oldcaps = purple_buddy_get_media_caps(buddy);
		PurpleMediaCaps newcaps = 0;
		const gchar *bname = purple_buddy_get_name(buddy);
		list = g_slist_delete_link(list, list);


		newcaps = purple_protocol_get_media_caps(account, bname);
		purple_buddy_set_media_caps(buddy, newcaps);

		if (oldcaps == newcaps)
			continue;

		purple_signal_emit(purple_blist_get_handle(),
				"buddy-caps-changed", buddy,
				newcaps, oldcaps);
	}
#endif
}

gssize
purple_protocol_get_max_message_size(PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), 0);

	return purple_protocol_client_iface_get_max_message_size(protocol, NULL);
}

/**************************************************************************/
/* Protocol Action API                                                    */
/**************************************************************************/

PurpleProtocolAction *
purple_protocol_action_new(const char* label,
		PurpleProtocolActionCallback callback)
{
	PurpleProtocolAction *action;

	g_return_val_if_fail(label != NULL && callback != NULL, NULL);

	action = g_new0(PurpleProtocolAction, 1);

	action->label    = g_strdup(label);
	action->callback = callback;

	return action;
}

void
purple_protocol_action_free(PurpleProtocolAction *action)
{
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_free(action);
}

/**************************************************************************
 * GBoxed code for PurpleProtocolAction
 **************************************************************************/

static PurpleProtocolAction *
purple_protocol_action_copy(PurpleProtocolAction *action)
{
	g_return_val_if_fail(action != NULL, NULL);

	return purple_protocol_action_new(action->label, action->callback);
}

GType
purple_protocol_action_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleProtocolAction",
				(GBoxedCopyFunc)purple_protocol_action_copy,
				(GBoxedFreeFunc)purple_protocol_action_free);
	}

	return type;
}

/**************************************************************************
 * Protocols API
 **************************************************************************/
/*
 * Negative if a before b, 0 if equal, positive if a after b.
 */
static gint
compare_protocol(PurpleProtocol *a, PurpleProtocol *b)
{
	const gchar *aname = purple_protocol_get_name(a);
	const gchar *bname = purple_protocol_get_name(b);

	if (aname) {
		if (bname)
			return strcmp(aname, bname);
		else
			return -1;
	} else {
		if (bname)
			return 1;
		else
			return 0;
	}
}

PurpleProtocol *
purple_protocols_find(const char *id)
{
	g_return_val_if_fail(protocols != NULL && id != NULL, NULL);

	return g_hash_table_lookup(protocols, id);
}

PurpleProtocol *
purple_protocols_add(GType protocol_type, GError **error)
{
	PurpleProtocol *protocol;
	PurpleProtocolClass *klass;

	if (protocol_type == G_TYPE_INVALID) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Protocol type is not registered"));
		return NULL;
	}

	if (!g_type_is_a(protocol_type, PURPLE_TYPE_PROTOCOL)) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Protocol type does not inherit PurpleProtocol"));
		return NULL;
	}

	if (G_TYPE_IS_ABSTRACT(protocol_type)) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Protocol type is abstract"));
		return NULL;
	}

	protocol = g_object_new(protocol_type, NULL);
	klass = PURPLE_PROTOCOL_GET_CLASS(protocol);

	if (!protocol) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Could not create protocol instance"));
		return NULL;
	}

	if (!purple_protocol_get_id(protocol)) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Protocol does not provide an ID"));

		g_object_unref(protocol);
		return NULL;
	}

	if (purple_protocols_find(purple_protocol_get_id(protocol))) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("A protocol with the ID %s is already added."),
		            purple_protocol_get_id(protocol));

		g_object_unref(protocol);
		return NULL;
	}

	/* Make sure the protocol implements the required functions */
	if (!klass->login        || !klass->close    ||
	    !klass->status_types || !klass->list_icon )
	{
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Protocol %s does not implement all the functions in "
		            "PurpleProtocolClass"), purple_protocol_get_id(protocol));

		g_object_unref(protocol);
		return NULL;
	}

	g_hash_table_insert(protocols, g_strdup(purple_protocol_get_id(protocol)),
	                    protocol);

	purple_debug_info("protocols", "Added protocol %s\n",
	                  purple_protocol_get_id(protocol));

	purple_signal_emit(purple_protocols_get_handle(), "protocol-added",
	                   protocol);
	return protocol;
}

gboolean purple_protocols_remove(PurpleProtocol *protocol, GError **error)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), FALSE);
	g_return_val_if_fail(purple_protocol_get_id(protocol) != NULL, FALSE);

	if (purple_protocols_find(purple_protocol_get_id(protocol)) == NULL) {
		g_set_error(error, PURPLE_PROTOCOLS_DOMAIN, 0,
		            _("Protocol %s is not added."),
		            purple_protocol_get_id(protocol));

		return FALSE;
	}

	purple_debug_info("protocols", "Removing protocol %s\n",
	                  purple_protocol_get_id(protocol));

	purple_signal_emit(purple_protocols_get_handle(), "protocol-removed",
	                   protocol);

	g_hash_table_remove(protocols, purple_protocol_get_id(protocol));
	return TRUE;
}

GList *
purple_protocols_get_all(void)
{
	GList *ret = NULL;
	PurpleProtocol *protocol;
	GHashTableIter iter;

	g_hash_table_iter_init(&iter, protocols);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&protocol))
		ret = g_list_insert_sorted(ret, protocol, (GCompareFunc)compare_protocol);

	return ret;
}

/**************************************************************************
 * Protocols Subsystem API
 **************************************************************************/
void
purple_protocols_init(void)
{
	void *handle = purple_protocols_get_handle();

	protocols = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)g_object_unref);

	purple_signal_register(handle, "protocol-added",
	                       purple_marshal_VOID__POINTER,
	                       G_TYPE_NONE, 1, PURPLE_TYPE_PROTOCOL);
	purple_signal_register(handle, "protocol-removed",
	                       purple_marshal_VOID__POINTER,
	                       G_TYPE_NONE, 1, PURPLE_TYPE_PROTOCOL);
}

void *
purple_protocols_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_protocols_uninit(void) 
{
	g_hash_table_destroy(protocols);
}

