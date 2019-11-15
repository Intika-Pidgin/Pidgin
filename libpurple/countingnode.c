/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include "countingnode.h"

typedef struct _PurpleCountingNodePrivate  PurpleCountingNodePrivate;

/* Private data of a counting node */
struct _PurpleCountingNodePrivate {
	int totalsize;    /* The number of children under this node            */
	int currentsize;  /* The number of children under this node
	                     corresponding to online accounts                  */
	int onlinecount;  /* The number of children under this contact who are
	                     currently online                                  */
};

/* Counting node property enums */
enum
{
	PROP_0,
	PROP_TOTAL_SIZE,
	PROP_CURRENT_SIZE,
	PROP_ONLINE_COUNT,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(PurpleCountingNode, purple_counting_node,
		PURPLE_TYPE_BLIST_NODE);

/******************************************************************************
 * API
 *****************************************************************************/
int
purple_counting_node_get_total_size(PurpleCountingNode *counter)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_COUNTING_NODE(counter), -1);

	priv = purple_counting_node_get_instance_private(counter);
	return priv->totalsize;
}

int
purple_counting_node_get_current_size(PurpleCountingNode *counter)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_COUNTING_NODE(counter), -1);

	priv = purple_counting_node_get_instance_private(counter);
	return priv->currentsize;
}

int
purple_counting_node_get_online_count(PurpleCountingNode *counter)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_COUNTING_NODE(counter), -1);

	priv = purple_counting_node_get_instance_private(counter);
	return priv->onlinecount;
}

void
purple_counting_node_change_total_size(PurpleCountingNode *counter, int delta)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_COUNTING_NODE(counter));

	priv = purple_counting_node_get_instance_private(counter);
	purple_counting_node_set_total_size(counter, priv->totalsize + delta);
}

void
purple_counting_node_change_current_size(PurpleCountingNode *counter, int delta)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_COUNTING_NODE(counter));

	priv = purple_counting_node_get_instance_private(counter);
	purple_counting_node_set_current_size(counter, priv->currentsize + delta);
}

void
purple_counting_node_change_online_count(PurpleCountingNode *counter, int delta)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_COUNTING_NODE(counter));

	priv = purple_counting_node_get_instance_private(counter);
	purple_counting_node_set_online_count(counter, priv->onlinecount + delta);
}

void
purple_counting_node_set_total_size(PurpleCountingNode *counter, int totalsize)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_COUNTING_NODE(counter));

	priv = purple_counting_node_get_instance_private(counter);
	priv->totalsize = totalsize;

	g_object_notify_by_pspec(G_OBJECT(counter), properties[PROP_TOTAL_SIZE]);
}

void
purple_counting_node_set_current_size(PurpleCountingNode *counter, int currentsize)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_COUNTING_NODE(counter));

	priv = purple_counting_node_get_instance_private(counter);
	priv->currentsize = currentsize;

	g_object_notify_by_pspec(G_OBJECT(counter), properties[PROP_CURRENT_SIZE]);
}

void
purple_counting_node_set_online_count(PurpleCountingNode *counter, int onlinecount)
{
	PurpleCountingNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_COUNTING_NODE(counter));

	priv = purple_counting_node_get_instance_private(counter);
	priv->onlinecount = onlinecount;

	g_object_notify_by_pspec(G_OBJECT(counter), properties[PROP_ONLINE_COUNT]);
}

/**************************************************************************
 * GObject Stuff
 **************************************************************************/
/* Set method for GObject properties */
static void
purple_counting_node_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleCountingNode *node = PURPLE_COUNTING_NODE(obj);

	switch (param_id) {
		case PROP_TOTAL_SIZE:
			purple_counting_node_set_total_size(node, g_value_get_int(value));
			break;
		case PROP_CURRENT_SIZE:
			purple_counting_node_set_current_size(node, g_value_get_int(value));
			break;
		case PROP_ONLINE_COUNT:
			purple_counting_node_set_online_count(node, g_value_get_int(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_counting_node_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleCountingNode *node = PURPLE_COUNTING_NODE(obj);

	switch (param_id) {
		case PROP_TOTAL_SIZE:
			g_value_set_int(value, purple_counting_node_get_total_size(node));
			break;
		case PROP_CURRENT_SIZE:
			g_value_set_int(value, purple_counting_node_get_current_size(node));
			break;
		case PROP_ONLINE_COUNT:
			g_value_set_int(value, purple_counting_node_get_online_count(node));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_counting_node_init(PurpleCountingNode *counter)
{
}

/* Class initializer function */
static void
purple_counting_node_class_init(PurpleCountingNodeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	/* Setup properties */
	obj_class->get_property = purple_counting_node_get_property;
	obj_class->set_property = purple_counting_node_set_property;

	properties[PROP_TOTAL_SIZE] = g_param_spec_int(
		"total-size",
		"Total size",
		"The number of children under this node.",
		G_MININT, G_MAXINT, 0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_CURRENT_SIZE] = g_param_spec_int(
		"current-size",
		"Current size",
		"The number of children with online accounts.",
		G_MININT, G_MAXINT, 0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
	);

	properties[PROP_ONLINE_COUNT] = g_param_spec_int(
		"online-count",
		"Online count",
		"The number of children that are online.",
		G_MININT, G_MAXINT, 0,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
	);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

