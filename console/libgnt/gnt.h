#include <glib.h>
#include "gntwidget.h"
#include "gntcolors.h"
#include "gntkeys.h"

void gnt_init();

void gnt_main();

gboolean gnt_ascii_only();

void gnt_screen_occupy(GntWidget *widget);

void gnt_screen_release(GntWidget *widget);

void gnt_screen_update(GntWidget *widget);

void gnt_screen_take_focus(GntWidget *widget);

void gnt_screen_resize_widget(GntWidget *widget, int width, int height);

void gnt_screen_move_widget(GntWidget *widget, int x, int y);

void gnt_screen_rename_widget(GntWidget *widget, const char *text);

gboolean gnt_widget_has_focus(GntWidget *widget);

void gnt_widget_set_urgent(GntWidget *widget);

void gnt_register_action(const char *label, void (*callback)());

gboolean gnt_screen_menu_show(gpointer menu);

void gnt_quit();

