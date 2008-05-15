/**
 * @file notification.c Notification server functions
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
#include "msn.h"
#include "notification.h"
#include "state.h"
#include "error.h"
#include "msnutils.h"
#include "page.h"

#include "userlist.h"
#include "sync.h"
#include "slplink.h"

static MsnTable *cbs_table;

/****************************************************************************
 * 	Local Function Prototype
 ****************************************************************************/

static void msn_notification_post_adl(MsnCmdProc *cmdproc, const char *payload, int payload_len);
static void
msn_add_contact_xml(MsnSession *session, xmlnode *mlNode,const char *passport,
					 MsnListOp list_op, MsnUserType type);

/**************************************************************************
 * Main
 **************************************************************************/

static void
destroy_cb(MsnServConn *servconn)
{
	MsnNotification *notification;

	notification = servconn->cmdproc->data;
	g_return_if_fail(notification != NULL);

	msn_notification_destroy(notification);
}

MsnNotification *
msn_notification_new(MsnSession *session)
{
	MsnNotification *notification;
	MsnServConn *servconn;

	g_return_val_if_fail(session != NULL, NULL);

	notification = g_new0(MsnNotification, 1);

	notification->session = session;
	notification->servconn = servconn = msn_servconn_new(session, MSN_SERVCONN_NS);
	msn_servconn_set_destroy_cb(servconn, destroy_cb);

	notification->cmdproc = servconn->cmdproc;
	notification->cmdproc->data = notification;
	notification->cmdproc->cbs_table = cbs_table;

	return notification;
}

void
msn_notification_destroy(MsnNotification *notification)
{
	notification->cmdproc->data = NULL;

	msn_servconn_set_destroy_cb(notification->servconn, NULL);

	msn_servconn_destroy(notification->servconn);

	g_free(notification);
}

/**************************************************************************
 * Connect
 **************************************************************************/

static void
connect_cb(MsnServConn *servconn)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	PurpleAccount *account;
	GString *vers;
	const char *ver_str;
	int i;

	g_return_if_fail(servconn != NULL);

	cmdproc = servconn->cmdproc;
	session = servconn->session;
	account = session->account;

	vers = g_string_new("");

/*	for (i = session->protocol_ver; i >= WLM_MIN_PROTOCOL; i--) */
	for (i = WLM_MAX_PROTOCOL; i >= WLM_MIN_PROTOCOL; i--)
		g_string_append_printf(vers, " MSNP%d", i);

	g_string_append(vers, " CVR0");

	if (session->login_step == MSN_LOGIN_STEP_START)
		msn_session_set_login_step(session, MSN_LOGIN_STEP_HANDSHAKE);
	else
		msn_session_set_login_step(session, MSN_LOGIN_STEP_HANDSHAKE2);

	/* Skip the initial space */
	ver_str = (vers->str + 1);
	msn_cmdproc_send(cmdproc, "VER", "%s", ver_str);

	g_string_free(vers, TRUE);
}

gboolean
msn_notification_connect(MsnNotification *notification, const char *host, int port)
{
	MsnServConn *servconn;

	g_return_val_if_fail(notification != NULL, FALSE);

	servconn = notification->servconn;

	msn_servconn_set_connect_cb(servconn, connect_cb);
	notification->in_use = msn_servconn_connect(servconn, host, port);

	return notification->in_use;
}

void
msn_notification_disconnect(MsnNotification *notification)
{
	g_return_if_fail(notification != NULL);
	g_return_if_fail(notification->in_use);

	msn_servconn_disconnect(notification->servconn);

	notification->in_use = FALSE;
}

/**************************************************************************
 * Util
 **************************************************************************/

static void
group_error_helper(MsnSession *session, const char *msg, const char *group_id, int error)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	char *reason = NULL;
	char *title = NULL;

	account = session->account;
	gc = purple_account_get_connection(account);

	if (error == 224)
	{
		if (group_id == 0)
		{
			return;
		}
		else
		{
			const char *group_name;
			group_name = msn_userlist_find_group_name(session->userlist,group_id);
			reason = g_strdup_printf(_("%s is not a valid group."),
									 group_name ? group_name : "");
		}
	}
	else
	{
		reason = g_strdup(_("Unknown error."));
	}

	title = g_strdup_printf(_("%s on %s (%s)"), msg,
						  purple_account_get_username(account),
						  purple_account_get_protocol_name(account));
	purple_notify_error(gc, NULL, title, reason);
	g_free(title);
	g_free(reason);
}

/**************************************************************************
 * Login
 **************************************************************************/

void
msn_got_login_params(MsnSession *session, const char *login_params)
{
	MsnCmdProc *cmdproc;

	cmdproc = session->notification->cmdproc;

	msn_session_set_login_step(session, MSN_LOGIN_STEP_AUTH_END);

	msn_cmdproc_send(cmdproc, "USR", "TWN S %s", login_params);
}

static void
cvr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	PurpleAccount *account;

	account = cmdproc->session->account;
	msn_cmdproc_send(cmdproc, "USR", "TWN I %s",
					 purple_account_get_username(account));
}

static void
usr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;

	session = cmdproc->session;
	account = session->account;

	if (!g_ascii_strcasecmp(cmd->params[1], "OK"))
	{
		/* authenticate OK */
		/* friendly name part no longer true in msnp11 */
#if 0
		const char *friendly = purple_url_decode(cmd->params[3]);

		purple_connection_set_display_name(gc, friendly);
#endif
		msn_session_set_login_step(session, MSN_LOGIN_STEP_SYN);

//		msn_cmdproc_send(cmdproc, "SYN", "%s", "0");
		//TODO we should use SOAP contact to fetch contact list
	}
	else if (!g_ascii_strcasecmp(cmd->params[1], "TWN"))
	{
		/* Passport authentication */
		char **elems, **cur, **tokens;

		session->nexus = msn_nexus_new(session);

		/* Parse the challenge data. */
		session->nexus->challenge_data_str = g_strdup(cmd->params[3]);
		elems = g_strsplit(cmd->params[3], ",", 0);

		for (cur = elems; *cur != NULL; cur++)
		{
			tokens = g_strsplit(*cur, "=", 2);
			if(tokens[0] && tokens[1])
			{
				purple_debug_info("MSNP14","challenge %p,key:%s,value:%s\n",
									session->nexus->challenge_data,tokens[0],tokens[1]);
				g_hash_table_insert(session->nexus->challenge_data, tokens[0], tokens[1]);
				/* Don't free each of the tokens, only the array. */
				g_free(tokens);
			} else
				g_strfreev(tokens);
		}

		g_strfreev(elems);

		msn_session_set_login_step(session, MSN_LOGIN_STEP_AUTH_START);

		msn_nexus_connect(session->nexus);
	}
}

static void
usr_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnErrorType msnerr = 0;

	switch (error)
	{
		case 500:
		case 601:
		case 910:
		case 921:
			msnerr = MSN_ERROR_SERV_UNAVAILABLE;
			break;
		case 911:
			msnerr = MSN_ERROR_AUTH;
			break;
		default:
			return;
			break;
	}

	msn_session_set_error(cmdproc->session, msnerr, NULL);
}

static void
ver_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;
	gboolean protocol_supported = FALSE;
	char proto_str[8];
	size_t i;

	session = cmdproc->session;
	account = session->account;

	g_snprintf(proto_str, sizeof(proto_str), "MSNP%d", session->protocol_ver);

	for (i = 1; i < cmd->param_count; i++)
	{
		if (!strcmp(cmd->params[i], proto_str))
		{
			protocol_supported = TRUE;
			break;
		}
	}

	if (!protocol_supported)
	{
		msn_session_set_error(session, MSN_ERROR_UNSUPPORTED_PROTOCOL,
							  NULL);
		return;
	}

	/*
	 * Windows Live Messenger 8.0 
	 * Notice :CVR String discriminate!
	 * reference of http://www.microsoft.com/globaldev/reference/oslocversion.mspx
	 * to see the Local ID
	 */
	msn_cmdproc_send(cmdproc, "CVR",
//					 "0x0409 winnt 5.1 i386 MSG80BETA 8.0.0689 msmsgs %s",
					"0x0804 winnt 5.1 i386 MSNMSGR 8.0.0792 msmsgs %s",
					 purple_account_get_username(account));
}

/**************************************************************************
 * Log out
 **************************************************************************/

static void
out_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	if (!g_ascii_strcasecmp(cmd->params[0], "OTH"))
		msn_session_set_error(cmdproc->session, MSN_ERROR_SIGN_OTHER,
							  NULL);
	else if (!g_ascii_strcasecmp(cmd->params[0], "SSD"))
		msn_session_set_error(cmdproc->session, MSN_ERROR_SERV_DOWN, NULL);
}

void
msn_notification_close(MsnNotification *notification)
{
	g_return_if_fail(notification != NULL);

	if (!notification->in_use)
		return;

	msn_cmdproc_send_quick(notification->cmdproc, "OUT", NULL, NULL);

	msn_notification_disconnect(notification);
}

/**************************************************************************
 * Messages
 **************************************************************************/

static void
msg_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
			 size_t len)
{
	MsnMessage *msg;

	msg = msn_message_new_from_cmd(cmdproc->session, cmd);

	msn_message_parse_payload(msg, payload, len,MSG_LINE_DEM,MSG_BODY_DEM);
#ifdef MSN_DEBUG_NS
	msn_message_show_readable(msg, "Notification", TRUE);
#endif

	msn_cmdproc_process_msg(cmdproc, msg);

	msn_message_destroy(msg);
}

static void
msg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_info("MSNP14","Processing MSG... \n");
	if(cmd->payload_len == 0){
		return;
	}
	/* NOTE: cmd is not always cmdproc->last_cmd, sometimes cmd is a queued
	 * command and we are processing it */
	if (cmd->payload == NULL)
	{
		cmdproc->last_cmd->payload_cb  = msg_cmd_post;
		cmdproc->servconn->payload_len = atoi(cmd->params[2]);
	}
	else
	{
		g_return_if_fail(cmd->payload_cb != NULL);

#if 0 /* glib on win32 doesn't correctly support precision modifiers for a string */
		purple_debug_info("MSNP14", "MSG payload:{%.*s}\n", cmd->payload_len, cmd->payload);
#endif
		cmd->payload_cb(cmdproc, cmd, cmd->payload, cmd->payload_len);
	}
}

/*send Message to Yahoo Messenger*/
void
uum_send_msg(MsnSession *session,MsnMessage *msg)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	char *payload;
	gsize payload_len;
	int type;
	
	cmdproc = session->notification->cmdproc;
	g_return_if_fail(msg     != NULL);
	payload = msn_message_gen_payload(msg, &payload_len);
	purple_debug_info("MSNP14",
		"send UUM, payload{%s}, strlen:%" G_GSIZE_FORMAT ", len:%" G_GSIZE_FORMAT "\n",
		payload, strlen(payload), payload_len);
	type = msg->type;
	trans = msn_transaction_new(cmdproc, "UUM", "%s 32 %d %" G_GSIZE_FORMAT,
		msg->remote_user, type, payload_len);
	msn_transaction_set_payload(trans, payload, strlen(payload));
	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
ubm_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
			 size_t len)
{
	MsnMessage *msg;
	PurpleConnection *gc;
	const char *passport;
	const char *content_type;

	purple_debug_info("MSNP14","Process UBM payload:%.*s\n", (guint)len, payload);
	msg = msn_message_new_from_cmd(cmdproc->session, cmd);

	msn_message_parse_payload(msg, payload, len,MSG_LINE_DEM,MSG_BODY_DEM);
#ifdef MSN_DEBUG_NS
	msn_message_show_readable(msg, "Notification", TRUE);
#endif

	gc = cmdproc->session->account->gc;
	passport = msg->remote_user;

	content_type = msn_message_get_content_type(msg);
	purple_debug_info("MSNP14", "type:%s\n", content_type);
	if(!strcmp(content_type,"text/plain")){
		const char *value;
		const char *body;
		char *body_enc;
		char *body_final = NULL;
		size_t body_len;

		body = msn_message_get_bin_data(msg, &body_len);
		body_enc = g_markup_escape_text(body, body_len);

		if ((value = msn_message_get_attr(msg, "X-MMS-IM-Format")) != NULL)	{
			char *pre, *post;

			msn_parse_format(value, &pre, &post);
			body_final = g_strdup_printf("%s%s%s", pre ? pre : "",
							body_enc ? body_enc : "", post ? post : "");
			g_free(pre);
			g_free(post);
		}
		g_free(body_enc);
		serv_got_im(gc, passport, body_final, 0, time(NULL));
		g_free(body_final);
	}
	if(!strcmp(content_type,"text/x-msmsgscontrol")){
		if(msn_message_get_attr(msg, "TypingUser") != NULL){
			serv_got_typing(gc, passport, MSN_TYPING_RECV_TIMEOUT,
						PURPLE_TYPING);
		}
	}
	if(!strcmp(content_type,"text/x-msnmsgr-datacast")){
		char *username, *str;
		PurpleAccount *account;
		PurpleBuddy *buddy;
		const char *user;

		account = cmdproc->session->account;
		user = msg->remote_user;

		if ((buddy = purple_find_buddy(account, user)) != NULL){
			username = g_markup_escape_text(purple_buddy_get_alias(buddy), -1);
		}else{
			username = g_markup_escape_text(user, -1);
		}

		str = g_strdup_printf(_("%s just sent you a Nudge!"), username);
		g_free(username);
		msn_session_report_user(cmdproc->session,user,str,PURPLE_MESSAGE_SYSTEM);
		g_free(str);
	}
	msn_message_destroy(msg);
}

/*Yahoo msg process*/
static void
ubm_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_info("MSNP14","Processing UBM... \n");
	if(cmd->payload_len == 0){
		return;
	}
	/* NOTE: cmd is not always cmdproc->last_cmd, sometimes cmd is a queued
	 * command and we are processing it */
	if (cmd->payload == NULL){
		cmdproc->last_cmd->payload_cb  = ubm_cmd_post;
		cmdproc->servconn->payload_len = atoi(cmd->params[2]);
	}else{
		g_return_if_fail(cmd->payload_cb != NULL);

		purple_debug_info("MSNP14", "UBM payload:{%.*s}\n", (guint)(cmd->payload_len), cmd->payload);
		ubm_cmd_post(cmdproc, cmd, cmd->payload, cmd->payload_len);
	}
}

/**************************************************************************
 * Challenges
 *  we use MD5 to caculate the Challenges
 **************************************************************************/
static void
chl_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnTransaction *trans;
	char buf[33];

#if 0
	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, (const guchar *)cmd->params[1],
							   strlen(cmd->params[1]));
	challenge_resp = MSNP13_WLM_PRODUCT_KEY;

	purple_cipher_context_append(context, (const guchar *)challenge_resp,
							   strlen(challenge_resp));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	purple_cipher_context_destroy(context);

	for (i = 0; i < 16; i++)
	{
		g_snprintf(buf + (i*2), 3, "%02x", digest[i]);
	}
#else
	msn_handle_chl(cmd->params[1], buf);
#endif
//	purple_debug_info("MSNP14","<<challenge:{%s}:{%s}\n",cmd->params[1],buf);
	trans = msn_transaction_new(cmdproc, "QRY", "%s 32", MSNP13_WLM_PRODUCT_ID);

	msn_transaction_set_payload(trans, buf, 32);

	msn_cmdproc_send_trans(cmdproc, trans);
}

/**************************************************************************
 * Buddy Lists
 **************************************************************************/
/* add contact to xmlnode */
static void
msn_add_contact_xml(MsnSession *session, xmlnode *mlNode,const char *passport, MsnListOp list_op, MsnUserType type)
{
	xmlnode *d_node,*c_node;
	char **tokens;
	const char *email,*domain;
	char fmt_str[3];

	g_return_if_fail(passport != NULL);

	purple_debug_info("MSNP14","Passport: %s, type: %d\n", passport, type);
	tokens = g_strsplit(passport, "@", 2);
	email = tokens[0];
	domain = tokens[1];

	if (email == NULL || domain == NULL) {
		purple_debug_error("msn", "Invalid passport (%s) specified to add to contact xml.\n", passport);
		g_strfreev(tokens);
		g_return_if_reached();
	}

	/*find a domain Node*/
	for(d_node = xmlnode_get_child(mlNode,"d"); d_node; d_node = xmlnode_get_next_twin(d_node))
	{
		const char *attr = xmlnode_get_attrib(d_node,"n");
		if (attr == NULL)
			continue;
		if (!strcmp(attr,domain))
			break;
	}

	if(d_node == NULL)
	{
		/*domain not found, create a new domain Node*/
		purple_debug_info("msn", "Didn't find existing domain node, adding one.\n");
		d_node = xmlnode_new("d");
		xmlnode_set_attrib(d_node, "n", domain);
		xmlnode_insert_child(mlNode, d_node);
	}

	/*create contact node*/
	c_node = xmlnode_new("c");
	xmlnode_set_attrib(c_node, "n", email);

	purple_debug_info("MSNP14", "list_op: %d\n", list_op);
	g_snprintf(fmt_str, sizeof(fmt_str), "%d", list_op);
	xmlnode_set_attrib(c_node, "l", fmt_str);

	if (type != MSN_USER_TYPE_UNKNOWN)
		g_snprintf(fmt_str, sizeof(fmt_str), "%d", type);
	else if (msn_user_is_yahoo(session->account, passport))
		g_snprintf(fmt_str, sizeof(fmt_str), "%d", MSN_USER_TYPE_YAHOO);
	else
		g_snprintf(fmt_str, sizeof(fmt_str), "%d", MSN_USER_TYPE_PASSPORT);

	/*mobile*/
	//type_str = g_strdup_printf("4");
	xmlnode_set_attrib(c_node, "t", fmt_str);

	xmlnode_insert_child(d_node, c_node);

	g_strfreev(tokens);
}

static void
msn_notification_post_adl(MsnCmdProc *cmdproc, const char *payload, int payload_len)
{
	MsnTransaction *trans;
	purple_debug_info("MSN Notification","Sending ADL with payload: %s\n", payload);
	trans = msn_transaction_new(cmdproc, "ADL","%" G_GSIZE_FORMAT, payload_len);
	msn_transaction_set_payload(trans, payload, payload_len);
	msn_cmdproc_send_trans(cmdproc, trans);
}

/*dump contact info to NS*/
void
msn_notification_dump_contact(MsnSession *session)
{
	MsnUser *user;
	GList *l;
	xmlnode *adl_node;
	char *payload;
	int payload_len;
	int adl_count = 0;
	const char *display_name;

	adl_node = xmlnode_new("ml");
	adl_node->child = NULL;
	xmlnode_set_attrib(adl_node, "l", "1");

	/*get the userlist*/
	for (l = session->userlist->users; l != NULL; l = l->next){
		user = l->data;

		/* skip RL & PL during initial dump */
		if (!(user->list_op & MSN_LIST_OP_MASK))
			continue;

		msn_add_contact_xml(session, adl_node, user->passport,
			user->list_op & MSN_LIST_OP_MASK, user->type);

		/* each ADL command may contain up to 150 contacts */
		if (++adl_count % 150 == 0 || l->next == NULL) {
			payload = xmlnode_to_str(adl_node,&payload_len);

			msn_notification_post_adl(session->notification->cmdproc,
				payload, payload_len);

			g_free(payload);
			xmlnode_free(adl_node);

			if (l->next) {
				adl_node = xmlnode_new("ml");
				adl_node->child = NULL;
				xmlnode_set_attrib(adl_node, "l", "1");
			}
		}
	}

	if (adl_count == 0) {
		payload = xmlnode_to_str(adl_node,&payload_len);

		msn_notification_post_adl(session->notification->cmdproc, payload, payload_len);

		g_free(payload);
		xmlnode_free(adl_node);
	}

	display_name = purple_connection_get_display_name(session->account->gc);
	if (display_name 
	    && strcmp(display_name, 
		      purple_account_get_username(session->account))) {
		msn_act_id(session->account->gc, display_name);
	}

}

/*Post FQY to NS,Inform add a Yahoo User*/
void
msn_notification_send_fqy(MsnSession *session, const char *passport)
{
	MsnTransaction *trans;
	MsnCmdProc *cmdproc;
	char* email,*domain,*payload;
	char **tokens;

	cmdproc = session->notification->cmdproc;

	tokens = g_strsplit(passport, "@", 2);
	email = tokens[0];
	domain = tokens[1];

	payload = g_strdup_printf("<ml><d n=\"%s\"><c n=\"%s\"/></d></ml>", domain, email);
	trans = msn_transaction_new(cmdproc, "FQY","%" G_GSIZE_FORMAT, strlen(payload));
	msn_transaction_set_payload(trans, payload, strlen(payload));
	msn_cmdproc_send_trans(cmdproc, trans);

	g_free(payload);
	g_strfreev(tokens);
}

static void
blp_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
}

static void
adl_cmd_parse(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
		                         size_t len)
{
	xmlnode *root, *domain_node;

	purple_debug_misc("MSN Notification", "Parsing received ADL XML data\n");

	g_return_if_fail(payload != NULL);
	
	root = xmlnode_from_str(payload, (gssize) len);
	
	if (root == NULL) {
		purple_debug_info("MSN Notification", "Invalid XML!\n");
		return;
	}
	for (domain_node = xmlnode_get_child(root, "d"); domain_node; domain_node = xmlnode_get_next_twin(domain_node)) {
		const gchar * domain = NULL; 
		xmlnode *contact_node = NULL;

		domain = xmlnode_get_attrib(domain_node, "n");

		for (contact_node = xmlnode_get_child(domain_node, "c"); contact_node; contact_node = xmlnode_get_next_twin(contact_node)) {
//			gchar *name = NULL, *friendlyname = NULL, *passport= NULL;
			const gchar *list;
			gint list_op = 0;

//			name = xmlnode_get_attrib(contact_node, "n");
			list = xmlnode_get_attrib(contact_node, "l");
			if (list != NULL) {
				list_op = atoi(list);
			}
//			friendlyname = xmlnode_get_attrib(contact_node, "f");

//			passport = g_strdup_printf("%s@%s", name, domain);

//			if (friendlyname != NULL) {
//				decoded_friendlyname = g_strdup(purple_url_decode(friendlyname));
//			} else {
//				decoded_friendlyname = g_strdup(passport);
//			}

			if (list_op & MSN_LIST_RL_OP) {
				/* someone is adding us */
//				got_new_entry(cmdproc->session->account->gc, passport, decoded_friendly_name);
				msn_get_contact_list(cmdproc->session->contact, MSN_PS_PENDING_LIST, NULL);
			}

//			g_free(decoded_friendly_name);
//			g_free(passport);
		}
	}

	xmlnode_free(root);
}

static void
adl_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(cmdproc->session != NULL);
	g_return_if_fail(cmdproc->last_cmd != NULL);
	g_return_if_fail(cmd != NULL);

	session = cmdproc->session;

	if ( !strcmp(cmd->params[1], "OK")) {
		/* ADL ack */
		msn_session_finish_login(session);
	} else {
		cmdproc->last_cmd->payload_cb = adl_cmd_parse;
	}

	return;
}

static void
adl_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnSession *session;
	PurpleAccount *account;
	PurpleConnection *gc;
	char *reason = NULL;

	session = cmdproc->session;
	account = session->account;
	gc = purple_account_get_connection(account);

	purple_debug_error("msn","ADL error\n");
	reason = g_strdup_printf(_("Unknown error (%d)"), error);
	purple_notify_error(gc, NULL, _("Unable to add user"), reason);
	g_free(reason);
}

static void
fqy_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
			 size_t len)
{
	purple_debug_info("MSN Notification","FQY payload:\n%s\n", payload);
	g_return_if_fail(cmdproc->session != NULL);
	g_return_if_fail(cmdproc->session->contact != NULL);
//	msn_notification_post_adl(cmdproc, payload, len);
//	msn_get_address_book(cmdproc->session->contact, MSN_AB_SAVE_CONTACT, NULL, NULL);
}

static void
fqy_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_info("MSNP14","Process FQY\n");
	cmdproc->last_cmd->payload_cb = fqy_cmd_post;
}

static void
rml_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
#if 0
	MsnTransaction *trans;
	char * payload;
#endif

	purple_debug_info("MSNP14","Process RML\n");
#if 0
	trans = msn_transaction_new(cmdproc, "RML","");

	msn_transaction_set_payload(trans, payload, strlen(payload));

	msn_cmdproc_send_trans(cmdproc, trans);
#endif
}

static void
add_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnSession *session;
	PurpleAccount *account;
	PurpleConnection *gc;
	const char *list, *passport;
	char *reason = NULL;
	char *msg = NULL;
	char **params;

	session = cmdproc->session;
	account = session->account;
	gc = purple_account_get_connection(account);
	params = g_strsplit(trans->params, " ", 0);

	list     = params[0];
	passport = params[1];

	if (!strcmp(list, "FL"))
		msg = g_strdup_printf(_("Unable to add user on %s (%s)"),
							  purple_account_get_username(account),
							  purple_account_get_protocol_name(account));
	else if (!strcmp(list, "BL"))
		msg = g_strdup_printf(_("Unable to block user on %s (%s)"),
							  purple_account_get_username(account),
							  purple_account_get_protocol_name(account));
	else if (!strcmp(list, "AL"))
		msg = g_strdup_printf(_("Unable to permit user on %s (%s)"),
							  purple_account_get_username(account),
							  purple_account_get_protocol_name(account));

	if (!strcmp(list, "FL"))
	{
		if (error == 210)
		{
			reason = g_strdup_printf(_("%s could not be added because "
									   "your buddy list is full."), passport);
		}
	}

	if (reason == NULL)
	{
		if (error == 208)
		{
			reason = g_strdup_printf(_("%s is not a valid passport account."),
									 passport);
		}
		else if (error == 500)
		{
			reason = g_strdup(_("Service Temporarily Unavailable."));
		}
		else
		{
			reason = g_strdup(_("Unknown error."));
		}
	}

	if (msg != NULL)
	{
		purple_notify_error(gc, NULL, msg, reason);
		g_free(msg);
	}

	if (!strcmp(list, "FL"))
	{
		PurpleBuddy *buddy;

		buddy = purple_find_buddy(account, passport);

		if (buddy != NULL)
			purple_blist_remove_buddy(buddy);
	}

	g_free(reason);

	g_strfreev(params);
}

static void
adg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	gint group_id;
	const char *group_name;

	session = cmdproc->session;

	group_id = atoi(cmd->params[3]);

	group_name = purple_url_decode(cmd->params[2]);

	msn_group_new(session->userlist, cmd->params[3], group_name);

	/* There is a user that must be moved to this group */
	if (cmd->trans->data)
	{
		/* msn_userlist_move_buddy(); */
		MsnUserList *userlist = cmdproc->session->userlist;
		MsnCallbackState *data = cmd->trans->data;

		if (data->old_group_name != NULL)
		{
			msn_userlist_move_buddy(userlist, data->who, data->old_group_name, group_name);
			g_free(data->old_group_name);
		} else {
			// msn_add_contact_to_group(userlist, data, data->who, group_name);
		}
	}
}

static void
qng_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	static int count = 0;
	const char *passport;
	PurpleAccount *account;

	session = cmdproc->session;
	account = session->account;

	if (session->passport_info.file == NULL)
		return;

	passport = purple_normalize(account, purple_account_get_username(account));

	if ((strstr(passport, "@hotmail.") == NULL) &&
		(strstr(passport, "@live.com") == NULL) &&
		(strstr(passport, "@msn.com") == NULL))
		return;

	if (count++ < 26)
		return;

	count = 0;
	msn_cmdproc_send(cmdproc, "URL", "%s", "INBOX");
}


static void
fln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSlpLink *slplink;
	MsnUser *user;

	user = msn_userlist_find_user(cmdproc->session->userlist, cmd->params[0]);

	user->status = "offline";
	msn_user_update(user);

	slplink = msn_session_find_slplink(cmdproc->session, cmd->params[0]);

	if (slplink != NULL)
		msn_slplink_destroy(slplink);

}

static void
iln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;
	PurpleConnection *gc;
	MsnUser *user;
	MsnObject *msnobj;
	unsigned long clientid;
	int wlmclient;
	const char *state, *passport, *friendly;

	session = cmdproc->session;
	account = session->account;
	gc = purple_account_get_connection(account);

	state    = cmd->params[1];
	passport = cmd->params[2];
	/*if a contact is actually on the WLM part or the yahoo part*/
	wlmclient = atoi(cmd->params[3]);
	friendly = purple_url_decode(cmd->params[4]);

	user = msn_userlist_find_user(session->userlist, passport);

	serv_got_alias(gc, passport, friendly);

	msn_user_set_friendly_name(user, friendly);

	if (session->protocol_ver >= 9 && cmd->param_count == 8)
	{
		msnobj = msn_object_new_from_string(purple_url_decode(cmd->params[6]));
		msn_user_set_object(user, msnobj);
	}

	clientid = strtoul(cmd->params[5], NULL, 10);
	user->mobile = (clientid & MSN_CLIENT_CAP_MSNMOBILE) || (user->phone.mobile && user->phone.mobile[0] == '+');

	msn_user_set_state(user, state);
	msn_user_update(user);
}

static void
ipg_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload, size_t len)
{
	PurpleConnection *gc;
	MsnUserList *userlist;
	char *who = NULL, *text = NULL;
	xmlnode *payloadNode, *from, *textNode;

	purple_debug_misc("msn", "Incoming Page: {%s}\n", payload);

	userlist = cmdproc->session->userlist;
	gc = purple_account_get_connection(cmdproc->session->account);

	/* payload looks like this:
	   <?xml version="1.0"?>
	   <NOTIFICATION id="0" siteid="111100400" siteurl="http://mobile.msn.com/">
	     <TO name="passport@example.com">
	       <VIA agent="mobile"/>
	     </TO>
	     <FROM name="tel:+XXXXXXXXXXX"/>
		 <MSG pri="1" id="1">
		   <CAT Id="110110001"/>
		   <ACTION url="2wayIM.asp"/>
		   <SUBSCR url="2wayIM.asp"/>
		   <BODY lcid="1033">
		     <TEXT>Message was here</TEXT>
		   </BODY>
		 </MSG>
	   </NOTIFICATION>
	*/

	if (!(payloadNode = xmlnode_from_str(payload, len)) ||
		!(from = xmlnode_get_child(payloadNode, "FROM")) ||
		!(textNode = xmlnode_get_child(payloadNode, "MSG/BODY/TEXT")))
		return;

	who = g_strdup(xmlnode_get_attrib(from, "name"));
	if (!who) return;

	text = xmlnode_get_data(textNode);

	/* Match number to user's mobile number, FROM is a phone number if the
	   other side page you using your phone number */
	if(!strncmp(who, "tel:+", 5)) {
		MsnUser *user =
			msn_userlist_find_user_with_mobile_phone(userlist, who + 4);

		if(user && user->passport) {
			g_free(who);
			who = g_strdup(user->passport);
		}
	}

	serv_got_im(gc, who, text, 0, time(NULL));

	g_free(text);
	g_free(who);
	xmlnode_free(payloadNode);
}

static void
ipg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->servconn->payload_len = atoi(cmd->params[0]);
	cmdproc->last_cmd->payload_cb = ipg_cmd_post;
}

static void
nln_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;
	PurpleConnection *gc;
	MsnUser *user;
	MsnObject *msnobj;
	unsigned long clientid;
	int wlmclient;
	const char *state, *passport, *friendly, *old_friendly;

	session = cmdproc->session;
	account = session->account;
	gc = purple_account_get_connection(account);

	state    = cmd->params[0];
	passport = cmd->params[1];
	wlmclient = atoi(cmd->params[2]);
	friendly = purple_url_decode(cmd->params[3]);

	user = msn_userlist_find_user(session->userlist, passport);

	old_friendly = msn_user_get_friendly_name(user);
	if (!old_friendly || (old_friendly && (!friendly || strcmp(old_friendly, friendly))))
	{
		serv_got_alias(gc, passport, friendly);
		msn_user_set_friendly_name(user, friendly);
	}

	if (session->protocol_ver >= 9)
	{
		if (cmd->param_count == 7)
		{
			msnobj = msn_object_new_from_string(purple_url_decode(cmd->params[5]));
			msn_user_set_object(user, msnobj);
		}
		else
		{
			msn_user_set_object(user, NULL);
		}
	}

	clientid = strtoul(cmd->params[4], NULL, 10);
	user->mobile = (clientid & MSN_CLIENT_CAP_MSNMOBILE) || (user->phone.mobile && user->phone.mobile[0] == '+');

	msn_user_set_state(user, state);
	msn_user_update(user);
}

#if 0
static void
chg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	char *state = cmd->params[1];
	int state_id = 0;

	if (!strcmp(state, "NLN"))
		state_id = MSN_ONLINE;
	else if (!strcmp(state, "BSY"))
		state_id = MSN_BUSY;
	else if (!strcmp(state, "IDL"))
		state_id = MSN_IDLE;
	else if (!strcmp(state, "BRB"))
		state_id = MSN_BRB;
	else if (!strcmp(state, "AWY"))
		state_id = MSN_AWAY;
	else if (!strcmp(state, "PHN"))
		state_id = MSN_PHONE;
	else if (!strcmp(state, "LUN"))
		state_id = MSN_LUNCH;
	else if (!strcmp(state, "HDN"))
		state_id = MSN_HIDDEN;

	cmdproc->session->state = state_id;
}
#endif


static void
not_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload, size_t len)
{
#if 0
	MSN_SET_PARAMS("NOT %d\r\n%s", cmdproc->servconn->payload, payload);
	purple_debug_misc("msn", "Notification: {%s}\n", payload);
#endif
}

static void
not_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->servconn->payload_len = atoi(cmd->params[0]);
	cmdproc->last_cmd->payload_cb = not_cmd_post;
}

static void
rea_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;
	PurpleConnection *gc;
	const char *friendly;
	char *username;

	session = cmdproc->session;
	account = session->account;
	username = g_strdup(purple_normalize(account,
						purple_account_get_username(account)));

	/* Only set display name if our *own* friendly name changed! */
	if (strcmp(username, purple_normalize(account, cmd->params[2])))
	{
		g_free(username);
		return;
	}

	g_free(username);

	gc = account->gc;
	friendly = purple_url_decode(cmd->params[3]);

	purple_connection_set_display_name(gc, friendly);
}

static void
prp_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session = cmdproc->session;
	const char *type, *value, *friendlyname;

	g_return_if_fail(cmd->param_count >= 3);

	type  = cmd->params[2];

	if (cmd->param_count == 4)
	{
		value = cmd->params[3];
		if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(session->user, purple_url_decode(value));
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(session->user, purple_url_decode(value));
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(session->user, purple_url_decode(value));
	}
	else
	{
		if (!strcmp(type, "PHH"))
			msn_user_set_home_phone(session->user, NULL);
		else if (!strcmp(type, "PHW"))
			msn_user_set_work_phone(session->user, NULL);
		else if (!strcmp(type, "PHM"))
			msn_user_set_mobile_phone(session->user, NULL);
		else {
			type = cmd->params[1];
			if (!strcmp(type, "MFN")) {
				friendlyname = purple_url_decode(cmd->params[2]);
				
				msn_update_contact(session->contact, friendlyname);

				purple_connection_set_display_name(
					purple_account_get_connection(session->account),
					friendlyname);
			}
		}
	}
}

static void
reg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	const char *group_id, *group_name;

	session = cmdproc->session;
	group_id = cmd->params[2];
	group_name = purple_url_decode(cmd->params[3]);

	msn_userlist_rename_group_id(session->userlist, group_id, group_name);
}

static void
reg_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	const char * group_id;
	char **params;

	params = g_strsplit(trans->params, " ", 0);

	group_id = params[0];

	group_error_helper(cmdproc->session, _("Unable to rename group"), group_id, error);

	g_strfreev(params);
}

#if 0
static void
rem_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnUser *user;
	const char *group_id, *list, *passport;
	MsnListId list_id;

	session = cmdproc->session;
	list = cmd->params[1];
	passport = cmd->params[3];
	user = msn_userlist_find_user(session->userlist, passport);

	g_return_if_fail(user != NULL);

	list_id = msn_get_list_id(list);

	if (cmd->param_count == 5)
		group_id = cmd->params[4];
	else
		group_id = NULL;

	msn_got_rem_user(session, user, list_id, group_id);
	msn_user_update(user);
}
#endif

static void
rmg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	const char *group_id;

	session = cmdproc->session;
	group_id = cmd->params[2];

	msn_userlist_remove_group_id(session->userlist, group_id);
}

static void
rmg_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	const char *group_id;
	char **params;

	params = g_strsplit(trans->params, " ", 0);

	group_id = params[0];

	group_error_helper(cmdproc->session, _("Unable to delete group"), group_id, error);

	g_strfreev(params);
}

#if 0
static void
syn_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnSync *sync;
	int total_users;

	session = cmdproc->session;

	if (cmd->param_count == 2)
	{
		/*
		 * This can happen if we sent a SYN with an up-to-date
		 * buddy list revision, but we send 0 to get a full list.
		 * So, error out.
		 */

		msn_session_set_error(cmdproc->session, MSN_ERROR_BAD_BLIST, NULL);
		return;
	}

	total_users  = atoi(cmd->params[2]);

	sync = msn_sync_new(session);
	sync->total_users = total_users;
	sync->old_cbs_table = cmdproc->cbs_table;

	session->sync = sync;
	cmdproc->cbs_table = sync->cbs_table;
}
#endif

/**************************************************************************
 * Misc commands
 **************************************************************************/

static void
url_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;
	const char *rru;
	const char *url;
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	guchar digest[16];
	FILE *fd;
	char *buf;
	char buf2[3];
	char sendbuf[64];
	int i;

	session = cmdproc->session;
	account = session->account;

	rru = cmd->params[1];
	url = cmd->params[2];

	buf = g_strdup_printf("%s%lu%s",
			   session->passport_info.mspauth ? session->passport_info.mspauth : "BOGUS",
			   time(NULL) - session->passport_info.sl,
			   purple_connection_get_password(account->gc));

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);

	purple_cipher_context_append(context, (const guchar *)buf, strlen(buf));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	purple_cipher_context_destroy(context);

	g_free(buf);

	memset(sendbuf, 0, sizeof(sendbuf));

	for (i = 0; i < 16; i++)
	{
		g_snprintf(buf2, sizeof(buf2), "%02x", digest[i]);
		strcat(sendbuf, buf2);
	}

	if (session->passport_info.file != NULL)
	{
		g_unlink(session->passport_info.file);
		g_free(session->passport_info.file);
	}

	if ((fd = purple_mkstemp(&session->passport_info.file, FALSE)) == NULL)
	{
		purple_debug_error("msn",
						 "Error opening temp passport file: %s\n",
						 g_strerror(errno));
	}
	else
	{
#ifdef _WIN32
		fputs("<!-- saved from url=(0013)about:internet -->\n", fd);
#endif
		fputs("<html>\n"
			  "<head>\n"
			  "<noscript>\n"
			  "<meta http-equiv=\"Refresh\" content=\"0; "
			  "url=http://www.hotmail.com\">\n"
			  "</noscript>\n"
			  "</head>\n\n",
			  fd);

		fprintf(fd, "<body onload=\"document.pform.submit(); \">\n");
		fprintf(fd, "<form name=\"pform\" action=\"%s\" method=\"POST\">\n\n",
				url);
		fprintf(fd, "<input type=\"hidden\" name=\"mode\" value=\"ttl\">\n");
		fprintf(fd, "<input type=\"hidden\" name=\"login\" value=\"%s\">\n",
				purple_account_get_username(account));
		fprintf(fd, "<input type=\"hidden\" name=\"username\" value=\"%s\">\n",
				purple_account_get_username(account));
		if (session->passport_info.sid != NULL)
			fprintf(fd, "<input type=\"hidden\" name=\"sid\" value=\"%s\">\n",
					session->passport_info.sid);
		if (session->passport_info.kv != NULL)
			fprintf(fd, "<input type=\"hidden\" name=\"kv\" value=\"%s\">\n",
					session->passport_info.kv);
		fprintf(fd, "<input type=\"hidden\" name=\"id\" value=\"2\">\n");
		fprintf(fd, "<input type=\"hidden\" name=\"sl\" value=\"%ld\">\n",
				time(NULL) - session->passport_info.sl);
		fprintf(fd, "<input type=\"hidden\" name=\"rru\" value=\"%s\">\n",
				rru);
		if (session->passport_info.mspauth != NULL)
			fprintf(fd, "<input type=\"hidden\" name=\"auth\" value=\"%s\">\n",
					session->passport_info.mspauth);
		fprintf(fd, "<input type=\"hidden\" name=\"creds\" value=\"%s\">\n",
				sendbuf); /* TODO Digest me (huh? -- ChipX86) */
		fprintf(fd, "<input type=\"hidden\" name=\"svc\" value=\"mail\">\n");
		fprintf(fd, "<input type=\"hidden\" name=\"js\" value=\"yes\">\n");
		fprintf(fd, "</form></body>\n");
		fprintf(fd, "</html>\n");

		if (fclose(fd))
		{
			purple_debug_error("msn",
							 "Error closing temp passport file: %s\n",
							 g_strerror(errno));

			g_unlink(session->passport_info.file);
			g_free(session->passport_info.file);
			session->passport_info.file = NULL;
		}
#ifdef _WIN32
		else
		{
			/*
			 * Renaming file with .html extension, so that the
			 * win32 open_url will work.
			 */
			char *tmp;

			if ((tmp =
				g_strdup_printf("%s.html",
					session->passport_info.file)) != NULL)
			{
				if (g_rename(session->passport_info.file,
							tmp) == 0)
				{
					g_free(session->passport_info.file);
					session->passport_info.file = tmp;
				}
				else
					g_free(tmp);
			}
		}
#endif
	}
}
/**************************************************************************
 * Switchboards
 **************************************************************************/

static void
rng_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	const char *session_id;
	char *host;
	int port;

	session = cmdproc->session;
	session_id = cmd->params[0];

	msn_parse_socket(cmd->params[1], &host, &port);

	if (session->http_method)
		port = 80;

	swboard = msn_switchboard_new(session);

	msn_switchboard_set_invited(swboard, TRUE);
	msn_switchboard_set_session_id(swboard, cmd->params[0]);
	msn_switchboard_set_auth_key(swboard, cmd->params[3]);
	swboard->im_user = g_strdup(cmd->params[4]);
	/* msn_switchboard_add_user(swboard, cmd->params[4]); */

	if (!msn_switchboard_connect(swboard, host, port))
		msn_switchboard_destroy(swboard);

	g_free(host);
}

static void
xfr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	char *host;
	int port;

	if (strcmp(cmd->params[1], "SB") && strcmp(cmd->params[1], "NS"))
	{
		/* Maybe we can have a generic bad command error. */
		purple_debug_error("msn", "Bad XFR command (%s)\n", cmd->params[1]);
		return;
	}

	msn_parse_socket(cmd->params[2], &host, &port);

	if (!strcmp(cmd->params[1], "SB"))
	{
		purple_debug_error("msn", "This shouldn't be handled here.\n");
	}
	else if (!strcmp(cmd->params[1], "NS"))
	{
		MsnSession *session;

		session = cmdproc->session;

		msn_session_set_login_step(session, MSN_LOGIN_STEP_TRANSFER);

		msn_notification_connect(session->notification, host, port);
	}

	g_free(host);
}

static void
gcf_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
			 size_t len)
{
	xmlnode * root;
	gchar * buf;
	int xmllen;

	g_return_if_fail(cmd->payload != NULL);

	if ( (root = xmlnode_from_str(cmd->payload, cmd->payload_len)) == NULL)
	{
		purple_debug_error("MSN","Unable to parse GCF payload into a XML tree");
		return;
	}
	
	buf = xmlnode_to_formatted_str(root, &xmllen);

	/* get the payload content */
	purple_debug_info("MSNP14","GCF command payload:\n%.*s\n", xmllen, buf);
	
	g_free(buf);
	xmlnode_free(root);
}

static void
gcf_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_info("MSNP14","Processing GCF command\n");
	cmdproc->last_cmd->payload_cb  = gcf_cmd_post;
	return;
}

static void
sbs_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_info("MSNP14","Processing SBS... \n");
	if(cmd->payload_len == 0){
		return;
	}
	/*get the payload content*/
}

/*
 * Get the UBX's PSM info
 * Post it to the User status
 * Thanks for Chris <ukdrizzle@yahoo.co.uk>'s code
 */
static void
ubx_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload,
			 size_t len)
{
	MsnSession *session;
	PurpleAccount *account;
	MsnUser *user;
	const char *passport;
	char *psm_str, *str;
	CurrentMedia media = {NULL, NULL, NULL};

	session = cmdproc->session;
	account = session->account;

	passport = cmd->params[0];
	user = msn_userlist_find_user(session->userlist, passport);
	
	psm_str = msn_get_psm(cmd->payload,len);
	msn_user_set_statusline(user, psm_str);
	g_free(psm_str);

	str = msn_get_currentmedia(cmd->payload, len);
	if (msn_parse_currentmedia(str, &media))
		msn_user_set_currentmedia(user, &media);
	else
		msn_user_set_currentmedia(user, NULL);
	g_free(media.title);
	g_free(media.album);
	g_free(media.artist);
	g_free(str);

	msn_user_update(user);
}

static void
ubx_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_misc("MSNP14","UBX received.\n");
	if(cmd->payload_len == 0){
		return;
	}
	cmdproc->last_cmd->payload_cb  = ubx_cmd_post;
}

static void
uux_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	purple_debug_misc("MSNP14","UUX received.\n");
}

/**************************************************************************
 * Message Types
 **************************************************************************/

static void
profile_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	const char *value;
	const char *clLastChange;

	session = cmdproc->session;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	if ((value = msn_message_get_attr(msg, "kv")) != NULL)
	{
		g_free(session->passport_info.kv);
		session->passport_info.kv = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "sid")) != NULL)
	{
		g_free(session->passport_info.sid);
		session->passport_info.sid = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "MSPAuth")) != NULL)
	{
		g_free(session->passport_info.mspauth);
		session->passport_info.mspauth = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "ClientIP")) != NULL)
	{
		g_free(session->passport_info.client_ip);
		session->passport_info.client_ip = g_strdup(value);
	}

	if ((value = msn_message_get_attr(msg, "ClientPort")) != NULL)
	{
		session->passport_info.client_port = ntohs(atoi(value));
	}

	if ((value = msn_message_get_attr(msg, "LoginTime")) != NULL)
		session->passport_info.sl = atol(value);

	/*starting retrieve the contact list*/
	clLastChange = purple_account_get_string(session->account, "CLLastChange", NULL);
	session->contact = msn_contact_new(session);
#ifdef MSN_PARTIAL_LISTS
	/* msn_userlist_load defeats all attempts at trying to detect blist sync issues */
	msn_userlist_load(session);
	msn_get_contact_list(session->contact, MSN_PS_INITIAL, clLastChange);
#else
	/* always get the full list? */
	msn_get_contact_list(session->contact, MSN_PS_INITIAL, NULL);
#endif
#if 0
	msn_contact_connect(session->contact);
#endif
}

static void
initial_email_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	PurpleConnection *gc;
	GHashTable *table;
	const char *unread;

	session = cmdproc->session;
	gc = session->account->gc;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	if (session->passport_info.file == NULL)
	{
		MsnTransaction *trans;
		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_queue_cmd(trans, msg->cmd);

		msn_cmdproc_send_trans(cmdproc, trans);

		return;
	}

	if (!purple_account_get_check_mail(session->account))
		return;

	table = msn_message_get_hashtable_from_body(msg);

	unread = g_hash_table_lookup(table, "Inbox-Unread");

	if (unread != NULL)
	{
		int count = atoi(unread);

		if (count > 0)
		{
			const char *passport;
			const char *url;

			passport = msn_user_get_passport(session->user);
			url = session->passport_info.file;

			purple_notify_emails(gc, count, FALSE, NULL, NULL,
							   &passport, &url, NULL, NULL);
		}
	}

	g_hash_table_destroy(table);
}

/*offline Message notification process*/
static void
initial_mdata_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	PurpleConnection *gc;
	GHashTable *table;
	const char *mdata, *unread;

	session = cmdproc->session;
	gc = session->account->gc;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	/*new a oim session*/
//	session->oim = msn_oim_new(session);
//	msn_oim_connect(session->oim);

	table = msn_message_get_hashtable_from_body(msg);

	mdata = g_hash_table_lookup(table, "Mail-Data");

	if (mdata != NULL)
		msn_parse_oim_msg(session->oim, mdata);

	if (g_hash_table_lookup(table, "Inbox-URL") == NULL)
	{
		g_hash_table_destroy(table);
		return;
	}

	if (session->passport_info.file == NULL)
	{
		MsnTransaction *trans;
		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_queue_cmd(trans, msg->cmd);

		msn_cmdproc_send_trans(cmdproc, trans);

		g_hash_table_destroy(table);
		return;
	}

	if (!purple_account_get_check_mail(session->account))
	{
		g_hash_table_destroy(table);
		return;
	}

	unread = g_hash_table_lookup(table, "Inbox-Unread");

	if (unread != NULL)
	{
		int count = atoi(unread);

		if (count > 0)
		{
			const char *passport;
			const char *url;

			passport = msn_user_get_passport(session->user);
			url = session->passport_info.file;

			purple_notify_emails(gc, count, FALSE, NULL, NULL,
							   &passport, &url, NULL, NULL);
		}
	}

	g_hash_table_destroy(table);
}

/*offline Message Notification*/
static void
delete_oim_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	purple_debug_misc("MSN Notification","Delete OIM message.\n");
}

static void
email_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSession *session;
	PurpleConnection *gc;
	GHashTable *table;
	char *from, *subject, *tmp;

	session = cmdproc->session;
	gc = session->account->gc;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	if (session->passport_info.file == NULL)
	{
		MsnTransaction *trans;
		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_queue_cmd(trans, msg->cmd);

		msn_cmdproc_send_trans(cmdproc, trans);

		return;
	}

	if (!purple_account_get_check_mail(session->account))
		return;

	table = msn_message_get_hashtable_from_body(msg);

	from = subject = NULL;

	tmp = g_hash_table_lookup(table, "From");
	if (tmp != NULL)
		from = purple_mime_decode_field(tmp);

	tmp = g_hash_table_lookup(table, "Subject");
	if (tmp != NULL)
		subject = purple_mime_decode_field(tmp);

	purple_notify_email(gc,
					  (subject != NULL ? subject : ""),
					  (from != NULL ?  from : ""),
					  msn_user_get_passport(session->user),
					  session->passport_info.file, NULL, NULL);

	g_free(from);
	g_free(subject);

	g_hash_table_destroy(table);
}

static void
system_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	GHashTable *table;
	const char *type_s;

	if (strcmp(msg->remote_user, "Hotmail"))
		/* This isn't an official message. */
		return;

	table = msn_message_get_hashtable_from_body(msg);

	if ((type_s = g_hash_table_lookup(table, "Type")) != NULL)
	{
		int type = atoi(type_s);
		char buf[MSN_BUF_LEN];
		int minutes;

		switch (type)
		{
			case 1:
				minutes = atoi(g_hash_table_lookup(table, "Arg1"));
				g_snprintf(buf, sizeof(buf), dngettext(PACKAGE, 
							"The MSN server will shut down for maintenance "
							"in %d minute. You will automatically be "
							"signed out at that time.  Please finish any "
							"conversations in progress.\n\nAfter the "
							"maintenance has been completed, you will be "
							"able to successfully sign in.",
							"The MSN server will shut down for maintenance "
							"in %d minutes. You will automatically be "
							"signed out at that time.  Please finish any "
							"conversations in progress.\n\nAfter the "
							"maintenance has been completed, you will be "
							"able to successfully sign in.", minutes),
						minutes);
			default:
				break;
		}

		if (*buf != '\0')
			purple_notify_info(cmdproc->session->account->gc, NULL, buf, NULL);
	}

	g_hash_table_destroy(table);
}

void
msn_notification_add_buddy_to_list(MsnNotification *notification, MsnListId list_id,
						   	  const char *who)
{
	MsnCmdProc *cmdproc;
	MsnListOp list_op = 1 << list_id;
	xmlnode *adl_node;
	char *payload;
	int payload_len;

	cmdproc = notification->servconn->cmdproc;

	adl_node = xmlnode_new("ml");
	adl_node->child = NULL;

	msn_add_contact_xml(notification->session, adl_node, who, list_op, 
						MSN_USER_TYPE_PASSPORT);

	payload = xmlnode_to_str(adl_node,&payload_len);
	xmlnode_free(adl_node);
	
	msn_notification_post_adl(notification->servconn->cmdproc,
						payload,payload_len);
	g_free(payload);
}

void
msn_notification_rem_buddy_from_list(MsnNotification *notification, MsnListId list_id,
						   const char *who)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	MsnListOp list_op = 1 << list_id;
	xmlnode *rml_node;
	char *payload;
	int payload_len;

	cmdproc = notification->servconn->cmdproc;

	rml_node = xmlnode_new("ml");
	rml_node->child = NULL;

	msn_add_contact_xml(notification->session, rml_node, who, list_op, MSN_USER_TYPE_PASSPORT);

	payload = xmlnode_to_str(rml_node, &payload_len);
	xmlnode_free(rml_node);

	purple_debug_info("MSN Notification","Send RML with payload:\n%s\n", payload);
	trans = msn_transaction_new(cmdproc, "RML","%" G_GSIZE_FORMAT, strlen(payload));
	msn_transaction_set_payload(trans, payload, strlen(payload));
	msn_cmdproc_send_trans(cmdproc, trans);
	g_free(payload);
}

/**************************************************************************
 * Init
 **************************************************************************/
void
msn_notification_init(void)
{
	/* TODO: check prp, blp */

	cbs_table = msn_table_new();

	/* Synchronous */
	msn_table_add_cmd(cbs_table, "CHG", "CHG", NULL);
	msn_table_add_cmd(cbs_table, "CHG", "ILN", iln_cmd);
	msn_table_add_cmd(cbs_table, "ADL", "ILN", iln_cmd);
//	msn_table_add_cmd(cbs_table, "REM", "REM", rem_cmd);	/* Removed as of MSNP13 */
	msn_table_add_cmd(cbs_table, "USR", "USR", usr_cmd);
	msn_table_add_cmd(cbs_table, "USR", "XFR", xfr_cmd);
	msn_table_add_cmd(cbs_table, "USR", "GCF", gcf_cmd);
//	msn_table_add_cmd(cbs_table, "SYN", "SYN", syn_cmd);	/* Removed as of MSNP13 */
	msn_table_add_cmd(cbs_table, "CVR", "CVR", cvr_cmd);
	msn_table_add_cmd(cbs_table, "VER", "VER", ver_cmd);
	msn_table_add_cmd(cbs_table, "REA", "REA", rea_cmd);
	msn_table_add_cmd(cbs_table, "PRP", "PRP", prp_cmd);
	msn_table_add_cmd(cbs_table, "BLP", "BLP", blp_cmd);
//	msn_table_add_cmd(cbs_table, "BLP", "BLP", NULL);
	msn_table_add_cmd(cbs_table, "REG", "REG", reg_cmd);
	msn_table_add_cmd(cbs_table, "ADG", "ADG", adg_cmd);
	msn_table_add_cmd(cbs_table, "RMG", "RMG", rmg_cmd);
	msn_table_add_cmd(cbs_table, "XFR", "XFR", xfr_cmd);

	/* Asynchronous */
	msn_table_add_cmd(cbs_table, NULL, "IPG", ipg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "MSG", msg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "UBM", ubm_cmd);
	msn_table_add_cmd(cbs_table, NULL, "GCF", gcf_cmd);
	msn_table_add_cmd(cbs_table, NULL, "SBS", sbs_cmd);
	msn_table_add_cmd(cbs_table, NULL, "NOT", not_cmd);

	msn_table_add_cmd(cbs_table, NULL, "CHL", chl_cmd);
	msn_table_add_cmd(cbs_table, NULL, "RML", rml_cmd);
	msn_table_add_cmd(cbs_table, NULL, "ADL", adl_cmd);
	msn_table_add_cmd(cbs_table, NULL, "FQY", fqy_cmd);

	msn_table_add_cmd(cbs_table, NULL, "QRY", NULL);
	msn_table_add_cmd(cbs_table, NULL, "QNG", qng_cmd);
	msn_table_add_cmd(cbs_table, NULL, "FLN", fln_cmd);
	msn_table_add_cmd(cbs_table, NULL, "NLN", nln_cmd);
	msn_table_add_cmd(cbs_table, NULL, "ILN", iln_cmd);
	msn_table_add_cmd(cbs_table, NULL, "OUT", out_cmd);
	msn_table_add_cmd(cbs_table, NULL, "RNG", rng_cmd);

	msn_table_add_cmd(cbs_table, NULL, "UBX", ubx_cmd);
	msn_table_add_cmd(cbs_table, NULL, "UUX", uux_cmd);

	msn_table_add_cmd(cbs_table, NULL, "URL", url_cmd);

	msn_table_add_cmd(cbs_table, "fallback", "XFR", xfr_cmd);

	msn_table_add_error(cbs_table, "ADD", add_error);
	msn_table_add_error(cbs_table, "ADL", adl_error);
	msn_table_add_error(cbs_table, "REG", reg_error);
	msn_table_add_error(cbs_table, "RMG", rmg_error);
	/* msn_table_add_error(cbs_table, "REA", rea_error); */
	msn_table_add_error(cbs_table, "USR", usr_error);

	msn_table_add_msg_type(cbs_table,
						   "text/x-msmsgsprofile",
						   profile_msg);
	/*initial OIM notification*/
	msn_table_add_msg_type(cbs_table,
							"text/x-msmsgsinitialmdatanotification",
							initial_mdata_msg);	
	/*OIM notification when user online*/
	msn_table_add_msg_type(cbs_table,
							"text/x-msmsgsoimnotification",
							initial_mdata_msg);	
	msn_table_add_msg_type(cbs_table,
						   "text/x-msmsgsinitialemailnotification",
						   initial_email_msg);
	msn_table_add_msg_type(cbs_table,
						   "text/x-msmsgsemailnotification",
						   email_msg);
	/*delete an offline Message notification*/
	msn_table_add_msg_type(cbs_table,
							"text/x-msmsgsactivemailnotification",
						   delete_oim_msg);
	msn_table_add_msg_type(cbs_table,
						   "application/x-msmsgssystemmessage",
						   system_msg);
}

void
msn_notification_end(void)
{
	msn_table_destroy(cbs_table);
}

