/*
 * gaim
 *
 * File: win32dep.h
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _GTKWIN32DEP_H_
#define _GTKWIN32DEP_H_
#include <windows.h>
#include <gtk/gtk.h>
#include "conversation.h"

HINSTANCE gtkwgaim_hinstance(void);

/* Utility */
int gtkwgaim_gz_decompress(const char* in, const char* out);
int gtkwgaim_gz_untar(const char* filename, const char* destdir);

/* Misc */
void gtkwgaim_notify_uri(const char *uri);
void gtkwgaim_shell_execute(const char *target, const char *verb, const char *clazz);
void gtkwgaim_ensure_onscreen(GtkWidget *win);
void gtkwgaim_conv_blink(GaimConversation *conv, GaimMessageFlags flags);
void gtkwgaim_window_flash(GtkWindow *window, gboolean flash);

/* init / cleanup */
void gtkwgaim_init(HINSTANCE);
void gtkwgaim_post_init(void);
void gtkwgaim_cleanup(void);

#endif /* _WIN32DEP_H_ */

