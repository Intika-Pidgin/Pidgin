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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GNT_CLIPBOARD_H
#define GNT_CLIPBOARD_H

#include <stdio.h>
#include <glib.h>
#include <glib-object.h>

#define GNT_TYPE_CLIPBOARD				(gnt_clipboard_get_gtype())
#define GNT_CLIPBOARD(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_CLIPBOARD, GntClipboard))
#define GNT_CLIPBOARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_CLIPBOARD, GntClipboardClass))
#define GNT_IS_CLIPBOARD(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_CLIPBOARD))
#define GNT_IS_CLIPBOARD_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_CLIPBOARD))
#define GNT_CLIPBOARD_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_CLIPBOARD, GntClipboardClass))

typedef struct _GntClipboard			GntClipboard;
typedef struct _GntClipboardClass		GntClipboardClass;

struct _GntClipboard
{
	GObject inherit;
	gchar *string;
};

struct _GntClipboardClass
{
	GObjectClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * 
 *
 * @return
 */
GType gnt_clipboard_get_gtype(void);

/**
 * 
 * @param clip
 *
 * @return
 */
gchar * gnt_clipboard_get_string(GntClipboard *clip);

/**
 * 
 * @param clip
 * @param string
 */
void gnt_clipboard_set_string(GntClipboard *clip, gchar *string);

G_END_DECLS

#endif
