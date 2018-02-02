/* Purple is the legal property of its developers, whose names are too numerous
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

#include "action.h"

struct _PurpleMenuAction {
	gchar *label;
	GCallback callback;
	gpointer data;
	GList *children;
	gchar *stock_icon;
};

/******************************************************************************
 * ActionMenu API
 *****************************************************************************/
PurpleMenuAction *
purple_menu_action_new(const gchar *label, GCallback callback, gpointer data,
                       GList *children)
{
	PurpleMenuAction *act = g_new(PurpleMenuAction, 1);

	act->label = g_strdup(label);
	act->callback = callback;
	act->data = data;
	act->children = children;
	act->stock_icon = NULL;

	return act;
}

void
purple_menu_action_free(PurpleMenuAction *act) {
	g_return_if_fail(act != NULL);

	g_free(act->stock_icon);
	g_free(act->label);
	g_free(act);
}

gchar *
purple_menu_action_get_label(const PurpleMenuAction *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->label;
}

GCallback
purple_menu_action_get_callback(const PurpleMenuAction *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->callback;
}

gpointer
purple_menu_action_get_data(const PurpleMenuAction *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->data;
}

GList *
purple_menu_action_get_children(const PurpleMenuAction *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->children;
}

void
purple_menu_action_set_label(PurpleMenuAction *act, gchar *label) {
	g_return_if_fail(act != NULL);

	act-> label = label;
}

void
purple_menu_action_set_callback(PurpleMenuAction *act, GCallback callback) {
	g_return_if_fail(act != NULL);

	act->callback = callback;
}

void
purple_menu_action_set_data(PurpleMenuAction *act, gpointer data) {
	g_return_if_fail(act != NULL);

	act->data = data;
}

void
purple_menu_action_set_children(PurpleMenuAction *act, GList *children) {
	g_return_if_fail(act != NULL);

	act->children = children;
}

void
purple_menu_action_set_stock_icon(PurpleMenuAction *act, const gchar *stock) {
	g_return_if_fail(act != NULL);

	g_free(act->stock_icon);

	act->stock_icon = g_strdup(stock);
}

const gchar *
purple_menu_action_get_stock_icon(PurpleMenuAction *act) {
	return act->stock_icon;
}

