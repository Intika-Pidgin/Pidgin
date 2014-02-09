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

#ifndef GNTWM_H
#define GNTWM_H
/**
 * SECTION:gntwm
 * @section_id: libgnt-gntwm
 * @short_description: <filename>gntwm.h</filename>
 * @title: Window-manager API
 */

#include "gntwidget.h"
#include "gntmenu.h"
#include "gntws.h"

#include <panel.h>
#include <time.h>

#define GNT_TYPE_WM				(gnt_wm_get_type())
#define GNT_WM(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WM, GntWM))
#define GNT_WM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_WM, GntWMClass))
#define GNT_IS_WM(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WM))
#define GNT_IS_WM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WM))
#define GNT_WM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WM, GntWMClass))

typedef enum
{
	GNT_KP_MODE_NORMAL,
	GNT_KP_MODE_RESIZE,
	GNT_KP_MODE_MOVE,
	GNT_KP_MODE_WAIT_ON_CHILD
} GntKeyPressMode;

typedef struct _GntNode GntNode;

struct _GntNode
{
	GntWidget *me;

	WINDOW *window;
	int scroll;
	PANEL *panel;
	GntWS *ws;
};

typedef struct _GntWM GntWM;

typedef struct _GntPosition
{
	int x;
	int y;
} GntPosition;

/**
 * GntAction:
 *
 * An application can register actions which will show up in a 'start-menu' like popup
 */
typedef struct _GntAction
{
	const char *label;
	void (*callback)(void);
} GntAction;

typedef struct _GntListWindow {
		GntWidget *window;
		GntWidget *tree;
} GntListWindow;

/**
 * GntWM:
 * @acts: List of actions
 * @menu: Currently active menu. There can be at most one menu at a time on the
 *        screen. If there is a menu being displayed, then all the keystrokes
 *        will be sent to the menu until it is closed, either when the user
 *        activates a menuitem, or presses Escape to cancel the menu.
 * @event_stack: Will be set to %TRUE when a user-event, ie. a mouse-click or a
 *               key-press is being processed. This variable will be used to
 *               determine whether to give focus to a new window.
 */
struct _GntWM
{
	GntBindable inherit;

	/*< public >*/
	GMainLoop *loop;

	GList *workspaces;
	GList *tagged; /* tagged windows */
	GntWS *cws;

	GntListWindow _list;
	GntListWindow *windows;         /* Window-list window */
	GntListWindow *actions;         /* Action-list window */

	GHashTable *nodes;    /* GntWidget -> GntNode */
	GHashTable *name_places;    /* window name -> ws*/
	GHashTable *title_places;    /* window title -> ws */

	GList *acts;

	GntMenu *menu;

	gboolean event_stack;

	GntKeyPressMode mode;

	GHashTable *positions;

	/*< private >*/
	void *res1;
	void *res2;
	void *res3;
	void *res4;
};

typedef struct _GntWMClass GntWMClass;

struct _GntWMClass
{
	GntBindableClass parent;

	/* This is called when a new window is shown */
	void (*new_window)(GntWM *wm, GntWidget *win);

	void (*decorate_window)(GntWM *wm, GntWidget *win);
	/* This is called when a window is being closed */
	gboolean (*close_window)(GntWM *wm, GntWidget *win);

	/* The WM may want to confirm a size for a window first */
	gboolean (*window_resize_confirm)(GntWM *wm, GntWidget *win, int *w, int *h);

	void (*window_resized)(GntWM *wm, GntNode *node);

	/* The WM may want to confirm the position of a window */
	gboolean (*window_move_confirm)(GntWM *wm, GntWidget *win, int *x, int *y);

	void (*window_moved)(GntWM *wm, GntNode *node);

	/* This gets called when:
	 * 	 - the title of the window changes
	 * 	 - the 'urgency' of the window changes
	 */
	void (*window_update)(GntWM *wm, GntNode *node);

	/* This should usually return NULL if the keys were processed by the WM.
	 * If not, the WM can simply return the original string, which will be
	 * processed by the default WM. The custom WM can also return a different
	 * static string for the default WM to process.
	 */
	gboolean (*key_pressed)(GntWM *wm, const char *key);

	gboolean (*mouse_clicked)(GntWM *wm, GntMouseEvent event, int x, int y, GntWidget *widget);

	/* Whatever the WM wants to do when a window is given focus */
	void (*give_focus)(GntWM *wm, GntWidget *widget);

	/* List of windows. Although the WM can keep a list of its own for the windows,
	 * it'd be better if there was a way to share between the 'core' and the WM.
	 */
	/*GList *(*window_list)();*/

	/* This is invoked whenever the terminal window is resized, or the
	 * screen session is attached to a new terminal. (ie, from the
	 * SIGWINCH callback)
	 */
	void (*terminal_refresh)(GntWM *wm);

	void (*res1)(void);
	void (*res2)(void);
	void (*res3)(void);
};

G_BEGIN_DECLS

/**
 * gnt_wm_get_type:
 *
 * Returns: GType for GntWM.
 */
GType gnt_wm_get_type(void);

/**
 * gnt_wm_add_workspace:
 * @wm:   The window-manager.
 * @ws:   The workspace to add.
 *
 * Add a workspace.
 */
void gnt_wm_add_workspace(GntWM *wm, GntWS *ws);

/**
 * gnt_wm_switch_workspace:
 * @wm:   The window-manager.
 * @n:    Index of the workspace to switch to.
 *
 * Switch to a workspace.
 *
 * Returns:   %TRUE if the switch was successful.
 */
gboolean gnt_wm_switch_workspace(GntWM *wm, gint n);

/**
 * gnt_wm_switch_workspace_prev:
 * @wm:  The window-manager.
 *
 * Switch to the previous workspace from the current one.
 */
gboolean gnt_wm_switch_workspace_prev(GntWM *wm);

/**
 * gnt_wm_switch_workspace_next:
 * @wm:  The window-manager.
 *
 * Switch to the next workspace from the current one.
 */
gboolean gnt_wm_switch_workspace_next(GntWM *wm);

/**
 * gnt_wm_widget_move_workspace:
 * @wm:     The window manager.
 * @neww:   The new workspace.
 * @widget: The widget to move.
 *
 * Move a window to a specific workspace.
 */
void gnt_wm_widget_move_workspace(GntWM *wm, GntWS *neww, GntWidget *widget);

/**
 * gnt_wm_set_workspaces:
 * @wm:         The window manager.
 * @workspaces: (element-type Gnt.WS): The list of workspaces.
 *
 * Set the list of workspaces .
 */
void gnt_wm_set_workspaces(GntWM *wm, GList *workspaces);

/**
 * gnt_wm_widget_find_workspace:
 * @wm:       The window-manager.
 * @widget:   The widget to find.
 *
 * Find the workspace that contains a specific widget.
 *
 * Returns: (transfer none): The workspace that has the widget.
 */
GntWS *gnt_wm_widget_find_workspace(GntWM *wm, GntWidget *widget);

/**
 * gnt_wm_new_window:
 * @wm:       The window-manager.
 * @widget:   The new window.
 *
 * Process a new window.
 */
void gnt_wm_new_window(GntWM *wm, GntWidget *widget);

/**
 * gnt_wm_window_decorate:
 * @wm:       The window-manager.
 * @widget:   The widget to decorate.
 *
 * Decorate a window.
 */
void gnt_wm_window_decorate(GntWM *wm, GntWidget *widget);

/**
 * gnt_wm_window_close:
 * @wm:       The window-manager.
 * @widget:   The window to close.
 *
 * Close a window.
 */
void gnt_wm_window_close(GntWM *wm, GntWidget *widget);

/**
 * gnt_wm_process_input:
 * @wm:      The window-manager.
 * @string:  The input string to process.
 *
 * Process input.
 *
 * Returns: %TRUE of the string was processed, %FALSE otherwise.
 */
gboolean gnt_wm_process_input(GntWM *wm, const char *string);

/**
 * gnt_wm_process_click:
 * @wm:      The window manager.
 * @event:   The mouse event.
 * @x:       The x-coordinate of the mouse.
 * @y:       The y-coordinate of the mouse.
 * @widget:  The widget under the mouse.
 *
 * Process a click event.
 *
 * Returns:  %TRUE if the event was handled, %FALSE otherwise.
 */
gboolean gnt_wm_process_click(GntWM *wm, GntMouseEvent event, int x, int y, GntWidget *widget);

/**
 * gnt_wm_resize_window:
 * @wm:        The window manager.
 * @widget:    The window to resize.
 * @width:     The desired width of the window.
 * @height:    The desired height of the window.
 *
 * Resize a window.
 */
void gnt_wm_resize_window(GntWM *wm, GntWidget *widget, int width, int height);

/**
 * gnt_wm_move_window:
 * @wm:      The window manager.
 * @widget:  The window to move.
 * @x:       The desired x-coordinate of the window.
 * @y:       The desired y-coordinate of the window.
 *
 * Move a window.
 */
void gnt_wm_move_window(GntWM *wm, GntWidget *widget, int x, int y);

/**
 * gnt_wm_update_window:
 * @wm:      The window-manager.
 * @widget:  The window to update.
 *
 * Update a window.
 */
void gnt_wm_update_window(GntWM *wm, GntWidget *widget);

/**
 * gnt_wm_raise_window:
 * @wm:      The window-manager.
 * @widget:  The window to raise.
 *
 * Raise a window.
 */
void gnt_wm_raise_window(GntWM *wm, GntWidget *widget);

/**
 * gnt_wm_set_event_stack:
 *
 * Internal function -- do not use.
 */
void gnt_wm_set_event_stack(GntWM *wm, gboolean set);

/**
 * gnt_wm_copy_win:
 *
 * Internal function -- do not use.
 */
void gnt_wm_copy_win(GntWidget *widget, GntNode *node);

/**
 * gnt_wm_get_idle_time:
 *
 * Returns:  The idle time of the user.
 */
time_t gnt_wm_get_idle_time(void);

G_END_DECLS

#endif
