/**
 * @file error.h Error functions
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 */
#ifndef _MSN_ERROR_H_
#define _MSN_ERROR_H_

#include "session.h"

/**
 * Returns the string representation of an error type.
 *
 * @param type The error type.
 *
 * @return The string representation of the error type.
 */
const char *msn_error_get_text(unsigned int type);

/**
 * Handles an error.
 *
 * @param session The current session.
 * @param type    The error type.
 */
void msn_error_handle(MsnSession *session, unsigned int type);

#endif /* _MSN_ERROR_H_ */
