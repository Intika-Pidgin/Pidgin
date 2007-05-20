/**
 * @file pounce.h Buddy Pounce API
 * @ingroup core
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
#ifndef _GAIM_POUNCE_H_
#define _GAIM_POUNCE_H_

typedef struct _GaimPounce GaimPounce;

#include <glib.h>
#include "account.h"

/**
 * Events that trigger buddy pounces.
 */
typedef enum
{
	GAIM_POUNCE_NONE             = 0x000, /**< No events.                    */
	GAIM_POUNCE_SIGNON           = 0x001, /**< The buddy signed on.          */
	GAIM_POUNCE_SIGNOFF          = 0x002, /**< The buddy signed off.         */
	GAIM_POUNCE_AWAY             = 0x004, /**< The buddy went away.          */
	GAIM_POUNCE_AWAY_RETURN      = 0x008, /**< The buddy returned from away. */
	GAIM_POUNCE_IDLE             = 0x010, /**< The buddy became idle.        */
	GAIM_POUNCE_IDLE_RETURN      = 0x020, /**< The buddy is no longer idle.  */
	GAIM_POUNCE_TYPING           = 0x040, /**< The buddy started typing.     */
	GAIM_POUNCE_TYPED            = 0x080, /**< The buddy has entered text.   */
	GAIM_POUNCE_TYPING_STOPPED   = 0x100, /**< The buddy stopped typing.     */
	GAIM_POUNCE_MESSAGE_RECEIVED = 0x200  /**< The buddy sent a message      */

} GaimPounceEvent;

typedef enum
{
	GAIM_POUNCE_OPTION_NONE		= 0x00, /**< No Option                */
	GAIM_POUNCE_OPTION_AWAY		= 0x01  /**< Pounce only when away    */
} GaimPounceOption;

/** A pounce callback. */
typedef void (*GaimPounceCb)(GaimPounce *, GaimPounceEvent, void *);

/**
 * A buddy pounce structure.
 *
 * Buddy pounces are actions triggered by a buddy-related event. For
 * example, a sound can be played or an IM window opened when a buddy
 * signs on or returns from away. Such responses are handled in the
 * UI. The events themselves are done in the core.
 */
struct _GaimPounce
{
	char *ui_type;                /**< The type of UI.            */

	GaimPounceEvent events;       /**< The event(s) to pounce on. */
	GaimPounceOption options;     /**< The pounce options         */
	GaimAccount *pouncer;         /**< The user who is pouncing.  */

	char *pouncee;                /**< The buddy to pounce on.    */

	GHashTable *actions;          /**< The registered actions.    */

	gboolean save;                /**< Whether or not the pounce should
	                                   be saved after activation. */
	void *data;                   /**< Pounce-specific data.      */
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Buddy Pounce API                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a new buddy pounce.
 *
 * @param ui_type The type of UI the pounce is for.
 * @param pouncer The account that will pounce.
 * @param pouncee The buddy to pounce on.
 * @param event   The event(s) to pounce on.
 * @param option  Pounce options.
 *
 * @return The new buddy pounce structure.
 */
GaimPounce *gaim_pounce_new(const char *ui_type, GaimAccount *pouncer,
							const char *pouncee, GaimPounceEvent event,
							GaimPounceOption option);

/**
 * Destroys a buddy pounce.
 *
 * @param pounce The buddy pounce.
 */
void gaim_pounce_destroy(GaimPounce *pounce);

/**
 * Destroys all buddy pounces for the account
 *
 * @param account The account to remove all pounces from.
 */
void gaim_pounce_destroy_all_by_account(GaimAccount *account);

/**
 * Sets the events a pounce should watch for.
 *
 * @param pounce The buddy pounce.
 * @param events The events to watch for.
 */
void gaim_pounce_set_events(GaimPounce *pounce, GaimPounceEvent events);

/**
 * Sets the options for a pounce.
 *
 * @param pounce  The buddy pounce.
 * @param options The options for the pounce.
 */
void gaim_pounce_set_options(GaimPounce *pounce, GaimPounceOption options);

/**
 * Sets the account that will do the pouncing.
 *
 * @param pounce  The buddy pounce.
 * @param pouncer The account that will pounce.
 */
void gaim_pounce_set_pouncer(GaimPounce *pounce, GaimAccount *pouncer);

/**
 * Sets the buddy a pounce should pounce on.
 *
 * @param pounce  The buddy pounce.
 * @param pouncee The buddy to pounce on.
 */
void gaim_pounce_set_pouncee(GaimPounce *pounce, const char *pouncee);

/**
 * Sets whether or not the pounce should be saved after execution.
 *
 * @param pounce The buddy pounce.
 * @param save   @c TRUE if the pounce should be saved, or @c FALSE otherwise.
 */
void gaim_pounce_set_save(GaimPounce *pounce, gboolean save);

/**
 * Registers an action type for the pounce.
 *
 * @param pounce The buddy pounce.
 * @param name   The action name.
 */
void gaim_pounce_action_register(GaimPounce *pounce, const char *name);

/**
 * Enables or disables an action for a pounce.
 *
 * @param pounce  The buddy pounce.
 * @param action  The name of the action.
 * @param enabled The enabled state.
 */
void gaim_pounce_action_set_enabled(GaimPounce *pounce, const char *action,
									gboolean enabled);

/**
 * Sets a value for an attribute in an action.
 *
 * If @a value is @c NULL, the value will be unset.
 *
 * @param pounce The buddy pounce.
 * @param action The action name.
 * @param attr   The attribute name.
 * @param value  The value.
 */
void gaim_pounce_action_set_attribute(GaimPounce *pounce, const char *action,
									  const char *attr, const char *value);

/**
 * Sets the pounce-specific data.
 *
 * @param pounce The buddy pounce.
 * @param data   Data specific to the pounce.
 */
void gaim_pounce_set_data(GaimPounce *pounce, void *data);

/**
 * Returns the events a pounce should watch for.
 *
 * @param pounce The buddy pounce.
 *
 * @return The events the pounce is watching for.
 */
GaimPounceEvent gaim_pounce_get_events(const GaimPounce *pounce);

/**
 * Returns the options for a pounce.
 *
 * @param pounce The buddy pounce.
 *
 * @return The options for the pounce.
 */
GaimPounceOption gaim_pounce_get_options(const GaimPounce *pounce);

/**
 * Returns the account that will do the pouncing.
 *
 * @param pounce The buddy pounce.
 *
 * @return The account that will pounce.
 */
GaimAccount *gaim_pounce_get_pouncer(const GaimPounce *pounce);

/**
 * Returns the buddy a pounce should pounce on.
 *
 * @param pounce The buddy pounce.
 *
 * @return The buddy to pounce on.
 */
const char *gaim_pounce_get_pouncee(const GaimPounce *pounce);

/**
 * Returns whether or not the pounce should save after execution.
 *
 * @param pounce The buddy pounce.
 *
 * @return @c TRUE if the pounce should be saved after execution, or
 *         @c FALSE otherwise.
 */
gboolean gaim_pounce_get_save(const GaimPounce *pounce);

/**
 * Returns whether or not an action is enabled.
 *
 * @param pounce The buddy pounce.
 * @param action The action name.
 *
 * @return @c TRUE if the action is enabled, or @c FALSE otherwise.
 */
gboolean gaim_pounce_action_is_enabled(const GaimPounce *pounce,
									   const char *action);

/**
 * Returns the value for an attribute in an action.
 *
 * @param pounce The buddy pounce.
 * @param action The action name.
 * @param attr   The attribute name.
 *
 * @return The attribute value, if it exists, or @c NULL.
 */
const char *gaim_pounce_action_get_attribute(const GaimPounce *pounce,
											 const char *action,
											 const char *attr);

/**
 * Returns the pounce-specific data.
 *
 * @param pounce The buddy pounce.
 *
 * @return The data specific to a buddy pounce.
 */
void *gaim_pounce_get_data(const GaimPounce *pounce);

/**
 * Executes a pounce with the specified pouncer, pouncee, and event type.
 *
 * @param pouncer The account that will do the pouncing.
 * @param pouncee The buddy that is being pounced.
 * @param events  The events that triggered the pounce.
 */
void gaim_pounce_execute(const GaimAccount *pouncer, const char *pouncee,
						 GaimPounceEvent events);

/*@}*/

/**************************************************************************/
/** @name Buddy Pounce Subsystem API                                      */
/**************************************************************************/
/*@{*/

/**
 * Finds a pounce with the specified event(s) and buddy.
 *
 * @param pouncer The account to match against.
 * @param pouncee The buddy to match against.
 * @param events  The event(s) to match against.
 *
 * @return The pounce if found, or @c NULL otherwise.
 */
GaimPounce *gaim_find_pounce(const GaimAccount *pouncer,
							 const char *pouncee, GaimPounceEvent events);


/**
 * Loads the pounces.
 *
 * @return @c TRUE if the pounces could be loaded.
 */
gboolean gaim_pounces_load(void);

/**
 * Registers a pounce handler for a UI.
 *
 * @param ui          The UI name.
 * @param cb          The callback function.
 * @param new_pounce  The function called when a pounce is created.
 * @param free_pounce The function called when a pounce is freed.
 */
void gaim_pounces_register_handler(const char *ui, GaimPounceCb cb,
								   void (*new_pounce)(GaimPounce *pounce),
								   void (*free_pounce)(GaimPounce *pounce));

/**
 * Unregisters a pounce handle for a UI.
 *
 * @param ui The UI name.
 */
void gaim_pounces_unregister_handler(const char *ui);

/**
 * Returns a list of all registered buddy pounces.
 *
 * @return The list of buddy pounces.
 */
GList *gaim_pounces_get_all(void);

/**
 * Returns the buddy pounce subsystem handle.
 *
 * @return The subsystem handle.
 */
void *gaim_pounces_get_handle(void);

/**
 * Initializes the pounces subsystem.
 */
void gaim_pounces_init(void);

/**
 * Uninitializes the pounces subsystem.
 */
void gaim_pounces_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_POUNCE_H_ */
