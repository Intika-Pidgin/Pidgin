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
#include "glibcompat.h"
#include "whiteboard.h"
#include "protocol.h"

typedef struct _PurpleWhiteboardPrivate  PurpleWhiteboardPrivate;

/* Private data for a whiteboard */
struct _PurpleWhiteboardPrivate
{
	int state;                      /* State of whiteboard session          */

	PurpleAccount *account;         /* Account associated with this session */
	char *who;                      /* Name of the remote user              */

	/* TODO Remove this and use protocol-specific subclasses. */
	void *proto_data;               /* Protocol specific data               */

	PurpleWhiteboardOps *protocol_ops; /* Protocol operations               */

	GList *draw_list;               /* List of drawing elements/deltas to
	                                   send                                 */
};

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_STATE,
	PROP_ACCOUNT,
	PROP_WHO,
	PROP_DRAW_LIST,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GParamSpec *properties[PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE(PurpleWhiteboard, purple_whiteboard, G_TYPE_OBJECT);

static PurpleWhiteboardUiOps *whiteboard_ui_ops = NULL;
/* static PurpleWhiteboardOps *whiteboard_protocol_ops = NULL; */

static GList *wb_list = NULL;

/*static gboolean auto_accept = TRUE; */

/******************************************************************************
 * API
 *****************************************************************************/
static PurpleWhiteboardUiOps *
purple_whiteboard_ui_ops_copy(PurpleWhiteboardUiOps *ops)
{
	PurpleWhiteboardUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleWhiteboardUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_whiteboard_ui_ops_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleWhiteboardUiOps",
				(GBoxedCopyFunc)purple_whiteboard_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

void purple_whiteboard_set_ui_ops(PurpleWhiteboardUiOps *ops)
{
	whiteboard_ui_ops = ops;
}

void purple_whiteboard_set_protocol_ops(PurpleWhiteboard *wb, PurpleWhiteboardOps *ops)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_if_fail(priv != NULL);

	priv->protocol_ops = ops;
}

PurpleAccount *purple_whiteboard_get_account(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

const char *purple_whiteboard_get_who(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->who;	
}

void purple_whiteboard_set_state(PurpleWhiteboard *wb, int state)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_if_fail(priv != NULL);

	priv->state = state;

	g_object_notify_by_pspec(G_OBJECT(wb), properties[PROP_STATE]);
}

int purple_whiteboard_get_state(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->state;
}

void purple_whiteboard_start(PurpleWhiteboard *wb)
{
	/* Create frontend for whiteboard */
	if(whiteboard_ui_ops && whiteboard_ui_ops->create)
		whiteboard_ui_ops->create(wb);
}

/* Looks through the list of whiteboard sessions for one that is between
 * usernames 'me' and 'who'.  Returns a pointer to a matching whiteboard
 * session; if none match, it returns NULL.
 */
PurpleWhiteboard *purple_whiteboard_get_session(const PurpleAccount *account, const char *who)
{
	PurpleWhiteboard *wb;
	PurpleWhiteboardPrivate *priv;

	GList *l = wb_list;

	/* Look for a whiteboard session between the local user and the remote user
	 */
	while(l != NULL)
	{
		wb = l->data;
		priv = purple_whiteboard_get_instance_private(wb);

		if(priv->account == account && purple_strequal(priv->who, who))
			return wb;

		l = l->next;
	}

	return NULL;
}

void purple_whiteboard_draw_list_destroy(GList *draw_list)
{
	g_list_free(draw_list);
}

gboolean purple_whiteboard_get_dimensions(PurpleWhiteboard *wb, int *width, int *height)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);
	PurpleWhiteboardOps *protocol_ops;

	g_return_val_if_fail(priv != NULL, FALSE);

	protocol_ops = priv->protocol_ops;

	if (protocol_ops && protocol_ops->get_dimensions)
	{
		protocol_ops->get_dimensions(wb, width, height);
		return TRUE;
	}

	return FALSE;
}

void purple_whiteboard_set_dimensions(PurpleWhiteboard *wb, int width, int height)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->set_dimensions)
		whiteboard_ui_ops->set_dimensions(wb, width, height);
}

void purple_whiteboard_send_draw_list(PurpleWhiteboard *wb, GList *list)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);
	PurpleWhiteboardOps *protocol_ops;

	g_return_if_fail(priv != NULL);

	protocol_ops = priv->protocol_ops;

	if (protocol_ops && protocol_ops->send_draw_list)
		protocol_ops->send_draw_list(wb, list);
}

void purple_whiteboard_draw_point(PurpleWhiteboard *wb, int x, int y, int color, int size)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->draw_point)
		whiteboard_ui_ops->draw_point(wb, x, y, color, size);
}

void purple_whiteboard_draw_line(PurpleWhiteboard *wb, int x1, int y1, int x2, int y2, int color, int size)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->draw_line)
		whiteboard_ui_ops->draw_line(wb, x1, y1, x2, y2, color, size);
}

void purple_whiteboard_clear(PurpleWhiteboard *wb)
{
	if(whiteboard_ui_ops && whiteboard_ui_ops->clear)
		whiteboard_ui_ops->clear(wb);
}

void purple_whiteboard_send_clear(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);
	PurpleWhiteboardOps *protocol_ops;

	g_return_if_fail(priv != NULL);

	protocol_ops = priv->protocol_ops;

	if (protocol_ops && protocol_ops->clear)
		protocol_ops->clear(wb);
}

void purple_whiteboard_send_brush(PurpleWhiteboard *wb, int size, int color)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);
	PurpleWhiteboardOps *protocol_ops;

	g_return_if_fail(priv != NULL);

	protocol_ops = priv->protocol_ops;

	if (protocol_ops && protocol_ops->set_brush)
		protocol_ops->set_brush(wb, size, color);
}

gboolean purple_whiteboard_get_brush(PurpleWhiteboard *wb, int *size, int *color)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);
	PurpleWhiteboardOps *protocol_ops;

	g_return_val_if_fail(priv != NULL, FALSE);

	protocol_ops = priv->protocol_ops;

	if (protocol_ops && protocol_ops->get_brush)
	{
		protocol_ops->get_brush(wb, size, color);
		return TRUE;
	}
	return FALSE;
}

void purple_whiteboard_set_brush(PurpleWhiteboard *wb, int size, int color)
{
	if (whiteboard_ui_ops && whiteboard_ui_ops->set_brush)
		whiteboard_ui_ops->set_brush(wb, size, color);
}

GList *purple_whiteboard_get_draw_list(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->draw_list;
}

void purple_whiteboard_set_draw_list(PurpleWhiteboard *wb, GList* draw_list)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_if_fail(priv != NULL);

	priv->draw_list = draw_list;

	g_object_notify_by_pspec(G_OBJECT(wb), properties[PROP_DRAW_LIST]);
}

void purple_whiteboard_set_protocol_data(PurpleWhiteboard *wb, gpointer proto_data)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_if_fail(priv != NULL);

	priv->proto_data = proto_data;
}

gpointer purple_whiteboard_get_protocol_data(PurpleWhiteboard *wb)
{
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->proto_data;
}

void purple_whiteboard_set_ui_data(PurpleWhiteboard *wb, gpointer ui_data)
{
	g_return_if_fail(PURPLE_IS_WHITEBOARD(wb));

	wb->ui_data = ui_data;
}

gpointer purple_whiteboard_get_ui_data(PurpleWhiteboard *wb)
{
	g_return_val_if_fail(PURPLE_IS_WHITEBOARD(wb), NULL);

	return wb->ui_data;
}

/******************************************************************************
 * GObject code
 *****************************************************************************/
/* Set method for GObject properties */
static void
purple_whiteboard_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(obj);
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	switch (param_id) {
		case PROP_STATE:
			purple_whiteboard_set_state(wb, g_value_get_int(value));
			break;
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_WHO:
			priv->who = g_value_dup_string(value);
			break;
		case PROP_DRAW_LIST:
			purple_whiteboard_set_draw_list(wb, g_value_get_pointer(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_whiteboard_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(obj);

	switch (param_id) {
		case PROP_STATE:
			g_value_set_int(value, purple_whiteboard_get_state(wb));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_whiteboard_get_account(wb));
			break;
		case PROP_WHO:
			g_value_set_string(value, purple_whiteboard_get_who(wb));
			break;
		case PROP_DRAW_LIST:
			g_value_set_pointer(value, purple_whiteboard_get_draw_list(wb));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_whiteboard_init(PurpleWhiteboard *wb)
{
}

/* Called when done constructing */
static void
purple_whiteboard_constructed(GObject *object)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(object);
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);
	PurpleProtocol *protocol;

	G_OBJECT_CLASS(purple_whiteboard_parent_class)->constructed(object);

	protocol = purple_connection_get_protocol(
				purple_account_get_connection(priv->account));
	purple_whiteboard_set_protocol_ops(wb,
				purple_protocol_get_whiteboard_ops(protocol));

	/* Start up protocol specifics */
	if(priv->protocol_ops && priv->protocol_ops->start)
		priv->protocol_ops->start(wb);

	wb_list = g_list_append(wb_list, wb);
}

/* GObject finalize function */
static void
purple_whiteboard_finalize(GObject *object)
{
	PurpleWhiteboard *wb = PURPLE_WHITEBOARD(object);
	PurpleWhiteboardPrivate *priv =
			purple_whiteboard_get_instance_private(wb);

	if(wb->ui_data)
	{
		/* Destroy frontend */
		if(whiteboard_ui_ops && whiteboard_ui_ops->destroy)
			whiteboard_ui_ops->destroy(wb);
	}

	/* Do protocol specific session ending procedures */
	if(priv->protocol_ops && priv->protocol_ops->end)
		priv->protocol_ops->end(wb);

	wb_list = g_list_remove(wb_list, wb);

	g_free(priv->who);

	G_OBJECT_CLASS(purple_whiteboard_parent_class)->finalize(object);
}

/* Class initializer function */
static void
purple_whiteboard_class_init(PurpleWhiteboardClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_whiteboard_finalize;
	obj_class->constructed = purple_whiteboard_constructed;

	/* Setup properties */
	obj_class->get_property = purple_whiteboard_get_property;
	obj_class->set_property = purple_whiteboard_set_property;

	properties[PROP_STATE] = g_param_spec_int("state", "State",
				"State of the whiteboard.",
				G_MININT, G_MAXINT, 0,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_ACCOUNT] = g_param_spec_object("account", "Account",
				"The whiteboard's account.", PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	properties[PROP_WHO] = g_param_spec_string("who", "Who",
				"Who you're drawing with.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	properties[PROP_DRAW_LIST] = g_param_spec_pointer("draw-list", "Draw list",
				"A list of points to draw to the buddy.",
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

PurpleWhiteboard *purple_whiteboard_new(PurpleAccount *account, const char *who, int state)
{
	PurpleWhiteboard *wb;
	PurpleProtocol *protocol;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(who != NULL, NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(account));

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	if (PURPLE_PROTOCOL_IMPLEMENTS(protocol, FACTORY_IFACE, whiteboard_new))
		wb = purple_protocol_factory_iface_whiteboard_new(protocol, account,
				who, state);
	else
		wb = g_object_new(PURPLE_TYPE_WHITEBOARD,
			"account", account,
			"who",     who,
			"state",   state,
			NULL
		);

	g_return_val_if_fail(wb != NULL, NULL);

	return wb;
}
