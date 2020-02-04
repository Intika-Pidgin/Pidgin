/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#ifndef PIDGIN_LOG_H
#define PIDGIN_LOG_H
/**
 * SECTION:pidginlog
 * @section_id: pidgin-log
 * @short_description: <filename>pidginlog.h</filename>
 * @title: Log Viewer
 * @see_also: <link linkend="chapter-signals-gtklog">Log signals</link>
 */

#include "pidgin.h"
#include "log.h"

#include "account.h"

G_BEGIN_DECLS

void pidgin_log_show(PurpleLogType type, const char *buddyname, PurpleAccount *account);
void pidgin_log_show_contact(PurpleContact *contact);

void pidgin_syslog_show(void);

/**************************************************************************/
/* Pidgin Log Subsystem                                                   */
/**************************************************************************/

/**
 * pidgin_log_init:
 *
 * Initializes the Pidgin log subsystem.
 */
void pidgin_log_init(void);

/**
 * pidgin_log_get_handle:
 *
 * Returns the Pidgin log subsystem handle.
 *
 * Returns: The Pidgin log subsystem handle.
 */
void *pidgin_log_get_handle(void);

/**
 * pidgin_log_uninit:
 *
 * Uninitializes the Pidgin log subsystem.
 */
void pidgin_log_uninit(void);

G_END_DECLS

#endif /* PIDGIN_LOG_H */
