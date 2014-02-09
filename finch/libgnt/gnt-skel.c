/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include "gnt-skel.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_skel_draw(GntWidget *widget)
{
	GNTDEBUG;
}

static void
gnt_skel_size_request(GntWidget *widget)
{
}

static void
gnt_skel_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	GNTDEBUG;
}

static gboolean
gnt_skel_key_pressed(GntWidget *widget, const char *text)
{
	return FALSE;
}

static void
gnt_skel_destroy(GntWidget *widget)
{
}

static void
gnt_skel_class_init(GntSkelClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_skel_destroy;
	parent_class->draw = gnt_skel_draw;
	parent_class->map = gnt_skel_map;
	parent_class->size_request = gnt_skel_size_request;
	parent_class->key_pressed = gnt_skel_key_pressed;

	parent_class->actions = gnt_hash_table_duplicate(parent_class->actions, g_str_hash,
				g_str_equal, NULL, (GDestroyNotify)gnt_widget_action_free);
	parent_class->bindings = gnt_hash_table_duplicate(parent_class->bindings, g_str_hash,
				g_str_equal, NULL, (GDestroyNotify)gnt_widget_action_param_free);

	gnt_widget_actions_read(G_OBJECT_CLASS_TYPE(klass), klass);

	GNTDEBUG;
}

static void
gnt_skel_init(GTypeInstance *instance, gpointer class)
{
	GNTDEBUG;
}

/******************************************************************************
 * GntSkel API
 *****************************************************************************/
GType
gnt_skel_get_type(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntSkelClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_skel_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntSkel),
			0,						/* n_preallocs		*/
			gnt_skel_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntSkel",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_skel_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_SKEL, NULL);
	GntSkel *skel = GNT_SKEL(widget);

	return widget;
}

