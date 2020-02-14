/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#include "pidginprotocolchooser.h"

#include "pidginprotocolstore.h"

/******************************************************************************
 * Structs
 *****************************************************************************/

struct _PidginProtocolChooser {
	GtkComboBox parent;

	GtkListStore *model;
};

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginProtocolChooser, pidgin_protocol_chooser,
              GTK_TYPE_COMBO_BOX)

static void
pidgin_protocol_chooser_class_init(PidginProtocolChooserClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class,
	                                            "/im/pidgin/Pidgin/Protocols/chooser.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginProtocolChooser,
	                                     model);
}

static void
pidgin_protocol_chooser_init(PidginProtocolChooser *chooser) {
	gtk_widget_init_template(GTK_WIDGET(chooser));
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_protocol_chooser_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_PROTOCOL_CHOOSER, NULL));
}

gchar *
pidgin_protocol_chooser_get_selected(PidginProtocolChooser *chooser)
{
	GtkTreeIter iter;
	gchar *name = NULL;

	g_return_val_if_fail(PIDGIN_IS_PROTOCOL_CHOOSER(chooser), NULL);

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(chooser), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(chooser->model), &iter,
		                   PIDGIN_PROTOCOL_STORE_COLUMN_NAME, &name,
		                   -1);
	}

	return name;
}

void
pidgin_protocol_chooser_set_selected(PidginProtocolChooser *chooser,
                                     const gchar *name)
{
	GtkTreeIter iter;
	gchar *iter_name = NULL;

	g_return_if_fail(PIDGIN_IS_PROTOCOL_CHOOSER(chooser));

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(chooser->model), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(chooser->model), &iter,
			                   PIDGIN_PROTOCOL_STORE_COLUMN_NAME, &iter_name,
			                   -1);

			if(purple_strequal(iter_name, name)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(chooser), &iter);

				g_free(iter_name);

				return;
			}

			g_free(iter_name);
		} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(chooser->model),
		                                 &iter));
	}
}
