/**
 * @file contact.c 
 * 	get MSN contacts via SOAP request
 *	created by MaYuan<mayuan2006@gmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "msn.h"
#include "soap.h"
#include "contact.h"
#include "xmlnode.h"
#include "group.h"

/*define This to debug the Contact Server*/
#undef  MSN_CONTACT_SOAP_DEBUG

/*new a contact*/
MsnContact *
msn_contact_new(MsnSession *session)
{
	MsnContact *contact;

	contact = g_new0(MsnContact, 1);
	contact->session = session;
	contact->soapconn = msn_soap_new(session,contact,1);

	return contact;
}

/*destroy the contact*/
void
msn_contact_destroy(MsnContact *contact)
{
	msn_soap_destroy(contact->soapconn);
	g_free(contact);
}

/*contact SOAP server login error*/
static void
msn_contact_login_error_cb(PurpleSslConnection *gsc, PurpleSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to contact server"));
}

/*msn contact SOAP server connect process*/
static void
msn_contact_login_connect_cb(gpointer data, PurpleSslConnection *gsc,
				 PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnContact *contact;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);

	session = contact->session;
	g_return_if_fail(session != NULL);

	/*login ok!We can retrieve the contact list*/
//	msn_get_contact_list(contact,NULL);
}

/*get MSN member role utility*/
static int
msn_get_memberrole(char * role)
{
	if(!strcmp(role,"Allow")){
		return MSN_LIST_AL_OP;
	}else if(!strcmp(role,"Block")){
		return MSN_LIST_BL_OP;
	}else if(!strcmp(role,"Reverse")){
		return MSN_LIST_RL_OP;
	}
	return 0;
}

/*get User Type*/
static int 
msn_get_user_type(char * type)
{
	if(!strcmp(type,"Regular")){
		return 1;
	}
	if(!strcmp(type,"Live")){
		return 1;
	}
	if(!strcmp(type,"LivePending")){
		return 1;
	}

	return 0;
}

/*parse contact list*/
static void
msn_parse_contact_list(MsnContact * contact)
{
	MsnSession * session;
	int list_op =0;
	char * passport;
	xmlnode * node,*body,*response,*result,*services;
	xmlnode *service,*memberships;
	xmlnode *LastChangeNode;
	xmlnode *membershipnode,*members,*member,*passportNode;
	char *LastChangeStr;

	session = contact->session;
	purple_debug_misc("MSNCL","parse contact list:{%s}\nsize:%d\n",contact->soapconn->body,contact->soapconn->body_len);
	node = 	xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);

	if(node == NULL){
		purple_debug_misc("MSNCL","parse contact from str err!\n");
		return;
	}
	purple_debug_misc("MSNCL","node{%p},name:%s,child:%s,last:%s\n",node,node->name,node->child->name,node->lastchild->name);
	body = xmlnode_get_child(node,"Body");
	purple_debug_misc("MSNCL","body{%p},name:%s\n",body,body->name);
	response = xmlnode_get_child(body,"FindMembershipResponse");

	if (response == NULL) {
		/* we may get a response if our cache data is too old:
		 *
		 * <faultstring>Need to do full sync. Can't sync deltas Client
		 * has too old a copy for us to do a delta sync</faultstring>
		 */
		msn_get_contact_list(contact, NULL);
		return;
	}

	purple_debug_misc("MSNCL","response{%p},name:%s\n",response,response->name);
	result =xmlnode_get_child(response,"FindMembershipResult");
	if(result == NULL){
		purple_debug_misc("MSNCL","receive No Update!\n");
		return;
	}
	purple_debug_misc("MSNCL","result{%p},name:%s\n",result,result->name);
	services =xmlnode_get_child(result,"Services");
	purple_debug_misc("MSNCL","services{%p},name:%s\n",services,services->name);
	service =xmlnode_get_child(services,"Service");
	purple_debug_misc("MSNCL","service{%p},name:%s\n",service,service->name);
	
	/*Last Change Node*/
	LastChangeNode = xmlnode_get_child(service,"LastChange");
	LastChangeStr = xmlnode_get_data(LastChangeNode);
	purple_debug_misc("MSNCL","LastChangeNode0 %s\n",LastChangeStr);	
	purple_blist_node_set_string(msn_session_get_bnode(contact->session),"CLLastChange",LastChangeStr);
	purple_debug_misc("MSNCL","LastChangeNode %s\n",LastChangeStr);
	
	memberships =xmlnode_get_child(service,"Memberships");
	purple_debug_misc("MSNCL","memberships{%p},name:%s\n",memberships,memberships->name);
	for(membershipnode = xmlnode_get_child(memberships, "Membership"); membershipnode;
					membershipnode = xmlnode_get_next_twin(membershipnode)){
		xmlnode *roleNode;
		char *role;
		roleNode = xmlnode_get_child(membershipnode,"MemberRole");
		role=xmlnode_get_data(roleNode);
		list_op = msn_get_memberrole(role);
		purple_debug_misc("MSNCL","MemberRole role:%s,list_op:%d\n",role,list_op);
		g_free(role);
		members = xmlnode_get_child(membershipnode,"Members");
		for(member = xmlnode_get_child(members, "Member"); member;
				member = xmlnode_get_next_twin(member)){
			MsnUser *user;
			xmlnode * typeNode;
			char * type;

			purple_debug_misc("MSNCL","type:%s\n",xmlnode_get_attrib(member,"type"));
			if(!g_strcasecmp(xmlnode_get_attrib(member,"type"),"PassportMember")){
				passportNode = xmlnode_get_child(member,"PassportName");
				passport = xmlnode_get_data(passportNode);
				typeNode = xmlnode_get_child(member,"Type");
				type = xmlnode_get_data(typeNode);
				purple_debug_misc("MSNCL","Passport name:%s,type:%s\n",passport,type);
				g_free(type);

				user = msn_userlist_find_add_user(session->userlist,passport,NULL);
				msn_got_lst_user(session, user, list_op, NULL);
				g_free(passport);
			}
			if(!g_strcasecmp(xmlnode_get_attrib(member,"type"),"PhoneMember")){
			}
			if(!g_strcasecmp(xmlnode_get_attrib(member,"type"),"EmailMember")){
				xmlnode *emailNode;

				emailNode = xmlnode_get_child(member,"Email");
				passport = xmlnode_get_data(emailNode);
				purple_debug_info("MSNCL","Email Member :name:%s,list_op:%d\n",passport,list_op);
				user = msn_userlist_find_add_user(session->userlist,passport,NULL);
				msn_got_lst_user(session,user,list_op,NULL);
				g_free(passport);
			}
		}
	}

	xmlnode_free(node);
}

static void
msn_get_contact_list_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn *soapconn = data;	
	MsnContact *contact;
	MsnSession *session;
	const char *abLastChange;
	const char *dynamicItemLastChange;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);

#ifdef  MSN_CONTACT_SOAP_DEBUG
	purple_debug_misc("msn", "soap contact server Reply: {%s}\n", soapconn->read_buf);
#endif
	msn_parse_contact_list(contact);
	/*free the read buffer*/
	msn_soap_free_read_buf(soapconn);

	abLastChange = purple_blist_node_get_string(msn_session_get_bnode(contact->session),"ablastChange");
	dynamicItemLastChange = purple_blist_node_get_string(msn_session_get_bnode(contact->session),"dynamicItemLastChange");
	msn_get_address_book(contact, abLastChange, dynamicItemLastChange);
}

static void
msn_get_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish contact written\n");
	soapconn->read_cb = msn_get_contact_list_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*SOAP  get contact list*/
void
msn_get_contact_list(MsnContact * contact, const char *update_time)
{
	MsnSoapReq *soap_request;
	char *body = NULL;
	char * update_str;
	
	purple_debug_info("MaYuan","Getting Contact List...\n");
	if(update_time != NULL){
		purple_debug_info("MSNCL","last update time:{%s}\n",update_time);
		update_str = g_strdup_printf(MSN_GET_CONTACT_UPDATE_XML,update_time);
	}else{
		update_str = g_strdup("");
	}
	body = g_strdup_printf(MSN_GET_CONTACT_TEMPLATE, update_str);
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_GET_CONTACT_POST_URL,MSN_GET_CONTACT_SOAP_ACTION,
					body,
					msn_get_contact_list_cb,
					msn_get_contact_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	g_free(update_str);
	g_free(body);
}

static gboolean
msn_parse_addressbook(MsnContact * contact)
{
	MsnSession * session;
	xmlnode * node,*body,*response,*result;
	xmlnode *groups,*group,*groupname,*groupId,*groupInfo;
	xmlnode	*contacts,*contactNode,*contactId,*contactInfo,*contactType,*passportName,*displayName,*groupIds,*guid;
	xmlnode *abNode;
	char *group_name,*group_id;

	session = contact->session;
	purple_debug_misc("xml","parse addressbook:{%s}\nsize:%d\n",contact->soapconn->body,contact->soapconn->body_len);
	node = 	xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);

	if(node == NULL){
		purple_debug_misc("xml","parse from str err!\n");
		return FALSE;
	}
	purple_debug_misc("xml","node{%p},name:%s,child:%s,last:%s\n",node,node->name,node->child->name,node->lastchild->name);
	body = xmlnode_get_child(node,"Body");
	purple_debug_misc("xml","body{%p},name:%s\n",body,body->name);
	response = xmlnode_get_child(body,"ABFindAllResponse");

	if (response == NULL) {
		return FALSE;
	}

	purple_debug_misc("xml","response{%p},name:%s\n",response,response->name);
	result =xmlnode_get_child(response,"ABFindAllResult");
	if(result == NULL){
		purple_debug_misc("MSNAB","receive no address book update\n");
		return TRUE;
	}
	purple_debug_misc("xml","result{%p},name:%s\n",result,result->name);

	/*Process Group List*/
	groups =xmlnode_get_child(result,"groups");
	for(group = xmlnode_get_child(groups, "Group"); group;
					group = xmlnode_get_next_twin(group)){
		groupId = xmlnode_get_child(group,"groupId");
		group_id = xmlnode_get_data(groupId);
		groupInfo = xmlnode_get_child(group,"groupInfo");
		groupname = xmlnode_get_child(groupInfo,"name");
		group_name = xmlnode_get_data(groupname);

		msn_group_new(session->userlist, group_id, group_name);

		if (group_id == NULL){
			/* Group of ungroupped buddies */
			continue;
		}

		purple_debug_misc("MsnAB","group_id:%s name:%s\n",group_id,group_name);
		if ((purple_find_group(group_name)) == NULL){
			PurpleGroup *g = purple_group_new(group_name);
			purple_blist_node_set_string(&(g->node),"groupId",group_id);
			purple_blist_add_group(g, NULL);
		}
		g_free(group_id);
		g_free(group_name);
	}
	/*add a default No group to set up the no group Membership*/
	group_id = g_strdup(MSN_INDIVIDUALS_GROUP_ID);
	group_name = g_strdup(MSN_INDIVIDUALS_GROUP_NAME);
	msn_group_new(session->userlist,group_id , group_name);
	if (group_id != NULL){
		purple_debug_misc("MsnAB","group_id:%s name:%s,value:%d\n",group_id,group_name,*group_name=='\0');
		if ((purple_find_group(group_name)) == NULL){
			PurpleGroup *g = purple_group_new(group_name);
			purple_blist_add_group(g, NULL);
		}
	}
	g_free(group_name);
	g_free(group_id);

	/*add a default No group to set up the no group Membership*/
	group_id = g_strdup(MSN_NON_IM_GROUP_ID);
	group_name = g_strdup(MSN_NON_IM_GROUP_NAME);
	msn_group_new(session->userlist,group_id , group_name);
	if (group_id != NULL){
		purple_debug_misc("MsnAB","group_id:%s name:%s,value:%d\n",group_id,group_name,*group_name=='\0');
		if ((purple_find_group(group_name)) == NULL){
			PurpleGroup *g = purple_group_new(group_name);
			purple_blist_add_group(g, NULL);
		}
	}
	g_free(group_name);
	g_free(group_id);

	/*Process contact List*/
	purple_debug_info("MSNAB","process contact list...\n");
	contacts =xmlnode_get_child(result,"contacts");
	for(contactNode = xmlnode_get_child(contacts, "Contact"); contactNode;
				contactNode = xmlnode_get_next_twin(contactNode)){
		MsnUser *user;
		char *passport,*Name,*uid,*type;

		passport = NULL;

		contactId= xmlnode_get_child(contactNode,"contactId");
		uid = xmlnode_get_data(contactId);

		contactInfo = xmlnode_get_child(contactNode,"contactInfo");
		contactType = xmlnode_get_child(contactInfo,"contactType");
		type = xmlnode_get_data(contactType);

		/*setup the Display Name*/
		if (!strcmp(type, "Me")){
			char *friendly;
			friendly = xmlnode_get_data(xmlnode_get_child(contactInfo, "displayName"));
			purple_connection_set_display_name(session->account->gc, purple_url_decode(friendly));
			g_free(friendly);
			continue; /* Not adding own account as buddy to buddylist */
		}

		passportName = xmlnode_get_child(contactInfo,"passportName");
		if(passportName == NULL){
			xmlnode *emailsNode, *contactEmailNode, *emailNode;
			xmlnode *messengerEnabledNode;
			char *msnEnabled;

			/*TODO: add it to the none-instant Messenger group and recognize as email Membership*/
			/*Yahoo User?*/
			emailsNode = xmlnode_get_child(contactInfo,"emails");
			if(emailsNode == NULL){
				/*TODO:  need to support the Mobile type*/
				continue;
			}
			for(contactEmailNode = xmlnode_get_child(emailsNode,"ContactEmail");contactEmailNode;
					contactEmailNode = xmlnode_get_next_twin(contactEmailNode) ){
				messengerEnabledNode = xmlnode_get_child(contactEmailNode,"isMessengerEnabled");
				if(messengerEnabledNode == NULL){
					break;
				}
				msnEnabled = xmlnode_get_data(messengerEnabledNode);
				if(!strcmp(msnEnabled,"true")){
					/*Messenger enabled, Get the Passport*/
					emailNode = xmlnode_get_child(contactEmailNode,"email");
					passport = xmlnode_get_data(emailNode);
					purple_debug_info("MsnAB","Yahoo User %s\n",passport);
					break;
				}else{
					/*TODO maybe we can just ignore it in Purple?*/
					emailNode = xmlnode_get_child(contactEmailNode,"email");
					passport = xmlnode_get_data(emailNode);
					purple_debug_info("MSNAB","Other type user\n");
				}
				g_free(msnEnabled);
			}
		}else{
			passport = xmlnode_get_data(passportName);
		}

		if(passport == NULL){
			continue;
		}

		displayName = xmlnode_get_child(contactInfo,"displayName");
		if(displayName == NULL){
			Name = g_strdup(passport);
		}else{
			Name =xmlnode_get_data(displayName);	
		}

		purple_debug_misc("MsnAB","passport:{%s} uid:{%s} display:{%s}\n",
						passport,uid,Name);

		user = msn_userlist_find_add_user(session->userlist, passport,Name);
		msn_user_set_uid(user,uid);
		msn_user_set_type(user,msn_get_user_type(type));
		user->list_op |= MSN_LIST_FL_OP;
		g_free(Name);
		g_free(passport);
		g_free(uid);
		g_free(type);

		purple_debug_misc("MsnAB","parse guid...\n");
		groupIds = xmlnode_get_child(contactInfo,"groupIds");
		if(groupIds){
			for(guid = xmlnode_get_child(groupIds, "guid");guid;
							guid = xmlnode_get_next_twin(guid)){
				group_id = xmlnode_get_data(guid);
				msn_user_add_group_id(user,group_id);
				purple_debug_misc("MsnAB","guid:%s\n",group_id);
				g_free(group_id);
			}
		}else{
			/*not in any group,Then set default group*/
			group_id = g_strdup(MSN_INDIVIDUALS_GROUP_ID);
			msn_user_add_group_id(user,group_id);
			g_free(group_id);
		}
	}

	abNode =xmlnode_get_child(result,"ab");
	if(abNode != NULL){
		xmlnode *LastChangeNode, *DynamicItemLastChangedNode;
		char *lastchange, *dynamicChange;

		LastChangeNode = xmlnode_get_child(abNode,"lastChange");
		lastchange = xmlnode_get_data(LastChangeNode);
		purple_debug_info("MsnAB"," lastchanged Time:{%s}\n",lastchange);
		purple_blist_node_set_string(msn_session_get_bnode(contact->session),"ablastChange",lastchange);
		
		DynamicItemLastChangedNode = xmlnode_get_child(abNode,"DynamicItemLastChanged");
		dynamicChange = xmlnode_get_data(DynamicItemLastChangedNode);
		purple_debug_info("MsnAB"," DynamicItemLastChanged :{%s}\n",dynamicChange);
		purple_blist_node_set_string(msn_session_get_bnode(contact->session),"DynamicItemLastChanged",lastchange);
	}

	xmlnode_free(node);
	msn_soap_free_read_buf(contact->soapconn);
	return TRUE;
}

static void
msn_get_address_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnContact *contact;
	MsnSession *session;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);

//	purple_debug_misc("msn", "soap contact server Reply: {%s}\n", soapconn->read_buf);
	if (msn_parse_addressbook(contact)) {
		msn_notification_dump_contact(session);
		msn_set_psm(session);
		msn_session_finish_login(session);
	} else {
		msn_get_address_book(contact, NULL, NULL);
	}

	/*free the read buffer*/
	msn_soap_free_read_buf(soapconn);
}

/**/
static void
msn_address_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish contact written\n");
	soapconn->read_cb = msn_get_address_cb;
}

/*get the address book*/
void
msn_get_address_book(MsnContact *contact, const char *LastChanged, const char *dynamicItemLastChange)
{
	MsnSoapReq *soap_request;
	char *body = NULL;
	char *ab_update_str,*update_str;

	purple_debug_info("MaYuan","msn_get_address_book()...\n");
	/*build SOAP and POST it*/
	if(LastChanged != NULL){
		ab_update_str = g_strdup_printf(MSN_GET_ADDRESS_UPDATE_XML,LastChanged);
	}else{
		ab_update_str = g_strdup("");
	}
	if(dynamicItemLastChange != NULL){
		update_str = g_strdup_printf(MSN_GET_ADDRESS_UPDATE_XML,
									 dynamicItemLastChange);
	}else{
		update_str = g_strdup(ab_update_str);
	}
	g_free(ab_update_str);
	
	body = g_strdup_printf(MSN_GET_ADDRESS_TEMPLATE,update_str);
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_GET_ADDRESS_SOAP_ACTION,
					body,
					msn_get_address_cb,
					msn_address_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
	g_free(update_str);
	g_free(body);
}

static void
msn_add_contact_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","add contact read done\n");
}

static void
msn_add_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish add contact  written\n");
	soapconn->read_cb = msn_add_contact_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*add a Contact */
void
msn_add_contact(MsnContact *contact,const char *passport,const char *groupId)
{
	MsnSoapReq *soap_request;
	char *body = NULL;
	char *contact_xml = NULL;
	char *soap_action;

	purple_debug_info("MaYuan","msn add a contact...\n");
	contact_xml = g_strdup_printf(MSN_CONTACT_XML,passport);
	if(groupId == NULL){
		body = g_strdup_printf(MSN_ADD_CONTACT_TEMPLATE,contact_xml);
		g_free(contact_xml);
		/*build SOAP and POST it*/
		soap_action = g_strdup(MSN_CONTACT_ADD_SOAP_ACTION);
	}else{
		body = g_strdup_printf(MSN_ADD_CONTACT_GROUP_TEMPLATE,groupId,contact_xml);
		g_free(contact_xml);
		/*build SOAP and POST it*/
		soap_action = g_strdup(MSN_ADD_CONTACT_GROUP_SOAP_ACTION);
	}
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,soap_action,
					body,
					msn_add_contact_read_cb,
					msn_add_contact_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(soap_action);
	g_free(body);
}

static void
msn_delete_contact_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","delete contact read done\n");
}

static void
msn_delete_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","delete contact written\n");
	soapconn->read_cb = msn_delete_contact_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*delete a Contact*/
void
msn_delete_contact(MsnContact *contact,const char *contactId)
{	
	char *body = NULL;
	char *contact_xml = NULL ;
	MsnSoapReq *soap_request;

	g_return_if_fail(contactId != NULL);
	purple_debug_info("MaYuan","msn delete a contact,contactId:{%s}...\n",contactId);
	contact_xml = g_strdup_printf(MSN_CONTACTS_DEL_XML,contactId);
	body = g_strdup_printf(MSN_DEL_CONTACT_TEMPLATE,contact_xml);
	g_free(contact_xml);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_CONTACT_DEL_SOAP_ACTION,
					body,
					msn_delete_contact_read_cb,
					msn_delete_contact_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(body);
}

#if 0
static void
msn_update_contact_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","update contact read done\n");
}

static void
msn_update_contact_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","update contact written\n");
	soapconn->read_cb = msn_update_contact_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*update a contact's Nickname*/
void
msn_update_contact(MsnContact *contact,const char* nickname)
{
	MsnSoapReq *soap_request;
	char *body = NULL;

	purple_debug_info("MaYuan","msn unblock a contact...\n");

	body = g_strdup_printf(MSN_CONTACT_UPDATE_TEMPLATE,nickname);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_CONTACT_UPDATE_SOAP_ACTION,
					body,
					msn_update_contact_read_cb,
					msn_update_contact_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(body);
}
#endif

static void
msn_block_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","block read done\n");
}

static void
msn_block_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish unblock written\n");
	soapconn->read_cb = msn_block_read_cb;
}

/*block a Contact*/
void
msn_block_contact(MsnContact *contact,const char* membership_id)
{
	MsnSoapReq *soap_request;
	char *body = NULL;

	purple_debug_info("MaYuan","msn block a contact...\n");
	body = g_strdup_printf(MSN_CONTACT_DELECT_FROM_ALLOW_TEMPLATE,membership_id);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_SHARE_POST_URL,MSN_CONTACT_BLOCK_SOAP_ACTION,
					body,
					msn_block_read_cb,
					msn_block_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(body);
}

static void
msn_unblock_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","unblock read done\n");
}

static void
msn_unblock_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish unblock written\n");
	soapconn->read_cb = msn_unblock_read_cb;
}

/*unblock a contact*/
void
msn_unblock_contact(MsnContact *contact,const char* passport)
{
	MsnSoapReq *soap_request;
	char *body = NULL;

	purple_debug_info("MaYuan","msn unblock a contact...\n");

	body = g_strdup_printf(MSN_UNBLOCK_CONTACT_TEMPLATE,passport);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_SHARE_POST_URL,MSN_CONTACT_UNBLOCK_SOAP_ACTION,
					body,
					msn_unblock_read_cb,
					msn_unblock_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(body);
}

#if 0
static void
msn_gleams_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","Gleams read done\n");
}

static void
msn_gleams_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish Group written\n");
	soapconn->read_cb = msn_gleams_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*get the gleams info*/
void
msn_get_gleams(MsnContact *contact)
{
	MsnSoapReq *soap_request;

	purple_debug_info("MaYuan","msn get gleams info...\n");
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_GET_GLEAMS_SOAP_ACTION,
					MSN_GLEAMS_TEMPLATE,
					msn_gleams_read_cb,
					msn_gleams_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
}
#endif

/***************************************************************
 * Group Operation
 ***************************************************************/
static void
msn_group_read_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	purple_debug_info("MaYuan","Group read \n");
}

static void
msn_group_written_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	purple_debug_info("MaYuan","finish Group written\n");
	soapconn->read_cb = msn_group_read_cb;
//	msn_soap_read_cb(data,source,cond);
}

/*add group*/
void msn_add_group(MsnSession *session,const char* group_name)
{
	MsnSoapReq *soap_request;
	MsnContact *contact ;
	char *body = NULL;

	g_return_if_fail(session != NULL);
	contact = session->contact;
	purple_debug_info("MaYuan","msn add group...\n");

	body = g_strdup_printf(MSN_GROUP_ADD_TEMPLATE,group_name);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_GROUP_ADD_SOAP_ACTION,
					body,
					msn_group_read_cb,
					msn_group_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);
}

/*delete a group*/
void msn_del_group(MsnSession *session,const char *guid)
{
	MsnSoapReq *soap_request;
	MsnContact *contact;
	char *body = NULL;

	g_return_if_fail(session != NULL);
	/*if group uid we need to del is NULL, 
	 * we need to delete nothing
	 */
	g_return_if_fail(guid != NULL);
	contact = session->contact;
	purple_debug_info("MaYuan","msn del group...\n");

	body = g_strdup_printf(MSN_GROUP_DEL_TEMPLATE,guid);
	/*build SOAP and POST it*/
	soap_request = msn_soap_request_new(MSN_CONTACT_SERVER,
					MSN_ADDRESS_BOOK_POST_URL,MSN_GROUP_DEL_SOAP_ACTION,
					body,
					msn_group_read_cb,
					msn_group_written_cb);
	msn_soap_post(contact->soapconn,soap_request,msn_contact_connect_init);

	g_free(body);
}

void
msn_contact_connect_init(MsnSoapConn *soapconn)
{
	/*  Authenticate via Windows Live ID. */
	purple_debug_info("MaYuan","msn_contact_connect...\n");

	msn_soap_init(soapconn,MSN_CONTACT_SERVER,1,
					msn_contact_login_connect_cb,
					msn_contact_login_error_cb);
}
