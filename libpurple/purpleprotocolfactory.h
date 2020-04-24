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

#ifndef PURPLE_PROTOCOL_FACTORY_H
#define PURPLE_PROTOCOL_FACTORY_H

/**
 * SECTION:protocol-factory
 * @section_id: purple-protocol-factory
 * @title: ProtocolFactoryInterface
 * @short_description: <filename>purpleprotocolfactory.h</filename>
 *
 * A interface where protocols can expose subclasses of libpurple objects.
 */

#include <glib.h>
#include <glib-object.h>

#include <libpurple/account.h>
#include <libpurple/connection.h>
#include <libpurple/protocol.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_PROTOCOL_FACTORY (purple_protocol_factory_iface_get_type())
G_DECLARE_INTERFACE(PurpleProtocolFactory, purple_protocol_factory, PURPLE,
                    PROTOCOL_FACTORY, GObject)

/**
 * PurpleProtocolFactory:
 *
 * #PurpleProtocolFactory is an opaque representation of an object that
 * implements #PurpleProtocolFactoryInterface.
 */

/**
 * PurpleProtocolFactoryInterface:
 * @connection_new: Creates a new protocol-specific connection object that
 *                  subclasses #PurpleConnection.
 * @roomlist_new:   Creates a new protocol-specific room list object that
 *                  subclasses #PurpleRoomlist.
 * @whiteboard_new: Creates a new protocol-specific whiteboard object that
 *                  subclasses #PurpleWhiteboard.
 *
 * The protocol factory interface.
 *
 * This interface provides callbacks for construction of protocol-specific
 * subclasses of some purple objects.
 */
struct _PurpleProtocolFactoryInterface {
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	PurpleConnection *(*connection_new)(PurpleProtocolFactory *factory,
	                                    PurpleAccount *account,
	                                    const gchar *password);

	PurpleRoomlist *(*roomlist_new)(PurpleProtocolFactory *factory,
	                                PurpleAccount *account);

	PurpleWhiteboard *(*whiteboard_new)(PurpleProtocolFactory *factory,
	                                    PurpleAccount *account,
	                                    const gchar *who,
	                                    gint state);
};

/**
 * purple_protocol_factory_get_type:
 *
 * The standard `_get_type` function for #PurpleProtocolFactory.
 *
 * Returns: The #GType for the protocol factory interface.
 *
 * Since: 3.0.0
 */

/**
 * purple_protocol_factory_connection_new:
 * @factory: The #PurpleProtocolFactory instance.
 * @account: The #PurpleAccount to create the connection for.
 * @password: The password for @account.
 *
 * Creates a new protocol-specific #PurpleConnection subclass.
 *
 * Returns: (transfer full): The new #PurpleConnection subclass.
 *
 * Since: 3.0.0
 */
PurpleConnection *purple_protocol_factory_connection_new(PurpleProtocolFactory *factory,
		PurpleAccount *account, const gchar *password);

/**
 * purple_protocol_factory_roomlist_new:
 * @factory: The #PurpleProtocolFactory instance.
 * @account: The #PurpleAccount to create a roomlist for.
 *
 * Creates a new protocol-specific #PurpleRoomlist subclass.
 *
 * Returns: (transfer full): The new #PurpleRoomlist subclass for @account.
 *
 * Since: 3.0.0
 */
PurpleRoomlist *purple_protocol_factory_roomlist_new(PurpleProtocolFactory *factory,
		PurpleAccount *account);

/**
 * purple_protocol_factory_whiteboard_new:
 * @factory: The #PurpleProtocolFactory instance.
 * @account: The #PurpleAccount instance to create a whiteboard for.
 * @who: The name of the contact to create a whiteboard with.
 * @state: NFI.
 *
 * Creates a new protocol-specific #PurpleWhiteboard subclass.
 *
 * Returns: (transfer full): The new #PurpleWhiteboard subclass for @account
 *          and @who.
 *
 * Since: 3.0.0
 */
PurpleWhiteboard *purple_protocol_factory_whiteboard_new(PurpleProtocolFactory *factory,
		PurpleAccount *account, const gchar *who, gint state);

G_END_DECLS

#endif /* PURPLE_PROTOCOL_FACTORY_H */
