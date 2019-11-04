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

#include "pidgincontactcompletion.h"

#include <purple.h>

struct _PidginContactCompletion {
	GtkEntryCompletion parent;
	PurpleAccount *account;
};

enum {
	PIDGIN_CONTACT_COMPLETION_COLUMN_CONTACT,
	PIDGIN_CONTACT_COMPLETION_COLUMN_ACCOUNT,
	PIDGIN_CONTACT_COMPLETION_N_COLUMNS,
};

enum {
	PROP_0,
	PROP_ACCOUNT,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = {0, };

G_DEFINE_TYPE(PidginContactCompletion, pidgin_contact_completion, GTK_TYPE_ENTRY_COMPLETION);

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_contact_completion_walk_contact_func(PurpleBlistNode *node, gpointer data) {
	PurpleBuddy *buddy = PURPLE_BUDDY(node);
	PurpleAccount *account = purple_buddy_get_account(buddy);
	GtkListStore *store = GTK_LIST_STORE(data);
	GtkTreeIter iter;
	const gchar *name;

	name = purple_buddy_get_name(buddy);
	if(name == NULL) {
		name = "";
	}

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(
		store,
		&iter,
		PIDGIN_CONTACT_COMPLETION_COLUMN_CONTACT, name,
		PIDGIN_CONTACT_COMPLETION_COLUMN_ACCOUNT, account,
		-1
	);
}

static GtkTreeModel *
pidgin_contact_completion_create_model(void) {
	GtkListStore *store = NULL;

	store = gtk_list_store_new(
		PIDGIN_CONTACT_COMPLETION_N_COLUMNS,
		G_TYPE_STRING,
		PURPLE_TYPE_ACCOUNT
	);

	purple_blist_walk(NULL,
	                  NULL,
	                  NULL,
	                  pidgin_contact_completion_walk_contact_func,
	                  store
	);

	return GTK_TREE_MODEL(store);
}

static gboolean
pidgin_contact_completion_match_func(GtkEntryCompletion *completion,
                                     const gchar *key,
                                     GtkTreeIter *iter,
                                     gpointer data)
{
	GtkTreeModel *model = NULL;
	PidginContactCompletion *comp = PIDGIN_CONTACT_COMPLETION(completion);
	gchar *name = NULL;

	model = gtk_entry_completion_get_model(completion);

	gtk_tree_model_get(
		model,
		iter,
		PIDGIN_CONTACT_COMPLETION_COLUMN_CONTACT, &name,
		-1
	);

	if(name == NULL) {
		return FALSE;
	}

	if (!g_str_has_prefix(name, key)) {
		g_free(name);
		return FALSE;
	}

	if(PURPLE_IS_ACCOUNT(comp->account)) {
		PurpleAccount *account = NULL;

		gtk_tree_model_get(
			model,
			iter,
			PIDGIN_CONTACT_COMPLETION_COLUMN_ACCOUNT, &account,
			-1
		);

		if(account != comp->account) {
			g_object_unref(account);
			return FALSE;
		}

		g_object_unref(account);
	}

	return TRUE;
}

/******************************************************************************
 * GObject Implemention
 *****************************************************************************/
static void
pidgin_contact_completion_init(PidginContactCompletion *comp) {
}

static void
pidgin_contact_completion_constructed(GObject *obj) {
	GtkTreeModel *model = NULL;

	G_OBJECT_CLASS(pidgin_contact_completion_parent_class)->constructed(obj);

	model = pidgin_contact_completion_create_model();

	gtk_entry_completion_set_model(
		GTK_ENTRY_COMPLETION(obj),
		model
	);

	gtk_entry_completion_set_match_func(
		GTK_ENTRY_COMPLETION(obj),
		pidgin_contact_completion_match_func,
		NULL,
		NULL
	);

	gtk_entry_completion_set_text_column(
		GTK_ENTRY_COMPLETION(obj),
		PIDGIN_CONTACT_COMPLETION_COLUMN_CONTACT
	);
}

static void
pidgin_contact_completion_get_property(GObject *obj,
                                       guint param_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	PidginContactCompletion *comp = PIDGIN_CONTACT_COMPLETION(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value, pidgin_contact_completion_get_account(comp));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_contact_completion_set_property(GObject *obj,
                                       guint param_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	PidginContactCompletion *comp = PIDGIN_CONTACT_COMPLETION(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			pidgin_contact_completion_set_account(comp,
			                                      g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_contact_completion_finalize(GObject *obj) {
	PidginContactCompletion *comp = PIDGIN_CONTACT_COMPLETION(obj);

	g_object_unref(comp->account);

	G_OBJECT_CLASS(pidgin_contact_completion_parent_class)->finalize(obj);
}

static void
pidgin_contact_completion_class_init(PidginContactCompletionClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	/* The only solution I could find to make this work was to implement the
	 * constructed handler and chain up to the parent.  If you find another,
	 * better way, please implement it :)
	 */
	obj_class->constructed = pidgin_contact_completion_constructed;

	obj_class->get_property = pidgin_contact_completion_get_property;
	obj_class->set_property = pidgin_contact_completion_set_property;
	obj_class->finalize = pidgin_contact_completion_finalize;

	properties[PROP_ACCOUNT] = g_param_spec_object(
		"account",
		"account",
		"The account to filter by or NULL for no filtering",
		PURPLE_TYPE_ACCOUNT,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT
	);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkEntryCompletion *
pidgin_contact_completion_new(void) {
	return GTK_ENTRY_COMPLETION(g_object_new(PIDGIN_TYPE_CONTACT_COMPLETION, NULL));
}

PurpleAccount *
pidgin_contact_completion_get_account(PidginContactCompletion *completion) {
	g_return_val_if_fail(PIDGIN_IS_CONTACT_COMPLETION(completion), NULL);

	return g_object_ref(completion->account);
}

void
pidgin_contact_completion_set_account(PidginContactCompletion *completion,
                                      PurpleAccount *account)
{
	g_return_if_fail(PIDGIN_IS_CONTACT_COMPLETION(completion));

	if(g_set_object(&completion->account, account)) {
		g_object_notify_by_pspec(G_OBJECT(completion),
		                         properties[PROP_ACCOUNT]);
	}
}
