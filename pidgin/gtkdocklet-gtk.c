/*
 * System tray icon (aka docklet) plugin for Purple
 *
 * Copyright (C) 2007 Anders Hasselqvist
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "internal.h"
#include "pidgin.h"
#include "debug.h"
#include "prefs.h"
#include "pidginstock.h"
#include "gtkdocklet.h"

#define SHORT_EMBED_TIMEOUT 5
#define LONG_EMBED_TIMEOUT 15

/* globals */
static GtkStatusIcon *docklet = NULL;
static guint embed_timeout = 0;

/* protos */
static void docklet_gtk_status_create(gboolean);

static gboolean
docklet_gtk_recreate_cb(gpointer data)
{
	docklet_gtk_status_create(TRUE);

	return FALSE;
}

static gboolean
docklet_gtk_embed_timeout_cb(gpointer data)
{
#if !GTK_CHECK_VERSION(2,12,0)
	if (gtk_status_icon_is_embedded(docklet)) {
		/* Older GTK+ (<2.12) don't implement the embedded signal, but the
		   information is still accessable through the above function. */
		purple_debug_info("docklet", "embedded\n");

		pidgin_docklet_embedded();
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", TRUE);
	}
	else
#endif
	{
		/* The docklet was not embedded within the timeout.
		 * Remove it as a visibility manager, but leave the plugin
		 * loaded so that it can embed automatically if/when a notification
		 * area becomes available.
		 */
		purple_debug_info("docklet", "failed to embed within timeout\n");
		pidgin_docklet_remove();
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", FALSE);
	}

#if GTK_CHECK_VERSION(2,12,0)
	embed_timeout = 0;
	return FALSE;
#else
	return TRUE;
#endif
}

#if GTK_CHECK_VERSION(2,12,0)
static gboolean
docklet_gtk_embedded_cb(GtkWidget *widget, gpointer data)
{
	if (embed_timeout) {
		purple_timeout_remove(embed_timeout);
		embed_timeout = 0;
	}

	if (gtk_status_icon_is_embedded(docklet)) {
		purple_debug_info("docklet", "embedded\n");

		pidgin_docklet_embedded();
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", TRUE);
	} else {
		purple_debug_info("docklet", "detached\n");

		pidgin_docklet_remove();
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", FALSE);
	}

	return TRUE;
}
#endif

static void
docklet_gtk_destroyed_cb(GtkWidget *widget, gpointer data)
{
	purple_debug_info("docklet", "destroyed\n");

	pidgin_docklet_remove();

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	g_idle_add(docklet_gtk_recreate_cb, NULL);
}

static void
docklet_gtk_status_activated_cb(GtkStatusIcon *status_icon, gpointer user_data)
{
	pidgin_docklet_clicked(1);
}

static void
docklet_gtk_status_clicked_cb(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data)
{
	purple_debug_info("docklet", "The button is %u\n", button);
#ifdef GDK_WINDOWING_QUARTZ
	/* You can only click left mouse button on MacOSX native GTK. Let that be the menu */
	pidgin_docklet_clicked(3);
#else
	pidgin_docklet_clicked(button);
#endif
}

static void
docklet_gtk_status_update_icon(PurpleStatusPrimitive status, gboolean connecting, gboolean pending)
{
	const gchar *icon_name = NULL;
	const gchar *current_icon_name = gtk_status_icon_get_icon_name(docklet);

	switch (status) {
		case PURPLE_STATUS_OFFLINE:
			icon_name = PIDGIN_STOCK_TRAY_OFFLINE;
			break;
		case PURPLE_STATUS_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_AWAY;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			icon_name = PIDGIN_STOCK_TRAY_BUSY;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_XA;
			break;
		case PURPLE_STATUS_INVISIBLE:
			icon_name = PIDGIN_STOCK_TRAY_INVISIBLE;
			break;
		default:
			icon_name = PIDGIN_STOCK_TRAY_AVAILABLE;
			break;
	}

	if (connecting && !purple_strequal(current_icon_name, PIDGIN_STOCK_TRAY_CONNECT)) {
		icon_name = PIDGIN_STOCK_TRAY_CONNECT;
	}

	if (pending && !purple_strequal(current_icon_name, PIDGIN_STOCK_TRAY_PENDING)) {
		icon_name = PIDGIN_STOCK_TRAY_PENDING;
	}

	if (icon_name) {
		gtk_status_icon_set_from_icon_name(docklet, icon_name);
	}
}

static void
docklet_gtk_status_set_tooltip(gchar *tooltip)
{
	gtk_status_icon_set_tooltip(docklet, tooltip);
}

static void
docklet_gtk_status_position_menu(GtkMenu *menu,
                                 int *x, int *y, gboolean *push_in,
                                 gpointer user_data)
{
	gtk_status_icon_position_menu(menu, x, y, push_in, docklet);
}

static void
docklet_gtk_status_destroy(void)
{
	g_return_if_fail(docklet != NULL);

	pidgin_docklet_remove();

	if (embed_timeout) {
		purple_timeout_remove(embed_timeout);
		embed_timeout = 0;
	}

	gtk_status_icon_set_visible(docklet, FALSE);
	g_signal_handlers_disconnect_by_func(G_OBJECT(docklet), G_CALLBACK(docklet_gtk_destroyed_cb), NULL);
	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	purple_debug_info("docklet", "GTK+ destroyed\n");
}

static void
docklet_gtk_status_create(gboolean recreate)
{
	if (docklet) {
		/* if this is being called when a tray icon exists, it's because
		   something messed up. try destroying it before we proceed,
		   although docklet_refcount may be all hosed. hopefully won't happen. */
		purple_debug_warning("docklet", "trying to create icon but it already exists?\n");
		docklet_gtk_status_destroy();
	}

	docklet = gtk_status_icon_new();
	g_return_if_fail(docklet != NULL);

	g_signal_connect(G_OBJECT(docklet), "activate", G_CALLBACK(docklet_gtk_status_activated_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "popup-menu", G_CALLBACK(docklet_gtk_status_clicked_cb), NULL);
#if GTK_CHECK_VERSION(2,12,0)
	g_signal_connect(G_OBJECT(docklet), "notify::embedded", G_CALLBACK(docklet_gtk_embedded_cb), NULL);
#endif
	g_signal_connect(G_OBJECT(docklet), "destroy", G_CALLBACK(docklet_gtk_destroyed_cb), NULL);

	gtk_status_icon_set_visible(docklet, TRUE);

	/* This is a hack to avoid a race condition between the docklet getting
	 * embedded in the notification area and the gtkblist restoring its
	 * previous visibility state.  If the docklet does not get embedded within
	 * the timeout, it will be removed as a visibility manager until it does
	 * get embedded.  Ideally, we would only call docklet_embedded() when the
	 * icon was actually embedded. This only happens when the docklet is first
	 * created, not when being recreated.
	 *
	 * The gtk docklet tracks whether it successfully embedded in a pref and
	 * allows for a longer timeout period if it successfully embedded the last
	 * time it was run. This should hopefully solve problems with the buddy
	 * list not properly starting hidden when Pidgin is started on login.
	 */
	if (!recreate) {
		pidgin_docklet_embedded();
#if GTK_CHECK_VERSION(2,12,0)
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded")) {
			embed_timeout = purple_timeout_add_seconds(LONG_EMBED_TIMEOUT, docklet_gtk_embed_timeout_cb, NULL);
		} else {
			embed_timeout = purple_timeout_add_seconds(SHORT_EMBED_TIMEOUT, docklet_gtk_embed_timeout_cb, NULL);
		}
#else
		embed_timeout = purple_timeout_add_seconds(SHORT_EMBED_TIMEOUT, docklet_gtk_embed_timeout_cb, NULL);
#endif
	}

	purple_debug_info("docklet", "GTK+ created\n");
}

static void
docklet_gtk_status_create_ui_op(void)
{
	docklet_gtk_status_create(FALSE);
}

static struct docklet_ui_ops ui_ops =
{
	docklet_gtk_status_create_ui_op,
	docklet_gtk_status_destroy,
	docklet_gtk_status_update_icon,
	NULL,
	docklet_gtk_status_set_tooltip,
	docklet_gtk_status_position_menu
};

void
docklet_ui_init(void)
{
	pidgin_docklet_set_ui_ops(&ui_ops);

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/docklet/gtk");
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/x11/embedded")) {
		purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", TRUE);
		purple_prefs_remove(PIDGIN_PREFS_ROOT "/docklet/x11/embedded");
	} else {
		purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", FALSE);
	}

	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(),
		DATADIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "pidgin" G_DIR_SEPARATOR_S "tray");
}

