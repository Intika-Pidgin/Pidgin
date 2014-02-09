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

#ifndef _GNT_MENUUTIL_H
#define _GNT_MENUUTIL_H
/**
 * SECTION:gntmenuutil
 * @section_id: finch-gntmenuutil
 * @short_description: <filename>gntmenuutil.h</filename>
 * @title: Menu Utility functions
 */

#include <gnt.h>
#include <gntmenu.h>

/***************************************************************************
 * GNT Menu Utility Functions
 ***************************************************************************/

/**
 * finch_append_menu_action:
 * @menu:   the GntMenu to add to
 * @action: the PurpleMenuAction to add
 * @ctx:    the callback context, passed as the first argument to
 *               the PurpleMenuAction's PurpleCallback function.
 *
 * Add a PurpleMenuAction to a GntMenu.
 */
void finch_append_menu_action(GntMenu *menu, PurpleMenuAction *action, gpointer ctx);

#endif
