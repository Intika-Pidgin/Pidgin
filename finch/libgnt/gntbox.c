/**
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

#include "gntinternal.h"
#include "gntbox.h"
#include "gntstyle.h"
#include "gntutils.h"

#include <string.h>

#define PROP_LAST_RESIZE_S "last-resize"
#define PROP_SIZE_QUEUED_S "size-queued"

enum
{
	PROP_0,
	PROP_VERTICAL,
	PROP_HOMO        /* ... */
};

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;

static GntWidget * find_focusable_widget(GntBox *box);

static void
add_to_focus(gpointer value, gpointer data)
{
	GntBox *box = GNT_BOX(data);
	GntWidget *w = GNT_WIDGET(value);

	if (GNT_IS_BOX(w))
		g_list_foreach(GNT_BOX(w)->list, add_to_focus, box);
	else if (GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_CAN_TAKE_FOCUS))
		box->focus = g_list_append(box->focus, w);
}

static void
get_title_thingies(GntBox *box, char *title, int *p, int *r)
{
	GntWidget *widget = GNT_WIDGET(box);
	int len;
	char *end = (char*)gnt_util_onscreen_width_to_pointer(title, widget->priv.width - 4, &len);

	if (p)
		*p = (widget->priv.width - len) / 2;
	if (r)
		*r = (widget->priv.width + len) / 2;
	*end = '\0';
}

static void
gnt_box_draw(GntWidget *widget)
{
	GntBox *box = GNT_BOX(widget);

	if (box->focus == NULL && widget->parent == NULL)
		g_list_foreach(box->list, add_to_focus, box);

	g_list_foreach(box->list, (GFunc)gnt_widget_draw, NULL);

	if (box->title && !GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
	{
		int pos, right;
		char *title = g_strdup(box->title);

		get_title_thingies(box, title, &pos, &right);

		if (gnt_widget_has_focus(widget))
			wbkgdset(widget->window, '\0' | gnt_color_pair(GNT_COLOR_TITLE));
		else
			wbkgdset(widget->window, '\0' | gnt_color_pair(GNT_COLOR_TITLE_D));
		mvwaddch(widget->window, 0, pos-1, ACS_RTEE | gnt_color_pair(GNT_COLOR_NORMAL));
		mvwaddstr(widget->window, 0, pos, C_(title));
		mvwaddch(widget->window, 0, right, ACS_LTEE | gnt_color_pair(GNT_COLOR_NORMAL));
		g_free(title);
	}

	gnt_box_sync_children(box);
}

static void
reposition_children(GntWidget *widget)
{
	GList *iter;
	GntBox *box = GNT_BOX(widget);
	int w, h, curx, cury, max;
	gboolean has_border = FALSE;

	w = h = 0;
	max = 0;
	curx = widget->priv.x;
	cury = widget->priv.y;
	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_NO_BORDER))
	{
		has_border = TRUE;
		curx += 1;
		cury += 1;
	}

	for (iter = box->list; iter; iter = iter->next)
	{
		if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(iter->data), GNT_WIDGET_INVISIBLE))
			continue;
		gnt_widget_set_position(GNT_WIDGET(iter->data), curx, cury);
		gnt_widget_get_size(GNT_WIDGET(iter->data), &w, &h);
		if (box->vertical)
		{
			if (h)
			{
				cury += h + box->pad;
				if (max < w)
					max = w;
			}
		}
		else
		{
			if (w)
			{
				curx += w + box->pad;
				if (max < h)
					max = h;
			}
		}
	}

	if (has_border)
	{
		curx += 1;
		cury += 1;
		max += 2;
	}

	if (box->list)
	{
		if (box->vertical)
			cury -= box->pad;
		else
			curx -= box->pad;
	}

	if (box->vertical)
	{
		widget->priv.width = max;
		widget->priv.height = cury - widget->priv.y;
	}
	else
	{
		widget->priv.width = curx - widget->priv.x;
		widget->priv.height = max;
	}
}

static void
gnt_box_set_position(GntWidget *widget, int x, int y)
{
	GList *iter;
	int changex, changey;

	changex = widget->priv.x - x;
	changey = widget->priv.y - y;

	for (iter = GNT_BOX(widget)->list; iter; iter = iter->next)
	{
		GntWidget *w = GNT_WIDGET(iter->data);
		gnt_widget_set_position(w, w->priv.x - changex,
				w->priv.y - changey);
	}
}

static void
gnt_box_size_request(GntWidget *widget)
{
	GntBox *box = GNT_BOX(widget);
	GList *iter;
	int maxw = 0, maxh = 0;

	g_list_foreach(box->list, (GFunc)gnt_widget_size_request, NULL);

	for (iter = box->list; iter; iter = iter->next)
	{
		int w, h;
		gnt_widget_get_size(GNT_WIDGET(iter->data), &w, &h);
		if (maxh < h)
			maxh = h;
		if (maxw < w)
			maxw = w;
	}

	for (iter = box->list; iter; iter = iter->next)
	{
		int w, h;
		GntWidget *wid = GNT_WIDGET(iter->data);

		gnt_widget_get_size(wid, &w, &h);

		if (box->homogeneous)
		{
			if (box->vertical)
				h = maxh;
			else
				w = maxw;
		}
		if (box->fill)
		{
			if (box->vertical)
				w = maxw;
			else
				h = maxh;
		}

		gnt_widget_confirm_size(wid, w, h);
		gnt_widget_set_size(wid, w, h);
	}

	reposition_children(widget);
}

static void
gnt_box_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
	{
		gnt_widget_size_request(widget);
		find_focusable_widget(GNT_BOX(widget));
	}
	GNTDEBUG;
}

/* Ensures that the current widget can take focus */
static GntWidget *
find_focusable_widget(GntBox *box)
{
	/* XXX: Make sure the widget is visible? */
	if (box->focus == NULL && GNT_WIDGET(box)->parent == NULL)
		g_list_foreach(box->list, add_to_focus, box);

	if (box->active == NULL && box->focus)
		box->active = box->focus->data;

	return box->active;
}

static void
find_next_focus(GntBox *box)
{
	gpointer last = box->active;
	do
	{
		GList *iter = g_list_find(box->focus, box->active);
		if (iter && iter->next)
			box->active = iter->next->data;
		else if (box->focus)
			box->active = box->focus->data;
		if (!GNT_WIDGET_IS_FLAG_SET(box->active, GNT_WIDGET_INVISIBLE) &&
				GNT_WIDGET_IS_FLAG_SET(box->active, GNT_WIDGET_CAN_TAKE_FOCUS))
			break;
	} while (box->active != last);
}

static void
find_prev_focus(GntBox *box)
{
	gpointer last = box->active;

	if (!box->focus)
		return;

	do
	{
		GList *iter = g_list_find(box->focus, box->active);
		if (!iter)
			box->active = box->focus->data;
		else if (!iter->prev)
			box->active = g_list_last(box->focus)->data;
		else
			box->active = iter->prev->data;
		if (!GNT_WIDGET_IS_FLAG_SET(box->active, GNT_WIDGET_INVISIBLE))
			break;
	} while (box->active != last);
}

static gboolean
gnt_box_key_pressed(GntWidget *widget, const char *text)
{
	GntBox *box = GNT_BOX(widget);
	gboolean ret;

	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_DISABLE_ACTIONS))
		return FALSE;

	if (box->active == NULL && !find_focusable_widget(box))
		return FALSE;

	if (gnt_widget_key_pressed(box->active, text))
		return TRUE;

	/* This dance is necessary to make sure that the child widgets get a chance
	   to trigger their bindings first */
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_DISABLE_ACTIONS);
	ret = gnt_widget_key_pressed(widget, text);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_DISABLE_ACTIONS);
	return ret;
}

static gboolean
box_focus_change(GntBox *box, gboolean next)
{
	GntWidget *now;
	now = box->active;

	if (next) {
		find_next_focus(box);
	} else {
		find_prev_focus(box);
	}

	if (now && now != box->active) {
		gnt_widget_set_focus(now, FALSE);
		gnt_widget_set_focus(box->active, TRUE);
		return TRUE;
	}

	return FALSE;
}

static gboolean
action_focus_next(GntBindable *bindable, GList *null)
{
	return box_focus_change(GNT_BOX(bindable), TRUE);
}

static gboolean
action_focus_prev(GntBindable *bindable, GList *null)
{
	return box_focus_change(GNT_BOX(bindable), FALSE);
}

static void
gnt_box_lost_focus(GntWidget *widget)
{
	GntWidget *w = GNT_BOX(widget)->active;
	if (w)
		gnt_widget_set_focus(w, FALSE);
	gnt_widget_draw(widget);
}

static void
gnt_box_gained_focus(GntWidget *widget)
{
	GntWidget *w = GNT_BOX(widget)->active;
	if (w)
		gnt_widget_set_focus(w, TRUE);
	gnt_widget_draw(widget);
}

static void
gnt_box_destroy(GntWidget *w)
{
	GntBox *box = GNT_BOX(w);

	gnt_box_remove_all(box);
	gnt_screen_release(w);
}

static void
gnt_box_expose(GntWidget *widget, int x, int y, int width, int height)
{
	WINDOW *win = newwin(height, width, widget->priv.y + y, widget->priv.x + x);
	copywin(widget->window, win, y, x, 0, 0, height - 1, width - 1, FALSE);
	wrefresh(win);
	delwin(win);
}

static gboolean
gnt_box_confirm_size(GntWidget *widget, int width, int height)
{
	GList *iter;
	GntBox *box = GNT_BOX(widget);
	int wchange, hchange;
	GntWidget *child, *last;

	if (!box->list)
		return TRUE;

	wchange = widget->priv.width - width;
	hchange = widget->priv.height - height;

	if (wchange == 0 && hchange == 0)
		return TRUE;		/* Quit playing games with my size */

	child = NULL;
	last = g_object_get_data(G_OBJECT(box), PROP_LAST_RESIZE_S);

	/* First, make sure all the widgets will fit into the box after resizing. */
	for (iter = box->list; iter; iter = iter->next) {
		GntWidget *wid = iter->data;
		int w, h;

		gnt_widget_get_size(wid, &w, &h);

		if (wid != last && !child && w > 0 && h > 0 &&
				!GNT_WIDGET_IS_FLAG_SET(wid, GNT_WIDGET_INVISIBLE) &&
				gnt_widget_confirm_size(wid, w - wchange, h - hchange)) {
			child = wid;
			break;
		}
	}

	if (!child && (child = last)) {
		int w, h;
		gnt_widget_get_size(child, &w, &h);
		if (!gnt_widget_confirm_size(child, w - wchange, h - hchange))
			child = NULL;
	}

	g_object_set_data(G_OBJECT(box), PROP_SIZE_QUEUED_S, child);

	if (child) {
		for (iter = box->list; iter; iter = iter->next) {
			GntWidget *wid = iter->data;
			int w, h;

			if (wid == child)
				continue;

			gnt_widget_get_size(wid, &w, &h);
			if (box->vertical) {
				/* For a vertical box, if we are changing the width, make sure the widgets
				 * in the box will fit after resizing the width. */
				if (wchange > 0 &&
						w >= child->priv.width &&
						!gnt_widget_confirm_size(wid, w - wchange, h))
					return FALSE;
			} else {
				/* If we are changing the height, make sure the widgets in the box fit after
				 * the resize. */
				if (hchange > 0 &&
						h >= child->priv.height &&
						!gnt_widget_confirm_size(wid, w, h - hchange))
					return FALSE;
			}

		}
	}

	return (child != NULL);
}

static void
gnt_box_size_changed(GntWidget *widget, int oldw, int oldh)
{
	int wchange, hchange;
	GList *i;
	GntBox *box = GNT_BOX(widget);
	GntWidget *wid;
	int tw, th;

	wchange = widget->priv.width - oldw;
	hchange = widget->priv.height - oldh;

	wid = g_object_get_data(G_OBJECT(box), PROP_SIZE_QUEUED_S);
	if (wid) {
		gnt_widget_get_size(wid, &tw, &th);
		gnt_widget_set_size(wid, tw + wchange, th + hchange);
		g_object_set_data(G_OBJECT(box), PROP_SIZE_QUEUED_S, NULL);
		g_object_set_data(G_OBJECT(box), PROP_LAST_RESIZE_S, wid);
	}

	if (box->vertical)
		hchange = 0;
	else
		wchange = 0;

	for (i = box->list; i; i = i->next)
	{
		if (wid != i->data)
		{
			gnt_widget_get_size(GNT_WIDGET(i->data), &tw, &th);
			gnt_widget_set_size(i->data, tw + wchange, th + hchange);
		}
	}

	reposition_children(widget);
}

static gboolean
gnt_box_clicked(GntWidget *widget, GntMouseEvent event, int cx, int cy)
{
	GList *iter;
	for (iter = GNT_BOX(widget)->list; iter; iter = iter->next) {
		int x, y, w, h;
		GntWidget *wid = iter->data;

		gnt_widget_get_position(wid, &x, &y);
		gnt_widget_get_size(wid, &w, &h);

		if (cx >= x && cx < x + w && cy >= y && cy < y + h) {
			if (event <= GNT_MIDDLE_MOUSE_DOWN &&
				GNT_WIDGET_IS_FLAG_SET(wid, GNT_WIDGET_CAN_TAKE_FOCUS)) {
				while (widget->parent)
					widget = widget->parent;
				gnt_box_give_focus_to_child(GNT_BOX(widget), wid);
			}
			return gnt_widget_clicked(wid, event, cx, cy);
		}
	}
	return FALSE;
}

static void
gnt_box_set_property(GObject *obj, guint prop_id, const GValue *value,
		GParamSpec *spec)
{
	GntBox *box = GNT_BOX(obj);
	switch (prop_id) {
		case PROP_VERTICAL:
			box->vertical = g_value_get_boolean(value);
			break;
		case PROP_HOMO:
			box->homogeneous = g_value_get_boolean(value);
			break;
		default:
			g_return_if_reached();
			break;
	}
}

static void
gnt_box_get_property(GObject *obj, guint prop_id, GValue *value,
		GParamSpec *spec)
{
	GntBox *box = GNT_BOX(obj);
	switch (prop_id) {
		case PROP_VERTICAL:
			g_value_set_boolean(value, box->vertical);
			break;
		case PROP_HOMO:
			g_value_set_boolean(value, box->homogeneous);
			break;
		default:
			break;
	}
}

static void
gnt_box_class_init(GntBoxClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	GObjectClass *gclass = G_OBJECT_CLASS(klass);
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_box_destroy;
	parent_class->draw = gnt_box_draw;
	parent_class->expose = gnt_box_expose;
	parent_class->map = gnt_box_map;
	parent_class->size_request = gnt_box_size_request;
	parent_class->set_position = gnt_box_set_position;
	parent_class->key_pressed = gnt_box_key_pressed;
	parent_class->clicked = gnt_box_clicked;
	parent_class->lost_focus = gnt_box_lost_focus;
	parent_class->gained_focus = gnt_box_gained_focus;
	parent_class->confirm_size = gnt_box_confirm_size;
	parent_class->size_changed = gnt_box_size_changed;

	gclass->set_property = gnt_box_set_property;
	gclass->get_property = gnt_box_get_property;
	g_object_class_install_property(gclass,
			PROP_VERTICAL,
			g_param_spec_boolean("vertical", "Vertical",
				"Whether the child widgets in the box should be stacked vertically.",
				TRUE,
				G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
			)
		);
	g_object_class_install_property(gclass,
			PROP_HOMO,
			g_param_spec_boolean("homogeneous", "Homogeneous",
				"Whether the child widgets in the box should have the same size.",
				TRUE,
				G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
			)
		);

	gnt_bindable_class_register_action(bindable, "focus-next", action_focus_next,
			"\t", NULL);
	gnt_bindable_register_binding(bindable, "focus-next", GNT_KEY_RIGHT, NULL);
	gnt_bindable_class_register_action(bindable, "focus-prev", action_focus_prev,
			GNT_KEY_BACK_TAB, NULL);
	gnt_bindable_register_binding(bindable, "focus-prev", GNT_KEY_LEFT, NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), bindable);
}

static void
gnt_box_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GntBox *box = GNT_BOX(widget);
	/* Initially make both the height and width resizable.
	 * Update the flags as necessary when widgets are added to it. */
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X | GNT_WIDGET_GROW_Y);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS | GNT_WIDGET_DISABLE_ACTIONS);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	box->pad = 1;
	box->fill = TRUE;
	GNTDEBUG;
}

/******************************************************************************
 * GntBox API
 *****************************************************************************/
GType
gnt_box_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntBoxClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_box_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntBox),
			0,						/* n_preallocs		*/
			gnt_box_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntBox",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_box_new(gboolean homo, gboolean vert)
{
	GntWidget *widget = g_object_new(GNT_TYPE_BOX, NULL);
	GntBox *box = GNT_BOX(widget);

	box->homogeneous = homo;
	box->vertical = vert;
	box->alignment = vert ? GNT_ALIGN_LEFT : GNT_ALIGN_MID;

	return widget;
}

void gnt_box_add_widget(GntBox *b, GntWidget *widget)
{
	b->list = g_list_append(b->list, widget);
	widget->parent = GNT_WIDGET(b);
}

void gnt_box_set_title(GntBox *b, const char *title)
{
	char *prev = b->title;
	GntWidget *w = GNT_WIDGET(b);
	b->title = g_strdup(title);
	if (w->window && !GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_NO_BORDER)) {
		/* Erase the old title */
		int pos, right;
		get_title_thingies(b, prev, &pos, &right);
		mvwhline(w->window, 0, pos - 1, ACS_HLINE | gnt_color_pair(GNT_COLOR_NORMAL),
				right - pos + 2);
	}
	g_free(prev);
}

void gnt_box_set_pad(GntBox *box, int pad)
{
	box->pad = pad;
	/* XXX: Perhaps redraw if already showing? */
}

void gnt_box_set_toplevel(GntBox *box, gboolean set)
{
	GntWidget *widget = GNT_WIDGET(box);
	if (set)
	{
		GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
	}
	else
	{
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
		GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
	}
}

void gnt_box_sync_children(GntBox *box)
{
	GList *iter;
	GntWidget *widget = GNT_WIDGET(box);
	int pos = 1;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		pos = 0;

	if (!box->active)
		find_focusable_widget(box);

	for (iter = box->list; iter; iter = iter->next)
	{
		GntWidget *w = GNT_WIDGET(iter->data);
		int height, width;
		int x, y;

		if (GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_INVISIBLE))
			continue;

		if (GNT_IS_BOX(w))
			gnt_box_sync_children(GNT_BOX(w));

		gnt_widget_get_size(w, &width, &height);

		x = w->priv.x - widget->priv.x;
		y = w->priv.y - widget->priv.y;

		if (box->vertical)
		{
			x = pos;
			if (box->alignment == GNT_ALIGN_RIGHT)
				x += widget->priv.width - width;
			else if (box->alignment == GNT_ALIGN_MID)
				x += (widget->priv.width - width)/2;
			if (x + width > widget->priv.width - pos)
				x -= x + width - (widget->priv.width - pos);
		}
		else
		{
			y = pos;
			if (box->alignment == GNT_ALIGN_BOTTOM)
				y += widget->priv.height - height;
			else if (box->alignment == GNT_ALIGN_MID)
				y += (widget->priv.height - height)/2;
			if (y + height >= widget->priv.height - pos)
				y = widget->priv.height - height - pos;
		}

		copywin(w->window, widget->window, 0, 0,
				y, x, y + height - 1, x + width - 1, FALSE);
		gnt_widget_set_position(w, x + widget->priv.x, y + widget->priv.y);
		if (w == box->active) {
			wmove(widget->window, y + getcury(w->window), x + getcurx(w->window));
		}
	}
}

void gnt_box_set_alignment(GntBox *box, GntAlignment alignment)
{
	box->alignment = alignment;
}

void gnt_box_remove(GntBox *box, GntWidget *widget)
{
	box->list = g_list_remove(box->list, widget);
	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_CAN_TAKE_FOCUS)
			&& GNT_WIDGET(box)->parent == NULL && box->focus)
	{
		if (widget == box->active)
		{
			find_next_focus(box);
			if (box->active == widget) /* There's only one widget */
				box->active = NULL;
		}
		box->focus = g_list_remove(box->focus, widget);
	}

	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(box), GNT_WIDGET_MAPPED))
		gnt_widget_draw(GNT_WIDGET(box));
}

void gnt_box_remove_all(GntBox *box)
{
	g_list_foreach(box->list, (GFunc)gnt_widget_destroy, NULL);
	g_list_free(box->list);
	g_list_free(box->focus);
	box->list = NULL;
	box->focus = NULL;
	GNT_WIDGET(box)->priv.width = 0;
	GNT_WIDGET(box)->priv.height = 0;
}

void gnt_box_readjust(GntBox *box)
{
	GList *iter;
	GntWidget *wid;
	int width, height;

	if (GNT_WIDGET(box)->parent != NULL)
		return;

	for (iter = box->list; iter; iter = iter->next)
	{
		GntWidget *w = iter->data;
		if (GNT_IS_BOX(w))
			gnt_box_readjust(GNT_BOX(w));
		else
		{
			GNT_WIDGET_UNSET_FLAGS(w, GNT_WIDGET_MAPPED);
			w->priv.width = 0;
			w->priv.height = 0;
		}
	}

	wid = GNT_WIDGET(box);
	GNT_WIDGET_UNSET_FLAGS(wid, GNT_WIDGET_MAPPED);
	wid->priv.width = 0;
	wid->priv.height = 0;

	if (wid->parent == NULL)
	{
		g_list_free(box->focus);
		box->focus = NULL;
		box->active = NULL;
		gnt_widget_size_request(wid);
		gnt_widget_get_size(wid, &width, &height);
		gnt_screen_resize_widget(wid, width, height);
		find_focusable_widget(box);
	}
}

void gnt_box_set_fill(GntBox *box, gboolean fill)
{
	box->fill = fill;
}

void gnt_box_move_focus(GntBox *box, int dir)
{
	GntWidget *now;

	if (box->active == NULL)
	{
		find_focusable_widget(box);
		return;
	}

	now = box->active;

	if (dir == 1)
		find_next_focus(box);
	else if (dir == -1)
		find_prev_focus(box);

	if (now && now != box->active)
	{
		gnt_widget_set_focus(now, FALSE);
		gnt_widget_set_focus(box->active, TRUE);
	}

	if (GNT_WIDGET(box)->window)
		gnt_widget_draw(GNT_WIDGET(box));
}

void gnt_box_give_focus_to_child(GntBox *box, GntWidget *widget)
{
	GList *find;
	gpointer now;

	while (GNT_WIDGET(box)->parent)
		box = GNT_BOX(GNT_WIDGET(box)->parent);

	find = g_list_find(box->focus, widget);
	now = box->active;
	if (find)
		box->active = widget;
	if (now && now != box->active)
	{
		gnt_widget_set_focus(now, FALSE);
		gnt_widget_set_focus(box->active, TRUE);
	}

	if (GNT_WIDGET(box)->window)
		gnt_widget_draw(GNT_WIDGET(box));
}

