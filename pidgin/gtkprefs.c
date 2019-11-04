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
#include <talkatu.h>

#include "internal.h"
#include "glibcompat.h"
#include "pidgin.h"

#include "debug.h"
#include "nat-pmp.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "protocol.h"
#include "request.h"
#include "savedstatuses.h"
#include "sound.h"
#include "sound-theme.h"
#include "stun.h"
#include "theme-manager.h"
#include "upnp.h"
#include "util.h"
#include "network.h"
#include "keyring.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksmiley-theme.h"
#include "gtksound.h"
#include "gtkstatus-icon-theme.h"
#include "gtkutils.h"
#include "pidgindebug.h"
#include "pidginstock.h"
#ifdef USE_VV
#include "media-gst.h"
#include <gst/video/videooverlay.h>
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_QUARTZ
#include <gdk/gdkquartz.h>
#endif
#endif
#include <libsoup/soup.h>

#include "gtk3compat.h"

#define PREFS_OPTIMAL_ICON_SIZE 32

/* 25MB */
#define PREFS_MAX_DOWNLOADED_THEME_SIZE 26214400

struct theme_info {
	gchar *type;
	gchar *extension;
	gchar *original_name;
};

typedef struct _PidginPrefCombo PidginPrefCombo;

typedef void (*PidginPrefsBindDropdownCallback)(GtkComboBox *combo_box,
	PidginPrefCombo *combo);

struct _PidginPrefCombo {
	GtkWidget *combo;
	PurplePrefType type;
	const gchar *key;
	union {
		const char *string;
		int integer;
		gboolean boolean;
	} value;
	gint previously_active;
	gint current_active;
	PidginPrefsBindDropdownCallback cb;
};

struct _PidginPrefsWindow {
	GtkDialog parent;

	/* Stack */
	GtkWidget *stack;

	/* Interface page */
	struct {
		PidginPrefCombo docklet;
		struct {
			PidginPrefCombo hide_new;
		} im;
		struct {
			GtkWidget *minimize_new_convs;
		} win32;
		struct {
			GtkWidget *tabs;
			GtkWidget *tabs_vbox;
			GtkWidget *close_on_tabs;
			PidginPrefCombo tab_side;
			PidginPrefCombo placement;
		} conversations;
	} iface;

	/* Browser page */
	struct {
		GtkWidget *page;
		GtkWidget *stack;
		/* GNOME version */
		GtkWidget *gnome_not_found;
		GtkWidget *gnome_program;
		gchar *gnome_program_path;
		/* Non-GNOME version */
		PidginPrefCombo browser;
		GtkWidget *place_hbox;
		PidginPrefCombo place;
		GtkWidget *manual_command_hbox;
		GtkWidget *manual_command;
	} browser;

	/* Conversations page */
	struct {
		PidginPrefCombo notification_chat;
		GtkWidget *show_incoming_formatting;
		struct {
			GtkWidget *close_immediately;
			GtkWidget *show_buddy_icons;
			GtkWidget *animate_buddy_icons;
			GtkWidget *send_typing;
		} im;
		GtkWidget *spellcheck;
		GtkWidget *use_smooth_scrolling;
		struct {
			GtkWidget *blink_im;
		} win32;
		GtkWidget *resize_custom_smileys;
		GtkWidget *custom_smileys_size;
		GtkWidget *minimum_entry_lines;
		GtkTextBuffer *format_buffer;
		GtkWidget *format_view;
		/* Win32 specific frame */
		GtkWidget *font_frame;
		GtkWidget *use_theme_font;
		GtkWidget *custom_font_hbox;
		GtkWidget *custom_font;
	} conversations;

	/* Logging page */
	struct {
		PidginPrefCombo format;
		GtkWidget *log_ims;
		GtkWidget *log_chats;
		GtkWidget *log_system;
	} logging;

	/* Network page */
	struct {
		GtkWidget *stun_server;
		GtkWidget *auto_ip;
		GtkWidget *public_ip;
		GtkWidget *public_ip_hbox;
		GtkWidget *map_ports;
		GtkWidget *ports_range_use;
		GtkWidget *ports_range_hbox;
		GtkWidget *ports_range_start;
		GtkWidget *ports_range_end;
		GtkWidget *turn_server;
		GtkWidget *turn_port_udp;
		GtkWidget *turn_port_tcp;
		GtkWidget *turn_username;
		GtkWidget *turn_password;
	} network;

	/* Proxy page */
	struct {
		GtkWidget *stack;
		/* GNOME version */
		GtkWidget *gnome_not_found;
		GtkWidget *gnome_program;
		gchar *gnome_program_path;
		/* Non-GNOME version */
		GtkWidget *socks4_remotedns;
		PidginPrefCombo type;
		GtkWidget *options;
		GtkWidget *host;
		GtkWidget *port;
		GtkWidget *username;
		GtkWidget *password;
	} proxy;

	/* Keyrings page */
	struct {
		PidginPrefCombo active;
		GtkWidget *vbox;
		PurpleRequestFields *settings;
		GtkWidget *settings_box;
		GtkWidget *apply;
	} keyring;

	/* Sounds page */
	struct {
		PidginPrefCombo method;
		GtkWidget *method_vbox;
		GtkWidget *command;
		GtkWidget *command_hbox;
		GtkWidget *mute;
		GtkWidget *conv_focus;
		PidginPrefCombo while_status;
		struct {
			GtkWidget *view;
			GtkListStore *store;
		} event;
		GtkWidget *entry;
	} sound;

	/* Away page */
	struct {
		PidginPrefCombo idle_reporting;
		GtkWidget *mins_before_away;
		GtkWidget *idle_hbox;
		GtkWidget *away_when_idle;
		PidginPrefCombo auto_reply;
		GtkWidget *startup_current_status;
		GtkWidget *startup_hbox;
		GtkWidget *startup_label;
	} away;

	/* Themes page */
	struct {
		SoupSession *session;
		GtkWidget *blist;
		GtkWidget *status;
		GtkWidget *sound;
		GtkWidget *smiley;
	} theme;

#ifdef USE_VV
	/* Voice/Video page */
	struct {
		struct {
			PidginPrefCombo input;
			PidginPrefCombo output;
			GtkWidget *level;
			GtkWidget *threshold;
			GtkWidget *volume;
			GtkWidget *test;
			GstElement *pipeline;
		} voice;

		struct {
			PidginPrefCombo input;
			PidginPrefCombo output;
			GtkWidget *drawing_area;
			GtkWidget *test;
			GstElement *pipeline;
		} video;
	} vv;
#endif
};

/* Main dialog */
static PidginPrefsWindow *prefs = NULL;

/* Themes page */
static GtkWidget *prefs_sound_themes_combo_box;
static GtkWidget *prefs_blist_themes_combo_box;
static GtkWidget *prefs_status_themes_combo_box;
static GtkWidget *prefs_smiley_themes_combo_box;

/* Sound theme specific */
static int sound_row_sel = 0;
static gboolean prefs_sound_themes_loading;

/* These exist outside the lifetime of the prefs dialog */
static GtkListStore *prefs_sound_themes;
static GtkListStore *prefs_blist_themes;
static GtkListStore *prefs_status_icon_themes;
static GtkListStore *prefs_smiley_themes;

/*
 * PROTOTYPES
 */
G_DEFINE_TYPE(PidginPrefsWindow, pidgin_prefs_window, GTK_TYPE_DIALOG);
static void delete_prefs(GtkWidget *, void *);

static void
update_spin_value(GtkWidget *w, GtkWidget *spin)
{
	const char *key = g_object_get_data(G_OBJECT(spin), "val");
	int value;

	value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));

	purple_prefs_set_int(key, value);
}

GtkWidget *
pidgin_prefs_labeled_spin_button(GtkWidget *box, const gchar *title,
		const char *key, int min, int max, GtkSizeGroup *sg)
{
	GtkWidget *spin;
	GtkAdjustment *adjust;
	int val;

	val = purple_prefs_get_int(key);

	adjust = GTK_ADJUSTMENT(gtk_adjustment_new(val, min, max, 1, 1, 0));
	spin = gtk_spin_button_new(adjust, 1, 0);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	if (max < 10000)
		gtk_widget_set_size_request(spin, 50, -1);
	else
		gtk_widget_set_size_request(spin, 60, -1);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
					 G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);

	return pidgin_add_widget_to_vbox(GTK_BOX(box), title, sg, spin, FALSE, NULL);
}

static void
pidgin_prefs_bind_spin_button(const char *key, GtkWidget *spin)
{
	GtkAdjustment *adjust;
	int val;

	val = purple_prefs_get_int(key);

	adjust = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_adjustment_set_value(adjust, val);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
			G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
}

static void
entry_set(GtkEntry *entry, gpointer data)
{
	const char *key = (const char*)data;

	purple_prefs_set_string(key, gtk_entry_get_text(entry));
}

GtkWidget *
pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

static void
pidgin_prefs_bind_entry(const char *key, GtkWidget *entry)
{
	const gchar *value;

	value = purple_prefs_get_string(key);

	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(entry_set),
			(char*)key);
}

GtkWidget *
pidgin_prefs_labeled_password(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

/* TODO: Maybe move this up somewheres... */
enum {
	PREF_DROPDOWN_TEXT,
	PREF_DROPDOWN_VALUE,
	PREF_DROPDOWN_COUNT
};

typedef struct
{
	PurplePrefType type;
	union {
		const char *string;
		int integer;
		gboolean boolean;
	} value;
} PidginPrefValue;

typedef void (*PidginPrefsDropdownCallback)(GtkComboBox *combo_box,
	PidginPrefValue value);

static void
dropdown_set(GtkComboBox *combo_box, gpointer _cb)
{
	PidginPrefsDropdownCallback cb = _cb;
	GtkTreeIter iter;
	GtkTreeModel *tree_model;
	PidginPrefValue active;

	tree_model = gtk_combo_box_get_model(combo_box);
	if (!gtk_combo_box_get_active_iter(combo_box, &iter))
		return;
	active.type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo_box),
		"type"));

	g_object_set_data(G_OBJECT(combo_box), "previously_active",
		g_object_get_data(G_OBJECT(combo_box), "current_active"));
	g_object_set_data(G_OBJECT(combo_box), "current_active",
		GINT_TO_POINTER(gtk_combo_box_get_active(combo_box)));

	if (active.type == PURPLE_PREF_INT) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&active.value.integer, -1);
	}
	else if (active.type == PURPLE_PREF_STRING) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&active.value.string, -1);
	}
	else if (active.type == PURPLE_PREF_BOOLEAN) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&active.value.boolean, -1);
	}

	cb(combo_box, active);
}

static void
pidgin_prefs_bind_dropdown_revert_active(PidginPrefCombo *combo)
{
	g_return_if_fail(combo != NULL);

	combo->current_active = combo->previously_active;

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo->combo),
	                         combo->previously_active);
}

static GtkWidget *
pidgin_prefs_dropdown_from_list_with_cb(GtkWidget *box, const gchar *title,
	GtkComboBox **dropdown_out, GList *menuitems,
	PidginPrefValue initial, PidginPrefsDropdownCallback cb)
{
	GtkWidget *dropdown;
	GtkWidget *label = NULL;
	gchar *text;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;
	GtkCellRenderer *renderer;
	gpointer current_active;

	g_return_val_if_fail(menuitems != NULL, NULL);

	if (initial.type == PURPLE_PREF_INT) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_INT);
	} else if (initial.type == PURPLE_PREF_STRING) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_STRING);
	} else if (initial.type == PURPLE_PREF_BOOLEAN) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	} else {
		g_warn_if_reached();
		return NULL;
	}

	dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	if (dropdown_out != NULL)
		*dropdown_out = GTK_COMBO_BOX(dropdown);
	g_object_set_data(G_OBJECT(dropdown), "type", GINT_TO_POINTER(initial.type));

	while (menuitems != NULL && (text = (char *)menuitems->data) != NULL) {
		int int_value  = 0;
		const char *str_value  = NULL;
		gboolean bool_value = FALSE;

		menuitems = g_list_next(menuitems);
		g_return_val_if_fail(menuitems != NULL, NULL);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   PREF_DROPDOWN_TEXT, text,
		                   -1);

		if (initial.type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, int_value,
			                   -1);
		}
		else if (initial.type == PURPLE_PREF_STRING) {
			str_value = (const char *)menuitems->data;
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, str_value,
			                   -1);
		}
		else if (initial.type == PURPLE_PREF_BOOLEAN) {
			bool_value = (gboolean)GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, bool_value,
			                   -1);
		}

		if ((initial.type == PURPLE_PREF_INT &&
			initial.value.integer == int_value) ||
			(initial.type == PURPLE_PREF_STRING &&
			purple_strequal(initial.value.string, str_value)) ||
			(initial.type == PURPLE_PREF_BOOLEAN &&
			(initial.value.boolean == bool_value))) {

			active = iter;
		}

		menuitems = g_list_next(menuitems);
	}

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dropdown), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dropdown), renderer,
	                               "text", 0,
	                               NULL);

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dropdown), &active);
	current_active = GINT_TO_POINTER(gtk_combo_box_get_active(GTK_COMBO_BOX(
		dropdown)));
	g_object_set_data(G_OBJECT(dropdown), "current_active", current_active);
	g_object_set_data(G_OBJECT(dropdown), "previously_active", current_active);

	g_signal_connect(G_OBJECT(dropdown), "changed",
		G_CALLBACK(dropdown_set), cb);

	pidgin_add_widget_to_vbox(GTK_BOX(box), title, NULL, dropdown, FALSE, &label);

	return label;
}

static void
pidgin_prefs_dropdown_from_list_cb(GtkComboBox *combo_box,
	PidginPrefValue value)
{
	const char *key;

	key = g_object_get_data(G_OBJECT(combo_box), "key");

	if (value.type == PURPLE_PREF_INT) {
		purple_prefs_set_int(key, value.value.integer);
	} else if (value.type == PURPLE_PREF_STRING) {
		purple_prefs_set_string(key, value.value.string);
	} else if (value.type == PURPLE_PREF_BOOLEAN) {
		purple_prefs_set_bool(key, value.value.boolean);
	} else {
		g_return_if_reached();
	}
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		PurplePrefType type, const char *key, GList *menuitems)
{
	PidginPrefValue initial;
	GtkComboBox *dropdown = NULL;
	GtkWidget *label;

	initial.type = type;
	if (type == PURPLE_PREF_INT) {
		initial.value.integer = purple_prefs_get_int(key);
	} else if (type == PURPLE_PREF_STRING) {
		initial.value.string = purple_prefs_get_string(key);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		initial.value.boolean = purple_prefs_get_bool(key);
	} else {
		g_return_val_if_reached(NULL);
	}

	label = pidgin_prefs_dropdown_from_list_with_cb(box, title, &dropdown,
		menuitems, initial, pidgin_prefs_dropdown_from_list_cb);

	g_object_set_data(G_OBJECT(dropdown), "key", (gpointer)key);

	return label;
}

GtkWidget *
pidgin_prefs_dropdown(GtkWidget *box, const gchar *title, PurplePrefType type,
			   const char *key, ...)
{
	va_list ap;
	GList *menuitems = NULL;
	GtkWidget *dropdown = NULL;
	char *name;
	int int_value;
	const char *str_value;

	g_return_val_if_fail(type == PURPLE_PREF_BOOLEAN || type == PURPLE_PREF_INT ||
			type == PURPLE_PREF_STRING, NULL);

	va_start(ap, key);
	while ((name = va_arg(ap, char *)) != NULL) {

		menuitems = g_list_prepend(menuitems, name);

		if (type == PURPLE_PREF_INT || type == PURPLE_PREF_BOOLEAN) {
			int_value = va_arg(ap, int);
			menuitems = g_list_prepend(menuitems, GINT_TO_POINTER(int_value));
		}
		else {
			str_value = va_arg(ap, const char *);
			menuitems = g_list_prepend(menuitems, (char *)str_value);
		}
	}
	va_end(ap);

	g_return_val_if_fail(menuitems != NULL, NULL);

	menuitems = g_list_reverse(menuitems);

	dropdown = pidgin_prefs_dropdown_from_list(box, title, type, key,
			menuitems);

	g_list_free(menuitems);

	return dropdown;
}

static void
pidgin_prefs_bind_dropdown_from_list_cb(GtkComboBox *combo_box,
	PidginPrefCombo *combo)
{
	if (combo->type == PURPLE_PREF_INT) {
		purple_prefs_set_int(combo->key, combo->value.integer);
	} else if (combo->type == PURPLE_PREF_STRING) {
		purple_prefs_set_string(combo->key, combo->value.string);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		purple_prefs_set_bool(combo->key, combo->value.boolean);
	} else {
		g_return_if_reached();
	}
}

static void
bind_dropdown_set(GtkComboBox *combo_box, gpointer data)
{
	PidginPrefCombo *combo = data;
	GtkTreeIter iter;
	GtkTreeModel *tree_model;

	tree_model = gtk_combo_box_get_model(combo_box);
	if (!gtk_combo_box_get_active_iter(combo_box, &iter))
		return;

	combo->previously_active = combo->current_active;
	combo->current_active = gtk_combo_box_get_active(combo_box);

	if (combo->type == PURPLE_PREF_INT) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&combo->value.integer, -1);
	}
	else if (combo->type == PURPLE_PREF_STRING) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&combo->value.string, -1);
	}
	else if (combo->type == PURPLE_PREF_BOOLEAN) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&combo->value.boolean, -1);
	}

	combo->cb(combo_box, combo);
}

static void
pidgin_prefs_bind_dropdown_from_list(PidginPrefCombo *combo, GList *menuitems)
{
	gchar *text;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;

	g_return_if_fail(menuitems != NULL);

	if (combo->type == PURPLE_PREF_INT) {
		combo->value.integer = purple_prefs_get_int(combo->key);
	} else if (combo->type == PURPLE_PREF_STRING) {
		combo->value.string = purple_prefs_get_string(combo->key);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		combo->value.boolean = purple_prefs_get_bool(combo->key);
	} else {
		g_return_if_reached();
	}

	store = GTK_LIST_STORE(
			gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo)));

	while (menuitems != NULL && (text = (char *)menuitems->data) != NULL) {
		int int_value = 0;
		const char *str_value = NULL;
		gboolean bool_value = FALSE;

		menuitems = g_list_next(menuitems);
		g_return_if_fail(menuitems != NULL);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   PREF_DROPDOWN_TEXT, text,
		                   -1);

		if (combo->type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, int_value,
			                   -1);
		}
		else if (combo->type == PURPLE_PREF_STRING) {
			str_value = (const char *)menuitems->data;
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, str_value,
			                   -1);
		}
		else if (combo->type == PURPLE_PREF_BOOLEAN) {
			bool_value = (gboolean)GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, bool_value,
			                   -1);
		}

		if ((combo->type == PURPLE_PREF_INT &&
			combo->value.integer == int_value) ||
			(combo->type == PURPLE_PREF_STRING &&
			purple_strequal(combo->value.string, str_value)) ||
			(combo->type == PURPLE_PREF_BOOLEAN &&
			(combo->value.boolean == bool_value))) {

			active = iter;
		}

		menuitems = g_list_next(menuitems);
	}

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo->combo), &active);
	combo->current_active = gtk_combo_box_get_active(
			GTK_COMBO_BOX(combo->combo));
	combo->previously_active = combo->current_active;

	combo->cb = pidgin_prefs_bind_dropdown_from_list_cb;
	g_signal_connect(G_OBJECT(combo->combo), "changed",
			G_CALLBACK(bind_dropdown_set), combo);
}

static void
pidgin_prefs_bind_dropdown(PidginPrefCombo *combo)
{
	GtkTreeModel *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;

	if (combo->type == PURPLE_PREF_INT) {
		combo->value.integer = purple_prefs_get_int(combo->key);
	} else if (combo->type == PURPLE_PREF_STRING) {
		combo->value.string = purple_prefs_get_string(combo->key);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		combo->value.boolean = purple_prefs_get_bool(combo->key);
	} else {
		g_return_if_reached();
	}

	store = gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo));

	if (!gtk_tree_model_get_iter_first(store, &iter)) {
		g_return_if_reached();
	}

	do {
		int int_value = 0;
		const char *str_value = NULL;
		gboolean bool_value = FALSE;

		if (combo->type == PURPLE_PREF_INT) {
			gtk_tree_model_get(store, &iter,
			                   PREF_DROPDOWN_VALUE, &int_value,
			                   -1);
			if (combo->value.integer == int_value) {
				active = iter;
				break;
			}
		}
		else if (combo->type == PURPLE_PREF_STRING) {
			gtk_tree_model_get(store, &iter,
			                   PREF_DROPDOWN_VALUE, &str_value,
			                   -1);
			if (purple_strequal(combo->value.string, str_value)) {
				active = iter;
				break;
			}
		}
		else if (combo->type == PURPLE_PREF_BOOLEAN) {
			gtk_tree_model_get(store, &iter,
			                   PREF_DROPDOWN_VALUE, &bool_value,
			                   -1);
			if (combo->value.boolean == bool_value) {
				active = iter;
				break;
			}
		}
	} while (gtk_tree_model_iter_next(store, &iter));

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo->combo), &active);

	combo->current_active = gtk_combo_box_get_active(
			GTK_COMBO_BOX(combo->combo));
	combo->previously_active = combo->current_active;

	combo->cb = pidgin_prefs_bind_dropdown_from_list_cb;
	g_signal_connect(G_OBJECT(combo->combo), "changed",
			G_CALLBACK(bind_dropdown_set), combo);
}

static void
set_bool_pref(GtkWidget *w, const char *key)
{
	purple_prefs_set_bool(key,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
}

GtkWidget *
pidgin_prefs_checkbox(const char *text, const char *key, GtkWidget *page)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			purple_prefs_get_bool(key));

	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(set_bool_pref), (char *)key);

	gtk_widget_show(button);

	return button;
}

static void
pidgin_prefs_bind_checkbox(const char *key, GtkWidget *button)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			purple_prefs_get_bool(key));
	g_signal_connect(G_OBJECT(button), "toggled",
			G_CALLBACK(set_bool_pref), (char *)key);
}

static void keyring_page_cleanup(PidginPrefsWindow *win);

static void
delete_prefs(GtkWidget *asdf, void *gdsa)
{
	/* Close any "select sound" request dialogs */
	purple_request_close_with_handle(prefs);

	purple_notify_close_with_handle(prefs);

	g_clear_object(&prefs->theme.session);

	/* Unregister callbacks. */
	purple_prefs_disconnect_by_handle(prefs);

	/* NULL-ify globals */
	sound_row_sel = 0;
	prefs_sound_themes_loading = FALSE;

	prefs_sound_themes_combo_box = NULL;
	prefs_blist_themes_combo_box = NULL;
	prefs_status_themes_combo_box = NULL;
	prefs_smiley_themes_combo_box = NULL;

	keyring_page_cleanup(prefs);

	g_free(prefs->proxy.gnome_program_path);
	g_free(prefs->browser.gnome_program_path);
	prefs = NULL;
}

static gchar *
get_theme_markup(const char *name, gboolean custom, const char *author,
				 const char *description)
{

	return g_strdup_printf("<b>%s</b>%s%s%s%s\n<span foreground='dim grey'>%s</span>",
						   name, custom ? " " : "", custom ? _("(Custom)") : "",
						   author != NULL ? " - " : "", author != NULL ? author : "",
						   description != NULL ? description : "");
}

static void
smileys_refresh_theme_list(void)
{
	GList *it;
	GtkTreeIter iter;
	gchar *description;

	description = get_theme_markup(_("none"), FALSE, _("Penguin Pimps"),
		_("Selecting this disables graphical emoticons."));
	gtk_list_store_append(prefs_smiley_themes, &iter);
	gtk_list_store_set(prefs_smiley_themes, &iter,
		0, NULL, 1, description, 2, "none", -1);
	g_free(description);

	for (it = pidgin_smiley_theme_get_all(); it; it = g_list_next(it)) {
		PidginSmileyTheme *theme = it->data;

		description = get_theme_markup(
			_(pidgin_smiley_theme_get_name(theme)), FALSE,
			_(pidgin_smiley_theme_get_author(theme)),
			_(pidgin_smiley_theme_get_description(theme)));

		gtk_list_store_append(prefs_smiley_themes, &iter);
		gtk_list_store_set(prefs_smiley_themes, &iter,
			0, pidgin_smiley_theme_get_icon(theme),
			1, description,
			2, pidgin_smiley_theme_get_name(theme),
			-1);

		g_free(description);
	}
}

/* Rebuild the markup for the sound theme selection for "(Custom)" themes */
static void
pref_sound_generate_markup(void)
{
	gboolean print_custom, customized;
	const gchar *author, *description, *current_theme;
	gchar *name, *markup;
	PurpleSoundTheme *theme;
	GtkTreeIter iter;

	customized = pidgin_sound_is_customized();
	current_theme = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(prefs_sound_themes), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &iter, 2, &name, -1);

			print_custom = customized && name && purple_strequal(current_theme, name);

			if (!name || *name == '\0') {
				g_free(name);
				name = g_strdup(_("Default"));
				author = _("Penguin Pimps");
				description = _("The default Pidgin sound theme");
			} else {
				theme = PURPLE_SOUND_THEME(purple_theme_manager_find_theme(name, "sound"));
				author = purple_theme_get_author(PURPLE_THEME(theme));
				description = purple_theme_get_description(PURPLE_THEME(theme));
			}

			markup = get_theme_markup(name, print_custom, author, description);

			gtk_list_store_set(prefs_sound_themes, &iter, 1, markup, -1);

			g_free(name);
			g_free(markup);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(prefs_sound_themes), &iter));
	}
}

/* adds the themes to the theme list from the manager so they can be displayed in prefs */
static void
prefs_themes_sort(PurpleTheme *theme)
{
	GdkPixbuf *pixbuf = NULL;
	GtkTreeIter iter;
	gchar *image_full = NULL, *markup;
	const gchar *name, *author, *description;

	if (PURPLE_IS_SOUND_THEME(theme)){

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = pidgin_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
			g_free(image_full);
		} else
			pixbuf = NULL;

		gtk_list_store_append(prefs_sound_themes, &iter);
		gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, purple_theme_get_name(theme), -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

	} else if (PIDGIN_IS_BLIST_THEME(theme) || PIDGIN_IS_STATUS_ICON_THEME(theme)){
		GtkListStore *store;

		if (PIDGIN_IS_BLIST_THEME(theme))
			store = prefs_blist_themes;
		else
			store = prefs_status_icon_themes;

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = pidgin_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
			g_free(image_full);
		} else
			pixbuf = NULL;

		name = purple_theme_get_name(theme);
		author = purple_theme_get_author(theme);
		description = purple_theme_get_description(theme);

		markup = get_theme_markup(name, FALSE, author, description);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pixbuf, 1, markup, 2, name, -1);

		g_free(markup);
		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

	}
}

static void
prefs_set_active_theme_combo(GtkWidget *combo_box, GtkListStore *store, const gchar *current_theme)
{
	GtkTreeIter iter;
	gchar *theme = NULL;
	gboolean unset = TRUE;

	if (current_theme && *current_theme && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &theme, -1);

			if (purple_strequal(current_theme, theme)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo_box), &iter);
				unset = FALSE;
			}

			g_free(theme);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
	}

	if (unset)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
}

static void
prefs_themes_refresh(void)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *tmp;
	GtkTreeIter iter;

	prefs_sound_themes_loading = TRUE;
	/* refresh the list of themes in the manager */
	purple_theme_manager_refresh();

	tmp = g_build_filename(PURPLE_DATADIR, "icons", "hicolor", "32x32",
		"apps", "im.pidgin.Pidgin3.png", NULL);
	pixbuf = pidgin_pixbuf_new_from_file_at_scale(tmp, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
	g_free(tmp);

	/* sound themes */
	gtk_list_store_clear(prefs_sound_themes);
	gtk_list_store_append(prefs_sound_themes, &iter);
	gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, "", -1);

	/* blist themes */
	gtk_list_store_clear(prefs_blist_themes);
	gtk_list_store_append(prefs_blist_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin buddy list theme"));
	gtk_list_store_set(prefs_blist_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);

	/* status icon themes */
	gtk_list_store_clear(prefs_status_icon_themes);
	gtk_list_store_append(prefs_status_icon_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin status icon theme"));
	gtk_list_store_set(prefs_status_icon_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);
	if (pixbuf)
		g_object_unref(G_OBJECT(pixbuf));

	/* smiley themes */
	gtk_list_store_clear(prefs_smiley_themes);

	purple_theme_manager_for_each_theme(prefs_themes_sort);
	pref_sound_generate_markup();
	smileys_refresh_theme_list();

	/* set active */
	prefs_set_active_theme_combo(prefs_sound_themes_combo_box, prefs_sound_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme"));
	prefs_set_active_theme_combo(prefs_blist_themes_combo_box, prefs_blist_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/theme"));
	prefs_set_active_theme_combo(prefs_status_themes_combo_box, prefs_status_icon_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/status/icon-theme"));
	prefs_set_active_theme_combo(prefs_smiley_themes_combo_box, prefs_smiley_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/smileys/theme"));
	prefs_sound_themes_loading = FALSE;
}

/* init all the theme variables so that the themes can be sorted later and used by pref pages */
static void
prefs_themes_init(void)
{
	prefs_sound_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_blist_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_status_icon_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_smiley_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
}

/*
 * prefs_theme_find_theme:
 * @path: A directory containing a theme.  The theme could be at the
 *        top level of this directory or in any subdirectory thereof.
 * @type: The type of theme to load.  The loader for this theme type
 *        will be used and this loader will determine what constitutes a
 *        "theme."
 *
 * Attempt to load the given directory as a theme.  If we are unable to
 * open the path as a theme then we recurse into path and attempt to
 * load each subdirectory that we encounter.
 *
 * Returns: A new reference to a #PurpleTheme.
 */
static PurpleTheme *
prefs_theme_find_theme(const gchar *path, const gchar *type)
{
	PurpleTheme *theme = purple_theme_manager_load_theme(path, type);
	GDir *dir = g_dir_open(path, 0, NULL);
	const gchar *next;

	while (!PURPLE_IS_THEME(theme) && (next = g_dir_read_name(dir))) {
		gchar *next_path = g_build_filename(path, next, NULL);

		if (g_file_test(next_path, G_FILE_TEST_IS_DIR))
			theme = prefs_theme_find_theme(next_path, type);

		g_free(next_path);
	}

	g_dir_close(dir);

	return theme;
}

/* Eww. Seriously ewww. But thanks, grim! This is taken from guifications2 */
static gboolean
purple_theme_file_copy(const gchar *source, const gchar *destination)
{
	FILE *src, *dest;
	gint chr = EOF;

	if(!(src = g_fopen(source, "rb")))
		return FALSE;
	if(!(dest = g_fopen(destination, "wb"))) {
		fclose(src);
		return FALSE;
	}

	while((chr = fgetc(src)) != EOF) {
		fputc(chr, dest);
	}

	fclose(dest);
	fclose(src);

	return TRUE;
}

static void
free_theme_info(struct theme_info *info)
{
	if (info != NULL) {
		g_free(info->type);
		g_free(info->extension);
		g_free(info->original_name);
		g_free(info);
	}
}

/* installs a theme, info is freed by function */
static void
theme_install_theme(char *path, struct theme_info *info)
{
	gchar *destdir;
	const char *tail;
	gboolean is_smiley_theme, is_archive;
	PurpleTheme *theme = NULL;

	if (info == NULL)
		return;

	/* check the extension */
	tail = info->extension ? info->extension : strrchr(path, '.');

	if (!tail) {
		free_theme_info(info);
		return;
	}

	is_archive = !g_ascii_strcasecmp(tail, ".gz") || !g_ascii_strcasecmp(tail, ".tgz");

	/* Just to be safe */
	g_strchomp(path);

	if ((is_smiley_theme = purple_strequal(info->type, "smiley")))
		destdir = g_build_filename(purple_data_dir(), "smileys", NULL);
	else
		destdir = g_build_filename(purple_data_dir(), "themes", "temp", NULL);

	/* We'll check this just to make sure. This also lets us do something different on
	 * other platforms, if need be */
	if (is_archive) {
#ifndef _WIN32
		gchar *path_escaped = g_shell_quote(path);
		gchar *destdir_escaped = g_shell_quote(destdir);
		gchar *command;

		if (!g_file_test(destdir, G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents(destdir, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		command = g_strdup_printf("tar > /dev/null xzf %s -C %s", path_escaped, destdir_escaped);
		g_free(path_escaped);
		g_free(destdir_escaped);

		/* Fire! */
		if (system(command)) {
			purple_notify_error(NULL, NULL, _("Theme failed to unpack."), NULL, NULL);
			g_free(command);
			g_free(destdir);
			free_theme_info(info);
			return;
		}
		g_free(command);
#else
		if (!winpidgin_gz_untar(path, destdir)) {
			purple_notify_error(NULL, NULL, _("Theme failed to unpack."), NULL, NULL);
			g_free(destdir);
			free_theme_info(info);
			return;
		}
#endif
	}

	if (is_smiley_theme) {
		/* just extract the folder to the smiley directory */
		prefs_themes_refresh();

	} else if (is_archive) {
		theme = prefs_theme_find_theme(destdir, info->type);

		if (PURPLE_IS_THEME(theme)) {
			/* create the location for the theme */
			gchar *theme_dest = g_build_filename(purple_data_dir(), "themes",
			                                     purple_theme_get_name(theme),
			                                     "purple", info->type, NULL);

			if (!g_file_test(theme_dest, G_FILE_TEST_IS_DIR)) {
				g_mkdir_with_parents(theme_dest, S_IRUSR | S_IWUSR | S_IXUSR);
			}

			g_free(theme_dest);
			theme_dest = g_build_filename(purple_data_dir(), "themes",
			                              purple_theme_get_name(theme),
			                              "purple", info->type, NULL);

			/* move the entire directory to new location */
			if (g_rename(purple_theme_get_dir(theme), theme_dest)) {
				purple_debug_error("gtkprefs", "Error renaming %s to %s: "
						"%s\n", purple_theme_get_dir(theme), theme_dest,
						g_strerror(errno));
			}

			g_free(theme_dest);
			if (g_remove(destdir) != 0) {
				purple_debug_error("gtkprefs",
					"couldn't remove temp (dest) path\n");
			}
			g_object_unref(theme);

			prefs_themes_refresh();

		} else {
			/* something was wrong with the theme archive */
			g_unlink(destdir);
			purple_notify_error(NULL, NULL, _("Theme failed to load."), NULL, NULL);
		}

	} else { /* just a single file so copy it to a new temp directory and attempt to load it*/
		gchar *temp_path, *temp_file;

		temp_path = g_build_filename(purple_data_dir(), "themes", "temp",
		                             "sub_folder", NULL);

		if (info->original_name != NULL) {
			/* name was changed from the original (probably a dnd) change it back before loading */
			temp_file = g_build_filename(temp_path, info->original_name, NULL);

		} else {
			gchar *source_name = g_path_get_basename(path);
			temp_file = g_build_filename(temp_path, source_name, NULL);
			g_free(source_name);
		}

		if (!g_file_test(temp_path, G_FILE_TEST_IS_DIR)) {
			g_mkdir_with_parents(temp_path, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		if (purple_theme_file_copy(path, temp_file)) {
			/* find the theme, could be in subfolder */
			theme = prefs_theme_find_theme(temp_path, info->type);

			if (PURPLE_IS_THEME(theme)) {
				gchar *theme_dest =
				        g_build_filename(purple_data_dir(), "themes",
				                         purple_theme_get_name(theme), "purple",
				                         info->type, NULL);

				if(!g_file_test(theme_dest, G_FILE_TEST_IS_DIR)) {
					g_mkdir_with_parents(theme_dest, S_IRUSR | S_IWUSR | S_IXUSR);
				}

				if (g_rename(purple_theme_get_dir(theme), theme_dest)) {
					purple_debug_error("gtkprefs", "Error renaming %s to %s: "
							"%s\n", purple_theme_get_dir(theme), theme_dest,
							g_strerror(errno));
				}

				g_free(theme_dest);
				g_object_unref(theme);

				prefs_themes_refresh();
			} else {
				if (g_remove(temp_path) != 0) {
					purple_debug_error("gtkprefs",
						"couldn't remove temp path");
				}
				purple_notify_error(NULL, NULL, _("Theme failed to load."), NULL, NULL);
			}
		} else {
			purple_notify_error(NULL, NULL, _("Theme failed to copy."), NULL, NULL);
		}

		g_free(temp_file);
		g_free(temp_path);
	}

	g_free(destdir);
	free_theme_info(info);
}

static void
theme_got_url(G_GNUC_UNUSED SoupSession *session, SoupMessage *msg,
              gpointer _info)
{
	struct theme_info *info = _info;
	FILE *f;
	gchar *path;
	size_t wc;

	if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		free_theme_info(info);
		return;
	}

	f = purple_mkstemp(&path, TRUE);
	wc = fwrite(msg->response_body->data, msg->response_body->length, 1, f);
	if (wc != 1) {
		purple_debug_warning("theme_got_url", "Unable to write theme data.\n");
		fclose(f);
		g_unlink(path);
		g_free(path);
		free_theme_info(info);
		return;
	}
	fclose(f);

	theme_install_theme(path, info);

	g_unlink(path);
	g_free(path);
}

static void
theme_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
		GtkSelectionData *sd, guint info, guint t, gpointer user_data)
{
	gchar *name = g_strchomp((gchar *)gtk_selection_data_get_data(sd));

	if ((gtk_selection_data_get_length(sd) >= 0)
	 && (gtk_selection_data_get_format(sd) == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */
		gchar *temp;
		struct theme_info *info =  g_new0(struct theme_info, 1);
		info->type = g_strdup((gchar *)user_data);
		info->extension = g_strdup(g_strrstr(name,"."));
		temp = g_strrstr(name, "/");
		info->original_name = temp ? g_strdup(++temp) : NULL;

		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp;
			/* It looks like we're dealing with a local file. Let's
			 * just untar it in the right place */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug(PURPLE_DEBUG_ERROR, "theme dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				free_theme_info(info);
				return;
			}
			theme_install_theme(tmp, info);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7) ||
			!g_ascii_strncasecmp(name, "https://", 8)) {
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			SoupMessage *msg;

			if (prefs->theme.session == NULL) {
				prefs->theme.session = soup_session_new();
			}

			soup_session_abort(prefs->theme.session);

			msg = soup_message_new("GET", name);
			// purple_http_request_set_max_len(msg, PREFS_MAX_DOWNLOADED_THEME_SIZE);
			soup_session_queue_message(prefs->theme.session, msg, theme_got_url,
			                           info);
		} else
			free_theme_info(info);

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}

/* builds a theme combo box from a list store with colums: icon preview, markup, theme name */
static void
prefs_build_theme_combo_box(GtkWidget *combo_box, GtkListStore *store,
                            const char *current_theme, const char *type)
{
	GtkTargetEntry te[3] = {
		{"text/plain", 0, 0},
		{"text/uri-list", 0, 1},
		{"STRING", 0, 2}
	};

	g_return_if_fail(store != NULL && current_theme != NULL);

	gtk_combo_box_set_model(GTK_COMBO_BOX(combo_box),
	                        GTK_TREE_MODEL(store));

	gtk_drag_dest_set(combo_box, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, te,
					sizeof(te) / sizeof(GtkTargetEntry) , GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(combo_box), "drag_data_received", G_CALLBACK(theme_dnd_recv), (gpointer) type);
}

/* sets the current sound theme */
static void
prefs_set_sound_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(user_data);
	gint i;
	gchar *pref;
	gchar *new_theme;
	GtkTreeIter new_iter;

	if(gtk_combo_box_get_active_iter(combo_box, &new_iter) && !prefs_sound_themes_loading) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &new_iter, 2, &new_theme, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/sound/theme", new_theme);

		/* New theme removes all customization */
		for(i = 0; i < PURPLE_NUM_SOUNDS; i++){
			pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						pidgin_sound_get_event_option(i));
			purple_prefs_set_path(pref, "");
			g_free(pref);
		}

		/* gets rid of the "(Custom)" from the last selection */
		pref_sound_generate_markup();

		gtk_entry_set_text(GTK_ENTRY(win->sound.entry), _("(default)"));

		g_free(new_theme);
	}
}

/* sets the current smiley theme */
static void
prefs_set_smiley_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	gchar *new_theme;
	GtkTreeIter new_iter;

	if (gtk_combo_box_get_active_iter(combo_box, &new_iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_smiley_themes), &new_iter, 2, &new_theme, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/smileys/theme", new_theme);

		g_free(new_theme);
	}
}


/* Does same as normal sort, except "none" is sorted first */
static gint pidgin_sort_smileys (GtkTreeModel	*model,
						GtkTreeIter		*a,
						GtkTreeIter		*b,
						gpointer		userdata)
{
	gint ret = 0;
	gchar *name1 = NULL, *name2 = NULL;

	gtk_tree_model_get(model, a, 2, &name1, -1);
	gtk_tree_model_get(model, b, 2, &name2, -1);

	if (name1 == NULL || name2 == NULL) {
		if (!(name1 == NULL && name2 == NULL))
			ret = (name1 == NULL) ? -1: 1;
	} else if (!g_ascii_strcasecmp(name1, "none")) {
		if (!g_utf8_collate(name1, name2))
			ret = 0;
		else
			/* Sort name1 first */
			ret = -1;
	} else if (!g_ascii_strcasecmp(name2, "none")) {
		/* Sort name2 first */
		ret = 1;
	} else {
		/* Neither string is "none", default to normal sort */
		ret = purple_utf8_strcasecmp(name1, name2);
	}

	g_free(name1);
	g_free(name2);

	return ret;
}

/* sets the current buddy list theme */
static void
prefs_set_blist_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginBlistTheme *theme =  NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if(gtk_combo_box_get_active_iter(combo_box, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_blist_themes), &iter, 2, &name, -1);

		if(!name || *name)
			theme = PIDGIN_BLIST_THEME(purple_theme_manager_find_theme(name, "blist"));

		g_free(name);

		pidgin_blist_set_theme(theme);
	}
}

/* sets the current icon theme */
static void
prefs_set_status_icon_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginStatusIconTheme *theme = NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if(gtk_combo_box_get_active_iter(combo_box, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_status_icon_themes), &iter, 2, &name, -1);

		if(!name || *name)
			theme = PIDGIN_STATUS_ICON_THEME(purple_theme_manager_find_theme(name, "status-icon"));

		g_free(name);

		pidgin_stock_load_status_icon_theme(theme);
		pidgin_blist_refresh(purple_blist_get_default());
	}
}

static void
bind_theme_page(PidginPrefsWindow *win)
{
	/* Buddy List Themes */
	prefs_build_theme_combo_box(win->theme.blist, prefs_blist_themes,
	                            PIDGIN_PREFS_ROOT "/blist/theme", "blist");
	prefs_blist_themes_combo_box = win->theme.blist;

	/* Status Icon Themes */
	prefs_build_theme_combo_box(win->theme.status, prefs_status_icon_themes,
	                            PIDGIN_PREFS_ROOT "/status/icon-theme",
	                            "icon");
	prefs_status_themes_combo_box = win->theme.status;

	/* Sound Themes */
	prefs_build_theme_combo_box(win->theme.sound, prefs_sound_themes,
	                            PIDGIN_PREFS_ROOT "/sound/theme", "sound");
	prefs_sound_themes_combo_box = win->theme.sound;

	/* Smiley Themes */
	prefs_build_theme_combo_box(win->theme.smiley, prefs_smiley_themes,
	                            PIDGIN_PREFS_ROOT "/smileys/theme",
	                            "smiley");
	prefs_smiley_themes_combo_box = win->theme.smiley;

	/* Custom sort so "none" theme is at top of list */
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(prefs_smiley_themes),
	                                2, pidgin_sort_smileys, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(prefs_smiley_themes),
										 2, GTK_SORT_ASCENDING);
}

static void
formatting_toggle_cb(TalkatuActionGroup *ag, GAction *action, const gchar *name, gpointer data)
{
	gboolean activated = talkatu_action_group_get_action_activated(ag, name);
	if(g_ascii_strcasecmp(TALKATU_ACTION_FORMAT_BOLD, name) != 0) {
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold",
		                      activated);
	} else if(g_ascii_strcasecmp(TALKATU_ACTION_FORMAT_ITALIC, name) != 0) {
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic",
		                      activated);
	} else if(g_ascii_strcasecmp(TALKATU_ACTION_FORMAT_UNDERLINE, name) != 0) {
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline",
		                      activated);
	} else if(g_ascii_strcasecmp(TALKATU_ACTION_FORMAT_STRIKETHROUGH, name) != 0) {
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike",
		                      activated);
	}
}

static void
bind_interface_page(PidginPrefsWindow *win)
{
	GList *names = NULL;

	/* System Tray */
	win->iface.docklet.type = PURPLE_PREF_STRING;
	win->iface.docklet.key = PIDGIN_PREFS_ROOT "/docklet/show";
	pidgin_prefs_bind_dropdown(&win->iface.docklet);

	win->iface.im.hide_new.type = PURPLE_PREF_STRING;
	win->iface.im.hide_new.key = PIDGIN_PREFS_ROOT "/conversations/im/hide_new";
	pidgin_prefs_bind_dropdown(&win->iface.im.hide_new);

#ifdef _WIN32
	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/win32/minimize_new_convs",
			win->iface.win32.minimize_new_convs);
#else
	gtk_widget_hide(win->iface.win32.minimize_new_convs);
#endif

	/* All the tab options! */
	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/tabs",
			win->iface.conversations.tabs);

	/*
	 * Connect a signal to the above preference.  When conversations are not
	 * shown in a tabbed window then all tabbing options should be disabled.
	 */
	g_object_bind_property(win->iface.conversations.tabs, "active",
			win->iface.conversations.tabs_vbox, "sensitive",
			G_BINDING_SYNC_CREATE);

	pidgin_prefs_bind_checkbox(
			PIDGIN_PREFS_ROOT "/conversations/close_on_tabs",
			win->iface.conversations.close_on_tabs);

	win->iface.conversations.tab_side.type = PURPLE_PREF_INT;
	win->iface.conversations.tab_side.key = PIDGIN_PREFS_ROOT "/conversations/tab_side";
	pidgin_prefs_bind_dropdown(&win->iface.conversations.tab_side);

	win->iface.conversations.placement.type = PURPLE_PREF_STRING;
	win->iface.conversations.placement.key = PIDGIN_PREFS_ROOT "/conversations/placement";
	names = pidgin_conv_placement_get_options();
	pidgin_prefs_bind_dropdown_from_list(
			&win->iface.conversations.placement,
			names);
	g_list_free(names);
}

/* This is also Win32-specific, but must be visible for Glade binding. */
static void
apply_custom_font(GtkWidget *unused, PidginPrefsWindow *win)
{
	PangoFontDescription *desc = NULL;
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font")) {
		const char *font = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
		desc = pango_font_description_from_string(font);
	}

	gtk_widget_override_font(win->conversations.format_view, desc);
	if (desc)
		pango_font_description_free(desc);

}

static void
pidgin_custom_font_set(GtkWidget *font_button, PidginPrefsWindow *win)
{

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/custom_font",
			gtk_font_chooser_get_font(GTK_FONT_CHOOSER(font_button)));

	apply_custom_font(font_button, win);
}

static void
bind_conv_page(PidginPrefsWindow *win)
{
	GSimpleActionGroup *ag = NULL;

	win->conversations.notification_chat.type = PURPLE_PREF_INT;
	win->conversations.notification_chat.key = PIDGIN_PREFS_ROOT "/conversations/notification_chat";
	pidgin_prefs_bind_dropdown(&win->conversations.notification_chat);

	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting",
			win->conversations.show_incoming_formatting);
	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/im/close_immediately",
			win->conversations.im.close_immediately);

	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons",
			win->conversations.im.show_buddy_icons);
	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons",
			win->conversations.im.animate_buddy_icons);
	g_object_bind_property(win->conversations.im.show_buddy_icons, "active",
			win->conversations.im.animate_buddy_icons, "sensitive",
			G_BINDING_SYNC_CREATE);

	pidgin_prefs_bind_checkbox("/purple/conversations/im/send_typing",
			win->conversations.im.send_typing);
	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/spellcheck",
			win->conversations.spellcheck);

	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling",
			win->conversations.use_smooth_scrolling);

#ifdef _WIN32
	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/win32/blink_im",
			win->conversations.win32.blink_im);
#else
	gtk_widget_hide(win->conversations.win32.blink_im);
#endif

#if 0
	/* TODO: it's not implemented */
	pidgin_prefs_bind_checkbox(
			PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys",
			win->conversations.resize_custom_smileys);

	pidgin_prefs_bind_spin_button(
			PIDGIN_PREFS_ROOT "/conversations/custom_smileys_size",
			win->conversations.custom_smileys_size);

	g_object_bind_property(win->conversations.resize_custom_smileys, "active",
			win->conversations.custom_smileys_size, "sensitive",
			G_BINDING_SYNC_CREATE);
#endif

	pidgin_prefs_bind_spin_button(
		PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines",
		win->conversations.minimum_entry_lines);

#ifdef _WIN32
	{
	const char *font_name;
	gtk_widget_show(win->conversations.font_frame);

	pidgin_prefs_bind_checkbox(
			PIDGIN_PREFS_ROOT "/conversations/use_theme_font",
			win->conversations.use_theme_font);

	font_name = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
	if (font_name != NULL && *font_name != '\0') {
		gtk_font_chooser_set_font(
				GTK_FONT_CHOOSER(win->conversations.custom_font),
				font_name);
	}

	g_object_bind_property(win->conversations.use_theme_font, "active",
			win->conversations.custom_font_hbox, "sensitive",
			G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
	}
#endif

	ag = talkatu_buffer_get_action_group(TALKATU_BUFFER(win->conversations.format_buffer));
	g_signal_connect_after(G_OBJECT(ag), "action-activated",
	                       G_CALLBACK(formatting_toggle_cb), NULL);
}

static void
network_ip_changed(GtkEntry *entry, gpointer data)
{
	const gchar *text = gtk_entry_get_text(entry);
	GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(entry));

	if (text && *text) {
		if (g_hostname_is_ip_address(text)) {
			purple_network_set_public_ip(text);
			gtk_style_context_add_class(context, "good-ip");
			gtk_style_context_remove_class(context, "bad-ip");
		} else {
			gtk_style_context_add_class(context, "bad-ip");
			gtk_style_context_remove_class(context, "good-ip");
		}

	} else {
		purple_network_set_public_ip("");
		gtk_style_context_remove_class(context, "bad-ip");
		gtk_style_context_remove_class(context, "good-ip");
	}
}

static gboolean
network_stun_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/stun_server",
		gtk_entry_get_text(entry));
	purple_network_set_stun_server(gtk_entry_get_text(entry));

	return FALSE;
}

static gboolean
network_turn_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/turn_server",
		gtk_entry_get_text(entry));
	purple_network_set_turn_server(gtk_entry_get_text(entry));

	return FALSE;
}

static void
proxy_changed_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	PidginPrefsWindow *win = data;
	const char *proxy = value;

	if (!purple_strequal(proxy, "none") && !purple_strequal(proxy, "envvar"))
		gtk_widget_show_all(win->proxy.options);
	else
		gtk_widget_hide(win->proxy.options);
}

static void
proxy_print_option(GtkWidget *entry, PidginPrefsWindow *win)
{
	if (entry == win->proxy.host) {
		purple_prefs_set_string("/purple/proxy/host",
				gtk_entry_get_text(GTK_ENTRY(entry)));
	} else if (entry == win->proxy.port) {
		purple_prefs_set_int("/purple/proxy/port",
				gtk_spin_button_get_value_as_int(
					GTK_SPIN_BUTTON(entry)));
	} else if (entry == win->proxy.username) {
		purple_prefs_set_string("/purple/proxy/username",
				gtk_entry_get_text(GTK_ENTRY(entry)));
	} else if (entry == win->proxy.password) {
		purple_prefs_set_string("/purple/proxy/password",
				gtk_entry_get_text(GTK_ENTRY(entry)));
	}
}

static void
proxy_button_clicked_cb(GtkWidget *button, PidginPrefsWindow *win)
{
	GError *err = NULL;

	if (g_spawn_command_line_async(win->proxy.gnome_program_path, &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start proxy configuration program."), err->message, NULL);
	g_error_free(err);
}

static void
browser_button_clicked_cb(GtkWidget *button, PidginPrefsWindow *win)
{
	GError *err = NULL;

	if (g_spawn_command_line_async(win->browser.gnome_program_path, &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start browser configuration program."), err->message, NULL);
	g_error_free(err);
}

static void
auto_ip_button_clicked_cb(GtkWidget *button, gpointer null)
{
	const char *ip;
	PurpleStunNatDiscovery *stun;
	char *auto_ip_text;

	/* purple_network_get_my_ip will return the IP that was set by the user with
	   purple_network_set_public_ip, so make a lookup for the auto-detected IP
	   ourselves. */

	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		/* Check if STUN discovery was already done */
		stun = purple_stun_discover(NULL);
		if ((stun != NULL) && (stun->status == PURPLE_STUN_STATUS_DISCOVERED)) {
			ip = stun->publicip;
		} else {
			/* Attempt to get the IP from a NAT device using UPnP */
			ip = purple_upnp_get_public_ip();
			if (ip == NULL) {
				/* Attempt to get the IP from a NAT device using NAT-PMP */
				ip = purple_pmp_get_public_ip();
				if (ip == NULL) {
					/* Just fetch the IP of the local system */
					ip = purple_network_get_local_system_ip(-1);
				}
			}
		}
	}
	else
		ip = _("Disabled");

	auto_ip_text = g_strdup_printf(_("Use _automatically detected IP address: %s"), ip);
	gtk_button_set_label(GTK_BUTTON(button), auto_ip_text);
	g_free(auto_ip_text);
}

static void
bind_network_page(PidginPrefsWindow *win)
{
	GtkStyleContext *context;
	GtkCssProvider *ip_css;
	const gchar ip_style[] =
		".bad-ip {"
			"color: @error_fg_color;"
			"text-shadow: 0 1px @error_text_shadow;"
			"background-image: none;"
			"background-color: @error_bg_color;"
		"}"
		".good-ip {"
			"color: @question_fg_color;"
			"text-shadow: 0 1px @question_text_shadow;"
			"background-image: none;"
			"background-color: @success_color;"
		"}";

	gtk_entry_set_text(GTK_ENTRY(win->network.stun_server),
			purple_prefs_get_string("/purple/network/stun_server"));

	pidgin_prefs_bind_checkbox("/purple/network/auto_ip",
			win->network.auto_ip);
	auto_ip_button_clicked_cb(win->network.auto_ip, NULL); /* Update label */

	gtk_entry_set_text(GTK_ENTRY(win->network.public_ip),
			purple_network_get_public_ip());

	ip_css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(ip_css, ip_style, -1, NULL);
	context = gtk_widget_get_style_context(win->network.public_ip);
	gtk_style_context_add_provider(context,
	                               GTK_STYLE_PROVIDER(ip_css),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_object_bind_property(win->network.auto_ip, "active",
			win->network.public_ip_hbox, "sensitive",
			G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);

	pidgin_prefs_bind_checkbox("/purple/network/map_ports",
			win->network.map_ports);

	pidgin_prefs_bind_checkbox("/purple/network/ports_range_use",
			win->network.ports_range_use);
	g_object_bind_property(win->network.ports_range_use, "active",
			win->network.ports_range_hbox, "sensitive",
			G_BINDING_SYNC_CREATE);

	pidgin_prefs_bind_spin_button("/purple/network/ports_range_start",
			win->network.ports_range_start);
	pidgin_prefs_bind_spin_button("/purple/network/ports_range_end",
			win->network.ports_range_end);

	/* TURN server */
	gtk_entry_set_text(GTK_ENTRY(win->network.turn_server),
			purple_prefs_get_string("/purple/network/turn_server"));

	pidgin_prefs_bind_spin_button("/purple/network/turn_port",
			win->network.turn_port_udp);

	pidgin_prefs_bind_spin_button("/purple/network/turn_port_tcp",
			win->network.turn_port_tcp);

	pidgin_prefs_bind_entry("/purple/network/turn_username",
			win->network.turn_username);
	pidgin_prefs_bind_entry("/purple/network/turn_password",
			win->network.turn_password);
}

static gboolean
manual_browser_set(GtkWidget *entry, GdkEventFocus *event, gpointer data)
{
	const char *program = gtk_entry_get_text(GTK_ENTRY(entry));

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", program);

	/* carry on normally */
	return FALSE;
}

#ifndef _WIN32
static GList *
get_available_browsers(void)
{
	struct browser {
		char *name;
		char *command;
	};

	/* Sorted reverse alphabetically */
	static const struct browser possible_browsers[] = {
		{N_("Seamonkey"), "seamonkey"},
		{N_("Opera"), "opera"},
		{N_("Mozilla"), "mozilla"},
		{N_("Konqueror"), "kfmclient"},
		{N_("Google Chrome"), "google-chrome"},
		/* Do not move the line below.  Code below expects gnome-open to be in
		 * this list immediately after xdg-open! */
		{N_("Desktop Default"), "xdg-open"},
		{N_("GNOME Default"), "gnome-open"},
		{N_("Galeon"), "galeon"},
		{N_("Firefox"), "firefox"},
		{N_("Firebird"), "mozilla-firebird"},
		{N_("Epiphany"), "epiphany"},
		/* Translators: please do not translate "chromium-browser" here! */
		{N_("Chromium (chromium-browser)"), "chromium-browser"},
		/* Translators: please do not translate "chrome" here! */
		{N_("Chromium (chrome)"), "chrome"}
	};
	static const int num_possible_browsers = G_N_ELEMENTS(possible_browsers);

	GList *browsers = NULL;
	int i = 0;
	char *browser_setting = (char *)purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser");

	browsers = g_list_prepend(browsers, (gpointer)"custom");
	browsers = g_list_prepend(browsers, (gpointer)_("Manual"));

	for (i = 0; i < num_possible_browsers; i++) {
		if (purple_program_is_valid(possible_browsers[i].command)) {
			browsers = g_list_prepend(browsers,
									  possible_browsers[i].command);
			browsers = g_list_prepend(browsers, (gpointer)_(possible_browsers[i].name));
			if(browser_setting && purple_strequal(possible_browsers[i].command, browser_setting))
				browser_setting = NULL;
			/* If xdg-open is valid, prefer it over gnome-open and skip forward */
			if(purple_strequal(possible_browsers[i].command, "xdg-open")) {
				if (purple_strequal("gnome-open", browser_setting)) {
					purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", possible_browsers[i].command);
					browser_setting = NULL;
				}
				i++;
			}
		}
	}

	if(browser_setting)
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", "custom");

	return browsers;
}

static void
browser_changed1_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, !purple_strequal(browser, "custom"));
}

static void
browser_changed2_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, purple_strequal(browser, "custom"));
}
#endif /* _WIN32 */

static void
bind_browser_page(PidginPrefsWindow *win)
{
#ifdef _WIN32
	/* We use the registered default browser in windows */
	gtk_widget_hide(win->browser.page);
	return;
#else
	/* if the user is running Mac OS X, hide the browsers tab */
	if (purple_running_osx()) {
		gtk_widget_hide(win->browser.page);
	} else if (purple_running_gnome()) {
		gchar *path;

		gtk_stack_set_visible_child_name(GTK_STACK(win->browser.stack),
				"gnome");

		path = g_find_program_in_path("gnome-control-center");
		if (path != NULL) {
			gchar *tmp = g_strdup_printf("%s info", path);
			g_free(path);
			path = tmp;
		} else {
			path = g_find_program_in_path("gnome-default-applications-properties");
		}

		win->browser.gnome_program_path = path;
		gtk_widget_set_visible(win->browser.gnome_not_found,
				path == NULL);
		gtk_widget_set_visible(win->browser.gnome_program,
				path != NULL);
	} else {
		GList *browsers = NULL;

		gtk_stack_set_visible_child_name(GTK_STACK(win->browser.stack),
				"nongnome");

		win->browser.browser.type = PURPLE_PREF_STRING;
		win->browser.browser.key = PIDGIN_PREFS_ROOT "/browsers/browser";
		browsers = get_available_browsers();
		pidgin_prefs_bind_dropdown_from_list(
				&win->browser.browser,
				browsers);
		g_list_free(browsers);

		win->browser.place.type = PURPLE_PREF_INT;
		win->browser.place.key = PIDGIN_PREFS_ROOT "/browsers/place";
		pidgin_prefs_bind_dropdown(&win->browser.place);

		purple_prefs_connect_callback(prefs,
				PIDGIN_PREFS_ROOT "/browsers/browser",
				browser_changed1_cb,
				win->browser.place_hbox);

		gtk_entry_set_text(GTK_ENTRY(win->browser.manual_command),
				purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/manual_command"));
		purple_prefs_connect_callback(prefs,
				PIDGIN_PREFS_ROOT "/browsers/browser",
				browser_changed2_cb,
				win->browser.manual_command_hbox);

		if (purple_strequal(
				purple_prefs_get_string(
					PIDGIN_PREFS_ROOT "/browsers/browser"),
				"custom")) {
			gtk_widget_set_sensitive(win->browser.place_hbox,
					FALSE);
		} else {
			gtk_widget_set_sensitive(win->browser.manual_command_hbox,
					FALSE);
		}
	}
#endif /* _WIN32 */
}

static void
bind_proxy_page(PidginPrefsWindow *win)
{
	PurpleProxyInfo *proxy_info;

	if(purple_running_gnome()) {
		gchar *path = NULL;

		gtk_stack_set_visible_child_name(GTK_STACK(win->proxy.stack),
				"gnome");

		path = g_find_program_in_path("gnome-network-properties");
		if (path == NULL)
			path = g_find_program_in_path("gnome-network-preferences");
		if (path == NULL) {
			path = g_find_program_in_path("gnome-control-center");
			if (path != NULL) {
				char *tmp = g_strdup_printf("%s network", path);
				g_free(path);
				path = tmp;
			}
		}

		win->proxy.gnome_program_path = path;
		gtk_widget_set_visible(win->proxy.gnome_not_found,
				path == NULL);
		gtk_widget_set_visible(win->proxy.gnome_program,
				path != NULL);
	} else {
		gtk_stack_set_visible_child_name(GTK_STACK(win->proxy.stack),
				"nongnome");

		/* This is a global option that affects SOCKS4 usage even with
		 * account-specific proxy settings */
		pidgin_prefs_bind_checkbox("/purple/proxy/socks4_remotedns",
				win->proxy.socks4_remotedns);

		win->proxy.type.type = PURPLE_PREF_STRING;
		win->proxy.type.key = "/purple/proxy/type";
		pidgin_prefs_bind_dropdown(&win->proxy.type);
		proxy_info = purple_global_proxy_get_info();

		purple_prefs_connect_callback(prefs, "/purple/proxy/type",
				proxy_changed_cb, win);

		if (proxy_info != NULL) {
			if (purple_proxy_info_get_host(proxy_info)) {
				gtk_entry_set_text(GTK_ENTRY(win->proxy.host),
						purple_proxy_info_get_host(proxy_info));
			}

			if (purple_proxy_info_get_port(proxy_info) != 0) {
				gtk_spin_button_set_value(
						GTK_SPIN_BUTTON(win->proxy.port),
						purple_proxy_info_get_port(proxy_info));
			}

			if (purple_proxy_info_get_username(proxy_info) != NULL) {
				gtk_entry_set_text(GTK_ENTRY(win->proxy.username),
						purple_proxy_info_get_username(proxy_info));
			}

			if (purple_proxy_info_get_password(proxy_info) != NULL) {
				gtk_entry_set_text(GTK_ENTRY(win->proxy.password),
						purple_proxy_info_get_password(proxy_info));
			}
		}

		proxy_changed_cb("/purple/proxy/type", PURPLE_PREF_STRING,
			purple_prefs_get_string("/purple/proxy/type"),
			win);
	}
}

static void
bind_logging_page(PidginPrefsWindow *win)
{
	GList *names;

	win->logging.format.type = PURPLE_PREF_STRING;
	win->logging.format.key = "/purple/logging/format";
	names = purple_log_logger_get_options();
	pidgin_prefs_bind_dropdown_from_list(&win->logging.format, names);
	g_list_free(names);

	pidgin_prefs_bind_checkbox("/purple/logging/log_ims",
			win->logging.log_ims);
	pidgin_prefs_bind_checkbox("/purple/logging/log_chats",
			win->logging.log_chats);
	pidgin_prefs_bind_checkbox("/purple/logging/log_system",
			win->logging.log_system);
}

/*** keyring page *******************************************************/

static void
keyring_page_settings_changed(GtkWidget *widget, gpointer _setting)
{
	PurpleRequestField *setting = _setting;
	PurpleRequestFieldType field_type;

	gtk_widget_set_sensitive(prefs->keyring.apply, TRUE);

	field_type = purple_request_field_get_field_type(setting);

	if (field_type == PURPLE_REQUEST_FIELD_BOOLEAN) {
		purple_request_field_bool_set_value(setting,
			gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(widget)));
	} else if (field_type == PURPLE_REQUEST_FIELD_STRING) {
		purple_request_field_string_set_value(setting,
			gtk_entry_get_text(GTK_ENTRY(widget)));
	} else if (field_type == PURPLE_REQUEST_FIELD_INTEGER) {
		purple_request_field_int_set_value(setting,
			gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(widget)));
	} else
		g_return_if_reached();
}

static void
keyring_page_add_settings_field(GtkBox *vbox, PurpleRequestField *setting,
	GtkSizeGroup *sg)
{
	GtkWidget *widget;
	PurpleRequestFieldType field_type;
	const gchar *label;

	label = purple_request_field_get_label(setting);

	field_type = purple_request_field_get_field_type(setting);
	if (field_type == PURPLE_REQUEST_FIELD_BOOLEAN) {
		widget = gtk_check_button_new_with_label(label);
		label = NULL;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			purple_request_field_bool_get_value(setting));
		g_signal_connect(G_OBJECT(widget), "toggled",
			G_CALLBACK(keyring_page_settings_changed), setting);
	} else if (field_type == PURPLE_REQUEST_FIELD_STRING) {
		widget = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(widget),
			purple_request_field_string_get_value(setting));
		if (purple_request_field_string_is_masked(setting))
			gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
		g_signal_connect(G_OBJECT(widget), "changed",
			G_CALLBACK(keyring_page_settings_changed), setting);
	} else if (field_type == PURPLE_REQUEST_FIELD_INTEGER) {
		widget = gtk_spin_button_new_with_range(
			purple_request_field_int_get_lower_bound(setting),
			purple_request_field_int_get_upper_bound(setting), 1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			purple_request_field_int_get_value(setting));
		g_signal_connect(G_OBJECT(widget), "value-changed",
			G_CALLBACK(keyring_page_settings_changed), setting);
	} else {
		purple_debug_error("gtkprefs", "Unsupported field type\n");
		return;
	}

	pidgin_add_widget_to_vbox(vbox, label, sg, widget, FALSE, NULL);
}

/* XXX: it could be available for all plugins, not keyrings only */
static void
keyring_page_add_settings(PidginPrefsWindow *win)
{
	GtkWidget *box;
	GList *it, *groups;
	GtkSizeGroup *sg;

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(win->keyring.vbox), box, FALSE, FALSE, 0);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	groups = purple_request_fields_get_groups(win->keyring.settings);
	for (it = g_list_first(groups); it != NULL; it = g_list_next(it)) {
		GList *it2, *fields;
		GtkBox *vbox;
		PurpleRequestFieldGroup *group;
		const gchar *group_title;

		group = it->data;
		group_title = purple_request_field_group_get_title(group);
		if (group_title) {
			vbox = GTK_BOX(pidgin_make_frame(box, group_title));
		} else {
			vbox = GTK_BOX(box);
		}

		fields = purple_request_field_group_get_fields(group);
		for (it2 = g_list_first(fields); it2 != NULL;
			it2 = g_list_next(it2)) {
			keyring_page_add_settings_field(vbox, it2->data, sg);
		}
	}

	g_object_unref(sg);

	win->keyring.settings_box = box;
}

static void
keyring_page_settings_apply(GtkButton *button, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (!purple_keyring_apply_settings(win, win->keyring.settings)) {
		return;
	}

	gtk_widget_set_sensitive(win->keyring.apply, FALSE);
}

static void
keyring_page_update_settings(PidginPrefsWindow *win)
{
	g_clear_pointer(&win->keyring.settings, purple_request_fields_destroy);
	win->keyring.settings = purple_keyring_read_settings();
	if (!win->keyring.settings) {
		return;
	}

	keyring_page_add_settings(win);

	win->keyring.apply = gtk_button_new_with_mnemonic(_("_Apply"));
	gtk_box_pack_start(GTK_BOX(win->keyring.settings_box),
	                   win->keyring.apply, FALSE, FALSE, 1);
	gtk_widget_set_sensitive(win->keyring.apply, FALSE);
	g_signal_connect(G_OBJECT(win->keyring.apply), "clicked",
	                 G_CALLBACK(keyring_page_settings_apply), win);

	gtk_widget_show_all(win->keyring.settings_box);
}

static void
keyring_page_pref_set_inuse(GError *error, G_GNUC_UNUSED gpointer unused)
{
	PurpleKeyring *in_use = purple_keyring_get_inuse();

	if (prefs == NULL) {
		purple_debug_info("gtkprefs", "pref window already closed\n");
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(prefs->keyring.active.combo), TRUE);

	if (error != NULL) {
		pidgin_prefs_bind_dropdown_revert_active(
		        &prefs->keyring.active);
		purple_notify_error(NULL, _("Keyring"),
			_("Failed to set new keyring"), error->message, NULL);
		return;
	}

	g_return_if_fail(in_use != NULL);
	purple_prefs_set_string("/purple/keyring/active",
		purple_keyring_get_id(in_use));

	keyring_page_update_settings(prefs);
}

static void
keyring_page_pref_changed(GtkComboBox *combo_box, PidginPrefCombo *combo)
{
	const char *keyring_id;
	PurpleKeyring *keyring;

	g_return_if_fail(combo_box != NULL);

	keyring_id = combo->value.string;
	keyring = purple_keyring_find_keyring_by_id(keyring_id);
	if (keyring == NULL) {
		pidgin_prefs_bind_dropdown_revert_active(combo);
		purple_notify_error(NULL, _("Keyring"),
			_("Selected keyring is disabled"), NULL, NULL);
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(combo_box), FALSE);

	g_clear_pointer(&prefs->keyring.settings_box, gtk_widget_destroy);
	g_clear_pointer(&prefs->keyring.settings,
	                purple_request_fields_destroy);

	purple_keyring_set_inuse(keyring, FALSE, keyring_page_pref_set_inuse,
	                         NULL);
}

static void
keyring_page_cleanup(PidginPrefsWindow *win)
{
	g_clear_pointer(&win->keyring.settings, purple_request_fields_destroy);
}

static void
bind_keyring_page(PidginPrefsWindow *win)
{
	GList *names;

	/* Keyring selection */
	names = purple_keyring_get_options();
	win->keyring.active.type = PURPLE_PREF_STRING;
	win->keyring.active.key = "/purple/keyring/active";
	pidgin_prefs_bind_dropdown_from_list(&win->keyring.active, names);
	/* Override the usual callback to defer changing the pref. */
	win->keyring.active.cb = keyring_page_pref_changed;
	g_list_free(names);

	keyring_page_update_settings(win);
}

/*** keyring page - end *************************************************/

static gboolean
sound_method_filter(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gboolean any = FALSE;
	gboolean gstreamer = FALSE;
	gboolean win32 = FALSE;

	gtk_tree_model_get(model, iter, 2, &any, 3, &gstreamer, 4, &win32, -1);

	if (any) {
		return TRUE;
	}

	if (gstreamer) {
#ifdef USE_GSTREAMER
#ifdef _WIN32
		return win32;
#else
		return !win32;
#endif
#else
		return FALSE;
#endif
	}

#ifdef _WIN32
	return win32;
#else
	return !win32;
#endif
}

static gint
sound_cmd_yeah(GtkEntry *entry, gpointer d)
{
	purple_prefs_set_path(PIDGIN_PREFS_ROOT "/sound/command",
			gtk_entry_get_text(GTK_ENTRY(entry)));
	return TRUE;
}

static void
sound_changed1_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox, purple_strequal(method, "custom"));
}

static void
sound_changed2_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *vbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(vbox, !purple_strequal(method, "none"));
}


static void
event_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	char *pref;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
						2, &pref,
						-1);

	purple_prefs_set_bool(pref, !gtk_cell_renderer_toggle_get_active(cell));
	g_free(pref);

	gtk_list_store_set(GTK_LIST_STORE (model), &iter,
					   0, !gtk_cell_renderer_toggle_get_active(cell),
					   -1);

	gtk_tree_path_free(path);
}

static void
test_sound(GtkWidget *button, gpointer i_am_NULL)
{
	char *pref;
	gboolean temp_enabled;
	gboolean temp_mute;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/enabled/%s",
			pidgin_sound_get_event_option(sound_row_sel));

	temp_enabled = purple_prefs_get_bool(pref);
	temp_mute = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/sound/mute");

	if (!temp_enabled) purple_prefs_set_bool(pref, TRUE);
	if (temp_mute) purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", FALSE);

	purple_sound_play_event(sound_row_sel, NULL);

	if (!temp_enabled) purple_prefs_set_bool(pref, FALSE);
	if (temp_mute) purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", TRUE);

	g_free(pref);
}

/*
 * Resets a sound file back to default.
 */
static void
reset_sound(GtkWidget *button, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);
	gchar *pref;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	purple_prefs_set_path(pref, "");
	g_free(pref);

	gtk_entry_set_text(GTK_ENTRY(win->sound.entry), _("(default)"));

	pref_sound_generate_markup();
}

static void
sound_chosen_cb(void *user_data, const char *filename)
{
	gchar *pref;
	int sound;

	sound = GPOINTER_TO_INT(user_data);

	/* Set it -- and forget it */
	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound));
	purple_prefs_set_path(pref, filename);
	g_free(pref);

	/*
	 * If the sound we just changed is still the currently selected
	 * sound, then update the box showing the file name.
	 */
	if (sound == sound_row_sel)
		gtk_entry_set_text(GTK_ENTRY(prefs->sound.entry), filename);

	pref_sound_generate_markup();
}

static void
select_sound(GtkWidget *button, gpointer being_NULL_is_fun)
{
	gchar *pref;
	const char *filename;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	filename = purple_prefs_get_path(pref);
	g_free(pref);

	if (*filename == '\0')
		filename = NULL;

	purple_request_file(prefs, _("Sound Selection"), filename, FALSE,
		G_CALLBACK(sound_chosen_cb), NULL, NULL,
		GINT_TO_POINTER(sound_row_sel));
}

static void
prefs_sound_sel(GtkTreeSelection *sel, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);
	GtkTreeModel *model;
	GtkTreeIter  iter;
	GValue val;
	const char *file;
	char *pref;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 3, &val);
	sound_row_sel = g_value_get_uint(&val);

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
			pidgin_sound_get_event_option(sound_row_sel));
	file = purple_prefs_get_path(pref);
	g_free(pref);
	if (win->sound.entry) {
		gtk_entry_set_text(GTK_ENTRY(win->sound.entry),
		                   (file && *file != '\0') ? file
		                                           : _("(default)"));
	}
	g_value_unset (&val);

	pref_sound_generate_markup();
}

static void
bind_sound_page(PidginPrefsWindow *win)
{
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	int j;
	const char *file;
	char *pref;
	const char *cmd;

	win->sound.method.type = PURPLE_PREF_STRING;
	win->sound.method.key = PIDGIN_PREFS_ROOT "/sound/method";
	pidgin_prefs_bind_dropdown(&win->sound.method);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(win->sound.method.combo));
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model), sound_method_filter, NULL, NULL);
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(model));

	gtk_widget_set_sensitive(
	        win->sound.method_vbox,
	        !purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT
	                                                 "/sound/method"),
	                         "none"));
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
	                              sound_changed2_cb,
	                              win->sound.method_vbox);

	gtk_widget_set_sensitive(
	        win->sound.command_hbox,
	        purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT
	                                                "/sound/method"),
	                        "custom"));
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
	                              sound_changed1_cb,
	                              win->sound.command_hbox);

	cmd = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/sound/command");
	if (cmd) {
		gtk_entry_set_text(GTK_ENTRY(win->sound.command), cmd);
	}

	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/sound/mute",
	                           win->sound.mute);

	pidgin_prefs_bind_checkbox(PIDGIN_PREFS_ROOT "/sound/conv_focus",
	                           win->sound.conv_focus);

	win->sound.while_status.type = PURPLE_PREF_INT;
	win->sound.while_status.key = "/purple/sound/while_status";
	pidgin_prefs_bind_dropdown(&win->sound.while_status);

	/* SOUND SELECTION */
	for (j=0; j < PURPLE_NUM_SOUNDS; j++) {
		char *pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/enabled/%s",
					     pidgin_sound_get_event_option(j));
		const char *label = pidgin_sound_get_event_label(j);
		GtkTreeIter iter;

		if (label == NULL) {
			g_free(pref);
			continue;
		}

		gtk_list_store_append(win->sound.event.store, &iter);
		gtk_list_store_set(win->sound.event.store, &iter,
				   0, purple_prefs_get_bool(pref),
				   1, _(label),
				   2, pref,
				   3, j,
				   -1);
		g_free(pref);
	}

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(win->sound.event.view));
	path = gtk_tree_path_new_first();
	gtk_tree_selection_select_path(sel, path);
	gtk_tree_path_free(path);

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
			       pidgin_sound_get_event_option(0));
	file = purple_prefs_get_path(pref);
	g_free(pref);
	gtk_entry_set_text(GTK_ENTRY(win->sound.entry),
	                   (file && *file != '\0') ? file : _("(default)"));
}


static void
set_idle_away(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/idleaway", purple_savedstatus_get_creation_time(status));
}

static void
set_startupstatus(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/startup", purple_savedstatus_get_creation_time(status));
}

static void
bind_away_page(PidginPrefsWindow *win)
{
	GtkWidget *menu;

	/* Idle stuff */
	win->away.idle_reporting.type = PURPLE_PREF_STRING;
	win->away.idle_reporting.key = "/purple/away/idle_reporting";
	pidgin_prefs_bind_dropdown(&win->away.idle_reporting);

	pidgin_prefs_bind_spin_button("/purple/away/mins_before_away",
			win->away.mins_before_away);

	pidgin_prefs_bind_checkbox("/purple/away/away_when_idle",
			win->away.away_when_idle);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_idleaway(), G_CALLBACK(set_idle_away));
	gtk_widget_show_all(menu);
	gtk_box_pack_start(GTK_BOX(win->away.idle_hbox), menu, FALSE, FALSE, 0);

	g_object_bind_property(win->away.away_when_idle, "active",
			menu, "sensitive",
			G_BINDING_SYNC_CREATE);

	/* Away stuff */
	win->away.auto_reply.type = PURPLE_PREF_STRING;
	win->away.auto_reply.key = "/purple/away/auto_reply";
	pidgin_prefs_bind_dropdown(&win->away.auto_reply);

	/* Signon status stuff */
	pidgin_prefs_bind_checkbox("/purple/savedstatus/startup_current_status",
			win->away.startup_current_status);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_startup(), G_CALLBACK(set_startupstatus));
	gtk_widget_show_all(menu);
	gtk_box_pack_start(GTK_BOX(win->away.startup_hbox), menu, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(win->away.startup_label), menu);
	pidgin_set_accessible_label(menu, GTK_LABEL(win->away.startup_label));
	g_object_bind_property(win->away.startup_current_status, "active",
			win->away.startup_hbox, "sensitive",
			G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
}

#ifdef USE_VV
static GList *
get_vv_device_menuitems(PurpleMediaElementType type)
{
	GList *result = NULL;
	GList *i;

	i = purple_media_manager_enumerate_elements(purple_media_manager_get(),
			type);
	for (; i; i = g_list_delete_link(i, i)) {
		PurpleMediaElementInfo *info = i->data;

		result = g_list_append(result,
				purple_media_element_info_get_name(info));
		result = g_list_append(result,
				purple_media_element_info_get_id(info));
		g_object_unref(info);
	}

	return result;
}

static GstElement *
create_test_element(PurpleMediaElementType type)
{
	PurpleMediaElementInfo *element_info;

	element_info = purple_media_manager_get_active_element(purple_media_manager_get(), type);

	g_return_val_if_fail(element_info, NULL);

	return purple_media_element_info_call_create(element_info,
		NULL, NULL, NULL);
}

static void
vv_test_switch_page_cb(GtkStack *stack, G_GNUC_UNUSED GParamSpec *pspec,
                       gpointer data)
{
	PidginPrefsWindow *win = data;

	if (!g_str_equal(gtk_stack_get_visible_child_name(stack), "vv")) {
		/* Disable any running test pipelines. */
		gtk_toggle_button_set_active(
		        GTK_TOGGLE_BUTTON(win->vv.voice.test), FALSE);
		gtk_toggle_button_set_active(
		        GTK_TOGGLE_BUTTON(win->vv.video.test), FALSE);
	}
}

static GstElement *
create_voice_pipeline(void)
{
	GstElement *pipeline;
	GstElement *src, *sink;
	GstElement *volume;
	GstElement *level;
	GstElement *valve;

	pipeline = gst_pipeline_new("voicetest");

	src = create_test_element(PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SRC);
	sink = create_test_element(PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SINK);
	volume = gst_element_factory_make("volume", "volume");
	level = gst_element_factory_make("level", "level");
	valve = gst_element_factory_make("valve", "valve");

	gst_bin_add_many(GST_BIN(pipeline), src, volume, level, valve, sink, NULL);
	gst_element_link_many(src, volume, level, valve, sink, NULL);

	purple_debug_info("gtkprefs", "create_voice_pipeline: setting pipeline "
		"state to GST_STATE_PLAYING - it may hang here on win32\n");
	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
	purple_debug_info("gtkprefs", "create_voice_pipeline: state is set\n");

	return pipeline;
}

static void
on_volume_change_cb(GtkWidget *w, gdouble value, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);
	GstElement *volume;

	if (!win->vv.voice.pipeline) {
		return;
	}

	volume = gst_bin_get_by_name(GST_BIN(win->vv.voice.pipeline), "volume");
	g_object_set(volume, "volume",
	             gtk_scale_button_get_value(GTK_SCALE_BUTTON(w)) * 10.0, NULL);
}

static gdouble
gst_msg_db_to_percent(GstMessage *msg, gchar *value_name)
{
	const GValue *list;
	const GValue *value;
	gdouble value_db;
	gdouble percent;

	list = gst_structure_get_value(gst_message_get_structure(msg), value_name);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	value = g_value_array_get_nth(g_value_get_boxed(list), 0);
G_GNUC_END_IGNORE_DEPRECATIONS
	value_db = g_value_get_double(value);
	percent = pow(10, value_db / 20);
	return (percent > 1.0) ? 1.0 : percent;
}

static gboolean
gst_bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
		gst_structure_has_name(gst_message_get_structure(msg), "level")) {

		GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
		gchar *name = gst_element_get_name(src);

		if (purple_strequal(name, "level")) {
			gdouble percent;
			gdouble threshold;
			GstElement *valve;

			percent = gst_msg_db_to_percent(msg, "rms");
			gtk_progress_bar_set_fraction(
			        GTK_PROGRESS_BAR(win->vv.voice.level), percent);

			percent = gst_msg_db_to_percent(msg, "decay");
			threshold = gtk_range_get_value(GTK_RANGE(
			                    win->vv.voice.threshold)) /
			            100.0;
			valve = gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(src)), "valve");
			g_object_set(valve, "drop", (percent < threshold), NULL);
			g_object_set(win->vv.voice.level, "text",
			             (percent < threshold) ? _("DROP") : " ",
			             NULL);
		}

		g_free(name);
	}

	return TRUE;
}

static void
voice_test_destroy_cb(GtkWidget *w, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (!win->vv.voice.pipeline) {
		return;
	}

	gst_element_set_state(win->vv.voice.pipeline, GST_STATE_NULL);
	g_clear_pointer(&win->vv.voice.pipeline, gst_object_unref);
}

static void
enable_voice_test(PidginPrefsWindow *win)
{
	GstBus *bus;

	win->vv.voice.pipeline = create_voice_pipeline();
	bus = gst_pipeline_get_bus(GST_PIPELINE(win->vv.voice.pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message", G_CALLBACK(gst_bus_cb), win);
	gst_object_unref(bus);
}

static void
toggle_voice_test_cb(GtkToggleButton *test, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (gtk_toggle_button_get_active(test)) {
		gtk_widget_set_sensitive(win->vv.voice.level, TRUE);
		enable_voice_test(win);

		g_signal_connect(win->vv.voice.volume, "value-changed",
		                 G_CALLBACK(on_volume_change_cb), win);
		g_signal_connect(test, "destroy",
		                 G_CALLBACK(voice_test_destroy_cb), win);
	} else {
		gtk_progress_bar_set_fraction(
		        GTK_PROGRESS_BAR(win->vv.voice.level), 0.0);
		gtk_widget_set_sensitive(win->vv.voice.level, FALSE);
		g_object_disconnect(win->vv.voice.volume,
		                    "any-signal::value-changed",
		                    G_CALLBACK(on_volume_change_cb), win, NULL);
		g_object_disconnect(test, "any-signal::destroy",
		                    G_CALLBACK(voice_test_destroy_cb), win,
		                    NULL);
		voice_test_destroy_cb(NULL, win);
	}
}

static void
volume_changed_cb(GtkScaleButton *button, gdouble value, gpointer data)
{
	purple_prefs_set_int("/purple/media/audio/volume/input", value * 100);
}

static void
threshold_value_changed_cb(GtkScale *scale, GtkWidget *label)
{
	int value;
	char *tmp;

	value = (int)gtk_range_get_value(GTK_RANGE(scale));
	tmp = g_strdup_printf(_("Silence threshold: %d%%"), value);
	gtk_label_set_label(GTK_LABEL(label), tmp);
	g_free(tmp);

	purple_prefs_set_int("/purple/media/audio/silence_threshold", value);
}

static void
bind_voice_test(PidginPrefsWindow *win, GtkBuilder *builder)
{
	GObject *test;
	GObject *label;
	GObject *volume;
	GObject *threshold;
	char *tmp;

	volume = gtk_builder_get_object(builder, "vv.voice.volume");
	win->vv.voice.volume = GTK_WIDGET(volume);
	gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume),
			purple_prefs_get_int("/purple/media/audio/volume/input") / 100.0);
	g_signal_connect(volume, "value-changed",
	                 G_CALLBACK(volume_changed_cb), NULL);

	label = gtk_builder_get_object(builder, "vv.voice.threshold_label");
	tmp = g_strdup_printf(_("Silence threshold: %d%%"),
	                      purple_prefs_get_int("/purple/media/audio/silence_threshold"));
	gtk_label_set_text(GTK_LABEL(label), tmp);
	g_free(tmp);

	threshold = gtk_builder_get_object(builder, "vv.voice.threshold");
	win->vv.voice.threshold = GTK_WIDGET(threshold);
	gtk_range_set_value(GTK_RANGE(threshold),
			purple_prefs_get_int("/purple/media/audio/silence_threshold"));
	g_signal_connect(threshold, "value-changed",
	                 G_CALLBACK(threshold_value_changed_cb), label);

	win->vv.voice.level =
	        GTK_WIDGET(gtk_builder_get_object(builder, "vv.voice.level"));

	test = gtk_builder_get_object(builder, "vv.voice.test");
	g_signal_connect(test, "toggled",
	                 G_CALLBACK(toggle_voice_test_cb), win);
	win->vv.voice.test = GTK_WIDGET(test);
}

static GstElement *
create_video_pipeline(void)
{
	GstElement *pipeline;
	GstElement *src, *sink;
	GstElement *videoconvert;
	GstElement *videoscale;

	pipeline = gst_pipeline_new("videotest");
	src = create_test_element(PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SRC);
	sink = create_test_element(PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SINK);
	videoconvert = gst_element_factory_make("videoconvert", NULL);
	videoscale = gst_element_factory_make("videoscale", NULL);

	g_object_set_data(G_OBJECT(pipeline), "sink", sink);

	gst_bin_add_many(GST_BIN(pipeline), src, videoconvert, videoscale, sink,
			NULL);
	gst_element_link_many(src, videoconvert, videoscale, sink, NULL);

	return pipeline;
}

static void
video_test_destroy_cb(GtkWidget *w, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (!win->vv.video.pipeline) {
		return;
	}

	gst_element_set_state(win->vv.video.pipeline, GST_STATE_NULL);
	g_clear_pointer(&win->vv.video.pipeline, gst_object_unref);
}

static void
window_id_cb(GstBus *bus, GstMessage *msg, gulong window_id)
{
	if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ELEMENT ||
	    !gst_is_video_overlay_prepare_window_handle_message(msg)) {
		return;
	}

	g_signal_handlers_disconnect_matched(bus,
	                                     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	                                     0, 0, NULL, window_id_cb,
	                                     (gpointer)window_id);

	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg)),
	                                    window_id);
}

static void
enable_video_test(PidginPrefsWindow *win)
{
	GstBus *bus;
	GdkWindow *window = gtk_widget_get_window(win->vv.video.drawing_area);
	gulong window_id = 0;

#ifdef GDK_WINDOWING_WIN32
	if (GDK_IS_WIN32_WINDOW(window))
		window_id = GPOINTER_TO_UINT(GDK_WINDOW_HWND(window));
	else
#endif
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_WINDOW(window))
		window_id = gdk_x11_window_get_xid(window);
	else
#endif
#ifdef GDK_WINDOWING_QUARTZ
	if (GDK_IS_QUARTZ_WINDOW(window))
		window_id = (gulong)gdk_quartz_window_get_nsview(window);
	else
#endif
		g_warning("Unsupported GDK backend");
#if !(defined(GDK_WINDOWING_WIN32) \
   || defined(GDK_WINDOWING_X11) \
   || defined(GDK_WINDOWING_QUARTZ))
#	error "Unsupported GDK windowing system"
#endif

	win->vv.video.pipeline = create_video_pipeline();
	bus = gst_pipeline_get_bus(GST_PIPELINE(win->vv.video.pipeline));
	gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, NULL, NULL);
	g_signal_connect(bus, "sync-message::element",
	                 G_CALLBACK(window_id_cb), (gpointer)window_id);
	gst_object_unref(bus);

	gst_element_set_state(GST_ELEMENT(win->vv.video.pipeline),
	                      GST_STATE_PLAYING);
}

static void
toggle_video_test_cb(GtkToggleButton *test, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (gtk_toggle_button_get_active(test)) {
		enable_video_test(win);
		g_signal_connect(test, "destroy",
		                 G_CALLBACK(video_test_destroy_cb), win);
	} else {
		g_object_disconnect(test, "any-signal::destroy",
		                    G_CALLBACK(video_test_destroy_cb), win,
		                    NULL);
		video_test_destroy_cb(NULL, win);
	}
}

static void
bind_video_test(PidginPrefsWindow *win, GtkBuilder *builder)
{
	GtkWidget *video;
	GObject *test;
	GdkRGBA color = {0.0, 0.0, 0.0, 1.0};

	win->vv.video.drawing_area = video = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.video.test_area"));
	gtk_widget_override_background_color(video, GTK_STATE_FLAG_NORMAL,
	                                     &color);

	/* In order to enable client shadow decorations, GtkDialog from GTK+ 3.0
	 * uses ARGB visual which by default gets inherited by its child
	 * widgets. XVideo adaptors on the other hand often support just depth
	 * 24 and rendering video through xvimagesink onto a widget inside a
	 * GtkDialog then results in no visible output.
	 *
	 * This ensures the default system visual of the drawing area doesn't
	 * get overridden by the widget's parent.
	 */
	gtk_widget_set_visual(video, gdk_screen_get_system_visual(
	                                     gtk_widget_get_screen(video)));

	test = gtk_builder_get_object(builder, "vv.video.test");
	g_signal_connect(test, "toggled",
	                 G_CALLBACK(toggle_video_test_cb), win);
	win->vv.video.test = GTK_WIDGET(test);
}

static void
vv_device_changed_cb(const gchar *name, PurplePrefType type,
                     gconstpointer value, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	PurpleMediaManager *manager;
	PurpleMediaElementInfo *info;

	manager = purple_media_manager_get();
	info = purple_media_manager_get_element_info(manager, value);
	purple_media_manager_set_active_element(manager, info);

	/* Refresh test viewers */
	if (strstr(name, "audio") && win->vv.voice.pipeline) {
		voice_test_destroy_cb(NULL, win);
		enable_voice_test(win);
	} else if (strstr(name, "video") && win->vv.video.pipeline) {
		video_test_destroy_cb(NULL, win);
		enable_video_test(win);
	}
}

static const char *
purple_media_type_to_preference_key(PurpleMediaElementType type)
{
	if (type & PURPLE_MEDIA_ELEMENT_AUDIO) {
		if (type & PURPLE_MEDIA_ELEMENT_SRC) {
			return PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device";
		} else if (type & PURPLE_MEDIA_ELEMENT_SINK) {
			return PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device";
		}
	} else if (type & PURPLE_MEDIA_ELEMENT_VIDEO) {
		if (type & PURPLE_MEDIA_ELEMENT_SRC) {
			return PIDGIN_PREFS_ROOT "/vvconfig/video/src/device";
		} else if (type & PURPLE_MEDIA_ELEMENT_SINK) {
			return PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device";
		}
	}

	return NULL;
}

static void
bind_vv_dropdown(PidginPrefCombo *combo, PurpleMediaElementType element_type)
{
	const gchar *preference_key;
	GList *devices;

	preference_key = purple_media_type_to_preference_key(element_type);
	devices = get_vv_device_menuitems(element_type);

	if (g_list_find_custom(devices, purple_prefs_get_string(preference_key),
		(GCompareFunc)strcmp) == NULL)
	{
		GList *next = g_list_next(devices);
		if (next)
			purple_prefs_set_string(preference_key, next->data);
	}

	combo->type = PURPLE_PREF_STRING;
	combo->key = preference_key;
	pidgin_prefs_bind_dropdown_from_list(combo, devices);
	g_list_free_full(devices, g_free);
}

static void
bind_vv_frame(PidginPrefsWindow *win, PidginPrefCombo *combo,
              PurpleMediaElementType type)
{
	bind_vv_dropdown(combo, type);

	purple_prefs_connect_callback(combo->combo,
	                              purple_media_type_to_preference_key(type),
	                              vv_device_changed_cb, win);
	g_signal_connect_swapped(combo->combo, "destroy",
	                         G_CALLBACK(purple_prefs_disconnect_by_handle),
	                         combo->combo);

	g_object_set_data(G_OBJECT(combo->combo), "vv_media_type",
	                  (gpointer)type);
	g_object_set_data(G_OBJECT(combo->combo), "vv_combo", combo);
}

static void
device_list_changed_cb(PurpleMediaManager *manager, GtkWidget *widget)
{
	PidginPrefCombo *combo;
	PurpleMediaElementType media_type;
	GtkTreeModel *model;

	combo = g_object_get_data(G_OBJECT(widget), "vv_combo");
	media_type = (PurpleMediaElementType)g_object_get_data(G_OBJECT(widget),
			"vv_media_type");

	/* Unbind original connections so we can repopulate the combo box. */
	g_object_disconnect(combo->combo, "any-signal::changed",
	                    G_CALLBACK(bind_dropdown_set), combo, NULL);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo));
	gtk_list_store_clear(GTK_LIST_STORE(model));

	bind_vv_dropdown(combo, media_type);
}

static GtkWidget *
vv_page(PidginPrefsWindow *win)
{
	GtkBuilder *builder;
	GtkWidget *ret;
	PurpleMediaManager *manager;

	builder = gtk_builder_new_from_resource("/im/pidgin/Pidgin/Prefs/vv.ui");
	gtk_builder_set_translation_domain(builder, PACKAGE);

	ret = GTK_WIDGET(gtk_builder_get_object(builder, "vv.page"));

	manager = purple_media_manager_get();

	win->vv.voice.input.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.voice.input.combo"));
	bind_vv_frame(win, &win->vv.voice.input,
	              PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SRC);
	g_signal_connect_object(manager, "elements-changed::audiosrc",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.voice.input.combo, 0);

	win->vv.voice.output.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.voice.output.combo"));
	bind_vv_frame(win, &win->vv.voice.output,
	              PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SINK);
	g_signal_connect_object(manager, "elements-changed::audiosink",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.voice.output.combo, 0);

	bind_voice_test(win, builder);

	win->vv.video.input.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.video.input.combo"));
	bind_vv_frame(win, &win->vv.video.input,
	              PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SRC);
	g_signal_connect_object(manager, "elements-changed::videosrc",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.video.input.combo, 0);

	win->vv.video.output.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.video.output.combo"));
	bind_vv_frame(win, &win->vv.video.output,
	              PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SINK);
	g_signal_connect_object(manager, "elements-changed::videosink",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.video.output.combo, 0);

	bind_video_test(win, builder);

	g_signal_connect(win->stack, "notify::visible-child",
	                 G_CALLBACK(vv_test_switch_page_cb), win);

	g_object_ref(ret);
	g_object_unref(builder);

	return ret;
}
#endif

static void
prefs_stack_init(PidginPrefsWindow *win)
{
#ifdef USE_VV
	GtkStack *stack = GTK_STACK(win->stack);
	GtkWidget *vv;
#endif

	bind_interface_page(win);
	bind_browser_page(win);
	bind_conv_page(win);
	bind_logging_page(win);
	bind_network_page(win);
	bind_proxy_page(win);
	bind_keyring_page(win);
	bind_sound_page(win);
	bind_away_page(win);
	bind_theme_page(win);
#ifdef USE_VV
	vv = vv_page(win);
	gtk_container_add_with_properties(GTK_CONTAINER(stack), vv, "name",
	                                  "vv", "title", _("Voice/Video"),
	                                  NULL);
	g_object_unref(vv);
#endif
}

static void
pidgin_prefs_window_class_init(PidginPrefsWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/Prefs/prefs.ui"
	);

	/* Main window */
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     stack);
	gtk_widget_class_bind_template_callback(widget_class, delete_prefs);

	/* Interface page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.docklet.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.im.hide_new.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.win32.minimize_new_convs);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.conversations.tabs);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.conversations.tabs_vbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.conversations.close_on_tabs);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.conversations.tab_side.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			iface.conversations.placement.combo);

	/* Browser page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.page);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.stack);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.gnome_not_found);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.gnome_program);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.browser.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.place_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.place.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.manual_command_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, browser.manual_command);
	gtk_widget_class_bind_template_callback(widget_class,
			browser_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			manual_browser_set);

	/* Conversations page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.notification_chat.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.show_incoming_formatting);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.im.close_immediately);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.im.show_buddy_icons);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.im.animate_buddy_icons);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.im.send_typing);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.spellcheck);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.use_smooth_scrolling);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.win32.blink_im);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.resize_custom_smileys);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.custom_smileys_size);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.minimum_entry_lines);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.format_buffer);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.format_view);
#ifdef WIN32
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.font_frame);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.use_theme_font);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.custom_font_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			conversations.custom_font);
#endif
	/* Even though Win32-specific, must be bound to avoid Glade warnings. */
	gtk_widget_class_bind_template_callback(widget_class,
			apply_custom_font);
	gtk_widget_class_bind_template_callback(widget_class,
			pidgin_custom_font_set);

	/* Logging page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, logging.format.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, logging.log_ims);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, logging.log_chats);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, logging.log_system);

	/* Network page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, network.stun_server);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, network.auto_ip);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, network.public_ip);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.public_ip_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, network.map_ports);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.ports_range_use);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.ports_range_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.ports_range_start);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.ports_range_end);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, network.turn_server);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.turn_port_udp);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.turn_port_tcp);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.turn_username);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			network.turn_password);
	gtk_widget_class_bind_template_callback(widget_class,
			network_stun_server_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                 auto_ip_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			network_ip_changed);
	gtk_widget_class_bind_template_callback(widget_class,
			network_turn_server_changed_cb);

	/* Proxy page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.stack);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.gnome_not_found);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.gnome_program);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			proxy.socks4_remotedns);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.type.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.options);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.host);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.port);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.username);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, proxy.password);
	gtk_widget_class_bind_template_callback(widget_class,
			proxy_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			proxy_print_option);

	/* Keyrings page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, keyring.active.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, keyring.vbox);

	/* Sounds page */
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.method.combo);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.method_vbox);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.command);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.command_hbox);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.mute);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.conv_focus);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.while_status.combo);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.event.view);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.event.store);
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     sound.entry);
	gtk_widget_class_bind_template_callback(widget_class, sound_cmd_yeah);
	gtk_widget_class_bind_template_callback(widget_class, prefs_sound_sel);
	gtk_widget_class_bind_template_callback(widget_class, event_toggled);
	gtk_widget_class_bind_template_callback(widget_class, select_sound);
	gtk_widget_class_bind_template_callback(widget_class, test_sound);
	gtk_widget_class_bind_template_callback(widget_class, reset_sound);

	/* Away page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			away.idle_reporting.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			away.mins_before_away);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, away.away_when_idle);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, away.idle_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			away.auto_reply.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow,
			away.startup_current_status);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, away.startup_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, away.startup_label);

	/* Themes page */
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, theme.blist);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, theme.status);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, theme.sound);
	gtk_widget_class_bind_template_child(
			widget_class, PidginPrefsWindow, theme.smiley);
	gtk_widget_class_bind_template_callback(widget_class,
			prefs_set_blist_theme_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			prefs_set_status_icon_theme_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			prefs_set_sound_theme_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			prefs_set_smiley_theme_cb);
}

static void
pidgin_prefs_window_init(PidginPrefsWindow *win)
{
	/* copy the preferences to tmp values...
	 * I liked "take affect immediately" Oh well :-( */
	/* (that should have been "effect," right?) */

	/* Back to instant-apply! I win!  BU-HAHAHA! */

	/* Create the window */
	gtk_widget_init_template(GTK_WIDGET(win));

	prefs_stack_init(win);

	/* Refresh the list of themes before showing the preferences window */
	prefs_themes_refresh();
}

void
pidgin_prefs_show(void)
{
	if (prefs == NULL) {
		prefs = PIDGIN_PREFS_WINDOW(g_object_new(
					pidgin_prefs_window_get_type(), NULL));
	}

	gtk_window_present(GTK_WINDOW(prefs));
}

static void
smiley_theme_pref_cb(const char *name, PurplePrefType type,
					 gconstpointer value, gpointer data)
{
	const gchar *theme_name = value;
	GList *themes, *it;

	if (purple_strequal(theme_name, "none")) {
		purple_smiley_theme_set_current(NULL);
		return;
	}

	/* XXX: could be cached when initializing prefs view */
	themes = pidgin_smiley_theme_get_all();

	for (it = themes; it; it = g_list_next(it)) {
		PidginSmileyTheme *theme = it->data;

		if (!purple_strequal(pidgin_smiley_theme_get_name(theme), theme_name))
			continue;

		purple_smiley_theme_set_current(PURPLE_SMILEY_THEME(theme));
	}
}

void
pidgin_prefs_init(void)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "");
	purple_prefs_add_none("/plugins/gtk");

#ifndef _WIN32
	/* Browsers */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/browsers");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/browsers/place", PIDGIN_BROWSER_DEFAULT);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/browser", "xdg-open");
#endif

	/* Plugins */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/plugins");
	purple_prefs_add_path_list(PIDGIN_PREFS_ROOT "/plugins/loaded", NULL);

	/* File locations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/filelocations");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_save_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_open_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", "");

	/* Themes */
	prefs_themes_init();

	/* Smiley Themes */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/smileys");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/smileys/theme", "Default");

	/* Smiley Callbacks */
	purple_prefs_connect_callback(&prefs, PIDGIN_PREFS_ROOT "/smileys/theme",
								smiley_theme_pref_cb, NULL);

#ifdef USE_VV
	/* Voice/Video */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio/src");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video/src");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video/sink");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device", "");
#endif

	pidgin_prefs_update_old();
}

void
pidgin_prefs_update_old(void)
{
	/* Rename some old prefs */
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_ims", "/purple/logging/log_ims");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_chats", "/purple/logging/log_chats");
	purple_prefs_rename("/purple/conversations/placement",
					  PIDGIN_PREFS_ROOT "/conversations/placement");

	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/im/raise_on_events", "/plugins/gtk/X11/notify/method_raise");

	purple_prefs_rename_boolean_toggle(PIDGIN_PREFS_ROOT "/conversations/ignore_colors",
									 PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting");

	/*
	 * This path pref changed to a string, so migrate. I know this will
	 * break things for and confuse users that use multiple versions with
	 * the same config directory, but I'm not inclined to want to deal with
	 * that at the moment. -- rekkanoryo
	 */
	if (purple_prefs_exists(PIDGIN_PREFS_ROOT "/browsers/command") &&
		purple_prefs_get_pref_type(PIDGIN_PREFS_ROOT "/browsers/command") ==
			PURPLE_PREF_PATH)
	{
		const char *str = purple_prefs_get_path(
			PIDGIN_PREFS_ROOT "/browsers/command");
		purple_prefs_set_string(
			PIDGIN_PREFS_ROOT "/browsers/manual_command", str);
		purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/command");
	}

	/* Remove some no-longer-used prefs */
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/auto_expand_contacts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/button_style");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/grey_idle_buddies");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_group_count");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_warning_level");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/tooltip_delay");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/x");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/y");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/button_type");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ctrl_enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/escape_closes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/html_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/icons_on_tabs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/send_formatting");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_smileys");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_urls_as_links");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/smiley_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_bgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_fgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_font");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_size");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/old_tab_complete");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/tab_completion");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/hide_on_send");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/color_nicks");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_fonts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_font_sizes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/passthrough_unknown_commands");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/debug/timestamps");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/idle");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/logging/individual_logs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/signon");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/silent_signon");

	/* Convert old queuing prefs to hide_new 3-way pref. */
	if (purple_prefs_exists("/plugins/gtk/docklet/queue_messages") &&
	    purple_prefs_get_bool("/plugins/gtk/docklet/queue_messages"))
	{
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "always");
	}
	else if (purple_prefs_exists(PIDGIN_PREFS_ROOT "/away/queue_messages") &&
	         purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/away/queue_messages"))
	{
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "away");
	}
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away/queue_messages");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away");
	purple_prefs_remove("/plugins/gtk/docklet/queue_messages");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_height");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_height");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/x",
			PIDGIN_PREFS_ROOT "/conversations/im/x");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/y",
			PIDGIN_PREFS_ROOT "/conversations/im/y");

	/* Fixup vvconfig plugin prefs */
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/src/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/src/device"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/sink/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/sink/device"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/video/src/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/device",
				purple_prefs_get_string("/plugins/core/vvconfig/video/src/device"));
	}
	if (purple_prefs_exists("/plugins/gtk/vvconfig/video/sink/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device",
				purple_prefs_get_string("/plugins/gtk/vvconfig/video/sink/device"));
	}

	purple_prefs_remove("/plugins/core/vvconfig");
	purple_prefs_remove("/plugins/gtk/vvconfig");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/plugin");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/plugin");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/video/src/plugin");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/plugin");

#ifndef _WIN32
	/* Added in 3.0.0. */
	if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/browsers/place") == 1) {
		/* If the "open link in" pref is set to the old value for "existing
		   window" then change it to "default." */
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/browsers/place",
				PIDGIN_BROWSER_DEFAULT);
	}

	/* Added in 3.0.0. */
	if (g_str_equal(
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"),
			"netscape")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", "xdg-open");
	}
#endif /* !_WIN32 */
}

