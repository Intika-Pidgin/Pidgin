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

#ifndef PURPLE_PRESENCE_H
#define PURPLE_PRESENCE_H
/**
 * SECTION:presence
 * @section_id: libpurple-presence
 * @short_description: <filename>presence.h</filename>
 * @title: Presence Objects API
 *
 * This file contains the presence base type, account presence, and buddy
 * presence API.
 */

/**
 * PurplePresence:
 *
 * A PurplePresence is like a collection of PurpleStatuses (plus some
 * other random info).  For any buddy, or for any one of your accounts,
 * or for any person with which you're chatting, you may know various
 * amounts of information.  This information is all contained in
 * one PurplePresence.  If one of your buddies is away and idle,
 * then the presence contains the PurpleStatus for their awayness,
 * and it contains their current idle time.  PurplePresences are
 * never saved to disk.  The information they contain is only relevant
 * for the current PurpleSession.
 *
 * Note: When a presence is destroyed with the last g_object_unref(), all
 *       statuses added to this list will be destroyed along with the presence.
 */
typedef struct _PurplePresence PurplePresence;

#include "account.h"
#include "buddylist.h"
#include "status.h"

/**
 * PurplePresenceClass:
 * @update_idle: Updates the logs and the UI when the idle state or time of the
 *               presence changes.
 *
 * Base class for all #PurplePresence's
 */
struct _PurplePresenceClass {
	GObjectClass parent_class;

	void (*update_idle)(PurplePresence *presence, gboolean old_idle);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* PurplePresence API                                                     */
/**************************************************************************/

#define PURPLE_TYPE_PRESENCE  purple_presence_get_type()

/**
 * purple_presence_get_type:
 *
 * Returns: The #GType for the #PurplePresence object.
 */
G_DECLARE_DERIVABLE_TYPE(PurplePresence, purple_presence, PURPLE,
		PRESENCE, GObject)

/**
 * purple_presence_set_status_active:
 * @presence:  The presence.
 * @status_id: The ID of the status.
 * @active:    The active state.
 *
 * Sets the active state of a status in a presence.
 *
 * Only independent statuses can be set unactive. Normal statuses can only
 * be set active, so if you wish to disable a status, set another
 * non-independent status to active, or use purple_presence_switch_status().
 */
void purple_presence_set_status_active(PurplePresence *presence,
									 const char *status_id, gboolean active);

/**
 * purple_presence_switch_status:
 * @presence: The presence.
 * @status_id: The status ID to switch to.
 *
 * Switches the active status in a presence.
 *
 * This is similar to purple_presence_set_status_active(), except it won't
 * activate independent statuses.
 */
void purple_presence_switch_status(PurplePresence *presence,
								 const char *status_id);

/**
 * purple_presence_set_idle:
 * @presence:  The presence.
 * @idle:      The idle state.
 * @idle_time: The idle time, if @idle is TRUE.  This
 *                  is the time at which the user became idle,
 *                  in seconds since the epoch.  If this value is
 *                  unknown then 0 should be used.
 *
 * Sets the idle state and time on a presence.
 */
void purple_presence_set_idle(PurplePresence *presence, gboolean idle,
							time_t idle_time);

/**
 * purple_presence_set_login_time:
 * @presence:   The presence.
 * @login_time: The login time.
 *
 * Sets the login time on a presence.
 */
void purple_presence_set_login_time(PurplePresence *presence, time_t login_time);

/**
 * purple_presence_get_statuses:
 * @presence: The presence.
 *
 * Returns all the statuses in a presence.
 *
 * Returns: (element-type PurpleStatus) (transfer none): The statuses.
 */
GList *purple_presence_get_statuses(PurplePresence *presence);

/**
 * purple_presence_get_status:
 * @presence:  The presence.
 * @status_id: The ID of the status.
 *
 * Returns the status with the specified ID from a presence.
 *
 * Returns: (transfer none): The status if found, or %NULL.
 */
PurpleStatus *purple_presence_get_status(PurplePresence *presence,
									 const char *status_id);

/**
 * purple_presence_get_active_status:
 * @presence: The presence.
 *
 * Returns the active exclusive status from a presence.
 *
 * Returns: (transfer none): The active exclusive status.
 */
PurpleStatus *purple_presence_get_active_status(PurplePresence *presence);

/**
 * purple_presence_is_available:
 * @presence: The presence.
 *
 * Returns whether or not a presence is available.
 *
 * Available presences are online and possibly invisible, but not away or idle.
 *
 * Returns: TRUE if the presence is available, or FALSE otherwise.
 */
gboolean purple_presence_is_available(PurplePresence *presence);

/**
 * purple_presence_is_online:
 * @presence: The presence.
 *
 * Returns whether or not a presence is online.
 *
 * Returns: TRUE if the presence is online, or FALSE otherwise.
 */
gboolean purple_presence_is_online(PurplePresence *presence);

/**
 * purple_presence_is_status_active:
 * @presence:  The presence.
 * @status_id: The ID of the status.
 *
 * Returns whether or not a status in a presence is active.
 *
 * A status is active if itself or any of its sub-statuses are active.
 *
 * Returns: TRUE if the status is active, or FALSE.
 */
gboolean purple_presence_is_status_active(PurplePresence *presence,
										const char *status_id);

/**
 * purple_presence_is_status_primitive_active:
 * @presence:  The presence.
 * @primitive: The status primitive.
 *
 * Returns whether or not a status with the specified primitive type
 * in a presence is active.
 *
 * A status is active if itself or any of its sub-statuses are active.
 *
 * Returns: TRUE if the status is active, or FALSE.
 */
gboolean purple_presence_is_status_primitive_active(
	PurplePresence *presence, PurpleStatusPrimitive primitive);

/**
 * purple_presence_is_idle:
 * @presence: The presence.
 *
 * Returns whether or not a presence is idle.
 *
 * Returns: TRUE if the presence is idle, or FALSE otherwise.
 *         If the presence is offline (purple_presence_is_online()
 *         returns FALSE) then FALSE is returned.
 */
gboolean purple_presence_is_idle(PurplePresence *presence);

/**
 * purple_presence_get_idle_time:
 * @presence: The presence.
 *
 * Returns the presence's idle time.
 *
 * Returns: The presence's idle time.
 */
time_t purple_presence_get_idle_time(PurplePresence *presence);

/**
 * purple_presence_get_login_time:
 * @presence: The presence.
 *
 * Returns the presence's login time.
 *
 * Returns: The presence's login time.
 */
time_t purple_presence_get_login_time(PurplePresence *presence);

/**************************************************************************/
/* PurpleAccountPresence API                                              */
/**************************************************************************/

#define PURPLE_TYPE_ACCOUNT_PRESENCE (purple_account_presence_get_type())

/**
 * purple_account_presence_get_type:
 *
 * Returns: The #GType for the #PurpleAccountPresence object.
 */
G_DECLARE_FINAL_TYPE(PurpleAccountPresence, purple_account_presence,
		PURPLE, ACCOUNT_PRESENCE, PurplePresence)

/**
 * purple_account_presence_new:
 * @account: The account to associate with the presence.
 *
 * Creates a presence for an account.
 *
 * Returns: The new presence.
 *
 * Since: 3.0.0
 */
PurpleAccountPresence *purple_account_presence_new(PurpleAccount *account);

/**
 * purple_account_presence_get_account:
 * @presence: The presence.
 *
 * Returns an account presence's account.
 *
 * Returns: (transfer none): The presence's account.
 */
PurpleAccount *purple_account_presence_get_account(PurpleAccountPresence *presence);

/**************************************************************************/
/* PurpleBuddyPresence API                                                */
/**************************************************************************/

#define PURPLE_TYPE_BUDDY_PRESENCE (purple_buddy_presence_get_type())

/**
 * purple_buddy_presence_get_type:
 *
 * Returns: The #GType for the #PurpleBuddyPresence object.
 */
G_DECLARE_FINAL_TYPE(PurpleBuddyPresence, purple_buddy_presence, PURPLE,
		BUDDY_PRESENCE, PurplePresence)

/**
 * purple_buddy_presence_new:
 * @buddy: The buddy to associate with the presence.
 *
 * Creates a presence for a buddy.
 *
 * Returns: The new presence.
 *
 * Since: 3.0.0
 */
PurpleBuddyPresence *purple_buddy_presence_new(PurpleBuddy *buddy);

/**
 * purple_buddy_presence_get_buddy:
 * @presence: The presence.
 *
 * Returns the buddy presence's buddy.
 *
 * Returns: (transfer none): The presence's buddy.
 */
PurpleBuddy *purple_buddy_presence_get_buddy(PurpleBuddyPresence *presence);

/**
 * purple_buddy_presence_compare:
 * @buddy_presence1: The first presence.
 * @buddy_presence2: The second presence.
 *
 * Compares two buddy presences for availability.
 *
 * Returns: -1 if @buddy_presence1 is more available than @buddy_presence2.
 *           0 if @buddy_presence1 is equal to @buddy_presence2.
 *           1 if @buddy_presence1 is less available than @buddy_presence2.
 */
gint purple_buddy_presence_compare(PurpleBuddyPresence *buddy_presence1,
						   PurpleBuddyPresence *buddy_presence2);

G_END_DECLS

#endif /* PURPLE_PRESENCE_H */
