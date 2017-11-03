/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or(at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#include "gtkdnd-hints.h"

#include "internal.h"

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gtk3compat.h"

typedef struct
{
	GtkWidget *widget;
	gchar *filename;
	gint ox;
	gint oy;

} HintWindowInfo;

/*
 * Info about each hint widget. See PidginDndHintWindowId enum.
 */
static HintWindowInfo hint_windows[] = {
	{ NULL, "arrow-up.xpm",   -13/2,     0 },
	{ NULL, "arrow-down.xpm", -13/2,   -16 },
	{ NULL, "arrow-left.xpm",     0, -13/2 },
	{ NULL, "arrow-right.xpm",  -16, -13/2 },
	{ NULL, NULL, 0, 0 }
};

static void
dnd_hints_realized_cb(GtkWidget *window, GtkWidget *pix)
{
	GdkPixbuf *pixbuf;
	cairo_surface_t *surface;
	cairo_region_t *region;
	cairo_t *cr;

	pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(pix));

	surface = cairo_image_surface_create(CAIRO_FORMAT_A1,
	                                     gdk_pixbuf_get_width(pixbuf),
	                                     gdk_pixbuf_get_height(pixbuf));

	cr = cairo_create(surface);
	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);

	region = gdk_cairo_region_create_from_surface(surface);
	gtk_widget_shape_combine_region(window, region);
	cairo_region_destroy(region);

	cairo_surface_destroy(surface);
}

static GtkWidget *
dnd_hints_init_window(const gchar *fname)
{
	GdkPixbuf *pixbuf;
	GtkWidget *pix;
	GtkWidget *win;

	pixbuf = gdk_pixbuf_new_from_file(fname, NULL);
	g_return_val_if_fail(pixbuf, NULL);

	win = gtk_window_new(GTK_WINDOW_POPUP);
	pix = gtk_image_new_from_pixbuf(pixbuf);
	gtk_container_add(GTK_CONTAINER(win), pix);
	gtk_widget_show_all(pix);

	g_object_unref(G_OBJECT(pixbuf));

	g_signal_connect(G_OBJECT(win), "realize",
	                 G_CALLBACK(dnd_hints_realized_cb), pix);

	return win;
}

static void
get_widget_coords(GtkWidget *w, gint *x1, gint *y1, gint *x2, gint *y2)
{
	gint ox, oy, width, height;
	GtkWidget *parent = gtk_widget_get_parent(w);

	if (parent && gtk_widget_get_window(parent) == gtk_widget_get_window(w))
	{
		GtkAllocation allocation;

		gtk_widget_get_allocation(w, &allocation);
		get_widget_coords(parent, &ox, &oy, NULL, NULL);
		height = allocation.height;
		width = allocation.width;
	}
	else
	{
		GdkWindow *win = gtk_widget_get_window(w);
		gdk_window_get_origin(win, &ox, &oy);
		width = gdk_window_get_width(win);
		height = gdk_window_get_height(win);
	}

	if (x1) *x1 = ox;
	if (y1) *y1 = oy;
	if (x2) *x2 = ox + width;
	if (y2) *y2 = oy + height;
}

static void
dnd_hints_init(void)
{
	static gboolean done = FALSE;
	gint i;

	if (done)
		return;

	done = TRUE;

	for (i = 0; hint_windows[i].filename != NULL; i++) {
		gchar *fname;

		fname = g_build_filename(PURPLE_DATADIR, "pixmaps", "pidgin",
			hint_windows[i].filename, NULL);

		hint_windows[i].widget = dnd_hints_init_window(fname);

		g_free(fname);
	}
}

void
pidgin_dnd_hints_hide_all(void)
{
	gint i;

	for (i = 0; hint_windows[i].filename != NULL; i++)
		pidgin_dnd_hints_hide(i);
}

void
pidgin_dnd_hints_hide(PidginDndHintWindowId i)
{
	GtkWidget *w = hint_windows[i].widget;

	if (w && GTK_IS_WIDGET(w))
		gtk_widget_hide(w);
}

void
pidgin_dnd_hints_show(PidginDndHintWindowId id, gint x, gint y)
{
	GtkWidget *w;

	dnd_hints_init();

	w = hint_windows[id].widget;

	if (w && GTK_IS_WIDGET(w))
	{
		gtk_window_move(GTK_WINDOW(w), hint_windows[id].ox + x,
								 hint_windows[id].oy + y);
		gtk_widget_show(w);
	}
}

void
pidgin_dnd_hints_show_relative(PidginDndHintWindowId id, GtkWidget *widget,
						PidginDndHintPosition horiz, PidginDndHintPosition vert)
{
	gint x1, x2, y1, y2;
	gint x = 0, y = 0;
	GtkAllocation allocation;

	gtk_widget_get_allocation(widget, &allocation);

	get_widget_coords(widget, &x1, &y1, &x2, &y2);
	x1 += allocation.x;	x2 += allocation.x;
	y1 += allocation.y;	y2 += allocation.y;

	switch (horiz)
	{
		case HINT_POSITION_RIGHT:  x = x2;            break;
		case HINT_POSITION_LEFT:   x = x1;            break;
		case HINT_POSITION_CENTER: x = (x1 + x2) / 2; break;
		default:
			/* should not happen */
			g_warning("Invalid parameter to pidgin_dnd_hints_show_relative");
			break;
	}

	switch (vert)
	{
		case HINT_POSITION_TOP:    y = y1;            break;
		case HINT_POSITION_BOTTOM: y = y2;            break;
		case HINT_POSITION_CENTER: y = (y1 + y2) / 2; break;
		default:
			/* should not happen */
			g_warning("Invalid parameter to pidgin_dnd_hints_show_relative");
			break;
	}

	pidgin_dnd_hints_show(id, x, y);
}

