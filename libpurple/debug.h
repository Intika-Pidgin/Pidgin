/**
 * @file debug.h Debug API
 * @ingroup core
 */

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
 */
#ifndef _PURPLE_DEBUG_H_
#define _PURPLE_DEBUG_H_

#include <glib.h>
#include <stdarg.h>

/**
 * Debug levels.
 */
typedef enum
{
	PURPLE_DEBUG_ALL = 0,  /**< All debug levels.              */
	PURPLE_DEBUG_MISC,     /**< General chatter.               */
	PURPLE_DEBUG_INFO,     /**< General operation Information. */
	PURPLE_DEBUG_WARNING,  /**< Warnings.                      */
	PURPLE_DEBUG_ERROR,    /**< Errors.                        */
	PURPLE_DEBUG_FATAL     /**< Fatal errors.                  */

} PurpleDebugLevel;

/**
 * Debug UI operations.
 */
typedef struct
{
	void (*print)(PurpleDebugLevel level, const char *category,
				  const char *arg_s);
	gboolean (*is_enabled)(PurpleDebugLevel level,
			const char *category);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleDebugUiOps;

G_BEGIN_DECLS

/**************************************************************************/
/** @name Debug API                                                       */
/**************************************************************************/
/**
 * Outputs debug information.
 *
 * @level:    The debug level.
 * @category: The category (or %NULL).
 * @format:   The format string.
 */
void purple_debug(PurpleDebugLevel level, const char *category,
				const char *format, ...) G_GNUC_PRINTF(3, 4);

/**
 * Outputs misc. level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_MISC as
 * the level.
 *
 * @category: The category (or %NULL).
 * @format:   The format string.
 *
 * @see purple_debug()
 */
void purple_debug_misc(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);

/**
 * Outputs info level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_INFO as
 * the level.
 *
 * @category: The category (or %NULL).
 * @format:   The format string.
 *
 * @see purple_debug()
 */
void purple_debug_info(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);

/**
 * Outputs warning level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_WARNING as
 * the level.
 *
 * @category: The category (or %NULL).
 * @format:   The format string.
 *
 * @see purple_debug()
 */
void purple_debug_warning(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);

/**
 * Outputs error level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_ERROR as
 * the level.
 *
 * @category: The category (or %NULL).
 * @format:   The format string.
 *
 * @see purple_debug()
 */
void purple_debug_error(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);

/**
 * Outputs fatal error level debug information.
 *
 * This is a wrapper for purple_debug(), and uses PURPLE_DEBUG_ERROR as
 * the level.
 *
 * @category: The category (or %NULL).
 * @format:   The format string.
 *
 * @see purple_debug()
 */
void purple_debug_fatal(const char *category, const char *format, ...) G_GNUC_PRINTF(2, 3);

/**
 * Enable or disable printing debug output to the console.
 *
 * @enabled: TRUE to enable debug output or FALSE to disable it.
 */
void purple_debug_set_enabled(gboolean enabled);

/**
 * Check if console debug output is enabled.
 *
 * Returns: TRUE if debugging is enabled, FALSE if it is not.
 */
gboolean purple_debug_is_enabled(void);

/**
 * Enable or disable verbose debugging.  This ordinarily should only be called
 * by #purple_debug_init, but there are cases where this can be useful for
 * plugins.
 *
 * @verbose: TRUE to enable verbose debugging or FALSE to disable it.
 */
void purple_debug_set_verbose(gboolean verbose);

/**
 * Check if verbose logging is enabled.
 *
 * Returns: TRUE if verbose debugging is enabled, FALSE if it is not.
 */
gboolean purple_debug_is_verbose(void);

/**
 * Enable or disable unsafe debugging.  This ordinarily should only be called
 * by #purple_debug_init, but there are cases where this can be useful for
 * plugins.
 *
 * @unsafe: TRUE to enable debug logging of messages that could
 *        potentially contain passwords and other sensitive information.
 *        FALSE to disable it.
 */
void purple_debug_set_unsafe(gboolean unsafe);

/**
 * Check if unsafe debugging is enabled.  Defaults to FALSE.
 *
 * Returns: TRUE if the debug logging of all messages is enabled, FALSE
 *         if messages that could potentially contain passwords and other
 *         sensitive information are not logged.
 */
gboolean purple_debug_is_unsafe(void);

/**
 * Enable or disable colored output for bash console.
 *
 * @colored: TRUE to enable colored output, FALSE to disable it.
 */
void purple_debug_set_colored(gboolean colored);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used when outputting debug
 * information.
 *
 * @ops: The UI operations structure.
 */
void purple_debug_set_ui_ops(PurpleDebugUiOps *ops);

/**
 * Returns the UI operations structure used when outputting debug
 * information.
 *
 * Returns: The UI operations structure in use.
 */
PurpleDebugUiOps *purple_debug_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Debug Subsystem                                                 */
/**************************************************************************/
/*@{*/

/**
 * Initializes the debug subsystem.
 */
void purple_debug_init(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_DEBUG_H_ */
