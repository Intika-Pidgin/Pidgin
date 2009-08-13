/**
 * @file media.c Account API
 * @ingroup core
 *
 * Pidgin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include "debug.h"
#include "internal.h"
#include "connection.h"
#include "media.h"
#include "mediamanager.h"
#include "pidgin.h"
#include "request.h"

#include "gtkmedia.h"
#include "gtkutils.h"

#ifdef USE_VV
#include "media-gst.h"

#ifdef _WIN32
#include <gdk/gdkwin32.h>
#endif

#include <gst/interfaces/xoverlay.h>

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

	GtkItemFactory *item_factory;
	GtkWidget *menubar;
	GtkWidget *statusbar;

	GtkWidget *mute;
	GtkWidget *pause;

	GtkWidget *send_progress;
	GtkWidget *recv_progress;

	PidginMediaState state;

	GtkWidget *display;
	GtkWidget *send_widget;
	GtkWidget *recv_widget;
	GtkWidget *button_widget;
	GtkWidget *local_video;
	GtkWidget *remote_video;

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
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class, PROP_SCREENNAME,
			g_param_spec_string("screenname",
			"Screenname",
			"The screenname of the user this session is with.",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(PidginMediaPrivate));
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
menu_hangup(gpointer data, guint action, GtkWidget *item)
{
	PidginMedia *gtkmedia = PIDGIN_MEDIA(data);
	purple_media_stream_info(gtkmedia->priv->media,
			PURPLE_MEDIA_INFO_HANGUP, NULL, NULL, TRUE);
}

static GtkItemFactoryEntry menu_items[] = {
	{ N_("/_Media"), NULL, NULL, 0, "<Branch>", NULL },
	{ N_("/Media/_Hangup"), NULL, menu_hangup, 0, "<Item>", NULL },
};

static gint menu_item_count = sizeof(menu_items) / sizeof(menu_items[0]);

static const char *
item_factory_translate_func (const char *path, gpointer func_data)
{
	return _(path);
}

static GtkWidget *
setup_menubar(PidginMedia *window)
{
	GtkAccelGroup *accel_group;
	GtkWidget *menu;

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
	g_object_unref(accel_group);

	window->priv->item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR,
			"<main>", accel_group);

	gtk_item_factory_set_translate_func(window->priv->item_factory,
			(GtkTranslateFunc)item_factory_translate_func,
			NULL, NULL);

	gtk_item_factory_create_items(window->priv->item_factory,
			menu_item_count, menu_items, window);
	g_signal_connect(G_OBJECT(accel_group), "accel-changed",
			G_CALLBACK(pidgin_save_accels_cb), NULL);

	menu = gtk_item_factory_get_widget(
			window->priv->item_factory, "<main>");

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

	vbox = gtk_vbox_new(FALSE, 0);
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

	media->priv->display = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(media->priv->display),
			PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), media->priv->display,
			TRUE, TRUE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(vbox);

	g_signal_connect(G_OBJECT(media), "delete-event",
			G_CALLBACK(pidgin_media_delete_event_cb), media);
}

static void
level_message_cb(PurpleMedia *media, gchar *session_id, gchar *participant,
		double level, PidginMedia *gtkmedia)
{
	GtkWidget *progress;
	if (participant == NULL)
		progress = gtkmedia->priv->send_progress;
	else
		progress = gtkmedia->priv->recv_progress;
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

	if (gtkmedia->priv->item_factory) {
		g_object_unref(gtkmedia->priv->item_factory);
		gtkmedia->priv->item_factory = NULL;
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
	PurpleConversation *conv = purple_find_conversation_with_account(
			PURPLE_CONV_TYPE_ANY, gtkmedia->priv->screenname,
			purple_media_get_account(gtkmedia->priv->media));
	if (conv != NULL)
		purple_conversation_write(conv, NULL, msg,
				PURPLE_MESSAGE_SYSTEM, time(NULL));
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
	gulong window_id;

#ifdef _WIN32
	if (data->participant == NULL)
		window_id = GDK_WINDOW_HWND(priv->local_video->window);
	else
		window_id = GDK_WINDOW_HWND(priv->remote_video->window);
#elif defined(HAVE_X11)
	if (data->participant == NULL)
		window_id = GDK_WINDOW_XWINDOW(priv->local_video->window);
	else
		window_id = GDK_WINDOW_XWINDOW(priv->remote_video->window);
#else
#	error "Unsupported windowing system"
#endif

	purple_media_set_output_window(priv->media, data->session_id,
			data->participant, window_id);

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
	PurpleConversation *conv = purple_find_conversation_with_account(
			PURPLE_CONV_TYPE_ANY, gtkmedia->priv->screenname,
			purple_media_get_account(gtkmedia->priv->media));
	if (conv != NULL)
		purple_conversation_write(conv, NULL, error,
				PURPLE_MESSAGE_ERROR, time(NULL));
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
	purple_media_stream_info(media, PURPLE_MEDIA_INFO_REJECT,
			NULL, NULL, TRUE);
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
	buddy = purple_find_buddy(account, gtkmedia->priv->screenname);
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

	purple_request_accept_cancel(gtkmedia, _("Incoming Call"),
			message, NULL, PURPLE_DEFAULT_ACTION_NONE,
			(void*)account, gtkmedia->priv->screenname, NULL,
			gtkmedia->priv->media,
			pidgin_media_accept_cb,
			pidgin_media_reject_cb);
	pidgin_media_emit_message(gtkmedia, message);
	g_free(message);
	return FALSE;
}

static void
#if GTK_CHECK_VERSION(2,12,0)
pidgin_media_input_volume_changed(GtkScaleButton *range, double value,
		PurpleMedia *media)
{
	double val = (double)value * 100.0;
#else
pidgin_media_input_volume_changed(GtkRange *range, PurpleMedia *media)
{
	double val = (double)gtk_range_get_value(GTK_RANGE(range));
#endif
	purple_media_set_input_volume(media, NULL, val);
}

static void
#if GTK_CHECK_VERSION(2,12,0)
pidgin_media_output_volume_changed(GtkScaleButton *range, double value,
		PurpleMedia *media)
{
	double val = (double)value * 100.0;
#else
pidgin_media_output_volume_changed(GtkRange *range, PurpleMedia *media)
{
	double val = (double)gtk_range_get_value(GTK_RANGE(range));
#endif
	purple_media_set_output_volume(media, NULL, NULL, val);
}

static GtkWidget *
pidgin_media_add_audio_widget(PidginMedia *gtkmedia,
		PurpleMediaSessionType type)
{
	GtkWidget *volume_widget, *progress_parent, *volume, *progress;
	double value;

	if (type & PURPLE_MEDIA_SEND_AUDIO) {
		value = purple_prefs_get_int(
			"/purple/media/audio/volume/input");
	} else if (type & PURPLE_MEDIA_RECV_AUDIO) {
		value = purple_prefs_get_int(
			"/purple/media/audio/volume/output");
	} else
		g_return_val_if_reached(NULL);

#if GTK_CHECK_VERSION(2,12,0)
	/* Setup widget structure */
	volume_widget = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	progress_parent = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(volume_widget),
			progress_parent, TRUE, TRUE, 0);

	/* Volume button */
	volume = gtk_volume_button_new();
	gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), value/100.0);
	gtk_box_pack_end(GTK_BOX(volume_widget),
			volume, FALSE, FALSE, 0);
#else
	/* Setup widget structure */
	volume_widget = gtk_vbox_new(FALSE, 0);
	progress_parent = volume_widget;

	/* Volume slider */
	volume = gtk_hscale_new_with_range(0.0, 100.0, 5.0);
	gtk_range_set_increments(GTK_RANGE(volume), 5.0, 25.0);
	gtk_range_set_value(GTK_RANGE(volume), value);
	gtk_scale_set_draw_value(GTK_SCALE(volume), FALSE);
	gtk_box_pack_end(GTK_BOX(volume_widget),
			volume, TRUE, FALSE, 0);
#endif

	/* Volume level indicator */
	progress = gtk_progress_bar_new();
	gtk_widget_set_size_request(progress, 250, 10);
	gtk_box_pack_end(GTK_BOX(progress_parent), progress, TRUE, FALSE, 0);

	if (type & PURPLE_MEDIA_SEND_AUDIO) {
		g_signal_connect (G_OBJECT(volume), "value-changed",
				G_CALLBACK(pidgin_media_input_volume_changed),
				gtkmedia->priv->media);
		gtkmedia->priv->send_progress = progress;
	} else if (type & PURPLE_MEDIA_RECV_AUDIO) {
		g_signal_connect (G_OBJECT(volume), "value-changed",
				G_CALLBACK(pidgin_media_output_volume_changed),
				gtkmedia->priv->media);
		gtkmedia->priv->recv_progress = progress;
	}

	gtk_widget_show_all(volume_widget);

	return volume_widget;
}

static void
pidgin_media_ready_cb(PurpleMedia *media, PidginMedia *gtkmedia, const gchar *sid)
{
	GtkWidget *send_widget = NULL, *recv_widget = NULL, *button_widget = NULL;
	PurpleMediaSessionType type =
			purple_media_get_session_type(media, sid);

	if (gtkmedia->priv->recv_widget == NULL
			&& type & (PURPLE_MEDIA_RECV_VIDEO |
			PURPLE_MEDIA_RECV_AUDIO)) {
		recv_widget = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);	
		gtk_box_pack_start(GTK_BOX(gtkmedia->priv->display),
				recv_widget, TRUE, TRUE, 0);
		gtk_widget_show(recv_widget);
	} else
		recv_widget = gtkmedia->priv->recv_widget;
	if (gtkmedia->priv->send_widget == NULL
			&& type & (PURPLE_MEDIA_SEND_VIDEO |
			PURPLE_MEDIA_SEND_AUDIO)) {
		send_widget = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(gtkmedia->priv->display),
				send_widget, TRUE, TRUE, 0);
		button_widget = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_end(GTK_BOX(send_widget), button_widget,
				FALSE, FALSE, 0);
		gtk_widget_show(GTK_WIDGET(button_widget));
		gtk_widget_show(send_widget);
	} else {
		send_widget = gtkmedia->priv->send_widget;
		button_widget = gtkmedia->priv->button_widget;
	}

	if (type & PURPLE_MEDIA_RECV_VIDEO) {
		PidginMediaRealizeData *data;
		GtkWidget *aspect;
		GtkWidget *remote_video;
		GdkColor color = {0, 0, 0, 0};

		aspect = gtk_aspect_frame_new(NULL, 0.5, 0.5, 4.0/3.0, FALSE);
		gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(recv_widget), aspect, TRUE, TRUE, 0);

		data = g_new0(PidginMediaRealizeData, 1);
		data->gtkmedia = gtkmedia;
		data->session_id = g_strdup(sid);
		data->participant = g_strdup(gtkmedia->priv->screenname);

		remote_video = gtk_drawing_area_new();
		gtk_widget_modify_bg(remote_video, GTK_STATE_NORMAL, &color);
		g_signal_connect(G_OBJECT(remote_video), "realize",
				G_CALLBACK(realize_cb), data);
		gtk_container_add(GTK_CONTAINER(aspect), remote_video);
		gtk_widget_set_size_request (GTK_WIDGET(remote_video), 320, 240);
		gtk_widget_show(remote_video);
		gtk_widget_show(aspect);

		gtkmedia->priv->remote_video = remote_video;
	}
	if (type & PURPLE_MEDIA_SEND_VIDEO) {
		PidginMediaRealizeData *data;
		GtkWidget *aspect;
		GtkWidget *local_video;
		GdkColor color = {0, 0, 0, 0};

		aspect = gtk_aspect_frame_new(NULL, 0.5, 0.5, 4.0/3.0, FALSE);
		gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(send_widget), aspect, TRUE, TRUE, 0);

		data = g_new0(PidginMediaRealizeData, 1);
		data->gtkmedia = gtkmedia;
		data->session_id = g_strdup(sid);
		data->participant = NULL;

		local_video = gtk_drawing_area_new();
		gtk_widget_modify_bg(local_video, GTK_STATE_NORMAL, &color);
		g_signal_connect(G_OBJECT(local_video), "realize",
				G_CALLBACK(realize_cb), data);
		gtk_container_add(GTK_CONTAINER(aspect), local_video);
		gtk_widget_set_size_request (GTK_WIDGET(local_video), 160, 120);

		gtk_widget_show(local_video);
		gtk_widget_show(aspect);

		gtkmedia->priv->pause =
				gtk_toggle_button_new_with_mnemonic(_("_Pause"));
		g_signal_connect(gtkmedia->priv->pause, "toggled",
				G_CALLBACK(pidgin_media_pause_toggled),
				gtkmedia);
		gtk_box_pack_end(GTK_BOX(button_widget), gtkmedia->priv->pause,
				FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->pause);

		gtkmedia->priv->local_video = local_video;
	}

	if (type & PURPLE_MEDIA_RECV_AUDIO) {
		gtk_box_pack_end(GTK_BOX(recv_widget),
				pidgin_media_add_audio_widget(gtkmedia,
				PURPLE_MEDIA_RECV_AUDIO), FALSE, FALSE, 0);
	}
	if (type & PURPLE_MEDIA_SEND_AUDIO) {
		gtkmedia->priv->mute =
				gtk_toggle_button_new_with_mnemonic("_Mute");
		g_signal_connect(gtkmedia->priv->mute, "toggled",
				G_CALLBACK(pidgin_media_mute_toggled),
				gtkmedia);
		gtk_box_pack_end(GTK_BOX(button_widget), gtkmedia->priv->mute,
				FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->mute);

		gtk_box_pack_end(GTK_BOX(send_widget),
				pidgin_media_add_audio_widget(gtkmedia,
				PURPLE_MEDIA_SEND_AUDIO), FALSE, FALSE, 0);
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
	if (button_widget != NULL)
		gtkmedia->priv->button_widget = button_widget;

	if (purple_media_is_initiator(media, sid, NULL) == FALSE) {
		if (gtkmedia->priv->timeout_id != 0)
			g_source_remove(gtkmedia->priv->timeout_id);
		gtkmedia->priv->request_type |= type;
		gtkmedia->priv->timeout_id = g_timeout_add(500,
				(GSourceFunc)pidgin_request_timeout_cb,
				gtkmedia);
	}

	gtk_widget_show(gtkmedia->priv->display);
}

static void
pidgin_media_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, PidginMedia *gtkmedia)
{
	purple_debug_info("gtkmedia", "state: %d sid: %s name: %s\n",
			state, sid ? sid : "(null)", name ? name : "(null)");
	if (sid == NULL && name == NULL) {
		if (state == PURPLE_MEDIA_STATE_END) {
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
		pidgin_media_set_state(gtkmedia, PIDGIN_MEDIA_ACCEPTED);
		pidgin_media_emit_message(gtkmedia, _("Call in progress."));
		gtk_statusbar_push(GTK_STATUSBAR(gtkmedia->priv->statusbar),
				0, _("Call in progress."));
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
			if (media->priv->screenname)
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
	PurpleBuddy *buddy = purple_find_buddy(account, screenname);
	const gchar *alias = buddy ? 
			purple_buddy_get_contact_alias(buddy) : screenname; 
	gtk_window_set_title(GTK_WINDOW(gtkmedia), alias);

	if (purple_media_is_initiator(media, NULL, NULL) == TRUE)
		gtk_widget_show(GTK_WIDGET(gtkmedia));

	return TRUE;
}

static GstElement *
create_default_video_src(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GstElement *sendbin, *src, *videoscale, *capsfilter;
	GstPad *pad;
	GstPad *ghost;
	GstCaps *caps;

#ifdef _WIN32
	/* autovideosrc doesn't pick ksvideosrc for some reason */
	src = gst_element_factory_make("ksvideosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("dshowvideosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("autovideosrc", NULL);
#else
	src = gst_element_factory_make("gconfvideosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("autovideosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("v4l2src", NULL);
	if (src == NULL)
		src = gst_element_factory_make("v4lsrc", NULL);
#endif
	if (src == NULL) {
		purple_debug_error("gtkmedia", "Unable to find a suitable "
				"element for the default video source.\n");
		return NULL;
	}

	sendbin = gst_bin_new("pidgindefaultvideosrc");
	videoscale = gst_element_factory_make("videoscale", NULL);
	capsfilter = gst_element_factory_make("capsfilter", NULL);

	/* It was recommended to set the size <= 352x288 and framerate <= 20 */
	caps = gst_caps_from_string("video/x-raw-yuv , width=[250,352] , "
			"height=[200,288] , framerate=[1/1,20/1]");
	g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);

	gst_bin_add_many(GST_BIN(sendbin), src,
			videoscale, capsfilter, NULL);
	gst_element_link_many(src, videoscale, capsfilter, NULL);

	pad = gst_element_get_static_pad(capsfilter, "src");
	ghost = gst_ghost_pad_new("ghostsrc", pad);
	gst_object_unref(pad);
	gst_element_add_pad(sendbin, ghost);

	return sendbin;
}

static GstElement *
create_default_video_sink(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GstElement *sink = gst_element_factory_make("gconfvideosink", NULL);
	if (sink == NULL)
		sink = gst_element_factory_make("autovideosink", NULL);
	if (sink == NULL)
		purple_debug_error("gtkmedia", "Unable to find a suitable "
				"element for the default video sink.\n");
	return sink;
}

static GstElement *
create_default_audio_src(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GstElement *src;
	src = gst_element_factory_make("gconfaudiosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("autoaudiosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("alsasrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("osssrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("dshowaudiosrc", NULL);
	if (src == NULL) {
		purple_debug_error("gtkmedia", "Unable to find a suitable "
				"element for the default audio source.\n");
		return NULL;
	}
	gst_element_set_name(src, "pidgindefaultaudiosrc");
	return src;
}

static GstElement *
create_default_audio_sink(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GstElement *sink;
	sink = gst_element_factory_make("gconfaudiosink", NULL);
	if (sink == NULL)
		sink = gst_element_factory_make("autoaudiosink",NULL);
	if (sink == NULL) {
		purple_debug_error("gtkmedia", "Unable to find a suitable "
				"element for the default audio sink.\n");
		return NULL;
	}
	return sink;
}
#endif  /* USE_VV */

void
pidgin_medias_init(void)
{
#ifdef USE_VV
	PurpleMediaManager *manager = purple_media_manager_get();
	PurpleMediaElementInfo *default_video_src =
			g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "pidgindefaultvideosrc",
			"name", "Pidgin Default Video Source",
			"type", PURPLE_MEDIA_ELEMENT_VIDEO
					| PURPLE_MEDIA_ELEMENT_SRC
					| PURPLE_MEDIA_ELEMENT_ONE_SRC
					| PURPLE_MEDIA_ELEMENT_UNIQUE,
			"create-cb", create_default_video_src, NULL);
	PurpleMediaElementInfo *default_video_sink =
			g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "pidgindefaultvideosink",
			"name", "Pidgin Default Video Sink",
			"type", PURPLE_MEDIA_ELEMENT_VIDEO
					| PURPLE_MEDIA_ELEMENT_SINK
					| PURPLE_MEDIA_ELEMENT_ONE_SINK,
			"create-cb", create_default_video_sink, NULL);
	PurpleMediaElementInfo *default_audio_src =
			g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "pidgindefaultaudiosrc",
			"name", "Pidgin Default Audio Source",
			"type", PURPLE_MEDIA_ELEMENT_AUDIO
					| PURPLE_MEDIA_ELEMENT_SRC
					| PURPLE_MEDIA_ELEMENT_ONE_SRC
					| PURPLE_MEDIA_ELEMENT_UNIQUE,
			"create-cb", create_default_audio_src, NULL);
	PurpleMediaElementInfo *default_audio_sink =
			g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "pidgindefaultaudiosink",
			"name", "Pidgin Default Audio Sink",
			"type", PURPLE_MEDIA_ELEMENT_AUDIO
					| PURPLE_MEDIA_ELEMENT_SINK
					| PURPLE_MEDIA_ELEMENT_ONE_SINK,
			"create-cb", create_default_audio_sink, NULL);

	g_signal_connect(G_OBJECT(manager), "init-media",
			 G_CALLBACK(pidgin_media_new_cb), NULL);

	purple_media_manager_set_ui_caps(manager, 
			PURPLE_MEDIA_CAPS_AUDIO |
			PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION |
			PURPLE_MEDIA_CAPS_VIDEO |
			PURPLE_MEDIA_CAPS_VIDEO_SINGLE_DIRECTION |
			PURPLE_MEDIA_CAPS_AUDIO_VIDEO);

	purple_debug_info("gtkmedia", "Registering media element types\n");
	purple_media_manager_set_active_element(manager, default_video_src);
	purple_media_manager_set_active_element(manager, default_video_sink);
	purple_media_manager_set_active_element(manager, default_audio_src);
	purple_media_manager_set_active_element(manager, default_audio_sink);
#endif
}

