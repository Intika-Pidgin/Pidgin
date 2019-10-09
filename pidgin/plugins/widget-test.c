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
#include <glib.h>
#include <gmodule.h>
#include <gplugin.h>

#include <purple.h>

#include "gtkplugin.h"
#include "pidginprotocolstore.h"

/* make the compiler happy... */
G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **error);
G_MODULE_EXPORT gboolean gplugin_load(GPluginPlugin *plugin, GError **error);
G_MODULE_EXPORT gboolean gplugin_unload(GPluginPlugin *plugin, GError **error);

/******************************************************************************
 * Config Frame
 *****************************************************************************/
static GtkWidget *
pidgin_widget_test_config_frame(G_GNUC_UNUSED PurplePlugin *plugin) {
	GtkWidget *vbox = NULL, *hbox = NULL;
	GtkWidget *label = NULL, *combo = NULL;
	GtkListStore *store = NULL;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("Protocols:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	store = pidgin_protocol_store_new();
	combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	return vbox;
}

/******************************************************************************
 * Plugin Exports
 *****************************************************************************/
G_MODULE_EXPORT GPluginPluginInfo *
gplugin_query(GError **error) {
	PidginPluginInfo *info = NULL;

	const gchar * const authors[] = {
		"Gary Kramlich <grim@reaperworld.com>",
		NULL
	};

	info = pidgin_plugin_info_new(
		"id", "pidgin/widget-test",
		"abi-version", PURPLE_ABI_VERSION,
		"name", "Pidgin Widget Test",
		"version", "0.0.1",
		"authors", authors,
		"gtk-config-frame-cb", pidgin_widget_test_config_frame,
		NULL
	);

	return GPLUGIN_PLUGIN_INFO(info);
}

G_MODULE_EXPORT gboolean
gplugin_load(GPluginPlugin *plugin, GError **error) {
	return TRUE;
}

G_MODULE_EXPORT gboolean
gplugin_unload(GPluginPlugin *plugin, GError **error) {
	return TRUE;
}
