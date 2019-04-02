/*
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
 *
 */

#ifndef _PIDGINCELLRENDEREREXPANDER_H_
#define _PIDGINCELLRENDEREREXPANDER_H_
/**
 * SECTION:gtkcellrendererexpander
 * @section_id: pidgin-gtkcellrendererexpander
 * @short_description: <filename>gtkcellrendererexpander.h</filename>
 * @title: Cell Renderer Expander
 */

#include <gtk/gtk.h>

#define PIDGIN_TYPE_CELL_RENDERER_EXPANDER  pidgin_cell_renderer_expander_get_type()

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(PidginCellRendererExpander, pidgin_cell_renderer_expander,
		PIDGIN, CELL_RENDERER_EXPANDER, GtkCellRenderer)
GtkCellRenderer  *pidgin_cell_renderer_expander_new          (void);

G_END_DECLS

#endif /* _PIDGINCELLRENDEREREXPANDER_H_ */
