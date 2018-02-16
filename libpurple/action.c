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

struct _PurpleActionMenu {
	gchar *label;
	GCallback callback;
	gpointer data;
	GList *children;
	gchar *stock_icon;
};

/******************************************************************************
 * ActionMenu API
 *****************************************************************************/
PurpleActionMenu *
purple_menu_action_new(const gchar *label, GCallback callback, gpointer data,
                       GList *children)
{
	PurpleActionMenu *act = g_new(PurpleActionMenu, 1);

	act->label = g_strdup(label);
	act->callback = callback;
	act->data = data;
	act->children = children;
	act->stock_icon = NULL;

	return act;
}

void
purple_menu_action_free(PurpleActionMenu *act) {
	g_return_if_fail(act != NULL);

	g_free(act->stock_icon);
	g_free(act->label);
	g_free(act);
}

gchar *
purple_menu_action_get_label(const PurpleActionMenu *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->label;
}

GCallback
purple_menu_action_get_callback(const PurpleActionMenu *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->callback;
}

gpointer
purple_menu_action_get_data(const PurpleActionMenu *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->data;
}

GList *
purple_menu_action_get_children(const PurpleActionMenu *act) {
	g_return_val_if_fail(act != NULL, NULL);

	return act->children;
}

void
purple_menu_action_set_label(PurpleActionMenu *act, gchar *label) {
	g_return_if_fail(act != NULL);

	act-> label = label;
}

void
purple_menu_action_set_callback(PurpleActionMenu *act, GCallback callback) {
	g_return_if_fail(act != NULL);

	act->callback = callback;
}

void
purple_menu_action_set_data(PurpleActionMenu *act, gpointer data) {
	g_return_if_fail(act != NULL);

	act->data = data;
}

void
purple_menu_action_set_children(PurpleActionMenu *act, GList *children) {
	g_return_if_fail(act != NULL);

	act->children = children;
}

void
purple_menu_action_set_stock_icon(PurpleActionMenu *act, const gchar *stock) {
	g_return_if_fail(act != NULL);

	g_free(act->stock_icon);

	act->stock_icon = g_strdup(stock);
}

const gchar *
purple_menu_action_get_stock_icon(PurpleActionMenu *act) {
	return act->stock_icon;
}

/******************************************************************************
 * Protocol Action API
 *****************************************************************************/
PurpleProtocolAction *
purple_protocol_action_new(const gchar* label, PurpleProtocolActionCallback callback) {
	PurpleProtocolAction *action;

	g_return_val_if_fail(label != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	action = g_new0(PurpleProtocolAction, 1);

	action->label    = g_strdup(label);
	action->callback = callback;

	return action;
}

void
purple_protocol_action_free(PurpleProtocolAction *action) {
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_free(action);
}

static PurpleProtocolAction *
purple_protocol_action_copy(PurpleProtocolAction *action) {
	g_return_val_if_fail(action != NULL, NULL);

	return purple_protocol_action_new(action->label, action->callback);
}

G_DEFINE_BOXED_TYPE(
	PurpleProtocolAction,
	purple_protocol_action,
	purple_protocol_action_copy,
	purple_protocol_action_free
);
