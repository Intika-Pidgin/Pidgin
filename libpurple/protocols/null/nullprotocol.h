/* purple
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
 *
 */
#ifndef _NULL_H_
#define _NULL_H_

#include "protocol.h"

#define NULL_TYPE_PROTOCOL             (null_protocol_get_type())
#define NULL_PROTOCOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), NULL_TYPE_PROTOCOL, NullProtocol))
#define NULL_PROTOCOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), NULL_TYPE_PROTOCOL, NullProtocolClass))
#define NULL_IS_PROTOCOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), NULL_TYPE_PROTOCOL))
#define NULL_IS_PROTOCOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), NULL_TYPE_PROTOCOL))
#define NULL_PROTOCOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), NULL_TYPE_PROTOCOL, NullProtocolClass))

typedef struct _NullProtocol
{
	PurpleProtocol parent;
} NullProtocol;

typedef struct _NullProtocolClass
{
	PurpleProtocolClass parent_class;
} NullProtocolClass;

/**
 * Returns the GType for the NullProtocol object.
 */
G_MODULE_EXPORT GType null_protocol_get_type(void);

#endif /* _NULL_H_ */
