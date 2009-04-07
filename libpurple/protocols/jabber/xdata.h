/**
 * @file xdata.h utility functions
 *
 * purple
 *
 * Copyright (C) 2003 Nathan Walp <faceprint@faceprint.com>
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
#ifndef PURPLE_JABBER_XDATA_H_
#define PURPLE_JABBER_XDATA_H_

#include "jabber.h"
#include "xmlnode.h"

typedef struct _JabberXDataAction {
	char *name;
	char *handle;
} JabberXDataAction;

typedef void (*jabber_x_data_cb)(JabberStream *js, xmlnode *result, gpointer user_data);
typedef void (*jabber_x_data_action_cb)(JabberStream *js, xmlnode *result, const char *actionhandle, gpointer user_data);
void *jabber_x_data_request(JabberStream *js, xmlnode *packet, jabber_x_data_cb cb, gpointer user_data);
void *jabber_x_data_request_with_actions(JabberStream *js, xmlnode *packet, GList *actions, int defaultaction, jabber_x_data_action_cb cb, gpointer user_data);

#endif /* PURPLE_JABBER_XDATA_H_ */
