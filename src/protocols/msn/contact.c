/**
 * @file contact.c 
 * 	get MSN contacts via SOAP request
 *	created by MaYuan<mayuan2006@gmail.com>
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

#include "msn.h"
#include "soap.h"
#include "contact.h"
#include "xmlnode.h"
#include "group.h"

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
msn_contact_login_error_cb(GaimSslConnection *gsc, GaimSslErrorType error, void *data)
{
	MsnSoapConn *soapconn = data;
	MsnSession *session;

	session = soapconn->session;
	g_return_if_fail(session != NULL);

	msn_session_set_error(session, MSN_ERROR_SERV_DOWN, _("Unable to connect to contact server"));
}

/*msn contact SOAP server connect process*/
static void
msn_contact_login_connect_cb(gpointer data, GaimSslConnection *gsc,
				 GaimInputCondition cond)
{
	MsnSoapConn *soapconn = data;
	MsnSession * session;
	MsnContact *contact;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);

	session = contact->session;
	g_return_if_fail(session != NULL);

	/*login ok!We can retrieve the contact list*/
	msn_get_contact_list(contact);
}

/*get MSN member role utility*/
static int
msn_get_memberrole(char * role)
{
	if(!strcmp(role,"Allow")){
		return MSN_LIST_AL_OP;
	}else if(!strcmp(role,"Block")){
		return MSN_LIST_BL_OP;
//	}else if(!strcmp(role,"Reverse")){
//		return MSN_LIST_RL_OP;
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
	return 0;
}

/*parse contact list*/
static void
msn_parse_contact_list(MsnContact * contact)
{
	MsnSession * session;
	MsnUser *user;
	int list_op =0;
	char * passport;
	xmlnode * node,*body,*response,*result,*services,*service,*memberships;
	xmlnode *membershipnode,*members,*member,*passportNode,*role;

	session = contact->session;
//	gaim_debug_misc("xml","parse contact list:{%s}\nsize:%d\n",contact->soapconn->body,contact->soapconn->body_len);
	node = 	xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);
//	node = 	xmlnode_from_str(contact->soapconn->body, -1);

	if(node == NULL){
		gaim_debug_misc("xml","parse from str err!\n");
		return;
	}
	gaim_debug_misc("xml","node{%p},name:%s,child:%s,last:%s\n",node,node->name,node->child->name,node->lastchild->name);
	body = xmlnode_get_child(node,"Body");
	gaim_debug_misc("xml","body{%p},name:%s\n",body,body->name);
	response = xmlnode_get_child(body,"FindMembershipResponse");
	gaim_debug_misc("xml","response{%p},name:%s\n",response,response->name);
	result =xmlnode_get_child(response,"FindMembershipResult");
	gaim_debug_misc("xml","result{%p},name:%s\n",result,result->name);
	services =xmlnode_get_child(result,"Services");
	gaim_debug_misc("xml","services{%p},name:%s\n",services,services->name);
	service =xmlnode_get_child(services,"Service");
	gaim_debug_misc("xml","service{%p},name:%s\n",service,service->name);
	memberships =xmlnode_get_child(service,"Memberships");
	gaim_debug_misc("xml","memberships{%p},name:%s\n",memberships,memberships->name);
	for(membershipnode = xmlnode_get_child(memberships, "Membership"); membershipnode;
					membershipnode = xmlnode_get_next_twin(membershipnode)){
		role = xmlnode_get_child(membershipnode,"MemberRole");
		list_op = msn_get_memberrole(xmlnode_get_data(role));
		gaim_debug_misc("memberrole","role:%s,list_op:%d\n",xmlnode_get_data(role),list_op);
		members = xmlnode_get_child(membershipnode,"Members");
		for(member = xmlnode_get_child(members, "Member"); member;
				member = xmlnode_get_next_twin(member)){
			xmlnode * typeNode;
			char * type;

			gaim_debug_misc("MaYuan","type:%s\n",xmlnode_get_attrib(member,"type"));
			if(!g_strcasecmp(xmlnode_get_attrib(member,"type"),"PassportMember")){
				passportNode = xmlnode_get_child(member,"PassportName");
				passport = xmlnode_get_data(passportNode);
				typeNode = xmlnode_get_child(member,"Type");
				type = xmlnode_get_data(typeNode);
				gaim_debug_misc("Passport","name:%s,type:%s\n",passport,type);
				user = msn_userlist_find_user(session->userlist, passport);
				if (user == NULL){
					user = msn_user_new(session->userlist, passport, "");
					msn_userlist_add_user(session->userlist, user);
				}
//				user->list_op |= list_op;
				msn_got_lst_user(session, user, list_op, NULL);
			}
			if(!g_strcasecmp(xmlnode_get_attrib(member,"type"),"PhoneMember")){
			}
			if(!g_strcasecmp(xmlnode_get_attrib(member,"type"),"EmailMember")){
			}
		}
	}

	xmlnode_free(node);

	msn_get_address_book(contact);
}

static void
msn_get_contact_list_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnContact *contact;
	MsnSession *session;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);

//	gaim_debug_misc("msn", "soap contact server Reply: {%s}\n", soapconn->read_buf);
	msn_parse_contact_list(contact);
}

static void
msn_get_contact_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish contact written\n");
	soapconn->read_cb = msn_get_contact_list_cb;
	msn_soap_read_cb(data,source,cond);
}

void
msn_get_contact_list(MsnContact * contact)
{
	gaim_debug_info("MaYuan","msn_get_contact_list()...\n");
	contact->soapconn->login_path = g_strdup(MSN_GET_CONTACT_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_GET_CONTACT_SOAP_ACTION);
	msn_soap_post(contact->soapconn,MSN_GET_CONTACT_TEMPLATE,msn_get_contact_written_cb);
}

static void
msn_parse_addressbook(MsnContact * contact)
{
	MsnSession * session;
	xmlnode * node,*body,*response,*result;
	xmlnode *groups,*group,*groupname,*groupId,*groupInfo;
	xmlnode	*contacts,*contactNode,*contactId,*contactInfo,*contactType,*passportName,*displayName,*groupIds,*guid;
	xmlnode *ab;
	char *group_name,*group_id;

	session = contact->session;
	gaim_debug_misc("xml","parse addressbook:{%s}\nsize:%d\n",contact->soapconn->body,contact->soapconn->body_len);
	node = 	xmlnode_from_str(contact->soapconn->body, contact->soapconn->body_len);

	if(node == NULL){
		gaim_debug_misc("xml","parse from str err!\n");
		return;
	}
	gaim_debug_misc("xml","node{%p},name:%s,child:%s,last:%s\n",node,node->name,node->child->name,node->lastchild->name);
	body = xmlnode_get_child(node,"Body");
	gaim_debug_misc("xml","body{%p},name:%s\n",body,body->name);
	response = xmlnode_get_child(body,"ABFindAllResponse");
	gaim_debug_misc("xml","response{%p},name:%s\n",response,response->name);
	result =xmlnode_get_child(response,"ABFindAllResult");
	gaim_debug_misc("xml","result{%p},name:%s\n",result,result->name);

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

		gaim_debug_misc("MsnContact","group_id:%s name:%s\n",group_id,group_name);
		if ((gaim_find_group(group_name)) == NULL){
			GaimGroup *g = gaim_group_new(group_name);
			gaim_blist_add_group(g, NULL);
		}
	}
	/*add a default No group to set up the no group Membership*/
	group_id = g_strdup(MSN_INDIVIDUALS_GROUP_ID);
	group_name = g_strdup(MSN_INDIVIDUALS_GROUP_NAME);
	msn_group_new(session->userlist,group_id , group_name);
	if (group_id != NULL){
		gaim_debug_misc("MsnContact","group_id:%s name:%s,value:%d\n",group_id,group_name,*group_name=='\0');
		if ((gaim_find_group(group_name)) == NULL){
			GaimGroup *g = gaim_group_new(group_name);
			gaim_blist_add_group(g, NULL);
		}
	}
	g_free(group_name);
	g_free(group_id);

	/*add a default No group to set up the no group Membership*/
	group_id = g_strdup(MSN_NON_IM_GROUP_ID);
	group_name = g_strdup(MSN_NON_IM_GROUP_NAME);
	msn_group_new(session->userlist,group_id , group_name);
	if (group_id != NULL){
		gaim_debug_misc("MsnContact","group_id:%s name:%s,value:%d\n",group_id,group_name,*group_name=='\0');
		if ((gaim_find_group(group_name)) == NULL){
			GaimGroup *g = gaim_group_new(group_name);
			gaim_blist_add_group(g, NULL);
		}
	}
	g_free(group_name);
	g_free(group_id);

	/*Process contact List*/
	contacts =xmlnode_get_child(result,"contacts");
	for(contactNode = xmlnode_get_child(contacts, "Contact"); contactNode;
				contactNode = xmlnode_get_next_twin(contactNode)){
		MsnUser *user;
		char *passport,*Name,*uid,*type;

		contactId= xmlnode_get_child(contactNode,"contactId");
		uid = xmlnode_get_data(contactId);

		contactInfo = xmlnode_get_child(contactNode,"contactInfo");
		contactType = xmlnode_get_child(contactInfo,"contactType");
		type = xmlnode_get_data(contactType);

		passportName = xmlnode_get_child(contactInfo,"passportName");
		if(passportName == NULL){
			/*TODO: add it to the none-instant Messenger group and recognize as email Membership*/
			continue;
		}
		passport = xmlnode_get_data(passportName);

		displayName = xmlnode_get_child(contactInfo,"displayName");
		Name =xmlnode_get_data(displayName);

		gaim_debug_misc("contact","name:%s,Id:{%s},display:{%s}\n",
						passport,
						xmlnode_get_data(contactId),
						Name);

		user = msn_userlist_find_user(session->userlist, passport);
		if (user == NULL){
			user = msn_user_new(session->userlist, passport, Name);
			msn_userlist_add_user(session->userlist, user);
		}
		msn_user_set_uid(user,uid);
		msn_user_set_type(user,msn_get_user_type(type));
		user->list_op |= 1;

		gaim_debug_misc("MsnContact","\n");
		groupIds = xmlnode_get_child(contactInfo,"groupIds");
		if(groupIds){
			for(guid = xmlnode_get_child(groupIds, "guid");guid;
							guid = xmlnode_get_next_twin(guid)){
				group_id = xmlnode_get_data(guid);
				msn_user_add_group_id(user,group_id);
				gaim_debug_misc("contact","guid:%s\n",group_id);
			}
		}else{
			group_id = g_strdup(MSN_INDIVIDUALS_GROUP_ID);
			msn_user_add_group_id(user,group_id);
			g_free(group_id);
#if 0
			/*not in any group,Then set default group*/
			char *name,*group_id;

			name = g_strdup(MSN_INDIVIDUALS_GROUP_NAME);
			group_id = g_strdup(MSN_INDIVIDUALS_GROUP_ID);
			gaim_debug_misc("MsnContact","group_id:%s name:%s\n",group_id,name);

			msn_user_add_group_id(user,group_id);
			msn_group_new(session->userlist, group_id, name);

			if (group_id != NULL){
				gaim_debug_misc("MsnContact","group_id:%s name:%s,value:%d\n",group_id,name,*name=='\0');
				if ((gaim_find_group(name)) == NULL){
					GaimGroup *g = gaim_group_new(name);
					gaim_blist_add_group(g, NULL);
				}
			}

			gaim_debug_misc("contact","guid is NULL\n");
			g_free(name);
			g_free(group_id);
#endif
		}
	}

	ab =xmlnode_get_child(result,"ab");

	xmlnode_free(node);
	msn_soap_free_read_buf(contact->soapconn);

	msn_notification_dump_contact(session);
	msn_set_psm(session);
	msn_session_finish_login(session);
}

static void
msn_get_address_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	
	MsnContact *contact;
	MsnSession *session;

	contact = soapconn->parent;
	g_return_if_fail(contact != NULL);
	session = soapconn->session;
	g_return_if_fail(session != NULL);

//	gaim_debug_misc("msn", "soap contact server Reply: {%s}\n", soapconn->read_buf);
	msn_parse_addressbook(contact);
}

/**/
static void
msn_address_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish contact written\n");
	soapconn->read_cb = msn_get_address_cb;
	msn_soap_read_cb(data,source,cond);
}

/*get the address book*/
void
msn_get_address_book(MsnContact *contact)
{
	gaim_debug_info("MaYuan","msn_get_address_book()...\n");
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_GET_ADDRESS_SOAP_ACTION);
	msn_soap_post(contact->soapconn,MSN_GET_ADDRESS_TEMPLATE,msn_address_written_cb);
}

static void
msn_add_contact_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	gaim_debug_info("MaYuan","block read done\n");
}

static void
msn_add_contact_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish unblock written\n");
	soapconn->read_cb = msn_add_contact_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*add a Contact */
void
msn_add_contact(MsnContact *contact,const char *passport,char *groupId)
{
	char *body = NULL;
	char *contact_xml = NULL;

	gaim_debug_info("MaYuan","msn add a contact...\n");
	contact_xml = g_strdup_printf(MSN_CONTACT_XML,passport);
	if(groupId == NULL){
		body = g_strdup_printf(MSN_ADD_CONTACT_TEMPLATE,contact_xml);
		g_free(contact_xml);
		/*build SOAP and POST it*/
		contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
		contact->soapconn->soap_action = g_strdup(MSN_CONTACT_ADD_SOAP_ACTION);
	}else{
		body = g_strdup_printf(MSN_ADD_CONTACT_GROUP_TEMPLATE,groupId,contact_xml);
		g_free(contact_xml);
		/*build SOAP and POST it*/
		contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
		contact->soapconn->soap_action = g_strdup(MSN_ADD_CONTACT_GROUP_SOAP_ACTION);
	}
	msn_soap_post(contact->soapconn,body,msn_add_contact_written_cb);

	g_free(body);
}

static void
msn_delete_contact_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	gaim_debug_info("MaYuan","delete contact read done\n");
}

static void
msn_delete_contact_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","delete contact written\n");
	soapconn->read_cb = msn_delete_contact_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*delete a Contact*/
void
msn_delete_contact(MsnContact *contact,const char *contactId)
{	
	char *body = NULL;
	char *contact_xml = NULL ;

	gaim_debug_info("MaYuan","msn delete a contact,contactId:{%s}...\n",contactId);
	contact_xml = g_strdup_printf(MSN_CONTACTS_DEL_XML,contactId);
	body = g_strdup_printf(MSN_DEL_CONTACT_TEMPLATE,contact_xml);
	g_free(contact_xml);
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_CONTACT_DEL_SOAP_ACTION);
	msn_soap_post(contact->soapconn,body,msn_delete_contact_written_cb);
	g_free(body);
}

static void
msn_block_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	gaim_debug_info("MaYuan","block read done\n");
}

static void
msn_block_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish unblock written\n");
	soapconn->read_cb = msn_block_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*block a Contact*/
void
msn_block_contact(MsnContact *contact,const char* membership_id)
{
	char *body = NULL;

	gaim_debug_info("MaYuan","msn block a contact...\n");
	body = g_strdup_printf(MSN_CONTACT_DELECT_FROM_ALLOW_TEMPLATE,membership_id);
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_SHARE_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_CONTACT_BLOCK_SOAP_ACTION);
	msn_soap_post(contact->soapconn,body,msn_block_written_cb);
	g_free(body);
}

static void
msn_unblock_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	gaim_debug_info("MaYuan","unblock read done\n");
}

static void
msn_unblock_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish unblock written\n");
	soapconn->read_cb = msn_unblock_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*unblock a contact*/
void
msn_unblock_contact(MsnContact *contact,const char* passport)
{
	char *body = NULL;

	gaim_debug_info("MaYuan","msn unblock a contact...\n");

	body = g_strdup_printf(MSN_UNBLOCK_CONTACT_TEMPLATE,passport);
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_SHARE_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_CONTACT_UNBLOCK_SOAP_ACTION);
	msn_soap_post(contact->soapconn,body,msn_unblock_written_cb);
	g_free(body);
}

static void
msn_gleams_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	gaim_debug_info("MaYuan","Gleams read done\n");
}

static void
msn_gleams_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish Group written\n");
	soapconn->read_cb = msn_gleams_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*get the gleams info*/
void
msn_get_gleams(MsnContact *contact)
{
	gaim_debug_info("MaYuan","msn get gleams info...\n");
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_GET_GLEAMS_SOAP_ACTION);
	msn_soap_post(contact->soapconn,MSN_GLEAMS_TEMPLATE,msn_gleams_written_cb);
}

/***************************************************************
 * Group Operation
 ***************************************************************/
static void
msn_group_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	gaim_debug_info("MaYuan","Group read \n");
}

static void
msn_group_written_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnSoapConn * soapconn = data;	

	gaim_debug_info("MaYuan","finish Group written\n");
	soapconn->read_cb = msn_group_read_cb;
	msn_soap_read_cb(data,source,cond);
}

/*add group*/
void msn_add_group(MsnSession *session,const char* group_name)
{
	char *body = NULL;
	MsnContact *contact ;

	g_return_if_fail(session != NULL);
	contact = session->contact;
	gaim_debug_info("MaYuan","msn add group...\n");

	body = g_strdup_printf(MSN_GROUP_ADD_TEMPLATE,group_name);
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_GROUP_ADD_SOAP_ACTION);
	msn_soap_post(contact->soapconn,body,msn_group_written_cb);
	g_free(body);
}

/*delete a group*/
void msn_del_group(MsnSession *session,const char *guid)
{
	MsnContact *contact;
	char *body = NULL;

	g_return_if_fail(session != NULL);
	contact = session->contact;
	gaim_debug_info("MaYuan","msn del group...\n");

	body = g_strdup_printf(MSN_GROUP_DEL_TEMPLATE,guid);
	/*build SOAP and POST it*/
	contact->soapconn->login_path = g_strdup(MSN_ADDRESS_BOOK_POST_URL);
	contact->soapconn->soap_action = g_strdup(MSN_GROUP_DEL_SOAP_ACTION);
	msn_soap_post(contact->soapconn,body,msn_group_written_cb);
	g_free(body);
}

void
msn_contact_connect(MsnContact *contact)
{
	/*  Authenticate via Windows Live ID. */
	gaim_debug_info("MaYuan","msn_contact_connect...\n");

	msn_soap_init(contact->soapconn,MSN_CONTACT_SERVER,1,
					msn_contact_login_connect_cb,
					msn_contact_login_error_cb);
}

