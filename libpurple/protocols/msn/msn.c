/**
 * @file msn.c The MSN protocol plugin
 *
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
#define PHOTO_SUPPORT 1

#include "internal.h"

#include "debug.h"
#include "http.h"
#include "request.h"

#include "accountopt.h"
#include "contact.h"
#include "msg.h"
#include "page.h"
#include "plugins.h"
#include "prefs.h"
#include "session.h"
#include "smiley.h"
#include "smiley-custom.h"
#include "smiley-parser.h"
#include "state.h"
#include "util.h"
#include "cmds.h"
#include "core.h"
#include "protocol.h"
#include "msnutils.h"
#include "version.h"

#include "error.h"
#include "msg.h"
#include "switchboard.h"
#include "notification.h"
#include "slplink.h"

#if PHOTO_SUPPORT
#define MAX_HTTP_BUDDYICON_BYTES (200 * 1024)
#include "image-store.h"
#endif

typedef struct
{
	PurpleConnection *gc;
	const char *passport;

} MsnMobileData;

typedef struct
{
	PurpleConnection *gc;
	char *name;

} MsnGetInfoData;

typedef struct
{
	MsnGetInfoData *info_data;
	char *stripped;
	char *url_buffer;
	PurpleNotifyUserInfo *user_info;
	char *photo_url_text;

} MsnGetInfoStepTwoData;

typedef struct
{
	PurpleConnection *gc;
	const char *who;
	char *msg;
	PurpleMessageFlags flags;
	time_t when;
} MsnIMData;

typedef struct
{
	char *smile;
	PurpleSmiley *ps;
	MsnObject *obj;
} MsnEmoticon;

static PurpleProtocol *my_protocol = NULL;
static GSList *cmds = NULL;

static const char *
msn_normalize(const PurpleAccount *account, const char *str)
{
	static char buf[BUF_LEN];
	char *tmp;

	g_return_val_if_fail(str != NULL, NULL);

	tmp = g_strchomp(g_utf8_strdown(str, -1));
	g_snprintf(buf, sizeof(buf), "%s%s", tmp,
			   (strchr(tmp, '@') ? "" : "@hotmail.com"));
	g_free(tmp);

	return buf;
}

static gboolean
msn_send_attention(PurpleConnection *gc, const char *username, guint type)
{
	MsnMessage *msg;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	msg = msn_message_new_nudge();
	session = purple_connection_get_protocol_data(gc);
	swboard = msn_session_get_swboard(session, username, MSN_SB_FLAG_IM);

	msn_switchboard_send_msg(swboard, msg, TRUE);
	msn_message_unref(msg);

	return TRUE;
}

static GList *
msn_attention_types(PurpleAccount *account)
{
	static GList *list = NULL;

	if (!list) {
		list = g_list_append(list, purple_attention_type_new("Nudge", _("Nudge"),
				_("%s has nudged you!"), _("Nudging %s...")));
	}

	return list;
}

static GHashTable *
msn_get_account_text_table(PurpleAccount *unused)
{
	GHashTable *table;

	table = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_insert(table, "login_label", (gpointer)_("Email Address..."));

	return table;
}

static PurpleCmdRet
msn_cmd_nudge(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleConnection *gc = purple_account_get_connection(account);
	const gchar *username;

	username = purple_conversation_get_name(conv);

	purple_protocol_send_attention(gc, username, MSN_NUDGE);

	return PURPLE_CMD_RET_OK;
}

struct public_alias_closure
{
	PurpleAccount *account;
	gpointer success_cb;
	gpointer failure_cb;
};

static gboolean
set_public_alias_length_error(gpointer data)
{
	struct public_alias_closure *closure = data;
	PurpleSetPublicAliasFailureCallback failure_cb = closure->failure_cb;

	failure_cb(closure->account, _("Your new MSN friendly name is too long."));

	g_object_unref(closure->account);
	g_free(closure);

	return FALSE;
}

static void
prp_success_cb(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	const char *type, *friendlyname;
	struct public_alias_closure *closure;

	g_return_if_fail(cmd->param_count >= 3);
	type = cmd->params[1];
	g_return_if_fail(!strcmp(type, "MFN"));

	closure = cmd->trans->data;
	friendlyname = purple_url_decode(cmd->params[2]);

	msn_update_contact(cmdproc->session, "Me", MSN_UPDATE_DISPLAY, friendlyname);

	purple_connection_set_display_name(
		purple_account_get_connection(closure->account),
		friendlyname);
	purple_account_set_string(closure->account, "display-name", friendlyname);

	if (closure->success_cb) {
		PurpleSetPublicAliasSuccessCallback success_cb = closure->success_cb;
		success_cb(closure->account, friendlyname);
	}
}

static void
prp_error_cb(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	struct public_alias_closure *closure = trans->data;
	PurpleSetPublicAliasFailureCallback failure_cb = closure->failure_cb;
	gboolean debug;
	const char *error_text;

	error_text = msn_error_get_text(error, &debug);
	failure_cb(closure->account, error_text);
}

static void
prp_timeout_cb(MsnCmdProc *cmdproc, MsnTransaction *trans)
{
	struct public_alias_closure *closure = trans->data;
	PurpleSetPublicAliasFailureCallback failure_cb = closure->failure_cb;
	failure_cb(closure->account, _("Connection Timeout"));
}

void
msn_set_public_alias(PurpleConnection *pc, const char *alias,
                     PurpleSetPublicAliasSuccessCallback success_cb,
                     PurpleSetPublicAliasFailureCallback failure_cb)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	MsnTransaction *trans;
	PurpleAccount *account;
	char real_alias[BUDDY_ALIAS_MAXLEN + 1];
	struct public_alias_closure *closure;

	session = purple_connection_get_protocol_data(pc);
	cmdproc = session->notification->cmdproc;
	account = purple_connection_get_account(pc);

	if (alias && *alias) {
		if (!msn_encode_spaces(alias, real_alias, BUDDY_ALIAS_MAXLEN + 1)) {
			if (failure_cb) {
				struct public_alias_closure *closure =
					g_new0(struct public_alias_closure, 1);
				closure->account = g_object_ref(account);
				closure->failure_cb = failure_cb;
				purple_timeout_add(0, set_public_alias_length_error, closure);
			} else {
				purple_notify_error(pc, NULL, _("Your new MSN "
					"friendly name is too long."), NULL,
					purple_request_cpar_from_connection(pc));
			}
			return;
		}

		if (real_alias[0] == '\0')
			g_strlcpy(real_alias, purple_account_get_username(account), sizeof(real_alias));
	} else
		g_strlcpy(real_alias, purple_account_get_username(account), sizeof(real_alias));

	closure = g_new0(struct public_alias_closure, 1);
	closure->account = account;
	closure->success_cb = success_cb;
	closure->failure_cb = failure_cb;

	trans = msn_transaction_new(cmdproc, "PRP", "MFN %s", real_alias);
	msn_transaction_set_data(trans, closure);
	msn_transaction_set_data_free(trans, g_free);
	msn_transaction_add_cb(trans, "PRP", prp_success_cb);
	if (failure_cb) {
		msn_transaction_set_error_cb(trans, prp_error_cb);
		msn_transaction_set_timeout_cb(trans, prp_timeout_cb);
	}
	msn_cmdproc_send_trans(cmdproc, trans);
}

static gboolean
get_public_alias_cb(gpointer data)
{
	struct public_alias_closure *closure = data;
	PurpleGetPublicAliasSuccessCallback success_cb = closure->success_cb;
	const char *alias;

	alias = purple_account_get_string(closure->account, "display-name",
	                                  purple_account_get_username(closure->account));
	success_cb(closure->account, alias);

	g_object_unref(closure->account);
	g_free(closure);

	return FALSE;
}

static void
msn_get_public_alias(PurpleConnection *pc,
                     PurpleGetPublicAliasSuccessCallback success_cb,
                     PurpleGetPublicAliasFailureCallback failure_cb)
{
	struct public_alias_closure *closure = g_new0(struct public_alias_closure, 1);
	PurpleAccount *account = purple_connection_get_account(pc);

	closure->account = g_object_ref(account);
	closure->success_cb = success_cb;
	purple_timeout_add(0, get_public_alias_cb, closure);
}

static void
msn_act_id(PurpleConnection *gc, const char *entry)
{
	msn_set_public_alias(gc, entry, NULL, NULL);
}

static void
msn_set_prp(PurpleConnection *gc, const char *type, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	MsnTransaction *trans;

	session = purple_connection_get_protocol_data(gc);
	cmdproc = session->notification->cmdproc;

	if (entry == NULL || *entry == '\0')
	{
		trans = msn_transaction_new(cmdproc, "PRP", "%s", type);
	}
	else
	{
		trans = msn_transaction_new(cmdproc, "PRP", "%s %s", type,
						 purple_url_encode(entry));
	}
	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
msn_set_home_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHH", entry);
}

static void
msn_set_work_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHW", entry);
}

static void
msn_set_mobile_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHM", entry);
}

static void
enable_msn_pages_cb(PurpleConnection *gc)
{
	msn_set_prp(gc, "MOB", "Y");
}

static void
disable_msn_pages_cb(PurpleConnection *gc)
{
	msn_set_prp(gc, "MOB", "N");
}

static void
send_to_mobile(PurpleConnection *gc, const char *who, const char *entry)
{
	MsnTransaction *trans;
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnPage *page;
	MsnMessage *msg;
	MsnUser *user;
	char *payload = NULL;
	const char *mobile_number = NULL;
	gsize payload_len;

	session = purple_connection_get_protocol_data(gc);
	cmdproc = session->notification->cmdproc;

	page = msn_page_new();
	msn_page_set_body(page, entry);

	payload = msn_page_gen_payload(page, &payload_len);

	if ((user = msn_userlist_find_user(session->userlist, who)) &&
		(mobile_number = msn_user_get_mobile_phone(user)) &&
		mobile_number[0] == '+') {
		/* if msn_user_get_mobile_phone() has a + in front, it's a number
		   that from the buddy's contact card */
		trans = msn_transaction_new(cmdproc, "PGD", "tel:%s 1 %" G_GSIZE_FORMAT,
			mobile_number, payload_len);
	} else {
		/* otherwise we send to whatever phone number the buddy registered
		   with msn */
		trans = msn_transaction_new(cmdproc, "PGD", "%s 1 %" G_GSIZE_FORMAT,
			who, payload_len);
	}

	msn_transaction_set_payload(trans, payload, payload_len);
	g_free(payload);

	msg = msn_message_new_plain(entry);
	msn_transaction_set_data(trans, msg);

	msn_page_destroy(page);

	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
send_to_mobile_cb(MsnMobileData *data, const char *entry)
{
	send_to_mobile(data->gc, data->passport, entry);
	g_free(data);
}

static void
close_mobile_page_cb(MsnMobileData *data, const char *entry)
{
	g_free(data);
}

/* -- */

static void
msn_show_set_friendly_name(PurpleProtocolAction *action)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	char *tmp;

	gc = action->connection;
	account = purple_connection_get_account(gc);

	tmp = g_strdup_printf(_("Set friendly name for %s."),
	                      purple_account_get_username(account));
	purple_request_input(gc, _("Set Friendly Name"), tmp,
					   _("This is the name that other MSN buddies will "
						 "see you as."),
					   purple_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_act_id),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_connection(gc),
					   gc);
	g_free(tmp);
}

typedef struct MsnLocationData {
	PurpleAccount *account;
	MsnSession *session;
	PurpleRequestFieldGroup *group;
} MsnLocationData;

static void
update_endpoint_cb(MsnLocationData *data, PurpleRequestFields *fields)
{
	PurpleAccount *account;
	MsnSession *session;
	const char *old_name;
	const char *name;
	GList *others;

	session = data->session;
	account = data->account;

	/* Update the current location's name */
	old_name = purple_account_get_string(account, "endpoint-name", NULL);
	name = purple_request_fields_get_string(fields, "endpoint-name");
	if (!g_str_equal(old_name, name)) {
		purple_account_set_string(account, "endpoint-name", name);
		msn_notification_send_uux_private_endpointdata(session);
	}

	/* Sign out other locations */
	for (others = purple_request_field_group_get_fields(data->group);
	     others;
	     others = g_list_next(others)) {
		PurpleRequestField *field = others->data;
		if (purple_request_field_get_field_type(field) != PURPLE_REQUEST_FIELD_BOOLEAN)
			continue;
		if (purple_request_field_bool_get_value(field)) {
			const char *id = purple_request_field_get_id(field);
			char *user;
			purple_debug_info("msn", "Disconnecting Endpoint %s\n", id);

			user = g_strdup_printf("%s;%s", purple_account_get_username(account), id);
			msn_notification_send_uun(session, user, MSN_UNIFIED_NOTIFICATION_MPOP, "goawyplzthxbye");
			g_free(user);
		}
	}

	g_free(data);
}

static void
msn_show_locations(PurpleProtocolAction *action)
{
	PurpleConnection *pc;
	PurpleAccount *account;
	MsnSession *session;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	gboolean have_other_endpoints;
	GSList *l;
	MsnLocationData *data;

	pc = action->connection;
	account = purple_connection_get_account(pc);
	session = purple_connection_get_protocol_data(pc);

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(_("This Location"));
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_label_new("endpoint-label", _("This is the name that identifies this location"));
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("endpoint-name",
	                                        _("Name"),
	                                        purple_account_get_string(account, "endpoint-name", NULL),
	                                        FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	group = purple_request_field_group_new(_("Other Locations"));
	purple_request_fields_add_group(fields, group);

	have_other_endpoints = FALSE;
	for (l = session->user->endpoints; l; l = l->next) {
		MsnUserEndpoint *ep = l->data;

		if (ep->id[0] != '\0' && strncasecmp(ep->id + 1, session->guid, 36) == 0)
			/* Don't add myself to the list */
			continue;

		if (!have_other_endpoints) {
			/* We do in fact have an endpoint other than ourselves... let's
			   add a label */
			field = purple_request_field_label_new("others-label",
					_("You can sign out from other locations here"));
			purple_request_field_group_add_field(group, field);
		}

		have_other_endpoints = TRUE;
		field = purple_request_field_bool_new(ep->id, ep->name, FALSE);
		purple_request_field_group_add_field(group, field);
	}
	if (!have_other_endpoints) {
		/* TODO: Due to limitations in our current request field API, the
		   following string will show up with a trailing colon.  This should
		   be fixed either by adding an "include_colon" boolean, or creating
		   a separate purple_request_field_label_new_without_colon function,
		   or by never automatically adding the colon and requiring that
		   callers add the colon themselves. */
		field = purple_request_field_label_new("others-label", _("You are not signed in from any other locations."));
		purple_request_field_group_add_field(group, field);
	}

	data = g_new0(MsnLocationData, 1);
	data->account = account;
	data->session = session;
	data->group = group;

	purple_request_fields(pc, NULL, NULL, NULL, fields, _("OK"),
		G_CALLBACK(update_endpoint_cb), _("Cancel"), G_CALLBACK(g_free),
		purple_request_cpar_from_connection(pc), data);
}

static void
enable_mpop_cb(PurpleConnection *pc)
{
	MsnSession *session = purple_connection_get_protocol_data(pc);

	purple_debug_info("msn", "Enabling MPOP\n");

	session->enable_mpop = TRUE;
	msn_annotate_contact(session, "Me", "MSN.IM.MPOP", "1", NULL);

	purple_protocol_got_account_actions(purple_connection_get_account(pc));
}

static void
disable_mpop_cb(PurpleConnection *pc)
{
	PurpleAccount *account = purple_connection_get_account(pc);
	MsnSession *session = purple_connection_get_protocol_data(pc);
	GSList *l;

	purple_debug_info("msn", "Disabling MPOP\n");

	session->enable_mpop = FALSE;
	msn_annotate_contact(session, "Me", "MSN.IM.MPOP", "0", NULL);

	for (l = session->user->endpoints; l; l = l->next) {
		MsnUserEndpoint *ep = l->data;
		char *user;

		if (ep->id[0] != '\0' && strncasecmp(ep->id + 1, session->guid, 36) == 0)
			/* Don't kick myself */
			continue;

		purple_debug_info("msn", "Disconnecting Endpoint %s\n", ep->id);

		user = g_strdup_printf("%s;%s", purple_account_get_username(account), ep->id);
		msn_notification_send_uun(session, user, MSN_UNIFIED_NOTIFICATION_MPOP, "goawyplzthxbye");
		g_free(user);
	}

	purple_protocol_got_account_actions(account);
}

static void
msn_show_set_mpop(PurpleProtocolAction *action)
{
	PurpleConnection *pc;

	pc = action->connection;

	purple_request_action(pc, NULL, _("Allow multiple logins?"),
						_("Do you want to allow or disallow connecting from "
						  "multiple locations simultaneously?"),
						PURPLE_DEFAULT_ACTION_NONE,
						purple_request_cpar_from_connection(pc),
						pc, 3,
						_("Allow"), G_CALLBACK(enable_mpop_cb),
						_("Disallow"), G_CALLBACK(disable_mpop_cb),
						_("Cancel"), NULL);
}

static void
msn_show_set_home_phone(PurpleProtocolAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = action->connection;
	session = purple_connection_get_protocol_data(gc);

	purple_request_input(gc, NULL, _("Set your home phone number."), NULL,
					   msn_user_get_home_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_home_phone_cb),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_connection(gc),
					   gc);
}

static void
msn_show_set_work_phone(PurpleProtocolAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = action->connection;
	session = purple_connection_get_protocol_data(gc);

	purple_request_input(gc, NULL, _("Set your work phone number."), NULL,
					   msn_user_get_work_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_work_phone_cb),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_connection(gc),
					   gc);
}

static void
msn_show_set_mobile_phone(PurpleProtocolAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = action->connection;
	session = purple_connection_get_protocol_data(gc);

	purple_request_input(gc, NULL, _("Set your mobile phone number."), NULL,
					   msn_user_get_mobile_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_mobile_phone_cb),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_connection(gc),
					   gc);
}

static void
msn_show_set_mobile_pages(PurpleProtocolAction *action)
{
	PurpleConnection *gc;

	gc = action->connection;

	purple_request_action(gc, NULL, _("Allow MSN Mobile pages?"),
						_("Do you want to allow or disallow people on "
						  "your buddy list to send you MSN Mobile pages "
						  "to your cell phone or other mobile device?"),
						PURPLE_DEFAULT_ACTION_NONE,
						purple_request_cpar_from_connection(gc),
						gc, 3,
						_("Allow"), G_CALLBACK(enable_msn_pages_cb),
						_("Disallow"), G_CALLBACK(disable_msn_pages_cb),
						_("Cancel"), NULL);
}

/* QuLogic: Disabled until confirmed correct. */
#if 0
static void
msn_show_blocked_text(PurpleProtocolAction *action)
{
	PurpleConnection *pc = action->connection;
	MsnSession *session;
	char *title;

	session = purple_connection_get_protocol_data(pc);

	title = g_strdup_printf(_("Blocked Text for %s"), session->account->username);
	if (session->blocked_text == NULL) {
		purple_notify_formatted(pc, title, title, NULL, _("No text is blocked for this account."), NULL, NULL);
	} else {
		char *blocked_text;
		blocked_text = g_strdup_printf(_("MSN servers are currently blocking the following regular expressions:<br/>%s"),
		                               session->blocked_text);

		purple_notify_formatted(pc, title, title, NULL, blocked_text, NULL, NULL);
		g_free(blocked_text);
	}
	g_free(title);
}
#endif

static void
msn_show_hotmail_inbox(PurpleProtocolAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = action->connection;
	session = purple_connection_get_protocol_data(gc);

	if (!session->passport_info.email_enabled) {
		purple_notify_error(gc, NULL, _("This account does not have "
			"email enabled."), NULL,
			purple_request_cpar_from_connection(gc));
		return;
	}

	/** apparently the correct value is 777, use 750 as a failsafe */
	if ((session->passport_info.mail_url == NULL)
		|| (time (NULL) - session->passport_info.mail_timestamp >= 750)) {
		MsnTransaction *trans;
		MsnCmdProc *cmdproc;

		cmdproc = session->notification->cmdproc;

		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_set_data(trans, GUINT_TO_POINTER(TRUE));

		msn_cmdproc_send_trans(cmdproc, trans);

	} else
		purple_notify_uri(gc, session->passport_info.mail_url);
}

static void
show_send_to_mobile_cb(PurpleBlistNode *node, gpointer ignored)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	MsnMobileData *data;
	PurpleAccount *account;
	const char *name;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	account = purple_buddy_get_account(buddy);
	gc = purple_account_get_connection(account);
	name = purple_buddy_get_name(buddy);

	data = g_new0(MsnMobileData, 1);
	data->gc = gc;
	data->passport = name;

	purple_request_input(gc, NULL, _("Send a mobile message."), NULL,
					   NULL, TRUE, FALSE, NULL,
					   _("Page"), G_CALLBACK(send_to_mobile_cb),
					   _("Close"), G_CALLBACK(close_mobile_page_cb),
					   purple_request_cpar_from_connection(gc),
					   data);
}

static gboolean
msn_offline_message(const PurpleBuddy *buddy) {
	return TRUE;
}

void
msn_send_privacy(PurpleConnection *gc)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;

	account = purple_connection_get_account(gc);
	session = purple_connection_get_protocol_data(gc);
	cmdproc = session->notification->cmdproc;

	if (purple_account_get_privacy_type(account) == PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL ||
	    purple_account_get_privacy_type(account) == PURPLE_ACCOUNT_PRIVACY_DENY_USERS)
		trans = msn_transaction_new(cmdproc, "BLP", "%s", "AL");
	else
		trans = msn_transaction_new(cmdproc, "BLP", "%s", "BL");

	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
initiate_chat_cb(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	PurpleAccount *account;

	MsnSession *session;
	MsnSwitchBoard *swboard;

	const char *alias;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	account = purple_buddy_get_account(buddy);
	gc = purple_account_get_connection(account);

	session = purple_connection_get_protocol_data(gc);

	swboard = msn_switchboard_new(session);
	msn_switchboard_request(swboard);
	msn_switchboard_request_add_user(swboard, purple_buddy_get_name(buddy));

	/* TODO: This might move somewhere else, after USR might be */
	swboard->chat_id = msn_switchboard_get_chat_id();
	swboard->conv = PURPLE_CONVERSATION(purple_serv_got_joined_chat(gc,
			swboard->chat_id, "MSN Chat"));
	swboard->flag = MSN_SB_FLAG_IM;

	/* Local alias > Display name > Username */
	if ((alias = purple_account_get_private_alias(account)) == NULL)
		if ((alias = purple_connection_get_display_name(gc)) == NULL)
			alias = purple_account_get_username(account);

	purple_chat_conversation_add_user(PURPLE_CHAT_CONVERSATION(swboard->conv),
	                          alias, NULL, PURPLE_CHAT_USER_NONE, TRUE);
}

static void
t_msn_xfer_init(PurpleXfer *xfer)
{
	msn_request_ft(xfer);
}

static void
t_msn_xfer_cancel_send(PurpleXfer *xfer)
{
	MsnSlpLink *slplink = purple_xfer_get_protocol_data(xfer);
	msn_slplink_unref(slplink);
}

static PurpleXfer*
msn_new_xfer(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	PurpleXfer *xfer;

	session = purple_connection_get_protocol_data(gc);

	xfer = purple_xfer_new(purple_connection_get_account(gc), PURPLE_XFER_TYPE_SEND, who);

	g_return_val_if_fail(xfer != NULL, NULL);

	purple_xfer_set_protocol_data(xfer, msn_slplink_ref(msn_session_get_slplink(session, who)));

	purple_xfer_set_init_fnc(xfer, t_msn_xfer_init);
	purple_xfer_set_cancel_send_fnc(xfer, t_msn_xfer_cancel_send);

	return xfer;
}

static void
msn_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	PurpleXfer *xfer = msn_new_xfer(gc, who);

	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

static gboolean
msn_can_receive_file(PurpleConnection *gc, const char *who)
{
	PurpleAccount *account;
	gchar *normal;
	gboolean ret;

	account = purple_connection_get_account(gc);

	normal = g_strdup(msn_normalize(account, purple_account_get_username(account)));
	ret = strcmp(normal, msn_normalize(account, who));
	g_free(normal);

	if (ret) {
		MsnSession *session = purple_connection_get_protocol_data(gc);
		if (session) {
			MsnUser *user = msn_userlist_find_user(session->userlist, who);
			if (user) {
				/* Include these too: MSN_CAP_MOBILE_ON|MSN_CAP_WEB_WATCH ? */
				if ((user->clientid & MSN_CAP_VIA_WEBIM) ||
						user->networkid == MSN_NETWORK_YAHOO)
					ret = FALSE;
				else
					ret = TRUE;
			}
		} else
			ret = FALSE;
	}

	return ret;
}

/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
msn_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "msn";
}

static const char *
msn_list_emblems(PurpleBuddy *b)
{
	MsnUser *user = purple_buddy_get_protocol_data(b);

	if (user != NULL) {
		if (user->clientid & MSN_CAP_BOT)
			return "bot";
		if (user->clientid & MSN_CAP_VIA_MOBILE)
			return "mobile";
#if 0
		/* XXX: Since we don't support this, there's no point in showing it just yet */
		if (user->clientid & MSN_CAP_SCHANNEL)
			return "secure";
#endif
		if (user->clientid & MSN_CAP_VIA_WEBIM)
			return "external";
		if (user->networkid == MSN_NETWORK_YAHOO)
			return "yahoo";
	}

	return NULL;
}

/*
 * Set the User status text
 */
static char *
msn_status_text(PurpleBuddy *buddy)
{
	PurplePresence *presence;
	PurpleStatus *status;
	const char *msg;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);

	/* Official client says media takes precedence over message */
	/* I say message take precedence over media! Plus jabber agrees
	   too */
	msg = purple_status_get_attr_string(status, "message");
	if (msg && *msg)
		return g_markup_escape_text(msg, -1);

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_TUNE)) {
		const char *title, *game, *office;
		char *media, *esc;
		status = purple_presence_get_status(presence, "tune");
		title = purple_status_get_attr_string(status, PURPLE_TUNE_TITLE);

		game = purple_status_get_attr_string(status, "game");
		office = purple_status_get_attr_string(status, "office");

		if (title && *title) {
			const char *artist = purple_status_get_attr_string(status, PURPLE_TUNE_ARTIST);
			const char *album = purple_status_get_attr_string(status, PURPLE_TUNE_ALBUM);
			media = purple_util_format_song_info(title, artist, album, NULL);
			return media;
		}
		else if (game && *game)
			media = g_strdup_printf("Playing %s", game);
		else if (office && *office)
			media = g_strdup_printf("Editing %s", office);
		else
			return NULL;
		esc = g_markup_escape_text(media, -1);
		g_free(media);
		return esc;
	}

	return NULL;
}

static void
msn_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	MsnUser *user;
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);

	user = purple_buddy_get_protocol_data(buddy);

	if (purple_presence_is_online(presence))
	{
		const char *psm, *name;
		const char *mediatype = NULL;
		char *currentmedia = NULL;

		psm = purple_status_get_attr_string(status, "message");
		if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_TUNE)) {
			PurpleStatus *tune = purple_presence_get_status(presence, "tune");
			const char *title = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
			const char *game = purple_status_get_attr_string(tune, "game");
			const char *office = purple_status_get_attr_string(tune, "office");
			if (title && *title) {
				const char *artist = purple_status_get_attr_string(tune, PURPLE_TUNE_ARTIST);
				const char *album = purple_status_get_attr_string(tune, PURPLE_TUNE_ALBUM);
				mediatype = _("Now Listening");
				currentmedia = purple_util_format_song_info(title, artist, album, NULL);
			} else if (game && *game) {
				mediatype = _("Playing a game");
				currentmedia = g_strdup(game);
			} else if (office && *office) {
				mediatype = _("Working");
				currentmedia = g_strdup(office);
			}
		}

		if (!purple_status_is_available(status)) {
			name = purple_status_get_name(status);
		} else {
			name = NULL;
		}

		if (name != NULL && *name) {
			char *tmp2;

			tmp2 = g_markup_escape_text(name, -1);
			if (purple_presence_is_idle(presence)) {
				char *idle;
				char *tmp3;
				/* Never know what those translations might end up like... */
				idle = g_markup_escape_text(_("Idle"), -1);
				tmp3 = g_strdup_printf("%s/%s", tmp2, idle);
				g_free(idle);
				g_free(tmp2);
				tmp2 = tmp3;
			}

			if (psm != NULL && *psm) {
				purple_notify_user_info_add_pair_plaintext(user_info, tmp2, psm);
			} else {
				purple_notify_user_info_add_pair_html(user_info, _("Status"), tmp2);
			}

			g_free(tmp2);
		} else {
			if (psm != NULL && *psm) {
				if (purple_presence_is_idle(presence)) {
					purple_notify_user_info_add_pair_plaintext(user_info, _("Idle"), psm);
				} else {
					purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), psm);
				}
			} else {
				if (purple_presence_is_idle(presence)) {
					purple_notify_user_info_add_pair_plaintext(user_info,
							_("Status"), _("Idle"));
				} else {
					purple_notify_user_info_add_pair_plaintext(user_info,
							_("Status"), purple_status_get_name(status));
				}
			}
		}

		if (currentmedia) {
			purple_notify_user_info_add_pair_html(user_info, mediatype, currentmedia);
			g_free(currentmedia);
		}
	}

	/* XXX: This is being shown in non-full tooltips because the
	 * XXX: blocked icon overlay isn't always accurate for MSN.
	 * XXX: This can die as soon as purple_privacy_check() knows that
	 * XXX: this protocol always honors both the allow and deny lists. */
	/* While the above comment may be strictly correct (the privacy API needs
	 * rewriteing), purple_privacy_check() is going to be more accurate at
	 * indicating whether a particular buddy is going to be able to message
	 * you, which is the important information that this is trying to convey.
	 */
	if (full && user)
	{
		const char *phone;

		purple_notify_user_info_add_pair_plaintext(user_info, _("Has you"),
									   ((user->list_op & (1 << MSN_LIST_RL)) ? _("Yes") : _("No")));

		purple_notify_user_info_add_pair_plaintext(user_info, _("Blocked"),
									   ((user->list_op & (1 << MSN_LIST_BL)) ? _("Yes") : _("No")));

		phone = msn_user_get_home_phone(user);
		if (phone != NULL) {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Home Phone Number"), phone);
		}

		phone = msn_user_get_work_phone(user);
		if (phone != NULL) {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Work Phone Number"), phone);
		}

		phone = msn_user_get_mobile_phone(user);
		if (phone != NULL) {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Mobile Phone Number"), phone);
		}
	}
}

static GList *
msn_status_types(PurpleAccount *account)
{
	PurpleStatusType *status;
	GList *types = NULL;

	status = purple_status_type_new_with_attrs(
				PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE,
				"message", _("Message"), purple_value_new(G_TYPE_STRING),
				NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "brb", _("Be Right Back"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_UNAVAILABLE, "busy", _("Busy"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_UNAVAILABLE, "phone", _("On the Phone"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "lunch", _("Out to Lunch"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_INVISIBLE,
			NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
			NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_MOBILE,
			"mobile", NULL, FALSE, FALSE, TRUE);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(PURPLE_STATUS_TUNE,
			"tune", NULL, FALSE, TRUE, TRUE,
			PURPLE_TUNE_ARTIST, _("Tune Artist"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_ALBUM, _("Tune Album"), purple_value_new(G_TYPE_STRING),
			PURPLE_TUNE_TITLE, _("Tune Title"), purple_value_new(G_TYPE_STRING),
			"game", _("Game Title"), purple_value_new(G_TYPE_STRING),
			"office", _("Office Title"), purple_value_new(G_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	return types;
}

static GList *
msn_get_actions(PurpleConnection *gc)
{
	MsnSession *session;
	GList *m = NULL;
	PurpleProtocolAction *act;

	session = purple_connection_get_protocol_data(gc);

	act = purple_protocol_action_new(_("Set Friendly Name..."),
								 msn_show_set_friendly_name);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

	if (session->enable_mpop)
	{
		act = purple_protocol_action_new(_("View Locations..."),
		                               msn_show_locations);
		m = g_list_append(m, act);
		m = g_list_append(m, NULL);
	}

	act = purple_protocol_action_new(_("Set Home Phone Number..."),
								 msn_show_set_home_phone);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Set Work Phone Number..."),
			msn_show_set_work_phone);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Set Mobile Phone Number..."),
			msn_show_set_mobile_phone);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

#if 0
	act = purple_protocol_action_new(_("Enable/Disable Mobile Devices..."),
			msn_show_set_mobile_support);
	m = g_list_append(m, act);
#endif

	act = purple_protocol_action_new(_("Allow/Disallow Multiple Logins..."),
			msn_show_set_mpop);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Allow/Disallow Mobile Pages..."),
			msn_show_set_mobile_pages);
	m = g_list_append(m, act);

/* QuLogic: Disabled until confirmed correct. */
#if 0
	m = g_list_append(m, NULL);
	act = purple_protocol_action_new(_("View Blocked Text..."),
			msn_show_blocked_text);
	m = g_list_append(m, act);
#endif

	m = g_list_append(m, NULL);
	act = purple_protocol_action_new(_("Open Hotmail Inbox"),
			msn_show_hotmail_inbox);
	m = g_list_append(m, act);

	return m;
}

static GList *
msn_buddy_menu(PurpleBuddy *buddy)
{
	MsnUser *user;

	GList *m = NULL;
	PurpleMenuAction *act;

	g_return_val_if_fail(buddy != NULL, NULL);

	user = purple_buddy_get_protocol_data(buddy);

	if (user != NULL)
	{
		if (user->mobile)
		{
			act = purple_menu_action_new(_("Send to Mobile"),
			                           PURPLE_CALLBACK(show_send_to_mobile_cb),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}
	}

	if (g_ascii_strcasecmp(purple_buddy_get_name(buddy),
				purple_account_get_username(purple_buddy_get_account(buddy))))
	{
		act = purple_menu_action_new(_("Initiate _Chat"),
		                           PURPLE_CALLBACK(initiate_chat_cb),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *
msn_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_IS_BUDDY(node))
	{
		return msn_buddy_menu((PurpleBuddy *) node);
	}
	else
	{
		return NULL;
	}
}

static void
msn_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	MsnSession *session;
	const char *username;
	const char *host;
	gboolean http_method = FALSE;
	int port;

	gc = purple_account_get_connection(account);

	http_method = purple_account_get_bool(account, "http_method", FALSE);

	if (http_method)
		host = purple_account_get_string(account, "http_method_server", MSN_HTTPCONN_SERVER);
	else
		host = purple_account_get_string(account, "server", MSN_SERVER);
	port = purple_account_get_int(account, "port", MSN_PORT);

	session = msn_session_new(account);

	purple_connection_set_protocol_data(gc, session);
	purple_connection_set_flags(gc,
		PURPLE_CONNECTION_FLAG_HTML |
		PURPLE_CONNECTION_FLAG_FORMATTING_WBFO |
		PURPLE_CONNECTION_FLAG_NO_BGCOLOR |
		PURPLE_CONNECTION_FLAG_NO_FONTSIZE |
		PURPLE_CONNECTION_FLAG_NO_URLDESC |
		PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY |
		PURPLE_CONNECTION_FLAG_NO_IMAGES);

	msn_session_set_login_step(session, MSN_LOGIN_STEP_START);

	/* Hmm, I don't like this. */
	/* XXX shx: Me neither */
	username = msn_normalize(account, purple_account_get_username(account));

	if (strcmp(username, purple_account_get_username(account)))
		purple_account_set_username(account, username);

	username = purple_account_get_string(account, "display-name", NULL);
	purple_connection_set_display_name(gc, username);

	if (purple_account_get_string(account, "endpoint-name", NULL) == NULL) {
		GHashTable *ui_info = purple_core_get_ui_info();
		const gchar *ui_name = ui_info ? g_hash_table_lookup(ui_info, "name") : NULL;
		purple_account_set_string(account, "endpoint-name",
				ui_name && *ui_name ? ui_name : PACKAGE_NAME);
	}

	if (!msn_session_connect(session, host, port, http_method))
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to connect"));
}

static void
msn_close(PurpleConnection *gc)
{
	MsnSession *session;

	session = purple_connection_get_protocol_data(gc);

	g_return_if_fail(session != NULL);

	msn_session_destroy(session);

	purple_connection_set_protocol_data(gc, NULL);
}

static gboolean
msn_send_me_im(gpointer data)
{
	MsnIMData *imdata = data;
	purple_serv_got_im(imdata->gc, imdata->who, imdata->msg, imdata->flags, imdata->when);

	g_object_unref(imdata->gc);
	g_free(imdata->msg);
	g_free(imdata);

	return FALSE;
}

static GString*
msn_msg_emoticon_add(GString *current, MsnEmoticon *emoticon)
{
	MsnObject *obj;
	char *strobj;

	if (emoticon == NULL)
		return current;

	obj = emoticon->obj;

	if (!obj)
		return current;

	strobj = msn_object_to_string(obj);

	if (current)
		g_string_append_printf(current, "\t%s\t%s", emoticon->smile, strobj);
	else {
		current = g_string_new("");
		g_string_printf(current, "%s\t%s", emoticon->smile, strobj);
	}

	g_free(strobj);

	return current;
}

static void
msn_send_emoticons(MsnSwitchBoard *swboard, GString *body)
{
	MsnMessage *msg;

	g_return_if_fail(body != NULL);

	msg = msn_message_new(MSN_MSG_SLP);
	msn_message_set_content_type(msg, "text/x-mms-emoticon");
	msn_message_set_flag(msg, 'N');
	msn_message_set_bin_data(msg, body->str, body->len);

	msn_switchboard_send_msg(swboard, msg, TRUE);
	msn_message_unref(msg);
}

static void msn_emoticon_destroy(MsnEmoticon *emoticon)
{
	if (emoticon->obj)
		msn_object_destroy(emoticon->obj, FALSE);
	g_free(emoticon->smile);
	g_free(emoticon);
}

static GSList* msn_msg_grab_emoticons(const char *msg, const char *username)
{
	GSList *list = NULL;
	GList *smileys, *it;
	MsnEmoticon *emoticon;

	smileys = purple_smiley_parser_find(purple_smiley_custom_get_list(),
		msg, FALSE);

	for (it = smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		PurpleImage *img;

		img = purple_smiley_get_image(smiley);

		emoticon = g_new0(MsnEmoticon, 1);
		emoticon->smile = g_strdup(purple_smiley_get_shortcut(smiley));
		emoticon->ps = smiley;
		/* TODO: we are leaking file location, consider using
		 * purple_image_get_friendly_filename. */
		emoticon->obj = msn_object_new_from_image(img,
			purple_image_get_path(purple_smiley_get_image(smiley)),
			username, MSN_OBJECT_EMOTICON);

		list = g_slist_prepend(list, emoticon);
	}
	g_list_free(smileys);

	return list;
}

void
msn_send_im_message(MsnSession *session, MsnMessage *msg)
{
	MsnEmoticon *smile;
	GSList *smileys;
	GString *emoticons = NULL;
	const char *username = purple_account_get_username(session->account);
	MsnSwitchBoard *swboard = msn_session_get_swboard(session, msg->remote_user, MSN_SB_FLAG_IM);

	smileys = msn_msg_grab_emoticons(msg->body, username);
	while (smileys) {
		smile = (MsnEmoticon *)smileys->data;
		emoticons = msn_msg_emoticon_add(emoticons, smile);
		msn_emoticon_destroy(smile);
		smileys = g_slist_delete_link(smileys, smileys);
	}

	if (emoticons) {
		msn_send_emoticons(swboard, emoticons);
		g_string_free(emoticons, TRUE);
	}

	msn_switchboard_send_msg(swboard, msg, TRUE);
}

static int
msn_send_im(PurpleConnection *gc, PurpleMessage *pmsg)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;
	size_t msglen;
	const char *username;
	const gchar *rcpt = purple_message_get_recipient(pmsg);
	PurpleMessageFlags flags = purple_message_get_flags(pmsg);
	PurpleBuddy *buddy = purple_blist_find_buddy(purple_connection_get_account(gc), rcpt);
	const gchar *cont = purple_message_get_contents(pmsg);

	account = purple_connection_get_account(gc);
	username = purple_account_get_username(account);

	session = purple_connection_get_protocol_data(gc);
	swboard = msn_session_find_swboard(session, rcpt);

	if (!strncmp("tel:+", rcpt, 5)) {
		char *text = purple_markup_strip_html(cont);
		send_to_mobile(gc, rcpt, text);
		g_free(text);
		return 1;
	}

	if (buddy) {
		PurplePresence *p = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
			char *text = purple_markup_strip_html(cont);
			send_to_mobile(gc, rcpt, text);
			g_free(text);
			return 1;
		}
	}

	msn_import_html(cont, &msgformat, &msgtext);
	msglen = strlen(msgtext);
	if (msglen == 0) {
		/* Stuff like <hr> will be ignored. Don't send an empty message
		   if that's all there is. */
		g_free(msgtext);
		g_free(msgformat);

		return 0;
	}

	if (msglen + strlen(msgformat) + strlen(VERSION) > 1564)
	{
		g_free(msgformat);
		g_free(msgtext);

		return -E2BIG;
	}

	msg = msn_message_new_plain(msgtext);
	msg->remote_user = g_strdup(rcpt);
	msn_message_set_header(msg, "X-MMS-IM-Format", msgformat);

	g_free(msgformat);
	g_free(msgtext);

	if (g_ascii_strcasecmp(rcpt, username))
	{
		if (flags & PURPLE_MESSAGE_AUTO_RESP) {
			msn_message_set_flag(msg, 'U');
		}

		if (msn_user_is_yahoo(account, rcpt) || !(msn_user_is_online(account, rcpt) || swboard != NULL)) {
			/*we send the online and offline Message to Yahoo User via UBM*/
			purple_debug_info("msn", "send to offline or Yahoo user\n");
			msn_notification_send_uum(session, msg);
		} else {
			purple_debug_info("msn", "send via switchboard\n");
			msn_send_im_message(session, msg);
		}
	}
	else
	{
		char *body_str, *body_enc, *pre, *post;
		const char *format;
		MsnIMData *imdata = g_new0(MsnIMData, 1);
		/*
		 * In MSN, you can't send messages to yourself, so
		 * we'll fake like we received it ;)
		 */
		body_str = msn_message_to_string(msg);
		body_enc = g_markup_escape_text(body_str, -1);
		g_free(body_str);

		format = msn_message_get_header_value(msg, "X-MMS-IM-Format");
		msn_parse_format(format, &pre, &post);
		body_str = g_strdup_printf("%s%s%s", pre ? pre :  "",
								   body_enc ? body_enc : "", post ? post : "");
		g_free(body_enc);
		g_free(pre);
		g_free(post);

		purple_serv_got_typing_stopped(gc, rcpt);
		imdata->gc = g_object_ref(gc);
		imdata->who = rcpt;
		imdata->msg = body_str;
		imdata->flags = flags & ~PURPLE_MESSAGE_SEND;
		imdata->when = time(NULL);
		purple_timeout_add(0, msn_send_me_im, imdata);
	}

	msn_message_unref(msg);

	return 1;
}

static unsigned int
msn_send_typing(PurpleConnection *gc, const char *who, PurpleIMTypingState state)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;

	account = purple_connection_get_account(gc);
	session = purple_connection_get_protocol_data(gc);

	/*
	 * TODO: I feel like this should be "if (state != PURPLE_IM_TYPING)"
	 *       but this is how it was before, and I don't want to break
	 *       anything. --KingAnt
	 */
	if (state == PURPLE_IM_NOT_TYPING)
		return 0;

	if (!g_ascii_strcasecmp(who, purple_account_get_username(account)))
	{
		/* We'll just fake it, since we're sending to ourself. */
		purple_serv_got_typing(gc, who, MSN_TYPING_RECV_TIMEOUT, PURPLE_IM_TYPING);

		return MSN_TYPING_SEND_TIMEOUT;
	}

	swboard = msn_session_find_swboard(session, who);

	if (swboard == NULL || !msn_switchboard_can_send(swboard))
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msg = msn_message_new(MSN_MSG_TYPING);
	msn_message_set_content_type(msg, "text/x-msmsgscontrol");
	msn_message_set_flag(msg, 'U');
	msn_message_set_header(msg, "TypingUser",
						 purple_account_get_username(account));
	msn_message_set_bin_data(msg, "\r\n", 2);

	msn_switchboard_send_msg(swboard, msg, FALSE);

	msn_message_unref(msg);

	return MSN_TYPING_SEND_TIMEOUT;
}

static void
msn_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = purple_account_get_connection(account);

	if (gc != NULL)
	{
		session = purple_connection_get_protocol_data(gc);
		msn_change_status(session);
	}
}

static void
msn_set_idle(PurpleConnection *gc, int idle)
{
	MsnSession *session;

	session = purple_connection_get_protocol_data(gc);

	msn_change_status(session);
}

/*
 * Actually adds a buddy once we have the response from FQY
 */
static void
add_pending_buddy(MsnSession *session,
                  const char *who,
                  MsnNetwork network,
                  MsnUser *user)
{
	char *group;
	MsnUserList *userlist;
	MsnUser *user2;

	g_return_if_fail(user != NULL);

	if (network == MSN_NETWORK_UNKNOWN) {
		purple_debug_error("msn", "Network in FQY response was unknown.  "
				"Assuming %s is a passport user and adding anyway.\n", who);
		network = MSN_NETWORK_PASSPORT;
	}

	group = msn_user_remove_pending_group(user);

	userlist = session->userlist;
	user2 = msn_userlist_find_user(userlist, who);
	if (user2 != NULL) {
		/* User already in userlist, so just update it. */
		msn_user_unref(user);
		user = user2;
	} else {
		msn_userlist_add_user(userlist, user);
		msn_user_unref(user);
	}

	msn_user_set_network(user, network);
	msn_userlist_add_buddy(userlist, who, group);

	g_free(group);
}

static void
msn_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char *message)
{
	PurpleAccount *account;
	const char *bname, *gname;
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	account = purple_connection_get_account(pc);
	session = purple_connection_get_protocol_data(pc);
	bname = purple_buddy_get_name(buddy);

	if (!session->logged_in)
	{
		purple_debug_error("msn", "msn_add_buddy called before connected\n");

		return;
	}

	/* XXX - Would group ever be NULL here?  I don't think so...
	 * shx: Yes it should; MSN handles non-grouped buddies, and this is only
	 * internal.
	 * KingAnt: But PurpleBuddys must always exist inside PurpleGroups, so
	 * won't group always be non-NULL here?
	 */
	bname = msn_normalize(account, bname);
	gname = group ? purple_group_get_name(group) : NULL;
	purple_debug_info("msn", "Add user:%s to group:%s\n",
	                  bname, gname ? gname : "(null)");

	if (!msn_email_is_valid(bname)) {
		gchar *buf;
		buf = g_strdup_printf(_("Unable to add the buddy %s because the username is invalid.  Usernames must be valid email addresses."), bname);
		if (!purple_conversation_present_error(bname, account, buf)) {
			purple_notify_error(pc, NULL, _("Unable to Add"), buf,
				purple_request_cpar_from_connection(pc));
		}
		g_free(buf);

		/* Remove from local list */
		purple_blist_remove_buddy(buddy);

		return;
	}

	/* Make sure name is normalized */
	purple_buddy_set_name(buddy, bname);

	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, bname);
	if (user && user->authorized) {
		message = NULL;
	}
	if ((user != NULL) && (user->networkid != MSN_NETWORK_UNKNOWN)) {
		/* We already know this buddy and their network. This function knows
		   what to do with users already in the list and stuff... */
		msn_user_set_invite_message(user, message);
		msn_userlist_add_buddy(userlist, bname, gname);
	} else {
		char **tokens;
		char *fqy;
		/* We need to check the network for this buddy first */
		user = msn_user_new(userlist, bname, NULL);
		msn_user_set_invite_message(user, message);
		msn_user_set_pending_group(user, gname);
		msn_user_set_network(user, MSN_NETWORK_UNKNOWN);
		/* Should probably re-use the msn_add_contact_xml function here */
		tokens = g_strsplit(bname, "@", 2);
		fqy = g_strdup_printf("<ml><d n=\"%s\"><c n=\"%s\"/></d></ml>",
		                      tokens[1],
		                      tokens[0]);
		/* TODO: I think user will leak if we disconnect before receiving
		         a response to this FQY request */
		msn_notification_send_fqy(session, fqy, strlen(fqy),
		                          (MsnFqyCb)add_pending_buddy, user);
		g_free(fqy);
		g_strfreev(tokens);
	}
}

static void
msn_rem_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = purple_connection_get_protocol_data(gc);
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	/* XXX - Does buddy->name need to be msn_normalize'd here?  --KingAnt */
	msn_userlist_rem_buddy(userlist, purple_buddy_get_name(buddy));
}

static void
msn_add_permit(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = purple_connection_get_protocol_data(gc);
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_BL_OP) {
		msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_BL);

		/* delete contact from Block list and add it to Allow in the callback */
		msn_del_contact_from_list(session, NULL, who, MSN_LIST_BL);
	} else {
		/* just add the contact to Allow list */
		msn_add_contact_to_list(session, NULL, who, MSN_LIST_AL);
	}


	msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_AL);
}

static void
msn_add_deny(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = purple_connection_get_protocol_data(gc);
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_AL_OP) {
		msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_AL);

		/* delete contact from Allow list and add it to Block in the callback */
		msn_del_contact_from_list(session, NULL, who, MSN_LIST_AL);
	} else {
		/* just add the contact to Block list */
		msn_add_contact_to_list(session, NULL, who, MSN_LIST_BL);
	}

	msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_BL);
}

static void
msn_rem_permit(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = purple_connection_get_protocol_data(gc);
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_AL);

	msn_del_contact_from_list(session, NULL, who, MSN_LIST_AL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_BL);
}

static void
msn_rem_deny(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = purple_connection_get_protocol_data(gc);
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_BL);

	msn_del_contact_from_list(session, NULL, who, MSN_LIST_BL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_AL);
}

static void
msn_set_permit_deny(PurpleConnection *gc)
{
	msn_send_privacy(gc);
}

static void
msn_chat_invite(PurpleConnection *gc, int id, const char *msg,
				const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;

	session = purple_connection_get_protocol_data(gc);

	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
	{
		/* if we have no switchboard, everyone else left the chat already */
		swboard = msn_switchboard_new(session);
		msn_switchboard_request(swboard);
		swboard->chat_id = id;
		swboard->conv = PURPLE_CONVERSATION(purple_conversations_find_chat(gc, id));
	}

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_switchboard_request_add_user(swboard, who);
}

static void
msn_chat_leave(PurpleConnection *gc, int id)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	PurpleConversation *conv;

	session = purple_connection_get_protocol_data(gc);

	swboard = msn_session_find_swboard_with_id(session, id);

	/* if swboard is NULL we were the only person left anyway */
	if (swboard == NULL)
		return;

	conv = swboard->conv;

	msn_switchboard_release(swboard, MSN_SB_FLAG_IM);

	/* If other switchboards managed to associate themselves with this
	 * conv, make sure they know it's gone! */
	if (conv != NULL)
	{
		while ((swboard = msn_session_find_swboard_with_conv(session, conv)) != NULL)
			swboard->conv = NULL;
	}
}

static int
msn_chat_send(PurpleConnection *gc, int id, PurpleMessage *pmsg)
{
	PurpleAccount *account;
	MsnSession *session;
	const char *username;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;
	size_t msglen;

	account = purple_connection_get_account(gc);
	session = purple_connection_get_protocol_data(gc);
	username = purple_account_get_username(account);
	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
		return -EINVAL;

	if (!swboard->ready)
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_import_html(purple_message_get_contents(pmsg), &msgformat, &msgtext);
	msglen = strlen(msgtext);

	if ((msglen == 0) || (msglen + strlen(msgformat) + strlen(VERSION) > 1564))
	{
		g_free(msgformat);
		g_free(msgtext);

		return -E2BIG;
	}

	msg = msn_message_new_plain(msgtext);
	msn_message_set_header(msg, "X-MMS-IM-Format", msgformat);

	msn_switchboard_send_msg(swboard, msg, FALSE);
	msn_message_unref(msg);

	g_free(msgformat);
	g_free(msgtext);

	purple_serv_got_chat_in(gc, id, username, purple_message_get_flags(pmsg),
		purple_message_get_contents(pmsg), time(NULL));

	return 0;
}

static void
msn_keepalive(PurpleConnection *gc)
{
	MsnSession *session;
	MsnTransaction *trans;

	session = purple_connection_get_protocol_data(gc);

	if (!session->http_method)
	{
		MsnCmdProc *cmdproc;

		cmdproc = session->notification->cmdproc;

		trans = msn_transaction_new(cmdproc, "PNG", NULL);
		msn_transaction_set_saveable(trans, FALSE);
		msn_cmdproc_send_trans(cmdproc, trans);
	}
}

static void msn_alias_buddy(PurpleConnection *pc, const char *name, const char *alias)
{
	MsnSession *session;

	session = purple_connection_get_protocol_data(pc);

	msn_update_contact(session, name, MSN_UPDATE_ALIAS, alias);
}

static void
msn_group_buddy(PurpleConnection *gc, const char *who,
				const char *old_group_name, const char *new_group_name)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = purple_connection_get_protocol_data(gc);
	userlist = session->userlist;

	msn_userlist_move_buddy(userlist, who, old_group_name, new_group_name);
}

static void
msn_rename_group(PurpleConnection *gc, const char *old_name,
				 PurpleGroup *group, GList *moved_buddies)
{
	MsnSession *session;
	const char *gname;

	session = purple_connection_get_protocol_data(gc);

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->userlist != NULL);

	gname = purple_group_get_name(group);
	if (msn_userlist_find_group_with_name(session->userlist, old_name) != NULL)
	{
		msn_contact_rename_group(session, old_name, gname);
	}
	else
	{
		/* not found */
		msn_add_group(session, NULL, gname);
	}
}

static void
msn_convo_closed(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	PurpleConversation *conv;

	session = purple_connection_get_protocol_data(gc);

	swboard = msn_session_find_swboard(session, who);

	/*
	 * Don't perform an assertion here. If swboard is NULL, then the
	 * switchboard was either closed by the other party, or the person
	 * is talking to himself.
	 */
	if (swboard == NULL)
		return;

	conv = swboard->conv;

	/* If we release the switchboard here, it may still have messages
	   pending ACK which would result in incorrect unsent message errors.
	   Just let it timeout... This is *so* going to screw with people who
	   use dumb clients that report "User has closed the conversation window" */
	/* msn_switchboard_release(swboard, MSN_SB_FLAG_IM); */
	swboard->conv = NULL;

	/* If other switchboards managed to associate themselves with this
	 * conv, make sure they know it's gone! */
	if (conv != NULL)
	{
		while ((swboard = msn_session_find_swboard_with_conv(session, conv)) != NULL)
			swboard->conv = NULL;
	}
}

static void
msn_set_buddy_icon(PurpleConnection *gc, PurpleImage *img)
{
	MsnSession *session;
	MsnUser *user;

	session = purple_connection_get_protocol_data(gc);
	user = session->user;

	msn_user_set_buddy_icon(user, img);

	msn_change_status(session);
}

static void
msn_remove_group(PurpleConnection *gc, PurpleGroup *group)
{
	MsnSession *session;
	const char *gname;

	session = purple_connection_get_protocol_data(gc);
	gname = purple_group_get_name(group);

	purple_debug_info("msn", "Remove group %s\n", gname);
	/*we can't delete the default group*/
	if(!strcmp(gname, MSN_INDIVIDUALS_GROUP_NAME)||
		!strcmp(gname, MSN_NON_IM_GROUP_NAME))
	{
		purple_debug_info("msn", "This group can't be removed, returning.\n");
		return ;
	}

	msn_del_group(session, gname);
}

/**
 * Extract info text from info_data and add it to user_info
 */
static gboolean
msn_tooltip_extract_info_text(PurpleNotifyUserInfo *user_info, MsnGetInfoData *info_data)
{
	PurpleBuddy *b;

	b = purple_blist_find_buddy(purple_connection_get_account(info_data->gc),
						info_data->name);

	if (b)
	{
		char *tmp;
		const char *alias;

		alias = purple_buddy_get_local_alias(b);
		if (alias && alias[0])
		{
			purple_notify_user_info_add_pair_plaintext(user_info, _("Alias"), alias);
		}

		if ((alias = purple_buddy_get_server_alias(b)) != NULL)
		{
			char *nicktext = g_markup_escape_text(alias, -1);
			tmp = g_strdup_printf("<font sml=\"msn\">%s</font>", nicktext);
			purple_notify_user_info_add_pair_html(user_info, _("Nickname"), tmp);
			g_free(tmp);
			g_free(nicktext);
		}

		/* Add the tooltip information */
		msn_tooltip_text(b, user_info, TRUE);

		return TRUE;
	}

	return FALSE;
}

#if PHOTO_SUPPORT

static char *
msn_get_photo_url(const char *url_text)
{
	char *p, *q;

	if ((p = strstr(url_text, PHOTO_URL)) != NULL)
	{
		p += strlen(PHOTO_URL);
	}
	if (p && (strncmp(p, "http://", strlen("http://")) == 0) && ((q = strchr(p, '"')) != NULL))
			return g_strndup(p, q - p);

	return NULL;
}

static void msn_got_photo(PurpleHttpConnection *http_conn, PurpleHttpResponse *response,
	gpointer _info2_data);

#endif

#if 0
static char *msn_info_date_reformat(const char *field, size_t len)
{
	char *tmp = g_strndup(field, len);
	time_t t = purple_str_to_time(tmp, FALSE, NULL, NULL, NULL);

	g_free(tmp);
	return g_strdup(purple_date_format_short(localtime(&t)));
}
#endif

#define MSN_GOT_INFO_GET_FIELD(a, b) \
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info, \
			"\n" a ":", 0, "\n", 0, "Undisclosed", b, 0, NULL, NULL); \
	if (found) \
		sect_info = TRUE;

#define MSN_GOT_INFO_GET_FIELD_NO_SEARCH(a, b) \
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info, \
			"\n" a ":", 0, "\n", 0, "Undisclosed", b, 0, NULL, msn_info_strip_search_link); \
	if (found) \
		sect_info = TRUE;

static char *
msn_info_strip_search_link(const char *field, size_t len)
{
	const char *c;
	if ((c = strstr(field, " (http://")) == NULL)
		return g_strndup(field, len);
	return g_strndup(field, c - field);
}

static void
msn_got_info(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _info_data)
{
	MsnGetInfoData *info_data = _info_data;
	MsnSession *session;
	PurpleNotifyUserInfo *user_info;
	char *stripped, *p, *q, *tmp;
	char *user_url = NULL;
	gboolean found;
	gboolean has_tooltip_text = FALSE;
	gboolean has_info = FALSE;
	gboolean sect_info = FALSE;
	gboolean has_contact_info = FALSE;
	char *url_buffer;
	int stripped_len;
	const gchar *got_data;
	size_t got_len;
#if PHOTO_SUPPORT
	char *photo_url_text = NULL;
	MsnGetInfoStepTwoData *info2_data = NULL;
#endif

	session = purple_connection_get_protocol_data(info_data->gc);

	user_info = purple_notify_user_info_new();
	has_tooltip_text = msn_tooltip_extract_info_text(user_info, info_data);

	if (!purple_http_response_is_successful(response))
	{
		purple_notify_user_info_add_pair_html(user_info,
				_("Error retrieving profile"), NULL);

		purple_notify_userinfo(info_data->gc, info_data->name, user_info, NULL, NULL);
		purple_notify_user_info_destroy(user_info);

		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	got_data = purple_http_response_get_data(response, &got_len);

	purple_debug_info("msn", "In msn_got_info,url_text:{%s}\n", got_data);

	url_buffer = g_strdup(got_data);

	/* If they have a homepage link, MSN masks it such that we need to
	 * fetch the url out before purple_markup_strip_html() nukes it */
	/* I don't think this works with the new spaces profiles - Stu 3/2/06 */
	if ((p = strstr(url_buffer,
			"Take a look at my </font><A class=viewDesc title=\"")) != NULL)
	{
		p += 50;

		if ((q = strchr(p, '"')) != NULL)
			user_url = g_strndup(p, q - p);
	}

	/*
	 * purple_markup_strip_html() doesn't strip out character entities like &nbsp;
	 * and &#183;
	 */
	while ((p = strstr(url_buffer, "&nbsp;")) != NULL)
	{
		*p = ' '; /* Turn &nbsp;'s into ordinary blanks */
		p += 1;
		memmove(p, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 5] = '\0';
	}

	while ((p = strstr(url_buffer, "&#183;")) != NULL)
	{
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_buffer) - 6] = '\0';
	}

	/* Nuke the nasty \r's that just get in the way */
	purple_str_strip_char(url_buffer, '\r');

	/* MSN always puts in &#39; for apostrophes...replace them */
	while ((p = strstr(url_buffer, "&#39;")) != NULL)
	{
		*p = '\'';
		memmove(p + 1, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 4] = '\0';
	}

	/* Nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = purple_markup_strip_html(url_buffer);
	stripped_len = strlen(stripped);

	purple_debug_misc("msn", "stripped = %p\n", stripped);
	purple_debug_misc("msn", "url_buffer = %p\n", url_buffer);

	/* General section header */
	if (has_tooltip_text)
		purple_notify_user_info_add_section_break(user_info);

	purple_notify_user_info_add_section_header(user_info, _("General"));

	/* Extract their Name and put it in */
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));

	/* General */
	MSN_GOT_INFO_GET_FIELD("Nickname", _("Nickname"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Age", _("Age"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Gender", _("Gender"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Occupation", _("Occupation"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Location", _("Location"));

	/* Extract their Interests and put it in */
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info,
			"\nInterests\t", 0, " (/default.aspx?page=searchresults", 0,
			"Undisclosed", _("Hobbies and Interests") /* _("Interests") */,
			0, NULL, NULL);

	if (found)
		sect_info = TRUE;

	MSN_GOT_INFO_GET_FIELD("More about me", _("A Little About Me"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
		if (has_tooltip_text)
			purple_notify_user_info_remove_last_item(user_info);
	}

	/* Social */
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Social"));

	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Marital status", _("Marital Status"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Interested in", _("Interests"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Pets", _("Pets"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Hometown", _("Hometown"));
	MSN_GOT_INFO_GET_FIELD("Places lived", _("Places Lived"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Fashion", _("Fashion"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Humor", _("Humor"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Music", _("Music"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Favorite quote", _("Favorite Quote"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
		purple_notify_user_info_remove_last_item(user_info);
	}

	/* Contact Info */
	/* Personal */
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Contact Info"));
	purple_notify_user_info_add_section_header(user_info, _("Personal"));

	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Significant other", _("Significant Other"));
	MSN_GOT_INFO_GET_FIELD("Home phone", _("Home Phone"));
	MSN_GOT_INFO_GET_FIELD("Home phone 2", _("Home Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Home address", _("Home Address"));
	MSN_GOT_INFO_GET_FIELD("Personal Mobile", _("Personal Mobile"));
	MSN_GOT_INFO_GET_FIELD("Home fax", _("Home Fax"));
	MSN_GOT_INFO_GET_FIELD("Personal email", _("Personal Email"));
	MSN_GOT_INFO_GET_FIELD("Personal IM", _("Personal IM"));
	MSN_GOT_INFO_GET_FIELD("Birthday", _("Birthday"));
	MSN_GOT_INFO_GET_FIELD("Anniversary", _("Anniversary"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
		has_contact_info = TRUE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

	/* Business */
	purple_notify_user_info_add_section_header(user_info, _("Work"));
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Job title", _("Job Title"));
	MSN_GOT_INFO_GET_FIELD("Company", _("Company"));
	MSN_GOT_INFO_GET_FIELD("Department", _("Department"));
	MSN_GOT_INFO_GET_FIELD("Profession", _("Profession"));
	MSN_GOT_INFO_GET_FIELD("Work phone 1", _("Work Phone"));
	MSN_GOT_INFO_GET_FIELD("Work phone 2", _("Work Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Work address", _("Work Address"));
	MSN_GOT_INFO_GET_FIELD("Work mobile", _("Work Mobile"));
	MSN_GOT_INFO_GET_FIELD("Work pager", _("Work Pager"));
	MSN_GOT_INFO_GET_FIELD("Work fax", _("Work Fax"));
	MSN_GOT_INFO_GET_FIELD("Work email", _("Work Email"));
	MSN_GOT_INFO_GET_FIELD("Work IM", _("Work IM"));
	MSN_GOT_INFO_GET_FIELD("Start date", _("Start Date"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		has_info = TRUE;
		has_contact_info = TRUE;
#if 0
		/* it's true, but we don't need these assignments */
		sect_info = FALSE;
#endif
	}
	else
	{
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

	if (!has_contact_info)
	{
		/* Remove the Contact Info section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

#if 0 /* these probably don't show up any more */
	/*
	 * The fields, 'A Little About Me', 'Favorite Things', 'Hobbies
	 * and Interests', 'Favorite Quote', and 'My Homepage' may or may
	 * not appear, in any combination. However, they do appear in
	 * certain order, so we can successively search to pin down the
	 * distinct values.
	 */

	/* Check if they have A Little About Me */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" A Little About Me \n\n", 0, "Favorite Things", '\n', NULL,
			_("A Little About Me"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Hobbies and Interests", '\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Favorite Quote", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "My Homepage \n\nTake a look",
				'\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "last updated", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Things */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" Favorite Things \n\n", 0, "Hobbies and Interests", '\n', NULL,
			_("Favorite Things"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "Favorite Quote", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "My Homepage \n\nTake a look", '\n',
				NULL, _("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Hobbies and Interests */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" Hobbies and Interests \n\n", 0, "Favorite Quote", '\n', NULL,
			_("Hobbies and Interests"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "My Homepage \n\nTake a look",
				'\n', NULL, _("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "last updated", '\n', NULL,
				_("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Quote */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			"Favorite Quote \n\n", 0, "My Homepage \n\nTake a look", '\n', NULL,
			_("Favorite Quote"), 0, NULL, NULL);

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Quote \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Quote"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Extract the last updated date and put it in */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" last updated:", 1, "\n", 0, NULL, _("Last Updated"), 0,
			NULL, msn_info_date_reformat);

	if (found)
		has_info = TRUE;
#endif

	/* If we were able to fetch a homepage url earlier, stick it in there */
	if (user_url != NULL)
	{
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", user_url, user_url);
		purple_notify_user_info_add_pair_html(user_info, _("Homepage"), tmp);
		g_free(tmp);
		g_free(user_url);

		has_info = TRUE;
	}

	if (!has_info)
	{
		/* MSN doesn't actually distinguish between "unknown member" and
		 * a known member with an empty profile. Try to explain this fact.
		 * Note that if we have a nonempty tooltip_text, we know the user
		 * exists.
		 */
		/* This doesn't work with the new spaces profiles - Stu 3/2/06
		char *p = strstr(url_buffer, "Unknown Member </TITLE>");
		 * This might not work for long either ... */
		/* Nope, it failed some time before 5/2/07 :(
		char *p = strstr(url_buffer, "form id=\"SpacesSearch\" name=\"SpacesSearch\"");
		 * Let's see how long this one holds out for ... */
		char *p = strstr(url_buffer, "<form id=\"profile_form\" name=\"profile_form\" action=\"http&#58;&#47;&#47;spaces.live.com&#47;profile.aspx&#63;cid&#61;0\"");
		PurpleBuddy *b = purple_blist_find_buddy
				(purple_connection_get_account(info_data->gc), info_data->name);
		purple_notify_user_info_add_pair_html(user_info,
				_("Error retrieving profile"), NULL);
		purple_notify_user_info_add_pair_plaintext(user_info, NULL,
				((p && b) ? _("The user has not created a public profile.") :
					(p ? _("MSN reported not being able to find the user's profile. "
							"This either means that the user does not exist, "
							"or that the user exists "
							"but has not created a public profile.") :
						_("Could not find "	/* This should never happen */
							"any information in the user's profile. "
							"The user most likely does not exist."))));
	}

	/* put a link to the actual profile URL */
	purple_notify_user_info_add_section_break(user_info);
	tmp = g_strdup_printf("<a href=\"%s%s\">%s</a>",
			PROFILE_URL, info_data->name, _("View web profile"));
	purple_notify_user_info_add_pair_html(user_info, NULL, tmp);
	g_free(tmp);

#if PHOTO_SUPPORT
	/* Find the URL to the photo; must be before the marshalling [Bug 994207] */
	photo_url_text = msn_get_photo_url(got_data);
	purple_debug_info("msn", "photo url:{%s}\n", photo_url_text ? photo_url_text : "(null)");

	/* Marshall the existing state */
	info2_data = g_new0(MsnGetInfoStepTwoData, 1);
	info2_data->info_data = info_data;
	info2_data->stripped = stripped;
	info2_data->url_buffer = url_buffer;
	info2_data->user_info = user_info;
	info2_data->photo_url_text = photo_url_text;

	/* Try to put the photo in there too, if there's one */
	if (photo_url_text)
	{
		PurpleHttpRequest *req;

		req = purple_http_request_new(photo_url_text);
		purple_http_request_set_max_len(req, MAX_HTTP_BUDDYICON_BYTES);
		purple_http_connection_set_add(session->http_reqs,
			purple_http_request(info_data->gc, req, msn_got_photo,
				info2_data));
		purple_http_request_unref(req);
	}
	else
	{
		/* Finish the Get Info and show the user something */
		msn_got_photo(NULL, NULL, info2_data);
	}
}

static void
msn_got_photo(PurpleHttpConnection *http_conn, PurpleHttpResponse *response,
	gpointer _info2_data)
{
	MsnGetInfoStepTwoData *info2_data = _info2_data;

	/* Unmarshall the saved state */
	MsnGetInfoData *info_data = info2_data->info_data;
	char *stripped = info2_data->stripped;
	char *url_buffer = info2_data->url_buffer;
	PurpleNotifyUserInfo *user_info = info2_data->user_info;
	char *photo_url_text = info2_data->photo_url_text;

	/* Try to put the photo in there too, if there's one and is readable */
	if (response && purple_http_response_is_successful(response))
	{
		PurpleImage *img;
		char buf[1024];
		const gchar *photo_data;
		size_t len;
		guint img_id;

		photo_data = purple_http_response_get_data(response, &len);
		purple_debug_info("msn", "%s is %" G_GSIZE_FORMAT " bytes\n", photo_url_text, len);

		img = purple_image_new_from_data(g_memdup(photo_data, len), len);
		img_id = purple_image_store_add_temporary(img);
		g_object_unref(img);

		g_snprintf(buf, sizeof(buf), "<img id=\""
			PURPLE_IMAGE_STORE_PROTOCOL "%u\"><br>", img_id);
		purple_notify_user_info_prepend_pair_html(user_info, NULL, buf);
	}

	/* We continue here from msn_got_info, as if nothing has happened */
#endif
	purple_notify_userinfo(info_data->gc, info_data->name, user_info, NULL, NULL);

	g_free(stripped);
	g_free(url_buffer);
	purple_notify_user_info_destroy(user_info);
	g_free(info_data->name);
	g_free(info_data);
#if PHOTO_SUPPORT
	g_free(photo_url_text);
	g_free(info2_data);
#endif
}

static void
msn_get_info(PurpleConnection *gc, const char *name)
{
	MsnSession *session = purple_connection_get_protocol_data(gc);
	MsnGetInfoData *data;

	data       = g_new0(MsnGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	purple_http_connection_set_add(session->http_reqs,
		purple_http_get_printf(gc, msn_got_info, data, "%s%s",
			PROFILE_URL, name));
}

static PurpleAccount *find_acct(const char *protocol, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, protocol);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(protocol, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

static gboolean msn_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	char *acct_id = g_hash_table_lookup(params, "account");
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "msnim"))
		return FALSE;

	acct = find_acct("prpl-msn", acct_id);

	if (!acct)
		return FALSE;

	/* msnim:chat?contact=user@domain.tld */
	if (!g_ascii_strcasecmp(cmd, "Chat")) {
		char *sname = g_hash_table_lookup(params, "contact");
		if (sname) {
			PurpleIMConversation *im = purple_conversations_find_im_with_account(
				sname, acct);
			if (im == NULL)
				im = purple_im_conversation_new(acct, sname);
			purple_conversation_present(PURPLE_CONVERSATION(im));
		}
		/*else
			**If pidgindialogs_im() was in the core, we could use it here.
			 * It is all purple_request_* based, but I'm not sure it really belongs in the core
			pidgindialogs_im();*/

		return TRUE;
	}
	/* msnim:add?contact=user@domain.tld */
	else if (!g_ascii_strcasecmp(cmd, "Add")) {
		char *name = g_hash_table_lookup(params, "contact");
		purple_blist_request_add_buddy(acct, name, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}

static gssize
msn_get_max_message_size(PurpleConversation *conv)
{
	/* XXX: pidgin-otr says 1409. Verify and document it. */
	return 1525 - strlen(VERSION);
}

static void
msn_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountOption *option;

	protocol->id        = "prpl-msn";
	protocol->name      = "MSN";
	protocol->options   = OPT_PROTO_MAIL_CHECK | OPT_PROTO_INVITE_MESSAGE;
	protocol->icon_spec = purple_buddy_icon_spec_new("png,gif",
	                                                    0, 0, 96, 96, 0,
	                                                    PURPLE_ICON_SCALE_SEND);

	option = purple_account_option_string_new(_("Server"), "server",
											MSN_SERVER);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);

	option = purple_account_option_int_new(_("Port"), "port", MSN_PORT);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);

	option = purple_account_option_bool_new(_("Use HTTP Method"),
										  "http_method", FALSE);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);

	option = purple_account_option_string_new(_("HTTP Method Server"),
										  "http_method_server", MSN_HTTPCONN_SERVER);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);

	option = purple_account_option_bool_new(_("Show custom smileys"),
										  "custom_smileys", TRUE);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);

	option = purple_account_option_bool_new(_("Allow direct connections"),
										  "direct_connect", TRUE);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);

	option = purple_account_option_bool_new(_("Allow connecting from multiple locations"),
										  "mpop", TRUE);
	protocol->account_options = g_list_append(protocol->account_options,
											   option);
}

static void
msn_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->login        = msn_login;
	klass->close        = msn_close;
	klass->status_types = msn_status_types;
	klass->list_icon    = msn_list_icon;
}

static void
msn_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_actions            = msn_get_actions;
	client_iface->list_emblem            = msn_list_emblems;
	client_iface->status_text            = msn_status_text;
	client_iface->tooltip_text           = msn_tooltip_text;
	client_iface->blist_node_menu        = msn_blist_node_menu;
	client_iface->convo_closed           = msn_convo_closed;
	client_iface->normalize              = msn_normalize;
	client_iface->offline_message        = msn_offline_message;
	client_iface->get_account_text_table = msn_get_account_text_table;
	client_iface->get_max_message_size   = msn_get_max_message_size;
}

static void
msn_protocol_server_iface_init(PurpleProtocolServerIface *server_iface)
{
	server_iface->get_info         = msn_get_info;
	server_iface->set_status       = msn_set_status;
	server_iface->set_idle         = msn_set_idle;
	server_iface->add_buddy        = msn_add_buddy;
	server_iface->remove_buddy     = msn_rem_buddy;
	server_iface->keepalive        = msn_keepalive;
	server_iface->alias_buddy      = msn_alias_buddy;
	server_iface->group_buddy      = msn_group_buddy;
	server_iface->rename_group     = msn_rename_group;
	server_iface->set_buddy_icon   = msn_set_buddy_icon;
	server_iface->remove_group     = msn_remove_group;
	server_iface->set_public_alias = msn_set_public_alias;
	server_iface->get_public_alias = msn_get_public_alias;
}

static void
msn_protocol_im_iface_init(PurpleProtocolIMIface *im_iface)
{
	im_iface->send        = msn_send_im;
	im_iface->send_typing = msn_send_typing;
}

static void
msn_protocol_chat_iface_init(PurpleProtocolChatIface *chat_iface)
{
	chat_iface->invite = msn_chat_invite;
	chat_iface->leave  = msn_chat_leave;
	chat_iface->send   = msn_chat_send;
}

static void
msn_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *privacy_iface)
{
	privacy_iface->add_permit      = msn_add_permit;
	privacy_iface->add_deny        = msn_add_deny;
	privacy_iface->rem_permit      = msn_rem_permit;
	privacy_iface->rem_deny        = msn_rem_deny;
	privacy_iface->set_permit_deny = msn_set_permit_deny;
}

static void
msn_protocol_attention_iface_init(PurpleProtocolAttentionIface *attention_iface)
{
	attention_iface->send      = msn_send_attention;
	attention_iface->get_types = msn_attention_types;
}

static void
msn_protocol_xfer_iface_init(PurpleProtocolXferIface *xfer_iface)
{
	xfer_iface->can_receive = msn_can_receive_file;
	xfer_iface->send        = msn_send_file;
	xfer_iface->new_xfer    = msn_new_xfer;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	MsnProtocol, msn_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  msn_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  msn_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  msn_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  msn_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  msn_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ATTENTION_IFACE,
	                                  msn_protocol_attention_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                  msn_protocol_xfer_iface_init)
);

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "prpl-msn",
		"name",         "MSN Protocol",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("Windows Live Messenger Protocol Plugin"),
		"description",  N_("Windows Live Messenger Protocol Plugin"),
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
	PurpleCmdId id;

	msn_protocol_register_type(plugin);

	my_protocol = purple_protocols_add(MSN_TYPE_PROTOCOL, error);
	if (!my_protocol)
		return FALSE;

	id = purple_cmd_register("nudge", "", PURPLE_CMD_P_PROTOCOL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
	                 "prpl-msn", msn_cmd_nudge,
	                  _("nudge: nudge a user to get their attention"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	purple_prefs_remove("/plugins/prpl/msn");

	msn_notification_init();
	msn_switchboard_init();

	purple_signal_connect(purple_get_core(), "uri-handler", my_protocol,
		PURPLE_CALLBACK(msn_uri_handler), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	msn_notification_end();
	msn_switchboard_end();

	while (cmds) {
		PurpleCmdId id = GPOINTER_TO_UINT(cmds->data);
		purple_cmd_unregister(id);
		cmds = g_slist_delete_link(cmds, cmds);
	}

	if (!purple_protocols_remove(my_protocol, error))
		return FALSE;

	return TRUE;
}

PURPLE_PLUGIN_INIT(msn, plugin_query, plugin_load, plugin_unload);
