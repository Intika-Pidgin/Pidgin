/*
 * Purple - iChat-style timestamps
 *
 * Copyright (C) 2002-2003, Sean Egan
 * Copyright (C) 2003, Chris J. Friesen <Darth_Sebulba04@yahoo.com>
 * Copyright (C) 2007, Andrew Gaul <andrew@gaul.org>
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

#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"

#include "gtkimhtml.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtkutils.h"

#define TIMESTAMP_PLUGIN_ID "gtk-timestamp"

/* minutes externally, seconds internally, and milliseconds in preferences */
static int interval = 5 * 60;

static void
timestamp_display(PurpleConversation *conv, time_t then, time_t now)
{
	PidginConversation *gtk_conv = PIDGIN_CONVERSATION(conv);
	GtkWidget *imhtml = gtk_conv->imhtml;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(imhtml));
	GtkTextIter iter;
	const char *mdate;
	int y, height;
	GdkRectangle rect;
	gboolean scrolled = FALSE;
	GtkTextTag *tag;

	/* display timestamp */
	mdate = purple_utf8_strftime(then == 0 ? "%H:%M" : "\n%H:%M",
		localtime(&now));
	gtk_text_buffer_get_end_iter(buffer, &iter);

	/* is the view already scrolled? */
	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(imhtml), &iter, &y, &height);
	if (((y + height) - (rect.y + rect.height)) > height)
		scrolled = TRUE;

	if ((tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), "TIMESTAMP")) == NULL)
		tag = gtk_text_buffer_create_tag(buffer, "TIMESTAMP",
			"foreground", "#888888", "justification", GTK_JUSTIFY_CENTER,
			"weight", PANGO_WEIGHT_BOLD, NULL);

	gtk_text_buffer_insert_with_tags(buffer, &iter, mdate,
		strlen(mdate), tag, NULL);

	/* scroll view if necessary */
	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_line_yrange(
		GTK_TEXT_VIEW(imhtml), &iter, &y, &height);
	if (!scrolled && ((y + height) - (rect.y + rect.height)) > height &&
	    gtk_text_buffer_get_char_count(buffer)) {
		gboolean smooth = purple_prefs_get_bool(
			PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling");
		gtk_imhtml_scroll_to_end(GTK_IMHTML(imhtml), smooth);
	}
}

static gboolean
timestamp_displaying_conv_msg(PurpleAccount *account, const char *who,
			      char **buffer, PurpleConversation *conv,
			      PurpleMessageFlags flags, void *data)
{
	time_t now = time(NULL) / interval * interval;
	time_t then;

	if (!g_list_find(purple_get_conversations(), conv))
		return FALSE;

	then = GPOINTER_TO_INT(purple_conversation_get_data(
		conv, "timestamp-last"));

	if (now - then >= interval) {
		timestamp_display(conv, then, now);
		purple_conversation_set_data(
			conv, "timestamp-last", GINT_TO_POINTER(now));
	}

	return FALSE;
}

static void
timestamp_new_convo(PurpleConversation *conv)
{
	if (!g_list_find(purple_get_conversations(), conv))
		return;

	purple_conversation_set_data(conv, "timestamp-last", GINT_TO_POINTER(0));
}

static void
set_timestamp(GtkWidget *spinner, void *null)
{
	int tm;

	tm = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));
	purple_debug(PURPLE_DEBUG_MISC, "timestamp",
		"setting interval to %d minutes\n", tm);

	interval = tm * 60;
	purple_prefs_set_int("/plugins/gtk/timestamp/interval", interval * 1000);
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *frame, *label;
	GtkWidget *vbox, *hbox;
	GtkObject *adj;
	GtkWidget *spinner;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	frame = pidgin_make_frame(ret, _("Display Timestamps Every"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* XXX limit to divisors of 60? */
	adj = gtk_adjustment_new(interval / 60, 1, 60, 1, 0, 0);
	spinner = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), spinner, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(spinner), "value-changed",
		G_CALLBACK(set_timestamp), NULL);
	label = gtk_label_new(_("minutes"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	gtk_widget_show_all(ret);
	return ret;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *conv_handle = purple_conversations_get_handle();
	void *gtkconv_handle = pidgin_conversations_get_handle();

	/* lower priority to display initial timestamp after logged messages */
	purple_signal_connect_priority(conv_handle, "conversation-created",
		plugin, PURPLE_CALLBACK(timestamp_new_convo), NULL,
		PURPLE_SIGNAL_PRIORITY_DEFAULT + 1);

	purple_signal_connect(gtkconv_handle, "displaying-chat-msg",
		plugin, PURPLE_CALLBACK(timestamp_displaying_conv_msg), NULL);
	purple_signal_connect(gtkconv_handle, "displaying-im-msg",
		plugin, PURPLE_CALLBACK(timestamp_displaying_conv_msg), NULL);

	interval = purple_prefs_get_int("/plugins/gtk/timestamp/interval") / 1000;

	return TRUE;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0, /* page_num (Reserved) */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	TIMESTAMP_PLUGIN_ID,                              /**< id             */
	N_("Timestamp"),                                  /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Display iChat-style timestamps"),
	                                                  /**  description    */
	N_("Display iChat-style timestamps every N minutes."),
	"Sean Egan <seanegan@gmail.com>",                 /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/gtk/timestamp");
	purple_prefs_add_int("/plugins/gtk/timestamp/interval", interval * 1000);
}

PURPLE_INIT_PLUGIN(timestamp, init_plugin, info)
