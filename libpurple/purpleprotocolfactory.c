/*
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

#include "purpleprotocolfactory.h"

G_DEFINE_INTERFACE(PurpleProtocolFactory, purple_protocol_factory, G_TYPE_INVALID);

/******************************************************************************
 * GInterface Implementation
 *****************************************************************************/
static void
purple_protocol_factory_default_init(PurpleProtocolFactoryInterface *iface) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleConnection *
purple_protocol_factory_connection_new(PurpleProtocolFactory *factory,
                                       PurpleAccount *account,
                                       const gchar *password)
{
	PurpleProtocolFactoryInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_FACTORY(factory), NULL);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	iface = PURPLE_PROTOCOL_FACTORY_GET_IFACE(factory);
	if(iface && iface->connection_new) {
		return iface->connection_new(factory, account, password);
	}

	return NULL;
}

PurpleRoomlist *
purple_protocol_factory_roomlist_new(PurpleProtocolFactory *factory,
                                     PurpleAccount *account)
{
	PurpleProtocolFactoryInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_FACTORY(factory), NULL);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	iface = PURPLE_PROTOCOL_FACTORY_GET_IFACE(factory);
	if(iface && iface->roomlist_new) {
		return iface->roomlist_new(factory, account);
	}

	return NULL;
}

PurpleWhiteboard *
purple_protocol_factory_whiteboard_new(PurpleProtocolFactory *factory,
                                       PurpleAccount *account,
                                       const gchar *who,
                                       gint state)
{
	PurpleProtocolFactoryInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_FACTORY(factory), NULL);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(who, NULL);

	iface = PURPLE_PROTOCOL_FACTORY_GET_IFACE(factory);
	if(iface && iface->whiteboard_new) {
		return iface->whiteboard_new(factory, account, who, state);
	}

	return NULL;
}
