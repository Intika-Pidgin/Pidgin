/**
 * @file oob.h out-of-band transfer functions
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

#ifndef PURPLE_JABBER_OOB_H
#define PURPLE_JABBER_OOB_H

#include "jabber.h"

G_BEGIN_DECLS

#define JABBER_TYPE_OOB_XFER (jabber_oob_xfer_get_type())
G_DECLARE_FINAL_TYPE(JabberOOBXfer, jabber_oob_xfer, JABBER, OOB_XFER, PurpleXfer);

void jabber_oob_parse(JabberStream *js, const char *from, JabberIqType type,
                      const char *id, PurpleXmlNode *querynode);

void jabber_oob_xfer_register(GTypeModule *module);

G_END_DECLS

#endif /* PURPLE_JABBER_OOB_H */
