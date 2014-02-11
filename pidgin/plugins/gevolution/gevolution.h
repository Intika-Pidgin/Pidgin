/*
 * Evolution integration plugin for Purple
 *
 * Copyright (C) 2003 Christian Hammond.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#ifndef _GEVOLUTION_H_
#define _GEVOLUTION_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libebook/libebook.h>

enum
{
	ADDRBOOK_COLUMN_NAME,
	ADDRBOOK_COLUMN_UID,
	NUM_ADDRBOOK_COLUMNS
};

typedef struct
{
	GtkListStore *sources;
	EBook *active_book;
	GList *contacts;

} GevoAddrbooksSelector;

typedef struct
{
	PurpleAccount *account;
	char *username;

	EBook *book;

	GtkWidget *win;
	GtkWidget *treeview;
	GtkWidget *addrbooks_combo;
	GtkWidget *search_field;
	GtkWidget *group_combo;
	GtkWidget *select_button;
	GtkWidget *account_optmenu;
	GtkListStore *model;

	GtkTreeModel *addrbooks;
	GList *contacts;

} GevoAddBuddyDialog;

typedef struct
{
	gboolean person_only;

	PurpleAccount *account;
	PurpleBuddy *buddy;

	EBook *book;
	EContact *contact;

	GtkWidget *win;
	GtkWidget *accounts_menu;
	GtkWidget *username;
	GtkWidget *firstname;
	GtkWidget *lastname;
	GtkWidget *email;
	GtkWidget *group_combo;
	GtkWidget *add_button;

	char *buddy_icon;

} GevoNewPersonDialog;

typedef struct
{
	PurpleBuddy *buddy;

	EBook *book;

	GtkWidget *win;
	GtkWidget *treeview;
	GtkWidget *addrbooks_combo;
	GtkWidget *search_field;
	GtkWidget *assoc_button;
	GtkListStore *model;

	GtkTreeModel *addrbooks;
	GList *contacts;

} GevoAssociateBuddyDialog;

void gevo_add_buddy_dialog_show(PurpleAccount *account, const char *username,
								const char *group, const char *alias);
void gevo_add_buddy_dialog_add_person(GevoAddBuddyDialog *dialog,
									  EContact *contact,
									  const char *name, PurpleAccount *account,
									  const char *screenname);

void gevo_new_person_dialog_show(EBook *book, EContact *contact,
								 PurpleAccount *account, const char *username,
								 const char *group, PurpleBuddy *buddy,
								 gboolean person_only);

void gevo_add_buddy(PurpleAccount *account, const char *group_name,
					const char *screenname, const char *alias);
GList *gevo_get_groups(void);

EContactField gevo_protocol_get_field(PurpleAccount *account, PurpleBuddy *buddy);
gboolean gevo_protocol_is_supported(PurpleAccount *account, PurpleBuddy *buddy);
gboolean gevo_load_addressbook(const gchar *uid, EBook **book, GError **error);
gboolean gevo_load_addressbook_from_source(ESource *source, EBook **book,
	GError **error);
char *gevo_get_email_for_buddy(PurpleBuddy *buddy);

GevoAssociateBuddyDialog *gevo_associate_buddy_dialog_new(PurpleBuddy *buddy);

GtkTreeModel *gevo_addrbooks_model_new(void);
void gevo_addrbooks_model_unref(GtkTreeModel *model);
void gevo_addrbooks_model_populate(GtkTreeModel *model);
EContact *gevo_search_buddy_in_contacts(PurpleBuddy *buddy, EBookQuery *query);

#endif /* _GEVOLUTION_H_ */
