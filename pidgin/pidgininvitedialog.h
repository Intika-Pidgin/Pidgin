#ifndef PIDGIN_ABOUT_H
#define PIDGIN_ABOUT_H

/**
 * SECTION:pidgininvitedialog
 * @section_id: pidgin-invite-dialog
 * @short_description: <filename>pidgininvitedialog.h</filename>
 * @title: Invite Dialog
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_INVITE_DIALOG  pidgin_invite_dialog_get_type()
G_DECLARE_FINAL_TYPE(PidginInviteDialog, pidgin_invite_dialog, PIDGIN,
		INVITE_DIALOG, GtkDialog)

GtkWidget *pidgin_invite_dialog_new();

void pidgin_invite_dialog_set_contact(PidginInviteDialog *dialog, const gchar *contact);
const gchar *pidgin_invite_dialog_get_contact(PidginInviteDialog *dialog);

void pidgin_invite_dialog_set_message(PidginInviteDialog *dialog, const gchar *message);
const gchar * pidgin_invite_dialog_get_message(PidginInviteDialog *dialog);

G_END_DECLS

#endif /* PIDGIN_INVITE_H */
