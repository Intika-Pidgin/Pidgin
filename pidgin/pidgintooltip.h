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

#ifndef _PIDGIN_TOOLTIP_H_
#define _PIDGIN_TOOLTIP_H_
/**
 * SECTION:pidgintooltip
 * @section_id: pidgin-pidgintooltip
 * @short_description: <filename>pidgintooltip.h</filename>
 * @title: Pidgin Tooltip API
 */

#include <gtk/gtk.h>

/**
 * PidginTooltipCreateForTree:
 * @tipwindow:  The window for the tooltip.
 * @path:       The GtkTreePath representing the row under the cursor.
 * @userdata:   The userdata set during pidgin_tooltip_setup_for_treeview.
 * @w:          The value of this should be set to the desired width of the tooltip window.
 * @h:          The value of this should be set to the desired height of the tooltip window.
 *
 * Returns:  %TRUE if the tooltip was created correctly, %FALSE otherwise.
 */
typedef gboolean (*PidginTooltipCreateForTree)(GtkWidget *tipwindow,
			GtkTreePath *path, gpointer userdata, int *w, int *h);

/**
 * PidginTooltipCreate:
 * @tipwindow:  The window for the tooltip.
 * @userdata:   The userdata set during pidgin_tooltip_show.
 * @w:          The value of this should be set to the desired width of the tooltip window.
 * @h:          The value of this should be set to the desired height of the tooltip window.
 *
 * Returns:  %TRUE if the tooltip was created correctly, %FALSE otherwise.
 */
typedef gboolean (*PidginTooltipCreate)(GtkWidget *tipwindow,
			gpointer userdata, int *w, int *h);

/**
 * PidginTooltipPaint:
 * @tipwindow:   The window for the tooltip.
 * @cr:          The cairo context for drawing.
 * @userdata:    The userdata set during pidgin_tooltip_setup_for_treeview or pidgin_tooltip_show.
 *
 * Returns:  %TRUE if the tooltip was painted correctly, %FALSE otherwise.
 */
typedef gboolean (*PidginTooltipPaint)(GtkWidget *tipwindow, cairo_t *cr,
			gpointer userdata);

G_BEGIN_DECLS

/**
 * pidgin_tooltip_setup_for_treeview:
 * @tree:         The treeview
 * @userdata:     The userdata to send to the callback functions
 * @create_cb:    Callback function to create the tooltip for a GtkTreePath
 * @paint_cb:     Callback function to paint the tooltip
 *
 * Setup tooltip drawing functions for a treeview.
 *
 * Returns:   %TRUE if the tooltip callbacks were setup correctly.
 */
gboolean pidgin_tooltip_setup_for_treeview(GtkWidget *tree, gpointer userdata,
		PidginTooltipCreateForTree create_cb, PidginTooltipPaint paint_cb);

/**
 * pidgin_tooltip_setup_for_widget:
 * @widget:       The widget
 * @userdata:     The userdata to send to the callback functions
 * @create_cb:    Callback function to create the tooltip for the widget
 * @paint_cb:     Callback function to paint the tooltip
 *
 * Setup tooltip drawing functions for any widget.
 *
 * Returns:   %TRUE if the tooltip callbacks were setup correctly.
 */
gboolean pidgin_tooltip_setup_for_widget(GtkWidget *widget, gpointer userdata,
		PidginTooltipCreate create_cb, PidginTooltipPaint paint_cb);

/**
 * pidgin_tooltip_destroy:
 *
 * Destroy the tooltip.
 */
void pidgin_tooltip_destroy(void);

/**
 * pidgin_tooltip_show:
 * @widget:      The widget the tooltip is for
 * @userdata:    The userdata to send to the callback functions
 * @create_cb:    Callback function to create the tooltip from the GtkTreePath
 * @paint_cb:     Callback function to paint the tooltip
 *
 * Create and show a tooltip.
 */
void pidgin_tooltip_show(GtkWidget *widget, gpointer userdata,
		PidginTooltipCreate create_cb, PidginTooltipPaint paint_cb);

G_END_DECLS

#endif
