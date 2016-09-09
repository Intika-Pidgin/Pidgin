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

#include "internal.h"
#include "debug.h"
#include "connection.h"
#include "media.h"
#include "mediamanager.h"
#include "pidgin.h"
#include "request.h"

#include "gtkmedia.h"
#include "gtkutils.h"
#include "pidginstock.h"

#ifdef USE_VV
#include "media-gst.h"

#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_QUARTZ
#include <gdk/gdkquartz.h>
#endif
#include <gdk/gdkkeysyms.h>

#include "gtk3compat.h"

#define PIDGIN_TYPE_MEDIA            (pidgin_media_get_type())
#define PIDGIN_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_MEDIA, PidginMedia))
#define PIDGIN_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_MEDIA, PidginMediaClass))
#define PIDGIN_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_MEDIA))
#define PIDGIN_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_MEDIA))
#define PIDGIN_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_MEDIA, PidginMediaClass))

typedef struct _PidginMedia PidginMedia;
typedef struct _PidginMediaClass PidginMediaClass;
typedef struct _PidginMediaPrivate PidginMediaPrivate;

typedef enum
{
	/* Waiting for response */
	PIDGIN_MEDIA_WAITING = 1,
	/* Got request */
	PIDGIN_MEDIA_REQUESTED,
	/* Accepted call */
	PIDGIN_MEDIA_ACCEPTED,
	/* Rejected call */
	PIDGIN_MEDIA_REJECTED,
} PidginMediaState;

struct _PidginMediaClass
{
	GtkWindowClass parent_class;
};

struct _PidginMedia
{
	GtkWindow parent;
	PidginMediaPrivate *priv;
};

struct _PidginMediaPrivate
{
	PurpleMedia *media;
	gchar *screenname;
	gulong level_handler_id;

	GtkUIManager *ui;
	GtkWidget *menubar;
	GtkWidget *statusbar;

	GtkWidget *hold;
	GtkWidget *mute;
	GtkWidget *pause;

	GtkWidget *send_progress;
	GHashTable *recv_progressbars;

	PidginMediaState state;

	GtkWidget *display;
	GtkWidget *send_widget;
	GtkWidget *recv_widget;
	GtkWidget *button_widget;
	GtkWidget *local_video;
	GHashTable *remote_videos;

	guint timeout_id;
	PurpleMediaSessionType request_type;
};

#define PIDGIN_MEDIA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_MEDIA, PidginMediaPrivate))

static void pidgin_media_class_init (PidginMediaClass *klass);
static void pidgin_media_init (PidginMedia *media);
static void pidgin_media_dispose (GObject *object);
static void pidgin_media_finalize (GObject *object);
static void pidgin_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void pidgin_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void pidgin_media_set_state(PidginMedia *gtkmedia, PidginMediaState state);

static GtkWindowClass *parent_class = NULL;


#if 0
enum {
	LAST_SIGNAL
};
static guint pidgin_media_signals[LAST_SIGNAL] = {0};
#endif

enum {
	PROP_0,
	PROP_MEDIA,
	PROP_SCREENNAME
};

static GType
pidgin_media_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginMediaClass),
			NULL,
			NULL,
			(GClassInitFunc) pidgin_media_class_init,
			NULL,
			NULL,
			sizeof(PidginMedia),
			0,
			(GInstanceInitFunc) pidgin_media_init,
			NULL
		};
		type = g_type_register_static(GTK_TYPE_WINDOW, "PidginMedia", &info, 0);
	}
	return type;
}


static void
pidgin_media_class_init (PidginMediaClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
/*	GtkContainerClass *container_class = (GtkContainerClass*)klass; */
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->dispose = pidgin_media_dispose;
	gobject_class->finalize = pidgin_media_finalize;
	gobject_class->set_property = pidgin_media_set_property;
	gobject_class->get_property = pidgin_media_get_property;

	g_object_class_install_property(gobject_class, PROP_MEDIA,
			g_param_spec_object("media",
			"PurpleMedia",
			"The PurpleMedia associated with this media.",
			PURPLE_TYPE_MEDIA,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property(gobject_class, PROP_SCREENNAME,
			g_param_spec_string("screenname",
			"Screenname",
			"The screenname of the user this session is with.",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_type_class_add_private(klass, sizeof(PidginMediaPrivate));
}

static void
pidgin_media_hold_toggled(GtkToggleButton *toggle, PidginMedia *media)
{
	purple_media_stream_info(media->priv->media,
			gtk_toggle_button_get_active(toggle) ?
			PURPLE_MEDIA_INFO_HOLD : PURPLE_MEDIA_INFO_UNHOLD,
			NULL, NULL, TRUE);
}

static void
pidgin_media_mute_toggled(GtkToggleButton *toggle, PidginMedia *media)
{
	purple_media_stream_info(media->priv->media,
			gtk_toggle_button_get_active(toggle) ?
			PURPLE_MEDIA_INFO_MUTE : PURPLE_MEDIA_INFO_UNMUTE,
			NULL, NULL, TRUE);
}

static void
pidgin_media_pause_toggled(GtkToggleButton *toggle, PidginMedia *media)
{
	purple_media_stream_info(media->priv->media,
			gtk_toggle_button_get_active(toggle) ?
			PURPLE_MEDIA_INFO_PAUSE : PURPLE_MEDIA_INFO_UNPAUSE,
			NULL, NULL, TRUE);
}

static gboolean
pidgin_media_delete_event_cb(GtkWidget *widget,
		GdkEvent *event, PidginMedia *media)
{
	if (media->priv->media)
		purple_media_stream_info(media->priv->media,
				PURPLE_MEDIA_INFO_HANGUP, NULL, NULL, TRUE);
	return FALSE;
}

#ifdef HAVE_X11
static int
pidgin_x_error_handler(Display *display, XErrorEvent *event)
{
	const gchar *error_type;
	switch (event->error_code) {
#define XERRORCASE(type) case type: error_type = #type; break
		XERRORCASE(BadAccess);
		XERRORCASE(BadAlloc);
		XERRORCASE(BadAtom);
		XERRORCASE(BadColor);
		XERRORCASE(BadCursor);
		XERRORCASE(BadDrawable);
		XERRORCASE(BadFont);
		XERRORCASE(BadGC);
		XERRORCASE(BadIDChoice);
		XERRORCASE(BadImplementation);
		XERRORCASE(BadLength);
		XERRORCASE(BadMatch);
		XERRORCASE(BadName);
		XERRORCASE(BadPixmap);
		XERRORCASE(BadRequest);
		XERRORCASE(BadValue);
		XERRORCASE(BadWindow);
#undef XERRORCASE
		default:
			error_type = "unknown";
			break;
	}
	purple_debug_error("media", "A %s Xlib error has occurred. "
			"The program would normally crash now.\n",
			error_type);
	return 0;
}
#endif

static void
menu_hangup(GtkAction *action, gpointer data)
{
	PidginMedia *gtkmedia = PIDGIN_MEDIA(data);
	purple_media_stream_info(gtkmedia->priv->media,
			PURPLE_MEDIA_INFO_HANGUP, NULL, NULL, TRUE);
}

static const GtkActionEntry menu_entries[] = {
	{ "MediaMenu", NULL, N_("_Media"), NULL, NULL, NULL },
	{ "Hangup", NULL, N_("_Hangup"), NULL, NULL, G_CALLBACK(menu_hangup) },
};

static const char *media_menu =
"<ui>"
	"<menubar name='Media'>"
		"<menu action='MediaMenu'>"
			"<menuitem action='Hangup'/>"
		"</menu>"
	"</menubar>"
"</ui>";

static GtkWidget *
setup_menubar(PidginMedia *window)
{
	GtkActionGroup *action_group;
	GError *error;
	GtkAccelGroup *accel_group;
	GtkWidget *menu;

	action_group = gtk_action_group_new("MediaActions");
#ifdef ENABLE_NLS
	gtk_action_group_set_translation_domain(action_group,
	                                        PACKAGE);
#endif
	gtk_action_group_add_actions(action_group,
	                             menu_entries,
	                             G_N_ELEMENTS(menu_entries),
	                             GTK_WINDOW(window));

	window->priv->ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(window->priv->ui, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group(window->priv->ui);
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string(window->priv->ui, media_menu, -1, &error))
	{
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		exit(EXIT_FAILURE);
	}

	menu = gtk_ui_manager_get_widget(window->priv->ui, "/Media");

	gtk_widget_show(menu);
	return menu;
}

static void
pidgin_media_init (PidginMedia *media)
{
	GtkWidget *vbox;
	media->priv = PIDGIN_MEDIA_GET_PRIVATE(media);

#ifdef HAVE_X11
	XSetErrorHandler(pidgin_x_error_handler);
#endif

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(media), vbox);

	media->priv->statusbar = gtk_statusbar_new();
	gtk_box_pack_end(GTK_BOX(vbox), media->priv->statusbar,
			FALSE, FALSE, 0);
	gtk_statusbar_push(GTK_STATUSBAR(media->priv->statusbar),
			0, _("Calling..."));
	gtk_widget_show(media->priv->statusbar);

	media->priv->menubar = setup_menubar(media);
	gtk_box_pack_start(GTK_BOX(vbox), media->priv->menubar,
			FALSE, TRUE, 0);

	media->priv->display = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(media->priv->display),
			PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), media->priv->display,
			TRUE, TRUE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(vbox);

	g_signal_connect(G_OBJECT(media), "delete-event",
			G_CALLBACK(pidgin_media_delete_event_cb), media);

	media->priv->recv_progressbars =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	media->priv->remote_videos =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

static gchar *
create_key(const gchar *session_id, const gchar *participant)
{
	return g_strdup_printf("%s_%s", session_id, participant);
}

static void
pidgin_media_insert_widget(PidginMedia *gtkmedia, GtkWidget *widget,
		const gchar *session_id, const gchar *participant)
{
	gchar *key = create_key(session_id, participant);
	PurpleMediaSessionType type =
			purple_media_get_session_type(gtkmedia->priv->media, session_id);

	if (type & PURPLE_MEDIA_AUDIO)
		g_hash_table_insert(gtkmedia->priv->recv_progressbars, key, widget);
	else if (type & PURPLE_MEDIA_VIDEO)
		g_hash_table_insert(gtkmedia->priv->remote_videos, key, widget);
}

static GtkWidget *
pidgin_media_get_widget(PidginMedia *gtkmedia,
		const gchar *session_id, const gchar *participant)
{
	GtkWidget *widget = NULL;
	gchar *key = create_key(session_id, participant);
	PurpleMediaSessionType type =
			purple_media_get_session_type(gtkmedia->priv->media, session_id);

	if (type & PURPLE_MEDIA_AUDIO)
		widget = g_hash_table_lookup(gtkmedia->priv->recv_progressbars, key);
	else if (type & PURPLE_MEDIA_VIDEO)
		widget = g_hash_table_lookup(gtkmedia->priv->remote_videos, key);

	g_free(key);
	return widget;
}

static void
pidgin_media_remove_widget(PidginMedia *gtkmedia,
		const gchar *session_id, const gchar *participant)
{
	GtkWidget *widget = pidgin_media_get_widget(gtkmedia, session_id, participant);

	if (widget) {
		PurpleMediaSessionType type =
				purple_media_get_session_type(gtkmedia->priv->media, session_id);
		gchar *key = create_key(session_id, participant);
		GtkRequisition req;

		if (type & PURPLE_MEDIA_AUDIO) {
			g_hash_table_remove(gtkmedia->priv->recv_progressbars, key);

			if (g_hash_table_size(gtkmedia->priv->recv_progressbars) == 0 &&
				gtkmedia->priv->send_progress) {

				gtk_widget_destroy(gtkmedia->priv->send_progress);
				gtkmedia->priv->send_progress = NULL;

				gtk_widget_destroy(gtkmedia->priv->mute);
				gtkmedia->priv->mute = NULL;
			}
		} else if (type & PURPLE_MEDIA_VIDEO) {
			g_hash_table_remove(gtkmedia->priv->remote_videos, key);

			if (g_hash_table_size(gtkmedia->priv->remote_videos) == 0 &&
				gtkmedia->priv->local_video) {

				gtk_widget_destroy(gtkmedia->priv->local_video);
				gtkmedia->priv->local_video = NULL;

				gtk_widget_destroy(gtkmedia->priv->pause);
				gtkmedia->priv->pause = NULL;
			}
		}

		g_free(key);

		gtk_widget_destroy(widget);

		gtk_widget_get_preferred_size(GTK_WIDGET(gtkmedia), NULL, &req);
		gtk_window_resize(GTK_WINDOW(gtkmedia), req.width, req.height);
	}
}

static void
level_message_cb(PurpleMedia *media, gchar *session_id, gchar *participant,
		double level, PidginMedia *gtkmedia)
{
	GtkWidget *progress = NULL;
	if (participant == NULL)
		progress = gtkmedia->priv->send_progress;
	else
		progress = pidgin_media_get_widget(gtkmedia, session_id, participant);

	if (progress)
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), level);
}


static void
pidgin_media_disconnect_levels(PurpleMedia *media, PidginMedia *gtkmedia)
{
	PurpleMediaManager *manager = purple_media_get_manager(media);
	GstElement *element = purple_media_manager_get_pipeline(manager);
	gulong handler_id = g_signal_handler_find(G_OBJECT(gst_pipeline_get_bus(GST_PIPELINE(element))),
						  G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0,
						  NULL, G_CALLBACK(level_message_cb), gtkmedia);
	if (handler_id)
		g_signal_handler_disconnect(G_OBJECT(gst_pipeline_get_bus(GST_PIPELINE(element))),
					    handler_id);
}

static void
pidgin_media_dispose(GObject *media)
{
	PidginMedia *gtkmedia = PIDGIN_MEDIA(media);
	purple_debug_info("gtkmedia", "pidgin_media_dispose\n");

	if (gtkmedia->priv->media) {
		purple_request_close_with_handle(gtkmedia);
		purple_media_remove_output_windows(gtkmedia->priv->media);
		pidgin_media_disconnect_levels(gtkmedia->priv->media, gtkmedia);
		g_object_unref(gtkmedia->priv->media);
		gtkmedia->priv->media = NULL;
	}

	if (gtkmedia->priv->ui) {
		g_object_unref(gtkmedia->priv->ui);
		gtkmedia->priv->ui = NULL;
	}

	if (gtkmedia->priv->timeout_id != 0)
		g_source_remove(gtkmedia->priv->timeout_id);

	if (gtkmedia->priv->recv_progressbars) {
		g_hash_table_destroy(gtkmedia->priv->recv_progressbars);
		g_hash_table_destroy(gtkmedia->priv->remote_videos);
		gtkmedia->priv->recv_progressbars = NULL;
		gtkmedia->priv->remote_videos = NULL;
	}

	G_OBJECT_CLASS(parent_class)->dispose(media);
}

static void
pidgin_media_finalize(GObject *media)
{
	/* PidginMedia *gtkmedia = PIDGIN_MEDIA(media); */
	purple_debug_info("gtkmedia", "pidgin_media_finalize\n");

	G_OBJECT_CLASS(parent_class)->finalize(media);
}

static void
pidgin_media_emit_message(PidginMedia *gtkmedia, const char *msg)
{
	PurpleConversation *conv = purple_conversations_find_with_account(
			gtkmedia->priv->screenname,
			purple_media_get_account(gtkmedia->priv->media));
	if (conv != NULL)
		purple_conversation_write_system_message(conv, msg, 0);
}

typedef struct
{
	PidginMedia *gtkmedia;
	gchar *session_id;
	gchar *participant;
} PidginMediaRealizeData;

static gboolean
realize_cb_cb(PidginMediaRealizeData *data)
{
	PidginMediaPrivate *priv = data->gtkmedia->priv;
	GdkWindow *window = NULL;

	if (data->participant == NULL)
		window = gtk_widget_get_window(priv->local_video);
	else {
		GtkWidget *widget = pidgin_media_get_widget(data->gtkmedia,
				data->session_id, data->participant);
		if (widget)
			window = gtk_widget_get_window(widget);
	}

	if (window) {
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
#		error "Unsupported GDK windowing system"
#endif

		purple_media_set_output_window(priv->media, data->session_id,
				data->participant, window_id);
	}

	g_free(data->session_id);
	g_free(data->participant);
	g_free(data);
	return FALSE;
}

static void
realize_cb(GtkWidget *widget, PidginMediaRealizeData *data)
{
	g_timeout_add(0, (GSourceFunc)realize_cb_cb, data);
}

static void
pidgin_media_error_cb(PidginMedia *media, const char *error, PidginMedia *gtkmedia)
{
	PurpleConversation *conv = purple_conversations_find_with_account(
			gtkmedia->priv->screenname,
			purple_media_get_account(gtkmedia->priv->media));
	if (conv != NULL) {
		purple_conversation_write_system_message(
			conv, error, PURPLE_MESSAGE_ERROR);
	} else {
		purple_notify_error(NULL, NULL, _("Media error"), error,
			purple_request_cpar_from_conversation(conv));
	}

	gtk_statusbar_push(GTK_STATUSBAR(gtkmedia->priv->statusbar),
			0, error);
}

static void
pidgin_media_accept_cb(PurpleMedia *media, int index)
{
	purple_media_stream_info(media, PURPLE_MEDIA_INFO_ACCEPT,
			NULL, NULL, TRUE);
}

static void
pidgin_media_reject_cb(PurpleMedia *media, int index)
{
	GList *iter = purple_media_get_session_ids(media);
	for (; iter; iter = g_list_delete_link(iter, iter)) {
		const gchar *sessionid = iter->data;
		if (!purple_media_accepted(media, sessionid, NULL))
			purple_media_stream_info(media, PURPLE_MEDIA_INFO_REJECT,
					sessionid, NULL, TRUE);
	}
}

static gboolean
pidgin_request_timeout_cb(PidginMedia *gtkmedia)
{
	PurpleAccount *account;
	PurpleBuddy *buddy;
	const gchar *alias;
	PurpleMediaSessionType type;
	gchar *message = NULL;

	account = purple_media_get_account(gtkmedia->priv->media);
	buddy = purple_blist_find_buddy(account, gtkmedia->priv->screenname);
	alias = buddy ? purple_buddy_get_contact_alias(buddy) :
			gtkmedia->priv->screenname;
	type = gtkmedia->priv->request_type;
	gtkmedia->priv->timeout_id = 0;

	if (type & PURPLE_MEDIA_AUDIO && type & PURPLE_MEDIA_VIDEO) {
		message = g_strdup_printf(_("%s wishes to start an audio/video session with you."),
				alias);
	} else if (type & PURPLE_MEDIA_AUDIO) {
		message = g_strdup_printf(_("%s wishes to start an audio session with you."),
				alias);
	} else if (type & PURPLE_MEDIA_VIDEO) {
		message = g_strdup_printf(_("%s wishes to start a video session with you."),
				alias);
	}

	gtkmedia->priv->request_type = PURPLE_MEDIA_NONE;
	if (!purple_media_accepted(gtkmedia->priv->media, NULL, NULL)) {
		purple_request_accept_cancel(gtkmedia, _("Incoming Call"),
			message, NULL, PURPLE_DEFAULT_ACTION_NONE,
			purple_request_cpar_from_account(account),
			gtkmedia->priv->media, pidgin_media_accept_cb,
			pidgin_media_reject_cb);
	}
	pidgin_media_emit_message(gtkmedia, message);
	g_free(message);
	return FALSE;
}

static void
pidgin_media_input_volume_changed(GtkScaleButton *range, double value,
		PurpleMedia *media)
{
	double val = (double)value * 100.0;
	purple_media_set_input_volume(media, NULL, val);
}

static void
pidgin_media_output_volume_changed(GtkScaleButton *range, double value,
		PurpleMedia *media)
{
	double val = (double)value * 100.0;
	purple_media_set_output_volume(media, NULL, NULL, val);
}

static void
destroy_parent_widget_cb(GtkWidget *widget, GtkWidget *parent)
{
	g_return_if_fail(GTK_IS_WIDGET(parent));

	gtk_widget_destroy(parent);
}

static GtkWidget *
pidgin_media_add_audio_widget(PidginMedia *gtkmedia,
		PurpleMediaSessionType type, const gchar *sid)
{
	GtkWidget *volume_widget, *progress_parent, *volume, *progress;
	double value;

	static const gchar * input_volume_icons[] = {
		"microphone-sensitivity-muted-symbolic",
		"microphone-sensitivity-high-symbolic",
		"microphone-sensitivity-low-symbolic",
		"microphone-sensitivity-medium-symbolic",
		NULL
	};

	if (type & PURPLE_MEDIA_SEND_AUDIO) {
		value = purple_prefs_get_int(
			"/purple/media/audio/volume/input");
	} else if (type & PURPLE_MEDIA_RECV_AUDIO) {
		value = purple_prefs_get_int(
			"/purple/media/audio/volume/output");
	} else
		g_return_val_if_reached(NULL);

	/* Setup widget structure */
	volume_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	progress_parent = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(volume_widget),
			progress_parent, TRUE, TRUE, 0);

	/* Volume button */
	volume = gtk_volume_button_new();
	gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), value/100.0);
	gtk_box_pack_end(GTK_BOX(volume_widget),
			volume, FALSE, FALSE, 0);

	/* Volume level indicator */
	progress = gtk_progress_bar_new();
	gtk_widget_set_size_request(progress, 250, 10);
	gtk_box_pack_end(GTK_BOX(progress_parent), progress, TRUE, FALSE, 0);

	if (type & PURPLE_MEDIA_SEND_AUDIO) {
		g_signal_connect (G_OBJECT(volume), "value-changed",
				G_CALLBACK(pidgin_media_input_volume_changed),
				gtkmedia->priv->media);
		gtk_scale_button_set_icons(GTK_SCALE_BUTTON(volume),
				input_volume_icons);

		gtkmedia->priv->send_progress = progress;
	} else if (type & PURPLE_MEDIA_RECV_AUDIO) {
		g_signal_connect (G_OBJECT(volume), "value-changed",
				G_CALLBACK(pidgin_media_output_volume_changed),
				gtkmedia->priv->media);

		pidgin_media_insert_widget(gtkmedia, progress, sid, gtkmedia->priv->screenname);
	}

	g_signal_connect(G_OBJECT(progress), "destroy",
			G_CALLBACK(destroy_parent_widget_cb),
			volume_widget);

	gtk_widget_show_all(volume_widget);

	return volume_widget;
}

static void
phone_dtmf_pressed_cb(GtkButton *button, gpointer user_data)
{
	PidginMedia *gtkmedia = user_data;
	gint num;
	gchar *sid;

	num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "dtmf-digit"));
	sid = g_object_get_data(G_OBJECT(button), "session-id");

	purple_media_send_dtmf(gtkmedia->priv->media, sid, num, 25, 50);
}

static inline GtkWidget *
phone_create_button(const gchar *text_hi, const gchar *text_lo)
{
	GtkWidget *button;
	GtkWidget *label_hi;
	GtkWidget *label_lo;
	GtkWidget *grid;
	const gchar *text_hi_local;

	if (text_hi)
		text_hi_local = _(text_hi);
	else
		text_hi_local = "";

	grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(grid), TRUE);

	button = gtk_button_new();
	label_hi = gtk_label_new(text_hi_local);
	gtk_box_pack_end(GTK_BOX(grid), label_hi, FALSE, TRUE, 0);
	label_lo = gtk_label_new(text_lo);
	gtk_label_set_use_markup(GTK_LABEL(label_lo), TRUE);
	gtk_box_pack_end(GTK_BOX(grid), label_lo, FALSE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(button), grid);

	return button;
}

static struct phone_label {
	gchar *subtext;
	gchar *text;
	gchar chr;
} phone_labels[] = {
	{"<b>1</b>", NULL, '1'},
	/* Translators note: These are the letters on the keys of a numeric
	   keypad; translate according to the tables in §7 of ETSI ES 202 130:
           http://webapp.etsi.org/WorkProgram/Report_WorkItem.asp?WKI_ID=11730
         */
	 /* Letters on the '2' key of a numeric keypad */
	{"<b>2</b>", N_("ABC"), '2'},
	 /* Letters on the '3' key of a numeric keypad */
	{"<b>3</b>", N_("DEF"), '3'},
	 /* Letters on the '4' key of a numeric keypad */
	{"<b>4</b>", N_("GHI"), '4'},
	 /* Letters on the '5' key of a numeric keypad */
	{"<b>5</b>", N_("JKL"), '5'},
	 /* Letters on the '6' key of a numeric keypad */
	{"<b>6</b>", N_("MNO"), '6'},
	 /* Letters on the '7' key of a numeric keypad */
	{"<b>7</b>", N_("PQRS"), '7'},
	 /* Letters on the '8' key of a numeric keypad */
	{"<b>8</b>", N_("TUV"), '8'},
	 /* Letters on the '9' key of a numeric keypad */
	{"<b>9</b>", N_("WXYZ"), '9'},
	{"<b>*</b>", NULL, '*'},
	{"<b>0</b>", NULL, '0'},
	{"<b>#</b>", NULL, '#'},
	{NULL, NULL, 0}
};

static gboolean
pidgin_media_dtmf_key_press_event_cb(GtkWidget *widget,
		GdkEvent *event, gpointer user_data)
{
	PidginMedia *gtkmedia = user_data;
	GdkEventKey *key = (GdkEventKey *) event;

	if (event->type != GDK_KEY_PRESS) {
		return FALSE;
	}

	if ((key->keyval >= GDK_KEY_0 && key->keyval <= GDK_KEY_9) ||
		key->keyval == GDK_KEY_asterisk ||
		key->keyval == GDK_KEY_numbersign) {
		gchar *sid = g_object_get_data(G_OBJECT(widget), "session-id");

		purple_media_send_dtmf(gtkmedia->priv->media, sid, key->keyval, 25, 50);
	}

	return FALSE;
}

static GtkWidget *
pidgin_media_add_dtmf_widget(PidginMedia *gtkmedia,
		PurpleMediaSessionType type, const gchar *_sid)
{
	GtkWidget *grid = gtk_grid_new();
	GtkWidget *button;
	gint index = 0;
	GtkWindow *win = &gtkmedia->parent;

	gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

	/* Add buttons */
	for (index = 0; phone_labels[index].subtext != NULL; index++) {
		button = phone_create_button(phone_labels[index].text,
				phone_labels[index].subtext);
		g_signal_connect(button, "pressed",
				G_CALLBACK(phone_dtmf_pressed_cb), gtkmedia);
		g_object_set_data(G_OBJECT(button), "dtmf-digit",
				GINT_TO_POINTER(phone_labels[index].chr));
		g_object_set_data_full(G_OBJECT(button), "session-id",
				g_strdup(_sid), g_free);
		gtk_grid_attach(GTK_GRID(grid), button,
				index % 3, index / 3, 1, 1);
		g_object_set(button, "expand", TRUE, "margin", 2, NULL);
	}

	g_signal_connect(G_OBJECT(win), "key-press-event",
		G_CALLBACK(pidgin_media_dtmf_key_press_event_cb), gtkmedia);
	g_object_set_data_full(G_OBJECT(win), "session-id",
		g_strdup(_sid), g_free);

	gtk_widget_show_all(grid);

	return grid;
}

static void
pidgin_media_ready_cb(PurpleMedia *media, PidginMedia *gtkmedia, const gchar *sid)
{
	GtkWidget *send_widget = NULL, *recv_widget = NULL, *button_widget = NULL;
	PurpleMediaSessionType type =
			purple_media_get_session_type(media, sid);
	GdkPixbuf *icon = NULL;

	if (gtkmedia->priv->recv_widget == NULL
			&& type & (PURPLE_MEDIA_RECV_VIDEO |
			PURPLE_MEDIA_RECV_AUDIO)) {
		recv_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(gtkmedia->priv->display),
				recv_widget, TRUE, TRUE, 0);
		gtk_widget_show(recv_widget);
	} else {
		recv_widget = gtkmedia->priv->recv_widget;
	}
	if (gtkmedia->priv->send_widget == NULL
			&& type & (PURPLE_MEDIA_SEND_VIDEO |
			PURPLE_MEDIA_SEND_AUDIO)) {
		send_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(gtkmedia->priv->display),
				send_widget, FALSE, TRUE, 0);
		button_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_end(GTK_BOX(recv_widget), button_widget,
				FALSE, TRUE, 0);
		gtk_widget_show(send_widget);

		/* Hold button */
		gtkmedia->priv->hold =
				gtk_toggle_button_new_with_mnemonic(_("_Hold"));
		gtk_box_pack_end(GTK_BOX(button_widget), gtkmedia->priv->hold,
				FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->hold);
		g_signal_connect(gtkmedia->priv->hold, "toggled",
				G_CALLBACK(pidgin_media_hold_toggled),
				gtkmedia);
	} else {
		send_widget = gtkmedia->priv->send_widget;
		button_widget = gtkmedia->priv->button_widget;
	}

	if (type & PURPLE_MEDIA_RECV_VIDEO) {
		PidginMediaRealizeData *data;
		GtkWidget *aspect;
		GtkWidget *remote_video;

		aspect = gtk_aspect_frame_new(NULL, 0, 0, 4.0/3.0, FALSE);
		gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(recv_widget), aspect, TRUE, TRUE, 0);

		data = g_new0(PidginMediaRealizeData, 1);
		data->gtkmedia = gtkmedia;
		data->session_id = g_strdup(sid);
		data->participant = g_strdup(gtkmedia->priv->screenname);

		remote_video = pidgin_create_video_widget();
		g_signal_connect(G_OBJECT(remote_video), "realize",
				G_CALLBACK(realize_cb), data);
		gtk_container_add(GTK_CONTAINER(aspect), remote_video);
		gtk_widget_set_size_request (GTK_WIDGET(remote_video), 320, 240);
		g_signal_connect(G_OBJECT(remote_video), "destroy",
				G_CALLBACK(destroy_parent_widget_cb), aspect);

		gtk_widget_show(remote_video);
		gtk_widget_show(aspect);

		pidgin_media_insert_widget(gtkmedia, remote_video,
				data->session_id, data->participant);
	}

	if (type & PURPLE_MEDIA_SEND_VIDEO && !gtkmedia->priv->local_video) {
		PidginMediaRealizeData *data;
		GtkWidget *aspect;
		GtkWidget *local_video;

		aspect = gtk_aspect_frame_new(NULL, 0, 0, 4.0/3.0, TRUE);
		gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(send_widget), aspect, FALSE, TRUE, 0);

		data = g_new0(PidginMediaRealizeData, 1);
		data->gtkmedia = gtkmedia;
		data->session_id = g_strdup(sid);
		data->participant = NULL;

		local_video = pidgin_create_video_widget();
		g_signal_connect(G_OBJECT(local_video), "realize",
				G_CALLBACK(realize_cb), data);
		gtk_container_add(GTK_CONTAINER(aspect), local_video);
		gtk_widget_set_size_request (GTK_WIDGET(local_video), 80, 60);
		g_signal_connect(G_OBJECT(local_video), "destroy",
				G_CALLBACK(destroy_parent_widget_cb), aspect);

		gtk_widget_show(local_video);
		gtk_widget_show(aspect);

		gtkmedia->priv->pause =
				gtk_toggle_button_new_with_mnemonic(_("_Pause"));
		gtk_box_pack_end(GTK_BOX(button_widget), gtkmedia->priv->pause,
				FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->pause);
		g_signal_connect(gtkmedia->priv->pause, "toggled",
				G_CALLBACK(pidgin_media_pause_toggled),
				gtkmedia);

		gtkmedia->priv->local_video = local_video;
	}
	if (type & PURPLE_MEDIA_RECV_AUDIO) {
		gtk_box_pack_end(GTK_BOX(recv_widget),
				pidgin_media_add_audio_widget(gtkmedia,
				PURPLE_MEDIA_RECV_AUDIO, sid), FALSE, FALSE, 0);
	}

	if (type & PURPLE_MEDIA_SEND_AUDIO) {
		gtkmedia->priv->mute =
				gtk_toggle_button_new_with_mnemonic(_("_Mute"));
		gtk_box_pack_end(GTK_BOX(button_widget), gtkmedia->priv->mute,
				FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->mute);
		g_signal_connect(gtkmedia->priv->mute, "toggled",
				G_CALLBACK(pidgin_media_mute_toggled),
				gtkmedia);

		gtk_box_pack_end(GTK_BOX(recv_widget),
				pidgin_media_add_audio_widget(gtkmedia,
				PURPLE_MEDIA_SEND_AUDIO, sid), FALSE, FALSE, 0);

		gtk_box_pack_end(GTK_BOX(recv_widget),
				pidgin_media_add_dtmf_widget(gtkmedia,
				PURPLE_MEDIA_SEND_AUDIO, sid), FALSE, FALSE, 0);
	}

	if (type & PURPLE_MEDIA_AUDIO &&
			gtkmedia->priv->level_handler_id == 0) {
		gtkmedia->priv->level_handler_id = g_signal_connect(
				media, "level", G_CALLBACK(level_message_cb),
				gtkmedia);
	}

	if (send_widget != NULL)
		gtkmedia->priv->send_widget = send_widget;
	if (recv_widget != NULL)
		gtkmedia->priv->recv_widget = recv_widget;
	if (button_widget != NULL) {
		gtkmedia->priv->button_widget = button_widget;
		gtk_widget_show(GTK_WIDGET(button_widget));
	}

	if (purple_media_is_initiator(media, sid, NULL) == FALSE) {
		if (gtkmedia->priv->timeout_id != 0)
			g_source_remove(gtkmedia->priv->timeout_id);
		gtkmedia->priv->request_type |= type;
		gtkmedia->priv->timeout_id = g_timeout_add(500,
				(GSourceFunc)pidgin_request_timeout_cb,
				gtkmedia);
	}

	/* set the window icon according to the type */
	if (type & PURPLE_MEDIA_VIDEO) {
		icon = gtk_widget_render_icon(GTK_WIDGET(gtkmedia),
			PIDGIN_STOCK_TOOLBAR_VIDEO_CALL,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_LARGE), NULL);
	} else if (type & PURPLE_MEDIA_AUDIO) {
		icon = gtk_widget_render_icon(GTK_WIDGET(gtkmedia),
			PIDGIN_STOCK_TOOLBAR_AUDIO_CALL,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_LARGE), NULL);
	}

	if (icon) {
		gtk_window_set_icon(GTK_WINDOW(gtkmedia), icon);
		g_object_unref(icon);
	}

	gtk_widget_show(gtkmedia->priv->display);
}

static void
pidgin_media_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, PidginMedia *gtkmedia)
{
	purple_debug_info("gtkmedia", "state: %d sid: %s name: %s\n",
			state, sid ? sid : "(null)", name ? name : "(null)");
	if (state == PURPLE_MEDIA_STATE_END) {
		if (sid != NULL && name != NULL) {
			pidgin_media_remove_widget(gtkmedia, sid, name);
		} else if (sid == NULL && name == NULL) {
			pidgin_media_emit_message(gtkmedia,
					_("The call has been terminated."));
			gtk_widget_destroy(GTK_WIDGET(gtkmedia));
		}
	} else if (state == PURPLE_MEDIA_STATE_NEW &&
			sid != NULL && name != NULL) {
		pidgin_media_ready_cb(media, gtkmedia, sid);
	}
}

static void
pidgin_media_stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local,
		PidginMedia *gtkmedia)
{
	if (type == PURPLE_MEDIA_INFO_REJECT) {
		pidgin_media_emit_message(gtkmedia,
				_("You have rejected the call."));
	} else if (type == PURPLE_MEDIA_INFO_ACCEPT) {
		if (local == TRUE)
			purple_request_close_with_handle(gtkmedia);
		pidgin_media_set_state(gtkmedia, PIDGIN_MEDIA_ACCEPTED);
		pidgin_media_emit_message(gtkmedia, _("Call in progress."));
		gtk_statusbar_push(GTK_STATUSBAR(gtkmedia->priv->statusbar),
				0, _("Call in progress"));
		gtk_widget_show(GTK_WIDGET(gtkmedia));
	}
}

static void
pidgin_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PidginMedia *media;
	g_return_if_fail(PIDGIN_IS_MEDIA(object));

	media = PIDGIN_MEDIA(object);
	switch (prop_id) {
		case PROP_MEDIA:
		{
			if (media->priv->media)
				g_object_unref(media->priv->media);
			media->priv->media = g_value_get_object(value);
			g_object_ref(media->priv->media);

			if (purple_media_is_initiator(media->priv->media,
					 NULL, NULL) == TRUE)
				pidgin_media_set_state(media, PIDGIN_MEDIA_WAITING);
			else
				pidgin_media_set_state(media, PIDGIN_MEDIA_REQUESTED);

			g_signal_connect(G_OBJECT(media->priv->media), "error",
				G_CALLBACK(pidgin_media_error_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "state-changed",
				G_CALLBACK(pidgin_media_state_changed_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "stream-info",
				G_CALLBACK(pidgin_media_stream_info_cb), media);
			break;
		}
		case PROP_SCREENNAME:
			g_free(media->priv->screenname);
			media->priv->screenname = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
pidgin_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	PidginMedia *media;
	g_return_if_fail(PIDGIN_IS_MEDIA(object));

	media = PIDGIN_MEDIA(object);

	switch (prop_id) {
		case PROP_MEDIA:
			g_value_set_object(value, media->priv->media);
			break;
		case PROP_SCREENNAME:
			g_value_set_string(value, media->priv->screenname);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static GtkWidget *
pidgin_media_new(PurpleMedia *media, const gchar *screenname)
{
	PidginMedia *gtkmedia = g_object_new(pidgin_media_get_type(),
					     "media", media,
					     "screenname", screenname, NULL);
	return GTK_WIDGET(gtkmedia);
}

static void
pidgin_media_set_state(PidginMedia *gtkmedia, PidginMediaState state)
{
	gtkmedia->priv->state = state;
}

static gboolean
pidgin_media_new_cb(PurpleMediaManager *manager, PurpleMedia *media,
		PurpleAccount *account, gchar *screenname, gpointer nul)
{
	PidginMedia *gtkmedia = PIDGIN_MEDIA(
			pidgin_media_new(media, screenname));
	PurpleBuddy *buddy = purple_blist_find_buddy(account, screenname);
	const gchar *alias = buddy ?
			purple_buddy_get_contact_alias(buddy) : screenname;
	gtk_window_set_title(GTK_WINDOW(gtkmedia), alias);

	if (purple_media_is_initiator(media, NULL, NULL) == TRUE)
		gtk_widget_show(GTK_WIDGET(gtkmedia));

	return TRUE;
}
#endif  /* USE_VV */

void
pidgin_medias_init(void)
{
#ifdef USE_VV
	PurpleMediaManager *manager = purple_media_manager_get();
	PurpleMediaElementInfo *video_src = NULL;
	PurpleMediaElementInfo *video_sink = NULL;
	PurpleMediaElementInfo *audio_src = NULL;
	PurpleMediaElementInfo *audio_sink = NULL;
	const char *pref;

	pref = purple_prefs_get_string(
			PIDGIN_PREFS_ROOT "/vvconfig/video/src/device");
	if (pref)
		video_src = purple_media_manager_get_element_info(manager, pref);
	if (!video_src) {
		pref = "autovideosrc";
		purple_prefs_set_string(
			PIDGIN_PREFS_ROOT "/vvconfig/video/src/device", pref);
		video_src = purple_media_manager_get_element_info(manager,
				pref);
	}

	pref = purple_prefs_get_string(
			PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device");
	if (pref)
		video_sink = purple_media_manager_get_element_info(manager, pref);
	if (!video_sink) {
		pref = "autovideosink";
		purple_prefs_set_string(
			PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device", pref);
		video_sink = purple_media_manager_get_element_info(manager,
				pref);
	}

	pref = purple_prefs_get_string(
			PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device");
	if (pref)
		audio_src = purple_media_manager_get_element_info(manager, pref);
	if (!audio_src) {
		pref = "autoaudiosrc";
		purple_prefs_set_string(
			PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device", pref);
		audio_src = purple_media_manager_get_element_info(manager,
				pref);
	}

	pref = purple_prefs_get_string(
			PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device");
	if (pref)
		audio_sink = purple_media_manager_get_element_info(manager, pref);
	if (!audio_sink) {
		pref = "autoaudiosink";
		purple_prefs_set_string(
			PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device", pref);
		audio_sink = purple_media_manager_get_element_info(manager,
				pref);
	}

	g_signal_connect(G_OBJECT(manager), "init-media",
			 G_CALLBACK(pidgin_media_new_cb), NULL);

	purple_media_manager_set_ui_caps(manager,
			PURPLE_MEDIA_CAPS_AUDIO |
			PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION |
			PURPLE_MEDIA_CAPS_VIDEO |
			PURPLE_MEDIA_CAPS_VIDEO_SINGLE_DIRECTION |
			PURPLE_MEDIA_CAPS_AUDIO_VIDEO);

	purple_debug_info("gtkmedia", "Registering media element types\n");
	purple_media_manager_set_active_element(manager, video_src);
	purple_media_manager_set_active_element(manager, video_sink);
	purple_media_manager_set_active_element(manager, audio_src);
	purple_media_manager_set_active_element(manager, audio_sink);
#endif
}

