/*
 * purple - Bonjour Protocol Plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA
 */
#ifndef PURPLE_BONJOUR_BONJOUR_FT_H
#define PURPLE_BONJOUR_BONJOUR_FT_H

#include <purple.h>

G_BEGIN_DECLS

#define XEP_TYPE_XFER (xep_xfer_get_type())
G_DECLARE_FINAL_TYPE(XepXfer, xep_xfer, XEP, XFER, PurpleXfer);

typedef enum {
	XEP_BYTESTREAMS = 1,
	XEP_IBB = 2,
	XEP_UNKNOWN = 4
} XepSiMode;

/**
 * Create a new PurpleXfer
 *
 * @param gc The PurpleConnection handle.
 * @param who Who will we be sending it to?
 */
PurpleXfer *bonjour_new_xfer(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *who);

/**
 * Send a file.
 *
 * @param gc The PurpleConnection handle.
 * @param who Who are we sending it to?
 * @param file What file? If NULL, user will choose after this call.
 */
void bonjour_send_file(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *who, const char *file);

void xep_si_parse(PurpleConnection *pc, PurpleXmlNode *packet, PurpleBuddy *pb);
void xep_bytestreams_parse(PurpleConnection *pc, PurpleXmlNode *packet, PurpleBuddy *pb);

void xep_xfer_register(GTypeModule *module);

G_END_DECLS

#endif /* PURPLE_BONJOUR_BONJOUR_FT_H */
