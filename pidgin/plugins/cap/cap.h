/*
 * Contact Availability Prediction plugin for Purple
 *
 * Copyright (C) 2006 Geoffrey Foster.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifndef _CAP_H_
#define _CAP_H_

#include "internal.h"
#include "pidgin.h"

#include "conversation.h"

#include "gtkconv.h"
#include "gtkblist.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "buddylist.h"
#include "notify.h"
#include "version.h"
#include "debug.h"

#include "util.h"

#include <glib.h>
#include <time.h>
#include <sqlite3.h>
#include "cap_statistics.h"

#define CAP_PLUGIN_ID "gtk-g-off_-cap"

/* Variables used throughout lifetime of the plugin */
PurplePlugin *_plugin_pointer;
sqlite3 *_db; /**< The database */

GHashTable *_buddy_stats = NULL;
GHashTable *_my_offline_times = NULL;
gboolean _signals_connected;
gboolean _sqlite_initialized;

/* Prefs UI */
typedef struct _CapPrefsUI CapPrefsUI;

struct _CapPrefsUI {
	GtkWidget *ret;
	GtkWidget *cap_vbox;
	GtkWidget *table_layout;

	GtkWidget *threshold_label;
	GtkWidget *threshold_input;
	GtkWidget *threshold_minutes_label;

	GtkWidget *msg_difference_label;
	GtkWidget *msg_difference_input;
	GtkWidget *msg_difference_minutes_label;

	GtkWidget *last_seen_label;
	GtkWidget *last_seen_input;
	GtkWidget *last_seen_minutes_label;
};

static void generate_prediction(CapStatistics *statistics);
static double generate_prediction_for(PurpleBuddy *buddy);
static CapStatistics * get_stats_for(PurpleBuddy *buddy);
static void destroy_stats(gpointer data);
static void insert_cap_msg_count_success(const char *buddy_name, const char *account, const char *protocol, int minute);
static void insert_cap_status_count_success(const char *buddy_name, const char *account, const char *protocol, const char *status_id);
static void insert_cap_msg_count_failed(const char *buddy_name, const char *account, const char *protocol, int minute);
static void insert_cap_status_count_failed(const char *buddy_name, const char *account, const char *protocol, const char *status_id);
static void insert_cap_success(CapStatistics *stats);
static void insert_cap_failure(CapStatistics *stats);
static gboolean max_message_difference_cb(gpointer data);
/* Pidgin Signal Handlers */
/* sent-im-msg */
static void sent_im_msg(PurpleAccount *account, PurpleMessage *msg, gpointer _unused);
/* received-im-msg */
static void received_im_msg(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags);
/* buddy-status-changed */
static void buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old_status, PurpleStatus *status);
/* buddy-signed-on */
static void buddy_signed_on(PurpleBuddy *buddy);
/* buddy-signed-off */
static void buddy_signed_off(PurpleBuddy *buddy);
/* drawing-tooltip */
static void drawing_tooltip(PurpleBlistNode *node, GString *text, gboolean full);
/* signed-on */
static void signed_on(PurpleConnection *gc);
/* signed-off */
static void signed_off(PurpleConnection *gc);
static void reset_all_last_message_times(gpointer key, gpointer value, gpointer user_data);
static PurpleStatus * get_status_for(PurpleBuddy *buddy);
static void create_tables(void);
static gboolean create_database_connection(void);
static void destroy_database_connection(void);
static guint word_count(const gchar *string);
static void insert_status_change(CapStatistics *statistics);
static void insert_status_change_from_purple_status(CapStatistics *statistics, PurpleStatus *status);
static void insert_word_count(const char *sender, const char *receiver, guint count);
static gboolean plugin_load(PurplePlugin *plugin, GError **error);
static void add_plugin_functionality(PurplePlugin *plugin);
static void cancel_conversation_timeouts(gpointer key, gpointer value, gpointer user_data);
static void remove_plugin_functionality(PurplePlugin *plugin);
static void write_stats_on_unload(gpointer key, gpointer value, gpointer user_data);
static gboolean plugin_unload(PurplePlugin *plugin, GError **error);
static CapPrefsUI * create_cap_prefs_ui(void);
static void cap_prefs_ui_destroy_cb(GObject *object, gpointer user_data);
static void numeric_spinner_prefs_cb(GtkSpinButton *spinbutton, gpointer user_data);
static GtkWidget * get_config_frame(PurplePlugin *plugin);

#endif
