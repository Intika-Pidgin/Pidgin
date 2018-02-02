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


#define PURPLE_TYPE_PROTOCOL_ACTION  (purple_protocol_action_get_type())

typedef struct _PurpleProtocolAction PurpleProtocolAction;

typedef void (*PurpleProtocolActionCallback)(PurpleProtocolAction *action);

/**
 * PurpleMenuAction:
 *
 * A generic structure that contains information about an "action."  One
 * place this is is used is by protocols to tell the core the list of available
 * right-click actions for a buddy list row.
 */
typedef struct _PurpleMenuAction PurpleMenuAction;

#include "connection.h"

/**
 * PurpleProtocolAction:
 *
 * Represents an action that the protocol can perform. This shows up in the
 * Accounts menu, under a submenu with the name of the account.
 */
struct _PurpleProtocolAction {
	char *label;
	PurpleProtocolActionCallback callback;
	PurpleConnection *connection;
	gpointer user_data;
};

G_BEGIN_DECLS

/******************************************************************************
 * Menu Action API
 *****************************************************************************/

/**
 * purple_menu_action_new:
 * @label:    The text label to display for this action.
 * @callback: The function to be called when the action is used on
 *                 the selected item.
 * @data:     Additional data to be passed to the callback.
 * @children: (element-type PurpleMenuAction) (transfer full): Menu actions to
 *            be added as a submenu of this action.
 *
 * Creates a new PurpleMenuAction.
 *
 * Returns: The PurpleMenuAction.
 */
PurpleMenuAction *purple_menu_action_new(const gchar *label, GCallback callback, gpointer data, GList *children);

/**
 * purple_menu_action_free:
 * @act: The PurpleMenuAction to free.
 *
 * Frees a PurpleMenuAction
 */
void purple_menu_action_free(PurpleMenuAction *act);

/**
 * purple_menu_action_get_label:
 * @act:	The PurpleMenuAction.
 *
 * Returns the label of the PurpleMenuAction.
 *
 * Returns: The label string.
 */
gchar *purple_menu_action_get_label(const PurpleMenuAction *act);

/**
 * purple_menu_action_get_callback:
 * @act:	The PurpleMenuAction.
 *
 * Returns the callback of the PurpleMenuAction.
 *
 * Returns: The callback function.
 */
GCallback purple_menu_action_get_callback(const PurpleMenuAction *act);

/**
 * purple_menu_action_get_data:
 * @act:	The PurpleMenuAction.
 *
 * Returns the data stored in the PurpleMenuAction.
 *
 * Returns: The data.
 */
gpointer purple_menu_action_get_data(const PurpleMenuAction *act);

/**
 * purple_menu_action_get_children:
 * @act:	The PurpleMenuAction.
 *
 * Returns the children of the PurpleMenuAction.
 *
 * Returns: (element-type PurpleMenuAction) (transfer none): The menu children.
 */
GList* purple_menu_action_get_children(const PurpleMenuAction *act);

/**
 * purple_menu_action_set_label:
 * @act:   The menu action.
 * @label: The label for the menu action.
 *
 * Set the label to the PurpleMenuAction.
 */
void purple_menu_action_set_label(PurpleMenuAction *act, gchar *label);

/**
 * purple_menu_action_set_callback:
 * @act:        The menu action.
 * @callback:   The callback.
 *
 * Set the callback that will be used by the PurpleMenuAction.
 */
void purple_menu_action_set_callback(PurpleMenuAction *act, GCallback callback);

/**
 * purple_menu_action_set_data:
 * @act:   The menu action.
 * @data:  The data used by this PurpleMenuAction
 *
 * Set the label to the PurpleMenuAction.
 */
void purple_menu_action_set_data(PurpleMenuAction *act, gpointer data);

/**
 * purple_menu_action_set_children:
 * @act:       The menu action.
 * @children: (element-type PurpleMenuAction) (transfer full): The menu children
 *
 * Set the children of the PurpleMenuAction.
 */
void purple_menu_action_set_children(PurpleMenuAction *act, GList *children);

/**
 * purple_menu_action_set_stock_icon:
 * @act:   The menu action.
 * @stock: The stock icon identifier.
 *
 * Sets the icon for the PurpleMenuAction.
 */
void purple_menu_action_set_stock_icon(PurpleMenuAction *act, const gchar *stock);

/**
 * purple_menu_action_get_stock_icon:
 * @act: The menu action.
 *
 * Gets the stock icon of the PurpleMenuAction.
 *
 * Returns: The stock icon identifier.
 */
const gchar *purple_menu_action_get_stock_icon(PurpleMenuAction *act);

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
 */
PurpleProtocolAction *purple_protocol_action_new(const gchar *label, PurpleProtocolActionCallback callback);

/**
 * purple_protocol_action_free:
 * @action: The PurpleProtocolAction to free.
 *
 * Frees a PurpleProtocolAction
 */
void purple_protocol_action_free(PurpleProtocolAction *action);


G_END_DECLS

#endif /* PURPLE_ACTION */
