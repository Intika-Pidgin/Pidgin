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

#ifndef PIDGIN_CONTACT_COMPLETION_H
#define PIDGIN_CONTACT_COMPLETION_H

/**
 * SECTION:pidgincontactcompletion
 * @section_id: pidgin-contact-completion
 * @short_description: A GtkEntryCompletion for contacts
 * @title: Contact Name Completion
 *
 * #PidginContactCompletion should be treated like a normal
 * #GtkEntryCompletion, except it already does all of the setup for the
 * completion.  You can also filter by a #PurpleAccount to limit what's shown.
 *
 * |[<!-- language="C" -->
 * GtkWidget *entry = gtk_entry_new();
 * GtkEntryCompletion *completion = pidgin_contact_completion_new();
 *
 * gtk_entry_set_completion(GTK_ENTRY(entry), completion);
 * pidgin_contact_completion_set_account(PIDGIN_CONTACT_COMPLETION(completion), account);
 * g_object_unref(completion);
 * ]|
 */

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_CONTACT_COMPLETION  pidgin_contact_completion_get_type()

G_DECLARE_FINAL_TYPE(PidginContactCompletion, pidgin_contact_completion, PIDGIN,
		CONTACT_COMPLETION, GtkEntryCompletion)

/**
 * pidgin_contact_completion_new:
 *
 * Creates a new #GtkEntryCompletion for looking up contacts.
 *
 * Returns: (transfer full): The new #GtkEntryCompletion instance.
 */
GtkEntryCompletion *pidgin_contact_completion_new(void);

/**
 * pidgin_contact_completion_get_account:
 * @completion: The #PidginContactCompletion instance.
 *
 * Gets the account that @completion is filtering for.  If no filtering is set
 * %NULL will be returned.
 *
 * Returns: (transfer full) (nullable): The #PurpleAccount that's being
 *          filtered for.
 */
PurpleAccount *pidgin_contact_completion_get_account(PidginContactCompletion *completion);

/**
 * pidgin_contact_completion_set_account:
 * @completion: The #PidginContactCompletion instance.
 * @account: (nullable): The #PurpleAccount to filter for or %NULL.
 *
 * Set the #PurpleAccount that @completion should filter for.  If @account is
 * %NULL, all filtering will be disabled.
 */
void pidgin_contact_completion_set_account(PidginContactCompletion *completion, PurpleAccount *account);

G_END_DECLS

#endif /* PIDGIN_CONTACT_COMPLETION_H */
