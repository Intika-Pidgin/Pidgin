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

#include "pidgininvitedialog.h"

struct _PidginInviteDialog {
	GtkDialog parent;
};

typedef struct {
	gchar *contact;
	gchar *message;
} PidginInviteDialogPrivate;

enum {
	PROP_ZERO,
	PROP_CONTACT,
	PROP_MESSAGE,
	N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = {};

G_DEFINE_TYPE_WITH_PRIVATE(PidginInviteDialog, pidgin_invite_dialog, GTK_TYPE_DIALOG);

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
static void
pidgin_invite_dialog_get_property(GObject *obj,
                                  guint param_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	PidginInviteDialog *dialog = PIDGIN_INVITE_DIALOG(obj);

	switch(param_id) {
		case PROP_CONTACT:
			g_value_set_string(value,
			                   pidgin_invite_dialog_get_contact(dialog));
			break;
		case PROP_MESSAGE:
			g_value_set_string(value,
			                   pidgin_invite_dialog_get_message(dialog));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_invite_dialog_set_property(GObject *obj,
                                  guint param_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	PidginInviteDialog *dialog = PIDGIN_INVITE_DIALOG(obj);

	switch(param_id) {
		case PROP_CONTACT:
			pidgin_invite_dialog_set_contact(dialog,
			                                 g_value_get_string(value));
			break;
		case PROP_MESSAGE:
			pidgin_invite_dialog_set_message(dialog,
			                                 g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_invite_dialog_finalize(GObject *obj) {
	PidginInviteDialogPrivate *priv = NULL;

	priv = pidgin_invite_dialog_get_instance_private(PIDGIN_INVITE_DIALOG(obj));

	g_clear_pointer(&priv->contact, g_free);
	g_clear_pointer(&priv->message, g_free);

	G_OBJECT_CLASS(pidgin_invite_dialog_parent_class)->finalize(obj);
}

static void
pidgin_invite_dialog_init(PidginInviteDialog *dialog) {
	gtk_widget_init_template(GTK_WIDGET(dialog));
}

static void
pidgin_invite_dialog_class_init(PidginInviteDialogClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->get_property = pidgin_invite_dialog_get_property;
	obj_class->set_property = pidgin_invite_dialog_set_property;
	obj_class->finalize = pidgin_invite_dialog_finalize;

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/Conversations/invite_dialog.ui"
	);

	properties[PROP_CONTACT] = g_param_spec_string(
		"contact",
		"contact",
		"The person that is being invited",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	properties[PROP_MESSAGE] = g_param_spec_string(
		"message",
		"message",
		"The invite message to send",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
const gchar *
pidgin_invite_dialog_get_contact(PidginInviteDialog *dialog) {
	PidginInviteDialogPrivate *priv = NULL;

	g_return_val_if_fail(PIDGIN_IS_INVITE_DIALOG(dialog), NULL);

	priv = pidgin_invite_dialog_get_instance_private(dialog);

	return priv->contact;
}

void
pidgin_invite_dialog_set_contact(PidginInviteDialog *dialog,
                                 const gchar *contact)
{
	PidginInviteDialogPrivate *priv = NULL;

	g_return_if_fail(PIDGIN_IS_INVITE_DIALOG(dialog));

	priv = pidgin_invite_dialog_get_instance_private(dialog);

	g_clear_pointer(&priv->contact, g_free);

	if(contact != NULL) {
		priv->contact = g_strdup(contact);
	}

	g_object_notify_by_pspec(G_OBJECT(dialog), properties[PROP_CONTACT]);
}

const gchar *
pidgin_invite_dialog_get_message(PidginInviteDialog *dialog) {
	PidginInviteDialogPrivate *priv = NULL;

	g_return_val_if_fail(PIDGIN_IS_INVITE_DIALOG(dialog), NULL);

	priv = pidgin_invite_dialog_get_instance_private(dialog);

	return priv->message;
}

void
pidgin_invite_dialog_set_message(PidginInviteDialog *dialog,
                                 const gchar *message)
{
	PidginInviteDialogPrivate *priv = NULL;

	g_return_if_fail(PIDGIN_IS_INVITE_DIALOG(dialog));

	priv = pidgin_invite_dialog_get_instance_private(dialog);

	g_clear_pointer(&priv->message, g_free);

	if(message != NULL) {
		priv->message = g_strdup(message);
	}

	g_object_notify_by_pspec(G_OBJECT(dialog), properties[PROP_MESSAGE]);
}

