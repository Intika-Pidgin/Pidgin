/**
 * @file mediamanager.c Media Manager API
 * @ingroup core
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#include "internal.h"

#include "connection.h"
#include "mediamanager.h"
#include "media.h"

#ifdef USE_FARSIGHT

#include <gst/farsight/fs-conference-iface.h>

struct _PurpleMediaManagerPrivate
{
	GList *medias;
};

#define PURPLE_MEDIA_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerPrivate))

static void purple_media_manager_class_init (PurpleMediaManagerClass *klass);
static void purple_media_manager_init (PurpleMediaManager *media);
static void purple_media_manager_finalize (GObject *object);

static GObjectClass *parent_class = NULL;



enum {
	INIT_MEDIA,
	LAST_SIGNAL
};
static guint purple_media_manager_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_FARSIGHT_SESSION,
	PROP_NAME,
	PROP_CONNECTION,
	PROP_MIC_ELEMENT,
	PROP_SPEAKER_ELEMENT,
};

GType
purple_media_manager_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMediaManagerClass),
			NULL,
			NULL,
			(GClassInitFunc) purple_media_manager_class_init,
			NULL,
			NULL,
			sizeof(PurpleMediaManager),
			0,
			(GInstanceInitFunc) purple_media_manager_init,
			NULL
		};
		type = g_type_register_static(G_TYPE_OBJECT, "PurpleMediaManager", &info, 0);
	}
	return type;
}


static void
purple_media_manager_class_init (PurpleMediaManagerClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);
	
	gobject_class->finalize = purple_media_manager_finalize;

	purple_media_manager_signals[INIT_MEDIA] = g_signal_new ("init-media",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, PURPLE_TYPE_MEDIA);
	g_type_class_add_private(klass, sizeof(PurpleMediaManagerPrivate));
}

static void
purple_media_manager_init (PurpleMediaManager *media)
{
	media->priv = PURPLE_MEDIA_MANAGER_GET_PRIVATE(media);
	media->priv->medias = NULL;
}

static void
purple_media_manager_finalize (GObject *media)
{
	parent_class->finalize(media);
}

PurpleMediaManager *
purple_media_manager_get()
{
	static PurpleMediaManager *manager = NULL;

	if (manager == NULL)
		manager = PURPLE_MEDIA_MANAGER(g_object_new(purple_media_manager_get_type(), NULL));
	return manager;
}

PurpleMedia *
purple_media_manager_create_media(PurpleMediaManager *manager,
				  PurpleConnection *gc,
				  const char *conference_type,
				  const char *remote_user)
{
	PurpleMedia *media;
	FsConference *conference = FS_CONFERENCE(gst_element_factory_make(conference_type, NULL));
	GstStateChangeReturn ret = gst_element_set_state(GST_ELEMENT(conference), GST_STATE_READY);

	if (ret == GST_STATE_CHANGE_FAILURE) {
		purple_conv_present_error(remote_user,
					  purple_connection_get_account(gc),
					  _("Error creating conference."));
		return NULL;
	}

	media = PURPLE_MEDIA(g_object_new(purple_media_get_type(),
			     "screenname", remote_user,
			     "connection", gc, 
			     "farsight-conference", conference,
			     NULL));
	manager->priv->medias = g_list_append(manager->priv->medias, media);
	g_signal_emit(manager, purple_media_manager_signals[INIT_MEDIA], 0, media);
	return media;
}

#endif  /* USE_FARSIGHT */
