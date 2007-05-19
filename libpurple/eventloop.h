/**
 * @file eventloop.h Purple Event Loop API
 * @ingroup core
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
 */
#ifndef _PURPLE_EVENTLOOP_H_
#define _PURPLE_EVENTLOOP_H_

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An input condition.
 */
typedef enum
{
	PURPLE_INPUT_READ  = 1 << 0,  /**< A read condition.  */
	PURPLE_INPUT_WRITE = 1 << 1   /**< A write condition. */

} PurpleInputCondition;

typedef void (*PurpleInputFunction)(gpointer, gint, PurpleInputCondition);

typedef struct _PurpleEventLoopUiOps PurpleEventLoopUiOps;

struct _PurpleEventLoopUiOps
{
	/**
	 * Creates a callback timer with an interval measured in milliseconds.
	 * @see g_timeout_add, purple_timeout_add
	 **/
	guint (*timeout_add)(guint interval, GSourceFunc function, gpointer data);

	/**
	 * Creates a callback timer with an interval measured in seconds.
	 * @see g_timeout_add_seconds, purple_timeout_add_seconds
	 **/
	guint (*timeout_add_seconds)(guint interval, GSourceFunc function, gpointer data);

	/**
	 * Removes a callback timer.
	 * @see purple_timeout_remove, g_source_remove
	 */
	gboolean (*timeout_remove)(guint handle);

	/**
	 * Adds an input handler.
	 * @see purple_input_add, g_io_add_watch_full
	 */
	guint (*input_add)(int fd, PurpleInputCondition cond,
					   PurpleInputFunction func, gpointer user_data);

	/**
	 * Removes an input handler.
	 * @see purple_input_remove, g_source_remove
	 */
	gboolean (*input_remove)(guint handle);
	
	
	/**
	 * Get the current error status for an input.
	 * Implementation of this UI op is optional. Implement it if the UI's sockets
	 * or event loop needs to customize determination of socket error status.
	 * @see purple_input_get_error, getsockopt
	 */
	int (*input_get_error)(int fd, int *error);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**************************************************************************/
/** @name Event Loop API                                                  */
/**************************************************************************/
/*@{*/
/**
 * Creates a callback timer.
 * The timer will repeat until the function returns @c FALSE. The
 * first call will be at the end of the first interval.
 * @param interval	The time between calls of the function, in
 *					milliseconds.
 * @param function	The function to call.
 * @param data		data to pass to @a function.
 * @return A handle to the timer which can be passed to 
 *         purple_timeout_remove to remove the timer.
 */
guint purple_timeout_add(guint interval, GSourceFunc function, gpointer data);

/**
 * Removes a timeout handler.
 *
 * @param handle The handle, as returned by purple_timeout_add.
 *
 * @return Something.
 */
gboolean purple_timeout_remove(guint handle);

/**
 * Adds an input handler.
 *
 * @param fd        The input file descriptor.
 * @param cond      The condition type.
 * @param func      The callback function for data.
 * @param user_data User-specified data.
 *
 * @return The resulting handle (will be greater than 0).
 * @see g_io_add_watch_full
 */
guint purple_input_add(int fd, PurpleInputCondition cond,
					 PurpleInputFunction func, gpointer user_data);

/**
 * Removes an input handler.
 *
 * @param handle The handle of the input handler. Note that this is the return
 * value from purple_input_add, <i>not</i> the file descriptor.
 */
gboolean purple_input_remove(guint handle);

/**
 * Get the current error status for an input.
 * The return value and error follow getsockopt() with a level of SOL_SOCKET and an
 * option name of SO_ERROR, and this is how the error is determined if the UI does not
 * implement the input_get_error UI op.
 *
 * @param fd        The input file descriptor.
 * @param error		A pointer to an int which on return will have the error, or 0 if no error.
 *
 * @return 0 if there is no error; -1 if there is an error, in which case errno will be set.
 */
int
purple_input_get_error(int fd, int *error);


/*@}*/


/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/
/**
 * Sets the UI operations structure to be used for accounts.
 *
 * @param ops The UI operations structure.
 */
void purple_eventloop_set_ui_ops(PurpleEventLoopUiOps *ops);

/**
 * Returns the UI operations structure used for accounts.
 *
 * @return The UI operations structure in use.
 */
PurpleEventLoopUiOps *purple_eventloop_get_ui_ops(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_EVENTLOOP_H_ */
