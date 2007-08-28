/**
 * @file oim.c 
 * 	get and send MSN offline Instant Message via SOAP request
 *	Author
 * 		MaYuan<mayuan2006@gmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "msn.h"
#include "soap.h"
#include "oim.h"
#include "msnutils.h"

/*Local Function Prototype*/
static void msn_oim_post_single_get_msg(MsnOim *oim,const char *msgid);
static MsnOimSendReq *msn_oim_new_send_req(const char *from_member,
										   const char *friendname,
										   const char* to_member,
										   gint send_seq,
										   const char *msg);
static void msn_oim_retrieve_connect_init(MsnSoapConn *soapconn);
static void msn_oim_send_connect_init(MsnSoapConn *soapconn);
static void msn_oim_free_send_req(MsnOimSendReq *req);
static void msn_oim_report_to_user(MsnOim *oim, char *msg_str);
static void msn_oim_get_process(MsnOim *oim, char *oim_msg);
static char *msn_oim_msg_to_str(MsnOim *oim, const char *body);
static void msn_oim_send_process(MsnOim *oim, const char *body, int len);

/*new a OIM object*/
MsnOim *
msn_oim_new(MsnSession *session)
{
	MsnOim *oim;

	oim = g_new0(MsnOim, 1);
	oim->session = session;
	oim->retrieveconn = msn_soap_new(session,oim,1);
	
	oim->oim_list	= NULL;
	oim->sendconn = msn_soap_new(session,oim,1);
	oim->run_id = rand_guid();
	oim->challenge = NULL;
	oim->send_queue = g_queue_new();
	oim->send_seq = 1;
	return oim;
}

/*destroy the oim object*/
void
msn_oim_destroy(MsnOim *oim)
{
	MsnOimSendReq *request;
	
	purple_debug_info("OIM","destroy the OIM \n");
	msn_soap_destroy(oim->retrieveconn);
	msn_soap_destroy(oim->sendconn);
	g_free(oim->run_id);
	g_free(oim->challenge);
	
	while((request = g_queue_pop_head(oim->send_queue)) != NULL){
		msn_oim_free_send_req(request);
	}
	g_queue_free(oim->send_queue);
	
	g_free(oim);
}

static MsnOimSendReq *
msn_oim_new_send_req(const char *from_member, const char*friendname,
					 const char* to_member, gint send_seq,
					 const char *msg)
{
	MsnOimSendReq *request;
	
	request = g_new0(MsnOimSendReq, 1);
	request->from_member	=g_strdup(from_member);
	request->friendname		= g_strdup(friendname);
	request->to_member		= g_strdup(to_member);
	request->send_seq		= send_seq;
	request->oim_msg		= g_strdup(msg);
	return request;
}

static void
msn_oim_free_send_req(MsnOimSendReq *req)
{
	g_return_if_fail(req != NULL);

	g_free(req->from_member);
	g_free(req->friendname);
	g_free(req->to_member);
	g_free(req->oim_msg);
	
	g_free(req);
}

/****************************************
 * OIM send SOAP request
 * **************************************/
/*encode the message to OIM Message Format*/
static char *
msn_oim_msg_to_str(MsnOim *oim, const char *body)
{
	char *oim_body,*oim_base64;
	
	purple_debug_info("MSNP14","encode OIM Message...\n");	
	oim_base64 = purple_base64_encode((const guchar *)body, strlen(body));
	purple_debug_info("MSNP14","encoded base64 body:{%s}\n",oim_base64);	
	oim_body = g_strdup_printf(MSN_OIM_MSG_TEMPLATE,
				oim->run_id,oim->send_seq,oim_base64);

	return oim_body;
}

/*oim SOAP server login error*/
static void
msn_oim_send_error_cb(PurpleSslConnection *gsc, PurpleSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to OIM server"));
}

/*msn oim SOAP server connect process*/
static void
msn_oim_send_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnOim *oim;

	oim = soapconn->parent;
	g_return_if_fail(oim != NULL);

	session = oim->session;
	g_return_if_fail(session != NULL);
}

/*
 * Process the send return SOAP string
 * If got SOAP Fault,get the lock key,and resend it.
 */
static void
msn_oim_send_process(MsnOim *oim, const char *body, int len)
{
	xmlnode *responseNode, *bodyNode;
	xmlnode	*faultNode, *faultCodeNode, *faultstringNode;
	xmlnode *detailNode, *challengeNode;
	char *faultCodeStr, *faultstring;

	responseNode = xmlnode_from_str(body,len);
	g_return_if_fail(responseNode != NULL);
	bodyNode = xmlnode_get_child(responseNode,"Body");
	faultNode = xmlnode_get_child(bodyNode,"Fault");
	if(faultNode == NULL){
		/*Send OK! return*/
		MsnOimSendReq *request;
		
		purple_debug_info("MSNP14","send OIM OK!");
		xmlnode_free(responseNode);
		request = g_queue_pop_head(oim->send_queue);
		msn_oim_free_send_req(request);
		/*send next buffered Offline Message*/
		msn_soap_post(oim->sendconn,NULL,msn_oim_send_connect_init);
		return;
	}
	/*get the challenge,and repost it*/
	faultCodeNode = xmlnode_get_child(faultNode,"faultcode");
	if(faultCodeNode == NULL){
		purple_debug_info("MSNP14","faultcode Node is NULL\n");
		goto oim_send_process_fail;
	}
	faultCodeStr = xmlnode_get_data(faultCodeNode);
	purple_debug_info("MSNP14","fault code:{%s}\n",faultCodeStr);

	if(!strcmp(faultCodeStr,"q0:AuthenticationFailed")){
		/*other Fault Reason?*/
		goto oim_send_process_fail;
	}

	faultstringNode = xmlnode_get_child(faultNode,"faultstring");
	faultstring = xmlnode_get_data(faultstringNode);
	purple_debug_info("MSNP14","fault string :{%s}\n",faultstring);

	/* lock key fault reason,
	 * compute the challenge and resend it
	 */
	detailNode = xmlnode_get_child(faultNode, "detail");
	if(detailNode == NULL){
		goto oim_send_process_fail;
	}
	challengeNode = xmlnode_get_child(detailNode,"LockKeyChallenge");
	if (challengeNode == NULL) {
		goto oim_send_process_fail;
	}

	g_free(oim->challenge);
	oim->challenge = xmlnode_get_data(challengeNode);
	purple_debug_info("MSNP14","lockkey:{%s}\n",oim->challenge);

	/*repost the send*/
	purple_debug_info("MSNP14","prepare to repost the send...\n");
	msn_oim_send_msg(oim);

oim_send_process_fail:
	xmlnode_free(responseNode);
	return ;
}

static void
msn_oim_send_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnSession *session = soapconn->session;
	MsnOim * oim;

	g_return_if_fail(session != NULL);
	oim = soapconn->session->oim;
	g_return_if_fail(oim != NULL);

	purple_debug_info("MSNP14","read buffer:{%s}\n",soapconn->body);
	msn_oim_send_process(oim,soapconn->body,soapconn->body_len);
}

static void
msn_oim_send_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	soapconn->read_cb = msn_oim_send_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

void
msn_oim_prep_send_msg_info(MsnOim *oim, const char *membername,
						   const char* friendname, const char *tomember,
						   const char * msg)
{
	MsnOimSendReq *request;

	g_return_if_fail(oim != NULL);

	request = msn_oim_new_send_req(membername,friendname,tomember,oim->send_seq,msg);
	g_queue_push_tail(oim->send_queue,request);
}

/*post send single message request to oim server*/
void 
msn_oim_send_msg(MsnOim *oim)
{
	MsnSoapReq *soap_request;
	MsnOimSendReq *oim_request;
	char *soap_body,*mspauth;
	char *msg_body;
	char buf[33];

	g_return_if_fail(oim != NULL);
	oim_request = g_queue_pop_head(oim->send_queue);
	g_return_if_fail(oim_request != NULL);

	purple_debug_info("MSNP14","send single OIM Message\n");
	mspauth = g_strdup_printf("t=%s&amp;p=%s",
		oim->session->passport_info.t,
		oim->session->passport_info.p
		);
	g_queue_push_head(oim->send_queue,oim_request);

	/* if we got the challenge lock key, we compute it
	 * else we go for the SOAP fault and resend it.
	 */
	if(oim->challenge != NULL){
		msn_handle_chl(oim->challenge, buf);
	}else{
		purple_debug_info("MSNP14","no lock key challenge,wait for SOAP Fault and Resend\n");
		buf[0]='\0';
	}
	purple_debug_info("MSNP14","get the lock key challenge {%s}\n",buf);	

	msg_body = msn_oim_msg_to_str(oim, oim_request->oim_msg);
	soap_body = g_strdup_printf(MSN_OIM_SEND_TEMPLATE,
					oim_request->from_member,
					oim_request->friendname,
					oim_request->to_member,
					mspauth,
					MSNP13_WLM_PRODUCT_ID,
					buf,
					oim_request->send_seq,
					msg_body
					);
	soap_request = msn_soap_request_new(MSN_OIM_SEND_HOST,
					MSN_OIM_SEND_URL,
					MSN_OIM_SEND_SOAP_ACTION,
					soap_body,
					NULL,
					msn_oim_send_read_cb,
					msn_oim_send_written_cb);
	g_free(mspauth);
	g_free(msg_body);
	g_free(soap_body);

	/*increase the offline Sequence control*/
	if(oim->challenge != NULL){
		oim->send_seq++;
	}
	msn_soap_post(oim->sendconn,soap_request,msn_oim_send_connect_init);
}

/****************************************
 * OIM delete SOAP request
 * **************************************/
static void
msn_oim_delete_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	purple_debug_info("MSNP14","OIM delete read buffer:{%s}\n",soapconn->body);

	msn_soap_free_read_buf(soapconn);
	/*get next single Offline Message*/
	msn_soap_post(soapconn,NULL,msn_oim_retrieve_connect_init);
}

static void
msn_oim_delete_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	soapconn->read_cb = msn_oim_delete_read_cb;
}

/*Post to get the Offline Instant Message*/
static void
msn_oim_post_delete_msg(MsnOim *oim,const char *msgid)
{
	MsnSoapReq *soap_request;
	const char *soap_body,*t,*p;

	g_return_if_fail(oim != NULL);
	g_return_if_fail(msgid != NULL);

	purple_debug_info("MSNP14","Delete single OIM Message {%s}\n",msgid);
	t = oim->session->passport_info.t;
	p = oim->session->passport_info.p;

	soap_body = g_strdup_printf(MSN_OIM_DEL_TEMPLATE,
					t,
					p,
					msgid
					);
	soap_request = msn_soap_request_new(MSN_OIM_RETRIEVE_HOST,
					MSN_OIM_RETRIEVE_URL,
					MSN_OIM_DEL_SOAP_ACTION,
					soap_body,
					NULL,
					msn_oim_delete_read_cb,
					msn_oim_delete_written_cb);
	msn_soap_post(oim->retrieveconn,soap_request,msn_oim_retrieve_connect_init);
}

/****************************************
 * OIM get SOAP request
 * **************************************/
/*oim SOAP server login error*/
static void
msn_oim_get_error_cb(PurpleSslConnection *gsc, PurpleSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);
	msn_soap_clean_unhandled_request(soapconn);

//	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to OIM server"));
}

/*msn oim SOAP server connect process*/
static void
msn_oim_get_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnOim *oim;

	oim = soapconn->parent;
	g_return_if_fail(oim != NULL);

	session = oim->session;
	g_return_if_fail(session != NULL);

	purple_debug_info("MSNP14","oim get SOAP Server connected!\n");
}

/*Post the Offline Instant Message to User Conversation*/
static void
msn_oim_report_to_user(MsnOim *oim, char *msg_str)
{
	MsnMessage *message;
	char *date,*from,*decode_msg;
	gsize body_len;
	char **tokens;
	char *start,*end;
	int has_nick = 0;
	char *passport_str, *passport;
	char *msg_id;
	time_t stamp;

	message = msn_message_new(MSN_MSG_UNKNOWN);

	msn_message_parse_payload(message, msg_str, strlen(msg_str),
							  MSG_OIM_LINE_DEM, MSG_OIM_BODY_DEM);
	purple_debug_info("MSNP14","oim body:{%s}\n",message->body);
	decode_msg = (char *)purple_base64_decode(message->body,&body_len);
	date =	(char *)g_hash_table_lookup(message->attr_table, "Date");
	from =	(char *)g_hash_table_lookup(message->attr_table, "From");
	if(strstr(from," ")){
		has_nick = 1;
	}
	if(has_nick){
		tokens = g_strsplit(from , " " , 2);
		passport_str = g_strdup(tokens[1]);
		purple_debug_info("MSNP14","oim Date:{%s},nickname:{%s},tokens[1]:{%s} passport{%s}\n",
							date,tokens[0],tokens[1],passport_str);
		g_strfreev(tokens);
	}else{
		passport_str = g_strdup(from);
		purple_debug_info("MSNP14","oim Date:{%s},passport{%s}\n",
					date,passport_str);
	}
	start = strstr(passport_str,"<");
	start += 1;
	end = strstr(passport_str,">");
	passport = g_strndup(start,end - start);
	g_free(passport_str);
	purple_debug_info("MSNP14","oim Date:{%s},passport{%s}\n",date,passport);

	stamp = purple_str_to_time(date, TRUE, NULL, NULL, NULL);

	serv_got_im(oim->session->account->gc, passport, decode_msg, 0, stamp);

	/*Now get the oim message ID from the oim_list.
	 * and append to read list to prepare for deleting the Offline Message when sign out
	 */
	if(oim->oim_list != NULL){
		msg_id = oim->oim_list->data;
		msn_oim_post_delete_msg(oim,msg_id);
		oim->oim_list = g_list_remove(oim->oim_list, oim->oim_list->data);
		g_free(msg_id);
	}

	g_free(passport);
}

/* Parse the XML data,
 * prepare to report the OIM to user
 */
static void
msn_oim_get_process(MsnOim *oim, char *oim_msg)
{
	xmlnode *oimNode,*bodyNode,*responseNode,*msgNode;
	char *msg_data,*msg_str;

	oimNode = xmlnode_from_str(oim_msg, strlen(oim_msg));
	bodyNode = xmlnode_get_child(oimNode,"Body");
	responseNode = xmlnode_get_child(bodyNode,"GetMessageResponse");
	msgNode = xmlnode_get_child(responseNode,"GetMessageResult");
	msg_data = xmlnode_get_data(msgNode);
	msg_str = g_strdup(msg_data);
	g_free(msg_data);
	purple_debug_info("OIM","msg:{%s}\n",msg_str);
	msn_oim_report_to_user(oim,msg_str);

	g_free(msg_str);
}

static void
msn_oim_get_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;
	MsnOim * oim = soapconn->session->oim;

	purple_debug_info("MSNP14","OIM get read buffer:{%s}\n",soapconn->body);

	/*we need to process the read message!*/
	msn_oim_get_process(oim,soapconn->body);
	msn_soap_free_read_buf(soapconn);

	/*get next single Offline Message*/
	msn_soap_post(soapconn,NULL,msn_oim_retrieve_connect_init);
}

static void
msn_oim_get_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;

	soapconn->read_cb = msn_oim_get_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/* parse the oim XML data 
 * and post it to the soap server to get the Offline Message
 * */
void
msn_parse_oim_msg(MsnOim *oim,const char *xmlmsg)
{
	xmlnode *node, *mdNode,*mNode,*ENode,*INode,*rtNode,*nNode;
	char *passport,*msgid,*nickname, *unread, *rTime = NULL;
	MsnSession *session = oim->session;

	node = xmlnode_from_str(xmlmsg, strlen(xmlmsg));

	ENode = xmlnode_get_child(node, "E");
	INode = xmlnode_get_child(ENode, "IU");
	unread = xmlnode_get_data(INode);

	if (unread != NULL && purple_account_get_check_mail(session->account))
	{
		int count = atoi(unread);

		if (count > 0)
		{
			const char *passport;
			const char *url;

			passport = msn_user_get_passport(session->user);
			url = session->passport_info.file;

			purple_notify_emails(session->account->gc, atoi(unread), FALSE, NULL, NULL,
					&passport, &url, NULL, NULL);
		}
	}

	mdNode = xmlnode_get_child(node, "MD");
	if (mdNode == NULL) {
		xmlnode_free(node);
		return;
	}
	for(mNode = xmlnode_get_child(mdNode, "M"); mNode;
					mNode = xmlnode_get_next_twin(mNode)){
		/*email Node*/
		ENode = xmlnode_get_child(mNode,"E");
		passport = xmlnode_get_data(ENode);
		/*Index */
		INode = xmlnode_get_child(mNode,"I");
		msgid = xmlnode_get_data(INode);
		/*Nickname*/
		nNode = xmlnode_get_child(mNode,"N");
		nickname = xmlnode_get_data(nNode);
		/*receive time*/
		rtNode = xmlnode_get_child(mNode,"RT");
		if(rtNode != NULL) {
			rTime = xmlnode_get_data(rtNode);
			rtNode = NULL;
		}
/*		purple_debug_info("MSNP14","E:{%s},I:{%s},rTime:{%s}\n",passport,msgid,rTime); */

		oim->oim_list = g_list_append(oim->oim_list,msgid);
		msn_oim_post_single_get_msg(oim,msgid);
		g_free(passport);
		g_free(msgid);
		g_free(rTime);
		rTime = NULL;
		g_free(nickname);
	}
	xmlnode_free(node);
}

/*Post to get the Offline Instant Message*/
static void
msn_oim_post_single_get_msg(MsnOim *oim,const char *msgid)
{
	MsnSoapReq *soap_request;
	const char *soap_body,*t,*p;

	purple_debug_info("MSNP14","Get single OIM Message\n");
	t = oim->session->passport_info.t;
	p = oim->session->passport_info.p;

	soap_body = g_strdup_printf(MSN_OIM_GET_TEMPLATE,
					t,
					p,
					msgid
					);
	soap_request = msn_soap_request_new(MSN_OIM_RETRIEVE_HOST,
					MSN_OIM_RETRIEVE_URL,
					MSN_OIM_GET_SOAP_ACTION,
					soap_body,
					NULL,
					msn_oim_get_read_cb,
					msn_oim_get_written_cb);
	msn_soap_post(oim->retrieveconn,soap_request,msn_oim_retrieve_connect_init);
}

/*msn oim retrieve server connect init */
static void
msn_oim_retrieve_connect_init(MsnSoapConn *soapconn)
{
	purple_debug_info("MSNP14","msn_oim_connect...\n");
	msn_soap_init(soapconn,MSN_OIM_RETRIEVE_HOST,1,
					msn_oim_get_connect_cb,
					msn_oim_get_error_cb);
}

/*Msn OIM Send Server Connect Init Function*/
static void
msn_oim_send_connect_init(MsnSoapConn *sendconn)
{
	purple_debug_info("MSNP14","msn oim send connect init...\n");
	msn_soap_init(sendconn,MSN_OIM_SEND_HOST,1,
					msn_oim_send_connect_cb,
					msn_oim_send_error_cb);
}

/*endof oim.c*/
