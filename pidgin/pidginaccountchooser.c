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

enum
{
	PROP_0,
	PROP_ACCOUNT,
	PROP_SHOW_ALL,
	PROP_LAST
};

/******************************************************************************
 * Structs
 *****************************************************************************/

struct _PidginAccountChooser {
	GtkComboBox parent;

	GtkListStore *model;

	PurpleFilterAccountFunc filter_func;
	gboolean show_all;
};

/******************************************************************************
 * Code
 *****************************************************************************/
static GParamSpec *properties[PROP_LAST] = {NULL};

G_DEFINE_TYPE(PidginAccountChooser, pidgin_account_chooser, GTK_TYPE_COMBO_BOX)

static gpointer
account_chooser_get_selected(PidginAccountChooser *chooser)
{
	gpointer data = NULL;
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(chooser), &iter)) {
		gtk_tree_model_get(
		        gtk_combo_box_get_model(GTK_COMBO_BOX(chooser)), &iter,
		        AOP_DATA_COLUMN, &data, -1);
	}

	return data;
}

static void
account_chooser_select_by_data(GtkWidget *chooser, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer iter_data;
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(chooser));
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, AOP_DATA_COLUMN,
			                   &iter_data, -1);
			if (iter_data == data) {
				gtk_combo_box_set_active_iter(
				        GTK_COMBO_BOX(chooser), &iter);
				return;
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}

static void
set_account_menu(PidginAccountChooser *chooser, PurpleAccount *default_account)
{
	PurpleAccount *account;
	GdkPixbuf *pixbuf = NULL;
	GList *list;
	GList *p;
	GtkTreeIter iter;
	gint default_item = 0;
	gint i;
	gchar buf[256];

	if (chooser->show_all) {
		list = purple_accounts_get_all();
	} else {
		list = purple_connections_get_all();
	}

	gtk_list_store_clear(chooser->model);
	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		if (chooser->show_all) {
			account = (PurpleAccount *)p->data;
		} else {
			PurpleConnection *gc = (PurpleConnection *)p->data;

			account = purple_connection_get_account(gc);
		}

		if (chooser->filter_func && !chooser->filter_func(account)) {
			i--;
			continue;
		}

		pixbuf = pidgin_create_protocol_icon(
		        account, PIDGIN_PROTOCOL_ICON_SMALL);

		if (pixbuf) {
			if (purple_account_is_disconnected(account) &&
			    chooser->show_all && purple_connections_get_all()) {
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

		gtk_list_store_append(chooser->model, &iter);
		gtk_list_store_set(chooser->model, &iter, AOP_ICON_COLUMN,
		                   pixbuf, AOP_NAME_COLUMN, buf,
		                   AOP_DATA_COLUMN, account, -1);

		if (pixbuf) {
			g_object_unref(pixbuf);
		}

		if (default_account && account == default_account) {
			default_item = i;
		}
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(chooser), default_item);
}

static void
regenerate_account_menu(PidginAccountChooser *chooser)
{
	PurpleAccount *account;

	account = (PurpleAccount *)account_chooser_get_selected(chooser);

	set_account_menu(chooser, account);
}

static void
account_menu_sign_on_off_cb(PurpleConnection *gc, PidginAccountChooser *chooser)
{
	regenerate_account_menu(chooser);
}

static void
account_menu_added_removed_cb(PurpleAccount *account,
                              PidginAccountChooser *chooser)
{
	regenerate_account_menu(chooser);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *chooser, GdkEvent *event, void *user_data)
{
	purple_signals_disconnect_by_handle(chooser);

	return FALSE;
}

static void
pidgin_account_chooser_changed_cb(GtkComboBox *widget, gpointer user_data)
{
	g_object_notify_by_pspec(G_OBJECT(widget), properties[PROP_ACCOUNT]);
}

/******************************************************************************
 * GObject implementation
 *****************************************************************************/
static void
pidgin_account_chooser_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(object);

	switch (prop_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value, account_chooser_get_selected(chooser));
			break;
		case PROP_SHOW_ALL:
			g_value_set_boolean(value, chooser->show_all);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void
pidgin_account_chooser_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(object);

	switch (prop_id) {
		case PROP_ACCOUNT:
			account_chooser_select_by_data(GTK_WIDGET(chooser),
			                               g_value_get_object(value));
			break;
		case PROP_SHOW_ALL:
			chooser->show_all = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void
pidgin_account_chooser_class_init(PidginAccountChooserClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* Properties */
	obj_class->get_property = pidgin_account_chooser_get_property;
	obj_class->set_property = pidgin_account_chooser_set_property;

	properties[PROP_ACCOUNT] = g_param_spec_object(
	        "account", "Account", "The account that is currently selected.",
	        PURPLE_TYPE_ACCOUNT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_SHOW_ALL] = g_param_spec_boolean(
	        "show-all", "Show all",
	        "Whether to show all accounts, or just online ones.", FALSE,
	        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
	                G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);

	/* Widget template */

	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin/Accounts/chooser.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginAccountChooser,
	                                     model);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        account_menu_destroyed_cb);
}

static void
pidgin_account_chooser_init(PidginAccountChooser *chooser)
{
	gtk_widget_init_template(GTK_WIDGET(chooser));

	g_signal_connect(chooser, "changed", pidgin_account_chooser_changed_cb,
	                 NULL);

	/* Register the purple sign on/off event callbacks. */
	purple_signal_connect(
	        purple_connections_get_handle(), "signed-on", chooser,
	        PURPLE_CALLBACK(account_menu_sign_on_off_cb), chooser);
	purple_signal_connect(
	        purple_connections_get_handle(), "signed-off", chooser,
	        PURPLE_CALLBACK(account_menu_sign_on_off_cb), chooser);
	purple_signal_connect(
	        purple_accounts_get_handle(), "account-added", chooser,
	        PURPLE_CALLBACK(account_menu_added_removed_cb), chooser);
	purple_signal_connect(
	        purple_accounts_get_handle(), "account-removed", chooser,
	        PURPLE_CALLBACK(account_menu_added_removed_cb), chooser);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_account_chooser_new(PurpleAccount *default_account, gboolean show_all)
{
	PidginAccountChooser *chooser = NULL;

	chooser = g_object_new(PIDGIN_TYPE_ACCOUNT_CHOOSER, "account",
	                       default_account, "show-all", show_all, NULL);
	set_account_menu(PIDGIN_ACCOUNT_CHOOSER(chooser), default_account);

	return GTK_WIDGET(chooser);
}

void
pidgin_account_chooser_set_filter_func(PidginAccountChooser *chooser,
                                       PurpleFilterAccountFunc filter_func)
{
	g_return_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser));

	chooser->filter_func = filter_func;
	regenerate_account_menu(chooser);
}

PurpleAccount *
pidgin_account_chooser_get_selected(GtkWidget *chooser)
{
	g_return_val_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser), NULL);

	return (PurpleAccount *)account_chooser_get_selected(
	        PIDGIN_ACCOUNT_CHOOSER(chooser));
}

void
pidgin_account_chooser_set_selected(GtkWidget *chooser, PurpleAccount *account)
{
	g_return_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser));

	account_chooser_select_by_data(chooser, account);

	/* NOTE: Property notification occurs in 'changed' signal callback. */
}

gboolean
pidgin_account_chooser_get_show_all(GtkWidget *chooser)
{
	g_return_val_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser), FALSE);

	return PIDGIN_ACCOUNT_CHOOSER(chooser)->show_all;
}
