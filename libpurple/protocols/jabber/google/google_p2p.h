/**
 * @file google_p2p.h
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

#ifndef PURPLE_JABBER_JINGLE_GOOGLE_P2P_H
#define PURPLE_JABBER_JINGLE_GOOGLE_P2P_H

#include <glib.h>
#include <glib-object.h>

#include "jingle/transport.h"

G_BEGIN_DECLS

#define JINGLE_TYPE_GOOGLE_P2P  jingle_google_p2p_get_type()
#define JINGLE_TYPE_GOOGLE_P2P_CANDIDATE  jingle_google_p2p_candidate_get_type()

/** @copydoc _JingleGoogleP2PCandidate */
typedef struct _JingleGoogleP2PCandidate JingleGoogleP2PCandidate;

struct _JingleGoogleP2PCandidate
{
	gchar *id;
	gchar *address;
	guint port;
	guint preference;
	gchar *type;
	gchar *protocol;
	guint network;
	gchar *username;
	gchar *password;
	guint generation;

	gboolean rem_known; /* TRUE if the remote side knows
	                     * about this candidate */
};

GType jingle_google_p2p_candidate_get_type(void);

/**
 * Gets the Google P2P class's GType
 *
 * @return The Google P2P class's GType.
 */
G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE(JingleGoogleP2P, jingle_google_p2p, JINGLE, GOOGLE_P2P,
		JingleTransport)

/**
 * Registers the JingleGoogleP2P type in the type system.
 */
void jingle_google_p2p_register(PurplePlugin *plugin);

JingleGoogleP2PCandidate *jingle_google_p2p_candidate_new(const gchar *id,
		guint generation, const gchar *address, guint port, guint preference,
		const gchar *type, const gchar *protocol,
		const gchar *username, const gchar *password);

G_END_DECLS

#endif /* PURPLE_JABBER_JINGLE_GOOGLE_P2P_H */

