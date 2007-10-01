/*
 * @file gtkcellrendererprogress.c GTK+ Cell Renderer Progress
 * @ingroup pidgin
 */

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
 *
 */

/* This is taken largely from GtkCellRenderer[Text|Pixbuf|Toggle] by 
 * Jonathon Blandford <jrb@redhat.com> for RedHat, Inc.
 */

#include "gtkcellrendererprogress.h"

static void pidgin_cell_renderer_progress_get_property  (GObject                    *object,
						      guint                       param_id,
						      GValue                     *value,
						      GParamSpec                 *pspec);
static void pidgin_cell_renderer_progress_set_property  (GObject                    *object,
						      guint                       param_id,
						      const GValue               *value,
						      GParamSpec                 *pspec);
static void pidgin_cell_renderer_progress_init       (PidginCellRendererProgress      *cellprogress);
static void pidgin_cell_renderer_progress_class_init (PidginCellRendererProgressClass *class);
static void pidgin_cell_renderer_progress_get_size   (GtkCellRenderer            *cell,
						   GtkWidget                  *widget,
						   GdkRectangle               *cell_area,
						   gint                       *x_offset,
						   gint                       *y_offset,
						   gint                       *width,
						   gint                       *height);
static void pidgin_cell_renderer_progress_render     (GtkCellRenderer            *cell,
						   GdkWindow                  *window,
						   GtkWidget                  *widget,
						   GdkRectangle               *background_area,
						   GdkRectangle               *cell_area,
						   GdkRectangle               *expose_area,
						   guint                       flags);
#if 0
static gboolean pidgin_cell_renderer_progress_activate  (GtkCellRenderer            *cell,
						      GdkEvent                   *event,
						      GtkWidget                  *widget,
						      const gchar                *path,
						      GdkRectangle               *background_area,
						      GdkRectangle               *cell_area,
						      guint                       flags);
#endif
static void  pidgin_cell_renderer_progress_finalize (GObject *gobject);

enum {
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_PERCENTAGE,
	PROP_TEXT,
	PROP_SHOW_TEXT
};
     
static gpointer parent_class;
/* static guint progress_cell_renderer_signals [LAST_SIGNAL]; */

GType  pidgin_cell_renderer_progress_get_type (void)
{
	static GType cell_progress_type = 0;
	
	if (!cell_progress_type)
		{
			static const GTypeInfo cell_progress_info =
				{
					sizeof (PidginCellRendererProgressClass),
					NULL,           /* base_init */
					NULL,           /* base_finalize */
					(GClassInitFunc) pidgin_cell_renderer_progress_class_init,
					NULL,           /* class_finalize */
					NULL,           /* class_data */
					sizeof (PidginCellRendererProgress),
					0,              /* n_preallocs */
					(GInstanceInitFunc) pidgin_cell_renderer_progress_init,
					NULL		/* value_table */
				};
			
			cell_progress_type =
				g_type_register_static (GTK_TYPE_CELL_RENDERER,
										"PidginCellRendererProgress",
										&cell_progress_info, 0);
		}
	
	return cell_progress_type;
}

static void pidgin_cell_renderer_progress_init (PidginCellRendererProgress *cellprogress)
{
	GTK_CELL_RENDERER(cellprogress)->mode = GTK_CELL_RENDERER_MODE_INERT;
	GTK_CELL_RENDERER(cellprogress)->xpad = 2;
	GTK_CELL_RENDERER(cellprogress)->ypad = 2;
}

static void pidgin_cell_renderer_progress_class_init (PidginCellRendererProgressClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);
	
	parent_class = g_type_class_peek_parent (class);
	object_class->finalize = pidgin_cell_renderer_progress_finalize;

	object_class->get_property = pidgin_cell_renderer_progress_get_property;
	object_class->set_property = pidgin_cell_renderer_progress_set_property;
	
	cell_class->get_size = pidgin_cell_renderer_progress_get_size;
	cell_class->render   = pidgin_cell_renderer_progress_render;
	
	g_object_class_install_property (object_class,
					 PROP_PERCENTAGE,
					 g_param_spec_double ("percentage",
							      "Percentage",
							      "The fractional progress to display",
							      0, 1, 0,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_TEXT,
					 g_param_spec_string ("text",
							      "Text",
							      "Text to overlay over progress bar",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property(object_class,
					PROP_SHOW_TEXT,
					g_param_spec_string("text_set",
							    "Text set",
							    "Whether to overlay text on the progress bar",
							    FALSE,
							    G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void pidgin_cell_renderer_progress_finalize (GObject *object)
{
/*
	PidginCellRendererProgress *cellprogress = PIDGIN_CELL_RENDERER_PROGRESS(object);
*/

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void pidgin_cell_renderer_progress_get_property (GObject    *object,
						     guint      param_id,
						     GValue     *value,
						     GParamSpec *psec)
{
	PidginCellRendererProgress *cellprogress = PIDGIN_CELL_RENDERER_PROGRESS(object);

	switch (param_id)
		{
		case PROP_PERCENTAGE:
			g_value_set_double(value, cellprogress->progress);
			break;
		case PROP_TEXT:
			g_value_set_string(value, cellprogress->text);
			break;
		case PROP_SHOW_TEXT:
			g_value_set_boolean(value, cellprogress->text_set);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, psec);
			break;
		}
}

static void pidgin_cell_renderer_progress_set_property (GObject      *object,
						     guint        param_id,
						     const GValue *value,
						     GParamSpec   *pspec)
{
	PidginCellRendererProgress *cellprogress = PIDGIN_CELL_RENDERER_PROGRESS (object);

	switch (param_id)
		{
		case PROP_PERCENTAGE:
			cellprogress->progress = g_value_get_double(value);
			break;
		case PROP_TEXT:
			if (cellprogress->text)
				g_free(cellprogress->text);
			cellprogress->text = g_strdup(g_value_get_string(value));
			g_object_notify(object, "text");
			break;
		case PROP_SHOW_TEXT:
			cellprogress->text_set = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
			break;
		}
}

GtkCellRenderer *pidgin_cell_renderer_progress_new(void)
{
	return g_object_new(PIDGIN_TYPE_GTK_CELL_RENDERER_PROGRESS, NULL);
}

static void pidgin_cell_renderer_progress_get_size (GtkCellRenderer *cell,
						 GtkWidget       *widget,
						 GdkRectangle    *cell_area,
						 gint            *x_offset,
						 gint            *y_offset,
						 gint            *width,
						 gint            *height)
{
	gint calc_width;
	gint calc_height;
	
	calc_width = (gint) cell->xpad * 2 + 50;
	calc_height = (gint) cell->ypad * 2 + 12;
	
	if (width)
		*width = calc_width;
	
	if (height)
		*height = calc_height;
	
	if (cell_area)
		{
			if (x_offset)
				{
					*x_offset = cell->xalign * (cell_area->width - calc_width);
					*x_offset = MAX (*x_offset, 0);
				}
			if (y_offset)
				{
					*y_offset = cell->yalign * (cell_area->height - calc_height);
					*y_offset = MAX (*y_offset, 0);
				}
		}
}


static void pidgin_cell_renderer_progress_render (GtkCellRenderer *cell,
					       GdkWindow       *window,
					       GtkWidget       *widget,
					       GdkRectangle    *background_area,
					       GdkRectangle    *cell_area,
					       GdkRectangle    *expose_area,
					       guint            flags)
{
	PidginCellRendererProgress *cellprogress = (PidginCellRendererProgress *) cell;
	
	gint width, height;
	GtkStateType state;

	width = cell_area->width;
	height = cell_area->height;
	
	if (GTK_WIDGET_HAS_FOCUS (widget))
		state = GTK_STATE_ACTIVE;
	else
		state = GTK_STATE_NORMAL;

	width -= cell->xpad*2;
	height -= cell->ypad*2;

	gtk_paint_box (widget->style,
		       window,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN, 
		       NULL, widget, "trough",
		       cell_area->x + cell->xpad,
		       cell_area->y + cell->ypad,
		       width - 1, height - 1);
	gtk_paint_box (widget->style,
		       window,
		       state, GTK_SHADOW_OUT,
		       NULL, widget, "bar",
		       cell_area->x + cell->xpad + 1,
		       cell_area->y + cell->ypad + 1,
		       (width - 3) * cellprogress->progress,
		       height - 3);
}
