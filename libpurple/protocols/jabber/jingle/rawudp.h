/**
 * @file rawudp.h
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

#ifndef PURPLE_JABBER_JINGLE_RAWUDP_H
#define PURPLE_JABBER_JINGLE_RAWUDP_H

#include <glib.h>
#include <glib-object.h>

#include "transport.h"

G_BEGIN_DECLS

#define JINGLE_TYPE_RAWUDP  jingle_rawudp_get_type()
#define JINGLE_TYPE_RAWUDP_CANDIDATE  jingle_rawudp_candidate_get_type()

/** @copydoc _JingleRawUdpCandidate */
typedef struct _JingleRawUdpCandidate JingleRawUdpCandidate;

struct _JingleRawUdpCandidate
{
	guint generation;
	guint component;
	gchar *id;
	gchar *ip;
	guint port;

	gboolean rem_known;	/* TRUE if the remote side knows
				 * about this candidate */
};

GType jingle_rawudp_candidate_get_type(void);

/**
 * Gets the rawudp class's GType
 *
 * @return The rawudp class's GType.
 */
G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE(JingleRawUdp, jingle_rawudp, JINGLE, RAWUDP,
		JingleTransport)

/**
 * Registers the JingleRawUdp type in the type system.
 */
void jingle_rawudp_register(PurplePlugin *plugin);

JingleRawUdpCandidate *jingle_rawudp_candidate_new(const gchar *id,
		guint generation, guint component, const gchar *ip, guint port);

G_END_DECLS

#endif /* PURPLE_JABBER_JINGLE_RAWUDP_H */

