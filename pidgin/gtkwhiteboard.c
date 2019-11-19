/*
 * pidgin
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

#include "internal.h"
#include "buddylist.h"
#include "debug.h"
#include "pidgin.h"
#include "whiteboard.h"

#include "gtkwhiteboard.h"
#include "gtkutils.h"

typedef enum {
	PIDGIN_WHITEBOARD_BRUSH_UP,
	PIDGIN_WHITEBOARD_BRUSH_DOWN,
	PIDGIN_WHITEBOARD_BRUSH_MOTION
} PidginWhiteboardBrushState;

#define PIDGIN_TYPE_WHITEBOARD (pidgin_whiteboard_get_type())
G_DECLARE_FINAL_TYPE(PidginWhiteboard, pidgin_whiteboard, PIDGIN, WHITEBOARD,
                     GtkWindow)

/**
 * PidginWhiteboard:
 * @cr:           Cairo context for drawing
 * @surface:      Cairo surface for drawing
 * @wb:           Backend data for this whiteboard
 * @drawing_area: Drawing area
 * @color_button: A color chooser widget
 * @width:        Canvas width
 * @height:       Canvas height
 * @brush_color:  Foreground color
 * @brush_size:   Brush size
 * @brush_state:  The @PidginWhiteboardBrushState state of the brush
 *
 * A PidginWhiteboard
 */
struct _PidginWhiteboard
{
	GtkWindow parent;

	cairo_t *cr;
	cairo_surface_t *surface;

	PurpleWhiteboard *wb;

	GtkWidget *drawing_area;
	GtkWidget *color_button;

	int width;
	int height;
	int brush_color;
	int brush_size;
	PidginWhiteboardBrushState brush_state;

	/* Tracks last position of the mouse when drawing */
	gint last_x;
	gint last_y;
	/* Tracks how many brush motions made */
	gint motion_count;
};

G_DEFINE_TYPE(PidginWhiteboard, pidgin_whiteboard, GTK_TYPE_WINDOW)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_whiteboard_rgb24_to_rgba(int color_rgb, GdkRGBA *color)
{
	color->red = ((color_rgb >> 16) & 0xFF) / 255.0f;
	color->green = ((color_rgb >> 8) & 0xFF) / 255.0f;
	color->blue = (color_rgb & 0xFF) / 255.0f;
	color->alpha = 1.0;
}

static gboolean
whiteboard_close_cb(GtkWidget *widget, GdkEvent *event,
                    G_GNUC_UNUSED gpointer data)
{
	PidginWhiteboard *gtkwb = PIDGIN_WHITEBOARD(widget);

	g_clear_object(&gtkwb->wb);

	return FALSE;
}

static gboolean pidgin_whiteboard_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;
	cairo_t *cr;
	GtkAllocation allocation;
	GdkRGBA white = {1.0, 1.0, 1.0, 1.0};

	if (gtkwb->cr) {
		cairo_destroy(gtkwb->cr);
	}
	if (gtkwb->surface) {
		cairo_surface_destroy(gtkwb->surface);
	}

	gtk_widget_get_allocation(widget, &allocation);

	gtkwb->surface = cairo_image_surface_create(
	        CAIRO_FORMAT_RGB24, allocation.width, allocation.height);
	gtkwb->cr = cr = cairo_create(gtkwb->surface);
	gdk_cairo_set_source_rgba(cr, &white);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	return TRUE;
}

static gboolean
pidgin_whiteboard_draw_event(GtkWidget *widget, cairo_t *cr,
	gpointer _gtkwb)
{
	PidginWhiteboard *gtkwb = _gtkwb;

	cairo_set_source_surface(cr, gtkwb->surface, 0, 0);
	cairo_paint(cr);

	return FALSE;
}

static void
pidgin_whiteboard_set_canvas_as_icon(PidginWhiteboard *gtkwb)
{
	GdkPixbuf *pixbuf;

	/* Makes an icon from the whiteboard's canvas 'image' */
	pixbuf = gdk_pixbuf_get_from_surface(gtkwb->surface, 0, 0, gtkwb->width,
	                                     gtkwb->height);
	gtk_window_set_icon(GTK_WINDOW(gtkwb), pixbuf);
	g_object_unref(pixbuf);
}

static void
pidgin_whiteboard_draw_brush_point(PurpleWhiteboard *wb, int x, int y,
                                   int color, int size)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);
	GtkWidget *widget = gtkwb->drawing_area;
	cairo_t *gfx_con = gtkwb->cr;
	GdkRGBA rgba;

	/* Interpret and convert color */
	pidgin_whiteboard_rgb24_to_rgba(color, &rgba);
	gdk_cairo_set_source_rgba(gfx_con, &rgba);

	/* Draw a circle */
	cairo_arc(gfx_con, x, y, size / 2.0, 0.0, 2.0 * M_PI);
	cairo_fill(gfx_con);

	gtk_widget_queue_draw_area(widget, x - size / 2, y - size / 2, size,
	                           size);
}
/* Uses Bresenham's algorithm (as provided by Wikipedia) */
static void
pidgin_whiteboard_draw_brush_line(PurpleWhiteboard *wb, int x0, int y0, int x1,
                                  int y1, int color, int size)
{
	int temp;

	int xstep;
	int ystep;

	int dx;
	int dy;

	int error;
	int derror;

	int x;
	int y;

	gboolean steep = abs(y1 - y0) > abs(x1 - x0);

	if (steep) {
		temp = x0;
		x0 = y0;
		y0 = temp;
		temp = x1;
		x1 = y1;
		y1 = temp;
	}

	dx = abs(x1 - x0);
	dy = abs(y1 - y0);

	error = 0;
	derror = dy;

	x = x0;
	y = y0;

	if (x0 < x1) {
		xstep = 1;
	} else {
		xstep = -1;
	}

	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	}

	if (steep) {
		pidgin_whiteboard_draw_brush_point(wb, y, x, color, size);
	} else {
		pidgin_whiteboard_draw_brush_point(wb, x, y, color, size);
	}

	while (x != x1) {
		x += xstep;
		error += derror;

		if ((error * 2) >= dx) {
			y += ystep;
			error -= dx;
		}

		if (steep) {
			pidgin_whiteboard_draw_brush_point(wb, y, x, color,
			                                   size);
		} else {
			pidgin_whiteboard_draw_brush_point(wb, x, y, color,
			                                   size);
		}
	}
}

static gboolean pidgin_whiteboard_brush_down(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = purple_whiteboard_get_draw_list(wb);

	if (gtkwb->brush_state != PIDGIN_WHITEBOARD_BRUSH_UP) {
		/* Potential double-click DOWN to DOWN? */
		gtkwb->brush_state = PIDGIN_WHITEBOARD_BRUSH_DOWN;

		/* return FALSE; */
	}

	gtkwb->brush_state = PIDGIN_WHITEBOARD_BRUSH_DOWN;

	if (event->button == GDK_BUTTON_PRIMARY && gtkwb->cr != NULL) {
		/* Check if draw_list has contents; if so, clear it */
		if(draw_list)
		{
			purple_whiteboard_draw_list_destroy(draw_list);
			draw_list = NULL;
		}

		/* Set tracking variables */
		gtkwb->last_x = event->x;
		gtkwb->last_y = event->y;

		gtkwb->motion_count = 0;

		draw_list = g_list_append(draw_list,
		                          GINT_TO_POINTER(gtkwb->last_x));
		draw_list = g_list_append(draw_list,
		                          GINT_TO_POINTER(gtkwb->last_y));

		pidgin_whiteboard_draw_brush_point(gtkwb->wb,
											 event->x, event->y,
											 gtkwb->brush_color, gtkwb->brush_size);
	}

	purple_whiteboard_set_draw_list(wb, draw_list);

	return TRUE;
}

static gboolean pidgin_whiteboard_brush_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	int x;
	int y;
	int dx;
	int dy;

	GdkModifierType state;

	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = purple_whiteboard_get_draw_list(wb);

	if(event->is_hint)
		gdk_window_get_device_position(event->window, event->device, &x, &y,
		                               &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if (state & GDK_BUTTON1_MASK && gtkwb->cr != NULL) {
		if ((gtkwb->brush_state != PIDGIN_WHITEBOARD_BRUSH_DOWN) &&
		    (gtkwb->brush_state != PIDGIN_WHITEBOARD_BRUSH_MOTION)) {
			purple_debug_error(
			        "gtkwhiteboard",
			        "***Bad brush state transition %d to MOTION\n",
			        gtkwb->brush_state);

			gtkwb->brush_state = PIDGIN_WHITEBOARD_BRUSH_MOTION;

			return FALSE;
		}
		gtkwb->brush_state = PIDGIN_WHITEBOARD_BRUSH_MOTION;

		dx = x - gtkwb->last_x;
		dy = y - gtkwb->last_y;

		gtkwb->motion_count++;

		/* NOTE 100 is a temporary constant for how many deltas/motions in a
		 * stroke (needs UI Ops?)
		 */
		if (gtkwb->motion_count == 100) {
			draw_list = g_list_append(draw_list, GINT_TO_POINTER(dx));
			draw_list = g_list_append(draw_list, GINT_TO_POINTER(dy));

			/* Send draw list to the draw_list handler */
			purple_whiteboard_send_draw_list(gtkwb->wb, draw_list);

			/* The brush stroke is finished, clear the list for another one */
			if(draw_list)
			{
				purple_whiteboard_draw_list_destroy(draw_list);
				draw_list = NULL;
			}

			/* Reset motion tracking */
			gtkwb->motion_count = 0;

			draw_list = g_list_append(
			        draw_list, GINT_TO_POINTER(gtkwb->last_x));
			draw_list = g_list_append(
			        draw_list, GINT_TO_POINTER(gtkwb->last_y));

			dx = x - gtkwb->last_x;
			dy = y - gtkwb->last_y;
		}

		draw_list = g_list_append(draw_list, GINT_TO_POINTER(dx));
		draw_list = g_list_append(draw_list, GINT_TO_POINTER(dy));

		pidgin_whiteboard_draw_brush_line(
		        gtkwb->wb, gtkwb->last_x, gtkwb->last_y, x, y,
		        gtkwb->brush_color, gtkwb->brush_size);

		/* Set tracking variables */
		gtkwb->last_x = x;
		gtkwb->last_y = y;
	}

	purple_whiteboard_set_draw_list(wb, draw_list);

	return TRUE;
}

static gboolean pidgin_whiteboard_brush_up(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)data;

	PurpleWhiteboard *wb = gtkwb->wb;
	GList *draw_list = purple_whiteboard_get_draw_list(wb);

	if ((gtkwb->brush_state != PIDGIN_WHITEBOARD_BRUSH_DOWN) &&
	    (gtkwb->brush_state != PIDGIN_WHITEBOARD_BRUSH_MOTION)) {
		purple_debug_error("gtkwhiteboard",
		                   "***Bad brush state transition %d to UP\n",
		                   gtkwb->brush_state);

		gtkwb->brush_state = PIDGIN_WHITEBOARD_BRUSH_UP;

		return FALSE;
	}
	gtkwb->brush_state = PIDGIN_WHITEBOARD_BRUSH_UP;

	if (event->button == GDK_BUTTON_PRIMARY && gtkwb->cr != NULL) {
		/* If the brush was never moved, express two sets of two deltas That's a
		 * 'point,' but not for Yahoo!
		 */
		if (gtkwb->motion_count == 0) {
			int index;

			/* For Yahoo!, a (0 0) indicates the end of drawing */
			/* FIXME: Yahoo Doodle specific! */
			for (index = 0; index < 2; index++) {
				draw_list = g_list_append(draw_list, 0);
				draw_list = g_list_append(draw_list, 0);
			}
		}

		/* Send draw list to protocol draw_list handler */
		purple_whiteboard_send_draw_list(gtkwb->wb, draw_list);

		pidgin_whiteboard_set_canvas_as_icon(gtkwb);

		/* The brush stroke is finished, clear the list for another one
		 */
		if (draw_list) {
			purple_whiteboard_draw_list_destroy(draw_list);
		}

		purple_whiteboard_set_draw_list(wb, NULL);
	}

	return TRUE;
}

static void pidgin_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);

	gtkwb->width = width;
	gtkwb->height = height;
}

static void pidgin_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);

	gtkwb->brush_size = size;
	gtkwb->brush_color = color;
}

static void pidgin_whiteboard_clear(PurpleWhiteboard *wb)
{
	PidginWhiteboard *gtkwb = purple_whiteboard_get_ui_data(wb);
	GtkWidget *drawing_area = gtkwb->drawing_area;
	cairo_t *cr = gtkwb->cr;
	GtkAllocation allocation;
	GdkRGBA white = {1.0, 1.0, 1.0, 1.0};

	gtk_widget_get_allocation(drawing_area, &allocation);

	gdk_cairo_set_source_rgba(cr, &white);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	gtk_widget_queue_draw_area(drawing_area, 0, 0,
		allocation.width, allocation.height);
}

static void pidgin_whiteboard_button_clear_press(GtkWidget *widget, gpointer data)
{
	PidginWhiteboard *gtkwb = (PidginWhiteboard*)(data);

	/* Confirm whether the user really wants to clear */
	GtkWidget *dialog = gtk_message_dialog_new(
	        GTK_WINDOW(gtkwb), GTK_DIALOG_DESTROY_WITH_PARENT,
	        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s",
	        _("Do you really want to clear?"));
	gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (response == GTK_RESPONSE_YES)
	{
		pidgin_whiteboard_clear(gtkwb->wb);

		pidgin_whiteboard_set_canvas_as_icon(gtkwb);

		/* Do protocol specific clearing procedures */
		purple_whiteboard_send_clear(gtkwb->wb);
	}
}

static void
pidgin_whiteboard_button_save_press(GtkWidget *widget, gpointer _gtkwb)
{
	PidginWhiteboard *gtkwb = _gtkwb;
	GdkPixbuf *pixbuf;
	GtkFileChooserNative *chooser;
	int result;

	chooser = gtk_file_chooser_native_new(_("Save File"), GTK_WINDOW(gtkwb),
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Save"), _("_Cancel"));

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser),
	                                               TRUE);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),
	                                  "whiteboard.png");

	result = gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser));
	if (result == GTK_RESPONSE_ACCEPT) {
		gboolean success;
		gchar *filename =
		        gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

		pixbuf = gdk_pixbuf_get_from_surface(
		        gtkwb->surface, 0, 0, gtkwb->width, gtkwb->height);

		success = gdk_pixbuf_save(pixbuf, filename, "png", NULL,
			"compression", "9", NULL);
		g_object_unref(pixbuf);
		if (success) {
			purple_debug_info("gtkwhiteboard",
				"whiteboard saved to \"%s\"", filename);
		} else {
			purple_notify_error(NULL, _("Whiteboard"),
				_("Unable to save the file"), NULL, NULL);
			purple_debug_error("gtkwhiteboard", "whiteboard "
				"couldn't be saved to \"%s\"", filename);
		}
		g_free(filename);
	}

	g_object_unref(chooser);
}

static void
color_selected(GtkColorButton *button, PidginWhiteboard *gtkwb)
{
	GdkRGBA color;
	PurpleWhiteboard *wb = gtkwb->wb;
	int old_size, old_color;
	int new_color;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &color);

	new_color = (unsigned int)(color.red * 255) << 16;
	new_color |= (unsigned int)(color.green * 255) << 8;
	new_color |= (unsigned int)(color.blue * 255);

	purple_whiteboard_get_brush(wb, &old_size, &old_color);
	purple_whiteboard_send_brush(wb, old_size, new_color);
}

static void
pidgin_whiteboard_create(PurpleWhiteboard *wb)
{
	PidginWhiteboard *gtkwb;
	PurpleBuddy *buddy;
	GdkRGBA color;

	gtkwb = PIDGIN_WHITEBOARD(g_object_new(PIDGIN_TYPE_WHITEBOARD, NULL));
	gtkwb->wb = wb;
	purple_whiteboard_set_ui_data(wb, gtkwb);

	/* Get dimensions (default?) for the whiteboard canvas */
	if (!purple_whiteboard_get_dimensions(wb, &gtkwb->width,
	                                      &gtkwb->height)) {
		/* Give some initial board-size */
		gtkwb->width = 300;
		gtkwb->height = 250;
	}

	if (!purple_whiteboard_get_brush(wb, &gtkwb->brush_size,
	                                 &gtkwb->brush_color)) {
		/* Give some initial brush-info */
		gtkwb->brush_size = 2;
		gtkwb->brush_color = 0xff0000;
	}

	/* Try and set window title as the name of the buddy, else just use
	 * their username
	 */
	buddy = purple_blist_find_buddy(purple_whiteboard_get_account(wb),
	                                purple_whiteboard_get_who(wb));

	gtk_window_set_title(GTK_WINDOW(gtkwb),
	                     buddy != NULL
	                             ? purple_buddy_get_contact_alias(buddy)
	                             : purple_whiteboard_get_who(wb));
	gtk_widget_set_name(GTK_WIDGET(gtkwb), purple_whiteboard_get_who(wb));

	gtk_widget_set_size_request(GTK_WIDGET(gtkwb->drawing_area),
	                            gtkwb->width, gtkwb->height);

	pidgin_whiteboard_rgb24_to_rgba(gtkwb->brush_color, &color);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(gtkwb->color_button),
	                           &color);

	/* Make all this (window) visible */
	gtk_widget_show(GTK_WIDGET(gtkwb));

	pidgin_whiteboard_set_canvas_as_icon(gtkwb);

	/* TODO Specific protocol/whiteboard assignment here? Needs a UI Op? */
	/* Set default brush size and color */
	/*
	ds->brush_size = DOODLE_BRUSH_MEDIUM;
	ds->brush_color = 0;
	*/
}

static void
pidgin_whiteboard_destroy(PurpleWhiteboard *wb)
{
	PidginWhiteboard *gtkwb;

	g_return_if_fail(wb != NULL);
	gtkwb = purple_whiteboard_get_ui_data(wb);
	g_return_if_fail(gtkwb != NULL);

	/* TODO Ask if user wants to save picture before the session is closed
	 */

	gtkwb->wb = NULL;
	g_object_unref(gtkwb);
	purple_whiteboard_set_ui_data(wb, NULL);
}

/******************************************************************************
 * GObject implementation
 *****************************************************************************/
static void
pidgin_whiteboard_init(PidginWhiteboard *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	self->brush_state = PIDGIN_WHITEBOARD_BRUSH_UP;
}

static void
pidgin_whiteboard_finalize(GObject *obj)
{
	PidginWhiteboard *gtkwb = PIDGIN_WHITEBOARD(obj);

	/* Clear graphical memory */
	g_clear_pointer(&gtkwb->cr, cairo_destroy);
	g_clear_pointer(&gtkwb->surface, cairo_surface_destroy);

	G_OBJECT_CLASS(pidgin_whiteboard_parent_class)->finalize(obj);
}

static void
pidgin_whiteboard_class_init(PidginWhiteboardClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->finalize = pidgin_whiteboard_finalize;

	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin/Whiteboard/whiteboard.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginWhiteboard,
	                                     drawing_area);
	gtk_widget_class_bind_template_child(widget_class, PidginWhiteboard,
	                                     color_button);

	gtk_widget_class_bind_template_callback(
	        widget_class, whiteboard_close_cb);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_draw_event);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_configure_event);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_brush_down);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_brush_motion);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_brush_up);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_button_clear_press);
	gtk_widget_class_bind_template_callback(
	        widget_class, pidgin_whiteboard_button_save_press);
	gtk_widget_class_bind_template_callback(
	        widget_class, color_selected);
}

/******************************************************************************
 * API
 *****************************************************************************/
static PurpleWhiteboardUiOps ui_ops =
{
	pidgin_whiteboard_create,
	pidgin_whiteboard_destroy,
	pidgin_whiteboard_set_dimensions,
	pidgin_whiteboard_set_brush,
	pidgin_whiteboard_draw_brush_point,
	pidgin_whiteboard_draw_brush_line,
	pidgin_whiteboard_clear,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleWhiteboardUiOps *
pidgin_whiteboard_get_ui_ops(void)
{
	return &ui_ops;
}
