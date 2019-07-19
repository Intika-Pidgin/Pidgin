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

#include "pidginmessage.h"

struct _PidginMessage {
	GObject parent;

	PurpleMessage *msg;
	GDateTime *timestamp;
};

enum {
	PROP_0,
	PROP_MESSAGE,
	N_PROPERTIES,
	/* overrides */
	PROP_ID = N_PROPERTIES,
	PROP_CONTENT_TYPE,
	PROP_AUTHOR,
	PROP_CONTENTS,
	PROP_TIMESTAMP,
	PROP_EDITED,
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_message_set_message(PidginMessage *msg, PurpleMessage *purple_msg) {
	if(g_set_object(&msg->msg, purple_msg)) {
		g_clear_pointer(&msg->timestamp, g_date_time_unref);
		msg->timestamp = g_date_time_new_from_unix_local(purple_message_get_time(purple_msg));

		g_object_freeze_notify(G_OBJECT(msg));
		g_object_notify_by_pspec(G_OBJECT(msg), properties[PROP_MESSAGE]);
		g_object_notify_by_pspec(G_OBJECT(msg), properties[PROP_TIMESTAMP]);
		g_object_thaw_notify(G_OBJECT(msg));
	}
}

/******************************************************************************
 * TalkatuMessage Implementation
 *****************************************************************************/
static void
pidgin_message_talkatu_message_init(TalkatuMessageInterface *iface) {
	/* we don't actually change behavior, we just override properties */
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE_EXTENDED(
	PidginMessage,
	pidgin_message,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE(TALKATU_TYPE_MESSAGE, pidgin_message_talkatu_message_init)
);

static void
pidgin_message_get_property(GObject *obj, guint param_id, GValue *value, GParamSpec *pspec) {
	PidginMessage *msg = PIDGIN_MESSAGE(obj);

	switch(param_id) {
		case PROP_MESSAGE:
			g_value_set_object(value, pidgin_message_get_message(msg));
			break;
		case PROP_ID:
			g_value_set_uint(value, purple_message_get_id(msg->msg));
			break;
		case PROP_CONTENT_TYPE:
			g_value_set_enum(value, TALKATU_CONTENT_TYPE_PLAIN);
			break;
		case PROP_AUTHOR:
			g_value_set_string(value, purple_message_get_author(msg->msg));
			break;
		case PROP_CONTENTS:
			g_value_set_string(value, purple_message_get_contents(msg->msg));
			break;
		case PROP_TIMESTAMP:
			g_value_set_pointer(value, msg->timestamp);
			break;
		case PROP_EDITED:
			g_value_set_boolean(value, FALSE);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_message_set_property(GObject *obj, guint param_id, const GValue *value, GParamSpec *pspec) {
	PidginMessage *msg = PIDGIN_MESSAGE(obj);

	switch(param_id) {
		case PROP_MESSAGE:
			pidgin_message_set_message(msg, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_message_init(PidginMessage *msg) {
}

static void
pidgin_message_finalize(GObject *obj) {
	PidginMessage *msg = PIDGIN_MESSAGE(obj);

	g_clear_object(&msg->msg);
	g_clear_pointer(&msg->timestamp, g_date_time_unref);

	G_OBJECT_CLASS(pidgin_message_parent_class)->finalize(obj);
}

static void
pidgin_message_class_init(PidginMessageClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = pidgin_message_get_property;
	obj_class->set_property = pidgin_message_set_property;
	obj_class->finalize = pidgin_message_finalize;

	/* add our custom properties */
	properties[PROP_MESSAGE] = g_param_spec_object(
		"message", "message", "The purple message",
		PURPLE_TYPE_MESSAGE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
	);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);

	/* add our overridden properties */
	g_object_class_override_property(obj_class, PROP_ID, "id");
	g_object_class_override_property(obj_class, PROP_TIMESTAMP, "timestamp");
	g_object_class_override_property(obj_class, PROP_CONTENT_TYPE, "content-type");
	g_object_class_override_property(obj_class, PROP_AUTHOR, "author");
	g_object_class_override_property(obj_class, PROP_CONTENTS, "contents");
	g_object_class_override_property(obj_class, PROP_EDITED, "edited");
}

/******************************************************************************
 * API
 *****************************************************************************/
PidginMessage *
pidgin_message_new(PurpleMessage *msg) {
	return g_object_new(
		PIDGIN_TYPE_MESSAGE,
		"message", msg,
		NULL
	);
}

PurpleMessage *
pidgin_message_get_message(PidginMessage *msg) {
	g_return_val_if_fail(PIDGIN_IS_MESSAGE(msg), NULL);

	return msg->msg;
}
