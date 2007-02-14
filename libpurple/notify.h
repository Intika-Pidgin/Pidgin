/**
 * @file notify.h Notification API
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
#ifndef _GAIM_NOTIFY_H_
#define _GAIM_NOTIFY_H_

#include <stdlib.h>
#include <glib-object.h>
#include <glib.h>

typedef struct _GaimNotifyUserInfoEntry	GaimNotifyUserInfoEntry;
typedef struct _GaimNotifyUserInfo	GaimNotifyUserInfo;

#include "connection.h"

/**
 * Notification close callbacks.
 */
typedef void  (*GaimNotifyCloseCallback) (gpointer user_data);


/**
 * Notification types.
 */
typedef enum
{
	GAIM_NOTIFY_MESSAGE = 0,   /**< Message notification.         */
	GAIM_NOTIFY_EMAIL,         /**< Single e-mail notification.   */
	GAIM_NOTIFY_EMAILS,        /**< Multiple e-mail notification. */
	GAIM_NOTIFY_FORMATTED,     /**< Formatted text.               */
	GAIM_NOTIFY_SEARCHRESULTS, /**< Buddy search results.         */
	GAIM_NOTIFY_USERINFO,      /**< Formatted userinfo text.      */
	GAIM_NOTIFY_URI            /**< URI notification or display.  */

} GaimNotifyType;


/**
 * Notification message types.
 */
typedef enum
{
	GAIM_NOTIFY_MSG_ERROR   = 0, /**< Error notification.       */
	GAIM_NOTIFY_MSG_WARNING,     /**< Warning notification.     */
	GAIM_NOTIFY_MSG_INFO         /**< Information notification. */

} GaimNotifyMsgType;


/**
 * The types of buttons
 */
typedef enum
{
	GAIM_NOTIFY_BUTTON_LABELED = 0,  /**< special use, see _button_add_labeled */
	GAIM_NOTIFY_BUTTON_CONTINUE = 1,
	GAIM_NOTIFY_BUTTON_ADD,
	GAIM_NOTIFY_BUTTON_INFO,
	GAIM_NOTIFY_BUTTON_IM,
	GAIM_NOTIFY_BUTTON_JOIN,
	GAIM_NOTIFY_BUTTON_INVITE
} GaimNotifySearchButtonType;


/**
 * Search results object.
 */
typedef struct
{
	GList *columns;        /**< List of the search column objects. */
	GList *rows;           /**< List of rows in the result. */
	GList *buttons;        /**< List of buttons to display. */

} GaimNotifySearchResults;

/**
 * Types of GaimNotifyUserInfoEntry objects
 */
typedef enum
{
	GAIM_NOTIFY_USER_INFO_ENTRY_PAIR = 0,
	GAIM_NOTIFY_USER_INFO_ENTRY_SECTION_BREAK,
	GAIM_NOTIFY_USER_INFO_ENTRY_SECTION_HEADER
} GaimNotifyUserInfoEntryType;

/**
 * Single column of a search result.
 */
typedef struct
{
	char *title; /**< Title of the column. */

} GaimNotifySearchColumn;


/**
 * Callback for a button in a search result.
 *
 * @param c         the GaimConnection passed to gaim_notify_searchresults
 * @param row       the contents of the selected row
 * @param user_data User defined data.
 */
typedef void (*GaimNotifySearchResultsCallback)(GaimConnection *c, GList *row,
												gpointer user_data);


/**
 * Definition of a button.
 */
typedef struct
{
	GaimNotifySearchButtonType type;
	GaimNotifySearchResultsCallback callback; /**< Function to be called when clicked. */
	char *label;                              /**< only for GAIM_NOTIFY_BUTTON_LABELED */
} GaimNotifySearchButton;


/**
 * Notification UI operations.
 */
typedef struct
{
	void *(*notify_message)(GaimNotifyMsgType type, const char *title,
	                        const char *primary, const char *secondary);

	void *(*notify_email)(GaimConnection *gc,
	                      const char *subject, const char *from,
	                      const char *to, const char *url);

	void *(*notify_emails)(GaimConnection *gc,
	                       size_t count, gboolean detailed,
	                       const char **subjects, const char **froms,
	                       const char **tos, const char **urls);

	void *(*notify_formatted)(const char *title, const char *primary,
	                          const char *secondary, const char *text);

	void *(*notify_searchresults)(GaimConnection *gc, const char *title,
	                              const char *primary, const char *secondary,
	                              GaimNotifySearchResults *results, gpointer user_data);

	void (*notify_searchresults_new_rows)(GaimConnection *gc,
	                                      GaimNotifySearchResults *results,
	                                      void *data);

	void *(*notify_userinfo)(GaimConnection *gc, const char *who,
	                         GaimNotifyUserInfo *user_info);

	void *(*notify_uri)(const char *uri);

	void (*close_notify)(GaimNotifyType type, void *ui_handle);

} GaimNotifyUiOps;


#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************/
/** Search results notification API                                       */
/**************************************************************************/
/*@{*/

/**
 * Displays results from a buddy search.  This can be, for example,
 * a window with a list of all found buddies, where you are given the
 * option of adding buddies to your buddy list.
 *
 * @param gc        The GaimConnection handle associated with the information.
 * @param title     The title of the message.  If this is NULL, the title
 *                  will be "Search Results."
 * @param primary   The main point of the message.
 * @param secondary The secondary information.
 * @param results   The GaimNotifySearchResults instance.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the close callback and any other
 *                  callback associated with a button.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_searchresults(GaimConnection *gc, const char *title,
								const char *primary, const char *secondary,
								GaimNotifySearchResults *results, GaimNotifyCloseCallback cb,
								gpointer user_data);

void gaim_notify_searchresults_free(GaimNotifySearchResults *results);

/**
 * Replace old rows with the new. Reuse an existing window.
 *
 * @param gc        The GaimConnection structure.
 * @param results   The GaimNotifySearchResults structure.
 * @param data      Data returned by the gaim_notify_searchresults().
 */
void gaim_notify_searchresults_new_rows(GaimConnection *gc,
										GaimNotifySearchResults *results,
										void *data);


/**
 * Adds a stock button that will be displayed in the search results dialog.
 *
 * @param results The search results object.
 * @param type    Type of the button. (TODO: Only one button of a given type can be displayed.)
 * @param cb      Function that will be called on the click event.
 */
void gaim_notify_searchresults_button_add(GaimNotifySearchResults *results,
										  GaimNotifySearchButtonType type,
										  GaimNotifySearchResultsCallback cb);


/**
 * Adds a plain labelled button that will be displayed in the search results dialog.
 * 
 * @param results The search results object
 * @param label   The label to display
 * @param cb      Function that will be called on the click event
 */
void gaim_notify_searchresults_button_add_labeled(GaimNotifySearchResults *results,
                                                  const char *label,
                                                  GaimNotifySearchResultsCallback cb);


/**
 * Returns a newly created search results object.
 *
 * @return The new search results object.
 */
GaimNotifySearchResults *gaim_notify_searchresults_new(void);

/**
 * Returns a newly created search result column object.
 *
 * @param title Title of the column. NOTE: Title will get g_strdup()ed.
 * 
 * @return The new search column object.
 */
GaimNotifySearchColumn *gaim_notify_searchresults_column_new(const char *title);

/**
 * Adds a new column to the search result object.
 *
 * @param results The result object to which the column will be added.
 * @param column The column that will be added to the result object.
 */
void gaim_notify_searchresults_column_add(GaimNotifySearchResults *results,
										  GaimNotifySearchColumn *column);

/**
 * Adds a new row of the results to the search results object.
 *
 * @param results The search results object.
 * @param row     The row of the results.
 */
void gaim_notify_searchresults_row_add(GaimNotifySearchResults *results,
									   GList *row);

/**
 * Returns a number of the rows in the search results object.
 * 
 * @param results The search results object.
 *
 * @return Number of the result rows.
 */
guint gaim_notify_searchresults_get_rows_count(GaimNotifySearchResults *results);

/**
 * Returns a number of the columns in the search results object.
 *
 * @param results The search results object.
 *
 * @return Number of the columns.
 */
guint gaim_notify_searchresults_get_columns_count(GaimNotifySearchResults *results);

/**
 * Returns a row of the results from the search results object.
 *
 * @param results The search results object.
 * @param row_id  Index of the row to be returned.
 *
 * @return Row of the results.
 */
GList *gaim_notify_searchresults_row_get(GaimNotifySearchResults *results,
										 unsigned int row_id);

/**
 * Returns a title of the search results object's column.
 * 
 * @param results   The search results object.
 * @param column_id Index of the column.
 *
 * @return Title of the column.
 */
char *gaim_notify_searchresults_column_get_title(GaimNotifySearchResults *results,
												 unsigned int column_id);

/*@}*/

/**************************************************************************/
/** @name Notification API                                                */
/**************************************************************************/
/*@{*/

/**
 * Displays a notification message to the user.
 *
 * @param handle    The plugin or connection handle.
 * @param type      The notification type.
 * @param title     The title of the message.
 * @param primary   The main point of the message.
 * @param secondary The secondary information.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_message(void *handle, GaimNotifyMsgType type,
						  const char *title, const char *primary,
						  const char *secondary, GaimNotifyCloseCallback cb,
						  gpointer user_data);

/**
 * Displays a single e-mail notification to the user.
 *
 * @param handle    The plugin or connection handle.
 * @param subject   The subject of the e-mail.
 * @param from      The from address.
 * @param to        The destination address.
 * @param url       The URL where the message can be read.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_email(void *handle, const char *subject,
						const char *from, const char *to,
						const char *url, GaimNotifyCloseCallback cb,
						gpointer user_data);

/**
 * Displays a notification for multiple e-mails to the user.
 *
 * @param handle    The plugin or connection handle.
 * @param count     The number of e-mails.
 * @param detailed  @c TRUE if there is information for each e-mail in the
 *                  arrays.
 * @param subjects  The array of subjects.
 * @param froms     The array of from addresses.
 * @param tos       The array of destination addresses.
 * @param urls      The URLs where the messages can be read.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_emails(void *handle, size_t count, gboolean detailed,
						 const char **subjects, const char **froms,
						 const char **tos, const char **urls,
						 GaimNotifyCloseCallback cb, gpointer user_data);

/**
 * Displays a notification with formatted text.
 *
 * The text is essentially a stripped-down format of HTML, the same that
 * IMs may send.
 *
 * @param handle    The plugin or connection handle.
 * @param title     The title of the message.
 * @param primary   The main point of the message.
 * @param secondary The secondary information.
 * @param text      The formatted text.
 * @param cb        The callback to call when the user closes
 *                  the notification.
 * @param user_data The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_formatted(void *handle, const char *title,
							const char *primary, const char *secondary,
							const char *text, GaimNotifyCloseCallback cb, gpointer user_data);

/**
 * Displays user information with formatted text, passing information giving
 * the connection and username from which the user information came.
 *
 * The text is essentially a stripped-down format of HTML, the same that
 * IMs may send.
 *
 * @param gc		         The GaimConnection handle associated with the information.
 * @param who				 The username associated with the information.
 * @param user_info          The GaimNotifyUserInfo which contains the information
 * @param cb                 The callback to call when the user closes
 *                           the notification.
 * @param user_data          The data to pass to the callback.
 *
 * @return A UI-specific handle.
 */
void *gaim_notify_userinfo(GaimConnection *gc, const char *who,
						   GaimNotifyUserInfo *user_info, GaimNotifyCloseCallback cb,
						   gpointer user_data);

/**
 * Create a new GaimNotifyUserInfo which is suitable for passing to gaim_notify_userinfo()
 *
 * @return A new GaimNotifyUserInfo, which the caller must destroy when done
 */
GaimNotifyUserInfo *gaim_notify_user_info_new(void);

/**
 * Destroy a GaimNotifyUserInfo
 *
 * @param user_info          The GaimNotifyUserInfo
 */
void gaim_notify_user_info_destroy(GaimNotifyUserInfo *user_info);

/**
 * Retrieve the array of GaimNotifyUserInfoEntry objects from a GaimNotifyUserInfo
 *
 * This GList may be manipulated directly with normal GList functions such as g_list_insert(). Only 
 * GaimNotifyUserInfoEntry are allowed in the list.  If a GaimNotifyUserInfoEntry item is added to the list,
 * it should not be g_free()'d by the caller; GaimNotifyUserInfo will g_free it when destroyed.
 *
 * To remove a GaimNotifyUserInfoEntry, use gaim_notify_user_info_remove_entry(). Do not use the GList directly.
 *
 * @param user_info          The GaimNotifyUserInfo
 *
 * @result                   A GList of GaimNotifyUserInfoEntry objects
 */
GList *gaim_notify_user_info_get_entries(GaimNotifyUserInfo *user_info);

/**
 * Create a textual representation of a GaimNotifyUserInfo, separating entries with newline
 *
 * @param user_info          The GaimNotifyUserInfo
 * @param newline            The separation character
 */
char *gaim_notify_user_info_get_text_with_newline(GaimNotifyUserInfo *user_info, const char *newline);

/**
 * Add a label/value pair to a GaimNotifyUserInfo object.
 * GaimNotifyUserInfo keeps track of the order in which pairs are added.
 *
 * @param user_info          The GaimNotifyUserInfo
 * @param label              A label, which for example might be displayed by a UI with a colon after it ("Status:"). Do not include a colon.
 *                           If NULL, value will be displayed without a label.
 * @param value              The value, which might be displayed by a UI after the label.
 *                           If NULL, label will still be displayed; the UI should then treat label as independent
 *                           and not include a colon if it would otherwise.
 */
void gaim_notify_user_info_add_pair(GaimNotifyUserInfo *user_info, const char *label, const char *value);

/**
 * Prepend a label/value pair to a GaimNotifyUserInfo object
 *
 * @param user_info          The GaimNotifyUserInfo
 * @param label              A label, which for example might be displayed by a UI with a colon after it ("Status:"). Do not include a colon.
 *                           If NULL, value will be displayed without a label.
 * @param value              The value, which might be displayed by a UI after the label.
 *                           If NULL, label will still be displayed; the UI should then treat label as independent
 *                           and not include a colon if it would otherwise.
 */
void gaim_notify_user_info_prepend_pair(GaimNotifyUserInfo *user_info, const char *label, const char *value);

/**
 * Remove a GaimNotifyUserInfoEntry from a GaimNotifyUserInfo object
 *
 * @param user_info          The GaimNotifyUserInfo
 * @param user_info_entry    The GaimNotifyUserInfoEntry
 */
void gaim_notify_user_info_remove_entry(GaimNotifyUserInfo *user_info, GaimNotifyUserInfoEntry *user_info_entry);
/**
 * Create a new GaimNotifyUserInfoEntry
 *
 * If added to a GaimNotifyUserInfo object, this should not be free()'d, as GaimNotifyUserInfo will do so
 * when destroyed.  gaim_notify_user_info_add_pair() and gaim_notify_user_info_prepend_pair() are convenience
 * methods for creating entries and adding them to a GaimNotifyUserInfo.
 *
 * @param label              A label, which for example might be displayed by a UI with a colon after it ("Status:"). Do not include a colon.
 *                           If NULL, value will be displayed without a label.
 * @param value              The value, which might be displayed by a UI after the label.
 *                           If NULL, label will still be displayed; the UI should then treat label as independent
 *                           and not include a colon if it would otherwise.
 *
 * @result A new GaimNotifyUserInfoEntry
 */
GaimNotifyUserInfoEntry *gaim_notify_user_info_entry_new(const char *label, const char *value);

/**
 * Add a section break.  A UI might display this as a horizontal line.
 *
 * @param user_info          The GaimNotifyUserInfo
 */
void gaim_notify_user_info_add_section_break(GaimNotifyUserInfo *user_info);

/**
 * Add a section header.  A UI might display this in a different font from other text.
 *
 * @param user_info          The GaimNotifyUserInfo
 * @param label              The name of the section
 */
void gaim_notify_user_info_add_section_header(GaimNotifyUserInfo *user_info, const char *label);

/**
 * Remove the last item which was added to a GaimNotifyUserInfo. This could be used to remove a section header which is not needed.
 */
void gaim_notify_user_info_remove_last_item(GaimNotifyUserInfo *user_info);

/**
 * Get the label for a GaimNotifyUserInfoEntry
 *
 * @param user_info_entry     The GaimNotifyUserInfoEntry
 *
 * @result                    The label
 */
gchar *gaim_notify_user_info_entry_get_label(GaimNotifyUserInfoEntry *user_info_entry);

/**
 * Set the label for a GaimNotifyUserInfoEntry
 *
 * @param user_info_entry     The GaimNotifyUserInfoEntry
 * @param label			      The label
 */
void gaim_notify_user_info_entry_set_label(GaimNotifyUserInfoEntry *user_info_entry, const char *label);

/**
 * Get the value for a GaimNotifyUserInfoEntry
 *
 * @param user_info_entry     The GaimNotifyUserInfoEntry
 *
 * @result                    The value
 */
gchar *gaim_notify_user_info_entry_get_value(GaimNotifyUserInfoEntry *user_info_entry);

/**
 * Set the value for a GaimNotifyUserInfoEntry
 *
 * @param user_info_entry     The GaimNotifyUserInfoEntry
 * @param value				  The value
 */
void gaim_notify_user_info_entry_set_value(GaimNotifyUserInfoEntry *user_info_entry, const char *value);


/**
 * Get the type of a GaimNotifyUserInfoEntry
 *
 * @param user_info_entry     The GaimNotifyUserInfoEntry
 *
 * @result					  The GaimNotifyUserInfoEntryType
 */
GaimNotifyUserInfoEntryType gaim_notify_user_info_entry_get_type(GaimNotifyUserInfoEntry *user_info_entry);

/**
 * Set the type of a GaimNotifyUserInfoEntry
 *
 * @param user_info_entry     The GaimNotifyUserInfoEntry
 * @param					  The GaimNotifyUserInfoEntryType
 */
void gaim_notify_user_info_entry_set_type(GaimNotifyUserInfoEntry *user_info_entry,
										  GaimNotifyUserInfoEntryType type);

/**
 * Opens a URI or somehow presents it to the user.
 *
 * @param handle The plugin or connection handle.
 * @param uri    The URI to display or go to.
 *
 * @return A UI-specific handle, if any. This may only be presented if
 *         the UI code displays a dialog instead of a webpage, or something
 *         similar.
 */
void *gaim_notify_uri(void *handle, const char *uri);

/**
 * Closes a notification.
 *
 * This should be used only by the UI operation functions and part of the
 * core.
 *
 * @param type      The notification type.
 * @param ui_handle The notification UI handle.
 */
void gaim_notify_close(GaimNotifyType type, void *ui_handle);

/**
 * Closes all notifications registered with the specified handle.
 *
 * @param handle The handle.
 */
void gaim_notify_close_with_handle(void *handle);

/**
 * A wrapper for gaim_notify_message that displays an information message.
 */
#define gaim_notify_info(handle, title, primary, secondary) \
	gaim_notify_message((handle), GAIM_NOTIFY_MSG_INFO, (title), \
						(primary), (secondary), NULL, NULL)

/**
 * A wrapper for gaim_notify_message that displays a warning message.
 */
#define gaim_notify_warning(handle, title, primary, secondary) \
	gaim_notify_message((handle), GAIM_NOTIFY_MSG_WARNING, (title), \
						(primary), (secondary), NULL, NULL)

/**
 * A wrapper for gaim_notify_message that displays an error message.
 */
#define gaim_notify_error(handle, title, primary, secondary) \
	gaim_notify_message((handle), GAIM_NOTIFY_MSG_ERROR, (title), \
						(primary), (secondary), NULL, NULL)

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when displaying a
 * notification.
 *
 * @param ops The UI operations structure.
 */
void gaim_notify_set_ui_ops(GaimNotifyUiOps *ops);

/**
 * Returns the UI operations structure to be used when displaying a
 * notification.
 *
 * @return The UI operations structure.
 */
GaimNotifyUiOps *gaim_notify_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Notify Subsystem                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns the notify subsystem handle.
 *
 * @return The notify subsystem handle.
 */
void *gaim_notify_get_handle(void);

/**
 * Initializes the notify subsystem.
 */
void gaim_notify_init(void);

/**
 * Uninitializes the notify subsystem.
 */
void gaim_notify_uninit(void);

/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* _GAIM_NOTIFY_H_ */
