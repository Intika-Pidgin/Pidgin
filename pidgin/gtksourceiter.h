/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *  gtksourceiter.h
 *
 *  Pidgin is the legal property of its developers, whose names are too numerous
 *  to list here.  Please refer to the COPYRIGHT file distributed with this
 *  source distribution.
 *
 *  The following copyright notice applies to this file:
 *
 *  Copyright (C) 2000 - 2005 Paolo Maggi
 *  Copyright (C) 2002, 2003 Jeroen Zwartepoorte
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#ifndef _PIDGINSOURCEITER_H_
#define _PIDGINSOURCEITER_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
	GTK_SOURCE_SEARCH_VISIBLE_ONLY		 = 1 << 0,
	GTK_SOURCE_SEARCH_TEXT_ONLY		 = 1 << 1,
	GTK_SOURCE_SEARCH_CASE_INSENSITIVE	 = 1 << 2
	/* Possible future plans: SEARCH_REGEXP */
} GtkSourceSearchFlags;

gboolean gtk_source_iter_forward_search	(const GtkTextIter   *iter,
						 const gchar         *str,
						 GtkSourceSearchFlags flags,
						 GtkTextIter         *match_start,
						 GtkTextIter         *match_end,
						 const GtkTextIter   *limit);

gboolean gtk_source_iter_backward_search	(const GtkTextIter   *iter,
						 const gchar         *str,
						 GtkSourceSearchFlags flags,
						 GtkTextIter         *match_start,
						 GtkTextIter         *match_end,
						 const GtkTextIter   *limit);

gboolean gtk_source_iter_find_matching_bracket	(GtkTextIter         *iter);

G_END_DECLS

#endif /* _PIDGINSOURCEITER_H_ */
