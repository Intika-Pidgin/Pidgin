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

#ifndef PIDGIN_INVITE_DIALOG_H
#define PIDGIN_INVITE_DIALOG_H

/**
 * SECTION:pidgininvitedialog
 * @section_id: pidgin-invite-dialog
 * @short_description: A dialog widget to invite others to chat
 * @title: Invite Dialog
 *
 * #PidginInviteDialog is a simple #GtkDialog that presents the user with an
 * interface to invite another user to a conversation.  Name completion is
 * automatically setup as well.
 *
 * |[<!-- language="C" -->
 * static void
 * do_invite(GtkWidget *widget, int resp, gpointer data) {
 *     PidginInviteDialog *dialog = PIDGIN_INVITE_DIALOG(widget);
 *
 *     if(resp == GTK_RESPONSE_ACCEPT) {
 *         g_message(
 *             "user wants to invite %s with message %s",
 *             pidgin_invite_dialog_get_contact(dialog),
 *             pidgin_invite_dialog_get_message(dialog)
 *         );
 *     }
 * }
 *
 * static void
 * do_invite(PurpleChatConversation *conv) {
 *     GtkWidget *dialog = pidgin_invite_dialog_new(conv);
 *     g_signal_connect(G_OBJECT(dialog), "response",
 *                      G_CALLBACK(do_invite), NULL);
 *
 * }
 * ]|
 */

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_INVITE_DIALOG  pidgin_invite_dialog_get_type()

G_DECLARE_FINAL_TYPE(PidginInviteDialog, pidgin_invite_dialog, PIDGIN,
		INVITE_DIALOG, GtkDialog)

/**
 * pidgin_invite_dialog_new:
 * @conversation: The #PurpleChatConversation instance.
 *
 * Creates a new #PidginInviteDialog to invite someone to @conversation.
 *
 * Returns: (transfer full): The new #PidginInviteDialog instance.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_invite_dialog_new(PurpleChatConversation *conversation);

/**
 * pidgin_invite_dialog_set_contact:
 * @dialog: The #PidginInviteDialog instance.
 * @contact: The contact to invite.
 *
 * Sets the contact that should be invited.  This function is intended to be
 * used to prepopulate the dialog in cases where you just need to prompt the
 * user for an invite message.
 *
 * Since: 3.0.0
 */
void pidgin_invite_dialog_set_contact(PidginInviteDialog *dialog, const gchar *contact);

/**
 * pidgin_invite_dialog_get_contact:
 * @dialog: #PidginInviteDialog instance.
 *
 * Gets the contact that was entered in @dialog.  This string is only valid as
 * long as @dialog exists.
 *
 * Returns: (transfer none): The contact that was entered.
 *
 * Since: 3.0.0
 */
const gchar *pidgin_invite_dialog_get_contact(PidginInviteDialog *dialog);

/**
 * pidgin_invite_dialog_set_message:
 * @dialog: The #PidginInviteDialog instance.
 * @message: The message that should be displayed.
 *
 * Sets the message to be displayed in @dialog.  The main use case is to
 * prepopulate the message.
 *
 * Since: 3.0.0
 */
void pidgin_invite_dialog_set_message(PidginInviteDialog *dialog, const gchar *message);

/**
 * pidgin_invite_dialog_get_message:
 * @dialog: The #PidginInviteDialog instance.
 *
 * Gets the message that was entered in @dialog.  The returned value is only
 * valid as long as @dialog exists.
 *
 * Returns: (transfer none): The message that was entered in @dialog.
 *
 * Since: 3.0.0
 */
const gchar *pidgin_invite_dialog_get_message(PidginInviteDialog *dialog);

/**
 * pidgin_invite_dialog_get_conversation:
 * @dialog: The #PidginInviteDialog instance.
 *
 * Gets the #PurpleChatConversation that @dialog was created for.
 *
 * Returns: (transfer none): The #PurpleChatConversation that @dialog was
 *          created with.
 *
 * Since: 3.0.0
 */
PurpleChatConversation *pidgin_invite_dialog_get_conversation(PidginInviteDialog *dialog);

G_END_DECLS

#endif /* PIDGIN_INVITE_DIALOG_H */
