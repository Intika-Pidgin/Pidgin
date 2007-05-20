/*
 * gaim
 *
 * File: wspell.h
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
#ifndef _WSPELL_H_
#define _WSPELL_H_
#include <gtkspell/gtkspell.h>

void wgaim_gtkspell_init(void);

extern GtkSpell* (*wgaim_gtkspell_new_attach)(GtkTextView*, const gchar*, GError**);
#define gtkspell_new_attach( view, lang, error ) \
wgaim_gtkspell_new_attach( view, lang, error )

extern GtkSpell* (*wgaim_gtkspell_get_from_text_view)(GtkTextView*);
#define gtkspell_get_from_text_view( view ) \
wgaim_gtkspell_get_from_text_view( view )

extern void (*wgaim_gtkspell_detach)(GtkSpell*);
#define gtkspell_detach( spell ) \
wgaim_gtkspell_detach( spell )

extern gboolean (*wgaim_gtkspell_set_language)(GtkSpell*,	const gchar*, GError**);
#define gtkspell_set_language( spell, lang, error ) \
wgaim_gtkspell_set_language( spell, lang, error )

extern void (*wgaim_gtkspell_recheck_all)(GtkSpell*);
#define gtkspell_recheck_all( spell ) \
wgaim_gtkspell_recheck_all( spell )

#endif /* _WSPELL_H_ */
