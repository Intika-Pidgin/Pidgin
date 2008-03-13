/* gtkcellviewmenuitem.c
 * Copyright (C) 2003  Kristian Rietveld <kris@gtk.org>
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
#include <gtk/gtkversion.h>
#if !GTK_CHECK_VERSION(2,6,0)
#include "gtkcellviewmenuitem.h"
#include "gtkcellview.h"

struct _GtkCellViewMenuItemPrivate
{
  GtkWidget *cell_view;
};

static void gtk_cell_view_menu_item_init       (GtkCellViewMenuItem      *item);
static void gtk_cell_view_menu_item_class_init (GtkCellViewMenuItemClass *klass);
static void gtk_cell_view_menu_item_finalize   (GObject                  *object);


GType
gtk_cell_view_menu_item_get_type (void)
{
  static GType cell_view_menu_item_type = 0;

  if (!cell_view_menu_item_type)
    {
      static const GTypeInfo cell_view_menu_item_info =
        {
          sizeof (GtkCellViewMenuItemClass),
          NULL,
          NULL,
          (GClassInitFunc) gtk_cell_view_menu_item_class_init,
          NULL,
          NULL,
          sizeof (GtkCellViewMenuItem),
          0,
          (GInstanceInitFunc) gtk_cell_view_menu_item_init
        };

      cell_view_menu_item_type =
        g_type_register_static (GTK_TYPE_MENU_ITEM, "PidginCellViewMenuItem",
                                &cell_view_menu_item_info, 0);
    }

  return cell_view_menu_item_type;
}

static void
gtk_cell_view_menu_item_class_init (GtkCellViewMenuItemClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *)klass;
  object_class->finalize = gtk_cell_view_menu_item_finalize;
}

static void
gtk_cell_view_menu_item_init (GtkCellViewMenuItem *item)
{
  item->priv = g_new0(GtkCellViewMenuItemPrivate,1);
}

GtkWidget *
gtk_cell_view_menu_item_new (void)
{
  GtkCellViewMenuItem *item;

  item = g_object_new (GTK_TYPE_CELL_VIEW_MENU_ITEM, NULL);

  item->priv->cell_view = gtk_cell_view_new ();
  gtk_container_add (GTK_CONTAINER (item), item->priv->cell_view);
  gtk_widget_show (item->priv->cell_view);

  return GTK_WIDGET (item);
}

GtkWidget *
gtk_cell_view_menu_item_new_with_pixbuf (GdkPixbuf *pixbuf)
{
  GtkCellViewMenuItem *item;

  item = g_object_new (GTK_TYPE_CELL_VIEW_MENU_ITEM, NULL);

  item->priv->cell_view = gtk_cell_view_new_with_pixbuf (pixbuf);
  gtk_container_add (GTK_CONTAINER (item), item->priv->cell_view);
  gtk_widget_show (item->priv->cell_view);

  return GTK_WIDGET (item);
}

GtkWidget *
gtk_cell_view_menu_item_new_with_text (const gchar *text)
{
  GtkCellViewMenuItem *item;

  item = g_object_new (GTK_TYPE_CELL_VIEW_MENU_ITEM, NULL);

  item->priv->cell_view = gtk_cell_view_new_with_text (text);
  gtk_container_add (GTK_CONTAINER (item), item->priv->cell_view);
  gtk_widget_show (item->priv->cell_view);

  return GTK_WIDGET (item);
}

GtkWidget *
gtk_cell_view_menu_item_new_with_markup (const gchar *markup)
{
  GtkCellViewMenuItem *item;

  item = g_object_new (GTK_TYPE_CELL_VIEW_MENU_ITEM, NULL);

  item->priv->cell_view = gtk_cell_view_new_with_markup (markup);
  gtk_container_add (GTK_CONTAINER (item), item->priv->cell_view);
  gtk_widget_show (item->priv->cell_view);

  return GTK_WIDGET (item);
}

GtkWidget *
gtk_cell_view_menu_item_new_from_model (GtkTreeModel *model,
                                        GtkTreePath  *path)
{
  GtkCellViewMenuItem *item;

  item = g_object_new (GTK_TYPE_CELL_VIEW_MENU_ITEM, NULL);

  item->priv->cell_view = gtk_cell_view_new ();
  gtk_container_add (GTK_CONTAINER (item), item->priv->cell_view);

  gtk_cell_view_set_model (GTK_CELL_VIEW (item->priv->cell_view), model);
  gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (item->priv->cell_view), path);

  gtk_widget_show (item->priv->cell_view);

  return GTK_WIDGET (item);
}

static void
gtk_cell_view_menu_item_finalize (GObject *object)
{
  GtkCellViewMenuItem *item = GTK_CELL_VIEW_MENU_ITEM (object);

  g_free (item->priv);
}
#endif /* Gtk 2.6 */
