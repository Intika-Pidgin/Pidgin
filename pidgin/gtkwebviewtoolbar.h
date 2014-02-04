/*
 * GtkWebViewToolbar
 *
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
 *
 */
/**
 * SECTION:gtkwebviewtoolbar
 * @section_id: pidgin-gtkwebviewtoolbar
 * @short_description: <filename>gtkwebviewtoolbar.h</filename>
 * @title: WebView Toolbar
 */

#ifndef _PIDGINWEBVIEWTOOLBAR_H_
#define _PIDGINWEBVIEWTOOLBAR_H_

#include <gtk/gtk.h>
#include "gtkwebview.h"

#define DEFAULT_FONT_FACE "Helvetica 12"

#define GTK_TYPE_WEBVIEWTOOLBAR            (gtk_webviewtoolbar_get_type())
#define GTK_WEBVIEWTOOLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_WEBVIEWTOOLBAR, GtkWebViewToolbar))
#define GTK_WEBVIEWTOOLBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_WEBVIEWTOOLBAR, GtkWebViewToolbarClass))
#define GTK_IS_WEBVIEWTOOLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_WEBVIEWTOOLBAR))
#define GTK_IS_WEBVIEWTOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_WEBVIEWTOOLBAR))
#define GTK_WEBVIEWTOOLBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_WEBVIEWTOOLBAR, GtkWebViewToolbarClass))

typedef struct _GtkWebViewToolbar GtkWebViewToolbar;
typedef struct _GtkWebViewToolbarClass GtkWebViewToolbarClass;

struct _GtkWebViewToolbar {
	GtkHBox box;

	GtkWidget *webview;
};

struct _GtkWebViewToolbarClass {
	GtkHBoxClass parent_class;
};

G_BEGIN_DECLS

/**
 * gtk_webviewtoolbar_get_type:
 *
 * Returns the GType for a GtkWebViewToolbar widget
 *
 * Returns: The GType for GtkWebViewToolbar widget
 */
GType gtk_webviewtoolbar_get_type(void);

/**
 * gtk_webviewtoolbar_new:
 *
 * Create a new GtkWebViewToolbar object
 *
 * Returns: A GtkWidget corresponding to the GtkWebViewToolbar object
 */
GtkWidget *gtk_webviewtoolbar_new(void);

/**
 * gtk_webviewtoolbar_attach:
 * @toolbar: The GtkWebViewToolbar object
 * @webview: The GtkWebView object
 *
 * Attach a GtkWebViewToolbar object to a GtkWebView
 */
void gtk_webviewtoolbar_attach(GtkWebViewToolbar *toolbar, GtkWidget *webview);

/**
 * gtk_webviewtoolbar_associate_smileys:
 * @toolbar:  The GtkWebViewToolbar object
 * @proto_id: The ID of the protocol from which smileys are associated
 *
 * Associate the smileys from a protocol to a GtkWebViewToolbar object
 */
void gtk_webviewtoolbar_associate_smileys(GtkWebViewToolbar *toolbar,
                                          const char *proto_id);

/**
 * gtk_webviewtoolbar_switch_active_conversation:
 * @toolbar: The GtkWebViewToolbar object
 * @conv:    The new conversation
 *
 * Switch the active conversation for a GtkWebViewToolbar object
 */
void gtk_webviewtoolbar_switch_active_conversation(GtkWebViewToolbar *toolbar,
                                                   PurpleConversation *conv);

/**
 * gtk_webviewtoolbar_activate:
 * @toolbar: The GtkWebViewToolbar object
 * @action:  The GtkWebViewAction
 *
 * Activate a GtkWebViewToolbar action
 */
void gtk_webviewtoolbar_activate(GtkWebViewToolbar *toolbar,
                                 GtkWebViewAction action);

G_END_DECLS

#endif /* _PIDGINWEBVIEWTOOLBAR_H_ */

