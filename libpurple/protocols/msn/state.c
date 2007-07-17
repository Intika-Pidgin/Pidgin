/**
 * @file state.c State functions and definitions
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
#include "state.h"

static const char *away_text[] =
{
	N_("Available"),
	N_("Available"),
	N_("Busy"),
	N_("Idle"),
	N_("Be Right Back"),
	N_("Away From Computer"),
	N_("On The Phone"),
	N_("Out To Lunch"),
	N_("Available"),
	N_("Available")
};

/* Local Function Prototype*/
static char *msn_build_psm(const char *psmstr,const char *mediastr,
						   const char *guidstr);

/*
 * WLM media PSM info build prcedure
 *
 * Result can like:
 *	<CurrentMedia>\0Music\01\0{0} - {1}\0 Song Title\0Song Artist\0Song Album\0\0</CurrentMedia>\
 *	<CurrentMedia>\0Games\01\0Playing {0}\0Game Name\0</CurrentMedia>\
 *	<CurrentMedia>\0Office\01\0Office Message\0Office App Name\0</CurrentMedia>"
 */
static char *
msn_build_psm(const char *psmstr,const char *mediastr, const char *guidstr)
{
	xmlnode *dataNode,*psmNode,*mediaNode,*guidNode;
	char *result;
	int length;

	dataNode = xmlnode_new("Data");

	psmNode = xmlnode_new("PSM");
	if(psmstr != NULL){
		xmlnode_insert_data(psmNode,psmstr,strlen(psmstr));
	}
	xmlnode_insert_child(dataNode,psmNode);

	mediaNode = xmlnode_new("CurrentMedia");
	if(mediastr != NULL){
		xmlnode_insert_data(psmNode,mediastr,strlen(mediastr));
	}
	xmlnode_insert_child(dataNode,mediaNode);

	guidNode = xmlnode_new("MachineGuid");
	if(guidstr != NULL){
		xmlnode_insert_data(guidNode,guidstr,strlen(guidstr));
	}
	xmlnode_insert_child(dataNode,guidNode);

	result = xmlnode_to_str(dataNode,&length);
	return result;
}

/* parse CurrentMedia string */
char *
msn_parse_currentmedia(const char *cmedia)
{
	char **cmedia_array;
	char *buffer=NULL, *inptr, *outptr, *tmpptr;
	int length, strings, tmp;

	if((cmedia == NULL) || (*cmedia == '\0')) {
		purple_debug_info("msn", "No currentmedia string\n");
		return NULL;
	}

	purple_debug_info("msn", "Parsing currentmedia string: \"%s\"\n", cmedia);

	cmedia_array=g_strsplit(cmedia, "\\0", 0);

	strings=1;	/* Skip first empty string */
	length=5;	/* Space for '\0' (1 byte) and prefix (4 bytes) */
	while(strcmp(cmedia_array[strings], "")) {
		length+= strlen(cmedia_array[strings]);
		strings++;
	}

	if((strings>3) && (!strcmp(cmedia_array[2], "1"))) { /* Check if enabled */

		buffer=g_malloc(length);

		inptr=cmedia_array[3];
		outptr=buffer;

		if(!strcmp(cmedia_array[1], "Music")) {
			strcpy(outptr, "np. ");
			outptr+=4;
		}/* else if(!strcmp(cmedia_array[1], "Games")) {
		} else if(!strcmp(cmedia_array[1], "Office")) {
		}*/

		while(*inptr!='\0') {
			if((*inptr == '{') && (strlen(inptr) > 2) && (*(inptr+2) == '}') ) {
				errno = 0;
				tmp = strtol(inptr+1,&tmpptr,10);
				if( (errno!=0) || (tmpptr == (inptr+1)) ||
				                  ((tmp+5)>(strings)) ) {
					*outptr = *inptr;	/* Conversion not successful */
					outptr++;
				} else {
					/* Replace {?} tag with appropriate text */
					strcpy(outptr, cmedia_array[tmp+4]);
					outptr+=strlen(cmedia_array[tmp+4]);
					inptr+=2;
				}
			} else {
				*outptr = *inptr;
				outptr++;
			}
			inptr++;
		}
		*outptr='\0';
		purple_debug_info("msn", "Parsed currentmedia string, result: \"%s\"\n",
		                buffer);
	} else {
		purple_debug_info("msn", "Current media marked disabled, not parsing\n");
	}

	g_strfreev(cmedia_array);
	return buffer;
}

/* get the CurrentMedia info from the XML string */
char *
msn_get_currentmedia(char *xml_str, gsize len)
{
	xmlnode *payloadNode, *currentmediaNode;
	char *currentmedia_str, *currentmedia;
	
	purple_debug_info("msn","msn get CurrentMedia\n");
	payloadNode = xmlnode_from_str(xml_str, len);
	if (!payloadNode){
		purple_debug_error("msn","PSM XML parse Error!\n");
		return NULL;
	}
	currentmediaNode = xmlnode_get_child(payloadNode, "CurrentMedia");
	if (currentmediaNode == NULL){
		purple_debug_info("msn","No CurrentMedia Node");
		g_free(payloadNode);
		return NULL;
	}
	currentmedia_str = xmlnode_get_data(currentmediaNode);
	currentmedia = g_strdup(currentmedia_str);

	g_free(currentmediaNode);
	g_free(payloadNode);

	return currentmedia;
}

/*get the PSM info from the XML string*/
char *
msn_get_psm(char *xml_str, gsize len)
{
	xmlnode *payloadNode, *psmNode;
	char *psm_str, *psm;
	
	purple_debug_info("MSNP14","msn get PSM\n");
	payloadNode = xmlnode_from_str(xml_str, len);
	if (!payloadNode){
		purple_debug_error("MSNP14","PSM XML parse Error!\n");
		return NULL;
	}
	psmNode = xmlnode_get_child(payloadNode, "PSM");
	if (psmNode == NULL){
		purple_debug_info("MSNP14","No PSM status Node");
		g_free(payloadNode);
		return NULL;
	}
	psm_str = xmlnode_get_data(psmNode);
	psm = g_strdup(psm_str);

	g_free(psmNode);
	g_free(payloadNode);

	return psm;
}

/* set the MSN's PSM info,Currently Read from the status Line 
 * Thanks for Cris Code
 */
void
msn_set_psm(MsnSession *session)
{
	PurpleAccount *account = session->account;
	PurplePresence *presence;
	PurpleStatus *status;
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	char *payload;
	const char *statusline;

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->notification != NULL);

	cmdproc = session->notification->cmdproc;
	/*prepare PSM info*/
	if(session->psm){
		g_free(session->psm);
	}
	/*Get the PSM string from Purple's Status Line*/
	presence = purple_account_get_presence(account);
	status = purple_presence_get_active_status(presence);
	statusline = purple_status_get_attr_string(status, "message");
	session->psm = msn_build_psm(statusline, NULL, NULL);
	payload = session->psm;

	purple_debug_info("MSNP14","UUX{%s}\n",payload);
	trans = msn_transaction_new(cmdproc, "UUX","%d",strlen(payload));
	msn_transaction_set_payload(trans, payload, strlen(payload));
	msn_cmdproc_send_trans(cmdproc, trans);
}

void
msn_change_status(MsnSession *session)
{
	PurpleAccount *account;
	MsnCmdProc *cmdproc;
	MsnUser *user;
	MsnObject *msnobj;
	const char *state_text;

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->notification != NULL);

	account = session->account;
	cmdproc = session->notification->cmdproc;
	user = session->user;
	state_text = msn_state_get_text(msn_state_from_account(account));

	/* If we're not logged in yet, don't send the status to the server,
	 * it will be sent when login completes
	 */
	if (!session->logged_in)
		return;

	msnobj = msn_user_get_object(user);

	if (msnobj == NULL)
	{
		msn_cmdproc_send(cmdproc, "CHG", "%s %d", state_text,
						 MSN_CLIENT_ID);
	}
	else
	{
		char *msnobj_str;

		msnobj_str = msn_object_to_string(msnobj);

		msn_cmdproc_send(cmdproc, "CHG", "%s %d %s", state_text,
						 MSN_CLIENT_ID, purple_url_encode(msnobj_str));

		g_free(msnobj_str);
	}
	msn_set_psm(session);
}

const char *
msn_away_get_text(MsnAwayType type)
{
	g_return_val_if_fail(type <= MSN_HIDDEN, NULL);

	return _(away_text[type]);
}

const char *
msn_state_get_text(MsnAwayType state)
{
	static char *status_text[] =
	{ "NLN", "NLN", "BSY", "IDL", "BRB", "AWY", "PHN", "LUN", "HDN", "HDN" };

	return status_text[state];
}

MsnAwayType
msn_state_from_account(PurpleAccount *account)
{
	MsnAwayType msnstatus;
	PurplePresence *presence;
	PurpleStatus *status;
	const char *status_id;

	presence = purple_account_get_presence(account);
	status = purple_presence_get_active_status(presence);
	status_id = purple_status_get_id(status);

	if (!strcmp(status_id, "away"))
		msnstatus = MSN_AWAY;
	else if (!strcmp(status_id, "brb"))
		msnstatus = MSN_BRB;
	else if (!strcmp(status_id, "busy"))
		msnstatus = MSN_BUSY;
	else if (!strcmp(status_id, "phone"))
		msnstatus = MSN_PHONE;
	else if (!strcmp(status_id, "lunch"))
		msnstatus = MSN_LUNCH;
	else if (!strcmp(status_id, "invisible"))
		msnstatus = MSN_HIDDEN;
	else
		msnstatus = MSN_ONLINE;

	if ((msnstatus == MSN_ONLINE) && purple_presence_is_idle(presence))
		msnstatus = MSN_IDLE;

	return msnstatus;
}
