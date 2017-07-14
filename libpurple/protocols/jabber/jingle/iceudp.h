/**
 * @file iceudp.h
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

#ifndef PURPLE_JABBER_JINGLE_ICEUDP_H
#define PURPLE_JABBER_JINGLE_ICEUDP_H

#include <glib.h>
#include <glib-object.h>

#include "transport.h"

G_BEGIN_DECLS

#define JINGLE_TYPE_ICEUDP  jingle_iceudp_get_type()
#define JINGLE_TYPE_ICEUDP_CANDIDATE  jingle_iceudp_candidate_get_type()

/** @copydoc _JingleIceUdpCandidate */
typedef struct _JingleIceUdpCandidate JingleIceUdpCandidate;

struct _JingleIceUdpCandidate
{
	gchar *id;
	guint component;
	gchar *foundation;
	guint generation;
	gchar *ip;
	guint network;
	guint port;
	guint priority;
	gchar *protocol;
	gchar *reladdr;
	guint relport;
	gchar *type;

	gchar *username;
	gchar *password;

	gboolean rem_known;	/* TRUE if the remote side knows
				 * about this candidate */
};

GType jingle_iceudp_candidate_get_type(void);

/**
 * Gets the iceudp class's GType
 *
 * @return The iceudp class's GType.
 */
G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE(JingleIceUdp, jingle_iceudp, JINGLE, ICEUDP,
		JingleTransport)

/**
 * Registers the JingleIceUdp type in the type system.
 */
void jingle_iceudp_register(PurplePlugin *plugin);

JingleIceUdpCandidate *jingle_iceudp_candidate_new(const gchar *id,
		guint component, const gchar *foundation, guint generation,
		const gchar *ip, guint network, guint port, guint priority,
		const gchar *protocol, const gchar *type,
		const gchar *username, const gchar *password);

G_END_DECLS

#endif /* PURPLE_JABBER_JINGLE_ICEUDP_H */

