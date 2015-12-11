/* purple
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
#define _PURPLE_NOTIFY_C_

#include "internal.h"
#include "dbus-maybe.h"
#include "notify.h"

static PurpleNotifyUiOps *notify_ui_ops = NULL;
static GList *handles = NULL;

typedef struct
{
	PurpleNotifyType type;
	void *handle;
	void *ui_handle;
	PurpleNotifyCloseCallback cb;
	gpointer cb_user_data;
} PurpleNotifyInfo;

/*
 * Definition of a user info entry
 */
struct _PurpleNotifyUserInfoEntry
{
	char *label;
	char *value;
	PurpleNotifyUserInfoEntryType type;
};

struct _PurpleNotifyUserInfo
{
	GQueue entries;
};

/*
 * Single column of a search result.
 */
struct _PurpleNotifySearchColumn
{
	char *title;           /* Title of the column. */
	gboolean visible;      /* Should the column be visible to the user. Defaults to TRUE. */

};

void *
purple_notify_message(void *handle, PurpleNotifyMessageType type, const char *title,
	const char *primary, const char *secondary,
	PurpleRequestCommonParameters *cpar, PurpleNotifyCloseCallback cb,
	gpointer user_data)
{
	PurpleNotifyUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_message != NULL) {
		void *ui_handle = ops->notify_message(type, title, primary,
			secondary, cpar);
		if (ui_handle != NULL) {

			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type = PURPLE_NOTIFY_MESSAGE;
			info->handle = handle;
			info->ui_handle = ui_handle;
			info->cb = cb;
			info->cb_user_data = user_data;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}

	}

	if (cb != NULL)
		cb(user_data);

	return NULL;
}

void *
purple_notify_email(void *handle, const char *subject, const char *from,
				  const char *to, const char *url, PurpleNotifyCloseCallback cb,
				  gpointer user_data)
{
	PurpleNotifyUiOps *ops;

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_email != NULL) {
		void *ui_handle;

		purple_signal_emit(purple_notify_get_handle(), "displaying-email-notification",
						   subject, from, to, url);

		ui_handle = ops->notify_email(handle, subject, from, to, url);

		if (ui_handle != NULL) {

			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type = PURPLE_NOTIFY_EMAIL;
			info->handle = handle;
			info->ui_handle = ui_handle;
			info->cb = cb;
			info->cb_user_data = user_data;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}
	}

	if (cb != NULL)
		cb(user_data);

	return NULL;
}

void *
purple_notify_emails(void *handle, size_t count, gboolean detailed,
				   const char **subjects, const char **froms,
				   const char **tos, const char **urls,
				   PurpleNotifyCloseCallback cb, gpointer user_data)
{
	PurpleNotifyUiOps *ops;

	if (count == 1) {
		return purple_notify_email(handle,
								 (subjects == NULL ? NULL : *subjects),
								 (froms    == NULL ? NULL : *froms),
								 (tos      == NULL ? NULL : *tos),
								 (urls     == NULL ? NULL : *urls),
								 cb, user_data);
	}

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_emails != NULL) {
		void *ui_handle;

		purple_signal_emit(purple_notify_get_handle(), "displaying-emails-notification",
							subjects, froms, tos, urls, count);

		ui_handle = ops->notify_emails(handle, count, detailed, subjects,
											 froms, tos, urls);

		if (ui_handle != NULL) {
			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type = PURPLE_NOTIFY_EMAILS;
			info->handle = handle;
			info->ui_handle = ui_handle;
			info->cb = cb;
			info->cb_user_data = user_data;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}

	}

	if (cb != NULL)
		cb(user_data);

	return NULL;
}

void *
purple_notify_formatted(void *handle, const char *title, const char *primary,
					  const char *secondary, const char *text,
					  PurpleNotifyCloseCallback cb, gpointer user_data)
{
	PurpleNotifyUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_formatted != NULL) {
		void *ui_handle = ops->notify_formatted(title, primary, secondary, text);

		if (ui_handle != NULL) {

			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type = PURPLE_NOTIFY_FORMATTED;
			info->handle = handle;
			info->ui_handle = ui_handle;
			info->cb = cb;
			info->cb_user_data = user_data;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}
	}

	if (cb != NULL)
		cb(user_data);
	return NULL;
}

void *
purple_notify_searchresults(PurpleConnection *gc, const char *title,
						  const char *primary, const char *secondary,
						  PurpleNotifySearchResults *results, PurpleNotifyCloseCallback cb,
						  gpointer user_data)
{
	PurpleNotifyUiOps *ops;

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_searchresults != NULL) {
		void *ui_handle = ops->notify_searchresults(gc, title, primary,
													secondary, results, user_data);
		if (ui_handle != NULL) {

			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type      = PURPLE_NOTIFY_SEARCHRESULTS;
			info->handle    = gc;
			info->ui_handle = ui_handle;
			info->cb = cb;
			info->cb_user_data = user_data;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}
	}

	if (cb != NULL)
		cb(user_data);

	return NULL;
}

void
purple_notify_searchresults_free(PurpleNotifySearchResults *results)
{
	GList *l;

	g_return_if_fail(results != NULL);

	for (l = results->buttons; l; l = g_list_delete_link(l, l)) {
		PurpleNotifySearchButton *button = l->data;
		g_free(button->label);
		g_free(button);
	}

	for (l = results->rows; l; l = g_list_delete_link(l, l)) {
		GList *row = l->data;
		g_list_foreach(row, (GFunc)g_free, NULL);
		g_list_free(row);
	}

	for (l = results->columns; l; l = g_list_delete_link(l, l)) {
		PurpleNotifySearchColumn *column = l->data;
		g_free(column->title);
		g_free(column);
	}

	g_free(results);
}

void
purple_notify_searchresults_new_rows(PurpleConnection *gc,
		PurpleNotifySearchResults *results,
		void *data)
{
	PurpleNotifyUiOps *ops;

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_searchresults != NULL) {
		ops->notify_searchresults_new_rows(gc, results, data);
	}
}

void
purple_notify_searchresults_button_add(PurpleNotifySearchResults *results,
									 PurpleNotifySearchButtonType type,
									 PurpleNotifySearchResultsCallback cb)
{
	PurpleNotifySearchButton *button;

	g_return_if_fail(results != NULL);
	g_return_if_fail(cb != NULL);

	button = g_new0(PurpleNotifySearchButton, 1);
	button->callback = cb;
	button->type = type;

	results->buttons = g_list_append(results->buttons, button);
}


void
purple_notify_searchresults_button_add_labeled(PurpleNotifySearchResults *results,
                                             const char *label,
                                             PurpleNotifySearchResultsCallback cb) {
	PurpleNotifySearchButton *button;

	g_return_if_fail(results != NULL);
	g_return_if_fail(cb != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(*label != '\0');

	button = g_new0(PurpleNotifySearchButton, 1);
	button->callback = cb;
	button->type = PURPLE_NOTIFY_BUTTON_LABELED;
	button->label = g_strdup(label);

	results->buttons = g_list_append(results->buttons, button);
}


PurpleNotifySearchResults *
purple_notify_searchresults_new()
{
	PurpleNotifySearchResults *rs = g_new0(PurpleNotifySearchResults, 1);

	return rs;
}

void
purple_notify_searchresults_column_add(PurpleNotifySearchResults *results,
									 PurpleNotifySearchColumn *column)
{
	g_return_if_fail(results != NULL);
	g_return_if_fail(column  != NULL);

	results->columns = g_list_append(results->columns, column);
}

void purple_notify_searchresults_row_add(PurpleNotifySearchResults *results,
									   GList *row)
{
	g_return_if_fail(results != NULL);
	g_return_if_fail(row     != NULL);

	results->rows = g_list_append(results->rows, row);
}

PurpleNotifySearchColumn *
purple_notify_searchresults_column_new(const char *title)
{
	PurpleNotifySearchColumn *sc;

	g_return_val_if_fail(title != NULL, NULL);

	sc = g_new0(PurpleNotifySearchColumn, 1);
	sc->title = g_strdup(title);
	sc->visible = TRUE;

	return sc;
}

const char *purple_notify_searchresult_column_get_title(const PurpleNotifySearchColumn *column)
{
	g_return_val_if_fail(column != NULL, NULL);
	
	return column->title;
}

void purple_notify_searchresult_column_set_visible(PurpleNotifySearchColumn *column, gboolean visible)
{
	g_return_if_fail(column != NULL);

	column->visible = visible;
}

gboolean
purple_notify_searchresult_column_is_visible(const PurpleNotifySearchColumn *column)
{
	g_return_val_if_fail(column != NULL, FALSE);

	return column->visible;
}

void *
purple_notify_userinfo(PurpleConnection *gc, const char *who,
						   PurpleNotifyUserInfo *user_info, PurpleNotifyCloseCallback cb, gpointer user_data)
{
	PurpleNotifyUiOps *ops;

	g_return_val_if_fail(who != NULL, NULL);

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_userinfo != NULL) {
		void *ui_handle;

		purple_signal_emit(purple_notify_get_handle(), "displaying-userinfo",
						 purple_connection_get_account(gc), who, user_info);

		ui_handle = ops->notify_userinfo(gc, who, user_info);

		if (ui_handle != NULL) {

			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type = PURPLE_NOTIFY_USERINFO;
			info->handle = gc;
			info->ui_handle = ui_handle;
			info->cb = cb;
			info->cb_user_data = user_data;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}
	}

	if (cb != NULL)
		cb(user_data);

	return NULL;
}

PurpleNotifyUserInfoEntry *
purple_notify_user_info_entry_new(const char *label, const char *value)
{
	PurpleNotifyUserInfoEntry *user_info_entry;

	user_info_entry = g_new0(PurpleNotifyUserInfoEntry, 1);
	PURPLE_DBUS_REGISTER_POINTER(user_info_entry, PurpleNotifyUserInfoEntry);
	user_info_entry->label = g_strdup(label);
	user_info_entry->value = g_strdup(value);
	user_info_entry->type = PURPLE_NOTIFY_USER_INFO_ENTRY_PAIR;

	return user_info_entry;
}

void
purple_notify_user_info_entry_destroy(PurpleNotifyUserInfoEntry *user_info_entry)
{
	g_return_if_fail(user_info_entry != NULL);

	g_free(user_info_entry->label);
	g_free(user_info_entry->value);
	PURPLE_DBUS_UNREGISTER_POINTER(user_info_entry);
	g_free(user_info_entry);
}

PurpleNotifyUserInfo *
purple_notify_user_info_new()
{
	PurpleNotifyUserInfo *user_info;

	user_info = g_new0(PurpleNotifyUserInfo, 1);
	PURPLE_DBUS_REGISTER_POINTER(user_info, PurpleNotifyUserInfo);
	g_queue_init(&user_info->entries);

	return user_info;
}

void
purple_notify_user_info_destroy(PurpleNotifyUserInfo *user_info)
{
	GList *l;

	for (l = user_info->entries.head; l != NULL; l = l->next) {
		PurpleNotifyUserInfoEntry *user_info_entry = l->data;

		purple_notify_user_info_entry_destroy(user_info_entry);
	}

	g_queue_clear(&user_info->entries);
	PURPLE_DBUS_UNREGISTER_POINTER(user_info);
	g_free(user_info);
}

GQueue *
purple_notify_user_info_get_entries(PurpleNotifyUserInfo *user_info)
{
	g_return_val_if_fail(user_info != NULL, NULL);

	return &user_info->entries;
}

char *
purple_notify_user_info_get_text_with_newline(PurpleNotifyUserInfo *user_info, const char *newline)
{
	GList *l;
	GString *text;

	text = g_string_new("");

	for (l = user_info->entries.head; l != NULL; l = l->next) {
		PurpleNotifyUserInfoEntry *user_info_entry = l->data;
		/* Add a newline before a section header */
		if (user_info_entry->type == PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER)
			g_string_append(text, newline);

		/* Handle the label/value pair itself */
		/* XXX Todo: Use a larger size for a section header? */
		if (user_info_entry->label)
			g_string_append_printf(text, "<b>%s</b>", user_info_entry->label);
		if (user_info_entry->label && user_info_entry->value)
			g_string_append(text, ": ");
		if (user_info_entry->value)
			g_string_append(text, user_info_entry->value);

		/* Display a section break as a horizontal line */
		if (user_info_entry->type == PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK)
			g_string_append(text, "<HR>");

		/* Don't insert a new line before or after a section break; <HR> does that for us */
		if ((user_info_entry->type != PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK) &&
			(l->next && ((((PurpleNotifyUserInfoEntry *)(l->next->data))->type != PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK))))
			g_string_append(text, newline);

		/* Add an extra newline after a section header */
		if (user_info_entry->type == PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER)
			g_string_append(text, newline);
	}

	return g_string_free(text, FALSE);
}


const gchar *
purple_notify_user_info_entry_get_label(PurpleNotifyUserInfoEntry *user_info_entry)
{
	g_return_val_if_fail(user_info_entry != NULL, NULL);

	return user_info_entry->label;
}

void
purple_notify_user_info_entry_set_label(PurpleNotifyUserInfoEntry *user_info_entry, const char *label)
{
	g_return_if_fail(user_info_entry != NULL);

	g_free(user_info_entry->label);
	user_info_entry->label = g_strdup(label);
}

const gchar *
purple_notify_user_info_entry_get_value(PurpleNotifyUserInfoEntry *user_info_entry)
{
	g_return_val_if_fail(user_info_entry != NULL, NULL);

	return user_info_entry->value;
}

void
purple_notify_user_info_entry_set_value(PurpleNotifyUserInfoEntry *user_info_entry, const char *value)
{
	g_return_if_fail(user_info_entry != NULL);

	g_free(user_info_entry->value);
	user_info_entry->value = g_strdup(value);
}

PurpleNotifyUserInfoEntryType
purple_notify_user_info_entry_get_entry_type(PurpleNotifyUserInfoEntry *user_info_entry)
{
	g_return_val_if_fail(user_info_entry != NULL, PURPLE_NOTIFY_USER_INFO_ENTRY_PAIR);

	return user_info_entry->type;
}

void
purple_notify_user_info_entry_set_entry_type(PurpleNotifyUserInfoEntry *user_info_entry,
		PurpleNotifyUserInfoEntryType type)
{
	g_return_if_fail(user_info_entry != NULL);

	user_info_entry->type = type;
}

void
purple_notify_user_info_add_pair_html(PurpleNotifyUserInfo *user_info, const char *label, const char *value)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = purple_notify_user_info_entry_new(label, value);
	g_queue_push_tail(&user_info->entries, entry);
}

void
purple_notify_user_info_add_pair_plaintext(PurpleNotifyUserInfo *user_info, const char *label, const char *value)
{
	gchar *escaped;

	escaped = g_markup_escape_text(value, -1);
	purple_notify_user_info_add_pair_html(user_info, label, escaped);
	g_free(escaped);
}

void
purple_notify_user_info_prepend_pair_html(PurpleNotifyUserInfo *user_info, const char *label, const char *value)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = purple_notify_user_info_entry_new(label, value);
	g_queue_push_head(&user_info->entries, entry);
}

void
purple_notify_user_info_prepend_pair_plaintext(PurpleNotifyUserInfo *user_info, const char *label, const char *value)
{
	gchar *escaped;

	escaped = g_markup_escape_text(value, -1);
	purple_notify_user_info_prepend_pair_html(user_info, label, escaped);
	g_free(escaped);
}

void
purple_notify_user_info_remove_entry(PurpleNotifyUserInfo *user_info, PurpleNotifyUserInfoEntry *entry)
{
	g_return_if_fail(user_info != NULL);
	g_return_if_fail(entry != NULL);

	g_queue_remove(&user_info->entries, entry);
}

void
purple_notify_user_info_add_section_header(PurpleNotifyUserInfo *user_info, const char *label)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = purple_notify_user_info_entry_new(label, NULL);
	entry->type = PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER;

	g_queue_push_tail(&user_info->entries, entry);
}

void
purple_notify_user_info_prepend_section_header(PurpleNotifyUserInfo *user_info, const char *label)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = purple_notify_user_info_entry_new(label, NULL);
	entry->type = PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER;

	g_queue_push_head(&user_info->entries, entry);
}

void
purple_notify_user_info_add_section_break(PurpleNotifyUserInfo *user_info)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = purple_notify_user_info_entry_new(NULL, NULL);
	entry->type = PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK;

	g_queue_push_tail(&user_info->entries, entry);
}

void
purple_notify_user_info_prepend_section_break(PurpleNotifyUserInfo *user_info)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = purple_notify_user_info_entry_new(NULL, NULL);
	entry->type = PURPLE_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK;

	g_queue_push_head(&user_info->entries, entry);
}

void
purple_notify_user_info_remove_last_item(PurpleNotifyUserInfo *user_info)
{
	PurpleNotifyUserInfoEntry *entry;

	entry = g_queue_pop_tail(&user_info->entries);
	if (entry)
		purple_notify_user_info_entry_destroy(entry);
}

static PurpleNotifyUserInfo *
purple_notify_user_info_copy(PurpleNotifyUserInfo *user_info)
{
	PurpleNotifyUserInfo *user_info_copy;
	GList *l;

	g_return_val_if_fail(user_info != NULL, NULL);

	user_info_copy = purple_notify_user_info_new();

	for (l = user_info->entries.head; l != NULL; l = l->next) {
		PurpleNotifyUserInfoEntry *new_entry, *user_info_entry = l->data;

		new_entry = purple_notify_user_info_entry_new(user_info_entry->label,
				user_info_entry->value);
		new_entry->type = user_info_entry->type;
		g_queue_push_tail(&user_info_copy->entries, new_entry);
	}

	return user_info_copy;
}

GType
purple_notify_user_info_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleNotifyUserInfo",
				(GBoxedCopyFunc)purple_notify_user_info_copy,
				(GBoxedFreeFunc)purple_notify_user_info_destroy);
	}

	return type;
}

void *
purple_notify_uri(void *handle, const char *uri)
{
	PurpleNotifyUiOps *ops;

	g_return_val_if_fail(uri != NULL, NULL);

	ops = purple_notify_get_ui_ops();

	if (ops != NULL && ops->notify_uri != NULL) {

		void *ui_handle = ops->notify_uri(uri);

		if (ui_handle != NULL) {

			PurpleNotifyInfo *info = g_new0(PurpleNotifyInfo, 1);
			info->type = PURPLE_NOTIFY_URI;
			info->handle = handle;
			info->ui_handle = ui_handle;

			handles = g_list_append(handles, info);

			return info->ui_handle;
		}
	}

	return NULL;
}

gboolean
purple_notify_is_valid_ui_handle(void *ui_handle, PurpleNotifyType *type)
{
	GList *it;

	if (ui_handle == NULL)
		return FALSE;

	for (it = handles; it != NULL; it = g_list_next(it)) {
		PurpleNotifyInfo *info = it->data;

		if (info->ui_handle != ui_handle)
			continue;

		if (type != NULL)
			*type = info->type;
		return TRUE;
	}

	return FALSE;
}

void
purple_notify_close(PurpleNotifyType type, void *ui_handle)
{
	GList *l;
	PurpleNotifyUiOps *ops;

	g_return_if_fail(ui_handle != NULL);

	ops = purple_notify_get_ui_ops();

	for (l = handles; l != NULL; l = l->next) {
		PurpleNotifyInfo *info = l->data;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_notify != NULL)
				ops->close_notify(info->type, ui_handle);

			if (info->cb != NULL)
				info->cb(info->cb_user_data);

			g_free(info);

			break;
		}
	}
}

void
purple_notify_close_with_handle(void *handle)
{
	GList *l, *prev = NULL;
	PurpleNotifyUiOps *ops;

	g_return_if_fail(handle != NULL);

	ops = purple_notify_get_ui_ops();

	for (l = handles; l != NULL; l = prev ? prev->next : handles) {
		PurpleNotifyInfo *info = l->data;

		if (info->handle == handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_notify != NULL)
				ops->close_notify(info->type, info->ui_handle);

			if (info->cb != NULL)
				info->cb(info->cb_user_data);

			g_free(info);
		} else
			prev = l;
	}
}

static PurpleNotifyUiOps *
purple_notify_ui_ops_copy(PurpleNotifyUiOps *ops)
{
	PurpleNotifyUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleNotifyUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_notify_ui_ops_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleNotifyUiOps",
				(GBoxedCopyFunc)purple_notify_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

static PurpleNotifySearchButton *
purple_notify_search_button_copy(PurpleNotifySearchButton *button)
{
	PurpleNotifySearchButton *button_new;

	g_return_val_if_fail(button != NULL, NULL);

	button_new = g_new(PurpleNotifySearchButton, 1);
	*button_new = *button;

	return button_new;
}

GType
purple_notify_search_button_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleNotifySearchButton",
				(GBoxedCopyFunc)purple_notify_search_button_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

void
purple_notify_set_ui_ops(PurpleNotifyUiOps *ops)
{
	notify_ui_ops = ops;
}

PurpleNotifyUiOps *
purple_notify_get_ui_ops(void)
{
	return notify_ui_ops;
}

void *
purple_notify_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_notify_init(void)
{
	gpointer handle = purple_notify_get_handle();

	purple_signal_register(handle, "displaying-email-notification",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING);

	purple_signal_register(handle, "displaying-emails-notification",
						 purple_marshal_VOID__POINTER_POINTER_POINTER_POINTER_UINT,
						 G_TYPE_NONE, 5, G_TYPE_POINTER, G_TYPE_POINTER,
						 G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_UINT);

	purple_signal_register(handle, "displaying-emails-clear",
						 purple_marshal_VOID, G_TYPE_NONE, 0);

	purple_signal_register(handle, "displaying-userinfo",
						 purple_marshal_VOID__POINTER_POINTER_POINTER,
						 G_TYPE_NONE, 3, PURPLE_TYPE_ACCOUNT, G_TYPE_STRING,
						 PURPLE_TYPE_NOTIFY_USER_INFO);
}

void
purple_notify_uninit(void)
{
	purple_signals_unregister_by_instance(purple_notify_get_handle());
}
