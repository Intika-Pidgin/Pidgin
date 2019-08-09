/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */
#include "internal.h"
#include "pidgin.h"

#include "gtkutils.h"
#include "pidginaccountchooser.h"

/******************************************************************************
 * Enums
 *****************************************************************************/

enum
{
	AOP_ICON_COLUMN,
	AOP_NAME_COLUMN,
	AOP_DATA_COLUMN,
	AOP_COLUMN_COUNT
};

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	GtkTreeModel *model;
	gint default_item;
} AopMenu;

/******************************************************************************
 * Code
 *****************************************************************************/
static gpointer
aop_option_menu_get_selected(GtkWidget *optmenu)
{
	gpointer data = NULL;
	GtkTreeIter iter;

	g_return_val_if_fail(optmenu != NULL, NULL);

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(optmenu), &iter)) {
		gtk_tree_model_get(
		        gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)), &iter,
		        AOP_DATA_COLUMN, &data, -1);
	}

	return data;
}

static void
aop_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	if (cb != NULL) {
		((void (*)(GtkWidget *, gpointer, gpointer))cb)(
		        optmenu, aop_option_menu_get_selected(optmenu),
		        g_object_get_data(G_OBJECT(optmenu), "user_data"));
	}
}

static void
aop_option_menu_replace_menu(GtkWidget *optmenu, AopMenu *new_aop_menu)
{
	gtk_combo_box_set_model(GTK_COMBO_BOX(optmenu), new_aop_menu->model);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu),
	                         new_aop_menu->default_item);
	g_free(new_aop_menu);
}

static GtkWidget *
aop_option_menu_new(AopMenu *aop_menu, GCallback cb, gpointer user_data)
{
	GtkWidget *optmenu = NULL;
	GtkCellRenderer *cr = NULL;

	optmenu = gtk_combo_box_new();
	gtk_widget_show(optmenu);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(optmenu),
	                           cr = gtk_cell_renderer_pixbuf_new(), FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(optmenu), cr, "pixbuf",
	                              AOP_ICON_COLUMN);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(optmenu),
	                           cr = gtk_cell_renderer_text_new(), TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(optmenu), cr, "text",
	                              AOP_NAME_COLUMN);

	aop_option_menu_replace_menu(optmenu, aop_menu);
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);

	g_signal_connect(G_OBJECT(optmenu), "changed", G_CALLBACK(aop_menu_cb),
	                 cb);

	return optmenu;
}

static void
aop_option_menu_select_by_data(GtkWidget *optmenu, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer iter_data;
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu));
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, AOP_DATA_COLUMN,
			                   &iter_data, -1);
			if (iter_data == data) {
				gtk_combo_box_set_active_iter(
				        GTK_COMBO_BOX(optmenu), &iter);
				return;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}

PurpleAccount *
pidgin_account_option_menu_get_selected(GtkWidget *optmenu)
{
	return (PurpleAccount *)aop_option_menu_get_selected(optmenu);
}

static AopMenu *
create_account_menu(PurpleAccount *default_account,
                    PurpleFilterAccountFunc filter_func, gboolean show_all)
{
	AopMenu *aop_menu = NULL;
	PurpleAccount *account;
	GdkPixbuf *pixbuf = NULL;
	GList *list;
	GList *p;
	GtkListStore *ls;
	GtkTreeIter iter;
	gint i;
	gchar buf[256];

	if (show_all) {
		list = purple_accounts_get_all();
	} else {
		list = purple_connections_get_all();
	}

	ls = gtk_list_store_new(AOP_COLUMN_COUNT, GDK_TYPE_PIXBUF,
	                        G_TYPE_STRING, G_TYPE_POINTER);

	aop_menu = g_malloc0(sizeof(AopMenu));
	aop_menu->default_item = 0;
	aop_menu->model = GTK_TREE_MODEL(ls);

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		if (show_all) {
			account = (PurpleAccount *)p->data;
		} else {
			PurpleConnection *gc = (PurpleConnection *)p->data;

			account = purple_connection_get_account(gc);
		}

		if (filter_func && !filter_func(account)) {
			i--;
			continue;
		}

		pixbuf = pidgin_create_protocol_icon(
		        account, PIDGIN_PROTOCOL_ICON_SMALL);

		if (pixbuf) {
			if (purple_account_is_disconnected(account) &&
			    show_all && purple_connections_get_all()) {
				gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf,
				                                 0.0, FALSE);
			}
		}

		if (purple_account_get_private_alias(account)) {
			g_snprintf(buf, sizeof(buf), "%s (%s) (%s)",
			           purple_account_get_username(account),
			           purple_account_get_private_alias(account),
			           purple_account_get_protocol_name(account));
		} else {
			g_snprintf(buf, sizeof(buf), "%s (%s)",
			           purple_account_get_username(account),
			           purple_account_get_protocol_name(account));
		}

		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter, AOP_ICON_COLUMN, pixbuf,
		                   AOP_NAME_COLUMN, buf, AOP_DATA_COLUMN,
		                   account, -1);

		if (pixbuf) {
			g_object_unref(pixbuf);
		}

		if (default_account && account == default_account) {
			aop_menu->default_item = i;
		}
	}

	return aop_menu;
}

static void
regenerate_account_menu(GtkWidget *optmenu)
{
	gboolean show_all;
	PurpleAccount *account;
	PurpleFilterAccountFunc filter_func;

	account = (PurpleAccount *)aop_option_menu_get_selected(optmenu);
	show_all = GPOINTER_TO_INT(
	        g_object_get_data(G_OBJECT(optmenu), "show_all"));
	filter_func = g_object_get_data(G_OBJECT(optmenu), "filter_func");

	aop_option_menu_replace_menu(
	        optmenu, create_account_menu(account, filter_func, show_all));
}

static void
account_menu_sign_on_off_cb(PurpleConnection *gc, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static void
account_menu_added_removed_cb(PurpleAccount *account, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *optmenu, GdkEvent *event, void *user_data)
{
	purple_signals_disconnect_by_handle(optmenu);

	return FALSE;
}

void
pidgin_account_option_menu_set_selected(GtkWidget *optmenu,
                                        PurpleAccount *account)
{
	aop_option_menu_select_by_data(optmenu, account);
}

GtkWidget *
pidgin_account_option_menu_new(PurpleAccount *default_account,
                               gboolean show_all, GCallback cb,
                               PurpleFilterAccountFunc filter_func,
                               gpointer user_data)
{
	GtkWidget *optmenu;

	/* Create the option menu */
	optmenu = aop_option_menu_new(
	        create_account_menu(default_account, filter_func, show_all), cb,
	        user_data);

	g_signal_connect(G_OBJECT(optmenu), "destroy",
	                 G_CALLBACK(account_menu_destroyed_cb), NULL);

	/* Register the purple sign on/off event callbacks. */
	purple_signal_connect(
	        purple_connections_get_handle(), "signed-on", optmenu,
	        PURPLE_CALLBACK(account_menu_sign_on_off_cb), optmenu);
	purple_signal_connect(
	        purple_connections_get_handle(), "signed-off", optmenu,
	        PURPLE_CALLBACK(account_menu_sign_on_off_cb), optmenu);
	purple_signal_connect(
	        purple_accounts_get_handle(), "account-added", optmenu,
	        PURPLE_CALLBACK(account_menu_added_removed_cb), optmenu);
	purple_signal_connect(
	        purple_accounts_get_handle(), "account-removed", optmenu,
	        PURPLE_CALLBACK(account_menu_added_removed_cb), optmenu);

	/* Set some data. */
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);
	g_object_set_data(G_OBJECT(optmenu), "show_all",
	                  GINT_TO_POINTER(show_all));
	g_object_set_data(G_OBJECT(optmenu), "filter_func", filter_func);

	return optmenu;
}
