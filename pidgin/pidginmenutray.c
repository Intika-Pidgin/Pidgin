/*
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
#include "internal.h"

#include "debug.h"

#include "pidginmenutray.h"

struct _PidginMenuTray {
	GtkMenuItem parent;

	GtkWidget *tray;
};

enum {
	PROP_ZERO = 0,
	PROP_BOX,
	N_PROPERTIES
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

/******************************************************************************
 * Item Stuff
 *****************************************************************************/
static void
pidgin_menu_tray_select(GtkMenuItem *widget) {
	/* this may look like nothing, but it's really overriding the
	 * GtkMenuItem's select function so that it doesn't get highlighted like
	 * a normal menu item would.
	 */
}

static void
pidgin_menu_tray_deselect(GtkMenuItem *widget) {
	/* Probably not necessary, but I'd rather be safe than sorry.  We're
	 * overridding the select, so it makes sense to override deselect as well.
	 */
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginMenuTray, pidgin_menu_tray, GTK_TYPE_MENU_ITEM);

static void
pidgin_menu_tray_get_property(GObject *obj, guint param_id, GValue *value,
                              GParamSpec *pspec)
{
	PidginMenuTray *menu_tray = PIDGIN_MENU_TRAY(obj);

	switch(param_id) {
		case PROP_BOX:
			g_value_set_object(value, pidgin_menu_tray_get_box(menu_tray));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_menu_tray_map(GtkWidget *widget) {
	GTK_WIDGET_CLASS(pidgin_menu_tray_parent_class)->map(widget);

	gtk_container_add(GTK_CONTAINER(widget), PIDGIN_MENU_TRAY(widget)->tray);
}

static void
pidgin_menu_tray_class_init(PidginMenuTrayClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkMenuItemClass *menu_item_class = GTK_MENU_ITEM_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->get_property = pidgin_menu_tray_get_property;

	menu_item_class->select = pidgin_menu_tray_select;
	menu_item_class->deselect = pidgin_menu_tray_deselect;

	widget_class->map = pidgin_menu_tray_map;

	properties[PROP_BOX] = g_param_spec_object("box", "The box", "The box",
	                                           GTK_TYPE_BOX,
	                                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

static void
pidgin_menu_tray_init(PidginMenuTray *menu_tray) {
	GtkWidget *widget = GTK_WIDGET(menu_tray);
	gint height = -1;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	/* Gtk3 docs says, it should be replaced with gtk_widget_set_hexpand and
	 * gtk_widget_set_halign. But it doesn't seems to work. */
	gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menu_tray), TRUE);
G_GNUC_END_IGNORE_DEPRECATIONS

	if(!GTK_IS_WIDGET(menu_tray->tray)) {
		menu_tray->tray = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	}

	if(gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, NULL, &height)) {
		gtk_widget_set_size_request(widget, -1, height);
	}

	gtk_widget_show(menu_tray->tray);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_menu_tray_new() {
	return g_object_new(PIDGIN_TYPE_MENU_TRAY, NULL);
}

GtkWidget *
pidgin_menu_tray_get_box(PidginMenuTray *menu_tray) {
	g_return_val_if_fail(PIDGIN_IS_MENU_TRAY(menu_tray), NULL);

	return menu_tray->tray;
}

static void
pidgin_menu_tray_add(PidginMenuTray *menu_tray, GtkWidget *widget,
					   const char *tooltip, gboolean prepend)
{
	g_return_if_fail(PIDGIN_IS_MENU_TRAY(menu_tray));
	g_return_if_fail(GTK_IS_WIDGET(widget));

	if (!gtk_widget_get_has_window(widget)) {
		GtkWidget *event;

		event = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(event), widget);
		gtk_widget_show(event);
		widget = event;
	}

	pidgin_menu_tray_set_tooltip(menu_tray, widget, tooltip);

	if (prepend) {
		gtk_box_pack_start(GTK_BOX(menu_tray->tray), widget, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_end(GTK_BOX(menu_tray->tray), widget, FALSE, FALSE, 0);
	}
}

void
pidgin_menu_tray_append(PidginMenuTray *menu_tray, GtkWidget *widget,
                        const char *tooltip)
{
	pidgin_menu_tray_add(menu_tray, widget, tooltip, FALSE);
}

void
pidgin_menu_tray_prepend(PidginMenuTray *menu_tray, GtkWidget *widget,
                         const char *tooltip)
{
	pidgin_menu_tray_add(menu_tray, widget, tooltip, TRUE);
}

void
pidgin_menu_tray_set_tooltip(PidginMenuTray *menu_tray, GtkWidget *widget,
                             const char *tooltip)
{
	/* Should we check whether widget is a child of menu_tray? */

	/*
	 * If the widget does not have its own window, then it
	 * must have automatically been added to an event box
	 * when it was added to the menu tray.  If this is the
	 * case, we want to set the tooltip on the widget's parent,
	 * not on the widget itself.
	 */
	if (!gtk_widget_get_has_window(widget)) {
		widget = gtk_widget_get_parent(widget);
	}

	gtk_widget_set_tooltip_text(widget, tooltip);
}

