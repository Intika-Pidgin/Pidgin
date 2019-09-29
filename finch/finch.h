/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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

#ifndef FINCH_H
#define FINCH_H

/**
 * SECTION:finch
 * @section_id: finch-finch
 * @short_description: <filename>finch.h</filename>
 * @title: UI Definitions and Includes
 */

#include <glib.h>

#define FINCH_UI "gnt-purple"

#define FINCH_PREFS_ROOT "/finch"

/**
 * finch_start:
 * @argc: Address of the argc parameter of your main() function (or 0 if argv
 *        is %NULL). This will be changed if any arguments were handled.
 * @argv: Address of the argv parameter of main(), or %NULL. Any options
 *        understood by Finch are stripped before return.
 *
 * Start finch with the given command line arguments.
 */
gboolean finch_start(int *argc, char ***argv);

#endif /* FINCH_H */

