#include "gnttextview.h"
#include "gntutils.h"

#include <string.h>

enum
{
	SIGS = 1,
};

typedef struct
{
	GntTextFormatFlags tvflag;
	chtype flags;
	int start;
	int end;     /* This is the next byte of the last character of this segment */
} GntTextSegment;

typedef struct
{
	GList *segments;         /* A list of GntTextSegments */
	int length;              /* The current length of the line so far (ie. onscreen width) */
	gboolean soft;           /* TRUE if it's an overflow from prev. line */
} GntTextLine;

typedef struct
{
	char *name;
	int start;
	int end;
} GntTextTag;

static GntWidgetClass *parent_class = NULL;

static void
gnt_text_view_draw(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);
	int i = 0;
	GList *lines;
	int rows, scrcol;

	werase(widget->window);

	for (i = 0, lines = view->list; i < widget->priv.height && lines; i++, lines = lines->next)
	{
		GList *iter;
		GntTextLine *line = lines->data;

		wmove(widget->window, widget->priv.height - 1 - i, 0);

		for (iter = line->segments; iter; iter = iter->next)
		{
			GntTextSegment *seg = iter->data;
			char *end = view->string->str + seg->end;
			char back = *end;
			*end = '\0';
			wattrset(widget->window, seg->flags);
			wprintw(widget->window, "%s", (view->string->str + seg->start));
			*end = back;
		}
		wattroff(widget->window, A_UNDERLINE | A_BLINK | A_REVERSE);
		whline(widget->window, ' ', widget->priv.width - line->length - 1);
	}

	scrcol = widget->priv.width - 1;
	rows = widget->priv.height - 2;
	if (rows > 0)
	{
		int total = g_list_length(g_list_first(view->list));
		int showing, position, up, down;

		showing = rows * rows / total + 1;
		showing = MIN(rows, showing);

		total -= rows;
		up = g_list_length(lines);
		down = total - up;

		position = (rows - showing) * up / MAX(1, up + down);
		position = MAX((lines != NULL), position);

		if (showing + position > rows)
			position = rows - showing;
		
		if (showing + position == rows && view->list && view->list->prev)
			position = MAX(1, rows - 1 - showing);
		else if (showing + position < rows && view->list && !view->list->prev)
			position = rows - showing;

		mvwvline(widget->window, position + 1, scrcol,
				ACS_CKBOARD | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D), showing);
	}

	mvwaddch(widget->window, 0, scrcol,
			(lines ? ACS_UARROW : ' ') | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));
	mvwaddch(widget->window, widget->priv.height - 1, scrcol,
			((view->list && view->list->prev) ? ACS_DARROW : ' ') |
				COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));

	GNTDEBUG;
}

static void
gnt_text_view_size_request(GntWidget *widget)
{
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
	{
		gnt_widget_set_size(widget, 64, 20);
	}
}

static void
gnt_text_view_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	GNTDEBUG;
}

static gboolean
gnt_text_view_key_pressed(GntWidget *widget, const char *text)
{
	return FALSE;
}

static void
free_text_segment(gpointer data, gpointer null)
{
	GntTextSegment *seg = data;
	g_free(seg);
}

static void
free_text_line(gpointer data, gpointer null)
{
	GntTextLine *line = data;
	g_list_foreach(line->segments, free_text_segment, NULL);
	g_list_free(line->segments);
	g_free(line);
}

static void
free_tag(gpointer data, gpointer null)
{
	GntTextTag *tag = data;
	g_free(tag->name);
	g_free(tag);
}

static void
gnt_text_view_destroy(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);
	view->list = g_list_first(view->list);
	g_list_foreach(view->list, free_text_line, NULL);
	g_list_free(view->list);
	g_list_foreach(view->tags, free_tag, NULL);
	g_list_free(view->tags);
	g_string_free(view->string, TRUE);
}

static gboolean
gnt_text_view_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	if (event == GNT_MOUSE_SCROLL_UP) {
		gnt_text_view_scroll(GNT_TEXT_VIEW(widget), -1);
	} else if (event == GNT_MOUSE_SCROLL_DOWN) {
		gnt_text_view_scroll(GNT_TEXT_VIEW(widget), 1);
	} else
		return FALSE;
	return TRUE;
}

static void
gnt_text_view_reflow(GntTextView *view)
{
	/* This is pretty ugly, and inefficient. Someone do something about it. */
	GntTextLine *line;
	GList *back, *iter, *list;
	GString *string;
	int pos = 0;    /* no. of 'real' lines */

	list = view->list;
	while (list->prev) {
		line = list->data;
		if (!line->soft)
			pos++;
		list = list->prev;
	}

	back = g_list_last(view->list);
	view->list = NULL;

	string = view->string;
	view->string = NULL;
	gnt_text_view_clear(view);

	view->string = g_string_set_size(view->string, string->len);
	view->string->len = 0;
	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(view), GNT_WIDGET_DRAWING);

	for (; back; back = back->prev) {
		line = back->data;
		if (back->next && !line->soft) {
			gnt_text_view_append_text_with_flags(view, "\n", GNT_TEXT_FLAG_NORMAL);
		}

		for (iter = line->segments; iter; iter = iter->next) {
			GntTextSegment *seg = iter->data;
			char *start = string->str + seg->start;
			char *end = string->str + seg->end;
			char back = *end;
			*end = '\0';
			gnt_text_view_append_text_with_flags(view, start, seg->tvflag);
			*end = back;
		}
		free_text_line(line, NULL);
	}
	g_list_free(list);

	list = view->list = g_list_first(view->list);
	/* Go back to the line that was in view before resizing started */
	while (pos--) {
		while (((GntTextLine*)list->data)->soft)
			list = list->next;
		list = list->next;
	}
	view->list = list;
	GNT_WIDGET_UNSET_FLAGS(GNT_WIDGET(view), GNT_WIDGET_DRAWING);
	if (GNT_WIDGET(view)->window)
		gnt_widget_draw(GNT_WIDGET(view));
	g_string_free(string, TRUE);
}

static void
gnt_text_view_size_changed(GntWidget *widget, int w, int h)
{
	if (w != widget->priv.width) {
		gnt_text_view_reflow(GNT_TEXT_VIEW(widget));
	}
}

static void
gnt_text_view_class_init(GntTextViewClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_text_view_destroy;
	parent_class->draw = gnt_text_view_draw;
	parent_class->map = gnt_text_view_map;
	parent_class->size_request = gnt_text_view_size_request;
	parent_class->key_pressed = gnt_text_view_key_pressed;
	parent_class->clicked = gnt_text_view_clicked;
	parent_class->size_changed = gnt_text_view_size_changed;

	GNTDEBUG;
}

static void
gnt_text_view_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	
	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(instance), GNT_WIDGET_GROW_Y | GNT_WIDGET_GROW_X);

	widget->priv.minw = 5;
	widget->priv.minh = 2;
	GNTDEBUG;
}

/******************************************************************************
 * GntTextView API
 *****************************************************************************/
GType
gnt_text_view_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntTextViewClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_text_view_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntTextView),
			0,						/* n_preallocs		*/
			gnt_text_view_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntTextView",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_text_view_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_TEXTVIEW, NULL);
	GntTextView *view = GNT_TEXT_VIEW(widget);
	GntTextLine *line = g_new0(GntTextLine, 1);

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);

	view->string = g_string_new(NULL);
	view->list = g_list_append(view->list, line);

	return widget;
}

void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextFormatFlags flags)
{
	gnt_text_view_append_text_with_tag(view, text, flags, NULL);
}

void gnt_text_view_append_text_with_tag(GntTextView *view, const char *text,
			GntTextFormatFlags flags, const char *tagname)
{
	GntWidget *widget = GNT_WIDGET(view);
	int fl = 0;
	const char *start, *end;
	GList *list = view->list;
	GntTextLine *line;
	int len;

	if (text == NULL || *text == '\0')
		return;

	fl = gnt_text_format_flag_to_chtype(flags);

	len = view->string->len;
	view->string = g_string_append(view->string, text);

	if (tagname) {
		GntTextTag *tag = g_new0(GntTextTag, 1);
		tag->name = g_strdup(tagname);
		tag->start = len;
		tag->end = view->string->len;
		view->tags = g_list_append(view->tags, tag);
	}

	view->list = g_list_first(view->list);

	start = end = view->string->str + len;

	while (*start) {
		GntTextSegment *seg = NULL;

		if (*end == '\n' || *end == '\r') {
			end++;
			start = end;
			gnt_text_view_next_line(view);
			view->list = g_list_first(view->list);
			continue;
		}

		line = view->list->data;
		if (line->length == widget->priv.width - 1) {
			/* The last added line was exactly the same width as the widget */
			line = g_new0(GntTextLine, 1);
			line->soft = TRUE;
			view->list = g_list_prepend(view->list, line);
		}

		if ((end = strchr(start, '\n')) != NULL ||
			(end = strchr(start, '\r')) != NULL) {
			len = gnt_util_onscreen_width(start, end - 1);
			if (len >= widget->priv.width - line->length - 1) {
				end = NULL;
			}
		}

		if (end == NULL)
			end = gnt_util_onscreen_width_to_pointer(start,
					widget->priv.width - line->length - 1, &len);

		/* Try to append to the previous segment if possible */
		if (line->segments) {
			seg = g_list_last(line->segments)->data;
			if (seg->flags != fl)
				seg = NULL;
		}

		if (seg == NULL) {
			seg = g_new0(GntTextSegment, 1);
			seg->start = start - view->string->str;
			seg->tvflag = flags;
			seg->flags = fl;
			line->segments = g_list_append(line->segments, seg);
		}
		seg->end = end - view->string->str;
		line->length += len;

		start = end;
		if (*end && *end != '\n' && *end != '\r') {
			line = g_new0(GntTextLine, 1);
			line->soft = TRUE;
			view->list = g_list_prepend(view->list, line);
		}
	}

	view->list = list;

	gnt_widget_draw(widget);
}

void gnt_text_view_scroll(GntTextView *view, int scroll)
{
	if (scroll == 0)
	{
		view->list = g_list_first(view->list);
	}
	else if (scroll > 0)
	{
		GList *list = g_list_nth_prev(view->list, scroll);
		if (list == NULL)
			list = g_list_first(view->list);
		view->list = list;
	}
	else if (scroll < 0)
	{
		GList *list = g_list_nth(view->list, -scroll);
		if (list == NULL)
			list = g_list_last(view->list);
		view->list = list;
	}
		
	gnt_widget_draw(GNT_WIDGET(view));
}

void gnt_text_view_next_line(GntTextView *view)
{
	GntTextLine *line = g_new0(GntTextLine, 1);
	GList *list = view->list;
	
	view->list = g_list_prepend(g_list_first(view->list), line);
	view->list = list;
	gnt_widget_draw(GNT_WIDGET(view));
}

chtype gnt_text_format_flag_to_chtype(GntTextFormatFlags flags)
{
	chtype fl = 0;

	if (flags & GNT_TEXT_FLAG_BOLD)
		fl |= A_BOLD;
	if (flags & GNT_TEXT_FLAG_UNDERLINE)
		fl |= A_UNDERLINE;
	if (flags & GNT_TEXT_FLAG_BLINK)
		fl |= A_BLINK;

	if (flags & GNT_TEXT_FLAG_DIM)
		fl |= (A_DIM | COLOR_PAIR(GNT_COLOR_DISABLED));
	else if (flags & GNT_TEXT_FLAG_HIGHLIGHT)
		fl |= (A_DIM | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
	else
		fl |= COLOR_PAIR(GNT_COLOR_NORMAL);

	return fl;
}

void gnt_text_view_clear(GntTextView *view)
{
	GntTextLine *line;

	g_list_foreach(view->list, free_text_line, NULL);
	g_list_free(view->list);
	view->list = NULL;

	line = g_new0(GntTextLine, 1);
	view->list = g_list_append(view->list, line);
	if (view->string)
		g_string_free(view->string, TRUE);
	view->string = g_string_new(NULL);

	if (GNT_WIDGET(view)->window)
		gnt_widget_draw(GNT_WIDGET(view));
}

int gnt_text_view_get_lines_below(GntTextView *view)
{
	int below = 0;
	GList *list = view->list;
	while ((list = list->prev))
		++below;
	return below;
}

int gnt_text_view_get_lines_above(GntTextView *view)
{
	int above = 0;
	GList *list = view->list;
	list = g_list_nth(view->list, GNT_WIDGET(view)->priv.height);
	if (!list)
		return 0;
	while ((list = list->next))
		++above;
	return above;
}

/**
 * XXX: There are quite possibly more than a few bugs here.
 */
int gnt_text_view_tag_change(GntTextView *view, const char *name, const char *text, gboolean all)
{
	GList *alllines = g_list_first(view->list);
	GList *list, *next, *iter, *inext;
	const int text_length = text ? strlen(text) : 0;
	int count = 0;
	for (list = view->tags; list; list = next) {
		GntTextTag *tag = list->data;
		next = list->next;
		if (strcmp(tag->name, name) == 0) {
			int change;
			char *before, *after;

			count++;

			before = g_strndup(view->string->str, tag->start);
			after = g_strdup(view->string->str + tag->end);
			change = (tag->end - tag->start) - text_length;

			g_string_printf(view->string, "%s%s%s", before, text ? text : "", after);
			g_free(before);
			g_free(after);

			/* Update the offsets of the next tags */
			for (iter = next; iter; iter = iter->next) {
				GntTextTag *t = iter->data;
				t->start -= change;
				t->end -= change;
			}

			/* Update the offsets of the segments */
			for (iter = alllines; iter; iter = inext) {
				GList *segs, *snext;
				GntTextLine *line = iter->data;
				inext = iter->next;
				for (segs = line->segments; segs; segs = snext) {
					GntTextSegment *seg = segs->data;
					snext = segs->next;
					if (seg->start >= tag->end) {
						/* The segment is somewhere after the tag */
						seg->start -= change;
						seg->end -= change;
					} else if (seg->end <= tag->start) {
						/* This segment is somewhere in front of the tag */
					} else if (seg->start >= tag->start) {
						/* This segment starts in the middle of the tag */
						if (text == NULL) {
							free_text_segment(seg, NULL);
							line->segments = g_list_delete_link(line->segments, segs);
							if (line->segments == NULL) {
								free_text_line(line, NULL);
								if (view->list == iter) {
									if (inext)
										view->list = inext;
									else
										view->list = iter->prev;
								}
								alllines = g_list_delete_link(alllines, iter);
							}
						} else {
							/* XXX: (null) */
							seg->start = tag->start;
							seg->end = tag->end - change;
						}
						line->length -= change;
						/* XXX: Make things work if the tagged text spans over several lines. */
					} else {
						/* XXX: handle the rest of the conditions */
						g_printerr("WTF! This needs to be handled properly!!\n");
					}
				}
			}
			if (text == NULL) {
				/* Remove the tag */
				view->tags = g_list_delete_link(view->tags, list);
				free_tag(tag, NULL);
			} else {
				tag->end -= change;
			}
			if (!all)
				break;
		}
	}
	return count;
}

