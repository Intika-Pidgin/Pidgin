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

#ifndef PURPLE_ACTION
#define PURPLE_ACTION

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_PROTOCOL_ACTION (purple_protocol_action_get_type())
typedef struct _PurpleProtocolAction PurpleProtocolAction;

typedef void (*PurpleProtocolActionCallback)(PurpleProtocolAction *action);

/**
 * PurpleActionMenu:
 *
 * A generic structure that contains information about an "action".  One
 * place this is used is by protocols to tell the core the list of available
 * right-click actions for a buddy list row.
 */
typedef struct _PurpleActionMenu PurpleActionMenu;

#include "connection.h"

/**
 * PurpleProtocolAction:
 * @label: A translated string to be shown in a user interface.
 * @callback: The function to call when the user wants to perform this action.
 * @connection: The connection that this action should be performed against.
 * @user_data: User data to pass to @callback.
 *
 * Represents an action that the protocol can perform. This shows up in the
 * Accounts menu, under a submenu with the name of the account.
 */
struct _PurpleProtocolAction {
	gchar *label;
	PurpleProtocolActionCallback callback;
	PurpleConnection *connection;
	gpointer user_data;
};

G_BEGIN_DECLS

/******************************************************************************
 * Menu Action API
 *****************************************************************************/

/**
 * purple_action_menu_new:
 * @label:    The text label to display for this action.
 * @callback: (scope notified): The function to be called when the action is used on
 *                 the selected item.
 * @data:     Additional data to be passed to the callback.
 * @children: (element-type PurpleActionMenu) (transfer full): Menu actions to
 *            be added as a submenu of this action.
 *
 * Creates a new PurpleActionMenu.
 *
 * Returns: (transfer full): The PurpleActionMenu.
 */
PurpleActionMenu *purple_action_menu_new(const gchar *label, GCallback callback, gpointer data, GList *children);

/**
 * purple_action_menu_free:
 * @act: The PurpleActionMenu to free.
 *
 * Frees a PurpleActionMenu
 */
void purple_action_menu_free(PurpleActionMenu *act);

/**
 * purple_action_menu_get_label:
 * @act:	The PurpleActionMenu.
 *
 * Returns the label of the PurpleActionMenu.
 *
 * Returns: The label string.
 */
const gchar *purple_action_menu_get_label(const PurpleActionMenu *act);

/**
 * purple_action_menu_get_callback:
 * @act:	The PurpleActionMenu.
 *
 * Returns the callback of the PurpleActionMenu.
 *
 * Returns: (transfer none): The callback function.
 */
GCallback purple_action_menu_get_callback(const PurpleActionMenu *act);

/**
 * purple_action_menu_get_data:
 * @act:	The PurpleActionMenu.
 *
 * Returns the data stored in the PurpleActionMenu.
 *
 * Returns: The data.
 */
gpointer purple_action_menu_get_data(const PurpleActionMenu *act);

/**
 * purple_action_menu_get_children:
 * @act:	The PurpleActionMenu.
 *
 * Returns the children of the PurpleActionMenu.
 *
 * Returns: (element-type PurpleActionMenu) (transfer none): The menu children.
 */
GList* purple_action_menu_get_children(const PurpleActionMenu *act);

/**
 * purple_action_menu_set_label:
 * @act:   The menu action.
 * @label: The label for the menu action.
 *
 * Set the label to the PurpleActionMenu.
 */
void purple_action_menu_set_label(PurpleActionMenu *act, const gchar *label);

/**
 * purple_action_menu_set_callback:
 * @act:        The menu action.
 * @callback: (scope notified):  The callback.
 *
 * Set the callback that will be used by the PurpleActionMenu.
 */
void purple_action_menu_set_callback(PurpleActionMenu *act, GCallback callback);

/**
 * purple_action_menu_set_data:
 * @act:   The menu action.
 * @data:  The data used by this PurpleActionMenu
 *
 * Set the label to the PurpleActionMenu.
 */
void purple_action_menu_set_data(PurpleActionMenu *act, gpointer data);

/**
 * purple_action_menu_set_children:
 * @act:       The menu action.
 * @children: (element-type PurpleActionMenu) (transfer full): The menu children
 *
 * Set the children of the PurpleActionMenu.
 */
void purple_action_menu_set_children(PurpleActionMenu *act, GList *children);

/******************************************************************************
 * Protocol Action API
 *****************************************************************************/

/**
 * purple_protocol_action_get_type:
 *
 * Returns: The #GType for the #PurpleProtocolAction boxed structure.
 */
GType purple_protocol_action_get_type(void);

/**
 * purple_protocol_action_new:
 * @label:    The description of the action to show to the user.
 * @callback: (scope call): The callback to call when the user selects this
 *            action.
 *
 * Allocates and returns a new PurpleProtocolAction. Use this to add actions in
 * a list in the get_actions function of the protocol.
 *
 * Returns: (transfer full): The new #PurpleProtocolAction.
 */
PurpleProtocolAction *purple_protocol_action_new(const gchar *label, PurpleProtocolActionCallback callback);

/**
 * purple_protocol_action_copy:
 * @action: The #PurpleProtocolAction to copy.
 *
 * Creates a newly allocated copy of @action.
 *
 * Returns: (transfer full): A copy of @action.
 */
PurpleProtocolAction *purple_protocol_action_copy(PurpleProtocolAction *action);

/**
 * purple_protocol_action_free:
 * @action: The PurpleProtocolAction to free.
 *
 * Frees a PurpleProtocolAction
 */
void purple_protocol_action_free(PurpleProtocolAction *action);


G_END_DECLS

#endif /* PURPLE_ACTION */
