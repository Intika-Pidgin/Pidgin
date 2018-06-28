/**
 * @file rtp.h
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

#ifndef PURPLE_JABBER_JINGLE_RTP_H
#define PURPLE_JABBER_JINGLE_RTP_H

#include "config.h"

#ifdef USE_VV

#include <glib.h>
#include <glib-object.h>

#include "content.h"
#include "media.h"
#include "xmlnode.h"

G_BEGIN_DECLS

#define JINGLE_TYPE_RTP  jingle_rtp_get_type()

/**
 * Gets the rtp class's GType
 *
 * @return The rtp class's GType.
 */
G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE(JingleRtp, jingle_rtp, JINGLE, RTP, JingleContent)

/**
 * Registers the JingleRtp type in the type system.
 */
void jingle_rtp_register(PurplePlugin *plugin);

gchar *jingle_rtp_get_media_type(JingleContent *content);
gchar *jingle_rtp_get_ssrc(JingleContent *content);

gboolean jingle_rtp_initiate_media(JabberStream *js,
				   const gchar *who,
				   PurpleMediaSessionType type);
void jingle_rtp_terminate_session(JabberStream *js, const gchar *who);

G_END_DECLS

#endif /* USE_VV */

#endif /* PURPLE_JABBER_JINGLE_RTP_H */

