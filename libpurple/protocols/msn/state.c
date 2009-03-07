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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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
 *	<CurrentMedia>\0Music\01\0{0} - {1}\0Song Title\0Song Artist\0Song Album\0\0</CurrentMedia>\
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
		xmlnode_insert_data(psmNode, psmstr, -1);
	}
	xmlnode_insert_child(dataNode, psmNode);

	mediaNode = xmlnode_new("CurrentMedia");
	if(mediastr != NULL){
		xmlnode_insert_data(mediaNode, mediastr, -1);
	}
	xmlnode_insert_child(dataNode, mediaNode);

	guidNode = xmlnode_new("MachineGuid");
	if(guidstr != NULL){
		xmlnode_insert_data(guidNode, guidstr, -1);
	}
	xmlnode_insert_child(dataNode, guidNode);

	result = xmlnode_to_str(dataNode, &length);
	xmlnode_free(dataNode);
	return result;
}

/* parse CurrentMedia string */
gboolean
msn_parse_currentmedia(const char *cmedia, CurrentMedia *media)
{
	char **cmedia_array;
	int strings = 0;
	gboolean parsed = FALSE;

	if ((cmedia == NULL) || (*cmedia == '\0')) {
		purple_debug_info("msn", "No currentmedia string\n");
		return FALSE;
	}

	purple_debug_info("msn", "Parsing currentmedia string: \"%s\"\n", cmedia);

	cmedia_array = g_strsplit(cmedia, "\\0", 0);

	/*
	 * 0: Application
	 * 1: 'Music'/'Games'/'Office'
	 * 2: '1' if enabled, '0' if not
	 * 3: Format (eg. {0} by {1})
	 * 4: Title
	 * If 'Music':
	 *  5: Artist
	 *  6: Album
	 *  7: ?
	 */
#if GLIB_CHECK_VERSION(2,6,0)
	strings  = g_strv_length(cmedia_array);
#else
	while (cmedia_array[++strings] != NULL);
#endif

	if (strings >= 4 && !strcmp(cmedia_array[2], "1")) {
		parsed = TRUE;

		if (!strcmp(cmedia_array[1], "Music"))
			media->type = CURRENT_MEDIA_MUSIC;
		else if (!strcmp(cmedia_array[1], "Games"))
			media->type = CURRENT_MEDIA_GAMES;
		else if (!strcmp(cmedia_array[1], "Office"))
			media->type = CURRENT_MEDIA_OFFICE;
		else
			media->type = CURRENT_MEDIA_UNKNOWN;

		g_free(media->title);
		if (strings == 4) {
			media->title = g_strdup(cmedia_array[3]);
		} else {
			media->title = g_strdup(cmedia_array[4]);
		}

		g_free(media->artist);
		if (strings > 5)
			media->artist = g_strdup(cmedia_array[5]);
		else
			media->artist = NULL;

		g_free(media->album);
		if (strings > 6)
			media->album = g_strdup(cmedia_array[6]);
		else
			media->album = NULL;

	}

	g_strfreev(cmedia_array);

	return parsed;
}

/* get the CurrentMedia info from the XML string */
char *
msn_get_currentmedia(char *xml_str, gsize len)
{
	xmlnode *payloadNode, *currentmediaNode;
	char *currentmedia;

	purple_debug_info("msn", "Get CurrentMedia\n");
	payloadNode = xmlnode_from_str(xml_str, len);
	if (!payloadNode) {
		purple_debug_error("msn", "PSM XML parse Error!\n");
		return NULL;
	}
	currentmediaNode = xmlnode_get_child(payloadNode, "CurrentMedia");
	if (currentmediaNode == NULL) {
		purple_debug_info("msn", "No CurrentMedia Node");
		xmlnode_free(payloadNode);
		return NULL;
	}
	currentmedia = xmlnode_get_data(currentmediaNode);

	xmlnode_free(payloadNode);

	return currentmedia;
}

/*get the PSM info from the XML string*/
char *
msn_get_psm(char *xml_str, gsize len)
{
	xmlnode *payloadNode, *psmNode;
	char *psm;

	purple_debug_info("msn", "msn get PSM\n");
	payloadNode = xmlnode_from_str(xml_str, len);
	if (!payloadNode) {
		purple_debug_error("msn", "PSM XML parse Error!\n");
		return NULL;
	}
	psmNode = xmlnode_get_child(payloadNode, "PSM");
	if (psmNode == NULL) {
		purple_debug_info("msn", "No PSM status Node");
		xmlnode_free(payloadNode);
		return NULL;
	}
	psm = xmlnode_get_data(psmNode);

	xmlnode_free(payloadNode);

	return psm;
}

static char *
create_media_string(PurplePresence *presence)
{
	const char *title, *game, *office;
	char *ret;
	PurpleStatus *status = purple_presence_get_status(presence, "tune");
	if (!status || !purple_status_is_active(status))
		return NULL;

	title = purple_status_get_attr_string(status, PURPLE_TUNE_TITLE);
	game = purple_status_get_attr_string(status, "game");
	office = purple_status_get_attr_string(status, "office");

	if (title && *title) {
		const char *artist = purple_status_get_attr_string(status, PURPLE_TUNE_ARTIST);
		const char *album = purple_status_get_attr_string(status, PURPLE_TUNE_ALBUM);
		ret = g_strdup_printf("WMP\\0Music\\01\\0{0}%s%s\\0%s\\0%s\\0%s\\0",
		                      artist ? " - {1}" : "",
		                      album ? " ({2})" : "",
		                      title,
		                      artist ? artist : "",
		                      album ? album : "");
	}
	else if (game && *game)
		ret = g_strdup_printf("\\0Games\\01\\0Playing {0}\\0%s\\0", game);
	else if (office && *office)
		ret = g_strdup_printf("\\0Office\\01\\0Editing {0}\\0%s\\0", office);
	else
		ret = NULL;

	return ret;
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
	gchar *statusline_stripped, *media = NULL;

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->notification != NULL);

	cmdproc = session->notification->cmdproc;

	/* Get the PSM string from Purple's Status Line */
	presence = purple_account_get_presence(account);
	status = purple_presence_get_active_status(presence);
	statusline = purple_status_get_attr_string(status, "message");

	/* MSN expects plain text, not HTML */
	statusline_stripped = purple_markup_strip_html(statusline);
	media = create_media_string(presence);
	g_free(session->psm);
	session->psm = msn_build_psm(statusline_stripped, media, NULL);

	payload = session->psm;
	purple_debug_misc("msn", "Sending UUX command with payload: %s\n", payload);
	trans = msn_transaction_new(cmdproc, "UUX", "%" G_GSIZE_FORMAT, strlen(payload));
	msn_transaction_set_payload(trans, payload, strlen(payload));
	msn_cmdproc_send_trans(cmdproc, trans);

	g_free(statusline_stripped);
	g_free(media);
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
