/* pidgincombobox.c
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
#include <gtk/gtkversion.h>
#if !GTK_CHECK_VERSION(2,6,0)
#include "pidgincombobox.h"

#if !GTK_CHECK_VERSION(2,4,0)
#include <gtk/gtkarrow.h>
#include <gtk/gtkbindings.h>
#include "gtkcelllayout.h"
#include <gtk/gtkcellrenderertext.h>
#include "gtkcellview.h"
#include "gtkcellviewmenuitem.h"
#include <gtk/gtkeventbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktreeselection.h>
/*
#include <gtk/gtktreeprivate.h>
*/
#include <gtk/gtkvseparator.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkversion.h>

#include <gdk/gdkkeysyms.h>

#include <gobject/gvaluecollector.h>

#include <string.h>
#include <stdarg.h>

#define P_(x) (x)

/* WELCOME, to THE house of evil code */

typedef struct _ComboCellInfo ComboCellInfo;
struct _ComboCellInfo
{
  GtkCellRenderer *cell;
  GSList *attributes;

  GtkCellLayoutDataFunc func;
  gpointer func_data;
  GDestroyNotify destroy;

  guint expand : 1;
  guint pack : 1;
};

struct _GtkComboBoxPrivate
{
  GtkTreeModel *model;

  gint col_column;
  gint row_column;

  gint wrap_width;

  gint active_item;

  GtkWidget *tree_view;
  GtkTreeViewColumn *column;

  GtkWidget *cell_view;
  GtkWidget *cell_view_frame;

  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *arrow;
  GtkWidget *separator;

  GtkWidget *popup_widget;
  GtkWidget *popup_window;
  GtkWidget *popup_frame;

  guint inserted_id;
  guint deleted_id;
  guint reordered_id;
  guint changed_id;

  gint width;
  GSList *cells;

  guint popup_in_progress : 1;
  guint destroying : 1;
};

/* While debugging this evil code, I have learned that
 * there are actually 4 modes to this widget, which can
 * be characterized as follows
 * 
 * 1) menu mode, no child added
 *
 * tree_view -> NULL
 * cell_view -> GtkCellView, regular child
 * cell_view_frame -> NULL
 * button -> GtkToggleButton set_parent to combo
 * box -> child of button
 * arrow -> child of box
 * separator -> child of box
 * popup_widget -> GtkMenu
 * popup_window -> NULL
 * popup_frame -> NULL
 *
 * 2) menu mode, child added
 * 
 * tree_view -> NULL
 * cell_view -> NULL 
 * cell_view_frame -> NULL
 * button -> GtkToggleButton set_parent to combo
 * box -> NULL
 * arrow -> GtkArrow, child of button
 * separator -> NULL
 * popup_widget -> GtkMenu
 * popup_window -> NULL
 * popup_frame -> NULL
 *
 * 3) list mode, no child added
 * 
 * tree_view -> GtkTreeView, child of popup_frame
 * cell_view -> GtkCellView, regular child
 * cell_view_frame -> GtkFrame, set parent to combo
 * button -> GtkToggleButton, set_parent to combo
 * box -> NULL
 * arrow -> GtkArrow, child of button
 * separator -> NULL
 * popup_widget -> tree_view
 * popup_window -> GtkWindow
 * popup_frame -> GtkFrame, child of popup_window
 *
 * 4) list mode, child added
 *
 * tree_view -> GtkTreeView, child of popup_frame
 * cell_view -> NULL
 * cell_view_frame -> NULL
 * button -> GtkToggleButton, set_parent to combo
 * box -> NULL
 * arrow -> GtkArrow, child of button
 * separator -> NULL
 * popup_widget -> tree_view
 * popup_window -> GtkWindow
 * popup_frame -> GtkFrame, child of popup_window
 * 
 */

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_WRAP_WIDTH,
  PROP_ROW_SPAN_COLUMN,
  PROP_COLUMN_SPAN_COLUMN,
  PROP_ACTIVE
};

static GtkBinClass *parent_class = NULL;
static guint combo_box_signals[LAST_SIGNAL] = {0,};

#define BONUS_PADDING 4


/* common */
static void     gtk_combo_box_class_init           (GtkComboBoxClass *klass);
static void     gtk_combo_box_cell_layout_init     (GtkCellLayoutIface *iface);
static void     gtk_combo_box_init                 (GtkComboBox      *combo_box);
static void     gtk_combo_box_finalize             (GObject          *object);
static void     gtk_combo_box_destroy              (GtkObject        *object);

static void     gtk_combo_box_set_property         (GObject         *object,
                                                    guint            prop_id,
                                                    const GValue    *value,
                                                    GParamSpec      *spec);
static void     gtk_combo_box_get_property         (GObject         *object,
                                                    guint            prop_id,
                                                    GValue          *value,
                                                    GParamSpec      *spec);

static void     gtk_combo_box_state_changed        (GtkWidget        *widget,
			                            GtkStateType      previous);
static void     gtk_combo_box_style_set            (GtkWidget       *widget,
                                                    GtkStyle        *previous);
static void     gtk_combo_box_button_toggled       (GtkWidget       *widget,
                                                    gpointer         data);
static void     gtk_combo_box_add                  (GtkContainer    *container,
                                                    GtkWidget       *widget);
static void     gtk_combo_box_remove               (GtkContainer    *container,
                                                    GtkWidget       *widget);

static ComboCellInfo *gtk_combo_box_get_cell_info  (GtkComboBox      *combo_box,
                                                    GtkCellRenderer  *cell);

static void     gtk_combo_box_menu_show            (GtkWidget        *menu,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_hide            (GtkWidget        *menu,
                                                    gpointer          user_data);

static void     gtk_combo_box_set_popup_widget     (GtkComboBox      *combo_box,
                                                    GtkWidget        *popup);
#if GTK_CHECK_VERSION(2,2,0)
static void     gtk_combo_box_menu_position_below  (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_position_over   (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_position        (GtkMenu          *menu,
                                                    gint             *x,
                                                    gint             *y,
                                                    gint             *push_in,
                                                    gpointer          user_data);
#endif

static gint     gtk_combo_box_calc_requested_width (GtkComboBox      *combo_box,
                                                    GtkTreePath      *path);
static void     gtk_combo_box_remeasure            (GtkComboBox      *combo_box);

static void     gtk_combo_box_unset_model          (GtkComboBox      *combo_box);

static void     gtk_combo_box_size_request         (GtkWidget        *widget,
                                                    GtkRequisition   *requisition);
static void     gtk_combo_box_size_allocate        (GtkWidget        *widget,
                                                    GtkAllocation    *allocation);
static void     gtk_combo_box_forall               (GtkContainer     *container,
                                                    gboolean          include_internals,
                                                    GtkCallback       callback,
                                                    gpointer          callback_data);
static gboolean gtk_combo_box_expose_event         (GtkWidget        *widget,
                                                    GdkEventExpose   *event);
static gboolean gtk_combo_box_scroll_event         (GtkWidget        *widget,
                                                    GdkEventScroll   *event);
static void     gtk_combo_box_set_active_internal  (GtkComboBox      *combo_box,
						    gint              index);
static gboolean gtk_combo_box_key_press            (GtkWidget        *widget,
						    GdkEventKey      *event,
						    gpointer          data);

/* listening to the model */
static void     gtk_combo_box_model_row_inserted   (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gpointer          user_data);
static void     gtk_combo_box_model_row_deleted    (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    gpointer          user_data);
static void     gtk_combo_box_model_rows_reordered (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gint             *new_order,
						    gpointer          user_data);
static void     gtk_combo_box_model_row_changed    (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gpointer          data);

/* list */
static void     gtk_combo_box_list_position        (GtkComboBox      *combo_box, 
						    gint             *x, 
						    gint             *y, 
						    gint             *width,
						    gint             *height);

static void     gtk_combo_box_list_setup           (GtkComboBox      *combo_box);
static void     gtk_combo_box_list_destroy         (GtkComboBox      *combo_box);

static void     gtk_combo_box_list_remove_grabs    (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_list_button_released (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_key_press       (GtkWidget        *widget,
                                                    GdkEventKey      *event,
                                                    gpointer          data);
static gboolean gtk_combo_box_list_button_pressed  (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          data);

static void     gtk_combo_box_list_row_changed     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);

/* menu */
static void     gtk_combo_box_menu_setup           (GtkComboBox      *combo_box,
                                                    gboolean          add_children);
static void     gtk_combo_box_menu_fill            (GtkComboBox      *combo_box);
static void     gtk_combo_box_menu_destroy         (GtkComboBox      *combo_box);

static void     gtk_combo_box_item_get_size        (GtkComboBox      *combo_box,
                                                    gint              index,
                                                    gint             *cols,
                                                    gint             *rows);
static void     gtk_combo_box_relayout_item        (GtkComboBox      *combo_box,
                                                    gint              index);
static void     gtk_combo_box_relayout             (GtkComboBox      *combo_box);

static gboolean gtk_combo_box_menu_button_press    (GtkWidget        *widget,
                                                    GdkEventButton   *event,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_item_activate   (GtkWidget        *item,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_row_inserted    (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_row_deleted     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    gpointer          user_data);
static void     gtk_combo_box_menu_rows_reordered  (GtkTreeModel     *model,
						    GtkTreePath      *path,
						    GtkTreeIter      *iter,
						    gint             *new_order,
						    gpointer          user_data);
static void     gtk_combo_box_menu_row_changed     (GtkTreeModel     *model,
                                                    GtkTreePath      *path,
                                                    GtkTreeIter      *iter,
                                                    gpointer          data);
static gboolean gtk_combo_box_menu_key_press       (GtkWidget        *widget,
						    GdkEventKey      *event,
						    gpointer          data);

/* cell layout */
static void     gtk_combo_box_cell_layout_pack_start         (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              gboolean               expand);
static void     gtk_combo_box_cell_layout_pack_end           (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              gboolean               expand);
static void     gtk_combo_box_cell_layout_clear              (GtkCellLayout         *layout);
static void     gtk_combo_box_cell_layout_add_attribute      (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              const gchar           *attribute,
                                                              gint                   column);
static void     gtk_combo_box_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              GtkCellLayoutDataFunc  func,
                                                              gpointer               func_data,
                                                              GDestroyNotify         destroy);
static void     gtk_combo_box_cell_layout_clear_attributes   (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell);
static void     gtk_combo_box_cell_layout_reorder            (GtkCellLayout         *layout,
                                                              GtkCellRenderer       *cell,
                                                              gint                   position);
static gboolean gtk_combo_box_mnemonic_activate              (GtkWidget    *widget,
							      gboolean      group_cycling);

static void     cell_view_sync_cells (GtkComboBox *combo_box,
                                      GtkCellView *cell_view);

#if !GTK_CHECK_VERSION(2,4,0)
static void     gtk_menu_attach (GtkMenu   *menu,
                                 GtkWidget *child,
                                 guint      left_attach,
                                 guint      right_attach,
                                 guint      top_attach,
                                 guint      bottom_attach);
#endif

GType
gtk_combo_box_get_type (void)
{
  static GType combo_box_type = 0;

  if (!combo_box_type)
    {
      static const GTypeInfo combo_box_info =
        {
          sizeof (GtkComboBoxClass),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) gtk_combo_box_class_init,
          NULL, /* class_finalize */
          NULL, /* class_data */
          sizeof (GtkComboBox),
          0,
          (GInstanceInitFunc) gtk_combo_box_init
        };

      static const GInterfaceInfo cell_layout_info =
        {
          (GInterfaceInitFunc) gtk_combo_box_cell_layout_init,
          NULL,
          NULL
        };

      combo_box_type = g_type_register_static (GTK_TYPE_BIN,
                                               "PidginComboBox",
                                               &combo_box_info,
                                               0);

      g_type_add_interface_static (combo_box_type,
                                   GTK_TYPE_CELL_LAYOUT,
                                   &cell_layout_info);
    }

  return combo_box_type;
}

/* common */
static void
gtk_combo_box_class_init (GtkComboBoxClass *klass)
{
  GObjectClass *object_class;
  GtkBindingSet *binding_set;
  GtkObjectClass *gtk_object_class;
  GtkContainerClass *container_class;
  GtkWidgetClass *widget_class;

  binding_set = gtk_binding_set_by_class (klass);

  container_class = (GtkContainerClass *)klass;
  container_class->forall = gtk_combo_box_forall;
  container_class->add = gtk_combo_box_add;
  container_class->remove = gtk_combo_box_remove;

  widget_class = (GtkWidgetClass *)klass;
  widget_class->size_allocate = gtk_combo_box_size_allocate;
  widget_class->size_request = gtk_combo_box_size_request;
  widget_class->expose_event = gtk_combo_box_expose_event;
  widget_class->scroll_event = gtk_combo_box_scroll_event;
  widget_class->mnemonic_activate = gtk_combo_box_mnemonic_activate;
  widget_class->style_set = gtk_combo_box_style_set;
  widget_class->state_changed = gtk_combo_box_state_changed;

  gtk_object_class = (GtkObjectClass *)klass;
  gtk_object_class->destroy = gtk_combo_box_destroy;

  object_class = (GObjectClass *)klass;
  object_class->finalize = gtk_combo_box_finalize;
  object_class->set_property = gtk_combo_box_set_property;
  object_class->get_property = gtk_combo_box_get_property;

  parent_class = g_type_class_peek_parent (klass);

  /* signals */
  combo_box_signals[CHANGED] =
    g_signal_new ("changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkComboBoxClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* properties */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("ComboBox model"),
                                                        P_("The model for the combo box"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_WRAP_WIDTH,
                                   g_param_spec_int ("wrap_width",
                                                     P_("Wrap width"),
                                                     P_("Wrap width for layouting the items in a grid"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ROW_SPAN_COLUMN,
                                   g_param_spec_int ("row_span_column",
                                                     P_("Row span column"),
                                                     P_("TreeModel column containing the row span values"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_COLUMN_SPAN_COLUMN,
                                   g_param_spec_int ("column_span_column",
                                                     P_("Column span column"),

                                                     P_("TreeModel column containing the column span values"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_int ("active",
                                                     P_("Active item"),
                                                     P_("The item which is currently active"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("appears-as-list",
                                                                 P_("Appears as list"),
                                                                 P_("Whether combobox dropdowns should look like lists rather than menus"),
                                                                 FALSE,
                                                                 G_PARAM_READABLE));
}

static void
gtk_combo_box_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_combo_box_cell_layout_pack_start;
  iface->pack_end = gtk_combo_box_cell_layout_pack_end;
  iface->clear = gtk_combo_box_cell_layout_clear;
  iface->add_attribute = gtk_combo_box_cell_layout_add_attribute;
  iface->set_cell_data_func = gtk_combo_box_cell_layout_set_cell_data_func;
  iface->clear_attributes = gtk_combo_box_cell_layout_clear_attributes;
  iface->reorder = gtk_combo_box_cell_layout_reorder;
}

static void
gtk_combo_box_init (GtkComboBox *combo_box)
{
  combo_box->priv = g_new0(GtkComboBoxPrivate,1);

  combo_box->priv->cell_view = gtk_cell_view_new ();
  gtk_widget_set_parent (combo_box->priv->cell_view, GTK_WIDGET (combo_box));
  GTK_BIN (combo_box)->child = combo_box->priv->cell_view;
  gtk_widget_show (combo_box->priv->cell_view);

  combo_box->priv->width = 0;
  combo_box->priv->wrap_width = 0;

  combo_box->priv->active_item = -1;
  combo_box->priv->col_column = -1;
  combo_box->priv->row_column = -1;
}

static void
gtk_combo_box_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        gtk_combo_box_set_model (combo_box, g_value_get_object (value));
        break;

      case PROP_WRAP_WIDTH:
        gtk_combo_box_set_wrap_width (combo_box, g_value_get_int (value));
        break;

      case PROP_ROW_SPAN_COLUMN:
        gtk_combo_box_set_row_span_column (combo_box, g_value_get_int (value));
        break;

      case PROP_COLUMN_SPAN_COLUMN:
        gtk_combo_box_set_column_span_column (combo_box, g_value_get_int (value));
        break;

      case PROP_ACTIVE:
        gtk_combo_box_set_active (combo_box, g_value_get_int (value));
        break;

      default:
        break;
    }
}

static void
gtk_combo_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value, combo_box->priv->model);
        break;

      case PROP_WRAP_WIDTH:
        g_value_set_int (value, combo_box->priv->wrap_width);
        break;

      case PROP_ROW_SPAN_COLUMN:
        g_value_set_int (value, combo_box->priv->row_column);
        break;

      case PROP_COLUMN_SPAN_COLUMN:
        g_value_set_int (value, combo_box->priv->col_column);
        break;

      case PROP_ACTIVE:
        g_value_set_int (value, gtk_combo_box_get_active (combo_box));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_state_changed (GtkWidget    *widget,
			     GtkStateType  previous)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      if (combo_box->priv->tree_view && combo_box->priv->cell_view)
	gtk_cell_view_set_background_color (GTK_CELL_VIEW (combo_box->priv->cell_view), 
					    &widget->style->base[GTK_WIDGET_STATE (widget)]);
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_combo_box_check_appearance (GtkComboBox *combo_box)
{
  gboolean appears_as_list;

  /* if wrap_width > 0, then we are in grid-mode and forced to use
   * unix style
   */
  if (combo_box->priv->wrap_width)
    appears_as_list = FALSE;
  else
    gtk_widget_style_get (GTK_WIDGET (combo_box),
			  "appears-as-list", &appears_as_list,
			  NULL);

  if (appears_as_list)
    {
      /* Destroy all the menu mode widgets, if they exist. */
      if (GTK_IS_MENU (combo_box->priv->popup_widget))
	gtk_combo_box_menu_destroy (combo_box);

      /* Create the list mode widgets, if they don't already exist. */
      if (!GTK_IS_TREE_VIEW (combo_box->priv->tree_view))
	gtk_combo_box_list_setup (combo_box);
    }
  else
    {
      /* Destroy all the list mode widgets, if they exist. */
      if (GTK_IS_TREE_VIEW (combo_box->priv->tree_view))
	gtk_combo_box_list_destroy (combo_box);

      /* Create the menu mode widgets, if they don't already exist. */
      if (!GTK_IS_MENU (combo_box->priv->popup_widget))
	gtk_combo_box_menu_setup (combo_box, TRUE);
    }
}

static void
gtk_combo_box_style_set (GtkWidget *widget,
                         GtkStyle  *previous)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  gtk_combo_box_check_appearance (combo_box);

  if (combo_box->priv->tree_view && combo_box->priv->cell_view)
    gtk_cell_view_set_background_color (GTK_CELL_VIEW (combo_box->priv->cell_view), 
					&widget->style->base[GTK_WIDGET_STATE (widget)]);
}

static void
gtk_combo_box_button_toggled (GtkWidget *widget,
                              gpointer   data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      if (!combo_box->priv->popup_in_progress)
        gtk_combo_box_popup (combo_box);
    }
  else
    gtk_combo_box_popdown (combo_box);
}

static void
gtk_combo_box_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);

  if (combo_box->priv->cell_view && combo_box->priv->cell_view->parent)
    {
      gtk_widget_unparent (combo_box->priv->cell_view);
      GTK_BIN (container)->child = NULL;
      gtk_widget_queue_resize (GTK_WIDGET (container));
    }
  
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
  GTK_BIN (container)->child = widget;

  if (combo_box->priv->cell_view &&
      widget != combo_box->priv->cell_view)
    {
      /* since the cell_view was unparented, it's gone now */
      combo_box->priv->cell_view = NULL;

      if (!combo_box->priv->tree_view && combo_box->priv->separator)
        {
	  gtk_container_remove (GTK_CONTAINER (combo_box->priv->separator->parent),
				combo_box->priv->separator);
	  combo_box->priv->separator = NULL;

          gtk_widget_queue_resize (GTK_WIDGET (container));
        }
      else if (combo_box->priv->cell_view_frame)
        {
          gtk_widget_unparent (combo_box->priv->cell_view_frame);
          combo_box->priv->cell_view_frame = NULL;
        }
    }
}

static void
gtk_combo_box_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);
  gboolean appears_as_list;

  gtk_widget_unparent (widget);
  GTK_BIN (container)->child = NULL;

  if (combo_box->priv->destroying)
    return;

  gtk_widget_queue_resize (GTK_WIDGET (container));

  if (!combo_box->priv->tree_view)
    appears_as_list = FALSE;
  else
    appears_as_list = TRUE;
  
  if (appears_as_list)
    gtk_combo_box_list_destroy (combo_box);
  else if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_combo_box_menu_destroy (combo_box);
      gtk_menu_detach (GTK_MENU (combo_box->priv->popup_widget));
      combo_box->priv->popup_widget = NULL;
    }

  if (!combo_box->priv->cell_view)
    {
      combo_box->priv->cell_view = gtk_cell_view_new ();
      gtk_widget_set_parent (combo_box->priv->cell_view, GTK_WIDGET (container));
      GTK_BIN (container)->child = combo_box->priv->cell_view;
      
      gtk_widget_show (combo_box->priv->cell_view);
      gtk_cell_view_set_model (GTK_CELL_VIEW (combo_box->priv->cell_view),
			       combo_box->priv->model);
      cell_view_sync_cells (combo_box, GTK_CELL_VIEW (combo_box->priv->cell_view));
    }


  if (appears_as_list)
    gtk_combo_box_list_setup (combo_box);
  else
    gtk_combo_box_menu_setup (combo_box, TRUE);

  gtk_combo_box_set_active_internal (combo_box, combo_box->priv->active_item);
}

static ComboCellInfo *
gtk_combo_box_get_cell_info (GtkComboBox     *combo_box,
                             GtkCellRenderer *cell)
{
  GSList *i;

  for (i = combo_box->priv->cells; i; i = i->next)
    {
      ComboCellInfo *info = (ComboCellInfo *)i->data;

      if (info && info->cell == cell)
        return info;
    }

  return NULL;
}

static void
gtk_combo_box_menu_show (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                TRUE);
  combo_box->priv->popup_in_progress = FALSE;
}

static void
gtk_combo_box_menu_hide (GtkWidget *menu,
                         gpointer   user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                FALSE);
}

static void
gtk_combo_box_detacher (GtkWidget *widget,
			GtkMenu	  *menu)
{
  GtkComboBox *combo_box;

  g_return_if_fail (GTK_IS_COMBO_BOX (widget));

  combo_box = GTK_COMBO_BOX (widget);
  g_return_if_fail (combo_box->priv->popup_widget == (GtkWidget*) menu);

  g_signal_handlers_disconnect_by_func (menu,
					gtk_combo_box_menu_show,
					combo_box);
  g_signal_handlers_disconnect_by_func (menu,
					gtk_combo_box_menu_hide,
					combo_box);
  
  combo_box->priv->popup_widget = NULL;
}

static void
gtk_combo_box_set_popup_widget (GtkComboBox *combo_box,
                                GtkWidget   *popup)
{
  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_menu_detach (GTK_MENU (combo_box->priv->popup_widget));
      combo_box->priv->popup_widget = NULL;
    }
  else if (combo_box->priv->popup_widget)
    {
      gtk_container_remove (GTK_CONTAINER (combo_box->priv->popup_frame),
                            combo_box->priv->popup_widget);
      g_object_unref (G_OBJECT (combo_box->priv->popup_widget));
      combo_box->priv->popup_widget = NULL;
    }

  if (GTK_IS_MENU (popup))
    {
      if (combo_box->priv->popup_window)
        {
          gtk_widget_destroy (combo_box->priv->popup_window);
          combo_box->priv->popup_window = NULL;
	  combo_box->priv->popup_frame = NULL;
        }

      combo_box->priv->popup_widget = popup;

      g_signal_connect (popup, "show",
                        G_CALLBACK (gtk_combo_box_menu_show), combo_box);
      g_signal_connect (popup, "hide",
                        G_CALLBACK (gtk_combo_box_menu_hide), combo_box);

      gtk_menu_attach_to_widget (GTK_MENU (popup),
				 GTK_WIDGET (combo_box),
				 gtk_combo_box_detacher);
    }
  else
    {
      if (!combo_box->priv->popup_window)
        {
          combo_box->priv->popup_window = gtk_window_new (GTK_WINDOW_POPUP);
	  gtk_window_set_resizable (GTK_WINDOW (combo_box->priv->popup_window), FALSE);
#if GTK_CHECK_VERSION(2,2,0)
          gtk_window_set_screen (GTK_WINDOW (combo_box->priv->popup_window),
                                 gtk_widget_get_screen (GTK_WIDGET (combo_box)));
#endif

          combo_box->priv->popup_frame = gtk_frame_new (NULL);
          gtk_frame_set_shadow_type (GTK_FRAME (combo_box->priv->popup_frame),
                                     GTK_SHADOW_ETCHED_IN);
          gtk_container_add (GTK_CONTAINER (combo_box->priv->popup_window),
                             combo_box->priv->popup_frame);

          gtk_widget_show (combo_box->priv->popup_frame);
        }

      gtk_container_add (GTK_CONTAINER (combo_box->priv->popup_frame),
                         popup);
      gtk_widget_show (popup);
      g_object_ref (G_OBJECT (popup));
      combo_box->priv->popup_widget = popup;
    }
}

#if GTK_CHECK_VERSION(2,2,0)
static void
gtk_combo_box_menu_position_below (GtkMenu  *menu,
				   gint     *x,
				   gint     *y,
				   gint     *push_in,
				   gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint sx, sy;
  GtkWidget *child;
  GtkRequisition req;
  GdkScreen *screen;
  gint monitor_num;
  GdkRectangle monitor;
  
  /* FIXME: is using the size request here broken? */
   child = GTK_BIN (combo_box)->child;
   
   gdk_window_get_origin (child->window, &sx, &sy);
   
   if (GTK_WIDGET_NO_WINDOW (child))
      {
	sx += child->allocation.x;
	sy += child->allocation.y;
      }

   gtk_widget_size_request (GTK_WIDGET (menu), &req);

   if (gtk_widget_get_direction (GTK_WIDGET (combo_box)) == GTK_TEXT_DIR_LTR)
     *x = sx;
   else
     *x = sx + child->allocation.width - req.width;
   *y = sy;

  screen = gtk_widget_get_screen (GTK_WIDGET (combo_box));
  monitor_num = gdk_screen_get_monitor_at_window (screen, 
						  GTK_WIDGET (combo_box)->window);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  
  if (*x < monitor.x)
    *x = monitor.x;
  else if (*x + req.width > monitor.x + monitor.width)
    *x = monitor.x + monitor.width - req.width;
  
  if (monitor.y + monitor.height - *y - child->allocation.height >= req.height)
    *y += child->allocation.height;
  else if (*y - monitor.y >= req.height)
    *y -= req.height;
  else if (monitor.y + monitor.height - *y - child->allocation.height > *y - monitor.y) 
    *y += child->allocation.height;
  else
    *y -= req.height;

   *push_in = FALSE;
}

static void
gtk_combo_box_menu_position_over (GtkMenu  *menu,
				  gint     *x,
				  gint     *y,
				  gboolean *push_in,
				  gpointer  user_data)
{
  GtkComboBox *combo_box;
  GtkWidget *active;
  GtkWidget *child;
  GtkWidget *widget;
  GtkRequisition requisition;
  GList *children;
  gint screen_width;
  gint menu_xpos;
  gint menu_ypos;
  gint menu_width;

  g_return_if_fail (GTK_IS_COMBO_BOX (user_data));
  
  combo_box = GTK_COMBO_BOX (user_data);
  widget = GTK_WIDGET (combo_box);

  gtk_widget_get_child_requisition (GTK_WIDGET (menu), &requisition);
  menu_width = requisition.width;

  active = gtk_menu_get_active (GTK_MENU (combo_box->priv->popup_widget));
  gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

  menu_xpos += widget->allocation.x;
  menu_ypos += widget->allocation.y + widget->allocation.height / 2 - 2;

  if (active != NULL)
    {
      gtk_widget_get_child_requisition (active, &requisition);
      menu_ypos -= requisition.height / 2;
    }

  children = GTK_MENU_SHELL (combo_box->priv->popup_widget)->children;
  while (children)
    {
      child = children->data;

      if (active == child)
	break;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_get_child_requisition (child, &requisition);
	  menu_ypos -= requisition.height;
	}

      children = children->next;
    }

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    menu_xpos = menu_xpos + widget->allocation.width - menu_width;

  /* Clamp the position on screen */
  screen_width = gdk_screen_get_width (gtk_widget_get_screen (widget));
  
  if (menu_xpos < 0)
    menu_xpos = 0;
  else if ((menu_xpos + menu_width) > screen_width)
    menu_xpos -= ((menu_xpos + menu_width) - screen_width);

  *x = menu_xpos;
  *y = menu_ypos;

  *push_in = TRUE;
}

static void
gtk_combo_box_menu_position (GtkMenu  *menu,
			     gint     *x,
			     gint     *y,
			     gint     *push_in,
			     gpointer  user_data)
{
  GtkComboBox *combo_box;
  GtkWidget *menu_item;

  combo_box = GTK_COMBO_BOX (user_data);

  if (combo_box->priv->wrap_width > 0 || combo_box->priv->cell_view == NULL)	
    gtk_combo_box_menu_position_below (menu, x, y, push_in, user_data);
  else
    {
      menu_item = gtk_menu_get_active (GTK_MENU (combo_box->priv->popup_widget));
      if (menu_item)
	gtk_menu_shell_select_item (GTK_MENU_SHELL (combo_box->priv->popup_widget), 
				    menu_item);

      gtk_combo_box_menu_position_over (menu, x, y, push_in, user_data);
    }

}
#endif /* Gtk 2.2 */

static void
gtk_combo_box_list_position (GtkComboBox *combo_box, 
			     gint        *x, 
			     gint        *y, 
			     gint        *width,
			     gint        *height)
{
  GtkWidget *sample;
  GtkRequisition popup_req;
#if GTK_CHECK_VERSION(2,2,0)
  GdkScreen *screen;
  gint monitor_num;
  GdkRectangle monitor;
#endif
  
  sample = GTK_BIN (combo_box)->child;

  *width = sample->allocation.width;
  gtk_widget_size_request (combo_box->priv->popup_window, &popup_req);
  *height = popup_req.height;

  gdk_window_get_origin (sample->window, x, y);

  if (combo_box->priv->cell_view_frame)
    {
       *x -= GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
	     GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness;
       *width += 2 * (GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness);
    }

  if (GTK_WIDGET_NO_WINDOW (sample))
    {
      *x += sample->allocation.x;
      *y += sample->allocation.y;
    }
  
#if GTK_CHECK_VERSION(2,2,0)
  screen = gtk_widget_get_screen (GTK_WIDGET (combo_box));
  monitor_num = gdk_screen_get_monitor_at_window (screen, 
						  GTK_WIDGET (combo_box)->window);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  
  if (*x < monitor.x)
    *x = monitor.x;
  else if (*x + *width > monitor.x + monitor.width)
    *x = monitor.x + monitor.width - *width;
  
  if (*y + sample->allocation.height + *height <= monitor.y + monitor.height)
    *y += sample->allocation.height;
  else
    *y -= *height;
#endif /* Gtk 2.2 */
} 

/**
 * gtk_combo_box_popup:
 * @combo_box: a #GtkComboBox
 * 
 * Pops up the menu or dropdown list of @combo_box. 
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Since: 2.4
 **/
void
gtk_combo_box_popup (GtkComboBox *combo_box)
{
  gint x, y, width, height;
  
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (GTK_WIDGET_MAPPED (combo_box->priv->popup_widget))
    return;

  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_menu_set_active (GTK_MENU (combo_box->priv->popup_widget),
			   combo_box->priv->active_item);

      if (combo_box->priv->wrap_width == 0)
	{
          GtkRequisition requisition;

	  width = GTK_WIDGET (combo_box)->allocation.width;
          gtk_widget_size_request (combo_box->priv->popup_widget, &requisition);

	  gtk_widget_set_size_request (combo_box->priv->popup_widget,
                                       MAX (width, requisition.width), -1);
	}
      
      gtk_menu_popup (GTK_MENU (combo_box->priv->popup_widget),
		      NULL, NULL,
#if GTK_CHECK_VERSION(2,2,0)
		      gtk_combo_box_menu_position,
#else
		      NULL,
#endif
		      combo_box, 0, 0);
      return;
    }

  gtk_widget_show_all (combo_box->priv->popup_frame);
  gtk_combo_box_list_position (combo_box, &x, &y, &width, &height);

  gtk_widget_set_size_request (combo_box->priv->popup_window, width, -1);  
  gtk_window_move (GTK_WINDOW (combo_box->priv->popup_window), x, y);

  /* popup */
  gtk_widget_show (combo_box->priv->popup_window);

  gtk_widget_grab_focus (combo_box->priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                TRUE);

  if (!GTK_WIDGET_HAS_FOCUS (combo_box->priv->tree_view))
    {
      gdk_keyboard_grab (combo_box->priv->popup_window->window,
                         FALSE, GDK_CURRENT_TIME);
      gtk_widget_grab_focus (combo_box->priv->tree_view);
    }

  gtk_grab_add (combo_box->priv->popup_window);
  gdk_pointer_grab (combo_box->priv->popup_window->window, TRUE,
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK |
                    GDK_POINTER_MOTION_MASK,
                    NULL, NULL, GDK_CURRENT_TIME);

  gtk_grab_add (combo_box->priv->tree_view);
}

/**
 * gtk_combo_box_popdown:
 * @combo_box: a #GtkComboBox
 * 
 * Hides the menu or dropdown list of @combo_box.
 *
 * This function is mostly intended for use by accessibility technologies;
 * applications should have little use for it.
 *
 * Since: 2.4
 **/
void
gtk_combo_box_popdown (GtkComboBox *combo_box)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET (combo_box)))
    return;

  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_menu_popdown (GTK_MENU (combo_box->priv->popup_widget));
      return;
    }

  gtk_combo_box_list_remove_grabs (combo_box);
  gtk_widget_hide_all (combo_box->priv->popup_window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                FALSE);
}

static gint
gtk_combo_box_calc_requested_width (GtkComboBox *combo_box,
                                    GtkTreePath *path)
{
  gint padding;
  GtkRequisition req;

  if (combo_box->priv->cell_view)
    gtk_widget_style_get (combo_box->priv->cell_view,
                          "focus-line-width", &padding,
                          NULL);
  else
    padding = 0;

  /* add some pixels for good measure */
  padding += BONUS_PADDING;

  if (combo_box->priv->cell_view)
    gtk_cell_view_get_size_of_row (GTK_CELL_VIEW (combo_box->priv->cell_view),
                                   path, &req);
  else
    req.width = 0;

  return req.width + padding;
}

static void
gtk_combo_box_remeasure (GtkComboBox *combo_box)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  gint padding = 0;

  if (!combo_box->priv->model ||
      !gtk_tree_model_get_iter_first (combo_box->priv->model, &iter))
    return;

  combo_box->priv->width = 0;

#if GTK_CHECK_VERSION(2,2,0)
  path = gtk_tree_path_new_from_indices (0, -1);
#else
  path = gtk_tree_path_new_first();
#endif

  if (combo_box->priv->cell_view)
    gtk_widget_style_get (combo_box->priv->cell_view,
                          "focus-line-width", &padding,
                          NULL);
  else
    padding = 0;

  /* add some pixels for good measure */
  padding += BONUS_PADDING;

  do
    {
      GtkRequisition req;

      if (combo_box->priv->cell_view)
	gtk_cell_view_get_size_of_row (GTK_CELL_VIEW (combo_box->priv->cell_view), 
                                       path, &req);
      else
        req.width = 0;

      combo_box->priv->width = MAX (combo_box->priv->width,
                                    req.width + padding);

      gtk_tree_path_next (path);
    }
  while (gtk_tree_model_iter_next (combo_box->priv->model, &iter));

  gtk_tree_path_free (path);
}

static void
gtk_combo_box_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  gint width, height;
  GtkRequisition bin_req;

  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  /* common */
  gtk_widget_size_request (GTK_BIN (widget)->child, &bin_req);
  gtk_combo_box_remeasure (combo_box);
  bin_req.width = MAX (bin_req.width, combo_box->priv->width);

  gtk_combo_box_check_appearance (combo_box);
      
  if (!combo_box->priv->tree_view)
    {
      /* menu mode */

      if (combo_box->priv->cell_view)
        {
          GtkRequisition button_req, sep_req, arrow_req;
          gint border_width, xthickness, ythickness;

          gtk_widget_size_request (combo_box->priv->button, &button_req);
          border_width = GTK_CONTAINER (combo_box->priv->button)->border_width;
          xthickness = combo_box->priv->button->style->xthickness;
          ythickness = combo_box->priv->button->style->ythickness;

          bin_req.width = MAX (bin_req.width, combo_box->priv->width);

          gtk_widget_size_request (combo_box->priv->separator, &sep_req);
          gtk_widget_size_request (combo_box->priv->arrow, &arrow_req);

          height = MAX (sep_req.height, arrow_req.height);
          height = MAX (height, bin_req.height);

          width = bin_req.width + sep_req.width + arrow_req.width;

          height += border_width + 1 + ythickness * 2 + 4;
          width += border_width + 1 + xthickness * 2 + 4;

          requisition->width = width;
          requisition->height = height;
        }
      else
        {
          GtkRequisition but_req;

          gtk_widget_size_request (combo_box->priv->button, &but_req);

          requisition->width = bin_req.width + but_req.width;
          requisition->height = MAX (bin_req.height, but_req.height);
        }
    }
  else
    {
      /* list mode */
      GtkRequisition button_req, frame_req;

      /* sample + frame */
      *requisition = bin_req;

      if (combo_box->priv->cell_view_frame)
        {
	  gtk_widget_size_request (combo_box->priv->cell_view_frame, &frame_req);
          requisition->width += 2 *
            (GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
             GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness);
          requisition->height += 2 *
            (GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
             GTK_WIDGET (combo_box->priv->cell_view_frame)->style->ythickness);
        }

      /* the button */
      gtk_widget_size_request (combo_box->priv->button, &button_req);

      requisition->height = MAX (requisition->height, button_req.height);
      requisition->width += button_req.width;
    }
}

static void
gtk_combo_box_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  GtkAllocation child;
  GtkRequisition req;
  gboolean is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  widget->allocation = *allocation;

  gtk_combo_box_check_appearance (combo_box);

  if (!combo_box->priv->tree_view)
    {
      if (combo_box->priv->cell_view)
        {
          gint border_width, xthickness, ythickness;
          gint width;

          /* menu mode */
          gtk_widget_size_allocate (combo_box->priv->button, allocation);

          /* set some things ready */
          border_width = GTK_CONTAINER (combo_box->priv->button)->border_width;
          xthickness = combo_box->priv->button->style->xthickness;
          ythickness = combo_box->priv->button->style->ythickness;

          child.x = allocation->x + border_width + 1 + xthickness + 2;
          child.y = allocation->y + border_width + 1 + ythickness + 2;

          width = MAX(1, allocation->width - (border_width + 1 + xthickness * 2 + 4));

          /* handle the children */
          gtk_widget_size_request (combo_box->priv->arrow, &req);
          child.width = req.width;
          child.height = MAX(1, allocation->height - 2 * (child.y - allocation->y));
          if (!is_rtl)
            child.x += width - req.width;
          gtk_widget_size_allocate (combo_box->priv->arrow, &child);
          if (is_rtl)
            child.x += req.width;
          gtk_widget_size_request (combo_box->priv->separator, &req);
          child.width = req.width;
          if (!is_rtl)
            child.x -= req.width;
          gtk_widget_size_allocate (combo_box->priv->separator, &child);

          if (is_rtl)
            {
              child.x += req.width;
              child.width = MAX(1, allocation->x + allocation->width 
                - (border_width + 1 + xthickness + 2) - child.x);
            }
          else 
            {
              child.width = child.x;
              child.x = allocation->x + border_width + 1 + xthickness + 2;
              child.width = MAX(1, child.width - child.x);
            }

          gtk_widget_size_allocate (GTK_BIN (widget)->child, &child);
        }
      else
        {
          gtk_widget_size_request (combo_box->priv->button, &req);
          if (is_rtl)
            child.x = allocation->x;
          else
            child.x = allocation->x + allocation->width - req.width;
          child.y = allocation->y;
          child.width = req.width;
          child.height = allocation->height;
          gtk_widget_size_allocate (combo_box->priv->button, &child);

          if (is_rtl)
            child.x = allocation->x + req.width;
          else
            child.x = allocation->x;
          child.y = allocation->y;
          child.width = MAX(1, allocation->width - req.width);
          gtk_widget_size_allocate (GTK_BIN (widget)->child, &child);
        }
    }
  else
    {
      /* list mode */

      /* button */
      gtk_widget_size_request (combo_box->priv->button, &req);
      if (is_rtl)
        child.x = allocation->x;
      else
        child.x = allocation->x + allocation->width - req.width;
      child.y = allocation->y;
      child.width = req.width;
      child.height = allocation->height;
      gtk_widget_size_allocate (combo_box->priv->button, &child);

      /* frame */
      if (is_rtl)
        child.x = allocation->x + req.width;
      else
        child.x = allocation->x;
      child.y = allocation->y;
      child.width = MAX (1, allocation->width - req.width);
      child.height = allocation->height;

      if (combo_box->priv->cell_view_frame)
        {
          gtk_widget_size_allocate (combo_box->priv->cell_view_frame, &child);

          /* the sample */
          child.x +=
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness;
          child.y +=
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->ythickness;
          child.width -= 2 * (
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->xthickness);
	  child.width = MAX(1,child.width);
          child.height -= 2 * (
            GTK_CONTAINER (combo_box->priv->cell_view_frame)->border_width +
            GTK_WIDGET (combo_box->priv->cell_view_frame)->style->ythickness);
	  child.height = MAX(1,child.height);
        }

      gtk_widget_size_allocate (GTK_BIN (combo_box)->child, &child);
    }
}

static void
gtk_combo_box_unset_model (GtkComboBox *combo_box)
{
  if (combo_box->priv->model)
    {
      g_signal_handler_disconnect (combo_box->priv->model,
				   combo_box->priv->inserted_id);
      g_signal_handler_disconnect (combo_box->priv->model,
				   combo_box->priv->deleted_id);
      g_signal_handler_disconnect (combo_box->priv->model,
				   combo_box->priv->reordered_id);
      g_signal_handler_disconnect (combo_box->priv->model,
				   combo_box->priv->changed_id);
    }

  /* menu mode */
  if (!combo_box->priv->tree_view)
    {
      if (combo_box->priv->popup_widget)
        gtk_container_foreach (GTK_CONTAINER (combo_box->priv->popup_widget),
                               (GtkCallback)gtk_widget_destroy, NULL);
    }

  if (combo_box->priv->model)
    {
      g_object_unref (G_OBJECT (combo_box->priv->model));
      combo_box->priv->model = NULL;
    }

  if (combo_box->priv->cell_view)
    gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (combo_box->priv->cell_view), NULL);
}

static void
gtk_combo_box_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (container);

  if (include_internals)
    {
      if (combo_box->priv->button)
	(* callback) (combo_box->priv->button, callback_data);
      if (combo_box->priv->cell_view_frame)
	(* callback) (combo_box->priv->cell_view_frame, callback_data);
    }

  if (GTK_BIN (container)->child)
    (* callback) (GTK_BIN (container)->child, callback_data);
}

static gboolean
gtk_combo_box_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  if (!combo_box->priv->tree_view)
    {
      gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                      combo_box->priv->button, event);
    }
  else
    {
      gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                      combo_box->priv->button, event);

      if (combo_box->priv->cell_view_frame)
        gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                        combo_box->priv->cell_view_frame, event);
    }

  gtk_container_propagate_expose (GTK_CONTAINER (widget),
                                  GTK_BIN (widget)->child, event);

  return FALSE;
}

static gboolean
gtk_combo_box_scroll_event (GtkWidget          *widget,
                            GdkEventScroll     *event)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);
  gint index;
  gint items;
    
  index = gtk_combo_box_get_active (combo_box);

  if (index != -1)
    {
      items = gtk_tree_model_iter_n_children (combo_box->priv->model, NULL);
      
      if (event->direction == GDK_SCROLL_UP)
        index--;
      else 
        index++;

      gtk_combo_box_set_active (combo_box, CLAMP (index, 0, items - 1));
    }

  return TRUE;
}

/*
 * menu style
 */

static void
cell_view_sync_cells (GtkComboBox *combo_box,
                      GtkCellView *cell_view)
{
  GSList *k;

  for (k = combo_box->priv->cells; k; k = k->next)
    {
      GSList *j;
      ComboCellInfo *info = (ComboCellInfo *)k->data;

      if (info->pack == GTK_PACK_START)
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cell_view),
                                    info->cell, info->expand);
      else if (info->pack == GTK_PACK_END)
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (cell_view),
                                  info->cell, info->expand);

      gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (cell_view),
                                          info->cell,
                                          info->func, info->func_data, NULL);

      for (j = info->attributes; j; j = j->next->next)
        {
          gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (cell_view),
                                         info->cell,
                                         j->data,
                                         GPOINTER_TO_INT (j->next->data));
        }
    }
}

static void
gtk_combo_box_menu_setup (GtkComboBox *combo_box,
                          gboolean     add_children)
{
  GtkWidget *menu;

  if (combo_box->priv->cell_view)
    {
      combo_box->priv->button = gtk_toggle_button_new ();
      g_signal_connect (combo_box->priv->button, "toggled",
                        G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
      g_signal_connect_after (combo_box->priv->button, "key_press_event",
			      G_CALLBACK (gtk_combo_box_key_press), combo_box);
      gtk_widget_set_parent (combo_box->priv->button,
                             GTK_BIN (combo_box)->child->parent);

      combo_box->priv->box = gtk_hbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (combo_box->priv->button), 
			 combo_box->priv->box);

      combo_box->priv->separator = gtk_vseparator_new ();
      gtk_container_add (GTK_CONTAINER (combo_box->priv->box), 
			 combo_box->priv->separator);

      combo_box->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (combo_box->priv->box), 
			 combo_box->priv->arrow);

      gtk_widget_show_all (combo_box->priv->button);
    }
  else
    {
      combo_box->priv->button = gtk_toggle_button_new ();
      g_signal_connect (combo_box->priv->button, "toggled",
                        G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
      g_signal_connect_after (combo_box, "key_press_event",
			      G_CALLBACK (gtk_combo_box_key_press), combo_box);
      gtk_widget_set_parent (combo_box->priv->button,
                             GTK_BIN (combo_box)->child->parent);

      combo_box->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_container_add (GTK_CONTAINER (combo_box->priv->button),
                         combo_box->priv->arrow);
      gtk_widget_show_all (combo_box->priv->button);
    }

  g_signal_connect (combo_box->priv->button, "button_press_event",
                    G_CALLBACK (gtk_combo_box_menu_button_press),
                    combo_box);

  /* create our funky menu */
  menu = gtk_menu_new ();
  g_signal_connect (menu, "key_press_event",
		    G_CALLBACK (gtk_combo_box_menu_key_press), combo_box);
  gtk_combo_box_set_popup_widget (combo_box, menu);

  /* add items */
  if (add_children)
    gtk_combo_box_menu_fill (combo_box);

}

static void
gtk_combo_box_menu_fill (GtkComboBox *combo_box)
{
  gint i, items;
  GtkWidget *menu;
  GtkWidget *tmp;

  if (!combo_box->priv->model)
    return;

  items = gtk_tree_model_iter_n_children (combo_box->priv->model, NULL);
  menu = combo_box->priv->popup_widget;

  for (i = 0; i < items; i++)
    {
      GtkTreePath *path;
#if GTK_CHECK_VERSION(2,2,0)
      path = gtk_tree_path_new_from_indices (i, -1);
#else
      char buf[32];
      g_snprintf(buf, sizeof(buf), "%d", i);
      path = gtk_tree_path_new_from_string(buf);
#endif
      tmp = gtk_cell_view_menu_item_new_from_model (combo_box->priv->model,
                                                    path);
      g_signal_connect (tmp, "activate",
                        G_CALLBACK (gtk_combo_box_menu_item_activate),
                        combo_box);

      cell_view_sync_cells (combo_box,
                            GTK_CELL_VIEW (GTK_BIN (tmp)->child));

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), tmp);

      if (combo_box->priv->wrap_width)
        gtk_combo_box_relayout_item (combo_box, i);

      gtk_widget_show (tmp);

      gtk_tree_path_free (path);
    }
}

static void
gtk_combo_box_menu_destroy (GtkComboBox *combo_box)
{
  g_signal_handlers_disconnect_matched (combo_box->priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_menu_button_press, NULL);

  /* unparent will remove our latest ref */
  gtk_widget_unparent (combo_box->priv->button);
  
  combo_box->priv->box = NULL;
  combo_box->priv->button = NULL;
  combo_box->priv->arrow = NULL;
  combo_box->priv->separator = NULL;

  /* changing the popup window will unref the menu and the children */
}

/*
 * grid
 */

static void
gtk_combo_box_item_get_size (GtkComboBox *combo_box,
                             gint         index_,
                             gint        *cols,
                             gint        *rows)
{
  GtkTreeIter iter;

  gtk_tree_model_iter_nth_child (combo_box->priv->model, &iter, NULL, index_);

  if (cols)
    {
      if (combo_box->priv->col_column == -1)
        *cols = 1;
      else
        gtk_tree_model_get (combo_box->priv->model, &iter,
                            combo_box->priv->col_column, cols,
                            -1);
    }

  if (rows)
    {
      if (combo_box->priv->row_column == -1)
        *rows = 1;
      else
        gtk_tree_model_get (combo_box->priv->model, &iter,
                            combo_box->priv->row_column, rows,
                            -1);
    }
}

static gboolean
menu_occupied (GtkMenu *menu,
               guint    left_attach,
               guint    right_attach,
               guint    top_attach,
               guint    bottom_attach)
{
  GList *i;

  g_return_val_if_fail (GTK_IS_MENU (menu), TRUE);
  g_return_val_if_fail (left_attach < right_attach, TRUE);
  g_return_val_if_fail (top_attach < bottom_attach, TRUE);

  for (i = GTK_MENU_SHELL (menu)->children; i; i = i->next)
    {
      guint l, r, b, t;
      gboolean h_intersect = FALSE;
      gboolean v_intersect = FALSE;

      gtk_container_child_get (GTK_CONTAINER (menu), i->data,
                               "left_attach", &l,
                               "right_attach", &r,
                               "bottom_attach", &b,
                               "top_attach", &t,
                               NULL);

      /* look if this item intersects with the given coordinates */
      h_intersect  = left_attach <= l && l <= right_attach;
      h_intersect &= left_attach <= r && r <= right_attach;

      v_intersect  = top_attach <= t && t <= bottom_attach;
      v_intersect &= top_attach <= b && b <= bottom_attach;

      if (h_intersect && v_intersect)
        return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_relayout_item (GtkComboBox *combo_box,
                             gint         index)
{
  gint current_col = 0, current_row = 0;
  gint rows, cols;
  GList *list, *nth;
  GtkWidget *item, *last;
  GtkWidget *menu;

  menu = combo_box->priv->popup_widget;
  if (!GTK_IS_MENU_SHELL (menu))
    return;

  list = gtk_container_get_children (GTK_CONTAINER (menu));
  nth = g_list_nth (list, index);
  item = nth->data;
  if (nth->prev)
    last = nth->prev->data;
  else 
    last = NULL;
  g_list_free (list);

  gtk_combo_box_item_get_size (combo_box, index, &cols, &rows);
      
   if (combo_box->priv->col_column == -1 &&
      combo_box->priv->row_column == -1 &&
      last)
    {
      gtk_container_child_get (GTK_CONTAINER (menu), 
			       last,
			       "right_attach", &current_col,
			       "top_attach", &current_row,
			       NULL);
      if (current_col + cols > combo_box->priv->wrap_width)
	{
	  current_col = 0;
	  current_row++;
	}
    }
  else
    {
      /* look for a good spot */
      while (1)
	{
	  if (current_col + cols > combo_box->priv->wrap_width)
	    {
	      current_col = 0;
	      current_row++;
	    }
	  
	  if (!menu_occupied (GTK_MENU (menu),
			      current_col, current_col + cols,
			      current_row, current_row + rows))
	    break;
	  
	  current_col++;
	}
    }

  /* set attach props */
  gtk_menu_attach (GTK_MENU (menu), item,
                   current_col, current_col + cols,
                   current_row, current_row + rows);
}

static void
gtk_combo_box_relayout (GtkComboBox *combo_box)
{
  GList *list, *j;
  GtkWidget *menu;

  menu = combo_box->priv->popup_widget;
  
  /* do nothing unless we are in menu style and realized */
  if (combo_box->priv->tree_view || !GTK_IS_MENU_SHELL (menu))
    return;
  
  /* get rid of all children */
  list = gtk_container_get_children (GTK_CONTAINER (menu));
  
  for (j = g_list_last (list); j; j = j->prev)
    gtk_container_remove (GTK_CONTAINER (menu), j->data);
  
  g_list_free (list);
      
  /* and relayout */
  gtk_combo_box_menu_fill (combo_box);
}

/* callbacks */
static gboolean
gtk_combo_box_menu_button_press (GtkWidget      *widget,
                                 GdkEventButton *event,
                                 gpointer        user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (! GTK_IS_MENU (combo_box->priv->popup_widget))
    return FALSE;

  if (event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      combo_box->priv->popup_in_progress = TRUE;
      
      gtk_menu_set_active (GTK_MENU (combo_box->priv->popup_widget),
			   combo_box->priv->active_item);

      if (combo_box->priv->wrap_width == 0)
	{
          GtkRequisition requisition;
          gint width;

	  width = GTK_WIDGET (combo_box)->allocation.width;
          gtk_widget_size_request (combo_box->priv->popup_widget, &requisition);

	  gtk_widget_set_size_request (combo_box->priv->popup_widget,
                                       MAX (width, requisition.width), -1);
	}

      gtk_menu_popup (GTK_MENU (combo_box->priv->popup_widget),
                      NULL, NULL,
#if GTK_CHECK_VERSION(2,2,0)
                      gtk_combo_box_menu_position,
#else
		      NULL,
#endif
		      combo_box, event->button, event->time);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_box_menu_item_activate (GtkWidget *item,
                                  gpointer   user_data)
{
  gint index;
  GtkWidget *menu;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  menu = combo_box->priv->popup_widget;
  g_return_if_fail (GTK_IS_MENU (menu));

  index = g_list_index (GTK_MENU_SHELL (menu)->children, item);

  gtk_combo_box_set_active (combo_box, index);
}

static void
gtk_combo_box_model_row_inserted (GtkTreeModel     *model,
				  GtkTreePath      *path,
				  GtkTreeIter      *iter,
				  gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint index = gtk_tree_path_get_indices (path)[0];

  if (combo_box->priv->active_item >= index)
    combo_box->priv->active_item++;

  if (!combo_box->priv->tree_view)
    gtk_combo_box_menu_row_inserted (model, path, iter, user_data);
}

static void
gtk_combo_box_model_row_deleted (GtkTreeModel     *model,
				 GtkTreePath      *path,
				 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint index = gtk_tree_path_get_indices (path)[0];

  if (!combo_box->priv->tree_view)
    gtk_combo_box_menu_row_deleted (model, path, user_data);
  
  if (index == combo_box->priv->active_item)
    {
      gint items = gtk_tree_model_iter_n_children (model, NULL);

      if (items == 0)
	gtk_combo_box_set_active_internal (combo_box, -1);
      else if (index == items)
	gtk_combo_box_set_active_internal (combo_box, index - 1);
      else
	gtk_combo_box_set_active_internal (combo_box, index);
    }
  else if (combo_box->priv->active_item > index)
    combo_box->priv->active_item--;
}

static void
gtk_combo_box_model_rows_reordered (GtkTreeModel    *model,
				    GtkTreePath     *path,
				    GtkTreeIter     *iter,
				    gint            *new_order,
				    gpointer         user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint items = gtk_tree_model_iter_n_children (model, NULL);
  gint i;

  for (i = 0; i < items; i++)
    if (new_order[i] == combo_box->priv->active_item)
      {
	combo_box->priv->active_item = i;
	break;
      }

  if (!combo_box->priv->tree_view)
    gtk_combo_box_menu_rows_reordered (model, path, iter, new_order, user_data);
}
						    
static void
gtk_combo_box_model_row_changed (GtkTreeModel     *model,
				 GtkTreePath      *path,
				 GtkTreeIter      *iter,
				 gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint index = gtk_tree_path_get_indices (path)[0];

  if (index == combo_box->priv->active_item &&
      combo_box->priv->cell_view)
    gtk_widget_queue_resize (GTK_WIDGET (combo_box->priv->cell_view));
  
  if (combo_box->priv->tree_view)
    gtk_combo_box_list_row_changed (model, path, iter, user_data);
  else
    gtk_combo_box_menu_row_changed (model, path, iter, user_data);
}


static void
gtk_combo_box_menu_row_inserted (GtkTreeModel *model,
                                 GtkTreePath  *path,
                                 GtkTreeIter  *iter,
                                 gpointer      user_data)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (!combo_box->priv->popup_widget)
    return;

  menu = combo_box->priv->popup_widget;
  g_return_if_fail (GTK_IS_MENU (menu));

  item = gtk_cell_view_menu_item_new_from_model (model, path);
  g_signal_connect (item, "activate",
                    G_CALLBACK (gtk_combo_box_menu_item_activate),
                    combo_box);

  cell_view_sync_cells (combo_box, GTK_CELL_VIEW (GTK_BIN (item)->child));

  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item,
                         gtk_tree_path_get_indices (path)[0]);
  gtk_widget_show (item);
}

static void
gtk_combo_box_menu_row_deleted (GtkTreeModel *model,
                                GtkTreePath  *path,
                                gpointer      user_data)
{
  gint index;
  GtkWidget *menu;
  GtkWidget *item;
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  if (!combo_box->priv->popup_widget)
    return;

  index = gtk_tree_path_get_indices (path)[0];

  menu = combo_box->priv->popup_widget;
  g_return_if_fail (GTK_IS_MENU (menu));

  item = g_list_nth_data (GTK_MENU_SHELL (menu)->children, index);
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  gtk_container_remove (GTK_CONTAINER (menu), item);
}

static void
gtk_combo_box_menu_rows_reordered  (GtkTreeModel     *model,
				    GtkTreePath      *path,
				    GtkTreeIter      *iter,
				    gint             *new_order,
				    gpointer          user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  gtk_combo_box_relayout (combo_box);
}
				    
static void
gtk_combo_box_menu_row_changed (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);
  gint width;

  if (!combo_box->priv->popup_widget)
    return;

  if (combo_box->priv->wrap_width)
    gtk_combo_box_relayout_item (combo_box,
                                 gtk_tree_path_get_indices (path)[0]);

  width = gtk_combo_box_calc_requested_width (combo_box, path);

  if (width > combo_box->priv->width)
    {
      if (combo_box->priv->cell_view)
	{
	  gtk_widget_set_size_request (combo_box->priv->cell_view, width, -1);
	  gtk_widget_queue_resize (combo_box->priv->cell_view);
	}
      combo_box->priv->width = width;
    }
}

/*
 * list style
 */

static void
gtk_combo_box_list_setup (GtkComboBox *combo_box)
{
  GSList *i;
  GtkTreeSelection *sel;

  combo_box->priv->button = gtk_toggle_button_new ();
  gtk_widget_set_parent (combo_box->priv->button,
                         GTK_BIN (combo_box)->child->parent);
  g_signal_connect (combo_box->priv->button, "button_press_event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed), combo_box);
  g_signal_connect (combo_box->priv->button, "toggled",
                    G_CALLBACK (gtk_combo_box_button_toggled), combo_box);
  g_signal_connect_after (combo_box, "key_press_event",
			  G_CALLBACK (gtk_combo_box_key_press), combo_box);

  combo_box->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (combo_box->priv->button),
                     combo_box->priv->arrow);
  combo_box->priv->separator = NULL;
  gtk_widget_show_all (combo_box->priv->button);

  if (combo_box->priv->cell_view)
    {
      combo_box->priv->cell_view_frame = gtk_frame_new (NULL);
      gtk_widget_set_parent (combo_box->priv->cell_view_frame,
                             GTK_BIN (combo_box)->child->parent);
      gtk_frame_set_shadow_type (GTK_FRAME (combo_box->priv->cell_view_frame),
                                 GTK_SHADOW_IN);

      gtk_cell_view_set_background_color (GTK_CELL_VIEW (combo_box->priv->cell_view), 
					  &GTK_WIDGET (combo_box)->style->base[GTK_WIDGET_STATE (combo_box)]);

      combo_box->priv->box = gtk_event_box_new ();
      /*
      gtk_event_box_set_visible_window (GTK_EVENT_BOX (combo_box->priv->box), 
					FALSE);
      */

      gtk_container_add (GTK_CONTAINER (combo_box->priv->cell_view_frame),
			 combo_box->priv->box);

      gtk_widget_show_all (combo_box->priv->cell_view_frame);

      g_signal_connect (combo_box->priv->box, "button_press_event",
			G_CALLBACK (gtk_combo_box_list_button_pressed), 
			combo_box);
    }

  combo_box->priv->tree_view = gtk_tree_view_new ();
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (combo_box->priv->tree_view),
                                     FALSE);
  /*
  _gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (combo_box->priv->tree_view),
				      TRUE);
  */
  if (combo_box->priv->model)
    gtk_tree_view_set_model (GTK_TREE_VIEW (combo_box->priv->tree_view),
			     combo_box->priv->model);
    
  g_signal_connect (combo_box->priv->tree_view, "button_press_event",
                    G_CALLBACK (gtk_combo_box_list_button_pressed),
                    combo_box);
  g_signal_connect (combo_box->priv->tree_view, "button_release_event",
                    G_CALLBACK (gtk_combo_box_list_button_released),
                    combo_box);
  g_signal_connect (combo_box->priv->tree_view, "key_press_event",
                    G_CALLBACK (gtk_combo_box_list_key_press),
                    combo_box);

  combo_box->priv->column = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (combo_box->priv->tree_view),
                               combo_box->priv->column);

  /* sync up */
  for (i = combo_box->priv->cells; i; i = i->next)
    {
      GSList *j;
      ComboCellInfo *info = (ComboCellInfo *)i->data;

      if (info->pack == GTK_PACK_START)
        gtk_tree_view_column_pack_start (combo_box->priv->column,
                                         info->cell, info->expand);
      else if (info->pack == GTK_PACK_END)
        gtk_tree_view_column_pack_end (combo_box->priv->column,
                                       info->cell, info->expand);

      for (j = info->attributes; j; j = j->next->next)
        {
          gtk_tree_view_column_add_attribute (combo_box->priv->column,
                                              info->cell,
                                              j->data,
                                              GPOINTER_TO_INT (j->next->data));
        }
    }

  if (combo_box->priv->active_item != -1)
    {
      GtkTreePath *path;

#if GTK_CHECK_VERSION(2,2,0)
      path = gtk_tree_path_new_from_indices (combo_box->priv->active_item, -1);
#else
      char buf[32];
      g_snprintf(buf, sizeof(buf), "%d", combo_box->priv->active_item);
      path = gtk_tree_path_new_from_string(buf);
#endif
      if (path)
        {
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (combo_box->priv->tree_view),
                                    path, NULL, FALSE);
          gtk_tree_path_free (path);
        }
    }

  /* set sample/popup widgets */
  gtk_combo_box_set_popup_widget (combo_box, combo_box->priv->tree_view);

  gtk_widget_show (combo_box->priv->tree_view);
}

static void
gtk_combo_box_list_destroy (GtkComboBox *combo_box)
{
  /* disconnect signals */
  g_signal_handlers_disconnect_matched (combo_box->priv->tree_view,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL, NULL, combo_box);
  g_signal_handlers_disconnect_matched (combo_box->priv->button,
                                        G_SIGNAL_MATCH_DATA,
                                        0, 0, NULL,
                                        gtk_combo_box_list_button_pressed,
                                        NULL);
  if (combo_box->priv->box)
    g_signal_handlers_disconnect_matched (combo_box->priv->box,
					  G_SIGNAL_MATCH_DATA,
					  0, 0, NULL,
					  gtk_combo_box_list_button_pressed,
					  NULL);

  /* destroy things (unparent will kill the latest ref from us)
   * last unref on button will destroy the arrow
   */
  gtk_widget_unparent (combo_box->priv->button);
  combo_box->priv->button = NULL;
  combo_box->priv->arrow = NULL;

  if (combo_box->priv->cell_view)
    {
      g_object_set (G_OBJECT (combo_box->priv->cell_view),
                    "background_set", FALSE,
                    NULL);
    }

  if (combo_box->priv->cell_view_frame)
    {
      gtk_widget_unparent (combo_box->priv->cell_view_frame);
      combo_box->priv->cell_view_frame = NULL;
      combo_box->priv->box = NULL;
    }

  gtk_widget_destroy (combo_box->priv->tree_view);

  combo_box->priv->tree_view = NULL;
  combo_box->priv->popup_widget = NULL;
}

/* callbacks */
static void
gtk_combo_box_list_remove_grabs (GtkComboBox *combo_box)
{
  if (combo_box->priv->tree_view &&
      GTK_WIDGET_HAS_GRAB (combo_box->priv->tree_view))
    {
      gtk_grab_remove (combo_box->priv->tree_view);
    }

  if (combo_box->priv->popup_window &&
      GTK_WIDGET_HAS_GRAB (combo_box->priv->popup_window))
    {
      gtk_grab_remove (combo_box->priv->popup_window);
      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
      gdk_pointer_ungrab (GDK_CURRENT_TIME);
    }
}

static gboolean
gtk_combo_box_list_button_pressed (GtkWidget      *widget,
                                   GdkEventButton *event,
                                   gpointer        data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (ewidget == combo_box->priv->tree_view)
    return TRUE;

  if ((ewidget != combo_box->priv->button && ewidget != combo_box->priv->box) ||
      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (combo_box->priv->button)))
    return FALSE;

  gtk_combo_box_popup (combo_box);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo_box->priv->button),
                                TRUE);

  combo_box->priv->popup_in_progress = TRUE;

  return TRUE;
}

static gboolean
gtk_combo_box_list_button_released (GtkWidget      *widget,
                                    GdkEventButton *event,
                                    gpointer        data)
{
  gboolean ret;
  GtkTreePath *path = NULL;

  GtkComboBox *combo_box = GTK_COMBO_BOX (data);

  gboolean popup_in_progress = FALSE;

  GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

  if (combo_box->priv->popup_in_progress)
    {
      popup_in_progress = TRUE;
      combo_box->priv->popup_in_progress = FALSE;
    }

  if (ewidget != combo_box->priv->tree_view)
    {
      if (ewidget == combo_box->priv->button &&
          !popup_in_progress &&
          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (combo_box->priv->button)))
        {
          gtk_combo_box_popdown (combo_box);
          return TRUE;
        }

      /* released outside treeview */
      if (ewidget != combo_box->priv->button)
        {
          gtk_combo_box_popdown (combo_box);

          return TRUE;
        }

      return FALSE;
    }

  /* select something cool */
  ret = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                       event->x, event->y,
                                       &path,
                                       NULL, NULL, NULL);

  if (!ret)
    return TRUE; /* clicked outside window? */

  gtk_combo_box_set_active (combo_box, gtk_tree_path_get_indices (path)[0]);
  gtk_combo_box_popdown (combo_box);

  gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
gtk_combo_box_key_press (GtkWidget   *widget,
			 GdkEventKey *event,
			 gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();
  gint items = 0;
  gint index = gtk_combo_box_get_active (combo_box);
  gint new_index;

  if (combo_box->priv->model)
    items = gtk_tree_model_iter_n_children (combo_box->priv->model, NULL);

  if ((event->keyval == GDK_Down || event->keyval == GDK_KP_Down) && 
      state == GDK_MOD1_MASK)
    {
      gtk_combo_box_popup (combo_box);

      return TRUE;
    }

  switch (event->keyval) 
    {
    case GDK_Down:
    case GDK_KP_Down:
      new_index = index + 1;
      break;
    case GDK_Up:
    case GDK_KP_Up:
      new_index = index - 1;
      break;
    case GDK_Page_Up:
    case GDK_KP_Page_Up:
    case GDK_Home: 
    case GDK_KP_Home:
      new_index = 0;
      break;
    case GDK_Page_Down:
    case GDK_KP_Page_Down:
    case GDK_End: 
    case GDK_KP_End:
      new_index = items - 1;
      break;
    default:
      return FALSE;
    }
      
  if (items > 0)
    gtk_combo_box_set_active (combo_box, CLAMP (new_index, 0, items - 1));

  return TRUE;
}

static gboolean
gtk_combo_box_menu_key_press (GtkWidget   *widget,
			      GdkEventKey *event,
			      gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();

  if ((event->keyval == GDK_Up || event->keyval == GDK_KP_Up) && 
      state == GDK_MOD1_MASK)
    {
      gtk_combo_box_popdown (combo_box);

      return TRUE;
    }
  
  return FALSE;
}

static gboolean
gtk_combo_box_list_key_press (GtkWidget   *widget,
                              GdkEventKey *event,
                              gpointer     data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();

  if (event->keyval == GDK_Escape ||
      ((event->keyval == GDK_Up || event->keyval == GDK_KP_Up) && 
       state == GDK_MOD1_MASK))
    {
      /* reset active item -- this is incredibly lame and ugly */
      gtk_combo_box_set_active (combo_box,
				gtk_combo_box_get_active (combo_box));
      
      gtk_combo_box_popdown (combo_box);
      
      return TRUE;
    }
    
  if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter ||
      event->keyval == GDK_space || event->keyval == GDK_KP_Space) 
  {
    gboolean ret = FALSE;
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;
    
    if (combo_box->priv->model)
      {
	GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view));
    
	ret = gtk_tree_selection_get_selected (sel, &model, &iter);
      }
    if (ret)
      {
	GtkTreePath *path;
	
	path = gtk_tree_model_get_path (model, &iter);
	if (path)
	  {
	    gtk_combo_box_set_active (combo_box, gtk_tree_path_get_indices (path)[0]);
	    gtk_tree_path_free (path);
	  }
      }

    gtk_combo_box_popdown (combo_box);
    
    return TRUE;
  }

  return FALSE;
}

static void
gtk_combo_box_list_row_changed (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (data);
  gint width;

  width = gtk_combo_box_calc_requested_width (combo_box, path);

  if (width > combo_box->priv->width)
    {
      if (combo_box->priv->cell_view) 
	{
	  gtk_widget_set_size_request (combo_box->priv->cell_view, width, -1);
	  gtk_widget_queue_resize (combo_box->priv->cell_view);
	}
      combo_box->priv->width = width;
    }
}

/*
 * GtkCellLayout implementation
 */
static void
gtk_combo_box_cell_layout_pack_start (GtkCellLayout   *layout,
                                      GtkCellRenderer *cell,
                                      gboolean         expand)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box;
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  combo_box = GTK_COMBO_BOX (layout);

  g_object_ref (G_OBJECT (cell));
  gtk_object_sink (GTK_OBJECT (cell));

  info = g_new0 (ComboCellInfo, 1);
  info->cell = cell;
  info->expand = expand;
  info->pack = GTK_PACK_START;

  combo_box->priv->cells = g_slist_append (combo_box->priv->cells, info);

  if (combo_box->priv->cell_view)
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                                cell, expand);

  if (combo_box->priv->column)
    gtk_tree_view_column_pack_start (combo_box->priv->column, cell, expand);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view), cell, expand);
        }
      g_list_free (list);
    }
}

static void
gtk_combo_box_cell_layout_pack_end (GtkCellLayout   *layout,
                                    GtkCellRenderer *cell,
                                    gboolean         expand)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box;
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  combo_box = GTK_COMBO_BOX (layout);

  g_object_ref (G_OBJECT (cell));
  gtk_object_sink (GTK_OBJECT (cell));

  info = g_new0 (ComboCellInfo, 1);
  info->cell = cell;
  info->expand = expand;
  info->pack = GTK_PACK_END;

  combo_box->priv->cells = g_slist_append (combo_box->priv->cells, info);

  if (combo_box->priv->cell_view)
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                              cell, expand);

  if (combo_box->priv->column)
    gtk_tree_view_column_pack_end (combo_box->priv->column, cell, expand);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (view), cell, expand);
        }
      g_list_free (list);
    }
}

static void
gtk_combo_box_cell_layout_clear (GtkCellLayout *layout)
{
  GtkWidget *menu;
  GtkComboBox *combo_box;
  GSList *i;
  
  g_return_if_fail (GTK_IS_COMBO_BOX (layout));

  combo_box = GTK_COMBO_BOX (layout);
 
  if (combo_box->priv->cell_view)
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box->priv->cell_view));

  if (combo_box->priv->column)
    gtk_tree_view_column_clear (combo_box->priv->column);

  for (i = combo_box->priv->cells; i; i = i->next)
    {
     ComboCellInfo *info = (ComboCellInfo *)i->data;

      gtk_combo_box_cell_layout_clear_attributes (layout, info->cell);
      g_object_unref (G_OBJECT (info->cell));
      g_free (info);
      i->data = NULL;
    }
  g_slist_free (combo_box->priv->cells);
  combo_box->priv->cells = NULL;

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_clear (GTK_CELL_LAYOUT (view));
        }
      g_list_free (list);
    }
}

static void
gtk_combo_box_cell_layout_add_attribute (GtkCellLayout   *layout,
                                         GtkCellRenderer *cell,
                                         const gchar     *attribute,
                                         gint             column)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box;
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  combo_box = GTK_COMBO_BOX (layout);

  info = gtk_combo_box_get_cell_info (combo_box, cell);

  info->attributes = g_slist_prepend (info->attributes,
                                      GINT_TO_POINTER (column));
  info->attributes = g_slist_prepend (info->attributes,
                                      g_strdup (attribute));

  if (combo_box->priv->cell_view)
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                                   cell, attribute, column);

  if (combo_box->priv->column)
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box->priv->column),
                                   cell, attribute, column);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (view), cell,
                                         attribute, column);
        }
      g_list_free (list);
    }

  gtk_widget_queue_resize (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_cell_layout_set_cell_data_func (GtkCellLayout         *layout,
                                              GtkCellRenderer       *cell,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         destroy)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box;
  GtkWidget *menu;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));

  combo_box = GTK_COMBO_BOX (layout);

  info = gtk_combo_box_get_cell_info (combo_box, cell);
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

  if (combo_box->priv->cell_view)
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo_box->priv->cell_view), cell, func, func_data, NULL);

  if (combo_box->priv->column)
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo_box->priv->column), cell, func, func_data, NULL);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (view), cell,
                                              func, func_data, NULL);
        }
      g_list_free (list);
    }

  gtk_widget_queue_resize (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_cell_layout_clear_attributes (GtkCellLayout   *layout,
                                            GtkCellRenderer *cell)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box;
  GtkWidget *menu;
  GSList *list;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  combo_box = GTK_COMBO_BOX (layout);

  info = gtk_combo_box_get_cell_info (combo_box, cell);
  if (info)
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

  if (combo_box->priv->cell_view)
    gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (combo_box->priv->cell_view), cell);

  if (combo_box->priv->column)
    gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (combo_box->priv->column), cell);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (view), cell);
        }
      g_list_free (list);
    }

  gtk_widget_queue_resize (GTK_WIDGET (combo_box));
}

static void
gtk_combo_box_cell_layout_reorder (GtkCellLayout   *layout,
                                   GtkCellRenderer *cell,
                                   gint             position)
{
  ComboCellInfo *info;
  GtkComboBox *combo_box;
  GtkWidget *menu;
  GSList *link;

  g_return_if_fail (GTK_IS_COMBO_BOX (layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  combo_box = GTK_COMBO_BOX (layout);

  info = gtk_combo_box_get_cell_info (combo_box, cell);

  g_return_if_fail (info != NULL);
  g_return_if_fail (position >= 0);

  link = g_slist_find (combo_box->priv->cells, info);

  g_return_if_fail (link != NULL);

  combo_box->priv->cells = g_slist_delete_link (combo_box->priv->cells, link);
  combo_box->priv->cells = g_slist_insert (combo_box->priv->cells, info,
                                           position);

  if (combo_box->priv->cell_view)
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo_box->priv->cell_view),
                             cell, position);

  if (combo_box->priv->column)
    gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo_box->priv->column),
                             cell, position);

  menu = combo_box->priv->popup_widget;
  if (GTK_IS_MENU (menu))
    {
      GList *i, *list;

      list = gtk_container_get_children (GTK_CONTAINER (menu));
      for (i = list; i; i = i->next)
        {
          GtkCellView *view;

          if (GTK_IS_CELL_VIEW_MENU_ITEM (i->data))
            view = GTK_CELL_VIEW (GTK_BIN (i->data)->child);
          else
            view = GTK_CELL_VIEW (i->data);

          gtk_cell_layout_reorder (GTK_CELL_LAYOUT (view), cell, position);
        }
      g_list_free (list);
    }

  gtk_widget_queue_draw (GTK_WIDGET (combo_box));
}

/*
 * public API
 */

/**
 * gtk_combo_box_new:
 *
 * Creates a new empty #GtkComboBox.
 *
 * Return value: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_COMBO_BOX, NULL));
}

/**
 * gtk_combo_box_new_with_model:
 * @model: A #GtkTreeModel.
 *
 * Creates a new #GtkComboBox with the model initialized to @model.
 *
 * Return value: A new #GtkComboBox.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new_with_model (GtkTreeModel *model)
{
  GtkComboBox *combo_box;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

  combo_box = GTK_COMBO_BOX (g_object_new (GTK_TYPE_COMBO_BOX,
                                           "model", model,
                                           NULL));

  return GTK_WIDGET (combo_box);
}

/**
 * gtk_combo_box_set_wrap_width:
 * @combo_box: A #GtkComboBox.
 * @width: Preferred number of columns.
 *
 * Sets the wrap width of @combo_box to be @width. The wrap width is basically
 * the preferred number of columns when you want to the popup to be layed out
 * in a table.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_wrap_width (GtkComboBox *combo_box,
                              gint         width)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (width >= 0);

  if (width != combo_box->priv->wrap_width)
    {
      combo_box->priv->wrap_width = width;

      gtk_combo_box_check_appearance (combo_box);
      gtk_combo_box_relayout (combo_box);
      
      g_object_notify (G_OBJECT (combo_box), "wrap_width");
    }
}

/**
 * gtk_combo_box_set_row_span_column:
 * @combo_box: A #GtkComboBox.
 * @row_span: A column in the model passed during construction.
 *
 * Sets the column with row span information for @combo_box to be @row_span.
 * The row span column contains integers which indicate how many rows
 * an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_row_span_column (GtkComboBox *combo_box,
                                   gint         row_span)
{
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  col = gtk_tree_model_get_n_columns (combo_box->priv->model);
  g_return_if_fail (row_span >= 0 && row_span < col);

  if (row_span != combo_box->priv->row_column)
    {
      combo_box->priv->row_column = row_span;
      
      gtk_combo_box_relayout (combo_box);
 
      g_object_notify (G_OBJECT (combo_box), "row_span_column");
    }
}

/**
 * gtk_combo_box_set_column_span_column:
 * @combo_box: A #GtkComboBox.
 * @column_span: A column in the model passed during construction.
 *
 * Sets the column with column span information for @combo_box to be
 * @column_span. The column span column contains integers which indicate
 * how many columns an item should span.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_column_span_column (GtkComboBox *combo_box,
                                      gint         column_span)
{
  gint col;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  col = gtk_tree_model_get_n_columns (combo_box->priv->model);
  g_return_if_fail (column_span >= 0 && column_span < col);

  if (column_span != combo_box->priv->col_column)
    {
      combo_box->priv->col_column = column_span;
      
      gtk_combo_box_relayout (combo_box);

      g_object_notify (G_OBJECT (combo_box), "column_span_column");
    }
}

/**
 * gtk_combo_box_get_active:
 * @combo_box: A #GtkComboBox.
 *
 * Returns the index of the currently active item, or -1 if there's no
 * active item.
 *
 * Return value: An integer which is the index of the currently active item, or
 * -1 if there's no active item.
 *
 * Since: 2.4
 */
gint
gtk_combo_box_get_active (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), 0);

  return combo_box->priv->active_item;
}

/**
 * gtk_combo_box_set_active:
 * @combo_box: A #GtkComboBox.
 * @index_: An index in the model passed during construction, or -1 to have
 * no active item.
 *
 * Sets the active item of @combo_box to be the item at @index.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_active (GtkComboBox *combo_box,
                          gint         index_)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  /* -1 means "no item selected" */
  g_return_if_fail (index_ >= -1);

  if (combo_box->priv->active_item == index_)
    return;
  
  gtk_combo_box_set_active_internal (combo_box, index_);
}

static void
gtk_combo_box_set_active_internal (GtkComboBox *combo_box,
				   gint         index)
{
  GtkTreePath *path;

  combo_box->priv->active_item = index;

  if (index == -1)
    {
      if (combo_box->priv->tree_view)
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (combo_box->priv->tree_view)));
      else
        {
          GtkMenu *menu = GTK_MENU (combo_box->priv->popup_widget);

          if (GTK_IS_MENU (menu))
            gtk_menu_set_active (menu, -1);
        }

      if (combo_box->priv->cell_view)
        gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (combo_box->priv->cell_view), NULL);
    }
  else
    {
#if GTK_CHECK_VERSION(2,2,0)
      path = gtk_tree_path_new_from_indices (index, -1);
#else
      char buf[32];
      g_snprintf(buf, sizeof(buf), "%d", index);
      path = gtk_tree_path_new_from_string(buf);
#endif

      if (combo_box->priv->tree_view)
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (combo_box->priv->tree_view), path, NULL, FALSE);
      else
        {
          GtkMenu *menu = GTK_MENU (combo_box->priv->popup_widget);

          if (GTK_IS_MENU (menu))
            gtk_menu_set_active (GTK_MENU (menu), index);
        }

      if (combo_box->priv->cell_view)
	gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (combo_box->priv->cell_view), path);

      gtk_tree_path_free (path);
    }

  g_signal_emit_by_name (combo_box, "changed", NULL, NULL);
}


/**
 * gtk_combo_box_get_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: The uninitialized #GtkTreeIter.
 * 
 * Sets @iter to point to the current active item, if it exists.
 * 
 * Return value: %TRUE, if @iter was set
 *
 * Since: 2.4
 **/
gboolean
gtk_combo_box_get_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path;
  gint active;
  gboolean retval;
#if !GTK_CHECK_VERSION(2,2,0)
  char buf[32];
#endif

  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), FALSE);

  active = gtk_combo_box_get_active (combo_box);
  if (active < 0)
    return FALSE;

#if GTK_CHECK_VERSION(2,2,0)
  path = gtk_tree_path_new_from_indices (active, -1);
#else
  g_snprintf(buf, sizeof(buf), "%d", active);
  path = gtk_tree_path_new_from_string(buf);
#endif
  retval = gtk_tree_model_get_iter (gtk_combo_box_get_model (combo_box),
                                    iter, path);
  gtk_tree_path_free (path);

  return retval;
}

/**
 * gtk_combo_box_set_active_iter:
 * @combo_box: A #GtkComboBox
 * @iter: The #GtkTreeIter.
 * 
 * Sets the current active item to be the one referenced by @iter. 
 * @iter must correspond to a path of depth one.
 * 
 * Since: 2.4
 **/
void
gtk_combo_box_set_active_iter (GtkComboBox     *combo_box,
                               GtkTreeIter     *iter)
{
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo_box), iter);
  g_return_if_fail (path != NULL);
  g_return_if_fail (gtk_tree_path_get_depth (path) == 1);
  
  gtk_combo_box_set_active (combo_box, gtk_tree_path_get_indices (path)[0]);
  gtk_tree_path_free (path);
}

/**
 * gtk_combo_box_set_model:
 * @combo_box: A #GtkComboBox.
 * @model: A #GtkTreeModel.
 *
 * Sets the model used by @combo_box to be @model. Will unset a previously set 
 * model (if applicable). If @model is %NULL, then it will unset the model.
 * 
 * Note that this function does not clear the cell renderers, you have to 
 * call gtk_combo_box_cell_layout_clear() yourself if you need to set up 
 * different cell renderers for the new model.
 *
 * Since: 2.4
 */
void
gtk_combo_box_set_model (GtkComboBox  *combo_box,
                         GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  if (!model)
    {
      gtk_combo_box_unset_model (combo_box);
      return;
    }

  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  if (model == combo_box->priv->model)
    return;
  
  if (combo_box->priv->model)
    gtk_combo_box_unset_model (combo_box);

  combo_box->priv->model = model;
  g_object_ref (G_OBJECT (combo_box->priv->model));

  combo_box->priv->inserted_id =
    g_signal_connect (combo_box->priv->model, "row_inserted",
		      G_CALLBACK (gtk_combo_box_model_row_inserted),
		      combo_box);
  combo_box->priv->deleted_id =
    g_signal_connect (combo_box->priv->model, "row_deleted",
		      G_CALLBACK (gtk_combo_box_model_row_deleted),
		      combo_box);
  combo_box->priv->reordered_id =
    g_signal_connect (combo_box->priv->model, "rows_reordered",
		      G_CALLBACK (gtk_combo_box_model_rows_reordered),
		      combo_box);
  combo_box->priv->changed_id =
    g_signal_connect (combo_box->priv->model, "row_changed",
		      G_CALLBACK (gtk_combo_box_model_row_changed),
		      combo_box);
      
  if (combo_box->priv->tree_view)
    {
      /* list mode */
      gtk_tree_view_set_model (GTK_TREE_VIEW (combo_box->priv->tree_view),
                               combo_box->priv->model);
    }
  else
    {
      /* menu mode */
      if (combo_box->priv->popup_widget)
	gtk_combo_box_menu_fill (combo_box);

    }

  if (combo_box->priv->cell_view)
    gtk_cell_view_set_model (GTK_CELL_VIEW (combo_box->priv->cell_view),
                             combo_box->priv->model);
}

/**
 * gtk_combo_box_get_model
 * @combo_box: A #GtkComboBox.
 *
 * Returns the #GtkTreeModel which is acting as data source for @combo_box.
 *
 * Return value: A #GtkTreeModel which was passed during construction.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_combo_box_get_model (GtkComboBox *combo_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), NULL);

  return combo_box->priv->model;
}


/* convenience API for simple text combos */

/**
 * gtk_combo_box_new_text:
 *
 * Convenience function which constructs a new text combo box, which is a
 * #GtkComboBox just displaying strings. If you use this function to create
 * a text combo box, you should only manipulate its data source with the
 * following convenience functions: gtk_combo_box_append_text(),
 * gtk_combo_box_insert_text(), gtk_combo_box_prepend_text() and
 * gtk_combo_box_remove_text().
 *
 * Return value: A new text combo box.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_new_text (void)
{
  GtkWidget *combo_box;
  GtkCellRenderer *cell;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_STRING);
  combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
                                  "text", 0,
                                  NULL);

  return combo_box;
}

/**
 * gtk_combo_box_append_text:
 * @combo_box: A #GtkComboBox constructed using gtk_combo_box_new_text().
 * @text: A string.
 *
 * Appends @string to the list of strings stored in @combo_box. Note that
 * you can only use this function with combo boxes constructed with
 * gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_append_text (GtkComboBox *combo_box,
                           const gchar *text)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (combo_box->priv->model);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

/**
 * gtk_combo_box_insert_text:
 * @combo_box: A #GtkComboBox constructed using gtk_combo_box_new_text().
 * @position: An index to insert @text.
 * @text: A string.
 *
 * Inserts @string at @position in the list of strings stored in @combo_box.
 * Note that you can only use this function with combo boxes constructed
 * with gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_insert_text (GtkComboBox *combo_box,
                           gint         position,
                           const gchar *text)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (position >= 0);
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (combo_box->priv->model);

  gtk_list_store_insert (store, &iter, position);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

/**
 * gtk_combo_box_prepend_text:
 * @combo_box: A #GtkComboBox constructed with gtk_combo_box_new_text().
 * @text: A string.
 *
 * Prepends @string to the list of strings stored in @combo_box. Note that
 * you can only use this function with combo boxes constructed with
 * gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_prepend_text (GtkComboBox *combo_box,
                            const gchar *text)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (combo_box->priv->model);

  gtk_list_store_prepend (store, &iter);
  gtk_list_store_set (store, &iter, 0, text, -1);
}

/**
 * gtk_combo_box_remove_text:
 * @combo_box: A #GtkComboBox constructed with gtk_combo_box_new_text().
 * @position: Index of the item to remove.
 *
 * Removes the string at @position from @combo_box. Note that you can only use
 * this function with combo boxes constructed with gtk_combo_box_new_text().
 *
 * Since: 2.4
 */
void
gtk_combo_box_remove_text (GtkComboBox *combo_box,
                           gint         position)
{
  GtkTreeIter iter;
  GtkListStore *store;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
  g_return_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model));
  g_return_if_fail (position >= 0);

  store = GTK_LIST_STORE (combo_box->priv->model);

  if (gtk_tree_model_iter_nth_child (combo_box->priv->model, &iter,
                                     NULL, position))
    gtk_list_store_remove (store, &iter);
}

static gboolean
gtk_combo_box_mnemonic_activate (GtkWidget *widget,
				 gboolean   group_cycling)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (widget);

  gtk_widget_grab_focus (combo_box->priv->button);

  return TRUE;
}

static void
gtk_combo_box_destroy (GtkObject *object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);

  gtk_combo_box_popdown (combo_box); 

  combo_box->priv->destroying = 1;

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
  combo_box->priv->cell_view = NULL;

  combo_box->priv->destroying = 0;
}

static void
gtk_combo_box_finalize (GObject *object)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (object);
  GSList *i;
  
  if (GTK_IS_MENU (combo_box->priv->popup_widget))
    {
      gtk_combo_box_menu_destroy (combo_box);
      gtk_menu_detach (GTK_MENU (combo_box->priv->popup_widget));
      combo_box->priv->popup_widget = NULL;
    }
  
  if (GTK_IS_TREE_VIEW (combo_box->priv->tree_view))
    gtk_combo_box_list_destroy (combo_box);

  if (combo_box->priv->popup_window)
    gtk_widget_destroy (combo_box->priv->popup_window);

  gtk_combo_box_unset_model (combo_box);

  for (i = combo_box->priv->cells; i; i = i->next)
    {
      ComboCellInfo *info = (ComboCellInfo *)i->data;
      GSList *list = info->attributes;

      if (info->destroy)
	info->destroy (info->func_data);

      while (list && list->next)
	{
	  g_free (list->data);
	  list = list->next->next;
	}
      g_slist_free (info->attributes);

      g_object_unref (G_OBJECT (info->cell));
      g_free (info);
    }
   g_slist_free (combo_box->priv->cells);

   g_free (combo_box->priv);

   G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * Code below this point has been pulled in from gtkmenu.c in 2.4.14
 * and is needed to provide gtk_menu_attach()
 */

typedef struct
{
  gint left_attach;
  gint right_attach;
  gint top_attach;
  gint bottom_attach;
  gint effective_left_attach;
  gint effective_right_attach;
  gint effective_top_attach;
  gint effective_bottom_attach;
} AttachInfo;

#define ATTACH_INFO_KEY "gtk-menu-child-attach-info-key"

static AttachInfo *
get_attach_info (GtkWidget *child)
{
  GObject *object = G_OBJECT (child);
  AttachInfo *ai = g_object_get_data (object, ATTACH_INFO_KEY);
                                                                                                   
  if (!ai)
    {
      ai = g_new0 (AttachInfo, 1);
      g_object_set_data_full (object, ATTACH_INFO_KEY, ai, g_free);
    }
                                                                                                   
  return ai;
}

/**
 * gtk_menu_attach:
 * @menu: a #GtkMenu.
 * @child: a #GtkMenuItem.
 * @left_attach: The column number to attach the left side of the item to.
 * @right_attach: The column number to attach the right side of the item to.
 * @top_attach: The row number to attach the top of the item to.
 * @bottom_attach: The row number to attach the bottom of the item to.
 *
 * Adds a new #GtkMenuItem to a (table) menu. The number of 'cells' that
 * an item will occupy is specified by @left_attach, @right_attach,
 * @top_attach and @bottom_attach. These each represent the leftmost,
 * rightmost, uppermost and lower column and row numbers of the table.
 * (Columns and rows are indexed from zero).
 *
 * Note that this function is not related to gtk_menu_detach().
 *
 * Since: 2.4
 **/
static void
gtk_menu_attach (GtkMenu   *menu,
                 GtkWidget *child,
                 guint      left_attach,
                 guint      right_attach,
                 guint      top_attach,
                 guint      bottom_attach)
{
  GtkMenuShell *menu_shell;
  
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));
  g_return_if_fail (child->parent == NULL || 
		    child->parent == GTK_WIDGET (menu));
  g_return_if_fail (left_attach < right_attach);
  g_return_if_fail (top_attach < bottom_attach);

  menu_shell = GTK_MENU_SHELL (menu);
  
  if (!child->parent)
    {
      AttachInfo *ai = get_attach_info (child);
      
      ai->left_attach = left_attach;
      ai->right_attach = right_attach;
      ai->top_attach = top_attach;
      ai->bottom_attach = bottom_attach;
      
      menu_shell->children = g_list_append (menu_shell->children, child);

      gtk_widget_set_parent (child, GTK_WIDGET (menu));

      /*
      menu_queue_resize (menu);
      */
    }
  else
    {
      gtk_container_child_set (GTK_CONTAINER (child->parent), child,
			       "left_attach",   left_attach,
			       "right_attach",  right_attach,
			       "top_attach",    top_attach,
			       "bottom_attach", bottom_attach,
			       NULL);
    }
}
#endif /* Gtk 2.4 */

gchar *
gtk_combo_box_get_active_text (GtkComboBox *combo_box)
{
  GtkTreeIter iter;
  gchar *text = NULL;

  /* g_return_val_if_fail (GTK_IS_LIST_STORE (combo_box->priv->model), NULL); */

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    gtk_tree_model_get (gtk_combo_box_get_model(combo_box), &iter, 
    			0, &text, -1);
  return text;
}

#endif
