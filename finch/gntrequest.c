/**
 * @file gntrequest.c GNT Request API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <internal.h>

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntfilesel.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>

#include "finch.h"
#include "gntrequest.h"
#include "debug.h"
#include "util.h"

/* XXX: Until gobjectification ... */
#undef FINCH_GET_DATA
#undef FINCH_SET_DATA
#define FINCH_GET_DATA(obj)  purple_request_field_get_ui_data(obj)
#define FINCH_SET_DATA(obj, data)  purple_request_field_set_ui_data(obj, data)

typedef struct
{
	void *user_data;
	GntWidget *dialog;
	GCallback *cbs;
	gboolean save;
} FinchFileRequest;

static GntWidget *
setup_request_window(const char *title, const char *primary,
		const char *secondary, PurpleRequestType type)
{
	GntWidget *window;

	window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), title);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);

	if (primary)
		gnt_box_add_widget(GNT_BOX(window),
				gnt_label_new_with_format(primary, GNT_TEXT_FLAG_BOLD));
	if (secondary)
		gnt_box_add_widget(GNT_BOX(window), gnt_label_new(secondary));

	g_signal_connect_swapped(G_OBJECT(window), "destroy", G_CALLBACK(purple_request_close),
			GINT_TO_POINTER(type));

	return window;
}

/**
 * If the window is closed by the wm (ie, without triggering any of
 * the buttons, then do some default callback.
 */
static void
setup_default_callback(GntWidget *window, gpointer default_cb, gpointer data)
{
	if (default_cb == NULL)
		return;
	g_object_set_data(G_OBJECT(window), "default-callback", default_cb);
	g_signal_connect_swapped(G_OBJECT(window), "destroy", G_CALLBACK(default_cb), data);
}

static void
action_performed(GntWidget *button, gpointer data)
{
	g_signal_handlers_disconnect_matched(data, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL,
			g_object_get_data(data, "default-callback"),
			 NULL);
}

/**
 * window: this is the window
 * userdata: the userdata to pass to the primary callbacks
 * cb: the callback
 * data: data for the callback
 * (text, primary-callback) pairs, ended by a NULL
 *
 * The cancellation callback should be the last callback sent.
 */
static GntWidget *
setup_button_box(GntWidget *win, gpointer userdata, gpointer cb, gpointer data, ...)
{
	GntWidget *box;
	GntWidget *button = NULL;
	va_list list;
	const char *text;
	gpointer callback;

	box = gnt_hbox_new(FALSE);

	va_start(list, data);

	while ((text = va_arg(list, const char *)))
	{
		callback = va_arg(list, gpointer);
		button = gnt_button_new(text);
		gnt_box_add_widget(GNT_BOX(box), button);
		g_object_set_data(G_OBJECT(button), "activate-callback", callback);
		g_object_set_data(G_OBJECT(button), "activate-userdata", userdata);
		g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(action_performed), win);
		g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(cb), data);
	}

	if (button)
		g_object_set_data(G_OBJECT(button), "cancellation-function", GINT_TO_POINTER(TRUE));

	va_end(list);
	return box;
}

static void
notify_input_cb(GntWidget *button, GntWidget *entry)
{
	PurpleRequestInputCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	const char *text = gnt_entry_get_text(GNT_ENTRY(entry));
	GntWidget *window;

	if (callback)
		callback(data, text);

	window = gnt_widget_get_toplevel(button);
	purple_request_close(PURPLE_REQUEST_INPUT, window);
}

static void *
finch_request_input(const char *title, const char *primary,
		const char *secondary, const char *default_value,
		gboolean multiline, gboolean masked, gchar *hint,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		PurpleAccount *account, const char *who, PurpleConversation *conv,
		void *user_data)
{
	GntWidget *window, *box, *entry;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_INPUT);

	entry = gnt_entry_new(default_value);
	if (masked)
		gnt_entry_set_masked(GNT_ENTRY(entry), TRUE);
	gnt_box_add_widget(GNT_BOX(window), entry);

	box = setup_button_box(window, user_data, notify_input_cb, entry,
			ok_text, ok_cb, cancel_text, cancel_cb, NULL);
	gnt_box_add_widget(GNT_BOX(window), box);

	setup_default_callback(window, cancel_cb, user_data);
	gnt_widget_show(window);

	return window;
}

static void
finch_close_request(PurpleRequestType type, gpointer ui_handle)
{
	GntWidget *widget = GNT_WIDGET(ui_handle);
	if (type == PURPLE_REQUEST_FIELDS) {
		PurpleRequestFields *fields = g_object_get_data(G_OBJECT(widget), "fields");
		purple_request_fields_destroy(fields);
	}

	widget = gnt_widget_get_toplevel(widget);
	gnt_widget_destroy(widget);
}

static void
request_choice_cb(GntWidget *button, GntComboBox *combo)
{
	PurpleRequestChoiceCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	int choice = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo))) - 1;
	GntWidget *window;

	if (callback)
		callback(data, choice);

	window = gnt_widget_get_toplevel(button);
	purple_request_close(PURPLE_REQUEST_INPUT, window);
}

static void *
finch_request_choice(const char *title, const char *primary,
		const char *secondary, int default_value,
		const char *ok_text, GCallback ok_cb,
		const char *cancel_text, GCallback cancel_cb,
		PurpleAccount *account, const char *who, PurpleConversation *conv,
		void *user_data, va_list choices)
{
	GntWidget *window, *combo, *box;
	const char *text;
	int val;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_CHOICE);

	combo = gnt_combo_box_new();
	gnt_box_add_widget(GNT_BOX(window), combo);
	while ((text = va_arg(choices, const char *)))
	{
		val = va_arg(choices, int);
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), GINT_TO_POINTER(val + 1), text);
	}
	gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), GINT_TO_POINTER(default_value + 1));

	box = setup_button_box(window, user_data, request_choice_cb, combo,
			ok_text, ok_cb, cancel_text, cancel_cb, NULL);
	gnt_box_add_widget(GNT_BOX(window), box);

	setup_default_callback(window, cancel_cb, user_data);
	gnt_widget_show(window);

	return window;
}

static void
request_action_cb(GntWidget *button, GntWidget *window)
{
	PurpleRequestActionCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "activate-id"));

	if (callback)
		callback(data, id);

	purple_request_close(PURPLE_REQUEST_ACTION, window);
}

static void*
finch_request_action(const char *title, const char *primary,
		const char *secondary, int default_value,
		PurpleAccount *account, const char *who, PurpleConversation *conv,
		void *user_data, size_t actioncount,
		va_list actions)
{
	GntWidget *window, *box, *button, *focus = NULL;
	gsize i;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_ACTION);

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);
	for (i = 0; i < actioncount; i++)
	{
		const char *text = va_arg(actions, const char *);
		PurpleRequestActionCb callback = va_arg(actions, PurpleRequestActionCb);

		button = gnt_button_new(text);
		gnt_box_add_widget(GNT_BOX(box), button);

		g_object_set_data(G_OBJECT(button), "activate-callback", callback);
		g_object_set_data(G_OBJECT(button), "activate-userdata", user_data);
		g_object_set_data(G_OBJECT(button), "activate-id", GINT_TO_POINTER(i));
		g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(request_action_cb), window);

		if (default_value >= 0 && i == (gsize)default_value)
			focus = button;
	}

	gnt_widget_show(window);
	if (focus)
		gnt_box_give_focus_to_child(GNT_BOX(window), focus);

	return window;
}

static void
request_fields_cb(GntWidget *button, PurpleRequestFields *fields)
{
	PurpleRequestFieldsCb callback = g_object_get_data(G_OBJECT(button), "activate-callback");
	gpointer data = g_object_get_data(G_OBJECT(button), "activate-userdata");
	GList *list;
	GntWidget *window;

	/* Update the data of the fields. Pidgin does this differently. Instead of
	 * updating the fields at the end like here, it updates the appropriate field
	 * instantly whenever a change is made. That allows it to make sure the
	 * 'required' fields are entered before the user can hit OK. It's not the case
	 * here, althought it can be done. */
	for (list = purple_request_fields_get_groups(fields); list; list = list->next)
	{
		PurpleRequestFieldGroup *group = list->data;
		GList *fields = purple_request_field_group_get_fields(group);

		for (; fields ; fields = fields->next)
		{
			PurpleRequestField *field = fields->data;
			PurpleRequestFieldType type = purple_request_field_get_type(field);
			if (!purple_request_field_is_visible(field))
				continue;
			if (type == PURPLE_REQUEST_FIELD_BOOLEAN)
			{
				GntWidget *check = FINCH_GET_DATA(field);
				gboolean value = gnt_check_box_get_checked(GNT_CHECK_BOX(check));
				purple_request_field_bool_set_value(field, value);
			}
			else if (type == PURPLE_REQUEST_FIELD_STRING)
			{
				GntWidget *entry = FINCH_GET_DATA(field);
				const char *text = gnt_entry_get_text(GNT_ENTRY(entry));
				purple_request_field_string_set_value(field, (text && *text) ? text : NULL);
			}
			else if (type == PURPLE_REQUEST_FIELD_INTEGER)
			{
				GntWidget *entry = FINCH_GET_DATA(field);
				const char *text = gnt_entry_get_text(GNT_ENTRY(entry));
				int value = (text && *text) ? atoi(text) : 0;
				purple_request_field_int_set_value(field, value);
			}
			else if (type == PURPLE_REQUEST_FIELD_CHOICE)
			{
				GntWidget *combo = FINCH_GET_DATA(field);
				int id;
				id = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo)));
				purple_request_field_choice_set_value(field, id);
			}
			else if (type == PURPLE_REQUEST_FIELD_LIST)
			{
				GList *list = NULL, *iter;
				if (purple_request_field_list_get_multi_select(field))
				{
					GntWidget *tree = FINCH_GET_DATA(field);

					iter = purple_request_field_list_get_items(field);
					for (; iter; iter = iter->next)
					{
						const char *text = iter->data;
						gpointer key = purple_request_field_list_get_data(field, text);
						if (gnt_tree_get_choice(GNT_TREE(tree), key))
							list = g_list_prepend(list, (gpointer)text);
					}
				}
				else
				{
					GntWidget *combo = FINCH_GET_DATA(field);
					gpointer data = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo));

					iter = purple_request_field_list_get_items(field);
					for (; iter; iter = iter->next) {
						const char *text = iter->data;
						gpointer key = purple_request_field_list_get_data(field, text);
						if (key == data) {
							list = g_list_prepend(list, (gpointer)text);
							break;
						}
					}
				}

				purple_request_field_list_set_selected(field, list);
				g_list_free(list);
			}
			else if (type == PURPLE_REQUEST_FIELD_ACCOUNT)
			{
				GntWidget *combo = FINCH_GET_DATA(field);
				PurpleAccount *acc = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(combo));
				purple_request_field_account_set_value(field, acc);
			}
		}
	}

	purple_notify_close_with_handle(button);

	if (!g_object_get_data(G_OBJECT(button), "cancellation-function") &&
			!purple_request_fields_all_required_filled(fields)) {
		purple_notify_error(button, _("Error"),
				_("You must fill all the required fields."),
				_("The required fields are underlined."));
		return;
	}

	if (callback)
		callback(data, fields);

	window = gnt_widget_get_toplevel(button);
	purple_request_close(PURPLE_REQUEST_FIELDS, window);
}

static void
update_selected_account(GntEntry *username, const char *start, const char *end,
		GntComboBox *accountlist)
{
	GList *accounts =
		gnt_tree_get_rows(GNT_TREE(gnt_combo_box_get_dropdown(accountlist)));
	const char *name = gnt_entry_get_text(username);
	while (accounts) {
		if (purple_find_buddy(accounts->data, name)) {
			gnt_combo_box_set_selected(accountlist, accounts->data);
			gnt_widget_draw(GNT_WIDGET(accountlist));
			break;
		}
		accounts = accounts->next;
	}
}

static GntWidget*
create_boolean_field(PurpleRequestField *field)
{
	const char *label = purple_request_field_get_label(field);
	GntWidget *check = gnt_check_box_new(label);
	gnt_check_box_set_checked(GNT_CHECK_BOX(check),
			purple_request_field_bool_get_default_value(field));
	return check;
}

static GntWidget*
create_string_field(PurpleRequestField *field, GntWidget **username)
{
	const char *hint = purple_request_field_get_type_hint(field);
	GntWidget *entry = gnt_entry_new(
			purple_request_field_string_get_default_value(field));
	gnt_entry_set_masked(GNT_ENTRY(entry),
			purple_request_field_string_is_masked(field));
	if (hint && purple_str_has_prefix(hint, "screenname")) {
		PurpleBlistNode *node = purple_blist_get_root();
		gboolean offline = purple_str_has_suffix(hint, "all");
		for (; node; node = purple_blist_node_next(node, offline)) {
			if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
				continue;
			gnt_entry_add_suggest(GNT_ENTRY(entry), purple_buddy_get_name((PurpleBuddy*)node));
		}
		gnt_entry_set_always_suggest(GNT_ENTRY(entry), TRUE);
		if (username)
			*username = entry;
	} else if (purple_strequal(hint, "group")) {
		PurpleBlistNode *node;
		for (node = purple_blist_get_root(); node;
				node = purple_blist_node_get_sibling_next(node)) {
			if (PURPLE_BLIST_NODE_IS_GROUP(node))
				gnt_entry_add_suggest(GNT_ENTRY(entry), purple_group_get_name((PurpleGroup *)node));
		}
	}
	return entry;
}

static GntWidget*
create_integer_field(PurpleRequestField *field)
{
	char str[256];
	int val = purple_request_field_int_get_default_value(field);
	GntWidget *entry;

	snprintf(str, sizeof(str), "%d", val);
	entry = gnt_entry_new(str);
	gnt_entry_set_flag(GNT_ENTRY(entry), GNT_ENTRY_FLAG_INT);
	return entry;
}

static GntWidget*
create_choice_field(PurpleRequestField *field)
{
	int id;
	GList *list;
	GntWidget *combo = gnt_combo_box_new();

	list = purple_request_field_choice_get_labels(field);
	for (id = 1; list; list = list->next, id++)
	{
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo),
				GINT_TO_POINTER(id), list->data);
	}
	gnt_combo_box_set_selected(GNT_COMBO_BOX(combo),
			GINT_TO_POINTER(purple_request_field_choice_get_default_value(field)));
	return combo;
}

static GntWidget*
create_list_field(PurpleRequestField *field)
{
	GntWidget *ret = NULL;
	GList *list;
	gboolean multi = purple_request_field_list_get_multi_select(field);
	if (multi)
	{
		GntWidget *tree = gnt_tree_new();

		list = purple_request_field_list_get_items(field);
		for (; list; list = list->next)
		{
			const char *text = list->data;
			gpointer key = purple_request_field_list_get_data(field, text);
			gnt_tree_add_choice(GNT_TREE(tree), key,
					gnt_tree_create_row(GNT_TREE(tree), text), NULL, NULL);
			if (purple_request_field_list_is_selected(field, text))
				gnt_tree_set_choice(GNT_TREE(tree), key, TRUE);
		}
		ret = tree;
	}
	else
	{
		GntWidget *combo = gnt_combo_box_new();

		list = purple_request_field_list_get_items(field);
		for (; list; list = list->next)
		{
			const char *text = list->data;
			gpointer key = purple_request_field_list_get_data(field, text);
			gnt_combo_box_add_data(GNT_COMBO_BOX(combo), key, text);
			if (purple_request_field_list_is_selected(field, text))
				gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), key);
		}
		ret = combo;
	}
	return ret;
}

static GntWidget*
create_account_field(PurpleRequestField *field)
{
	gboolean all;
	PurpleAccount *def;
	GList *list;
	GntWidget *combo = gnt_combo_box_new();

	all = purple_request_field_account_get_show_all(field);
	def = purple_request_field_account_get_value(field);
	if (!def)
		def = purple_request_field_account_get_default_value(field);

	if (all)
		list = purple_accounts_get_all();
	else
		list = purple_connections_get_all();

	for (; list; list = list->next)
	{
		PurpleAccount *account;
		char *text;

		if (all)
			account = list->data;
		else
			account = purple_connection_get_account(list->data);

		text = g_strdup_printf("%s (%s)",
				purple_account_get_username(account),
				purple_account_get_protocol_name(account));
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), account, text);
		g_free(text);
		if (account == def)
			gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), account);
	}
	gnt_widget_set_size(combo, 20, 3); /* ew */
	return combo;
}

static void *
finch_request_fields(const char *title, const char *primary,
		const char *secondary, PurpleRequestFields *allfields,
		const char *ok, GCallback ok_cb,
		const char *cancel, GCallback cancel_cb,
		PurpleAccount *account, const char *who, PurpleConversation *conv,
		void *userdata)
{
	GntWidget *window, *box;
	GList *grlist;
	GntWidget *username = NULL, *accountlist = NULL;

	window = setup_request_window(title, primary, secondary, PURPLE_REQUEST_FIELDS);

	/* This is how it's going to work: the request-groups are going to be
	 * stacked vertically one after the other. A GntLine will be separating
	 * the groups. */
	box = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(box), 0);
	gnt_box_set_fill(GNT_BOX(box), TRUE);
	for (grlist = purple_request_fields_get_groups(allfields); grlist; grlist = grlist->next)
	{
		PurpleRequestFieldGroup *group = grlist->data;
		GList *fields = purple_request_field_group_get_fields(group);
		GntWidget *hbox;
		const char *title = purple_request_field_group_get_title(group);

		if (title)
			gnt_box_add_widget(GNT_BOX(box),
					gnt_label_new_with_format(title, GNT_TEXT_FLAG_BOLD));

		for (; fields ; fields = fields->next)
		{
			PurpleRequestField *field = fields->data;
			PurpleRequestFieldType type = purple_request_field_get_type(field);
			const char *label = purple_request_field_get_label(field);

			if (!purple_request_field_is_visible(field))
				continue;

			hbox = gnt_hbox_new(TRUE);   /* hrm */
			gnt_box_add_widget(GNT_BOX(box), hbox);

			if (type != PURPLE_REQUEST_FIELD_BOOLEAN && label)
			{
				GntWidget *l;
				if (purple_request_field_is_required(field))
					l = gnt_label_new_with_format(label, GNT_TEXT_FLAG_UNDERLINE);
				else
					l = gnt_label_new(label);
				gnt_widget_set_size(l, 0, 1);
				gnt_box_add_widget(GNT_BOX(hbox), l);
			}

			if (type == PURPLE_REQUEST_FIELD_BOOLEAN)
			{
				FINCH_SET_DATA(field, create_boolean_field(field));
			}
			else if (type == PURPLE_REQUEST_FIELD_STRING)
			{
				FINCH_SET_DATA(field, create_string_field(field, &username));
			}
			else if (type == PURPLE_REQUEST_FIELD_INTEGER)
			{
				FINCH_SET_DATA(field, create_integer_field(field));
			}
			else if (type == PURPLE_REQUEST_FIELD_CHOICE)
			{
				FINCH_SET_DATA(field, create_choice_field(field));
			}
			else if (type == PURPLE_REQUEST_FIELD_LIST)
			{
				FINCH_SET_DATA(field, create_list_field(field));
			}
			else if (type == PURPLE_REQUEST_FIELD_ACCOUNT)
			{
				accountlist = create_account_field(field);
				FINCH_SET_DATA(field, accountlist);
			}
			else
			{
				FINCH_SET_DATA(field, gnt_label_new_with_format(_("Not implemented yet."),
						GNT_TEXT_FLAG_BOLD));
			}
			gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);
			gnt_box_add_widget(GNT_BOX(hbox), GNT_WIDGET(FINCH_GET_DATA(field)));
		}
		if (grlist->next)
			gnt_box_add_widget(GNT_BOX(box), gnt_hline_new());
	}
	gnt_box_add_widget(GNT_BOX(window), box);

	box = setup_button_box(window, userdata, request_fields_cb, allfields,
			ok, ok_cb, cancel, cancel_cb, NULL);
	gnt_box_add_widget(GNT_BOX(window), box);

	setup_default_callback(window, cancel_cb, userdata);
	gnt_widget_show(window);

	if (username && accountlist) {
		g_signal_connect(username, "completion", G_CALLBACK(update_selected_account), accountlist);
	}

	g_object_set_data(G_OBJECT(window), "fields", allfields);

	return window;
}

static void
file_cancel_cb(GntWidget *wid, gpointer fq)
{
	FinchFileRequest *data = fq;
	if (data->dialog == NULL) {
		/* We've already handled this request, and are in the destroy handler. */
		return;
	}

	if (data->cbs[1] != NULL)
		((PurpleRequestFileCb)data->cbs[1])(data->user_data, NULL);

	purple_request_close(PURPLE_REQUEST_FILE, data->dialog);
	data->dialog = NULL;
}

static void
file_ok_cb(GntWidget *widget, const char *path, const char *file, gpointer fq)
{
	FinchFileRequest *data = fq;
	char *dir = g_path_get_dirname(path);
	if (data->cbs[0] != NULL)
		((PurpleRequestFileCb)data->cbs[0])(data->user_data, file);
	purple_prefs_set_path(data->save ? "/finch/filelocations/last_save_folder" :
			"/finch/filelocations/last_open_folder", dir);
	g_free(dir);

	purple_request_close(PURPLE_REQUEST_FILE, data->dialog);
	data->dialog = NULL;
}

static void
file_request_destroy(FinchFileRequest *data)
{
	g_free(data->cbs);
	g_free(data);
}

static FinchFileRequest *
finch_file_request_window(const char *title, const char *path,
				GCallback ok_cb, GCallback cancel_cb,
				void *user_data)
{
	GntWidget *window = gnt_file_sel_new();
	GntFileSel *sel = GNT_FILE_SEL(window);
	FinchFileRequest *data = g_new0(FinchFileRequest, 1);

	data->user_data = user_data;
	data->cbs = g_new0(GCallback, 2);
	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;
	data->dialog = window;
	gnt_box_set_title(GNT_BOX(window), title);

	gnt_file_sel_set_current_location(sel, (path && *path) ? path : purple_home_dir());

	g_signal_connect(G_OBJECT(sel), "destroy",
			G_CALLBACK(file_cancel_cb), data);
	g_signal_connect(G_OBJECT(sel), "cancelled",
			G_CALLBACK(file_cancel_cb), data);
	g_signal_connect(G_OBJECT(sel), "file_selected",
			G_CALLBACK(file_ok_cb), data);

	g_object_set_data_full(G_OBJECT(window), "filerequestdata", data,
			(GDestroyNotify)file_request_destroy);

	return data;
}

static void *
finch_request_file(const char *title, const char *filename,
				gboolean savedialog,
				GCallback ok_cb, GCallback cancel_cb,
				PurpleAccount *account, const char *who, PurpleConversation *conv,
				void *user_data)
{
	FinchFileRequest *data;
	const char *path;

	path = purple_prefs_get_path(savedialog ? "/finch/filelocations/last_save_folder" : "/finch/filelocations/last_open_folder");
	data = finch_file_request_window(title ? title : (savedialog ? _("Save File...") : _("Open File...")), path,
			ok_cb, cancel_cb, user_data);
	data->save = savedialog;
	if (savedialog)
		gnt_file_sel_set_suggested_filename(GNT_FILE_SEL(data->dialog), filename);

	gnt_widget_show(data->dialog);
	return data->dialog;
}

static void *
finch_request_folder(const char *title, const char *dirname, GCallback ok_cb,
		GCallback cancel_cb, PurpleAccount *account, const char *who, PurpleConversation *conv,
		void *user_data)
{
	FinchFileRequest *data;

	data = finch_file_request_window(title ? title : _("Choose Location..."), dirname,
			ok_cb, cancel_cb, user_data);
	data->save = TRUE;
	gnt_file_sel_set_dirs_only(GNT_FILE_SEL(data->dialog), TRUE);

	gnt_widget_show(data->dialog);
	return data->dialog;
}

static PurpleRequestUiOps uiops =
{
	finch_request_input,
	finch_request_choice,
	finch_request_action,
	finch_request_fields,
	finch_request_file,
	finch_close_request,
	finch_request_folder,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleRequestUiOps *finch_request_get_ui_ops()
{
	return &uiops;
}

void finch_request_init()
{
}

void finch_request_uninit()
{
}

void finch_request_save_in_prefs(gpointer null, PurpleRequestFields *allfields)
{
	GList *list;
	for (list = purple_request_fields_get_groups(allfields); list; list = list->next) {
		PurpleRequestFieldGroup *group = list->data;
		GList *fields = purple_request_field_group_get_fields(group);

		for (; fields ; fields = fields->next) {
			PurpleRequestField *field = fields->data;
			PurpleRequestFieldType type = purple_request_field_get_type(field);
			PurplePrefType pt;
			gpointer val = NULL;
			const char *id = purple_request_field_get_id(field);

			switch (type) {
				case PURPLE_REQUEST_FIELD_LIST:
					val = purple_request_field_list_get_selected(field)->data;
					val = purple_request_field_list_get_data(field, val);
					break;
				case PURPLE_REQUEST_FIELD_BOOLEAN:
					val = GINT_TO_POINTER(purple_request_field_bool_get_value(field));
					break;
				case PURPLE_REQUEST_FIELD_INTEGER:
					val = GINT_TO_POINTER(purple_request_field_int_get_value(field));
					break;
				case PURPLE_REQUEST_FIELD_STRING:
					val = (gpointer)purple_request_field_string_get_value(field);
					break;
				default:
					break;
			}

			pt = purple_prefs_get_type(id);
			switch (pt) {
				case PURPLE_PREF_INT:
				{
					long int tmp = GPOINTER_TO_INT(val);
					if (type == PURPLE_REQUEST_FIELD_LIST) {
						/* Lists always return string */
						if (sscanf(val, "%ld", &tmp) != 1)
							tmp = 0;
					}
					purple_prefs_set_int(id, (gint)tmp);
					break;
				}
				case PURPLE_PREF_BOOLEAN:
					purple_prefs_set_bool(id, GPOINTER_TO_INT(val));
					break;
				case PURPLE_PREF_STRING:
					purple_prefs_set_string(id, val);
					break;
				default:
					break;
			}
		}
	}
}

GntWidget *finch_request_field_get_widget(PurpleRequestField *field)
{
	GntWidget *ret = NULL;
	switch (purple_request_field_get_type(field)) {
		case PURPLE_REQUEST_FIELD_BOOLEAN:
			ret = create_boolean_field(field);
			break;
		case PURPLE_REQUEST_FIELD_STRING:
			ret = create_string_field(field, NULL);
			break;
		case PURPLE_REQUEST_FIELD_INTEGER:
			ret = create_integer_field(field);
			break;
		case PURPLE_REQUEST_FIELD_CHOICE:
			ret = create_choice_field(field);
			break;
		case PURPLE_REQUEST_FIELD_LIST:
			ret = create_list_field(field);
			break;
		case PURPLE_REQUEST_FIELD_ACCOUNT:
			ret = create_account_field(field);
			break;
		default:
			purple_debug_error("GntRequest", "Unimplemented request-field %d\n", purple_request_field_get_type(field));
			break;
	}
	return ret;
}

