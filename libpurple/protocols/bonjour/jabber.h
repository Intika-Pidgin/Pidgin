/**
 * @file jabber.h The Purple interface to mDNS and peer to peer Jabber.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _BONJOUR_JABBER_H_
#define _BONJOUR_JABBER_H_

#include "account.h"

typedef struct _BonjourJabber
{
	gint port;
	gint socket;
	gint watcher_id;
	PurpleAccount* account;
} BonjourJabber;

typedef struct _BonjourJabberConversation
{
	gint socket;
	gint watcher_id;
	gchar* buddy_name;
	gboolean stream_started;
} BonjourJabberConversation;

/**
 * Start listening for jabber connections.
 *
 * @return -1 if there was a problem, else returns the listening
 *         port number.
 */
gint bonjour_jabber_start(BonjourJabber *data);

int bonjour_jabber_send_message(BonjourJabber *data, const gchar *to, const gchar *body);

void bonjour_jabber_close_conversation(PurpleBuddy *gb);

void bonjour_jabber_stop(BonjourJabber *data);

#endif /* _BONJOUR_JABBER_H_ */
