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

#define PIDGIN_TYPE_CELL_RENDERER_EXPANDER         (pidgin_cell_renderer_expander_get_type())
#define PIDGIN_CELL_RENDERER_EXPANDER(obj)         (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_CELL_RENDERER_EXPANDER, PidginCellRendererExpander))
#define PIDGIN_CELL_RENDERER_EXPANDER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PURPLE_TYPE_GTK_CELL_RENDERER_EXPANDER, PidginCellRendererExpanderClass))
#define PIDGIN_IS_CELL_RENDERER_EXPANDER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_CELL_RENDERER_EXPANDER))
#define PIDGIN_IS_CELL_RENDERER_EXPANDER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_CELL_RENDERER_EXPANDER))
#define PIDGIN_CELL_RENDERER_EXPANDER_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_CELL_RENDERER_EXPANDER, PidginCellRendererExpanderClass))

typedef struct _PidginCellRendererExpander PidginCellRendererExpander;
typedef struct _PidginCellRendererExpanderClass PidginCellRendererExpanderClass;

struct _PidginCellRendererExpander {
	GtkCellRenderer parent;

	gboolean is_expander;
};

struct _PidginCellRendererExpanderClass {
	GtkCellRendererClass parent_class;
};

G_BEGIN_DECLS

GType            pidgin_cell_renderer_expander_get_type     (void);
GtkCellRenderer  *pidgin_cell_renderer_expander_new          (void);

G_END_DECLS

#endif /* _PIDGINCELLRENDEREREXPANDER_H_ */
