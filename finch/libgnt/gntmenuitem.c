#include "gntmenu.h"
#include "gntmenuitem.h"

static GObjectClass *parent_class = NULL;

static void
gnt_menuitem_destroy(GObject *obj)
{
	GntMenuItem *item = GNT_MENU_ITEM(obj);
	g_free(item->text);
	item->text = NULL;
	if (item->submenu)
		gnt_widget_destroy(GNT_WIDGET(item->submenu));
	parent_class->dispose(obj);
}

static void
gnt_menuitem_class_init(GntMenuItemClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = gnt_menuitem_destroy;
}

static void
gnt_menuitem_init(GTypeInstance *instance, gpointer class)
{
}

/******************************************************************************
 * GntMenuItem API
 *****************************************************************************/
GType
gnt_menuitem_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntMenuItemClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_menuitem_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntMenuItem),
			0,						/* n_preallocs		*/
			gnt_menuitem_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntMenuItem",
									  &info, 0);
	}

	return type;
}

GntMenuItem *gnt_menuitem_new(const char *text)
{
	GObject *item = g_object_new(GNT_TYPE_MENU_ITEM, NULL);
	GntMenuItem *menuitem = GNT_MENU_ITEM(item);

	menuitem->text = g_strdup(text);

	return menuitem;
}

void gnt_menuitem_set_callback(GntMenuItem *item, GntMenuItemCallback callback, gpointer data)
{
	item->callback = callback;
	item->callbackdata = data;
}

void gnt_menuitem_set_submenu(GntMenuItem *item, GntMenu *menu)
{
	if (item->submenu)
		gnt_widget_destroy(GNT_WIDGET(item->submenu));
	item->submenu = menu;
}

