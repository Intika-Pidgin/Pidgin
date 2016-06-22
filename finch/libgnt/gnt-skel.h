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

#ifndef GNT_SKEL_H
#define GNT_SKEL_H
/*
 * SECTION:gnt-skel
 * @section_id: libgnt-gnt-skel
 * @short_description: <filename>gnt-skel.h</filename>
 * @title: Skel API
 */

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_SKEL				(gnt_skel_get_type())
#define GNT_SKEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_SKEL, GntSkel))
#define GNT_SKEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_SKEL, GntSkelClass))
#define GNT_IS_SKEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_SKEL))
#define GNT_IS_SKEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_SKEL))
#define GNT_SKEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_SKEL, GntSkelClass))

typedef struct _GntSkel			GntSkel;
typedef struct _GntSkelPriv		GntSkelPriv;
typedef struct _GntSkelClass		GntSkelClass;

struct _GntSkel
{
	GntWidget parent;
};

struct _GntSkelClass
{
	GntWidgetClass parent;

	/*< private >*/
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_skel_get_type(void);

GntWidget * gnt_skel_new();

G_END_DECLS

#endif /* GNT_SKEL_H */
