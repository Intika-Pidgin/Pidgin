/**
 * @file irc.c
 *
 * purple
 *
 * Copyright (C) 2003, Robbert Haarman <purple@inglorion.net>
 * Copyright (C) 2003, 2012 Ethan Blanton <elb@pidgin.im>
 * Copyright (C) 2000-2003, Rob Flynn <rob@tgflinux.com>
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#include "accountopt.h"
#include "buddylist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "protocol.h"
#include "plugins.h"
#include "purple-gio.h"
#include "util.h"
#include "version.h"

#include "irc.h"

#define PING_TIMEOUT 60

static void irc_ison_buddy_init(char *name, struct irc_buddy *ib, GList **list);

static const char *irc_blist_icon(PurpleAccount *a, PurpleBuddy *b);
static GList *irc_status_types(PurpleAccount *account);
static GList *irc_get_actions(PurpleConnection *gc);
/* static GList *irc_chat_info(PurpleConnection *gc); */
static void irc_login(PurpleAccount *account);
static void irc_login_cb(GObject *source, GAsyncResult *res, gpointer user_data);
static void irc_close(PurpleConnection *gc);
static int irc_im_send(PurpleConnection *gc, PurpleMessage *msg);
static int irc_chat_send(PurpleConnection *gc, int id, PurpleMessage *msg);
static void irc_chat_join (PurpleConnection *gc, GHashTable *data);
static void irc_read_input(struct irc_conn *irc);

static guint irc_nick_hash(const char *nick);
static gboolean irc_nick_equal(const char *nick1, const char *nick2);
static void irc_buddy_free(struct irc_buddy *ib);

PurpleProtocol *_irc_protocol = NULL;

static void irc_view_motd(PurpleProtocolAction *action)
{
	PurpleConnection *gc = action->connection;
	struct irc_conn *irc;
	char *title, *body;

	if (gc == NULL || purple_connection_get_protocol_data(gc) == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "irc", "got MOTD request for NULL gc\n");
		return;
	}
	irc = purple_connection_get_protocol_data(gc);
	if (irc->motd == NULL) {
		purple_notify_error(gc, _("Error displaying MOTD"),
			_("No MOTD available"),
			_("There is no MOTD associated with this connection."),
			purple_request_cpar_from_connection(gc));
		return;
	}
	title = g_strdup_printf(_("MOTD for %s"), irc->server);
	body = g_strdup_printf("<span style=\"font-family: monospace;\">%s</span>", irc->motd->str);
	purple_notify_formatted(gc, title, title, NULL, body, NULL, NULL);
	g_free(title);
	g_free(body);
}

static int irc_send_raw(PurpleConnection *gc, const char *buf, int len)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	if (len == -1) {
		len = strlen(buf);
	}
	irc_send_len(irc, buf, len);
	return len;
}

static void
irc_flush_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	PurpleConnection *gc = data;
	gboolean result;
	GError *error = NULL;

	result = g_output_stream_flush_finish(G_OUTPUT_STREAM(source),
			res, &error);

	if (!result) {
		g_prefix_error(&error, _("Lost connection with server: "));
		purple_connection_take_error(gc, error);
		return;
	}
}

int irc_send(struct irc_conn *irc, const char *buf)
{
    return irc_send_len(irc, buf, strlen(buf));
}

int irc_send_len(struct irc_conn *irc, const char *buf, int buflen)
{
 	char *tosend= g_strdup(buf);
	int len;
	GBytes *data;

	purple_signal_emit(_irc_protocol, "irc-sending-text", purple_account_get_connection(irc->account), &tosend);
	
	if (tosend == NULL)
		return 0;

	len = strlen(tosend);
	data = g_bytes_new_take(tosend, len);
	purple_queued_output_stream_push_bytes(irc->output, data);
	g_bytes_unref(data);

	if (!g_output_stream_has_pending(G_OUTPUT_STREAM(irc->output))) {
		/* Connection idle. Flush data. */
		g_output_stream_flush_async(G_OUTPUT_STREAM(irc->output),
				G_PRIORITY_DEFAULT, irc->cancellable,
				irc_flush_cb,
				purple_account_get_connection(irc->account));
	}

	return len;
}

/* XXX I don't like messing directly with these buddies */
gboolean irc_blist_timeout(struct irc_conn *irc)
{
	if (irc->ison_outstanding) {
		return TRUE;
	}

	g_hash_table_foreach(irc->buddies, (GHFunc)irc_ison_buddy_init,
	                     (gpointer *)&irc->buddies_outstanding);

	irc_buddy_query(irc);

	return TRUE;
}

void irc_buddy_query(struct irc_conn *irc)
{
	GList *lp;
	GString *string;
	struct irc_buddy *ib;
	char *buf;

	string = g_string_sized_new(512);

	while ((lp = g_list_first(irc->buddies_outstanding))) {
		ib = (struct irc_buddy *)lp->data;
		if (string->len + strlen(ib->name) + 1 > 450)
			break;
		g_string_append_printf(string, "%s ", ib->name);
		ib->new_online_status = FALSE;
		irc->buddies_outstanding = g_list_delete_link(irc->buddies_outstanding, lp);
	}

	if (string->len) {
		buf = irc_format(irc, "vn", "ISON", string->str);
		irc_send(irc, buf);
		g_free(buf);
		irc->ison_outstanding = TRUE;
	} else
		irc->ison_outstanding = FALSE;

	g_string_free(string, TRUE);
}

static void irc_ison_buddy_init(char *name, struct irc_buddy *ib, GList **list)
{
	*list = g_list_append(*list, ib);
}


static void irc_ison_one(struct irc_conn *irc, struct irc_buddy *ib)
{
	char *buf;

	if (irc->buddies_outstanding != NULL) {
		irc->buddies_outstanding = g_list_append(irc->buddies_outstanding, ib);
		return;
	}

	ib->new_online_status = FALSE;
	buf = irc_format(irc, "vn", "ISON", ib->name);
	irc_send(irc, buf);
	g_free(buf);
}


static const char *irc_blist_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "irc";
}

static GList *irc_status_types(PurpleAccount *account)
{
	PurpleStatusType *type;
	GList *types = NULL;

	type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(
		PURPLE_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(G_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE);
	types = g_list_append(types, type);

	return types;
}

static GList *irc_get_actions(PurpleConnection *gc)
{
	GList *list = NULL;
	PurpleProtocolAction *act = NULL;

	act = purple_protocol_action_new(_("View MOTD"), irc_view_motd);
	list = g_list_append(list, act);

	return list;
}

static GList *irc_chat_join_info(PurpleConnection *gc)
{
	GList *m = NULL;
	PurpleProtocolChatEntry *pce;

	pce = g_new0(PurpleProtocolChatEntry, 1);
	pce->label = _("_Channel:");
	pce->identifier = "channel";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	pce = g_new0(PurpleProtocolChatEntry, 1);
	pce->label = _("_Password:");
	pce->identifier = "password";
	pce->secret = TRUE;
	m = g_list_append(m, pce);

	return m;
}

static GHashTable *irc_chat_info_defaults(PurpleConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "channel", g_strdup(chat_name));

	return defaults;
}

static void irc_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	struct irc_conn *irc;
	char **userparts;
	const char *username = purple_account_get_username(account);
	GSocketClient *client;
	GError *error = NULL;

	gc = purple_account_get_connection(account);
	purple_connection_set_flags(gc, PURPLE_CONNECTION_FLAG_NO_NEWLINES |
		PURPLE_CONNECTION_FLAG_NO_IMAGES);

	if (strpbrk(username, " \t\v\r\n") != NULL) {
		purple_connection_take_error(gc, g_error_new_literal(
			PURPLE_CONNECTION_ERROR,
			PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
			_("IRC nick and server may not contain whitespace")));
		return;
	}

	irc = g_new0(struct irc_conn, 1);
	purple_connection_set_protocol_data(gc, irc);
	irc->account = account;
	irc->cancellable = g_cancellable_new();

	userparts = g_strsplit(username, "@", 2);
	purple_connection_set_display_name(gc, userparts[0]);
	irc->server = g_strdup(userparts[1]);
	g_strfreev(userparts);

	irc->buddies = g_hash_table_new_full((GHashFunc)irc_nick_hash, (GEqualFunc)irc_nick_equal,
					     NULL, (GDestroyNotify)irc_buddy_free);
	irc->cmds = g_hash_table_new(g_str_hash, g_str_equal);
	irc_cmd_table_build(irc);
	irc->msgs = g_hash_table_new(g_str_hash, g_str_equal);
	irc_msg_table_build(irc);

	purple_connection_update_progress(gc, _("Connecting"), 1, 2);

	client = purple_gio_socket_client_new(account, &error);

	if (client == NULL) {
		purple_connection_take_error(gc, error);
		return;
	}

	/* Optionally use TLS if it's set in the account settings */
	g_socket_client_set_tls(client,
			purple_account_get_bool(account, "ssl", FALSE));

	g_socket_client_connect_to_host_async(client, irc->server,
			purple_account_get_int(account, "port",
					g_socket_client_get_tls(client) ?
							IRC_DEFAULT_SSL_PORT :
							IRC_DEFAULT_PORT),
			irc->cancellable, irc_login_cb, gc);
	g_object_unref(client);
}

static gboolean do_login(PurpleConnection *gc) {
	char *buf, *tmp = NULL;
	char *server;
	const char *nickname, *identname, *realname;
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	const char *pass = purple_connection_get_password(gc);
#ifdef HAVE_CYRUS_SASL
	const gboolean use_sasl = purple_account_get_bool(irc->account, "sasl", FALSE);
#endif

	if (pass && *pass) {
#ifdef HAVE_CYRUS_SASL
		if (use_sasl)
			buf = irc_format(irc, "vv:", "CAP", "REQ", "sasl");
		else /* intended to fall through */
#endif
			buf = irc_format(irc, "v:", "PASS", pass);
		if (irc_send(irc, buf) < 0) {
			g_free(buf);
			return FALSE;
		}
		g_free(buf);
	}

	realname = purple_account_get_string(irc->account, "realname", "");
	identname = purple_account_get_string(irc->account, "username", "");

	if (identname == NULL || *identname == '\0') {
		identname = g_get_user_name();
	}

	if (identname != NULL && strchr(identname, ' ') != NULL) {
		tmp = g_strdup(identname);
		while ((buf = strchr(tmp, ' ')) != NULL) {
			*buf = '_';
		}
	}

	if (*irc->server == ':') {
		/* Same as hostname, above. */
		server = g_strdup_printf("0%s", irc->server);
	} else {
		server = g_strdup(irc->server);
	}

	buf = irc_format(irc, "vvvv:", "USER", tmp ? tmp : identname, "*", server,
	                 *realname == '\0' ? IRC_DEFAULT_ALIAS : realname);
	g_free(tmp);
	g_free(server);
	if (irc_send(irc, buf) < 0) {
		g_free(buf);
		return FALSE;
	}
	g_free(buf);
	nickname = purple_connection_get_display_name(gc);
	buf = irc_format(irc, "vn", "NICK", nickname);
	irc->reqnick = g_strdup(nickname);
	irc->nickused = FALSE;
	if (irc_send(irc, buf) < 0) {
		g_free(buf);
		return FALSE;
	}
	g_free(buf);

	irc->recv_time = time(NULL);

	return TRUE;
}

static void
irc_login_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	PurpleConnection *gc = user_data;
	GSocketConnection *conn;
	GError *error = NULL;
	struct irc_conn *irc;

	conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
			res, &error);

	if (conn == NULL) {
		g_prefix_error(&error, _("Unable to connect: "));
		purple_connection_take_error(gc, error);
		return;
	}

	irc = purple_connection_get_protocol_data(gc);
	irc->conn = conn;
	irc->output = purple_queued_output_stream_new(
			g_io_stream_get_output_stream(G_IO_STREAM(irc->conn)));

	if (do_login(gc)) {
		irc->input = g_data_input_stream_new(
				g_io_stream_get_input_stream(
						G_IO_STREAM(irc->conn)));
		irc_read_input(irc);
	}
}

static void irc_close(PurpleConnection *gc)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);

	if (irc == NULL)
		return;

	if (irc->conn != NULL)
		irc_cmd_quit(irc, "quit", NULL, NULL);

	if (irc->cancellable != NULL) {
		g_cancellable_cancel(irc->cancellable);
		g_clear_object(&irc->cancellable);
	}

	if (irc->conn != NULL) {
		purple_gio_graceful_close(G_IO_STREAM(irc->conn),
				G_INPUT_STREAM(irc->input),
				G_OUTPUT_STREAM(irc->output));
	}

	g_clear_object(&irc->input);
	g_clear_object(&irc->output);
	g_clear_object(&irc->conn);

	if (irc->timer)
		purple_timeout_remove(irc->timer);
	g_hash_table_destroy(irc->cmds);
	g_hash_table_destroy(irc->msgs);
	g_hash_table_destroy(irc->buddies);
	if (irc->motd)
		g_string_free(irc->motd, TRUE);
	g_free(irc->server);

	g_free(irc->mode_chars);
	g_free(irc->reqnick);

#ifdef HAVE_CYRUS_SASL
	if (irc->sasl_conn) {
		sasl_dispose(&irc->sasl_conn);
		irc->sasl_conn = NULL;
	}
	g_free(irc->sasl_cb);
	if(irc->sasl_mechs)
		g_string_free(irc->sasl_mechs, TRUE);
#endif


	g_free(irc);
}

static int irc_im_send(PurpleConnection *gc, PurpleMessage *msg)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	char *plain;
	const char *args[2];

	args[0] = irc_nick_skip_mode(irc, purple_message_get_recipient(msg));

	purple_markup_html_to_xhtml(purple_message_get_contents(msg),
		NULL, &plain);
	args[1] = plain;

	irc_cmd_privmsg(irc, "msg", NULL, args);
	g_free(plain);
	return 1;
}

static void irc_get_info(PurpleConnection *gc, const char *who)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	const char *args[2];
	args[0] = who;
	args[1] = NULL;
	irc_cmd_whois(irc, "whois", NULL, args);
}

static void irc_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	struct irc_conn *irc;
	const char *args[1];
	const char *status_id = purple_status_get_id(status);

	g_return_if_fail(gc != NULL);
	irc = purple_connection_get_protocol_data(gc);

	if (!purple_status_is_active(status))
		return;

	args[0] = NULL;

	if (!strcmp(status_id, "away")) {
		args[0] = purple_status_get_attr_string(status, "message");
		if ((args[0] == NULL) || (*args[0] == '\0'))
			args[0] = _("Away");
		irc_cmd_away(irc, "away", NULL, args);
	} else if (!strcmp(status_id, "available")) {
		irc_cmd_away(irc, "back", NULL, args);
	}
}

static void irc_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group, const char *message)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	struct irc_buddy *ib;
	const char *bname = purple_buddy_get_name(buddy);

	ib = g_hash_table_lookup(irc->buddies, bname);
	if (ib != NULL) {
		ib->ref++;
		purple_protocol_got_user_status(irc->account, bname,
				ib->online ? "available" : "offline", NULL);
	} else {
		ib = g_new0(struct irc_buddy, 1);
		ib->name = g_strdup(bname);
		ib->ref = 1;
		g_hash_table_replace(irc->buddies, ib->name, ib);
	}

	/* if the timer isn't set, this is during signon, so we don't want to flood
	 * ourself off with ISON's, so we don't, but after that we want to know when
	 * someone's online asap */
	if (irc->timer)
		irc_ison_one(irc, ib);
}

static void irc_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	struct irc_buddy *ib;

	ib = g_hash_table_lookup(irc->buddies, purple_buddy_get_name(buddy));
	if (ib && --ib->ref == 0) {
		g_hash_table_remove(irc->buddies, purple_buddy_get_name(buddy));
	}
}

static void
irc_read_input_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	PurpleConnection *gc = data;
	struct irc_conn *irc;
	gchar *line;
	gsize len;
	gsize start = 0;
	GError *error = NULL;

	line = g_data_input_stream_read_line_finish(
			G_DATA_INPUT_STREAM(source), res, &len, &error);

	if (line == NULL && error != NULL) {
		g_prefix_error(&error, _("Lost connection with server: "));
		purple_connection_take_error(gc, error);
		return;
	} else if (line == NULL) {
		purple_connection_take_error(gc, g_error_new_literal(
			PURPLE_CONNECTION_ERROR,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Server closed the connection")));
		return;
	}

	irc = purple_connection_get_protocol_data(gc);

	purple_connection_update_last_received(gc);

	if (len > 0 && line[len - 1] == '\r')
		line[len - 1] = '\0';

	/* This is a hack to work around the fact that marv gets messages
	 * with null bytes in them while using some weird irc server at work
 	 */
	while (start < len && line[start] == '\0')
		++start;

	if (len - start > 0)
		irc_parse_msg(irc, line + start);

	g_free(line);

	irc_read_input(irc);
}

static void
irc_read_input(struct irc_conn *irc)
{
	PurpleConnection *gc = purple_account_get_connection(irc->account);

	g_data_input_stream_read_line_async(irc->input,
			G_PRIORITY_DEFAULT, irc->cancellable,
			irc_read_input_cb, gc);
}

static void irc_chat_join (PurpleConnection *gc, GHashTable *data)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	const char *args[2];

	args[0] = g_hash_table_lookup(data, "channel");
	args[1] = g_hash_table_lookup(data, "password");
	irc_cmd_join(irc, "join", NULL, args);
}

static char *irc_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "channel"));
}

static void irc_chat_invite(PurpleConnection *gc, int id, const char *message, const char *name)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	PurpleConversation *convo = PURPLE_CONVERSATION(purple_conversations_find_chat(gc, id));
	const char *args[2];

	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, "irc", "Got chat invite request for bogus chat\n");
		return;
	}
	args[0] = name;
	args[1] = purple_conversation_get_name(convo);
	irc_cmd_invite(irc, "invite", purple_conversation_get_name(convo), args);
}


static void irc_chat_leave (PurpleConnection *gc, int id)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	PurpleConversation *convo = PURPLE_CONVERSATION(purple_conversations_find_chat(gc, id));
	const char *args[2];

	if (!convo)
		return;

	args[0] = purple_conversation_get_name(convo);
	args[1] = NULL;
	irc_cmd_part(irc, "part", purple_conversation_get_name(convo), args);
	purple_serv_got_chat_left(gc, id);
}

static int irc_chat_send(PurpleConnection *gc, int id, PurpleMessage *msg)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	PurpleConversation *convo = PURPLE_CONVERSATION(purple_conversations_find_chat(gc, id));
	const char *args[2];
	char *tmp;

	if (!convo) {
		purple_debug(PURPLE_DEBUG_ERROR, "irc", "chat send on nonexistent chat\n");
		return -EINVAL;
	}
#if 0
	if (*what == '/') {
		return irc_parse_cmd(irc, convo->name, what + 1);
	}
#endif
	purple_markup_html_to_xhtml(purple_message_get_contents(msg), NULL, &tmp);
	args[0] = purple_conversation_get_name(convo);
	args[1] = tmp;

	irc_cmd_privmsg(irc, "msg", NULL, args);

	/* TODO: use msg */
	purple_serv_got_chat_in(gc, id, purple_connection_get_display_name(gc),
		purple_message_get_flags(msg),
		purple_message_get_contents(msg), time(NULL));
	g_free(tmp);
	return 0;
}

static guint irc_nick_hash(const char *nick)
{
	char *lc;
	guint bucket;

	lc = g_utf8_strdown(nick, -1);
	bucket = g_str_hash(lc);
	g_free(lc);

	return bucket;
}

static gboolean irc_nick_equal(const char *nick1, const char *nick2)
{
	return (purple_utf8_strcasecmp(nick1, nick2) == 0);
}

static void irc_buddy_free(struct irc_buddy *ib)
{
	g_free(ib->name);
	g_free(ib);
}

static void irc_chat_set_topic(PurpleConnection *gc, int id, const char *topic)
{
	char *buf;
	const char *name = NULL;
	struct irc_conn *irc;

	irc = purple_connection_get_protocol_data(gc);
	name = purple_conversation_get_name(PURPLE_CONVERSATION(
			purple_conversations_find_chat(gc, id)));

	if (name == NULL)
		return;

	buf = irc_format(irc, "vt:", "TOPIC", name, topic);
	irc_send(irc, buf);
	g_free(buf);
}

static PurpleRoomlist *irc_roomlist_get_list(PurpleConnection *gc)
{
	struct irc_conn *irc;
	GList *fields = NULL;
	PurpleRoomlistField *f;
	char *buf;

	irc = purple_connection_get_protocol_data(gc);

	if (irc->roomlist)
		g_object_unref(irc->roomlist);

	irc->roomlist = purple_roomlist_new(purple_connection_get_account(gc));

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "channel", TRUE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(irc->roomlist, fields);

	buf = irc_format(irc, "v", "LIST");
	irc_send(irc, buf);
	g_free(buf);

	return irc->roomlist;
}

static void irc_roomlist_cancel(PurpleRoomlist *list)
{
	PurpleAccount *account = purple_roomlist_get_account(list);
	PurpleConnection *gc = purple_account_get_connection(account);
	struct irc_conn *irc;

	if (gc == NULL)
		return;

	irc = purple_connection_get_protocol_data(gc);

	purple_roomlist_set_in_progress(list, FALSE);

	if (irc->roomlist == list) {
		irc->roomlist = NULL;
		g_object_unref(list);
	}
}

static void irc_keepalive(PurpleConnection *gc)
{
	struct irc_conn *irc = purple_connection_get_protocol_data(gc);
	if ((time(NULL) - irc->recv_time) > PING_TIMEOUT)
		irc_cmd_ping(irc, NULL, NULL, NULL);
}

static gssize
irc_get_max_message_size(PurpleConversation *conv)
{
	/* TODO: this static value is got from pidgin-otr, but it depends on
	 * some factors, for example IRC channel name. */
	return 417;
}

static void
irc_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;

	protocol->id        = "prpl-irc";
	protocol->name      = "IRC";
	protocol->options   = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL |
	                      OPT_PROTO_SLASH_COMMANDS_NATIVE;

	split = purple_account_user_split_new(_("Server"), IRC_DEFAULT_SERVER, '@');
	protocol->user_splits = g_list_append(protocol->user_splits, split);

	option = purple_account_option_int_new(_("Port"), "port", IRC_DEFAULT_PORT);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Encodings"), "encoding", IRC_DEFAULT_CHARSET);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(_("Auto-detect incoming UTF-8"), "autodetect_utf8", IRC_DEFAULT_AUTODETECT);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Ident name"), "username", "");
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Real name"), "realname", "");
	protocol->account_options = g_list_append(protocol->account_options, option);

	/*
	option = purple_account_option_string_new(_("Quit message"), "quitmsg", IRC_DEFAULT_QUIT);
	protocol->account_options = g_list_append(protocol->account_options, option);
	*/

	option = purple_account_option_bool_new(_("Use SSL"), "ssl", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

#ifdef HAVE_CYRUS_SASL
	option = purple_account_option_bool_new(_("Authenticate with SASL"), "sasl", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(
						_("Allow plaintext SASL auth over unencrypted connection"),
						"auth_plain_in_clear", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);
#endif
}

static void
irc_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->login        = irc_login;
	klass->close        = irc_close;
	klass->status_types = irc_status_types;
	klass->list_icon    = irc_blist_icon;
}

static void
irc_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_actions          = irc_get_actions;
	client_iface->normalize            = purple_normalize_nocase;
	client_iface->get_max_message_size = irc_get_max_message_size;
}

static void
irc_protocol_server_iface_init(PurpleProtocolServerIface *server_iface)
{
	server_iface->set_status   = irc_set_status;
	server_iface->get_info     = irc_get_info;
	server_iface->add_buddy    = irc_add_buddy;
	server_iface->remove_buddy = irc_remove_buddy;
	server_iface->keepalive    = irc_keepalive;
	server_iface->send_raw     = irc_send_raw;
}

static void
irc_protocol_im_iface_init(PurpleProtocolIMIface *im_iface)
{
	im_iface->send = irc_im_send;
}

static void
irc_protocol_chat_iface_init(PurpleProtocolChatIface *chat_iface)
{
	chat_iface->info          = irc_chat_join_info;
	chat_iface->info_defaults = irc_chat_info_defaults;
	chat_iface->join          = irc_chat_join;
	chat_iface->get_name      = irc_get_chat_name;
	chat_iface->invite        = irc_chat_invite;
	chat_iface->leave         = irc_chat_leave;
	chat_iface->send          = irc_chat_send;
	chat_iface->set_topic     = irc_chat_set_topic;
}

static void
irc_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *roomlist_iface)
{
	roomlist_iface->get_list = irc_roomlist_get_list;
	roomlist_iface->cancel   = irc_roomlist_cancel;
}

static void
irc_protocol_xfer_iface_init(PurpleProtocolXferIface *xfer_iface)
{
	xfer_iface->send     = irc_dccsend_send_file;
	xfer_iface->new_xfer = irc_dccsend_new_xfer;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	IRCProtocol, irc_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  irc_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  irc_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  irc_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  irc_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  irc_protocol_roomlist_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                  irc_protocol_xfer_iface_init)
);

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "prpl-irc",
		"name",         "IRC Protocol",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("IRC Protocol Plugin"),
		"description",  N_("The IRC Protocol Plugin that Sucks Less"),
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	irc_protocol_register_type(plugin);

	_irc_protocol = purple_protocols_add(IRC_TYPE_PROTOCOL, error);
	if (!_irc_protocol)
		return FALSE;

	purple_prefs_remove("/plugins/prpl/irc/quitmsg");
	purple_prefs_remove("/plugins/prpl/irc");

	irc_register_commands();

	purple_signal_register(_irc_protocol, "irc-sending-text",
			     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			     PURPLE_TYPE_CONNECTION,
			     G_TYPE_POINTER); /* pointer to a string */
	purple_signal_register(_irc_protocol, "irc-receiving-text",
			     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			     PURPLE_TYPE_CONNECTION,
			     G_TYPE_POINTER); /* pointer to a string */

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	irc_unregister_commands();

	if (!purple_protocols_remove(_irc_protocol, error))
		return FALSE;

	return TRUE;
}

PURPLE_PLUGIN_INIT(irc, plugin_query, plugin_load, plugin_unload);
