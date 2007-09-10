/**
 * @file ping.h utility functions
 *
 * purple
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _PURPLE_JABBER_PING_H_
#define _PURPLE_JABBER_PING_H_

#include "jabber.h"
#include "conversation.h"

void jabber_ping_parse(JabberStream *js,
						xmlnode *packet);


gboolean jabber_ping_jid(PurpleConversation *conv, const char *jid);


#endif /* _PURPLE_JABBER_PING_H_ */
