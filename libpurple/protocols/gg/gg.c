/**
 * @file gg.c Gadu-Gadu protocol plugin
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
 *
 * Some parts of the code are adapted or taken from the previous implementation
 * of this plugin written by Arkadiusz Miskiewicz <misiek@pld.org.pl>
 * Some parts Copyright (C) 2009  Krzysztof Klinikowski <grommasher@gmail.com>
 *
 * Thanks to Google's Summer of Code Program.
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

#include "plugin.h"
#include "version.h"
#include "notify.h"
#include "status.h"
#include "blist.h"
#include "accountopt.h"
#include "debug.h"
#include "util.h"
#include "request.h"
#include "xmlnode.h"

#include "gg.h"
#include "confer.h"
#include "search.h"
#include "buddylist.h"
#include "utils.h"
#include "resolver-purple.h"
#include "account.h"
#include "deprecated.h"
#include "purplew.h"
#include "libgadu-events.h"
#include "multilogon.h"

/* Prototypes */
static int ggp_to_gg_status(PurpleStatus *status, char **msg);
static void ggp_set_purplestatus(PurpleAccount *account, PurpleStatus *status);
static gboolean ggp_set_status(PurpleAccount *account, int status, const gchar* msg);

typedef struct
{
	gboolean blocked;
} ggp_buddy_data;


/* ---------------------------------------------------------------------- */
// buddy list import/export from/to file

static void ggp_callback_buddylist_save_ok(PurpleConnection *gc, const char *filename)
{
	PurpleAccount *account = purple_connection_get_account(gc);

	char *buddylist = ggp_buddylist_dump(account);

	purple_debug_info("gg", "Saving...\n");
	purple_debug_info("gg", "file = %s\n", filename);

	if (buddylist == NULL) {
		purple_notify_info(account, _("Save Buddylist..."),
			 _("Your buddylist is empty, nothing was written to the file."),
			 NULL);
		return;
	}

	if(purple_util_write_data_to_file_absolute(filename, buddylist, -1)) {
		purple_notify_info(account, _("Save Buddylist..."),
			 _("Buddylist saved successfully!"), NULL);
	} else {
		gchar *primary = g_strdup_printf(
			_("Couldn't write buddy list for %s to %s"),
			purple_account_get_username(account), filename);
		purple_notify_error(account, _("Save Buddylist..."),
			primary, NULL);
		g_free(primary);
	}

	g_free(buddylist);
}

static void ggp_callback_buddylist_load_ok(PurpleConnection *gc, gchar *file)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GError *error = NULL;
	char *buddylist = NULL;
	gsize length;

	purple_debug_info("gg", "file_name = %s\n", file);

	if (!g_file_get_contents(file, &buddylist, &length, &error)) {
		purple_notify_error(account,
				_("Couldn't load buddylist"),
				_("Couldn't load buddylist"),
				error->message);

		purple_debug_error("gg",
			"Couldn't load buddylist. file = %s; error = %s\n",
			file, error->message);

		g_error_free(error);

		return;
	}

	ggp_buddylist_load(gc, buddylist);
	g_free(buddylist);

	purple_notify_info(account,
			 _("Load Buddylist..."),
			 _("Buddylist loaded successfully!"), NULL);
}
/* }}} */

/*
 */
/* static void ggp_action_buddylist_save(PurplePluginAction *action) {{{ */
static void ggp_action_buddylist_save(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;

	purple_request_file(action, _("Save buddylist..."), NULL, TRUE,
			G_CALLBACK(ggp_callback_buddylist_save_ok), NULL,
			purple_connection_get_account(gc), NULL, NULL,
			gc);
}

static void ggp_action_buddylist_load(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;

	purple_request_file(action, _("Load buddylist from file..."), NULL,
			FALSE,
			G_CALLBACK(ggp_callback_buddylist_load_ok), NULL,
			purple_connection_get_account(gc), NULL, NULL,
			gc);
}

/* ----- PUBLIC DIRECTORY SEARCH ---------------------------------------- */

static void ggp_callback_show_next(PurpleConnection *gc, GList *row, gpointer user_data)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	GGPSearchForm *form = user_data;
	guint32 seq;

	form->page_number++;

	ggp_search_remove(info->searches, form->seq);
	purple_debug_info("gg", "ggp_callback_show_next(): Removed seq %u\n",
		form->seq);

	seq = ggp_search_start(gc, form);
	ggp_search_add(info->searches, seq, form);
	purple_debug_info("gg", "ggp_callback_show_next(): Added seq %u\n",
		seq);
}

static void ggp_callback_add_buddy(PurpleConnection *gc, GList *row, gpointer user_data)
{
	purple_blist_request_add_buddy(purple_connection_get_account(gc),
				     g_list_nth_data(row, 0), NULL, NULL);
}

static void ggp_callback_im(PurpleConnection *gc, GList *row, gpointer user_data)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	char *name;

	account = purple_connection_get_account(gc);

	name = g_list_nth_data(row, 0);
	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, name);
	purple_conversation_present(conv);
}

static void ggp_callback_find_buddies(PurpleConnection *gc, PurpleRequestFields *fields)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	GGPSearchForm *form;
	guint32 seq;

	form = ggp_search_form_new(GGP_SEARCH_TYPE_FULL);

	form->user_data = info;
	form->lastname = g_strdup(
		purple_request_fields_get_string(fields, "lastname"));
	form->firstname = g_strdup(
		purple_request_fields_get_string(fields, "firstname"));
	form->nickname = g_strdup(
		purple_request_fields_get_string(fields, "nickname"));
	form->city = g_strdup(
		purple_request_fields_get_string(fields, "city"));
	form->birthyear = g_strdup(
		purple_request_fields_get_string(fields, "year"));

	switch (purple_request_fields_get_choice(fields, "gender")) {
		case 1:
			form->gender = g_strdup(GG_PUBDIR50_GENDER_MALE);
			break;
		case 2:
			form->gender = g_strdup(GG_PUBDIR50_GENDER_FEMALE);
			break;
		default:
			form->gender = NULL;
			break;
	}

	form->active = purple_request_fields_get_bool(fields, "active")
				   ? g_strdup(GG_PUBDIR50_ACTIVE_TRUE) : NULL;

	seq = ggp_search_start(gc, form);
	ggp_search_add(info->searches, seq, form);
	purple_debug_info("gg", "ggp_callback_find_buddies(): Added seq %u\n",
		seq);
}

static void ggp_find_buddies(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;

	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("lastname",
			_("Last name"), NULL, FALSE);
	purple_request_field_string_set_masked(field, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("firstname",
			_("First name"), NULL, FALSE);
	purple_request_field_string_set_masked(field, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("nickname",
			_("Nickname"), NULL, FALSE);
	purple_request_field_string_set_masked(field, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("city",
			_("City"), NULL, FALSE);
	purple_request_field_string_set_masked(field, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("year",
			_("Year of birth"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_choice_new("gender", _("Gender"), 0);
	purple_request_field_choice_add(field, _("Male or female"));
	purple_request_field_choice_add(field, _("Male"));
	purple_request_field_choice_add(field, _("Female"));
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_bool_new("active",
			_("Only online"), FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(gc,
		_("Find buddies"),
		_("Find buddies"),
		_("Please, enter your search criteria below"),
		fields,
		_("OK"), G_CALLBACK(ggp_callback_find_buddies),
		_("Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL,
		gc);
}

/* ----- CHANGE STATUS BROADCASTING ------------------------------------------------ */

static void ggp_action_change_status_broadcasting_ok(PurpleConnection *gc, PurpleRequestFields *fields)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	int selected_field;
	PurpleAccount *account = purple_connection_get_account(gc);

	selected_field = purple_request_fields_get_choice(fields, "status_broadcasting");
	
	if (selected_field == 0)
		info->status_broadcasting = TRUE;
	else
		info->status_broadcasting = FALSE;
	
	ggp_set_purplestatus(account, purple_account_get_active_status(account));
}

static void ggp_action_change_status_broadcasting(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	
	field = purple_request_field_choice_new("status_broadcasting", _("Show status to:"), 0);
	purple_request_field_choice_add(field, _("All people"));
	purple_request_field_choice_add(field, _("Only buddies"));
	purple_request_field_group_add_field(group, field);

	if (info->status_broadcasting)
		purple_request_field_choice_set_default_value(field, 0);
	else
		purple_request_field_choice_set_default_value(field, 1);

	purple_request_fields(gc,
		_("Change status broadcasting"),
		_("Change status broadcasting"),
		_("Please, select who can see your status"),
		fields,
		_("OK"), G_CALLBACK(ggp_action_change_status_broadcasting_ok),
		_("Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL,
		gc);
}

/* ----- CONFERENCES ---------------------------------------------------- */

static void ggp_callback_add_to_chat_ok(PurpleBuddy *buddy, PurpleRequestFields *fields)
{
	PurpleConnection *conn;
	PurpleRequestField *field;
	GList *sel;

	conn = purple_account_get_connection(purple_buddy_get_account(buddy));

	g_return_if_fail(conn != NULL);

	field = purple_request_fields_get_field(fields, "name");
	sel = purple_request_field_list_get_selected(field);

	if (sel == NULL) {
		purple_debug_error("gg", "No chat selected\n");
		return;
	}

	ggp_confer_participants_add_uin(conn, sel->data,
					ggp_str_to_uin(purple_buddy_get_name(buddy)));
}

static void ggp_bmenu_add_to_chat(PurpleBlistNode *node, gpointer ignored)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	GGPInfo *info;

	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	GList *l;
	gchar *msg;

	buddy = (PurpleBuddy *)node;
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));
	info = purple_connection_get_protocol_data(gc);

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_list_new("name", "Chat name");
	for (l = info->chats; l != NULL; l = l->next) {
		GGPChat *chat = l->data;
		purple_request_field_list_add_icon(field, chat->name, NULL, chat->name);
	}
	purple_request_field_group_add_field(group, field);

	msg = g_strdup_printf(_("Select a chat for buddy: %s"),
			      purple_buddy_get_alias(buddy));
	purple_request_fields(gc,
			_("Add to chat..."),
			_("Add to chat..."),
			msg,
			fields,
			_("Add"), G_CALLBACK(ggp_callback_add_to_chat_ok),
			_("Cancel"), NULL,
			purple_connection_get_account(gc), NULL, NULL,
			buddy);
	g_free(msg);
}

/* ----- BLOCK BUDDIES -------------------------------------------------- */

static void ggp_add_deny(PurpleConnection *gc, const char *who)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	uin_t uin = ggp_str_to_uin(who);
	
	purple_debug_info("gg", "ggp_add_deny: %u\n", uin);
	
	gg_remove_notify_ex(info->session, uin, GG_USER_NORMAL);
	gg_add_notify_ex(info->session, uin, GG_USER_BLOCKED);
}

static void ggp_rem_deny(PurpleConnection *gc, const char *who)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	uin_t uin = ggp_str_to_uin(who);
	
	purple_debug_info("gg", "ggp_rem_deny: %u\n", uin);
	
	gg_remove_notify_ex(info->session, uin, GG_USER_BLOCKED);
	gg_add_notify_ex(info->session, uin, GG_USER_NORMAL);
}

/* ---------------------------------------------------------------------- */
/* ----- INTERNAL CALLBACKS --------------------------------------------- */
/* ---------------------------------------------------------------------- */


/**
 * Handle change of the status of the buddy.
 *
 * @param gc     PurpleConnection
 * @param uin    UIN of the buddy.
 * @param status ID of the status.
 * @param descr  Description.
 */
static void ggp_generic_status_handler(PurpleConnection *gc, uin_t uin,
				       int status, const char *descr)
{
	gchar *from;
	const char *st;
	char *status_msg = NULL;
	ggp_buddy_data *buddy_data;
	PurpleBuddy *buddy;
	PurpleAccount *account = purple_connection_get_account(gc);

	from = g_strdup_printf("%u", uin);
	buddy = purple_find_buddy(purple_connection_get_account(gc), from);

	switch (status) {
		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
		case GG_STATUS_BLOCKED:
			st = purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
			break;
		case GG_STATUS_FFC:
		case GG_STATUS_FFC_DESCR:
			st = "freeforchat";
			break;
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			st = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
			break;
		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			st = purple_primitive_get_id_from_type(PURPLE_STATUS_AWAY);
			break;
		case GG_STATUS_INVISIBLE:
		case GG_STATUS_INVISIBLE_DESCR:
			st = purple_primitive_get_id_from_type(PURPLE_STATUS_INVISIBLE);
			break;
		case GG_STATUS_DND:
		case GG_STATUS_DND_DESCR:
			st = purple_primitive_get_id_from_type(PURPLE_STATUS_UNAVAILABLE);
			break;
		default:
			st = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
			purple_debug_info("gg",
				"GG_EVENT_NOTIFY: Unknown status: %d\n", status);
			break;
	}

	if (descr != NULL) {
		status_msg = g_strdup(descr);
		g_strstrip(status_msg);
		if (status_msg[0] == '\0') {
			g_free(status_msg);
			status_msg = NULL;
		}
	}

	buddy_data = purple_buddy_get_protocol_data(buddy);
	if (!buddy_data)
	{
		buddy_data = g_new0(ggp_buddy_data, 1);
		purple_buddy_set_protocol_data(buddy, buddy_data);
	}
	buddy_data->blocked = (status == GG_STATUS_BLOCKED);

	if (uin == ggp_str_to_uin(purple_account_get_username(account)))
	{
		purple_debug_info("gg", "own status changed to %s [%s]\n", st,
			status_msg ? status_msg : "");
	}

	purple_debug_info("gg", "status of %u is %s [%s]\n", uin, st,
		status_msg ? status_msg : "");
	if (status_msg == NULL) {
		purple_prpl_got_user_status(account,
			from, st, NULL);
	} else {
		purple_prpl_got_user_status(account,
			from, st, "message", status_msg, NULL);
		g_free(status_msg);
	}
	g_free(from);
}

static void ggp_sr_close_cb(gpointer user_data)
{
	GGPSearchForm *form = user_data;
	GGPInfo *info = form->user_data;

	ggp_search_remove(info->searches, form->seq);
	purple_debug_info("gg", "ggp_sr_close_cb(): Removed seq %u\n",
		form->seq);
	ggp_search_form_destroy(form);
}

/**
 * Translate a status' ID to a more user-friendly name.
 *
 * @param id The ID of the status.
 *
 * @return The user-friendly name of the status.
 */
static const char *ggp_status_by_id(unsigned int id)
{
	const char *st;

	purple_debug_info("gg", "ggp_status_by_id: %d\n", id);
	switch (id) {
		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
			st = _("Offline");
			break;
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			st = _("Available");
			break;
		case GG_STATUS_FFC:
		case GG_STATUS_FFC_DESCR:
			return _("Chatty");
		case GG_STATUS_DND:
		case GG_STATUS_DND_DESCR:
			return _("Do Not Disturb");
		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			st = _("Away");
			break;
		default:
			st = _("Unknown");
			break;
	}

	return st;
}

static void ggp_pubdir_handle_info(PurpleConnection *gc, gg_pubdir50_t req,
				   GGPSearchForm *form)
{
	PurpleNotifyUserInfo *user_info;
	PurpleBuddy *buddy;
	char *val, *who;

	user_info = purple_notify_user_info_new();

	val = ggp_search_get_result(req, 0, GG_PUBDIR50_STATUS);
	/* XXX: Use of ggp_str_to_uin() is an ugly hack! */
	purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), ggp_status_by_id(ggp_str_to_uin(val)));
	g_free(val);

	who = ggp_search_get_result(req, 0, GG_PUBDIR50_UIN);
	/* TODO: Check whether it's correct to call add_pair_html,
	         or if we should be using add_pair_plaintext */
	purple_notify_user_info_add_pair_html(user_info, _("UIN"), who);

	val = ggp_search_get_result(req, 0, GG_PUBDIR50_FIRSTNAME);
	/* TODO: Check whether it's correct to call add_pair_html,
	         or if we should be using add_pair_plaintext */
	purple_notify_user_info_add_pair_html(user_info, _("First Name"), val);
	g_free(val);

	val = ggp_search_get_result(req, 0, GG_PUBDIR50_NICKNAME);
	/* TODO: Check whether it's correct to call add_pair_html,
	         or if we should be using add_pair_plaintext */
	purple_notify_user_info_add_pair_html(user_info, _("Nickname"), val);
	g_free(val);

	val = ggp_search_get_result(req, 0, GG_PUBDIR50_CITY);
	/* TODO: Check whether it's correct to call add_pair_html,
	         or if we should be using add_pair_plaintext */
	purple_notify_user_info_add_pair_html(user_info, _("City"), val);
	g_free(val);

	val = ggp_search_get_result(req, 0, GG_PUBDIR50_BIRTHYEAR);
	if (strncmp(val, "0", 1)) {
		/* TODO: Check whether it's correct to call add_pair_html,
		         or if we should be using add_pair_plaintext */
		purple_notify_user_info_add_pair_html(user_info, _("Birth Year"), val);
	}
	g_free(val);

	/*
	 * Include a status message, if exists and buddy is in the blist.
	 */
	buddy = purple_find_buddy(purple_connection_get_account(gc), who);
	if (NULL != buddy) {
		PurpleStatus *status;
		const char *msg;

		status = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
		msg = purple_status_get_attr_string(status, "message");

		if (msg != NULL) {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Message"), msg);
		}
	}

	purple_notify_userinfo(gc, who, user_info, ggp_sr_close_cb, form);
	g_free(who);
	purple_notify_user_info_destroy(user_info);
}

static void ggp_pubdir_handle_full(PurpleConnection *gc, gg_pubdir50_t req,
				   GGPSearchForm *form)
{
	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	int res_count;
	int start;
	int i;

	g_return_if_fail(form != NULL);

	res_count = gg_pubdir50_count(req);
	res_count = (res_count > PUBDIR_RESULTS_MAX) ? PUBDIR_RESULTS_MAX : res_count;
	if (form->page_size == 0)
		form->page_size = res_count;

	results = purple_notify_searchresults_new();

	if (results == NULL) {
		purple_debug_error("gg", "ggp_pubdir_reply_handler: "
				 "Unable to display the search results.\n");
		purple_notify_error(gc, NULL,
				  _("Unable to display the search results."),
				  NULL);
		if (form->window == NULL)
			ggp_sr_close_cb(form);
		return;
	}

	column = purple_notify_searchresults_column_new(_("UIN"));
	purple_notify_searchresults_column_add(results, column);

	column = purple_notify_searchresults_column_new(_("First Name"));
	purple_notify_searchresults_column_add(results, column);

	column = purple_notify_searchresults_column_new(_("Nickname"));
	purple_notify_searchresults_column_add(results, column);

	column = purple_notify_searchresults_column_new(_("City"));
	purple_notify_searchresults_column_add(results, column);

	column = purple_notify_searchresults_column_new(_("Birth Year"));
	purple_notify_searchresults_column_add(results, column);

	purple_debug_info("gg", "Going with %d entries\n", res_count);

	start = (int)ggp_str_to_uin(gg_pubdir50_get(req, 0, GG_PUBDIR50_START));
	purple_debug_info("gg", "start = %d\n", start);

	for (i = 0; i < res_count; i++) {
		GList *row = NULL;
		char *birth = ggp_search_get_result(req, i, GG_PUBDIR50_BIRTHYEAR);

		/* TODO: Status will be displayed as an icon. */
		/* row = g_list_append(row, ggp_search_get_result(req, i, GG_PUBDIR50_STATUS)); */
		row = g_list_append(row, ggp_search_get_result(req, i,
							GG_PUBDIR50_UIN));
		row = g_list_append(row, ggp_search_get_result(req, i,
							GG_PUBDIR50_FIRSTNAME));
		row = g_list_append(row, ggp_search_get_result(req, i,
							GG_PUBDIR50_NICKNAME));
		row = g_list_append(row, ggp_search_get_result(req, i,
							GG_PUBDIR50_CITY));
		row = g_list_append(row,
			(birth && strncmp(birth, "0", 1)) ? birth : g_strdup("-"));

		purple_notify_searchresults_row_add(results, row);
	}

	purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_CONTINUE,
					     ggp_callback_show_next);
	purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_ADD,
					     ggp_callback_add_buddy);
	purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_IM,
					     ggp_callback_im);

	if (form->window == NULL) {
		void *h = purple_notify_searchresults(gc,
				_("Gadu-Gadu Public Directory"),
				_("Search results"), NULL, results,
				(PurpleNotifyCloseCallback)ggp_sr_close_cb,
				form);

		if (h == NULL) {
			purple_debug_error("gg", "ggp_pubdir_reply_handler: "
					 "Unable to display the search results.\n");
			purple_notify_error(gc, NULL,
					  _("Unable to display the search results."),
					  NULL);
			return;
		}

		form->window = h;
	} else {
		purple_notify_searchresults_new_rows(gc, results, form->window);
	}
}

static void ggp_pubdir_reply_handler(PurpleConnection *gc, gg_pubdir50_t req)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	GGPSearchForm *form;
	int res_count;
	guint32 seq;

	seq = gg_pubdir50_seq(req);
	form = ggp_search_get(info->searches, seq);
	purple_debug_info("gg",
		"ggp_pubdir_reply_handler(): seq %u --> form %p\n", seq, form);
	/*
	 * this can happen when user will request more results
	 * and close the results window before they arrive.
	 */
	g_return_if_fail(form != NULL);

	res_count = gg_pubdir50_count(req);
	if (res_count < 1) {
		purple_debug_info("gg", "GG_EVENT_PUBDIR50_SEARCH_REPLY: Nothing found\n");
		purple_notify_error(gc, NULL,
			_("No matching users found"),
			_("There are no users matching your search criteria."));
		if (form->window == NULL)
			ggp_sr_close_cb(form);
		return;
	}

	switch (form->search_type) {
		case GGP_SEARCH_TYPE_INFO:
			ggp_pubdir_handle_info(gc, req, form);
			break;
		case GGP_SEARCH_TYPE_FULL:
			ggp_pubdir_handle_full(gc, req, form);
			break;
		default:
			purple_debug_warning("gg", "Unknown search_type!\n");
			break;
	}
}

/**
 * Dispatch a message received from a buddy.
 *
 * @param gc PurpleConnection.
 * @param ev Gadu-Gadu event structure.
 *
 * Image receiving, some code borrowed from Kadu http://www.kadu.net
 */
void ggp_recv_message_handler(PurpleConnection *gc, const struct gg_event_msg *ev, gboolean multilogon)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	PurpleConversation *conv;
	gchar *from;
	gchar *msg;
	gchar *tmp;
	time_t mtime;
	uin_t sender = ev->sender;

	if (ev->message == NULL)
	{
		purple_debug_warning("gg", "ggp_recv_message_handler: NULL as message pointer\n");
		return;
	}

	from = g_strdup_printf("%lu", (unsigned long int)ev->sender);

	tmp = g_strdup_printf("%s", ev->message);
	purple_str_strip_char(tmp, '\r');
	msg = g_markup_escape_text(tmp, -1);
	g_free(tmp);

	if (ev->msgclass & GG_CLASS_QUEUED)
		mtime = ev->time;
	else
		mtime = time(NULL);

	/* We got richtext message */
	if (ev->formats_length)
	{
		gboolean got_image = FALSE, bold = FALSE, italic = FALSE, under = FALSE;
		char *cformats = (char *)ev->formats;
		char *cformats_end = cformats + ev->formats_length;
		gint increased_len = 0;
		struct gg_msg_richtext_format *actformat;
		struct gg_msg_richtext_image *actimage;
		GString *message = g_string_new(msg);

		purple_debug_info("gg", "ggp_recv_message_handler: richtext msg from (%s): %s %i formats\n", from, msg, ev->formats_length);

		while (cformats < cformats_end)
		{
			gint byteoffset;
			actformat = (struct gg_msg_richtext_format *)cformats;
			cformats += sizeof(struct gg_msg_richtext_format);
			byteoffset = g_utf8_offset_to_pointer(message->str, actformat->position + increased_len) - message->str;

			if(actformat->position == 0 && actformat->font == 0) {
				purple_debug_warning("gg", "ggp_recv_message_handler: bogus formatting (inc: %i)\n", increased_len);
				continue;
			}
			purple_debug_info("gg", "ggp_recv_message_handler: format at pos: %i, image:%i, bold:%i, italic: %i, under:%i (inc: %i)\n",
				actformat->position,
				(actformat->font & GG_FONT_IMAGE) != 0,
				(actformat->font & GG_FONT_BOLD) != 0,
				(actformat->font & GG_FONT_ITALIC) != 0,
				(actformat->font & GG_FONT_UNDERLINE) != 0,
				increased_len);

			if (actformat->font & GG_FONT_IMAGE)
			{
				const char *placeholder;
			
				got_image = TRUE;
				actimage = (struct gg_msg_richtext_image*)(cformats);
				cformats += sizeof(struct gg_msg_richtext_image);
				purple_debug_info("gg", "ggp_recv_message_handler: image received, size: %d, crc32: %i\n", actimage->size, actimage->crc32);

				/* Checking for errors, image size shouldn't be
				 * larger than 255.000 bytes */
				if (actimage->size > 255000) {
					purple_debug_warning("gg", "ggp_recv_message_handler: received image large than 255 kb\n");
					continue;
				}

				gg_image_request(info->session, ev->sender,
					actimage->size, actimage->crc32);

				placeholder = ggp_image_pending_placeholder(actimage->crc32);
				g_string_insert(message, byteoffset, placeholder);
				increased_len += strlen(placeholder);
				continue;
			}

			if (actformat->font & GG_FONT_BOLD) {
				if (bold == FALSE) {
					g_string_insert(message, byteoffset, "<b>");
					increased_len += 3;
					bold = TRUE;
				}
			} else if (bold) {
				g_string_insert(message, byteoffset, "</b>");
				increased_len += 4;
				bold = FALSE;
			}

			if (actformat->font & GG_FONT_ITALIC) {
				if (italic == FALSE) {
					g_string_insert(message, byteoffset, "<i>");
					increased_len += 3;
					italic = TRUE;
				}
			} else if (italic) {
				g_string_insert(message, byteoffset, "</i>");
				increased_len += 4;
				italic = FALSE;
			}

			if (actformat->font & GG_FONT_UNDERLINE) {
				if (under == FALSE) {
					g_string_insert(message, byteoffset, "<u>");
					increased_len += 3;
					under = TRUE;
				}
			} else if (under) {
				g_string_insert(message, byteoffset, "</u>");
				increased_len += 4;
				under = FALSE;
			}

			if (actformat->font & GG_FONT_COLOR) {
				cformats += sizeof(struct gg_msg_richtext_color);
			}
		}

		msg = message->str;
		g_string_free(message, FALSE);

		if (got_image)
		{
			ggp_image_got_im(gc, sender, msg, mtime);
			return;
		}
	}

	purple_debug_info("gg", "ggp_recv_message_handler: msg from (%s): %s (class = %d; rcpt_count = %d; multilogon = %d)\n",
			from, msg, ev->msgclass,
			ev->recipients_count,
			multilogon);

	if (multilogon && ev->recipients_count != 0) {
		purple_debug_warning("gg", "ggp_recv_message_handler: conference multilogon messages are not yet handled\n");
	} else if (multilogon) {
		PurpleAccount *account = purple_connection_get_account(gc);
		PurpleConversation *conv;
		const gchar *who = ggp_uin_to_str(ev->sender); // not really sender
		conv = purple_find_conversation_with_account(
			PURPLE_CONV_TYPE_IM, who, account);
		if (conv == NULL)
			conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, who);
		purple_conversation_write(conv, purple_account_get_username(account), msg, PURPLE_MESSAGE_SEND, mtime);
	} else if (ev->recipients_count == 0) {
		serv_got_im(gc, from, msg, 0, mtime);
	} else {
		const char *chat_name;
		int chat_id;

		chat_name = ggp_confer_find_by_participants(gc,
				ev->recipients,
				ev->recipients_count);

		if (chat_name == NULL) {
			chat_name = ggp_confer_add_new(gc, NULL);
			serv_got_joined_chat(gc, info->chats_count, chat_name);

			ggp_confer_participants_add_uin(gc, chat_name,
							ev->sender);

			ggp_confer_participants_add(gc, chat_name,
						    ev->recipients,
						    ev->recipients_count);
		}
		conv = ggp_confer_find_by_name(gc, chat_name);
		chat_id = purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv));

		serv_got_chat_in(gc, chat_id,
			ggp_buddylist_get_buddy_name(gc, ev->sender),
			PURPLE_MESSAGE_RECV, msg, mtime);
	}
	g_free(msg);
	g_free(from);
}

static void ggp_typing_notification_handler(PurpleConnection *gc, uin_t uin, int length) {
	gchar *from;

	from = g_strdup_printf("%u", uin);
	if (length)
		serv_got_typing(gc, from, 0, PURPLE_TYPING);
	else
		serv_got_typing_stopped(gc, from);
	g_free(from);
}

/**
 * Handling of XML events.
 *
 * @param gc PurpleConnection.
 * @param data Raw XML contents.
 *
 * @see http://toxygen.net/libgadu/protocol/#ch1.13
 * @todo: this may not be necessary anymore
 */
static void ggp_xml_event_handler(PurpleConnection *gc, char *data)
{
	xmlnode *xml = NULL;
	xmlnode *xmlnode_next_event;

	xml = xmlnode_from_str(data, -1);
	if (xml == NULL)
		goto out;

	xmlnode_next_event = xmlnode_get_child(xml, "event");
	while (xmlnode_next_event != NULL)
	{
		xmlnode *xmlnode_current_event = xmlnode_next_event;
		
		xmlnode *xmlnode_type;
		char *event_type_raw;
		int event_type = 0;
		
		xmlnode *xmlnode_sender;
		char *event_sender_raw;
		uin_t event_sender = 0;

		xmlnode_next_event = xmlnode_get_next_twin(xmlnode_next_event);
		
		xmlnode_type = xmlnode_get_child(xmlnode_current_event, "type");
		if (xmlnode_type == NULL)
			continue;
		event_type_raw = xmlnode_get_data(xmlnode_type);
		if (event_type_raw != NULL)
			event_type = atoi(event_type_raw);
		g_free(event_type_raw);
		
		xmlnode_sender = xmlnode_get_child(xmlnode_current_event, "sender");
		if (xmlnode_sender != NULL)
		{
			event_sender_raw = xmlnode_get_data(xmlnode_sender);
			if (event_sender_raw != NULL)
				event_sender = ggp_str_to_uin(event_sender_raw);
			g_free(event_sender_raw);
		}
		
		switch (event_type)
		{
			case 28: /* avatar update */
				purple_debug_info("gg",
					"ggp_xml_event_handler: avatar updated (uid: %u)\n",
					event_sender);
				break;
			default:
				purple_debug_error("gg",
					"ggp_xml_event_handler: unsupported event type=%d from=%u\n",
					event_type, event_sender);
		}
	}
	
	out:
		if (xml)
			xmlnode_free(xml);
}

static void ggp_callback_recv(gpointer _gc, gint fd, PurpleInputCondition cond)
{
	PurpleConnection *gc = _gc;
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	struct gg_event *ev;
	int i;

	if (!(ev = gg_watch_fd(info->session))) {
		purple_debug_error("gg",
			"ggp_callback_recv: gg_watch_fd failed -- CRITICAL!\n");
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to read from socket"));
		return;
	}

	switch (ev->type) {
		case GG_EVENT_NONE:
			/* Nothing happened. */
			break;
		case GG_EVENT_MSG:
			ggp_recv_message_handler(gc, &ev->event.msg, FALSE);
			break;
		case GG_EVENT_ACK:
			/* Changing %u to %i fixes compiler warning */
			purple_debug_info("gg",
				"ggp_callback_recv: message sent to: %i, delivery status=%d, seq=%d\n",
				ev->event.ack.recipient, ev->event.ack.status,
				ev->event.ack.seq);
			break;
		case GG_EVENT_IMAGE_REPLY:
			ggp_image_recv(gc, &ev->event.image_reply);
			break;
		case GG_EVENT_IMAGE_REQUEST:
			ggp_image_send(gc, &ev->event.image_request);
			break;
		case GG_EVENT_NOTIFY:
		case GG_EVENT_NOTIFY_DESCR:
			{
				struct gg_notify_reply *n;
				char *descr;

				purple_debug_info("gg", "notify_pre: (%d) status: %d\n",
						ev->event.notify->uin,
						GG_S(ev->event.notify->status));

				n = (ev->type == GG_EVENT_NOTIFY) ? ev->event.notify
								  : ev->event.notify_descr.notify;

				for (; n->uin; n++) {
					descr = (ev->type == GG_EVENT_NOTIFY) ? NULL
							: ev->event.notify_descr.descr;

					purple_debug_info("gg",
						"notify: (%d) status: %d; descr: %s\n",
						n->uin, GG_S(n->status), descr ? descr : "(null)");

					ggp_generic_status_handler(gc,
						n->uin, GG_S(n->status), descr);
				}
			}
			break;
		case GG_EVENT_NOTIFY60:
			for (i = 0; ev->event.notify60[i].uin; i++) {
				purple_debug_info("gg",
					"notify60: (%d) status=%d; version=%d; descr=%s\n",
					ev->event.notify60[i].uin,
					GG_S(ev->event.notify60[i].status),
					ev->event.notify60[i].version,
					ev->event.notify60[i].descr ? ev->event.notify60[i].descr : "(null)");

				ggp_generic_status_handler(gc, ev->event.notify60[i].uin,
					GG_S(ev->event.notify60[i].status),
					ev->event.notify60[i].descr);
			}
			break;
		case GG_EVENT_STATUS:
			purple_debug_info("gg", "status: (%d) status=%d; descr=%s\n",
					ev->event.status.uin, GG_S(ev->event.status.status),
					ev->event.status.descr ? ev->event.status.descr : "(null)");

			ggp_generic_status_handler(gc, ev->event.status.uin,
				GG_S(ev->event.status.status), ev->event.status.descr);
			break;
		case GG_EVENT_STATUS60:
			purple_debug_info("gg",
				"status60: (%d) status=%d; version=%d; descr=%s\n",
				ev->event.status60.uin, GG_S(ev->event.status60.status),
				ev->event.status60.version,
				ev->event.status60.descr ? ev->event.status60.descr : "(null)");

			ggp_generic_status_handler(gc, ev->event.status60.uin,
				GG_S(ev->event.status60.status), ev->event.status60.descr);
			break;
		case GG_EVENT_PUBDIR50_SEARCH_REPLY:
			ggp_pubdir_reply_handler(gc, ev->event.pubdir50);
			break;
		case GG_EVENT_TYPING_NOTIFICATION:
			ggp_typing_notification_handler(gc, ev->event.typing_notification.uin,
				ev->event.typing_notification.length);
			break;
		case GG_EVENT_XML_EVENT:
			purple_debug_info("gg", "GG_EVENT_XML_EVENT\n");
			ggp_xml_event_handler(gc, ev->event.xml_event.data);
			break;
		case GG_EVENT_USER_DATA:
			ggp_events_user_data(gc, &ev->event.user_data);
			break;
		case GG_EVENT_USERLIST100_VERSION:
			ggp_roster_version(gc, &ev->event.userlist100_version);
			break;
		case GG_EVENT_USERLIST100_REPLY:
			ggp_roster_reply(gc, &ev->event.userlist100_reply);
			break;
		case GG_EVENT_MULTILOGON_MSG:
			ggp_multilogon_msg(gc, &ev->event.multilogon_msg);
			break;
		case GG_EVENT_MULTILOGON_INFO:
			ggp_multilogon_info(gc, &ev->event.multilogon_info);
			break;
		default:
			purple_debug_error("gg",
				"unsupported event type=%d\n", ev->type);
			break;
	}

	gg_free_event(ev);
}

static void ggp_async_login_handler(gpointer _gc, gint fd, PurpleInputCondition cond)
{
	PurpleConnection *gc = _gc;
	GGPInfo *info;
	struct gg_event *ev;

	g_return_if_fail(PURPLE_CONNECTION_IS_VALID(gc));

	info = purple_connection_get_protocol_data(gc);

	purple_debug_info("gg", "login_handler: session: check = %d; state = %d;\n",
			info->session->check, info->session->state);

	switch (info->session->state) {
		case GG_STATE_RESOLVING:
			purple_debug_info("gg", "GG_STATE_RESOLVING\n");
			break;
		case GG_STATE_RESOLVING_GG:
			purple_debug_info("gg", "GG_STATE_RESOLVING_GG\n");
			break;
		case GG_STATE_CONNECTING_HUB:
			purple_debug_info("gg", "GG_STATE_CONNECTING_HUB\n");
			break;
		case GG_STATE_READING_DATA:
			purple_debug_info("gg", "GG_STATE_READING_DATA\n");
			break;
		case GG_STATE_CONNECTING_GG:
			purple_debug_info("gg", "GG_STATE_CONNECTING_GG\n");
			break;
		case GG_STATE_READING_KEY:
			purple_debug_info("gg", "GG_STATE_READING_KEY\n");
			break;
		case GG_STATE_READING_REPLY:
			purple_debug_info("gg", "GG_STATE_READING_REPLY\n");
			break;
		case GG_STATE_TLS_NEGOTIATION:
			purple_debug_info("gg", "GG_STATE_TLS_NEGOTIATION\n");
			break;
		default:
			purple_debug_error("gg", "unknown state = %d\n",
					 info->session->state);
		break;
	}

	if (!(ev = gg_watch_fd(info->session))) {
		purple_debug_error("gg", "login_handler: gg_watch_fd failed!\n");
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to read from socket"));
		return;
	}
	purple_debug_info("gg", "login_handler: session->fd = %d\n", info->session->fd);
	purple_debug_info("gg", "login_handler: session: check = %d; state = %d;\n",
			info->session->check, info->session->state);

	purple_input_remove(info->inpa);
	info->inpa = 0;

	/** XXX I think that this shouldn't be done if ev->type is GG_EVENT_CONN_FAILED or GG_EVENT_CONN_SUCCESS -datallah */
	if (info->session->fd >= 0)
		info->inpa = purple_input_add(info->session->fd,
			(info->session->check == 1) ? PURPLE_INPUT_WRITE :
				PURPLE_INPUT_READ,
			ggp_async_login_handler, gc);

	switch (ev->type) {
		case GG_EVENT_NONE:
			/* Nothing happened. */
			purple_debug_info("gg", "GG_EVENT_NONE\n");
			break;
		case GG_EVENT_CONN_SUCCESS:
			{
				purple_debug_info("gg", "GG_EVENT_CONN_SUCCESS\n");
				purple_input_remove(info->inpa);
				info->inpa = purple_input_add(info->session->fd,
							  PURPLE_INPUT_READ,
							  ggp_callback_recv, gc);

				purple_connection_update_progress(gc, _("Connected"), 1, 2);
				purple_connection_set_state(gc, PURPLE_CONNECTED);
				
				ggp_buddylist_send(gc);
				ggp_roster_request_update(gc);
			}
			break;
		case GG_EVENT_CONN_FAILED:
			purple_input_remove(info->inpa);
			info->inpa = 0;
			purple_debug_info("gg", "Connection failure: %d\n",
				ev->event.failure);
			switch (ev->event.failure) {
				case GG_FAILURE_RESOLVING:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Unable to resolve "
						"hostname"));
					break;
				case GG_FAILURE_PASSWORD:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
						_("Incorrect password"));
					break;
				case GG_FAILURE_TLS:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
						_("SSL Connection Failed"));
					break;
				case GG_FAILURE_INTRUDER:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
						_("Your account has been "
						"disabled because too many "
						"incorrect passwords were "
						"entered"));
					break;
				case GG_FAILURE_UNAVAILABLE:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Service temporarily "
						"unavailable"));
					break;
				case GG_FAILURE_PROXY:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Error connecting to proxy "
						"server"));
					break;
				case GG_FAILURE_HUB:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Error connecting to master "
						"server"));
					break;
				default:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Connection failed"));
			}
			break;
		case GG_EVENT_MSG:
			if (ev->event.msg.sender == 0)
			{
				if (ev->event.msg.message == NULL)
					break;

				/* system messages are mostly ads */
				purple_debug_info("gg", "System message:\n%s\n",
					ev->event.msg.message);
			}
			else
				purple_debug_warning("gg", "GG_EVENT_MSG: message from user %u "
					"unexpected while connecting:\n%s\n",
					ev->event.msg.sender,
					ev->event.msg.message);
			break;
		default:
			purple_debug_error("gg", "strange event: %d\n", ev->type);
			break;
	}

	gg_free_event(ev);
}

/* ---------------------------------------------------------------------- */
/* ----- PurplePluginProtocolInfo ----------------------------------------- */
/* ---------------------------------------------------------------------- */

static const char *ggp_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "gadu-gadu";
}

static const char *ggp_normalize(const PurpleAccount *account, const char *who)
{
	static char normalized[21]; /* maximum unsigned long long int size */

	uin_t uin = ggp_str_to_uin(who);
	if (uin <= 0)
		return NULL;

	g_snprintf(normalized, sizeof(normalized), "%u", uin);

	return normalized;
}

static char *ggp_status_text(PurpleBuddy *b)
{
	const char *msg;
	char *text;
	char *tmp;
	ggp_buddy_data *buddy_data = purple_buddy_get_protocol_data(b);

	if (buddy_data && buddy_data->blocked)
		msg = _("Blocked");
	else
	{
		PurpleStatus *status = purple_presence_get_active_status(
			purple_buddy_get_presence(b));
		msg = purple_status_get_attr_string(status, "message");
	}

	if (msg == NULL)
		return NULL;

	tmp = purple_markup_strip_html(msg);
	text = g_markup_escape_text(tmp, -1);
	g_free(tmp);

	return text;
}

static void ggp_tooltip_text(PurpleBuddy *b, PurpleNotifyUserInfo *user_info, gboolean full)
{
	PurpleStatus *status;
	char *tmp;
	const char *msg, *name, *alias;

	g_return_if_fail(b != NULL);

	status = purple_presence_get_active_status(purple_buddy_get_presence(b));
	msg = purple_status_get_attr_string(status, "message");
	name = purple_status_get_name(status);
	alias = purple_buddy_get_alias(b);

	purple_notify_user_info_add_pair_plaintext(user_info, _("Alias"), alias);

	if (msg != NULL) {
		if (PURPLE_BUDDY_IS_ONLINE(b)) {
			tmp = g_strdup_printf("%s: %s", name, msg);
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), tmp);
			g_free(tmp);
		} else {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Message"), msg);
		}
	/* We don't want to duplicate 'Status: Offline'. */
	} else if (PURPLE_BUDDY_IS_ONLINE(b)) {
		purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), name);
	}
}

static GList *ggp_status_types(PurpleAccount *account)
{
	PurpleStatusType *type;
	GList *types = NULL;

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
		NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	/*
	 * New status for GG 8.0: PoGGadaj ze mna (chatty).
	 * NOTE: at this time, this is used only to set our own status.
	 */
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
		"freeforchat", _("Chatty"), TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY,
		NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	/*
	 * New status for GG 8.0: Nie przeszkadzac (do not disturb).
	 */
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE,
		NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	/*
	 * It's used on buddy list if and only if it's showing our own
	 * (invisible) status.
	 */
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_INVISIBLE,
		NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE,
		NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	return types;
}

static GList *ggp_blist_node_menu(PurpleBlistNode *node)
{
	PurpleMenuAction *act;
	GList *m = NULL;
	PurpleAccount *account;
	PurpleConnection *gc;
	GGPInfo *info;

	if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
		return NULL;

	account = purple_buddy_get_account((PurpleBuddy *) node);
	gc = purple_account_get_connection(account);
	info = purple_connection_get_protocol_data(gc);
	if (info->chats) {
		act = purple_menu_action_new(_("Add to chat"),
			PURPLE_CALLBACK(ggp_bmenu_add_to_chat),
			NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *ggp_chat_info(PurpleConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Chat _name:");
	pce->identifier = "name";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	return m;
}

static void ggp_login(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePresence *presence;
	PurpleStatus *status;
	struct gg_login_params *glp;
	GGPInfo *info;
	const char *address;
	const gchar *encryption_type;

	if (!ggp_deprecated_setup_proxy(gc))
		return;

	glp = g_new0(struct gg_login_params, 1);
	info = g_new0(GGPInfo, 1);

	/* Probably this should be moved to *_new() function. */
	info->session = NULL;
	info->chats = NULL;
	info->chats_count = 0;
	info->searches = ggp_search_new();
	info->status_broadcasting = purple_account_get_bool(account, "status_broadcasting", TRUE);
	
	purple_connection_set_protocol_data(gc, info);

	ggp_image_setup(gc);
	ggp_avatar_setup(gc);
	ggp_roster_setup(gc);
	ggp_multilogon_setup(gc);
	
	glp->uin = ggp_str_to_uin(purple_account_get_username(account));
	glp->password = ggp_convert_to_cp1250(purple_account_get_password(account));

	if (glp->uin == 0) {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_INVALID_USERNAME,
			_("The username specified is invalid."));
		g_free(glp);
		return;
	}

	glp->image_size = 255;
	glp->status_flags = GG_STATUS_FLAG_UNKNOWN;

	if (purple_account_get_bool(account, "show_links_from_strangers", 1))
		glp->status_flags |= GG_STATUS_FLAG_SPAM;

	presence = purple_account_get_presence(account);
	status = purple_presence_get_active_status(presence);

	glp->encoding = GG_ENCODING_UTF8;
	glp->protocol_features = (GG_FEATURE_DND_FFC |
		GG_FEATURE_TYPING_NOTIFICATION | GG_FEATURE_MULTILOGON |
		GG_FEATURE_USER_DATA);

	glp->async = 1;
	glp->status = ggp_to_gg_status(status, &glp->status_descr);
	info->old_status = g_strdup(glp->status_descr);
	
	encryption_type = purple_account_get_string(account, "encryption",
		"opportunistic_tls");
	purple_debug_info("gg", "Requested encryption type: %s\n",
		encryption_type);
	if (strcmp(encryption_type, "opportunistic_tls") == 0)
		glp->tls = GG_SSL_ENABLED;
	else if (strcmp(encryption_type, "require_tls") == 0) {
		if (gg_libgadu_check_feature(GG_LIBGADU_FEATURE_SSL))
			glp->tls = GG_SSL_REQUIRED;
		else {
			purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("SSL support unavailable"));
			g_free(glp);
			return;
		}
	}
	else /* encryption_type == "none" */
		glp->tls = GG_SSL_DISABLED;
	purple_debug_info("gg", "TLS mode: %d\n", glp->tls);

	if (!info->status_broadcasting)
		glp->status = glp->status|GG_STATUS_FRIENDS_MASK;
	
	address = purple_account_get_string(account, "gg_server", "");
	if (address && *address)
	{
		glp->server_addr = inet_addr(address);
		glp->server_port = 8074;
		
		if (glp->server_addr == INADDR_NONE)
		{
			purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
				_("Provided server IP address is not valid"));
			g_free(glp);
			return;
		}
	} else
		purple_debug_info("gg", "Trying to retrieve address from gg appmsg service\n");

	info->session = gg_login(glp);
	purple_connection_update_progress(gc, _("Connecting"), 0, 2);
	if (info->session == NULL) {
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Connection failed"));
		g_free(glp);
		return;
	}
	info->inpa = purple_input_add(info->session->fd, PURPLE_INPUT_READ,
				  ggp_async_login_handler, gc);
}

static void ggp_close(PurpleConnection *gc)
{
	PurpleAccount *account;
	GGPInfo *info;;

	if (gc == NULL) {
		purple_debug_info("gg", "gc == NULL\n");
		return;
	}

	account = purple_connection_get_account(gc);
	info = purple_connection_get_protocol_data(gc);

	if (info) {
		PurpleStatus *status = purple_account_get_active_status(account);

		if (info->session != NULL)
		{
			const gchar *status_msg = purple_status_get_attr_string(status, "message");
			
			if (ggp_set_status(account,
				status_msg ? GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL,
				status_msg))
			{
				struct gg_event *ev;
				guint64 wait_start = ggp_microtime(), now;
				int sleep_time = 5000;
				while ((ev = gg_watch_fd(info->session)) != NULL)
				{
					if (ev->type == GG_EVENT_DISCONNECT_ACK)
						break;
					now = ggp_microtime();
					if (now - wait_start + sleep_time >= 100000)
						break;
					usleep(sleep_time);
					sleep_time *= 2;
				}
			}
			gg_logoff(info->session);
			gg_free_session(info->session);
		}

		purple_account_set_bool(account, "status_broadcasting", info->status_broadcasting);

		/* Immediately close any notifications on this handle since that process depends
		 * upon the contents of info->searches, which we are about to destroy.
		 */
		purple_notify_close_with_handle(gc);

		ggp_search_destroy(info->searches);
		ggp_image_cleanup(gc);
		ggp_avatar_cleanup(gc);
		ggp_roster_cleanup(gc);
		ggp_multilogon_cleanup(gc);

		g_free(info->old_status);
		if (info->inpa > 0)
			purple_input_remove(info->inpa);

		purple_connection_set_protocol_data(gc, NULL);
		g_free(info);
	}

	purple_debug_info("gg", "Connection closed.\n");
}

static int ggp_send_im(PurpleConnection *gc, const char *who, const char *msg,
		       PurpleMessageFlags flags)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	char *tmp, *plain;
	int ret = 1;
	unsigned char format[1024];
	unsigned int format_length = sizeof(struct gg_msg_richtext);
	gint pos = 0;
	GData *attribs;
	const char *start, *end = NULL, *last;
	ggp_buddy_data *buddy_data = purple_buddy_get_protocol_data(
		purple_find_buddy(purple_connection_get_account(gc), who));

	if (msg == NULL || *msg == '\0') {
		return 0;
	}

	if (buddy_data && buddy_data->blocked)
		return -1;

	last = msg;

	/* Check if the message is richtext */
	/* TODO: Check formatting, too */
	if(purple_markup_find_tag("img", last, &start, &end, &attribs)) {

		GString *string_buffer = g_string_new(NULL);
		struct gg_msg_richtext fmt;

		do
		{
			const char *id = g_datalist_get_data(&attribs, "id");
			struct gg_msg_richtext_format actformat;
			struct gg_msg_richtext_image actimage;
			ggp_image_prepare_result prepare_result;

			/* Add text before the image */
			if(start - last)
			{
				pos = pos + g_utf8_strlen(last, start - last);
				g_string_append_len(string_buffer, last,
					start - last);
			}
			last = end + 1;
			
			if (id == NULL)
			{
				g_datalist_clear(&attribs);
				continue;
			}

			/* add the image itself */
			prepare_result = ggp_image_prepare(
				gc, atoi(id), who, &actimage);
			if (prepare_result == GGP_IMAGE_PREPARE_OK)
			{
				actformat.font = GG_FONT_IMAGE;
				actformat.position = pos;

				memcpy(format + format_length, &actformat,
					sizeof(actformat));
				format_length += sizeof(actformat);
				memcpy(format + format_length, &actimage,
					sizeof(actimage));
				format_length += sizeof(actimage);
			}
			else if (prepare_result == GGP_IMAGE_PREPARE_TOO_BIG)
			{
				PurpleConversation *conv =
					purple_find_conversation_with_account(
						PURPLE_CONV_TYPE_IM, who,
						purple_connection_get_account(gc));
				purple_conversation_write(conv, "",
					_("Image is too large, please try "
					"smaller one."), PURPLE_MESSAGE_ERROR,
					time(NULL));
			}
			
			g_datalist_clear(&attribs);
		} while (purple_markup_find_tag("img", last, &start, &end,
			&attribs));

		/* Add text after the images */
		if(last && *last) {
			pos = pos + g_utf8_strlen(last, -1);
			g_string_append(string_buffer, last);
		}

		fmt.flag = 2;
		fmt.length = format_length - sizeof(fmt);
		memcpy(format, &fmt, sizeof(fmt));

		purple_debug_info("gg", "ggp_send_im: richtext msg = %s\n", string_buffer->str);
		plain = purple_unescape_html(string_buffer->str);
		g_string_free(string_buffer, TRUE);
	} else {
		purple_debug_info("gg", "ggp_send_im: msg = %s\n", msg);
		plain = purple_unescape_html(msg);
	}

	tmp = g_strdup(plain);

	if (tmp && (format_length - sizeof(struct gg_msg_richtext))) {
		if(gg_send_message_richtext(info->session, GG_CLASS_CHAT, ggp_str_to_uin(who), (unsigned char *)tmp, format, format_length) < 0) {
			ret = -1;
		} else {
			ret = 1;
		}
	} else if (NULL == tmp || *tmp == 0) {
		ret = 0;
	} else if (strlen(tmp) > GG_MSG_MAXSIZE) {
		ret = -E2BIG;
	} else if (gg_send_message(info->session, GG_CLASS_CHAT,
				ggp_str_to_uin(who), (unsigned char *)tmp) < 0) {
		ret = -1;
	} else {
		ret = 1;
	}

	g_free(plain);
	g_free(tmp);

	return ret;
}

static unsigned int ggp_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState state)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	int dummy_length; // we don't send real length of typed message
	
	if (state == PURPLE_TYPED) // not supported
		return 1;
	
	if (state == PURPLE_TYPING)
		dummy_length = (int)g_random_int();
	else // PURPLE_NOT_TYPING
		dummy_length = 0;
	
	gg_typing_notification(
		info->session,
		ggp_str_to_uin(name),
		dummy_length); 
	
	return 1; // wait 1 second before another notification
}

static void ggp_get_info(PurpleConnection *gc, const char *name)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	GGPSearchForm *form;
	guint32 seq;

	form = ggp_search_form_new(GGP_SEARCH_TYPE_INFO);

	form->user_data = info;
	form->uin = g_strdup(name);

	seq = ggp_search_start(gc, form);
	ggp_search_add(info->searches, seq, form);
	purple_debug_info("gg", "ggp_get_info(): Added seq %u", seq);
}

static int ggp_to_gg_status(PurpleStatus *status, char **msg)
{
	const char *status_id = purple_status_get_id(status);
	int new_status, new_status_descr;
	const char *new_msg;

	g_return_val_if_fail(msg != NULL, 0);

	purple_debug_info("gg", "ggp_to_gg_status: Requested status = %s\n",
			status_id);

	if (strcmp(status_id, "available") == 0) {
		new_status = GG_STATUS_AVAIL;
		new_status_descr = GG_STATUS_AVAIL_DESCR;
	} else if (strcmp(status_id, "freeforchat") == 0) {
		new_status = GG_STATUS_FFC;
		new_status_descr = GG_STATUS_FFC_DESCR;
	} else if (strcmp(status_id, "away") == 0) {
		new_status = GG_STATUS_BUSY;
		new_status_descr = GG_STATUS_BUSY_DESCR;
	} else if (strcmp(status_id, "unavailable") == 0) {
		new_status = GG_STATUS_DND;
		new_status_descr = GG_STATUS_DND_DESCR;
	} else if (strcmp(status_id, "invisible") == 0) {
		new_status = GG_STATUS_INVISIBLE;
		new_status_descr = GG_STATUS_INVISIBLE_DESCR;
	} else if (strcmp(status_id, "offline") == 0) {
		new_status = GG_STATUS_NOT_AVAIL;
		new_status_descr = GG_STATUS_NOT_AVAIL_DESCR;
	} else {
		new_status = GG_STATUS_AVAIL;
		new_status_descr = GG_STATUS_AVAIL_DESCR;
		purple_debug_info("gg",
			"ggp_set_status: unknown status requested (status_id=%s)\n",
			status_id);
	}

	new_msg = purple_status_get_attr_string(status, "message");

	if(new_msg) {
		*msg = purple_markup_strip_html(new_msg);
		return new_status_descr;
	} else {
		*msg = NULL;
		return new_status;
	}
}

static void ggp_set_purplestatus(PurpleAccount *account, PurpleStatus *status)
{
	int new_status;
	gchar *new_msg = NULL;
	
	if (!purple_status_is_active(status))
		return;

	new_status = ggp_to_gg_status(status, &new_msg);
	
	ggp_set_status(account, new_status, new_msg);
	g_free(new_msg);
}

static gboolean ggp_set_status(PurpleAccount *account, int status, const gchar* msg)
{
	PurpleConnection *gc;
	GGPInfo *info;
	gboolean new_msg_differs;

	gc = purple_account_get_connection(account);
	info = purple_connection_get_protocol_data(gc);

	if (!info->status_broadcasting)
		status = status|GG_STATUS_FRIENDS_MASK;
	
	new_msg_differs = (0 != g_strcmp0(info->old_status, msg));
	g_free(info->old_status);
	info->old_status = g_strdup(msg);
	if (!new_msg_differs && (status == GG_STATUS_NOT_AVAIL || status == GG_STATUS_NOT_AVAIL_DESCR))
	{
		purple_debug_info("gg", "ggp_set_status: new status doesn't differ when closing connection - ignore\n");
		return FALSE;
	}
	
	if (msg == NULL) {
		gg_change_status(info->session, status);
	} else {
		gg_change_status_descr(info->session, status, msg);
	}
	
	return TRUE;
}

static void ggp_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group, const char *message)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	const gchar *name = purple_buddy_get_name(buddy);

	gg_add_notify(info->session, ggp_str_to_uin(name));

	// gg server won't tell us our status
	if (strcmp(purple_account_get_username(account), name) == 0)
	{
		PurpleStatus *status = purple_presence_get_active_status(
			purple_account_get_presence(account));
		const char *status_msg = purple_status_get_attr_string(status,
			"message");
		gchar *status_msg_gg = NULL;
		
		if (status_msg != NULL && status_msg[0] != '\0')
		{
			status_msg_gg = g_new0(gchar,
				GG_STATUS_DESCR_MAXSIZE + 1);
			g_utf8_strncpy(status_msg_gg, status_msg,
				GG_STATUS_DESCR_MAXSIZE);
		}
		
		purple_prpl_got_user_status(account,
			purple_account_get_username(account),
			purple_status_get_id(status),
			status_msg_gg ? "message" : NULL, status_msg_gg, NULL);
		
		g_free(status_msg_gg);
	}
	
	ggp_roster_add_buddy(gc, buddy, group, message);
}

static void ggp_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
						 PurpleGroup *group)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);

	gg_remove_notify(info->session, ggp_str_to_uin(purple_buddy_get_name(buddy)));
	ggp_roster_remove_buddy(gc, buddy, group);
}

static void ggp_join_chat(PurpleConnection *gc, GHashTable *data)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	GGPChat *chat;
	char *chat_name;
	GList *l;
	PurpleConversation *conv;
	PurpleAccount *account = purple_connection_get_account(gc);

	chat_name = g_hash_table_lookup(data, "name");

	if (chat_name == NULL)
		return;

	purple_debug_info("gg", "joined %s chat\n", chat_name);

	for (l = info->chats; l != NULL; l = l->next) {
		 chat = l->data;

		 if (chat != NULL && g_utf8_collate(chat->name, chat_name) == 0) {
			 purple_notify_error(gc, _("Chat error"),
				 _("This chat name is already in use"), NULL);
			 return;
		 }
	}

	ggp_confer_add_new(gc, chat_name);
	conv = serv_got_joined_chat(gc, info->chats_count, chat_name);
	purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
				purple_account_get_username(account), NULL,
				PURPLE_CBFLAGS_NONE, TRUE);
}

static char *ggp_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "name"));
}

static int ggp_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	PurpleConversation *conv;
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	GGPChat *chat = NULL;
	GList *l;
	/* char *msg, *plain; */
	gchar *msg;
	uin_t *uins;
	int count = 0;

	if ((conv = purple_find_chat(gc, id)) == NULL)
		return -EINVAL;

	for (l = info->chats; l != NULL; l = l->next) {
		chat = l->data;

		if (g_utf8_collate(chat->name, purple_conversation_get_name(conv)) == 0) {
			break;
		}

		chat = NULL;
	}

	if (chat == NULL) {
		purple_debug_error("gg",
			"ggp_chat_send: Hm... that's strange. No such chat?\n");
		return -EINVAL;
	}

	uins = g_new0(uin_t, g_list_length(chat->participants));

	for (l = chat->participants; l != NULL; l = l->next) {
		uin_t uin = GPOINTER_TO_INT(l->data);

		uins[count++] = uin;
	}

	msg = purple_unescape_html(message);
	gg_send_message_confer(info->session, GG_CLASS_CHAT, count, uins,
				(unsigned char *)msg);
	g_free(msg);
	g_free(uins);

	serv_got_chat_in(gc, id,
			 purple_account_get_username(purple_connection_get_account(gc)),
			 flags, message, time(NULL));

	return 0;
}

static void ggp_keepalive(PurpleConnection *gc)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);

	/* purple_debug_info("gg", "Keeping connection alive....\n"); */

	if (gg_ping(info->session) < 0) {
		purple_debug_info("gg", "Not connected to the server "
				"or gg_session is not correct\n");
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Not connected to the server"));
	}
}

static void ggp_action_chpass(PurplePluginAction *action)
{
	ggp_account_chpass((PurpleConnection *)action->context);
}

static GList *ggp_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Change password..."),
		ggp_action_chpass);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Find buddies..."),
				     ggp_find_buddies);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Change status broadcasting"),
				     ggp_action_change_status_broadcasting);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);

	act = purple_plugin_action_new(_("Save buddylist to file..."),
				     ggp_action_buddylist_save);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Load buddylist from file..."),
				     ggp_action_buddylist_load);
	m = g_list_append(m, act);

	return m;
}

static const char* ggp_list_emblem(PurpleBuddy *buddy)
{
	ggp_buddy_data *buddy_data = purple_buddy_get_protocol_data(buddy);

	if (!buddy_data)
		return NULL;

	if (buddy_data->blocked)
		return "not-authorized";

	return NULL;
}

static void ggp_buddy_free(PurpleBuddy *buddy)
{
	ggp_buddy_data *buddy_data = purple_buddy_get_protocol_data(buddy);

	if (!buddy_data)
		return;

	g_free(buddy_data);

	purple_buddy_set_protocol_data(buddy, NULL);
}

static gboolean ggp_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

static GHashTable * ggp_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("GG number..."));
	return table;
}

static PurplePluginProtocolInfo prpl_info =
{
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
	OPT_PROTO_REGISTER_NOSCREENNAME | OPT_PROTO_IM_IMAGE,
	NULL,				/* user_splits */
	NULL,				/* protocol_options */
	{"png", 1, 1, 200, 200, 0, PURPLE_ICON_SCALE_DISPLAY | PURPLE_ICON_SCALE_SEND},	/* icon_spec */
	ggp_list_icon,			/* list_icon */
	ggp_list_emblem,		/* list_emblem */
	ggp_status_text,		/* status_text */
	ggp_tooltip_text,		/* tooltip_text */
	ggp_status_types,		/* status_types */
	ggp_blist_node_menu,		/* blist_node_menu */
	ggp_chat_info,			/* chat_info */
	NULL,				/* chat_info_defaults */
	ggp_login,			/* login */
	ggp_close,			/* close */
	ggp_send_im,			/* send_im */
	NULL,				/* set_info */
	ggp_send_typing,		/* send_typing */
	ggp_get_info,			/* get_info */
	ggp_set_purplestatus,		/* set_away */
	NULL,				/* set_idle */
	NULL,				/* change_passwd */
	ggp_add_buddy,			/* add_buddy */
	NULL,				/* add_buddies */
	ggp_remove_buddy,		/* remove_buddy */
	NULL,				/* remove_buddies */
	NULL,				/* add_permit */
	ggp_add_deny,			/* add_deny */
	NULL,				/* rem_permit */
	ggp_rem_deny,			/* rem_deny */
	NULL,				/* set_permit_deny */
	ggp_join_chat,			/* join_chat */
	NULL,				/* reject_chat */
	ggp_get_chat_name,		/* get_chat_name */
	NULL,				/* chat_invite */
	NULL,				/* chat_leave */
	NULL,				/* chat_whisper */
	ggp_chat_send,			/* chat_send */
	ggp_keepalive,			/* keepalive */
	ggp_account_register,		/* register_user */
	NULL,				/* get_cb_info */
	ggp_roster_alias_buddy,		/* alias_buddy */
	ggp_roster_group_buddy,		/* group_buddy */
	ggp_roster_rename_group,	/* rename_group */
	ggp_buddy_free,			/* buddy_free */
	NULL,				/* convo_closed */
	ggp_normalize,			/* normalize */
	ggp_avatar_own_set,		/* set_buddy_icon */
	NULL,				/* remove_group */
	NULL,				/* get_cb_real_name */
	NULL,				/* set_chat_topic */
	NULL,				/* find_blist_chat */
	NULL,				/* roomlist_get_list */
	NULL,				/* roomlist_cancel */
	NULL,				/* roomlist_expand_category */
	NULL,				/* can_receive_file */
	NULL,				/* send_file */
	NULL,				/* new_xfer */
	ggp_offline_message,		/* offline_message */
	NULL,				/* whiteboard_prpl_ops */
	NULL,				/* send_raw */
	NULL,				/* roomlist_room_serialize */
	NULL,				/* unregister_user */
	NULL,				/* send_attention */
	NULL,				/* get_attention_types */
	ggp_get_account_text_table,	/* get_account_text_table */
	NULL,                           /* initiate_media */
	NULL,                            /* can_do_media */
	NULL,				/* get_moods */
	NULL,				/* set_public_alias */
	NULL				/* get_public_alias */
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,			/* magic */
	PURPLE_MAJOR_VERSION,			/* major_version */
	PURPLE_MINOR_VERSION,			/* minor_version */
	PURPLE_PLUGIN_PROTOCOL,			/* plugin type */
	NULL,					/* ui_requirement */
	0,					/* flags */
	NULL,					/* dependencies */
	PURPLE_PRIORITY_DEFAULT,		/* priority */

	"prpl-gg",				/* id */
	"Gadu-Gadu",				/* name */
	DISPLAY_VERSION,			/* version */

	N_("Gadu-Gadu Protocol Plugin"),	/* summary */
	N_("Polish popular IM"),		/* description */
	"boler@sourceforge.net",		/* author */
	PURPLE_WEBSITE,				/* homepage */

	NULL,					/* load */
	NULL,					/* unload */
	NULL,					/* destroy */

	NULL,					/* ui_info */
	&prpl_info,				/* extra_info */
	NULL,					/* prefs_info */
	ggp_actions,				/* actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void purple_gg_debug_handler(int level, const char * format, va_list args) {
	PurpleDebugLevel purple_level;
	char *msg = g_strdup_vprintf(format, args);

	/* This is pretty pointless since the GG_DEBUG levels don't correspond to
	 * the purple ones */
	switch (level) {
		case GG_DEBUG_FUNCTION:
			purple_level = PURPLE_DEBUG_INFO;
			break;
		case GG_DEBUG_MISC:
		case GG_DEBUG_NET:
		case GG_DEBUG_DUMP:
		case GG_DEBUG_TRAFFIC:
		default:
			purple_level = PURPLE_DEBUG_MISC;
			break;
	}

	purple_debug(purple_level, "gg", "%s", msg);
	g_free(msg);
}

static void init_plugin(PurplePlugin *plugin)
{
	PurpleAccountOption *option;
	GList *encryption_options = NULL;

	purple_debug_info("gg", "Loading Gadu-Gadu protocol plugin with "
		"libgadu %s...\n", gg_libgadu_version());

	option = purple_account_option_string_new(_("GG server"),
			"gg_server", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

#define ADD_VALUE(list, desc, v) { \
	PurpleKeyValuePair *kvp = g_new0(PurpleKeyValuePair, 1); \
	kvp->key = g_strdup((desc)); \
	kvp->value = g_strdup((v)); \
	list = g_list_append(list, kvp); \
}

	ADD_VALUE(encryption_options, _("Use encryption if available"),
		"opportunistic_tls");
	ADD_VALUE(encryption_options, _("Require encryption"), "require_tls");
	ADD_VALUE(encryption_options, _("Don't use encryption"), "none");

	option = purple_account_option_list_new(_("Connection security"),
		"encryption", encryption_options);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
		option);

	option = purple_account_option_bool_new(_("Show links from strangers"),
		"show_links_from_strangers", 1);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
		option);

	gg_debug_handler = purple_gg_debug_handler;
	
	ggp_resolver_purple_setup();
}

PURPLE_INIT_PLUGIN(gg, init_plugin, info);

/* vim: set ts=8 sts=0 sw=8 noet: */
