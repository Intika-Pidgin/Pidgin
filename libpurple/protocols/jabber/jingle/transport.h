/**
 * @file transport.h
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

#ifndef PURPLE_JABBER_JINGLE_TRANSPORT_H
#define PURPLE_JABBER_JINGLE_TRANSPORT_H

#include <glib.h>
#include <glib-object.h>

#include "jingle.h"
#include "xmlnode.h"

G_BEGIN_DECLS

#define JINGLE_TYPE_TRANSPORT  jingle_transport_get_type()

typedef struct _JingleTransport JingleTransport;

/** The transport class */
struct _JingleTransportClass
{
	GObjectClass parent_class;     /**< The parent class. */

	const gchar *transport_type;
	PurpleXmlNode *(*to_xml) (JingleTransport *transport, PurpleXmlNode *content, JingleActionType action);
	JingleTransport *(*parse) (PurpleXmlNode *transport);
	void (*add_local_candidate) (JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate);
	GList *(*get_remote_candidates) (JingleTransport *transport);
};

/**
 * Gets the transport class's GType
 *
 * @return The transport class's GType.
 */
G_MODULE_EXPORT
G_DECLARE_DERIVABLE_TYPE(JingleTransport, jingle_transport, JINGLE, TRANSPORT,
		GObject)

/**
 * Registers the JingleTransport type in the type system.
 */
void jingle_transport_register(PurplePlugin *plugin);

JingleTransport *jingle_transport_create(const gchar *type);
const gchar *jingle_transport_get_transport_type(JingleTransport *transport);

void jingle_transport_add_local_candidate(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate);
GList *jingle_transport_get_remote_candidates(JingleTransport *transport);

JingleTransport *jingle_transport_parse(PurpleXmlNode *transport);
PurpleXmlNode *jingle_transport_to_xml(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action);

G_END_DECLS

#endif /* PURPLE_JABBER_JINGLE_TRANSPORT_H */

