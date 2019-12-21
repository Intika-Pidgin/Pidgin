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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "pidgin.h"

#include "connection.h"
#include "debug.h"
#include "request.h"
#include "util.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkprivacy.h"
#include "gtkutils.h"
#include "pidginaccountchooser.h"

#define PIDGIN_TYPE_PRIVACY_DIALOG (pidgin_privacy_dialog_get_type())
G_DECLARE_FINAL_TYPE(PidginPrivacyDialog, pidgin_privacy_dialog, PIDGIN,
                     PRIVACY_DIALOG, GtkDialog)

struct _PidginPrivacyDialog {
	GtkDialog parent;

	GtkWidget *type_menu;

	GtkWidget *add_button;
	GtkWidget *remove_button;
	GtkWidget *removeall_button;
	GtkWidget *close_button;

	GtkWidget *button_box;
	GtkWidget *allow_widget;
	GtkWidget *block_widget;

	GtkListStore *allow_store;
	GtkListStore *block_store;

	GtkWidget *allow_list;
	GtkWidget *block_list;

	gboolean in_allow_list;

	GtkWidget *account_chooser;
	PurpleAccount *account;
};

typedef struct
{
	PurpleAccount *account;
	char *name;
	gboolean block;

} PidginPrivacyRequestData;

static struct
{
	const char *text;
	PurpleAccountPrivacyType type;

} const menu_entries[] =
{
	{ N_("Allow all users to contact me"),         PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL },
	{ N_("Allow only the users on my buddy list"), PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST },
	{ N_("Allow only the users below"),            PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS },
	{ N_("Block all users"),                       PURPLE_ACCOUNT_PRIVACY_DENY_ALL },
	{ N_("Block only the users below"),            PURPLE_ACCOUNT_PRIVACY_DENY_USERS }
};

static const size_t menu_entry_count = sizeof(menu_entries) / sizeof(*menu_entries);

static PidginPrivacyDialog *privacy_dialog = NULL;

static void
rebuild_allow_list(PidginPrivacyDialog *dialog)
{
	GSList *l;
	GtkTreeIter iter;

	gtk_list_store_clear(dialog->allow_store);

	for (l = purple_account_privacy_get_permitted(dialog->account); l != NULL; l = l->next) {
		gtk_list_store_append(dialog->allow_store, &iter);
		gtk_list_store_set(dialog->allow_store, &iter, 0, l->data, -1);
	}
}

static void
rebuild_block_list(PidginPrivacyDialog *dialog)
{
	GSList *l;
	GtkTreeIter iter;

	gtk_list_store_clear(dialog->block_store);

	for (l = purple_account_privacy_get_denied(dialog->account); l != NULL; l = l->next) {
		gtk_list_store_append(dialog->block_store, &iter);
		gtk_list_store_set(dialog->block_store, &iter, 0, l->data, -1);
	}
}

static void
user_selected_cb(GtkTreeSelection *sel, PidginPrivacyDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->remove_button, TRUE);
}

static void
select_account_cb(GtkWidget *chooser, PidginPrivacyDialog *dialog)
{
	PurpleAccount *account = pidgin_account_chooser_get_selected(chooser);
	gsize i;

	dialog->account = account;

	if (account) {
		gtk_widget_set_sensitive(dialog->type_menu, TRUE);
		gtk_widget_set_sensitive(dialog->add_button, TRUE);
		/* dialog->remove_button is enabled when a user is selected. */
		gtk_widget_set_sensitive(dialog->removeall_button, TRUE);
	} else {
		gtk_widget_set_sensitive(dialog->type_menu, FALSE);
		gtk_widget_set_sensitive(dialog->add_button, FALSE);
		gtk_widget_set_sensitive(dialog->remove_button, FALSE);
		gtk_widget_set_sensitive(dialog->removeall_button, FALSE);
		gtk_list_store_clear(dialog->allow_store);
		gtk_list_store_clear(dialog->block_store);
		return;
	}

	for (i = 0; i < menu_entry_count; i++) {
		if (menu_entries[i].type == purple_account_get_privacy_type(account)) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->type_menu), i);
			break;
		}
	}

	rebuild_allow_list(dialog);
	rebuild_block_list(dialog);
}

/*
 * TODO: Setting the permit/deny setting needs to go through privacy.c
 *       Even better: the privacy API needs to not suck.
 */
static void
type_changed_cb(GtkComboBox *combo, PidginPrivacyDialog *dialog)
{
	PurpleAccountPrivacyType new_type =
		menu_entries[gtk_combo_box_get_active(combo)].type;

	purple_account_set_privacy_type(dialog->account, new_type);
	purple_serv_set_permit_deny(purple_account_get_connection(dialog->account));

	gtk_widget_hide(dialog->allow_widget);
	gtk_widget_hide(dialog->block_widget);
	gtk_widget_hide(dialog->button_box);

	if (new_type == PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS) {
		gtk_widget_show(dialog->allow_widget);
		gtk_widget_show_all(dialog->button_box);
		dialog->in_allow_list = TRUE;
	}
	else if (new_type == PURPLE_ACCOUNT_PRIVACY_DENY_USERS) {
		gtk_widget_show(dialog->block_widget);
		gtk_widget_show_all(dialog->button_box);
		dialog->in_allow_list = FALSE;
	}

	gtk_widget_show_all(dialog->close_button);
	gtk_widget_show(dialog->button_box);

	purple_blist_schedule_save();
	pidgin_blist_refresh(purple_blist_get_default());
}

static void
add_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	if (dialog->in_allow_list)
		pidgin_request_add_permit(dialog->account, NULL);
	else
		pidgin_request_add_block(dialog->account, NULL);
}

static void
remove_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	char *name;

	if (dialog->in_allow_list && dialog->allow_store == NULL)
		return;

	if (!dialog->in_allow_list && dialog->block_store == NULL)
		return;

	if (dialog->in_allow_list) {
		model = GTK_TREE_MODEL(dialog->allow_store);
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->allow_list));
	}
	else {
		model = GTK_TREE_MODEL(dialog->block_store);
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->block_list));
	}

	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
		gtk_tree_model_get(model, &iter, 0, &name, -1);
	else
		return;

	if (dialog->in_allow_list)
		purple_account_privacy_permit_remove(dialog->account, name, FALSE);
	else
		purple_account_privacy_deny_remove(dialog->account, name, FALSE);

	g_free(name);
}

static void
removeall_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	GSList *l;
	if (dialog->in_allow_list)
		l = purple_account_privacy_get_permitted(dialog->account);
	else
		l = purple_account_privacy_get_denied(dialog->account);
	while (l) {
		char *user;
		user = l->data;
		l = l->next;
		if (dialog->in_allow_list)
			purple_account_privacy_permit_remove(dialog->account, user, FALSE);
		else
			purple_account_privacy_deny_remove(dialog->account, user, FALSE);
	}
}

G_DEFINE_TYPE(PidginPrivacyDialog, pidgin_privacy_dialog, GTK_TYPE_DIALOG)

static void
pidgin_privacy_dialog_class_init(PidginPrivacyDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin/Privacy/dialog.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     account_chooser);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     add_button);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     allow_list);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     allow_store);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     allow_widget);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     block_list);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     block_store);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     block_widget);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     button_box);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     close_button);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     remove_button);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     removeall_button);
	gtk_widget_class_bind_template_child(widget_class, PidginPrivacyDialog,
	                                     type_menu);

	gtk_widget_class_bind_template_callback(widget_class, add_cb);
	gtk_widget_class_bind_template_callback(widget_class, remove_cb);
	gtk_widget_class_bind_template_callback(widget_class, removeall_cb);
	gtk_widget_class_bind_template_callback(widget_class, select_account_cb);
	gtk_widget_class_bind_template_callback(widget_class, type_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class, user_selected_cb);
}

static void
pidgin_privacy_dialog_init(PidginPrivacyDialog *dialog)
{
	gssize selected = -1;
	gsize i;

	gtk_widget_init_template(GTK_WIDGET(dialog));

	dialog->account =
	        pidgin_account_chooser_get_selected(dialog->account_chooser);

	/* Add the drop-down list with the allow/block types. */
	for (i = 0; i < menu_entry_count; i++) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->type_menu),
		                          _(menu_entries[i].text));

		if (menu_entries[i].type == purple_account_get_privacy_type(dialog->account))
			selected = (gssize)i;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->type_menu), selected);

	/* Rebuild the allow and block lists views. */
	rebuild_allow_list(dialog);
	rebuild_block_list(dialog);

	type_changed_cb(GTK_COMBO_BOX(dialog->type_menu), dialog);
#if 0
	if (purple_account_get_privacy_type(dialog->account) == PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS) {
		gtk_widget_show(dialog->allow_widget);
		gtk_widget_show(dialog->button_box);
		dialog->in_allow_list = TRUE;
	}
	else if (purple_account_get_privacy_type(dialog->account) == PURPLE_ACCOUNT_PRIVACY_DENY_USERS) {
		gtk_widget_show(dialog->block_widget);
		gtk_widget_show(dialog->button_box);
		dialog->in_allow_list = FALSE;
	}
#endif
}

void
pidgin_privacy_dialog_show(void)
{
	g_return_if_fail(purple_connections_get_all() != NULL);

	if (privacy_dialog == NULL) {
		privacy_dialog = g_object_new(PIDGIN_TYPE_PRIVACY_DIALOG, NULL);
		g_signal_connect(privacy_dialog, "destroy",
		                 G_CALLBACK(gtk_widget_destroyed), &privacy_dialog);
	}

	gtk_widget_show(GTK_WIDGET(privacy_dialog));
	gdk_window_raise(gtk_widget_get_window(GTK_WIDGET(privacy_dialog)));
}

void
pidgin_privacy_dialog_hide(void)
{
	gtk_widget_destroy(GTK_WIDGET(privacy_dialog));
}

static void
destroy_request_data(PidginPrivacyRequestData *data)
{
	g_free(data->name);
	g_free(data);
}

static void
confirm_permit_block_cb(PidginPrivacyRequestData *data, int option)
{
	if (data->block)
		purple_account_privacy_deny(data->account, data->name);
	else
		purple_account_privacy_allow(data->account, data->name);

	destroy_request_data(data);
}

static void
add_permit_block_cb(PidginPrivacyRequestData *data, const char *name)
{
	data->name = g_strdup(name);

	confirm_permit_block_cb(data, 0);
}

void
pidgin_request_add_permit(PurpleAccount *account, const char *name)
{
	PidginPrivacyRequestData *data;

	g_return_if_fail(account != NULL);

	data = g_new0(PidginPrivacyRequestData, 1);
	data->account = account;
	data->name    = g_strdup(name);
	data->block   = FALSE;

	if (name == NULL) {
		purple_request_input(account, _("Permit User"),
			_("Type a user you permit to contact you."),
			_("Please enter the name of the user you wish to be "
			  "able to contact you."),
			NULL, FALSE, FALSE, NULL,
			_("_Permit"), G_CALLBACK(add_permit_block_cb),
			_("Cancel"), G_CALLBACK(destroy_request_data),
			purple_request_cpar_from_account(account),
			data);
	}
	else {
		char *primary = g_strdup_printf(_("Allow %s to contact you?"), name);
		char *secondary =
			g_strdup_printf(_("Are you sure you wish to allow "
							  "%s to contact you?"), name);


		purple_request_action(account, _("Permit User"), primary,
			secondary, 0, purple_request_cpar_from_account(account),
			data, 2,
			_("_Permit"), G_CALLBACK(confirm_permit_block_cb),
			_("Cancel"), G_CALLBACK(destroy_request_data));

		g_free(primary);
		g_free(secondary);
	}
}

void
pidgin_request_add_block(PurpleAccount *account, const char *name)
{
	PidginPrivacyRequestData *data;

	g_return_if_fail(account != NULL);

	data = g_new0(PidginPrivacyRequestData, 1);
	data->account = account;
	data->name    = g_strdup(name);
	data->block   = TRUE;

	if (name == NULL) {
		purple_request_input(account, _("Block User"),
			_("Type a user to block."),
			_("Please enter the name of the user you wish to block."),
			NULL, FALSE, FALSE, NULL,
			_("_Block"), G_CALLBACK(add_permit_block_cb),
			_("Cancel"), G_CALLBACK(destroy_request_data),
			purple_request_cpar_from_account(account),
			data);
	}
	else {
		char *primary = g_strdup_printf(_("Block %s?"), name);
		char *secondary =
			g_strdup_printf(_("Are you sure you want to block %s?"), name);

		purple_request_action(account, _("Block User"), primary,
			secondary, 0, purple_request_cpar_from_account(account),
			data, 2,
			_("_Block"), G_CALLBACK(confirm_permit_block_cb),
			_("Cancel"), G_CALLBACK(destroy_request_data));

		g_free(primary);
		g_free(secondary);
	}
}

static void
pidgin_permit_added_removed(PurpleAccount *account, const char *name)
{
	if (privacy_dialog != NULL)
		rebuild_allow_list(privacy_dialog);
}

static void
pidgin_deny_added_removed(PurpleAccount *account, const char *name)
{
	if (privacy_dialog != NULL)
		rebuild_block_list(privacy_dialog);
}

void
pidgin_privacy_init(void)
{
	PurpleAccountUiOps *ops = pidgin_accounts_get_ui_ops();

	ops->permit_added = ops->permit_removed = pidgin_permit_added_removed;
	ops->deny_added = ops->deny_removed = pidgin_deny_added_removed;
}
