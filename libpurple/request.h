/**
 * @file request.h Request API
 * @ingroup core
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
#ifndef _GAIM_REQUEST_H_
#define _GAIM_REQUEST_H_

#include <stdlib.h>
#include <glib-object.h>
#include <glib.h>

#include "account.h"

#define GAIM_DEFAULT_ACTION_NONE	-1

/**
 * Request types.
 */
typedef enum
{
	GAIM_REQUEST_INPUT = 0,  /**< Text input request.        */
	GAIM_REQUEST_CHOICE,     /**< Multiple-choice request.   */
	GAIM_REQUEST_ACTION,     /**< Action request.            */
	GAIM_REQUEST_FIELDS,     /**< Multiple fields request.   */
	GAIM_REQUEST_FILE,       /**< File open or save request. */
	GAIM_REQUEST_FOLDER      /**< Folder selection request.  */

} GaimRequestType;

/**
 * A type of field.
 */
typedef enum
{
	GAIM_REQUEST_FIELD_NONE,
	GAIM_REQUEST_FIELD_STRING,
	GAIM_REQUEST_FIELD_INTEGER,
	GAIM_REQUEST_FIELD_BOOLEAN,
	GAIM_REQUEST_FIELD_CHOICE,
	GAIM_REQUEST_FIELD_LIST,
	GAIM_REQUEST_FIELD_LABEL,
	GAIM_REQUEST_FIELD_IMAGE,
	GAIM_REQUEST_FIELD_ACCOUNT

} GaimRequestFieldType;

/**
 * Multiple fields request data.
 */
typedef struct
{
	GList *groups;

	GHashTable *fields;

	GList *required_fields;

	void *ui_data;

} GaimRequestFields;

/**
 * A group of fields with a title.
 */
typedef struct
{
	GaimRequestFields *fields_list;

	char *title;

	GList *fields;

} GaimRequestFieldGroup;

/**
 * A request field.
 */
typedef struct
{
	GaimRequestFieldType type;
	GaimRequestFieldGroup *group;

	char *id;
	char *label;
	char *type_hint;

	gboolean visible;
	gboolean required;

	union
	{
		struct
		{
			gboolean multiline;
			gboolean masked;
			gboolean editable;
			char *default_value;
			char *value;

		} string;

		struct
		{
			int default_value;
			int value;

		} integer;

		struct
		{
			gboolean default_value;
			gboolean value;

		} boolean;

		struct
		{
			int default_value;
			int value;

			GList *labels;

		} choice;

		struct
		{
			GList *items;
			GHashTable *item_data;
			GList *selected;
			GHashTable *selected_table;

			gboolean multiple_selection;

		} list;

		struct
		{
			GaimAccount *default_account;
			GaimAccount *account;
			gboolean show_all;

			GaimFilterAccountFunc filter_func;

		} account;

		struct
		{
			unsigned int scale_x;
			unsigned int scale_y;
			const char *buffer;
			gsize size;
		} image;

	} u;

	void *ui_data;

} GaimRequestField;

/**
 * Request UI operations.
 */
typedef struct
{
	void *(*request_input)(const char *title, const char *primary,
						   const char *secondary, const char *default_value,
						   gboolean multiline, gboolean masked, gchar *hint,
						   const char *ok_text, GCallback ok_cb,
						   const char *cancel_text, GCallback cancel_cb,
						   void *user_data);
	void *(*request_choice)(const char *title, const char *primary,
							const char *secondary, unsigned int default_value,
							const char *ok_text, GCallback ok_cb,
							const char *cancel_text, GCallback cancel_cb,
							void *user_data, va_list choices);
	void *(*request_action)(const char *title, const char *primary,
							const char *secondary, unsigned int default_action,
							void *user_data, size_t action_count,
							va_list actions);
	void *(*request_fields)(const char *title, const char *primary,
							const char *secondary, GaimRequestFields *fields,
							const char *ok_text, GCallback ok_cb,
							const char *cancel_text, GCallback cancel_cb,
							void *user_data);
	void *(*request_file)(const char *title, const char *filename,
						  gboolean savedialog, GCallback ok_cb,
						  GCallback cancel_cb, void *user_data);
	void (*close_request)(GaimRequestType type, void *ui_handle);
	void *(*request_folder)(const char *title, const char *dirname,
						  GCallback ok_cb, GCallback cancel_cb,
						  void *user_data);
} GaimRequestUiOps;

typedef void (*GaimRequestInputCb)(void *, const char *);
typedef void (*GaimRequestActionCb)(void *, int);
typedef void (*GaimRequestChoiceCb)(void *, int);
typedef void (*GaimRequestFieldsCb)(void *, GaimRequestFields *fields);
typedef void (*GaimRequestFileCb)(void *, const char *filename);

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Field List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a list of fields to pass to gaim_request_fields().
 *
 * @return A GaimRequestFields structure.
 */
GaimRequestFields *gaim_request_fields_new(void);

/**
 * Destroys a list of fields.
 *
 * @param fields The list of fields to destroy.
 */
void gaim_request_fields_destroy(GaimRequestFields *fields);

/**
 * Adds a group of fields to the list.
 *
 * @param fields The fields list.
 * @param group  The group to add.
 */
void gaim_request_fields_add_group(GaimRequestFields *fields,
								   GaimRequestFieldGroup *group);

/**
 * Returns a list of all groups in a field list.
 *
 * @param fields The fields list.
 *
 * @return A list of groups.
 */
GList *gaim_request_fields_get_groups(const GaimRequestFields *fields);

/**
 * Returns whether or not the field with the specified ID exists.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return TRUE if the field exists, or FALSE.
 */
gboolean gaim_request_fields_exists(const GaimRequestFields *fields,
									const char *id);

/**
 * Returns a list of all required fields.
 *
 * @param fields The fields list.
 *
 * @return The list of required fields.
 */
const GList *gaim_request_fields_get_required(const GaimRequestFields *fields);

/**
 * Returns whether or not a field with the specified ID is required.
 *
 * @param fields The fields list.
 * @param id     The field ID.
 *
 * @return TRUE if the specified field is required, or FALSE.
 */
gboolean gaim_request_fields_is_field_required(const GaimRequestFields *fields,
											   const char *id);

/**
 * Returns whether or not all required fields have values.
 *
 * @param fields The fields list.
 *
 * @return TRUE if all required fields have values, or FALSE.
 */
gboolean gaim_request_fields_all_required_filled(
	const GaimRequestFields *fields);

/**
 * Return the field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The field, if found.
 */
GaimRequestField *gaim_request_fields_get_field(
		const GaimRequestFields *fields, const char *id);

/**
 * Returns the string value of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The string value, if found, or @c NULL otherwise.
 */
const char *gaim_request_fields_get_string(const GaimRequestFields *fields,
										   const char *id);

/**
 * Returns the integer value of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The integer value, if found, or 0 otherwise.
 */
int gaim_request_fields_get_integer(const GaimRequestFields *fields,
									const char *id);

/**
 * Returns the boolean value of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The boolean value, if found, or @c FALSE otherwise.
 */
gboolean gaim_request_fields_get_bool(const GaimRequestFields *fields,
									  const char *id);

/**
 * Returns the choice index of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The choice index, if found, or -1 otherwise.
 */
int gaim_request_fields_get_choice(const GaimRequestFields *fields,
								   const char *id);

/**
 * Returns the account of a field with the specified ID.
 *
 * @param fields The fields list.
 * @param id     The ID of the field.
 *
 * @return The account value, if found, or NULL otherwise.
 */
GaimAccount *gaim_request_fields_get_account(const GaimRequestFields *fields,
											 const char *id);

/*@}*/

/**************************************************************************/
/** @name Fields Group API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a fields group with an optional title.
 *
 * @param title The optional title to give the group.
 *
 * @return A new fields group
 */
GaimRequestFieldGroup *gaim_request_field_group_new(const char *title);

/**
 * Destroys a fields group.
 *
 * @param group The group to destroy.
 */
void gaim_request_field_group_destroy(GaimRequestFieldGroup *group);

/**
 * Adds a field to the group.
 *
 * @param group The group to add the field to.
 * @param field The field to add to the group.
 */
void gaim_request_field_group_add_field(GaimRequestFieldGroup *group,
										GaimRequestField *field);

/**
 * Returns the title of a fields group.
 *
 * @param group The group.
 *
 * @return The title, if set.
 */
const char *gaim_request_field_group_get_title(
		const GaimRequestFieldGroup *group);

/**
 * Returns a list of all fields in a group.
 *
 * @param group The group.
 *
 * @return The list of fields in the group.
 */
GList *gaim_request_field_group_get_fields(
		const GaimRequestFieldGroup *group);

/*@}*/

/**************************************************************************/
/** @name Field API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Creates a field of the specified type.
 *
 * @param id   The field ID.
 * @param text The text label of the field.
 * @param type The type of field.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_new(const char *id, const char *text,
										 GaimRequestFieldType type);

/**
 * Destroys a field.
 *
 * @param field The field to destroy.
 */
void gaim_request_field_destroy(GaimRequestField *field);

/**
 * Sets the label text of a field.
 *
 * @param field The field.
 * @param label The text label.
 */
void gaim_request_field_set_label(GaimRequestField *field, const char *label);

/**
 * Sets whether or not a field is visible.
 *
 * @param field   The field.
 * @param visible TRUE if visible, or FALSE if not.
 */
void gaim_request_field_set_visible(GaimRequestField *field, gboolean visible);

/**
 * Sets the type hint for the field.
 *
 * This is optionally used by the UIs to provide such features as
 * auto-completion for type hints like "account" and "screenname".
 *
 * @param field     The field.
 * @param type_hint The type hint.
 */
void gaim_request_field_set_type_hint(GaimRequestField *field,
									  const char *type_hint);

/**
 * Sets whether or not a field is required.
 *
 * @param field    The field.
 * @param required TRUE if required, or FALSE.
 */
void gaim_request_field_set_required(GaimRequestField *field,
									 gboolean required);

/**
 * Returns the type of a field.
 *
 * @param field The field.
 *
 * @return The field's type.
 */
GaimRequestFieldType gaim_request_field_get_type(const GaimRequestField *field);

/**
 * Returns the ID of a field.
 *
 * @param field The field.
 *
 * @return The ID
 */
const char *gaim_request_field_get_id(const GaimRequestField *field);

/**
 * Returns the label text of a field.
 *
 * @param field The field.
 *
 * @return The label text.
 */
const char *gaim_request_field_get_label(const GaimRequestField *field);

/**
 * Returns whether or not a field is visible.
 *
 * @param field The field.
 *
 * @return TRUE if the field is visible. FALSE otherwise.
 */
gboolean gaim_request_field_is_visible(const GaimRequestField *field);

/**
 * Returns the field's type hint.
 *
 * @param field The field.
 *
 * @return The field's type hint.
 */
const char *gaim_request_field_get_type_hint(const GaimRequestField *field);

/**
 * Returns whether or not a field is required.
 *
 * @param field The field.
 *
 * @return TRUE if the field is required, or FALSE.
 */
gboolean gaim_request_field_is_required(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name String Field API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a string request field.
 *
 * @param id            The field ID.
 * @param text          The text label of the field.
 * @param default_value The optional default value.
 * @param multiline     Whether or not this should be a multiline string.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_string_new(const char *id,
												const char *text,
												const char *default_value,
												gboolean multiline);

/**
 * Sets the default value in a string field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_string_set_default_value(GaimRequestField *field,
												 const char *default_value);

/**
 * Sets the value in a string field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_string_set_value(GaimRequestField *field,
										 const char *value);

/**
 * Sets whether or not a string field is masked
 * (commonly used for password fields).
 *
 * @param field  The field.
 * @param masked The masked value.
 */
void gaim_request_field_string_set_masked(GaimRequestField *field,
										  gboolean masked);

/**
 * Sets whether or not a string field is editable.
 *
 * @param field    The field.
 * @param editable The editable value.
 */
void gaim_request_field_string_set_editable(GaimRequestField *field,
											gboolean editable);

/**
 * Returns the default value in a string field.
 *
 * @param field The field.
 *
 * @return The default value.
 */
const char *gaim_request_field_string_get_default_value(
		const GaimRequestField *field);

/**
 * Returns the user-entered value in a string field.
 *
 * @param field The field.
 *
 * @return The value.
 */
const char *gaim_request_field_string_get_value(const GaimRequestField *field);

/**
 * Returns whether or not a string field is multi-line.
 *
 * @param field The field.
 *
 * @return @c TRUE if the field is mulit-line, or @c FALSE otherwise.
 */
gboolean gaim_request_field_string_is_multiline(const GaimRequestField *field);

/**
 * Returns whether or not a string field is masked.
 *
 * @param field The field.
 *
 * @return @c TRUE if the field is masked, or @c FALSE otherwise.
 */
gboolean gaim_request_field_string_is_masked(const GaimRequestField *field);

/**
 * Returns whether or not a string field is editable.
 *
 * @param field The field.
 *
 * @return @c TRUE if the field is editable, or @c FALSE otherwise.
 */
gboolean gaim_request_field_string_is_editable(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Integer Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates an integer field.
 *
 * @param id            The field ID.
 * @param text          The text label of the field.
 * @param default_value The default value.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_int_new(const char *id,
											 const char *text,
											 int default_value);

/**
 * Sets the default value in an integer field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_int_set_default_value(GaimRequestField *field,
											  int default_value);

/**
 * Sets the value in an integer field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_int_set_value(GaimRequestField *field, int value);

/**
 * Returns the default value in an integer field.
 *
 * @param field The field.
 *
 * @return The default value.
 */
int gaim_request_field_int_get_default_value(const GaimRequestField *field);

/**
 * Returns the user-entered value in an integer field.
 *
 * @param field The field.
 *
 * @return The value.
 */
int gaim_request_field_int_get_value(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Boolean Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates a boolean field.
 *
 * This is often represented as a checkbox.
 *
 * @param id            The field ID.
 * @param text          The text label of the field.
 * @param default_value The default value.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_bool_new(const char *id,
											  const char *text,
											  gboolean default_value);

/**
 * Sets the default value in an boolean field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_bool_set_default_value(GaimRequestField *field,
											   gboolean default_value);

/**
 * Sets the value in an boolean field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_bool_set_value(GaimRequestField *field,
									   gboolean value);

/**
 * Returns the default value in an boolean field.
 *
 * @param field The field.
 *
 * @return The default value.
 */
gboolean gaim_request_field_bool_get_default_value(
		const GaimRequestField *field);

/**
 * Returns the user-entered value in an boolean field.
 *
 * @param field The field.
 *
 * @return The value.
 */
gboolean gaim_request_field_bool_get_value(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Choice Field API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a multiple choice field.
 *
 * This is often represented as a group of radio buttons.
 *
 * @param id            The field ID.
 * @param text          The optional label of the field.
 * @param default_value The default choice.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_choice_new(const char *id,
												const char *text,
												int default_value);

/**
 * Adds a choice to a multiple choice field.
 *
 * @param field The choice field.
 * @param label The choice label.
 */
void gaim_request_field_choice_add(GaimRequestField *field,
								   const char *label);

/**
 * Sets the default value in an choice field.
 *
 * @param field         The field.
 * @param default_value The default value.
 */
void gaim_request_field_choice_set_default_value(GaimRequestField *field,
												 int default_value);

/**
 * Sets the value in an choice field.
 *
 * @param field The field.
 * @param value The value.
 */
void gaim_request_field_choice_set_value(GaimRequestField *field, int value);

/**
 * Returns the default value in an choice field.
 *
 * @param field The field.
 *
 * @return The default value.
 */
int gaim_request_field_choice_get_default_value(const GaimRequestField *field);

/**
 * Returns the user-entered value in an choice field.
 *
 * @param field The field.
 *
 * @return The value.
 */
int gaim_request_field_choice_get_value(const GaimRequestField *field);

/**
 * Returns a list of labels in a choice field.
 *
 * @param field The field.
 *
 * @return The list of labels.
 */
GList *gaim_request_field_choice_get_labels(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name List Field API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a multiple list item field.
 *
 * @param id   The field ID.
 * @param text The optional label of the field.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_list_new(const char *id, const char *text);

/**
 * Sets whether or not a list field allows multiple selection.
 *
 * @param field        The list field.
 * @param multi_select TRUE if multiple selection is enabled,
 *                     or FALSE otherwise.
 */
void gaim_request_field_list_set_multi_select(GaimRequestField *field,
											  gboolean multi_select);

/**
 * Returns whether or not a list field allows multiple selection.
 *
 * @param field The list field.
 *
 * @return TRUE if multiple selection is enabled, or FALSE otherwise.
 */
gboolean gaim_request_field_list_get_multi_select(
	const GaimRequestField *field);

/**
 * Returns the data for a particular item.
 *
 * @param field The list field.
 * @param text  The item text.
 *
 * @return The data associated with the item.
 */
void *gaim_request_field_list_get_data(const GaimRequestField *field,
									   const char *text);

/**
 * Adds an item to a list field.
 *
 * @param field The list field.
 * @param item  The list item.
 * @param data  The associated data.
 */
void gaim_request_field_list_add(GaimRequestField *field,
								 const char *item, void *data);

/**
 * Adds a selected item to the list field.
 *
 * @param field The field.
 * @param item  The item to add.
 */
void gaim_request_field_list_add_selected(GaimRequestField *field,
										  const char *item);

/**
 * Clears the list of selected items in a list field.
 *
 * @param field The field.
 */
void gaim_request_field_list_clear_selected(GaimRequestField *field);

/**
 * Sets a list of selected items in a list field.
 *
 * @param field The field.
 * @param items The list of selected items.
 */
void gaim_request_field_list_set_selected(GaimRequestField *field,
										  const GList *items);

/**
 * Returns whether or not a particular item is selected in a list field.
 *
 * @param field The field.
 * @param item  The item.
 *
 * @return TRUE if the item is selected. FALSE otherwise.
 */
gboolean gaim_request_field_list_is_selected(const GaimRequestField *field,
											 const char *item);

/**
 * Returns a list of selected items in a list field.
 *
 * To retrieve the data for each item, use
 * gaim_request_field_list_get_data().
 *
 * @param field The field.
 *
 * @return The list of selected items.
 */
const GList *gaim_request_field_list_get_selected(
	const GaimRequestField *field);

/**
 * Returns a list of items in a list field.
 *
 * @param field The field.
 *
 * @return The list of items.
 */
const GList *gaim_request_field_list_get_items(const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Label Field API                                                 */
/**************************************************************************/
/*@{*/

/**
 * Creates a label field.
 *
 * @param id   The field ID.
 * @param text The label of the field.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_label_new(const char *id,
											   const char *text);

/*@}*/

/**************************************************************************/
/** @name Image Field API                                                 */
/**************************************************************************/
/*@{*/

/**
 * Creates an image field.
 *
 * @param id   The field ID.
 * @param text The label of the field.
 * @param buf  The image data.
 * @param size The size of the data in @a buffer.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_image_new(const char *id, const char *text,
											   const char *buf, gsize size);

/**
 * Sets the scale factors of an image field.
 *
 * @param field The image field.
 * @param x     The x scale factor.
 * @param y     The y scale factor.
 */
void gaim_request_field_image_set_scale(GaimRequestField *field, unsigned int x, unsigned int y);

/**
 * Returns pointer to the image.
 *
 * @param field The image field.
 *
 * @return Pointer to the image.
 */
const char *gaim_request_field_image_get_buffer(GaimRequestField *field);

/**
 * Returns size (in bytes) of the image.
 *
 * @param field The image field.
 *
 * @return Size of the image.
 */
gsize gaim_request_field_image_get_size(GaimRequestField *field);

/**
 * Returns X scale coefficient of the image.
 *
 * @param field The image field.
 *
 * @return X scale coefficient of the image.
 */
unsigned int gaim_request_field_image_get_scale_x(GaimRequestField *field);

/**
 * Returns Y scale coefficient of the image.
 *
 * @param field The image field.
 *
 * @return Y scale coefficient of the image.
 */
unsigned int gaim_request_field_image_get_scale_y(GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Account Field API                                               */
/**************************************************************************/
/*@{*/

/**
 * Creates an account field.
 *
 * By default, this field will not show offline accounts.
 *
 * @param id      The field ID.
 * @param text    The text label of the field.
 * @param account The optional default account.
 *
 * @return The new field.
 */
GaimRequestField *gaim_request_field_account_new(const char *id,
												 const char *text,
												 GaimAccount *account);

/**
 * Sets the default account on an account field.
 *
 * @param field         The account field.
 * @param default_value The default account.
 */
void gaim_request_field_account_set_default_value(GaimRequestField *field,
												  GaimAccount *default_value);

/**
 * Sets the account in an account field.
 *
 * @param field The account field.
 * @param value The account.
 */
void gaim_request_field_account_set_value(GaimRequestField *field,
										  GaimAccount *value);

/**
 * Sets whether or not to show all accounts in an account field.
 *
 * If TRUE, all accounts, online or offline, will be shown. If FALSE,
 * only online accounts will be shown.
 *
 * @param field    The account field.
 * @param show_all Whether or not to show all accounts.
 */
void gaim_request_field_account_set_show_all(GaimRequestField *field,
											 gboolean show_all);

/**
 * Sets the account filter function in an account field.
 *
 * This function will determine which accounts get displayed and which
 * don't.
 *
 * @param field       The account field.
 * @param filter_func The account filter function.
 */
void gaim_request_field_account_set_filter(GaimRequestField *field,
										   GaimFilterAccountFunc filter_func);

/**
 * Returns the default account in an account field.
 *
 * @param field The field.
 *
 * @return The default account.
 */
GaimAccount *gaim_request_field_account_get_default_value(
		const GaimRequestField *field);

/**
 * Returns the user-entered account in an account field.
 *
 * @param field The field.
 *
 * @return The user-entered account.
 */
GaimAccount *gaim_request_field_account_get_value(
		const GaimRequestField *field);

/**
 * Returns whether or not to show all accounts in an account field.
 *
 * If TRUE, all accounts, online or offline, will be shown. If FALSE,
 * only online accounts will be shown.
 *
 * @param field    The account field.
 * @return Whether or not to show all accounts.
 */
gboolean gaim_request_field_account_get_show_all(
		const GaimRequestField *field);

/**
 * Returns the account filter function in an account field.
 *
 * This function will determine which accounts get displayed and which
 * don't.
 *
 * @param field       The account field.
 *
 * @return The account filter function.
 */
GaimFilterAccountFunc gaim_request_field_account_get_filter(
		const GaimRequestField *field);

/*@}*/

/**************************************************************************/
/** @name Request API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Prompts the user for text input.
 *
 * @param handle        The plugin or connection handle.  For some
 *                      things this is EXTREMELY important.  The
 *                      handle is used to programmatically close
 *                      the request dialog when it is no longer
 *                      needed.  For PRPLs this is often a pointer
 *                      to the GaimConnection instance.  For plugins
 *                      this should be a similar, unique memory
 *                      location.  This value is important because
 *                      it allows a request to be closed, say, when
 *                      you sign offline.  If the request is NOT
 *                      closed it is VERY likely to cause a crash
 *                      whenever the callback handler functions are
 *                      triggered.
 * @param title         The title of the message.
 * @param primary       The main point of the message.
 * @param secondary     The secondary information.
 * @param default_value The default value.
 * @param multiline     TRUE if the inputted text can span multiple lines.
 * @param masked        TRUE if the inputted text should be masked in some way.
 * @param hint          Optionally suggest how the input box should appear.
 *                      Use "html," for example, to allow the user to enter
 *                      HTML.
 * @param ok_text       The text for the @c OK button.
 * @param ok_cb         The callback for the @c OK button.
 * @param cancel_text   The text for the @c Cancel button.
 * @param cancel_cb     The callback for the @c Cancel button.
 * @param user_data     The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_input(void *handle, const char *title,
						 const char *primary, const char *secondary,
						 const char *default_value,
						 gboolean multiline, gboolean masked, gchar *hint,
						 const char *ok_text, GCallback ok_cb,
						 const char *cancel_text, GCallback cancel_cb,
						 void *user_data);

/**
 * Prompts the user for multiple-choice input.
 *
 * @param handle        The plugin or connection handle.  For some
 *                      things this is EXTREMELY important.  See
 *                      the comments on gaim_request_input.
 * @param title         The title of the message.
 * @param primary       The main point of the message.
 * @param secondary     The secondary information.
 * @param default_value The default value.
 * @param ok_text       The text for the @c OK button.
 * @param ok_cb         The callback for the @c OK button.
 * @param cancel_text   The text for the @c Cancel button.
 * @param cancel_cb     The callback for the @c Cancel button.
 * @param user_data     The data to pass to the callback.
 * @param ...           The choices.  This argument list should be
 *                      terminated with a NULL parameter.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_choice(void *handle, const char *title,
						  const char *primary, const char *secondary,
						  unsigned int default_value,
						  const char *ok_text, GCallback ok_cb,
						  const char *cancel_text, GCallback cancel_cb,
						  void *user_data, ...);

/**
 * Prompts the user for multiple-choice input.
 *
 * @param handle        The plugin or connection handle.  For some
 *                      things this is EXTREMELY important.  See
 *                      the comments on gaim_request_input.
 * @param title         The title of the message.
 * @param primary       The main point of the message.
 * @param secondary     The secondary information.
 * @param default_value The default value.
 * @param ok_text       The text for the @c OK button.
 * @param ok_cb         The callback for the @c OK button.
 * @param cancel_text   The text for the @c Cancel button.
 * @param cancel_cb     The callback for the @c Cancel button.
 * @param user_data     The data to pass to the callback.
 * @param choices       The choices.  This argument list should be
 *                      terminated with a @c NULL parameter.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_choice_varg(void *handle, const char *title,
							   const char *primary, const char *secondary,
							   unsigned int default_value,
							   const char *ok_text, GCallback ok_cb,
							   const char *cancel_text, GCallback cancel_cb,
							   void *user_data, va_list choices);

/**
 * Prompts the user for an action.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * @param handle         The plugin or connection handle.  For some
 *                       things this is EXTREMELY important.  See
 *                       the comments on gaim_request_input.
 * @param title          The title of the message.
 * @param primary        The main point of the message.
 * @param secondary      The secondary information.
 * @param default_action The default value.
 * @param user_data      The data to pass to the callback.
 * @param action_count   The number of actions.
 * @param ...            A list of actions.  These are pairs of
 *                       arguments.  The first of each pair is the
 *                       string that appears on the button.  It should
 *                       have an underscore before the letter you want
 *                       to use as the accelerator key for the button.
 *                       The second of each pair is the callback
 *                       function to use when the button is clicked.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_action(void *handle, const char *title,
						  const char *primary, const char *secondary,
						  unsigned int default_action,
						  void *user_data, size_t action_count, ...);

/**
 * Prompts the user for an action.
 *
 * This is often represented as a dialog with a button for each action.
 *
 * @param handle         The plugin or connection handle.  For some
 *                       things this is EXTREMELY important.  See
 *                       the comments on gaim_request_input.
 * @param title          The title of the message.
 * @param primary        The main point of the message.
 * @param secondary      The secondary information.
 * @param default_action The default value.
 * @param user_data      The data to pass to the callback.
 * @param action_count   The number of actions.
 * @param actions        A list of actions and callbacks.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_action_varg(void *handle, const char *title,
							   const char *primary, const char *secondary,
							   unsigned int default_action,
							   void *user_data, size_t action_count,
							   va_list actions);

/**
 * Displays groups of fields for the user to fill in.
 *
 * @param handle      The plugin or connection handle.  For some
 *                    things this is EXTREMELY important.  See
 *                    the comments on gaim_request_input.
 * @param title       The title of the message.
 * @param primary     The main point of the message.
 * @param secondary   The secondary information.
 * @param fields      The list of fields.
 * @param ok_text     The text for the @c OK button.
 * @param ok_cb       The callback for the @c OK button.
 * @param cancel_text The text for the @c Cancel button.
 * @param cancel_cb   The callback for the @c Cancel button.
 * @param user_data   The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_fields(void *handle, const char *title,
						  const char *primary, const char *secondary,
						  GaimRequestFields *fields,
						  const char *ok_text, GCallback ok_cb,
						  const char *cancel_text, GCallback cancel_cb,
						  void *user_data);

/**
 * Closes a request.
 *
 * @param type     The request type.
 * @param uihandle The request UI handle.
 */
void gaim_request_close(GaimRequestType type, void *uihandle);

/**
 * Closes all requests registered with the specified handle.
 *
 * @param handle The handle.
 */
void gaim_request_close_with_handle(void *handle);

/**
 * A wrapper for gaim_request_action() that uses @c Yes and @c No buttons.
 */
#define gaim_request_yes_no(handle, title, primary, secondary, \
							default_action, user_data, yes_cb, no_cb) \
	gaim_request_action((handle), (title), (primary), (secondary), \
						(default_action), (user_data), 2, \
						_("_Yes"), (yes_cb), _("_No"), (no_cb))

/**
 * A wrapper for gaim_request_action() that uses @c OK and @c Cancel buttons.
 */
#define gaim_request_ok_cancel(handle, title, primary, secondary, \
							default_action, user_data, ok_cb, cancel_cb) \
	gaim_request_action((handle), (title), (primary), (secondary), \
						(default_action), (user_data), 2, \
						_("_OK"), (ok_cb), _("_Cancel"), (cancel_cb))

/**
 * A wrapper for gaim_request_action() that uses Accept and Cancel buttons.
 */
#define gaim_request_accept_cancel(handle, title, primary, secondary, \
								   default_action, user_data, accept_cb, \
								   cancel_cb) \
	gaim_request_action((handle), (title), (primary), (secondary), \
						(default_action), (user_data), 2, \
						_("_Accept"), (accept_cb), _("_Cancel"), (cancel_cb))

/**
 * Displays a file selector request dialog.  Returns the selected filename to
 * the callback.  Can be used for either opening a file or saving a file.
 *
 * @param handle      The plugin or connection handle.  For some
 *                    things this is EXTREMELY important.  See
 *                    the comments on gaim_request_input.
 * @param title       The title for the dialog (may be @c NULL)
 * @param filename    The default filename (may be @c NULL)
 * @param savedialog  True if this dialog is being used to save a file.
 *                    False if it is being used to open a file.
 * @param ok_cb       The callback for the @c OK button.
 * @param cancel_cb   The callback for the @c Cancel button.
 * @param user_data   The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_file(void *handle, const char *title, const char *filename,
						gboolean savedialog,
						GCallback ok_cb, GCallback cancel_cb,
						void *user_data);

/**
 * Displays a folder select dialog. Returns the selected filename to
 * the callback.
 *
 * @param handle      The plugin or connection handle.  For some
 *                    things this is EXTREMELY important.  See
 *                    the comments on gaim_request_input.
 * @param title       The title for the dialog (may be @c NULL)
 * @param dirname     The default directory name (may be @c NULL)
 * @param ok_cb       The callback for the @c OK button.
 * @param cancel_cb   The callback for the @c Cancel button.
 * @param user_data   The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_request_folder(void *handle, const char *title, const char *dirname,
						GCallback ok_cb, GCallback cancel_cb,
						void *user_data);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when displaying a
 * request.
 *
 * @param ops The UI operations structure.
 */
void gaim_request_set_ui_ops(GaimRequestUiOps *ops);

/**
 * Returns the UI operations structure to be used when displaying a
 * request.
 *
 * @return The UI operations structure.
 */
GaimRequestUiOps *gaim_request_get_ui_ops(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_REQUEST_H_ */
