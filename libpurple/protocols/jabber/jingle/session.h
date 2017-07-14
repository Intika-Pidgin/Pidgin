/**
 * @file session.h
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#ifndef PURPLE_JABBER_JINGLE_SESSION_H
#define PURPLE_JABBER_JINGLE_SESSION_H

#include "iq.h"
#include "jabber.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define JINGLE_TYPE_SESSION  jingle_session_get_type()

struct _JingleContent;

/**
 * Gets the session class's GType
 *
 * @return The session class's GType.
 */
G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE(JingleSession, jingle_session, JINGLE, SESSION, GObject)

/**
 * Registers the JingleSession type in the type system.
 */
void jingle_session_register(PurplePlugin *plugin);

JingleSession *jingle_session_create(JabberStream *js, const gchar *sid,
				     const gchar *local_jid, const gchar *remote_jid,
				     gboolean is_initiator);
JabberStream *jingle_session_get_js(JingleSession *session);
gchar *jingle_session_get_sid(JingleSession *session);
gchar *jingle_session_get_local_jid(JingleSession *session);
gchar *jingle_session_get_remote_jid(JingleSession *session);
gboolean jingle_session_is_initiator(JingleSession *session);
gboolean jingle_session_get_state(JingleSession *session);

GList *jingle_session_get_contents(JingleSession *session);
GList *jingle_session_get_pending_contents(JingleSession *session);

JingleSession *jingle_session_find_by_sid(JabberStream *js, const gchar *sid);
JingleSession *jingle_session_find_by_jid(JabberStream *js, const gchar *jid);

JabberIq *jingle_session_create_ack(JingleSession *session, const PurpleXmlNode *jingle);
PurpleXmlNode *jingle_session_to_xml(JingleSession *session, PurpleXmlNode *parent, JingleActionType action);
JabberIq *jingle_session_to_packet(JingleSession *session, JingleActionType action);

void jingle_session_handle_action(JingleSession *session, PurpleXmlNode *jingle, JingleActionType action);

struct _JingleContent *jingle_session_find_content(JingleSession *session,
					const gchar *name, const gchar *creator);
struct _JingleContent *jingle_session_find_pending_content(JingleSession *session,
					const gchar *name, const gchar *creator);

void jingle_session_add_content(JingleSession *session, struct _JingleContent* content);
void jingle_session_remove_content(JingleSession *session, const gchar *name, const gchar *creator);
void jingle_session_add_pending_content(JingleSession *session, struct _JingleContent* content);
void jingle_session_remove_pending_content(JingleSession *session, const gchar *name, const gchar *creator);
void jingle_session_accept_content(JingleSession *session, const gchar *name, const gchar *creator);
void jingle_session_accept_session(JingleSession *session);
JabberIq *jingle_session_terminate_packet(JingleSession *session, const gchar *reason);
JabberIq *jingle_session_redirect_packet(JingleSession *session, const gchar *sid);

G_END_DECLS

#endif /* PURPLE_JABBER_JINGLE_SESSION_H */

