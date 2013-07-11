/*
 * nmuserrecord.h
 *
 * Copyright (c) 2004 Novell, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA	02111-1301	USA
 *
 */

#ifndef __NM_USER_RECORD_H__
#define __NM_USER_RECORD_H__

#include <glib.h>

typedef struct _NMUserRecord NMUserRecord;
typedef struct _NMProperty NMProperty;

#include "nmfield.h"
#include "nmuser.h"

/**
 * Creates an NMUserRecord
 *
 * The NMUserRecord should be released by calling
 * nm_release_user_record
 *
 * @return 			The new user record
 *
 */
NMUserRecord *nm_create_user_record(void);

/**
 * Creates an NMUserRecord
 *
 * The NMUserRecord should be released by calling
 * nm_release_user_record
 *
 * @param	details	Should be a NM_A_FA_USER_DETAILS
 *
 *
 * @return 			The new user record
 *
 */
NMUserRecord *nm_create_user_record_from_fields(NMField * details);

/**
 * Add a reference to an existing user_record
 *
 * The reference should be released by calling
 * nm_release_user_record
 *
 * @param	user_record	The contact to addref
 *
 */
void nm_user_record_add_ref(NMUserRecord * user_record);

/**
 * Release a reference to the user record
 *
 * @param	user_record		The user record
 *
 */
void nm_release_user_record(NMUserRecord * user_record);

/**
 * Set the status for the user record
 *
 * @param	user_record		The user record
 * @param 	status			The status for the user
 * @param	text			The status text for the user
 *
 */
void nm_user_record_set_status(NMUserRecord * user_record, NMSTATUS_T status,
							   const char *text);

/**
 * Get the status for the user record
 *
 * @param	user_record 	The user record
 *
 * @return	The status for the user record
 */
NMSTATUS_T nm_user_record_get_status(NMUserRecord * user_record);

/**
 * Get the status text for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The status text if there is any, NULL otherwise
 *
 */
const char *nm_user_record_get_status_text(NMUserRecord * user_record);

/**
 * Set the DN for the user record
 *
 * @param	user_record		The user record
 * @param	dn				The new DN for the user record
 *
 */
void nm_user_record_set_dn(NMUserRecord * user_record, const char *dn);

/**
 * Get the DN for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The DN for the user record
 */
const char *nm_user_record_get_dn(NMUserRecord * user_record);

/**
 * Set the user id for the
 *
 * @param	user_record		The user record
 * @param	userid			The userid (CN) for the user record
 *
 */
void nm_user_record_set_userid(NMUserRecord * user_record, const char *userid);

/**
 * Get the user id for the user record
 *
 * @param	user_record	The user record
 *
 * @return	The user id for the user record
 */
const char *nm_user_record_get_userid(NMUserRecord * user_record);

/**
 * Set the display id for the user record
 *
 * @param	user_record		The user record
 * @param	display_id		The new display id for the user
 *
 */
void nm_user_record_set_display_id(NMUserRecord * user_record,
								   const char *display_id);

/**
 * Get the display id for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The display id for the user record
 */
const char *nm_user_record_get_display_id(NMUserRecord * user_record);

/**
 * Return whether or not the display id is an auth attribute or not.
 *
 * @param	user_record		The user record
 *
 * @return	TRUE if display_id is an auth attribute, FALSE otherwise.
 */
gboolean
nm_user_record_get_auth_attr(NMUserRecord *user_record);

/**
 * Get the full name for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The full name for the user
 */
const char *nm_user_record_get_full_name(NMUserRecord * user_record);

/**
 * Get the first name for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The first name for the user
 */
const char *nm_user_record_get_first_name(NMUserRecord * user_record);

/**
 * Get the last name for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The last name for the user
 */
const char *nm_user_record_get_last_name(NMUserRecord * user_record);

/**
 * Set the user defined data for the user record
 *
 * @param	user_record		The user record
 * @param	data			The user defined data for the user record
 *
 */
void nm_user_record_set_data(NMUserRecord * user_record, gpointer data);

/**
 * Get the user defined data for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The user defined data for the user record
 */
gpointer nm_user_record_get_data(NMUserRecord * user_record);

/**
 * Get the property count for the user record
 *
 * @param	user_record		The user record
 *
 * @return	The number of information properties for the user record
 *
 */
int nm_user_record_get_property_count(NMUserRecord * user_record);

/**
 * Get an info property for the user record. The property must be released
 * by calling nm_release_property()
 *
 * @param	user_record		The user record
 * @param	index			The index of the property to get (zero based)
 *
 * @return	The property
 */
NMProperty *nm_user_record_get_property(NMUserRecord * user_record, int index);

/**
 * Release a property object
 *
 * @param	property	The property
 *
 */
void nm_release_property(NMProperty * property);

/**
 * Get the tag for the property
 *
 * @param 	property	The property
 *
 * @return 	The tag of the property (i.e. "Email Address")
 */
const char *nm_property_get_tag(NMProperty * property);

/**
 * Get the value for the property
 *
 * @param	property	The property
 *
 * @return	The value of the property (i.e. "nobody@nowhere.com")
 */
const char *nm_property_get_value(NMProperty * property);

/**
 * Copy a user record (deep copy). The dest user record must have already been
 * created (nm_create_user_record)
 *
 * @param	dest	The destination of the copy
 * @param	src		The source of the copy
 *
 */
void nm_user_record_copy(NMUserRecord * dest, NMUserRecord * src);

#endif
