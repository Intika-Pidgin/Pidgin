/**
 * @file msn.c The MSN protocol plugin
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define PHOTO_SUPPORT 1

#include <glib.h>

#include "msn.h"
#include "accountopt.h"
#include "msg.h"
#include "page.h"
#include "pluginpref.h"
#include "prefs.h"
#include "session.h"
#include "state.h"
#include "util.h"
#include "cmds.h"
#include "prpl.h"
#include "msn-utils.h"
#include "version.h"

#include "switchboard.h"
#include "notification.h"
#include "sync.h"
#include "slplink.h"

#if PHOTO_SUPPORT
#include "imgstore.h"
#endif

typedef struct
{
	GaimConnection *gc;
	const char *passport;

} MsnMobileData;

typedef struct
{
	GaimConnection *gc;
	char *name;

} MsnGetInfoData;

typedef struct
{
	MsnGetInfoData *info_data;
	char *stripped;
	char *url_buffer;
	GString *s;
	char *photo_url_text;
	char *tooltip_text;
	const char *title;

} MsnGetInfoStepTwoData;

static const char *
msn_normalize(const GaimAccount *account, const char *str)
{
	static char buf[BUF_LEN];
	char *tmp;

	g_return_val_if_fail(str != NULL, NULL);

	g_snprintf(buf, sizeof(buf), "%s%s", str,
			   (strchr(str, '@') ? "" : "@hotmail.com"));

	tmp = g_utf8_strdown(buf, -1);
	strncpy(buf, tmp, sizeof(buf));
	g_free(tmp);

	return buf;
}

static GaimCmdRet
msn_cmd_nudge(GaimConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	GaimAccount *account = gaim_conversation_get_account(conv);
	GaimConnection *gc = gaim_account_get_connection(account);
	MsnMessage *msg;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	msg = msn_message_new_nudge();
	session = gc->proto_data;
	swboard = msn_session_get_swboard(session, gaim_conversation_get_name(conv), MSN_SB_FLAG_IM);

	if (swboard == NULL)
		return GAIM_CMD_RET_FAILED;

	msn_switchboard_send_msg(swboard, msg, TRUE);

	gaim_conversation_write(conv, NULL, _("You have just sent a Nudge!"), GAIM_MESSAGE_SYSTEM, time(NULL));

	return GAIM_CMD_RET_OK;
}

static void
msn_act_id(GaimConnection *gc, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	GaimAccount *account;
	const char *alias;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;
	account = gaim_connection_get_account(gc);

	if(entry && strlen(entry))
		alias = gaim_url_encode(entry);
	else
		alias = "";

	if (strlen(alias) > BUDDY_ALIAS_MAXLEN)
	{
		gaim_notify_error(gc, NULL,
						  _("Your new MSN friendly name is too long."), NULL);
		return;
	}

	msn_cmdproc_send(cmdproc, "REA", "%s %s",
					 gaim_account_get_username(account),
					 alias);
}

static void
msn_set_prp(GaimConnection *gc, const char *type, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	if (entry == NULL || *entry == '\0')
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s", type);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s %s", type,
						 gaim_url_encode(entry));
	}
}

static void
msn_set_home_phone_cb(GaimConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHH", entry);
}

static void
msn_set_work_phone_cb(GaimConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHW", entry);
}

static void
msn_set_mobile_phone_cb(GaimConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHM", entry);
}

static void
enable_msn_pages_cb(GaimConnection *gc)
{
	msn_set_prp(gc, "MOB", "Y");
}

static void
disable_msn_pages_cb(GaimConnection *gc)
{
	msn_set_prp(gc, "MOB", "N");
}

static void
send_to_mobile(GaimConnection *gc, const char *who, const char *entry)
{
	MsnTransaction *trans;
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnPage *page;
	char *payload;
	size_t payload_len;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	page = msn_page_new();
	msn_page_set_body(page, entry);

	payload = msn_page_gen_payload(page, &payload_len);

	trans = msn_transaction_new(cmdproc, "PGD", "%s 1 %d", who, payload_len);

	msn_transaction_set_payload(trans, payload, payload_len);

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
msn_show_set_friendly_name(GaimPluginAction *action)
{
	GaimConnection *gc;

	gc = (GaimConnection *) action->context;

	gaim_request_input(gc, NULL, _("Set your friendly name."),
					   _("This is the name that other MSN buddies will "
						 "see you as."),
					   gaim_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_act_id),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_home_phone(GaimPluginAction *action)
{
	GaimConnection *gc;
	MsnSession *session;

	gc = (GaimConnection *) action->context;
	session = gc->proto_data;

	gaim_request_input(gc, NULL, _("Set your home phone number."), NULL,
					   msn_user_get_home_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_home_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_work_phone(GaimPluginAction *action)
{
	GaimConnection *gc;
	MsnSession *session;

	gc = (GaimConnection *) action->context;
	session = gc->proto_data;

	gaim_request_input(gc, NULL, _("Set your work phone number."), NULL,
					   msn_user_get_work_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_work_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_mobile_phone(GaimPluginAction *action)
{
	GaimConnection *gc;
	MsnSession *session;

	gc = (GaimConnection *) action->context;
	session = gc->proto_data;

	gaim_request_input(gc, NULL, _("Set your mobile phone number."), NULL,
					   msn_user_get_mobile_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_mobile_phone_cb),
					   _("Cancel"), NULL, gc);
}

static void
msn_show_set_mobile_pages(GaimPluginAction *action)
{
	GaimConnection *gc;

	gc = (GaimConnection *) action->context;

	gaim_request_action(gc, NULL, _("Allow MSN Mobile pages?"),
						_("Do you want to allow or disallow people on "
						  "your buddy list to send you MSN Mobile pages "
						  "to your cell phone or other mobile device?"),
						-1, gc, 3,
						_("Allow"), G_CALLBACK(enable_msn_pages_cb),
						_("Disallow"), G_CALLBACK(disable_msn_pages_cb),
						_("Cancel"), NULL);
}

static void
msn_show_hotmail_inbox(GaimPluginAction *action)
{
	GaimConnection *gc;
	MsnSession *session;

	gc = (GaimConnection *) action->context;
	session = gc->proto_data;

	if (session->passport_info.file == NULL)
	{
		gaim_notify_error(gc, NULL,
						  _("This Hotmail account may not be active."), NULL);
		return;
	}

	gaim_notify_uri(gc, session->passport_info.file);
}

static void
show_send_to_mobile_cb(GaimBlistNode *node, gpointer ignored)
{
	GaimBuddy *buddy;
	GaimConnection *gc;
	MsnSession *session;
	MsnMobileData *data;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	session = gc->proto_data;

	data = g_new0(MsnMobileData, 1);
	data->gc = gc;
	data->passport = buddy->name;

	gaim_request_input(gc, NULL, _("Send a mobile message."), NULL,
					   NULL, TRUE, FALSE, NULL,
					   _("Page"), G_CALLBACK(send_to_mobile_cb),
					   _("Close"), G_CALLBACK(close_mobile_page_cb),
					   data);
}

static void
initiate_chat_cb(GaimBlistNode *node, gpointer data)
{
	GaimBuddy *buddy;
	GaimConnection *gc;

	MsnSession *session;
	MsnSwitchBoard *swboard;

	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY(node));

	buddy = (GaimBuddy *) node;
	gc = gaim_account_get_connection(buddy->account);

	session = gc->proto_data;

	swboard = msn_switchboard_new(session);
	msn_switchboard_request(swboard);
	msn_switchboard_request_add_user(swboard, buddy->name);

	/* TODO: This might move somewhere else, after USR might be */
	swboard->chat_id = session->conv_seq++;
	swboard->conv = serv_got_joined_chat(gc, swboard->chat_id, "MSN Chat");
	swboard->flag = MSN_SB_FLAG_IM;

	gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->conv),
							gaim_account_get_username(buddy->account), NULL, GAIM_CBFLAGS_NONE, TRUE);
}

static void
t_msn_xfer_init(GaimXfer *xfer)
{
	MsnSlpLink *slplink;
	const char *filename;
	FILE *fp;

	filename = gaim_xfer_get_local_filename(xfer);

	slplink = xfer->data;

	if ((fp = g_fopen(filename, "rb")) == NULL)
	{
		GaimAccount *account;
		const char *who;
		char *msg;

		account = slplink->session->account;
		who = slplink->remote_user;

		msg = g_strdup_printf(_("Error reading %s: \n%s.\n"),
							  filename, strerror(errno));
		gaim_xfer_error(gaim_xfer_get_type(xfer), account, xfer->who, msg);
		gaim_xfer_cancel_local(xfer);
		g_free(msg);

		return;
	}
	fclose(fp);

	msn_slplink_request_ft(slplink, xfer);
}

static GaimXfer*
msn_new_xfer(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnSlpLink *slplink;
	GaimXfer *xfer;

	session = gc->proto_data;

	xfer = gaim_xfer_new(gc->account, GAIM_XFER_SEND, who);

	slplink = msn_session_get_slplink(session, who);

	xfer->data = slplink;

	gaim_xfer_set_init_fnc(xfer, t_msn_xfer_init);

	return xfer;
}

static void
msn_send_file(GaimConnection *gc, const char *who, const char *file)
{
	GaimXfer *xfer = msn_new_xfer(gc, who);

	if (file)
		gaim_xfer_request_accepted(xfer, file);
	else
		gaim_xfer_request(xfer);
}

static gboolean
msn_can_receive_file(GaimConnection *gc, const char *who)
{
	GaimAccount *account;
	char *normal;
	gboolean ret;

	account = gaim_connection_get_account(gc);

	normal = g_strdup(msn_normalize(account, gaim_account_get_username(account)));

	ret = strcmp(normal, msn_normalize(account, who));

	g_free(normal);

	return ret;
}

/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
msn_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "msn";
}

static void
msn_list_emblems(GaimBuddy *b, const char **se, const char **sw,
				 const char **nw, const char **ne)
{
	MsnUser *user;
	GaimPresence *presence;
	const char *emblems[4] = { NULL, NULL, NULL, NULL };
	int i = 0;

	user = b->proto_data;
	presence = gaim_buddy_get_presence(b);

	if (!gaim_presence_is_online(presence))
		emblems[i++] = "offline";
	else if (gaim_presence_is_status_active(presence, "busy") ||
			 gaim_presence_is_status_active(presence, "phone"))
		emblems[i++] = "occupied";
	else if (!gaim_presence_is_available(presence))
		emblems[i++] = "away";

	if (user == NULL)
	{
		emblems[0] = "offline";
	}
	else
	{
		if (user->mobile)
			emblems[i++] = "wireless";
		if (!(user->list_op & (1 << MSN_LIST_RL)))
			emblems[i++] = "nr";
	}

	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static char *
msn_status_text(GaimBuddy *buddy)
{
	GaimPresence *presence;
	GaimStatus *status;

	presence = gaim_buddy_get_presence(buddy);
	status = gaim_presence_get_active_status(presence);

	if (!gaim_presence_is_available(presence) && !gaim_presence_is_idle(presence))
	{
		return g_strdup(gaim_status_get_name(status));
	}

	return NULL;
}

static void
msn_tooltip_text(GaimBuddy *buddy, GString *str, gboolean full)
{
	MsnUser *user;
	GaimPresence *presence = gaim_buddy_get_presence(buddy);
	GaimStatus *status = gaim_presence_get_active_status(presence);

	user = buddy->proto_data;

	if (gaim_presence_is_online(presence))
	{
		g_string_append_printf(str, _("\n<b>%s:</b> %s"), _("Status"),
							   gaim_presence_is_idle(presence) ?
							   _("Idle") : gaim_status_get_name(status));
	}

	if (full && user)
	{
		g_string_append_printf(str, _("\n<b>%s:</b> %s"), _("Has you"),
							   (user->list_op & (1 << MSN_LIST_RL)) ?
							   _("Yes") : _("No"));

	/* XXX: This is being shown in non-full tooltips because the
	 * XXX: blocked icon overlay isn't always accurate for MSN.
	 * XXX: This can die as soon as gaim_privacy_check() knows that
	 * XXX: this prpl always honors both the allow and deny lists. */
	if (user)
		g_string_append_printf(str, _("\n<b>%s:</b> %s"), _("Blocked"),
							   (user->list_op & (1 << MSN_LIST_BL)) ?
							   _("Yes") : _("No"));
	}

	gaim_debug_info("MaYuan","tooltip string:{%s}\n",str);
}

static GList *
msn_status_types(GaimAccount *account)
{
	GaimStatusType *status;
	GList *types = NULL;

	status = gaim_status_type_new_full(GAIM_STATUS_AVAILABLE,
			NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_AWAY,
			NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_AWAY,
			"brb", _("Be Right Back"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_UNAVAILABLE,
			"busy", _("Busy"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_UNAVAILABLE,
			"phone", _("On the Phone"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_AWAY,
			"lunch", _("Out to Lunch"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_INVISIBLE,
			NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = gaim_status_type_new_full(GAIM_STATUS_OFFLINE,
			NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	return types;
}

static GList *
msn_actions(GaimPlugin *plugin, gpointer context)
{
	GaimConnection *gc = (GaimConnection *)context;
	GaimAccount *account;
	const char *user;

	GList *m = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Set Friendly Name..."),
								 msn_show_set_friendly_name);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

	act = gaim_plugin_action_new(_("Set Home Phone Number..."),
								 msn_show_set_home_phone);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Set Work Phone Number..."),
			msn_show_set_work_phone);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Set Mobile Phone Number..."),
			msn_show_set_mobile_phone);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

#if 0
	act = gaim_plugin_action_new(_("Enable/Disable Mobile Devices..."),
			msn_show_set_mobile_support);
	m = g_list_append(m, act);
#endif

	act = gaim_plugin_action_new(_("Allow/Disallow Mobile Pages..."),
			msn_show_set_mobile_pages);
	m = g_list_append(m, act);

	account = gaim_connection_get_account(gc);
	user = msn_normalize(account, gaim_account_get_username(account));

	if (strstr(user, "@hotmail.com") != NULL)
	{
		m = g_list_append(m, NULL);
		act = gaim_plugin_action_new(_("Open Hotmail Inbox"),
				msn_show_hotmail_inbox);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *
msn_buddy_menu(GaimBuddy *buddy)
{
	MsnUser *user;

	GList *m = NULL;
	GaimMenuAction *act;

	g_return_val_if_fail(buddy != NULL, NULL);

	user = buddy->proto_data;

	if (user != NULL)
	{
		if (user->mobile)
		{
			act = gaim_menu_action_new(_("Send to Mobile"),
			                           GAIM_CALLBACK(show_send_to_mobile_cb),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}
	}

	if (g_ascii_strcasecmp(buddy->name,
	                       gaim_account_get_username(buddy->account)))
	{
		act = gaim_menu_action_new(_("Initiate _Chat"),
		                           GAIM_CALLBACK(initiate_chat_cb),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *
msn_blist_node_menu(GaimBlistNode *node)
{
	if(GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		return msn_buddy_menu((GaimBuddy *) node);
	}
	else
	{
		return NULL;
	}
}

static void
msn_login(GaimAccount *account)
{
	GaimConnection *gc;
	MsnSession *session;
	const char *username;
	const char *host;
	gboolean http_method = FALSE;
	int port;

	gc = gaim_account_get_connection(account);

	if (!gaim_ssl_is_supported())
	{
		gc->wants_to_die = TRUE;
		gaim_connection_error(gc,
			_("SSL support is needed for MSN. Please install a supported "
			  "SSL library. See http://gaim.sf.net/faq-ssl.php for more "
			  "information."));

		return;
	}

	if (gaim_account_get_bool(account, "http_method", FALSE))
		http_method = TRUE;

	host = gaim_account_get_string(account, "server", MSN_SERVER);
	port = gaim_account_get_int(account, "port", MSN_PORT);

	session = msn_session_new(account);

	gc->proto_data = session;
	gc->flags |= GAIM_CONNECTION_HTML | GAIM_CONNECTION_FORMATTING_WBFO | GAIM_CONNECTION_NO_BGCOLOR | GAIM_CONNECTION_NO_FONTSIZE | GAIM_CONNECTION_NO_URLDESC;

	msn_session_set_login_step(session, MSN_LOGIN_STEP_START);

	/* Hmm, I don't like this. */
	/* XXX shx: Me neither */
	username = msn_normalize(account, gaim_account_get_username(account));

	if (strcmp(username, gaim_account_get_username(account)))
		gaim_account_set_username(account, username);

	if (!msn_session_connect(session, host, port, http_method))
		gaim_connection_error(gc, _("Failed to connect to server."));
}

static void
msn_close(GaimConnection *gc)
{
	MsnSession *session;

	session = gc->proto_data;

	g_return_if_fail(session != NULL);

	msn_session_destroy(session);

	gc->proto_data = NULL;
}

static int
msn_send_im(GaimConnection *gc, const char *who, const char *message,
			GaimMessageFlags flags)
{
	GaimAccount *account;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;

	gaim_debug_info("MaYuan","send IM {%s}\n",message);
	account = gaim_connection_get_account(gc);

	msn_import_html(message, &msgformat, &msgtext);

	if (strlen(msgtext) + strlen(msgformat) + strlen(VERSION) > 1564)
	{
		g_free(msgformat);
		g_free(msgtext);

		return -E2BIG;
	}

	msg = msn_message_new_plain(msgtext);
	msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);

	g_free(msgformat);
	g_free(msgtext);

	if (g_ascii_strcasecmp(who, gaim_account_get_username(account)))
	{
		MsnSession *session;
		MsnSwitchBoard *swboard;

		session = gc->proto_data;
		swboard = msn_session_get_swboard(session, who, MSN_SB_FLAG_IM);

		msn_switchboard_send_msg(swboard, msg, TRUE);
	}
	else
	{
		char *body_str, *body_enc, *pre, *post;
		const char *format;
		/*
		 * In MSN, you can't send messages to yourself, so
		 * we'll fake like we received it ;)
		 */
		body_str = msn_message_to_string(msg);
		body_enc = g_markup_escape_text(body_str, -1);
		g_free(body_str);

		format = msn_message_get_attr(msg, "X-MMS-IM-Format");
		msn_parse_format(format, &pre, &post);
		body_str = g_strdup_printf("%s%s%s", pre ? pre :  "",
								   body_enc ? body_enc : "", post ? post : "");
		g_free(body_enc);
		g_free(pre);
		g_free(post);

		serv_got_typing_stopped(gc, who);
		serv_got_im(gc, who, body_str, flags, time(NULL));
		g_free(body_str);
	}

	msn_message_destroy(msg);

	return 1;
}

static unsigned int
msn_send_typing(GaimConnection *gc, const char *who, GaimTypingState state)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;

	/*
	 * TODO: I feel like this should be "if (state != GAIM_TYPING)"
	 *       but this is how it was before, and I don't want to break
	 *       anything. --KingAnt
	 */
	if (state == GAIM_NOT_TYPING)
		return 0;

	if (!g_ascii_strcasecmp(who, gaim_account_get_username(account)))
	{
		/* We'll just fake it, since we're sending to ourself. */
		serv_got_typing(gc, who, MSN_TYPING_RECV_TIMEOUT, GAIM_TYPING);

		return MSN_TYPING_SEND_TIMEOUT;
	}

	swboard = msn_session_find_swboard(session, who);

	if (swboard == NULL || !msn_switchboard_can_send(swboard))
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msg = msn_message_new(MSN_MSG_TYPING);
	msn_message_set_content_type(msg, "text/x-msmsgscontrol");
	msn_message_set_flag(msg, 'U');
	msn_message_set_attr(msg, "TypingUser",
						 gaim_account_get_username(account));
	msn_message_set_bin_data(msg, "\r\n", 2);

	msn_switchboard_send_msg(swboard, msg, FALSE);

	msn_message_destroy(msg);

	return MSN_TYPING_SEND_TIMEOUT;
}

static void
msn_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc;
	MsnSession *session;

	gc = gaim_account_get_connection(account);

	if (gc != NULL){
		session = gc->proto_data;
		msn_change_status(session);
	}
}

static void
msn_set_idle(GaimConnection *gc, int idle)
{
	MsnSession *session;

	session = gc->proto_data;

	msn_change_status(session);
}

#if 0
static void
fake_userlist_add_buddy(MsnUserList *userlist,
					   const char *who, int list_id,
					   const char *group_name)
{
	MsnUser *user;
	static int group_id_c = 1;
	int group_id;

	group_id = -1;

	if (group_name != NULL)
	{
		MsnGroup *group;
		group = msn_group_new(userlist, group_id_c, group_name);
		group_id = group_id_c++;
	}

	user = msn_userlist_find_user(userlist, who);

	if (user == NULL)
	{
		user = msn_user_new(userlist, who, NULL);
		msn_userlist_add_user(userlist, user);
	}
	else
		if (user->list_op & (1 << list_id))
		{
			if (list_id == MSN_LIST_FL)
			{
				if (group_id >= 0)
					if (g_list_find(user->group_ids,
									GINT_TO_POINTER(group_id)))
						return;
			}
			else
				return;
		}

	if (group_id >= 0)
	{
		user->group_ids = g_list_append(user->group_ids,
										GINT_TO_POINTER(group_id));
	}

	user->list_op |= (1 << list_id);
}
#endif

static void
msn_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	MsnSession *session;
	MsnUserList *userlist;
	const char *who;

	session = gc->proto_data;
	userlist = session->userlist;
	who = msn_normalize(gc->account, buddy->name);

	if (!session->logged_in)
	{
#if 0
		fake_userlist_add_buddy(session->sync_userlist, who, MSN_LIST_FL,
								group ? group->name : NULL);
#else
		gaim_debug_error("msn", "msn_add_buddy called before connected\n");
#endif

		return;
	}

#if 0
	if (group != NULL && group->name != NULL)
		gaim_debug_info("msn", "msn_add_buddy: %s, %s\n", who, group->name);
	else
		gaim_debug_info("msn", "msn_add_buddy: %s\n", who);
#endif

#if 0
	/* Which is the max? */
	if (session->fl_users_count >= 150)
	{
		gaim_debug_info("msn", "Too many buddies\n");
		/* Buddy list full */
		/* TODO: gaim should be notified of this */
		return;
	}
#endif

	/* XXX - Would group ever be NULL here?  I don't think so...
	 * shx: Yes it should; MSN handles non-grouped buddies, and this is only
	 * internal. */
	msn_userlist_add_buddy(userlist, who, MSN_LIST_FL,
						   group ? group->name : NULL);
}

static void
msn_rem_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	/* XXX - Does buddy->name need to be msn_normalize'd here?  --KingAnt */
	msn_userlist_rem_buddy(userlist, buddy->name, MSN_LIST_FL, group->name);
}

static void
msn_add_permit(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_BL_OP)
		msn_userlist_rem_buddy(userlist, who, MSN_LIST_BL, NULL);

	msn_userlist_add_buddy(userlist, who, MSN_LIST_AL, NULL);
}

static void
msn_add_deny(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_AL_OP)
		msn_userlist_rem_buddy(userlist, who, MSN_LIST_AL, NULL);

	msn_userlist_add_buddy(userlist, who, MSN_LIST_BL, NULL);
}

static void
msn_rem_permit(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy(userlist, who, MSN_LIST_AL, NULL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy(userlist, who, MSN_LIST_BL, NULL);
}

static void
msn_rem_deny(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy(userlist, who, MSN_LIST_BL, NULL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy(userlist, who, MSN_LIST_AL, NULL);
}

static void
msn_set_permit_deny(GaimConnection *gc)
{
	GaimAccount *account;
	MsnSession *session;
	MsnCmdProc *cmdproc;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	if (account->perm_deny == GAIM_PRIVACY_ALLOW_ALL ||
		account->perm_deny == GAIM_PRIVACY_DENY_USERS){
		msn_cmdproc_send(cmdproc, "BLP", "%s", "AL");
	}else{
		msn_cmdproc_send(cmdproc, "BLP", "%s", "BL");
	}
}

static void
msn_chat_invite(GaimConnection *gc, int id, const char *msg,
				const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;

	session = gc->proto_data;

	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
	{
		/* if we have no switchboard, everyone else left the chat already */
		swboard = msn_switchboard_new(session);
		msn_switchboard_request(swboard);
		swboard->chat_id = id;
		swboard->conv = gaim_find_chat(gc, id);
	}

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_switchboard_request_add_user(swboard, who);
}

static void
msn_chat_leave(GaimConnection *gc, int id)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	GaimConversation *conv;

	session = gc->proto_data;

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
msn_chat_send(GaimConnection *gc, int id, const char *message, GaimMessageFlags flags)
{
	GaimAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;

	account = gaim_connection_get_account(gc);
	session = gc->proto_data;
	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
		return -EINVAL;

	if (!swboard->ready)
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_import_html(message, &msgformat, &msgtext);

	if (strlen(msgtext) + strlen(msgformat) + strlen(VERSION) > 1564)
	{
		g_free(msgformat);
		g_free(msgtext);

		return -E2BIG;
	}

	msg = msn_message_new_plain(msgtext);
	msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);
	msn_switchboard_send_msg(swboard, msg, FALSE);
	msn_message_destroy(msg);

	g_free(msgformat);
	g_free(msgtext);

	serv_got_chat_in(gc, id, gaim_account_get_username(account), 0,
					 message, time(NULL));

	return 0;
}

static void
msn_keepalive(GaimConnection *gc)
{
	MsnSession *session;

	session = gc->proto_data;

	if (!session->http_method)
	{
		MsnCmdProc *cmdproc;

		cmdproc = session->notification->cmdproc;

		msn_cmdproc_send_quick(cmdproc, "PNG", NULL, NULL);
	}
}

static void
msn_group_buddy(GaimConnection *gc, const char *who,
				const char *old_group_name, const char *new_group_name)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = gc->proto_data;
	userlist = session->userlist;

	msn_userlist_move_buddy(userlist, who, old_group_name, new_group_name);
}

static void
msn_rename_group(GaimConnection *gc, const char *old_name,
				 GaimGroup *group, GList *moved_buddies)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	const char *old_gid;
	const char *enc_new_group_name;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;
	enc_new_group_name = gaim_url_encode(group->name);

	gaim_debug_info("MaYuan","rename group:old{%s},new{%s}",old_name,enc_new_group_name);
	old_gid = msn_userlist_find_group_id(session->userlist, old_name);

	if (old_gid != NULL){
		/*find a Group*/
		msn_cmdproc_send(cmdproc, "REG", "%d %s 0", old_gid,
						 enc_new_group_name);
	}else{
		/*not found*/
		msn_cmdproc_send(cmdproc, "ADG", "%s 0", enc_new_group_name);
	}
}

static void
msn_convo_closed(GaimConnection *gc, const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	GaimConversation *conv;

	session = gc->proto_data;

	swboard = msn_session_find_swboard(session, who);

	/*
	 * Don't perform an assertion here. If swboard is NULL, then the
	 * switchboard was either closed by the other party, or the person
	 * is talking to himself.
	 */
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

static void
msn_set_buddy_icon(GaimConnection *gc, const char *filename)
{
	MsnSession *session;
	MsnUser *user;

	session = gc->proto_data;
	user = session->user;

	msn_user_set_buddy_icon(user, filename);

	msn_change_status(session);
}

static void
msn_remove_group(GaimConnection *gc, GaimGroup *group)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	const char *group_id;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	group_id = msn_userlist_find_group_id(session->userlist, group->name);
	if (group_id != NULL){
		msn_cmdproc_send(cmdproc, "RMG", "%d", group_id);
	}
}

static char *
msn_tooltip_info_text(MsnGetInfoData *info_data)
{
	GString *s;
	GaimBuddy *b;

	s = g_string_sized_new(80); /* wild guess */

	b = gaim_find_buddy(gaim_connection_get_account(info_data->gc),
						info_data->name);

	if (b){
		GString *str = g_string_new("");
		char *tmp;

		if (b->alias && b->alias[0]){
			char *aliastext = g_markup_escape_text(b->alias, -1);
			g_string_append_printf(s, _("<b>Alias:</b> %s<br>"), aliastext);
			g_free(aliastext);
		}

		if (b->server_alias){
			char *nicktext = g_markup_escape_text(b->server_alias, -1);
			g_string_append_printf(s, _("<b>%s:</b> "), _("Nickname"));
			g_string_append_printf(s, "<font sml=\"msn\">%s</font><br>",
					nicktext);
			g_free(nicktext);
		}

		msn_tooltip_text(b, str, TRUE);
		tmp = gaim_strreplace((*str->str == '\n' ? str->str + 1 : str->str),
							  "\n", "<br>");
		g_string_free(str, TRUE);
		g_string_append_printf(s, "%s<br>", tmp);
//		gaim_debug_info("MaYuan","tooltip info string:{%s}\n",s);
		g_free(tmp);
	}

	return g_string_free(s, FALSE);
}

#if PHOTO_SUPPORT

static char *
msn_get_photo_url(const char *url_text)
{
	char *p, *q;

	if ((p = strstr(url_text, PHOTO_URL)) != NULL){
		p += strlen(PHOTO_URL);
	}
	if (p && (strncmp(p, "http://",strlen("http://")) == 0) && ((q = strchr(p, '"')) != NULL))
			return g_strndup(p, q - p);

	return NULL;
}

static void msn_got_photo(void *data, const char *url_text, size_t len);

#endif

#if 0
static char *msn_info_date_reformat(const char *field, size_t len)
{
	char *tmp = g_strndup(field, len);
	time_t t = gaim_str_to_time(tmp, FALSE, NULL, NULL, NULL);

	g_free(tmp);
	return g_strdup(gaim_date_format_short(localtime(&t)));
}
#endif

#define MSN_GOT_INFO_GET_FIELD(a, b) \
	found = gaim_markup_extract_info_field(stripped, stripped_len, s, \
			"\n" a ":\t", 0, "\n", 0, "Undisclosed", b, 0, NULL, NULL); \
	if (found) \
		sect_info = TRUE;

static void
msn_got_info(void *data, const char *url_text, size_t len)
{
	MsnGetInfoData *info_data = (MsnGetInfoData *)data;
	char *stripped, *p, *q;
	char buf[1024];
	char *tooltip_text;
	char *user_url = NULL;
	gboolean found;
	gboolean has_info = FALSE;
	gboolean sect_info = FALSE;
	const char* title = NULL;
	char *url_buffer;
	char *personal = NULL;
	char *business = NULL;
	GString *s, *s2;
	int stripped_len;
#if PHOTO_SUPPORT
	char *photo_url_text = NULL;
	MsnGetInfoStepTwoData *info2_data = NULL;
#endif

	gaim_debug_info("msn", "In msn_got_info,url_text:{%s}\n",url_text);

	/* Make sure the connection is still valid */
	if (g_list_find(gaim_connections_get_all(), info_data->gc) == NULL)
	{
		gaim_debug_warning("msn", "invalid connection. ignoring buddy info.\n");
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	tooltip_text = msn_tooltip_info_text(info_data);
	title = _("MSN Profile");

	if (url_text == NULL || strcmp(url_text, "") == 0)
	{
		g_snprintf(buf, 1024, "<html><body>%s<b>%s</b></body></html>",
				tooltip_text, _("Error retrieving profile"));

		gaim_notify_userinfo(info_data->gc, info_data->name, buf, NULL, NULL);

		g_free(tooltip_text);
		return;
	}

	url_buffer = g_strdup(url_text);

	/* If they have a homepage link, MSN masks it such that we need to
	 * fetch the url out before gaim_markup_strip_html() nukes it */
	/* I don't think this works with the new spaces profiles - Stu 3/2/06 */
	if ((p = strstr(url_text,
			"Take a look at my </font><A class=viewDesc title=\"")) != NULL)
	{
		p += 50;

		if ((q = strchr(p, '"')) != NULL)
			user_url = g_strndup(p, q - p);
	}

	/*
	 * gaim_markup_strip_html() doesn't strip out character entities like &nbsp;
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
	gaim_str_strip_char(url_buffer, '\r');

	/* MSN always puts in &#39; for apostrophes...replace them */
	while ((p = strstr(url_buffer, "&#39;")) != NULL)
	{
		*p = '\'';
		memmove(p + 1, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 4] = '\0';
	}

	/* Nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = gaim_markup_strip_html(url_buffer);
	stripped_len = strlen(stripped);

	gaim_debug_misc("msn", "stripped = %p\n", stripped);
	gaim_debug_misc("msn", "url_buffer = %p\n", url_buffer);

	/* Gonna re-use the memory we've already got for url_buffer */
	/* No we're not. */
	s = g_string_sized_new(strlen(url_buffer));
	s2 = g_string_sized_new(strlen(url_buffer));

	/* Extract their Name and put it in */
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"))

	/* General */
	MSN_GOT_INFO_GET_FIELD("Nickname", _("Nickname"));
	MSN_GOT_INFO_GET_FIELD("Age", _("Age"));
	MSN_GOT_INFO_GET_FIELD("Gender", _("Gender"));
	MSN_GOT_INFO_GET_FIELD("Occupation", _("Occupation"));
	MSN_GOT_INFO_GET_FIELD("Location", _("Location"));

	/* Extract their Interests and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"\nInterests\t", 0, " (/default.aspx?page=searchresults", 0,
			"Undisclosed", _("Hobbies and Interests") /* _("Interests") */,
			0, NULL, NULL);

	if (found)
		sect_info = TRUE;

	MSN_GOT_INFO_GET_FIELD("More about me", _("A Little About Me"));

	if (sect_info)
	{
		/* trim off the trailing "<br>\n" */
		g_string_truncate(s, strlen(s->str) - 5);
		g_string_append_printf(s2, _("%s<b>General</b><br>%s"),
							   (*tooltip_text) ? "<hr>" : "", s->str);
		s = g_string_truncate(s, 0);
		has_info = TRUE;
		sect_info = FALSE;
	}


	/* Social */
	MSN_GOT_INFO_GET_FIELD("Marital status", _("Marital Status"));
	MSN_GOT_INFO_GET_FIELD("Interested in", _("Interests"));
	MSN_GOT_INFO_GET_FIELD("Pets", _("Pets"));
	MSN_GOT_INFO_GET_FIELD("Hometown", _("Hometown"));
	MSN_GOT_INFO_GET_FIELD("Places lived", _("Places Lived"));
	MSN_GOT_INFO_GET_FIELD("Fashion", _("Fashion"));
	MSN_GOT_INFO_GET_FIELD("Humor", _("Humor"));
	MSN_GOT_INFO_GET_FIELD("Music", _("Music"));
	MSN_GOT_INFO_GET_FIELD("Favorite quote", _("Favorite Quote"));

	if (sect_info)
	{
		g_string_append_printf(s2, _("%s<b>Social</b><br>%s"), has_info ? "<br><hr>" : "", s->str);
		s = g_string_truncate(s, 0);
		has_info = TRUE;
		sect_info = FALSE;
	}

	/* Contact Info */
	/* Personal */
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Significant other", _("Significant Other"));
	MSN_GOT_INFO_GET_FIELD("Home phone", _("Home Phone"));
	MSN_GOT_INFO_GET_FIELD("Home phone 2", _("Home Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Home address", _("Home Address"));
	MSN_GOT_INFO_GET_FIELD("Personal Mobile", _("Personal Mobile"));
	MSN_GOT_INFO_GET_FIELD("Home fax", _("Home Fax"));
	MSN_GOT_INFO_GET_FIELD("Personal e-mail", _("Personal E-Mail"));
	MSN_GOT_INFO_GET_FIELD("Personal IM", _("Personal IM"));
	MSN_GOT_INFO_GET_FIELD("Birthday", _("Birthday"));
	MSN_GOT_INFO_GET_FIELD("Anniversary", _("Anniversary"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		personal = g_strdup_printf(_("<br><b>Personal</b><br>%s"), s->str);
		s = g_string_truncate(s, 0);
		sect_info = FALSE;
	}

	/* Business */
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
	MSN_GOT_INFO_GET_FIELD("Work e-mail", _("Work E-Mail"));
	MSN_GOT_INFO_GET_FIELD("Work IM", _("Work IM"));
	MSN_GOT_INFO_GET_FIELD("Start date", _("Start Date"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		business = g_strdup_printf(_("<br><b>Business</b><br>%s"), s->str);
		s = g_string_truncate(s, 0);
		sect_info = FALSE;
	}

	if ((personal != NULL) || (business != NULL))
	{
		/* trim off the trailing "<br>\n" */
		g_string_truncate(s, strlen(s->str) - 5);

		has_info = TRUE;
		g_string_append_printf(s2, _("<hr><b>Contact Info</b>%s%s"),
							   personal ? personal : "",
							   business ? business : "");
	}

	g_free(personal);
	g_free(business);
	g_string_free(s, TRUE);
	s = s2;

#if 0 /* these probably don't show up any more */
	/*
	 * The fields, 'A Little About Me', 'Favorite Things', 'Hobbies
	 * and Interests', 'Favorite Quote', and 'My Homepage' may or may
	 * not appear, in any combination. However, they do appear in
	 * certain order, so we can successively search to pin down the
	 * distinct values.
	 */

	/* Check if they have A Little About Me */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			" A Little About Me \n\n", 0, "Favorite Things", '\n', NULL,
			_("A Little About Me"), 0, NULL, NULL);

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Hobbies and Interests", '\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Favorite Quote", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "My Homepage \n\nTake a look",
				'\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "last updated", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Things */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			" Favorite Things \n\n", 0, "Hobbies and Interests", '\n', NULL,
			_("Favorite Things"), 0, NULL, NULL);

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "Favorite Quote", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "My Homepage \n\nTake a look", '\n',
				NULL, _("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Hobbies and Interests */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			" Hobbies and Interests \n\n", 0, "Favorite Quote", '\n', NULL,
			_("Hobbies and Interests"), 0, NULL, NULL);

	if (!found)
	{
		found = gaim_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "My Homepage \n\nTake a look",
				'\n', NULL, _("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "last updated", '\n', NULL,
				_("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Quote */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			"Favorite Quote \n\n", 0, "My Homepage \n\nTake a look", '\n', NULL,
			_("Favorite Quote"), 0, NULL, NULL);

	if (!found)
	{
		gaim_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Quote \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Quote"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Extract the last updated date and put it in */
	found = gaim_markup_extract_info_field(stripped, stripped_len, s,
			" last updated:", 1, "\n", 0, NULL, _("Last Updated"), 0,
			NULL, msn_info_date_reformat);

	if (found)
		has_info = TRUE;
#endif

	/* If we were able to fetch a homepage url earlier, stick it in there */
	if (user_url != NULL)
	{
		g_snprintf(buf, sizeof(buf),
				   "<b>%s:</b><br><a href=\"%s\">%s</a><br>\n",
				   _("Homepage"), user_url, user_url);

		g_string_append(s, buf);

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
		char *p = strstr(url_buffer, "form id=\"SpacesSearch\" name=\"SpacesSearch\"");
		GaimBuddy *b = gaim_find_buddy
				(gaim_connection_get_account(info_data->gc), info_data->name);
		g_string_append_printf(s, "<br><b>%s</b><br>%s<br><br>",
				_("Error retrieving profile"),
				((p && b)?
					_("The user has not created a public profile."):
				 p? _("MSN reported not being able to find the user's profile. "
					  "This either means that the user does not exist, "
					  "or that the user exists "
					  "but has not created a public profile."):
					_("Gaim could not find "	/* This should never happen */
					  "any information in the user's profile. "
					  "The user most likely does not exist.")));
	}
	/* put a link to the actual profile URL */
	g_string_append_printf(s, _("<hr><b>%s:</b> "), _("Profile URL"));
	g_string_append_printf(s, "<br><a href=\"%s%s\">%s%s</a><br>",
			PROFILE_URL, info_data->name, PROFILE_URL, info_data->name);

	/* Finish it off, and show it to them */
	g_string_append(s, "</body></html>\n");

#if PHOTO_SUPPORT
	/* Find the URL to the photo; must be before the marshalling [Bug 994207] */
	photo_url_text = msn_get_photo_url(url_text);
	gaim_debug_info("Ma Yuan","photo url:{%s}\n",photo_url_text);

	/* Marshall the existing state */
	info2_data = g_malloc0(sizeof(MsnGetInfoStepTwoData));
	info2_data->info_data = info_data;
	info2_data->stripped = stripped;
	info2_data->url_buffer = url_buffer;
	info2_data->s = s;
	info2_data->photo_url_text = photo_url_text;
	info2_data->tooltip_text = tooltip_text;
	info2_data->title = title;

	/* Try to put the photo in there too, if there's one */
	if (photo_url_text)
	{
		gaim_url_fetch(photo_url_text, FALSE, NULL, FALSE, msn_got_photo,
					   info2_data);
	}
	else
	{
		/* Emulate a callback */
		msn_got_photo(info2_data, NULL, 0);
	}
}

static void
msn_got_photo(void *data, const char *url_text, size_t len)
{
	MsnGetInfoStepTwoData *info2_data = (MsnGetInfoStepTwoData *)data;
	int id = -1;

	/* Unmarshall the saved state */
	MsnGetInfoData *info_data = info2_data->info_data;
	char *stripped = info2_data->stripped;
	char *url_buffer = info2_data->url_buffer;
	GString *s = info2_data->s;
	char *photo_url_text = info2_data->photo_url_text;
	char *tooltip_text = info2_data->tooltip_text;

	/* Make sure the connection is still valid if we got here by fetching a photo url */
	if (url_text &&
		g_list_find(gaim_connections_get_all(), info_data->gc) == NULL)
	{
		gaim_debug_warning("msn", "invalid connection. ignoring buddy photo info.\n");
		g_free(stripped);
		g_free(url_buffer);
		g_string_free(s, TRUE);
		g_free(tooltip_text);
		g_free(info_data->name);
		g_free(info_data);
		g_free(photo_url_text);
		g_free(info2_data);

		return;
	}

	/* Try to put the photo in there too, if there's one and is readable */
	if (data && url_text && len != 0)
	{
		if (strstr(url_text, "400 Bad Request")
			|| strstr(url_text, "403 Forbidden")
			|| strstr(url_text, "404 Not Found"))
		{

			gaim_debug_info("msn", "Error getting %s: %s\n",
					photo_url_text, url_text);
		}
		else
		{
			char buf[1024];
			gaim_debug_info("msn", "%s is %d bytes\n", photo_url_text, len);
			id = gaim_imgstore_add(url_text, len, NULL);
			g_snprintf(buf, sizeof(buf), "<img id=\"%d\"><br>", id);
			g_string_prepend(s, buf);
		}
	}

	/* We continue here from msn_got_info, as if nothing has happened */
#endif

	g_string_prepend(s, tooltip_text);
	gaim_notify_userinfo(info_data->gc, info_data->name, s->str, NULL, NULL);

	g_free(stripped);
	g_free(url_buffer);
	g_string_free(s, TRUE);
	g_free(tooltip_text);
	g_free(info_data->name);
	g_free(info_data);
#if PHOTO_SUPPORT
	g_free(photo_url_text);
	g_free(info2_data);
	if (id != -1)
		gaim_imgstore_unref(id);
#endif
}

static void
msn_get_info(GaimConnection *gc, const char *name)
{
	MsnGetInfoData *data;
	char *url;

	data       = g_new0(MsnGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	url = g_strdup_printf("%s%s", PROFILE_URL, name);

	gaim_url_fetch(url, FALSE,
				   "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)",
				   TRUE, msn_got_info, data);

	g_free(url);
}

static gboolean msn_load(GaimPlugin *plugin)
{
	msn_notification_init();
	msn_switchboard_init();
	msn_sync_init();

	return TRUE;
}

static gboolean msn_unload(GaimPlugin *plugin)
{
	msn_notification_end();
	msn_switchboard_end();
	msn_sync_end();

	return TRUE;
}

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"png", 0, 0, 96, 96, GAIM_ICON_SCALE_SEND},	/* icon_spec */
	msn_list_icon,			/* list_icon */
	msn_list_emblems,		/* list_emblems */
	msn_status_text,		/* status_text */
	msn_tooltip_text,		/* tooltip_text */
	msn_status_types,		/* away_states */
	msn_blist_node_menu,		/* blist_node_menu */
	NULL,					/* chat_info */
	NULL,					/* chat_info_defaults */
	msn_login,			/* login */
	msn_close,			/* close */
	msn_send_im,			/* send_im */
	NULL,					/* set_info */
	msn_send_typing,		/* send_typing */
	msn_get_info,			/* get_info */
	msn_set_status,			/* set_away */
	msn_set_idle,			/* set_idle */
	NULL,					/* change_passwd */
	msn_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	msn_rem_buddy,			/* remove_buddy */
	NULL,					/* remove_buddies */
	msn_add_permit,			/* add_permit */
	msn_add_deny,			/* add_deny */
	msn_rem_permit,			/* rem_permit */
	msn_rem_deny,			/* rem_deny */
	msn_set_permit_deny,	/* set_permit_deny */
	NULL,					/* join_chat */
	NULL,					/* reject chat invite */
	NULL,					/* get_chat_name */
	msn_chat_invite,		/* chat_invite */
	msn_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	msn_chat_send,			/* chat_send */
	msn_keepalive,			/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	msn_group_buddy,		/* group_buddy */
	msn_rename_group,		/* rename_group */
	NULL,					/* buddy_free */
	msn_convo_closed,		/* convo_closed */
	msn_normalize,			/* normalize */
	msn_set_buddy_icon,		/* set_buddy_icon */
	msn_remove_group,		/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	msn_can_receive_file,	/* can_receive_file */
	msn_send_file,			/* send_file */
	msn_new_xfer,			/* new_xfer */
	NULL,					/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-msn",                                       /**< id             */
	"MSN",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Windows Live Messenger Protocol Plugin"),
	                                                  /**  description    */
	N_("Windows Live Messenger Protocol Plugin"),
	"MaYuan <mayuan2006@gmail.com>",				/**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	msn_load,                                         /**< load           */
	msn_unload,                                       /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	msn_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Server"), "server",
											WLM_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_int_new(_("Port"), "port", WLM_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_bool_new(_("Use HTTP Method"),
										  "http_method", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_bool_new(_("Show custom smileys"),
										  "custom_smileys", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	gaim_cmd_register("nudge", "", GAIM_CMD_P_PRPL,
	                  GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_PRPL_ONLY,
	                 "prpl-msn", msn_cmd_nudge,
	                  _("nudge: nudge a user to get their attention"), NULL);

	gaim_prefs_remove("/plugins/prpl/msn");
}

GAIM_INIT_PLUGIN(msn, init_plugin, info);
