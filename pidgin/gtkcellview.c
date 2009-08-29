/* gtkellview.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02111-1301, USA.
 */

/*
#include <config.h>
*/
#include "gtkcellview.h"
#include <gtk/gtkversion.h>
#if !GTK_CHECK_VERSION(2,6,0)
#if GTK_CHECK_VERSION(2,4,0)
#include <gtk/gtkcelllayout.h>
#else
#include "gtkcelllayout.h"
#endif
#include <gtk/gtksignal.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gobject/gmarshal.h>

#define P_(x) (x)

typedef struct _GtkCellViewCellInfo GtkCellViewCellInfo;
struct _GtkCellViewCellInfo
{
  GtkCellRenderer *cell;

  gint requested_width;
  gint real_width;
  guint expand : 1;
  guint pack : 1;

  GSList *attributes;

  GtkCellLayoutDataFunc func;
  gpointer func_data;
  GDestroyNotify destroy;
};

struct _GtkCellViewPrivate
{
  GtkTreeModel *model;
  GtkTreeRowReference *displayed_row;
  GList *cell_list;
  gint spacing;

  GdkColor background;
  gboolean background_set;
};


static void        gtk_cell_view_class_init               (GtkCellViewClass *klass);
static void        gtk_cell_view_cell_layout_init         (GtkCellLayoutIface *iface);
static void        gtk_cell_view_get_property             (GObject           *object,
                                                           guint             param_id,
                                                           GValue           *value,
                                                           GParamSpec       *pspec);
static void        gtk_cell_view_set_property             (GObject          *object,
                                                           guint             param_id,
                                                           const GValue     *value,
                                                           GParamSpec       *pspec);
static void        gtk_cell_view_init                     (GtkCellView      *cellview);
static void        gtk_cell_view_finalize                 (GObject          *object);
static void        gtk_cell_view_style_set                (GtkWidget        *widget,
                                                           GtkStyle         *previous_style);
static void        gtk_cell_view_size_request             (GtkWidget        *widget,
                                                           GtkRequisition   *requisition);
static void        gtk_cell_view_size_allocate            (GtkWidget        *widget,
                                                           GtkAllocation    *allocation);
static gboolean    gtk_cell_view_expose                   (GtkWidget        *widget,
                                                           GdkEventExpose   *event);
static void        gtk_cell_view_set_valuesv              (GtkCellView      *cellview,
                                                           GtkCellRenderer  *renderer,
                                                           va_list           args);
static GtkCellViewCellInfo *gtk_cell_view_get_cell_info   (GtkCellView      *cellview,
                                                           GtkCellRenderer  *renderer);
static void        gtk_cell_view_set_cell_data            (GtkCellView      *cellview);


static void        gtk_cell_view_cell_layout_pack_start        (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer,
                                                                gboolean               expand);
static void        gtk_cell_view_cell_layout_pack_end          (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer,
                                                                gboolean               expand);
static void        gtk_cell_view_cell_layout_add_attribute     (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer,
                                                                const gchar           *attribute,
                                                                gint                   column);
static void       gtk_cell_view_cell_layout_clear              (GtkCellLayout         *layout);
static void       gtk_cell_view_cell_layout_clear_attributes   (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *renderer);
static void       gtk_cell_view_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *cell,
                                                                GtkCellLayoutDataFunc  func,
                                                                gpointer               func_data,
                                                                GDestroyNotify         destroy);
static void       gtk_cell_view_cell_layout_reorder            (GtkCellLayout         *layout,
                                                                GtkCellRenderer       *cell,
                                                                gint                   position);


enum
{
  PROP_0,
  PROP_BACKGROUND,
  PROP_BACKGROUND_GDK,
  PROP_BACKGROUND_SET
};

static GtkObjectClass *parent_class = NULL;


GType
gtk_cell_view_get_type (void)
{
  static GType cell_view_type = 0;

  if (!cell_view_type)
    {
      static const GTypeInfo cell_view_info =
        {
          sizeof (GtkCellViewClass),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) gtk_cell_view_class_init,
          NULL, /* class_finalize */
          NULL, /* class_data */
          sizeof (GtkCellView),
          0,
          (GInstanceInitFunc) gtk_cell_view_init
        };

      static const GInterfaceInfo cell_layout_info =
       {
         (GInterfaceInitFunc) gtk_cell_view_cell_layout_init,
         NULL,
         NULL
       };

      cell_view_type = g_type_register_static (GTK_TYPE_WIDGET, "PidginCellView",
                                               &cell_view_info, 0);

      g_type_add_interface_static (cell_view_type, GTK_TYPE_CELL_LAYOUT,
                                   &cell_layout_info);
    }

  return cell_view_type;
}

static void
gtk_cell_view_class_init (GtkCellViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->get_property = gtk_cell_view_get_property;
  gobject_class->set_property = gtk_cell_view_set_property;
  gobject_class->finalize = gtk_cell_view_finalize;

  widget_class->expose_event = gtk_cell_view_expose;
  widget_class->size_allocate = gtk_cell_view_size_allocate;
  widget_class->size_request = gtk_cell_view_size_request;
  widget_class->style_set = gtk_cell_view_style_set;

  /* properties */
  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        P_("Background color name"),
                                                        P_("Background color as a string"),
                                                        NULL,
                                                        G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_BACKGROUND_GDK,
                                   g_param_spec_boxed ("background_gdk",
                                                      P_("Background color"),
                                                      P_("Background color as a GdkColor"),
                                                      GDK_TYPE_COLOR,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (gobject_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE))

  ADD_SET_PROP ("background_set", PROP_BACKGROUND_SET,
                P_("Background set"),
                P_("Whether this tag affects the background color"));
}

static void
gtk_cell_view_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_cell_view_cell_layout_pack_start;
  iface->pack_end = gtk_cell_view_cell_layout_pack_end;
  iface->clear = gtk_cell_view_cell_layout_clear;
  iface->add_attribute = gtk_cell_view_cell_layout_add_attribute;
  iface->set_cell_data_func = gtk_cell_view_cell_layout_set_cell_data_func;
  iface->clear_attributes = gtk_cell_view_cell_layout_clear_attributes;
  iface->reorder = gtk_cell_view_cell_layout_reorder;
}

static void
gtk_cell_view_get_property (GObject    *object,
                            guint       param_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkCellView *view = GTK_CELL_VIEW (object);

  switch (param_id)
    {
      case PROP_BACKGROUND_GDK:
        {
          GdkColor color;

          color = view->priv->background;

          g_value_set_boxed (value, &color);
        }
        break;
      case PROP_BACKGROUND_SET:
        g_value_set_boolean (value, view->priv->background_set);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_cell_view_set_property (GObject      *object,
                            guint         param_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkCellView *view = GTK_CELL_VIEW (object);

  switch (param_id)
    {
      case PROP_BACKGROUND:
        {
          GdkColor color;

          if (!g_value_get_string (value))
            gtk_cell_view_set_background_color (view, NULL);
          else if (gdk_color_parse (g_value_get_string (value), &color))
            gtk_cell_view_set_background_color (view, &color);
          else
            g_warning ("Don't know color `%s'", g_value_get_string (value));

          g_object_notify (object, "background_gdk");
        }
        break;
      case PROP_BACKGROUND_GDK:
        gtk_cell_view_set_background_color (view, g_value_get_boxed (value));
        break;
      case PROP_BACKGROUND_SET:
        view->priv->background_set = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_cell_view_init (GtkCellView *cellview)
{
  GTK_WIDGET_SET_FLAGS (cellview, GTK_NO_WINDOW);

  cellview->priv = g_new0(GtkCellViewPrivate,1);
}

static void
gtk_cell_view_style_set (GtkWidget *widget,
                         GtkStyle  *previous_style)
{
  if (previous_style && GTK_WIDGET_REALIZED (widget))
    gdk_window_set_background (widget->window,
                               &widget->style->base[GTK_WIDGET_STATE (widget)]);
}

static void
gtk_cell_view_finalize (GObject *object)
{
  GtkCellView *cellview = GTK_CELL_VIEW (object);

  gtk_cell_view_cell_layout_clear (GTK_CELL_LAYOUT (cellview));

  if (cellview->priv->model)
     g_object_unref (cellview->priv->model);

  if (cellview->priv->displayed_row)
     gtk_tree_row_reference_free (cellview->priv->displayed_row);

  if (G_OBJECT_CLASS (parent_class)->finalize)
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);

  g_free (cellview->priv);
}

static void
gtk_cell_view_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  GList *i;
  gboolean first_cell = TRUE;
  GtkCellView *cellview;

  cellview = GTK_CELL_VIEW (widget);

  requisition->width = 0;
  requisition->height = 0;

  if (cellview->priv->displayed_row)
    gtk_cell_view_set_cell_data (cellview);

  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      gint width, height;
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (!info->cell->visible)
        continue;

      if (!first_cell)
        requisition->width += cellview->priv->spacing;

      gtk_cell_renderer_get_size (info->cell, widget, NULL, NULL, NULL,
                                  &width, &height);

      info->requested_width = width;
      requisition->width += width;
      requisition->height = MAX (requisition->height, height);

      first_cell = FALSE;
    }
}

static void
gtk_cell_view_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GList *i;
  gint expand_cell_count = 0;
  gint full_requested_width = 0;
  gint extra_space;
  GtkCellView *cellview;

  widget->allocation = *allocation;

  cellview = GTK_CELL_VIEW (widget);

  /* checking how much extra space we have */
  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (!info->cell->visible)
        continue;

      if (info->expand)
        expand_cell_count++;

      full_requested_width += info->requested_width;
    }

  extra_space = widget->allocation.width - full_requested_width;
  if (extra_space < 0)
    extra_space = 0;
  else if (extra_space > 0 && expand_cell_count > 0)
    extra_space /= expand_cell_count;

  /* iterate list for PACK_START cells */
  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (info->pack == GTK_PACK_END)
        continue;

      if (!info->cell->visible)
        continue;

      info->real_width = info->requested_width + (info->expand ? extra_space : 0);
    }

  /* iterate list for PACK_END cells */
  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (info->pack == GTK_PACK_START)
        continue;

      if (!info->cell->visible)
        continue;

      info->real_width = info->requested_width + (info->expand ? extra_space : 0);
    }
}

static gboolean
gtk_cell_view_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GList *i;
  GtkCellView *cellview;
  GdkRectangle area;
  GtkCellRendererState state;
  gboolean rtl = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);

  cellview = GTK_CELL_VIEW (widget);

  if (! GTK_WIDGET_DRAWABLE (widget))
    return FALSE;

  /* "blank" background */
  if (cellview->priv->background_set)
    {
      GdkGC *gc;

      gc = gdk_gc_new (GTK_WIDGET (cellview)->window);
      gdk_gc_set_rgb_fg_color (gc, &cellview->priv->background);

      gdk_draw_rectangle (GTK_WIDGET (cellview)->window,
                          gc,
                          TRUE,

                          /*0, 0,*/
                          widget->allocation.x,
                          widget->allocation.y,

                          widget->allocation.width,
                          widget->allocation.height);

      g_object_unref (G_OBJECT (gc));
    }

  /* set cell data (if available) */
  if (cellview->priv->displayed_row)
    gtk_cell_view_set_cell_data (cellview);
  else if (cellview->priv->model)
    return FALSE;

  /* render cells */
  area = widget->allocation;

  /* we draw on our very own window, initialize x and y to zero */
  area.x = widget->allocation.x + (rtl ? widget->allocation.width : 0);
  area.y = widget->allocation.y;

  if (GTK_WIDGET_STATE (widget) == GTK_STATE_PRELIGHT)
    state = GTK_CELL_RENDERER_PRELIT;
  else
    state = 0;

  /* PACK_START */
  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (info->pack == GTK_PACK_END)
        continue;

      if (!info->cell->visible)
        continue;

      area.width = info->real_width;
      if (rtl)
         area.x -= area.width;

      gtk_cell_renderer_render (info->cell,
                                event->window,
                                widget,
                                /* FIXME! */
                                &area, &area, &event->area, state);

      if (!rtl)
         area.x += info->real_width;
    }

   area.x = rtl ? widget->allocation.x : (widget->allocation.x + widget->allocation.width);

  /* PACK_END */
  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (info->pack == GTK_PACK_START)
        continue;

      if (!info->cell->visible)
        continue;

      area.width = info->real_width;
      if (!rtl)
         area.x -= area.width;

      gtk_cell_renderer_render (info->cell,
                                widget->window,
                                widget,
                                /* FIXME ! */
                                &area, &area, &event->area, state);
      if (rtl)
         area.x += info->real_width;
    }

  return FALSE;
}

static GtkCellViewCellInfo *
gtk_cell_view_get_cell_info (GtkCellView     *cellview,
                             GtkCellRenderer *renderer)
{
  GList *i;

  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)i->data;

      if (info->cell == renderer)
        return info;
    }

  return NULL;
}

static void
gtk_cell_view_set_cell_data (GtkCellView *cellview)
{
  GList *i;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_return_if_fail (cellview->priv->displayed_row != NULL);

  path = gtk_tree_row_reference_get_path (cellview->priv->displayed_row);
  gtk_tree_model_get_iter (cellview->priv->model, &iter, path);
  gtk_tree_path_free (path);

  for (i = cellview->priv->cell_list; i; i = i->next)
    {
      GSList *j;
      GtkCellViewCellInfo *info = i->data;

      g_object_freeze_notify (G_OBJECT (info->cell));

      for (j = info->attributes; j && j->next; j = j->next->next)
        {
          gchar *property = j->data;
          gint column = GPOINTER_TO_INT (j->next->data);
          GValue value = {0, };

          gtk_tree_model_get_value (cellview->priv->model, &iter,
                                    column, &value);
          g_object_set_property (G_OBJECT (info->cell),
                                 property, &value);
          g_value_unset (&value);
        }

      if (info->func)
	(* info->func) (GTK_CELL_LAYOUT (cellview),
			info->cell,
			cellview->priv->model,
			&iter,
			info->func_data);

      g_object_thaw_notify (G_OBJECT (info->cell));
    }
}

/* GtkCellLayout implementation */
static void
gtk_cell_view_cell_layout_pack_start (GtkCellLayout   *layout,
                                      GtkCellRenderer *renderer,
                                      gboolean         expand)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (!gtk_cell_view_get_cell_info (cellview, renderer));

  g_object_ref (G_OBJECT (renderer));
  gtk_object_sink (GTK_OBJECT (renderer));

  info = g_new0 (GtkCellViewCellInfo, 1);
  info->cell = renderer;
  info->expand = expand ? TRUE : FALSE;
  info->pack = GTK_PACK_START;

  cellview->priv->cell_list = g_list_append (cellview->priv->cell_list, info);
}

static void
gtk_cell_view_cell_layout_pack_end (GtkCellLayout   *layout,
                                    GtkCellRenderer *renderer,
                                    gboolean         expand)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (!gtk_cell_view_get_cell_info (cellview, renderer));

  g_object_ref (G_OBJECT (renderer));
  gtk_object_sink (GTK_OBJECT (renderer));

  info = g_new0 (GtkCellViewCellInfo, 1);
  info->cell = renderer;
  info->expand = expand ? TRUE : FALSE;
  info->pack = GTK_PACK_END;

  cellview->priv->cell_list = g_list_append (cellview->priv->cell_list, info);
}

static void
gtk_cell_view_cell_layout_add_attribute (GtkCellLayout   *layout,
                                         GtkCellRenderer *renderer,
                                         const gchar     *attribute,
                                         gint             column)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));
  info = gtk_cell_view_get_cell_info (cellview, renderer);
  g_return_if_fail (info != NULL);

  info->attributes = g_slist_prepend (info->attributes,
                                      GINT_TO_POINTER (column));
  info->attributes = g_slist_prepend (info->attributes,
                                      g_strdup (attribute));
}

static void
gtk_cell_view_cell_layout_clear (GtkCellLayout *layout)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));

  while (cellview->priv->cell_list)
    {
      GtkCellViewCellInfo *info = (GtkCellViewCellInfo *)cellview->priv->cell_list->data;

      gtk_cell_view_cell_layout_clear_attributes (layout, info->cell);
      g_object_unref (G_OBJECT (info->cell));
      g_free (info);
      cellview->priv->cell_list = g_list_delete_link (cellview->priv->cell_list,
						      cellview->priv->cell_list);
    }
}

static void
gtk_cell_view_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                              GtkCellRenderer       *cell,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         destroy)
{
  GtkCellView *cellview = GTK_CELL_VIEW (layout);
  GtkCellViewCellInfo *info;

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));

  info = gtk_cell_view_get_cell_info (cellview, cell);
  g_return_if_fail (info != NULL);

  if (info->destroy)
    {
      GDestroyNotify d = info->destroy;

      info->destroy = NULL;
      d (info->func_data);
    }

  info->func = func;
  info->func_data = func_data;
  info->destroy = destroy;
}

static void
gtk_cell_view_cell_layout_clear_attributes (GtkCellLayout   *layout,
                                            GtkCellRenderer *renderer)
{
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);
  GSList *list;

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  info = gtk_cell_view_get_cell_info (cellview, renderer);
  if (info != NULL)
    {
      list = info->attributes;
      while (list && list->next)
	{
	  g_free (list->data);
	  list = list->next->next;
	}

      g_slist_free (info->attributes);
      info->attributes = NULL;
    }
}

static void
gtk_cell_view_cell_layout_reorder (GtkCellLayout   *layout,
                                   GtkCellRenderer *cell,
                                   gint             position)
{
  GList *link;
  GtkCellViewCellInfo *info;
  GtkCellView *cellview = GTK_CELL_VIEW (layout);

  g_return_if_fail (GTK_IS_CELL_VIEW (cellview));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  info = gtk_cell_view_get_cell_info (cellview, cell);

  g_return_if_fail (info != NULL);
  g_return_if_fail (position >= 0);

  link = g_list_find (cellview->priv->cell_list, info);

  g_return_if_fail (link != NULL);

  cellview->priv->cell_list = g_list_remove_link (cellview->priv->cell_list,
                                                  link);
  cellview->priv->cell_list = g_list_insert (cellview->priv->cell_list,
                                             info, position);

  gtk_widget_queue_draw (GTK_WIDGET (cellview));
}

/* public API */
GtkWidget *
gtk_cell_view_new (void)
{
  GtkCellView *cellview;

  cellview = GTK_CELL_VIEW (g_object_new (gtk_cell_view_get_type (), NULL));

  return GTK_WIDGET (cellview);
}

GtkWidget *
gtk_cell_view_new_with_text (const gchar *text)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = {0, };

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_view_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                        renderer, TRUE);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, text);
  gtk_cell_view_set_values (cellview, renderer, "text", &value, NULL);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

GtkWidget *
gtk_cell_view_new_with_markup (const gchar *markup)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = {0, };

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_view_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                        renderer, TRUE);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, markup);
  gtk_cell_view_set_values (cellview, renderer, "markup", &value, NULL);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

GtkWidget *
gtk_cell_view_new_with_pixbuf (GdkPixbuf *pixbuf)
{
  GtkCellView *cellview;
  GtkCellRenderer *renderer;
  GValue value = {0, };

  cellview = GTK_CELL_VIEW (gtk_cell_view_new ());

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_view_cell_layout_pack_start (GTK_CELL_LAYOUT (cellview),
                                        renderer, TRUE);

  g_value_init (&value, GDK_TYPE_PIXBUF);
  g_value_set_object (&value, pixbuf);
  gtk_cell_view_set_values (cellview, renderer, "pixbuf", &value, NULL);
  g_value_unset (&value);

  return GTK_WIDGET (cellview);
}

void
gtk_cell_view_set_value (GtkCellView     *cell_view,
                         GtkCellRenderer *renderer,
                         gchar           *property,
                         GValue          *value)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  g_object_set_property (G_OBJECT (renderer), property, value);

  /* force resize and redraw */
  gtk_widget_queue_resize (GTK_WIDGET (cell_view));
  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

static void
gtk_cell_view_set_valuesv (GtkCellView     *cell_view,
                           GtkCellRenderer *renderer,
                           va_list          args)
{
  gchar *attribute;
  GValue *value;

  attribute = va_arg (args, gchar *);

  while (attribute)
    {
      value = va_arg (args, GValue *);
      gtk_cell_view_set_value (cell_view, renderer, attribute, value);
      attribute = va_arg (args, gchar *);
    }
}

void
gtk_cell_view_set_values (GtkCellView     *cell_view,
                          GtkCellRenderer *renderer,
                          ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (gtk_cell_view_get_cell_info (cell_view, renderer));

  va_start (args, renderer);
  gtk_cell_view_set_valuesv (cell_view, renderer, args);
  va_end (args);
}

void
gtk_cell_view_set_model (GtkCellView  *cell_view,
                         GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  if (cell_view->priv->model)
    {
      if (cell_view->priv->displayed_row)
        gtk_tree_row_reference_free (cell_view->priv->displayed_row);
      cell_view->priv->displayed_row = NULL;

      g_object_unref (G_OBJECT (cell_view->priv->model));
      cell_view->priv->model = NULL;
    }

  cell_view->priv->model = model;

  if (cell_view->priv->model)
    g_object_ref (G_OBJECT (cell_view->priv->model));
}

/**
 * gtk_cell_view_set_displayed_row:
 * @cell_view: a #GtkCellView
 * @path: a #GtkTreePath or %NULL to unset.
 *
 * Sets the row of the model that is currently displayed
 * by the #GtkCellView. If the path is unset, then the
 * contents of the cellview "stick" at their last value;
 * this is not normally a desired result, but may be
 * a needed intermediate state if say, the model for
 * the #GtkCellView becomes temporarily empty.
 **/
void
gtk_cell_view_set_displayed_row (GtkCellView *cell_view,
                                 GtkTreePath *path)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (cell_view));
  g_return_if_fail (GTK_IS_TREE_MODEL (cell_view->priv->model));

  if (cell_view->priv->displayed_row)
    gtk_tree_row_reference_free (cell_view->priv->displayed_row);

  if (path)
    {
      cell_view->priv->displayed_row =
	gtk_tree_row_reference_new (cell_view->priv->model, path);
    }
  else
    cell_view->priv->displayed_row = NULL;

  /* force resize and redraw */
  gtk_widget_queue_resize (GTK_WIDGET (cell_view));
  gtk_widget_queue_draw (GTK_WIDGET (cell_view));
}

GtkTreePath *
gtk_cell_view_get_displayed_row (GtkCellView *cell_view)
{
  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), NULL);

  if (!cell_view->priv->displayed_row)
    return NULL;

  return gtk_tree_row_reference_get_path (cell_view->priv->displayed_row);
}

gboolean
gtk_cell_view_get_size_of_row (GtkCellView    *cell_view,
                               GtkTreePath    *path,
                               GtkRequisition *requisition)
{
  GtkTreeRowReference *tmp;
  GtkRequisition req;

  g_return_val_if_fail (GTK_IS_CELL_VIEW (cell_view), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (requisition != NULL, FALSE);

  tmp = cell_view->priv->displayed_row;
  cell_view->priv->displayed_row =
    gtk_tree_row_reference_new (cell_view->priv->model, path);

  gtk_cell_view_size_request (GTK_WIDGET (cell_view), requisition);

  gtk_tree_row_reference_free (cell_view->priv->displayed_row);
  cell_view->priv->displayed_row = tmp;

  /* restore actual size info */
  gtk_cell_view_size_request (GTK_WIDGET (cell_view), &req);

  return TRUE;
}

void
gtk_cell_view_set_background_color (GtkCellView    *view,
                                    const GdkColor *color)
{
  g_return_if_fail (GTK_IS_CELL_VIEW (view));

  if (color)
    {
      if (!view->priv->background_set)
        {
          view->priv->background_set = TRUE;
          g_object_notify (G_OBJECT (view), "background_set");
        }

      view->priv->background = *color;
    }
  else
    {
      if (view->priv->background_set)
        {
          view->priv->background_set = FALSE;
          g_object_notify (G_OBJECT (view), "background_set");
        }
    }
}
#endif /* Gtk 2.6 */
