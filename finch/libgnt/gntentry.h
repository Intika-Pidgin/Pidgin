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

#ifndef GNT_ENTRY_H
#define GNT_ENTRY_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_ENTRY				(gnt_entry_get_gtype())
#define GNT_ENTRY(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_ENTRY, GntEntry))
#define GNT_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_ENTRY, GntEntryClass))
#define GNT_IS_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_ENTRY))
#define GNT_IS_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_ENTRY))
#define GNT_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_ENTRY, GntEntryClass))

#define GNT_ENTRY_FLAGS(obj)				(GNT_ENTRY(obj)->priv.flags)
#define GNT_ENTRY_SET_FLAGS(obj, flags)		(GNT_ENTRY_FLAGS(obj) |= flags)
#define GNT_ENTRY_UNSET_FLAGS(obj, flags)	(GNT_ENTRY_FLAGS(obj) &= ~(flags))

#define	ENTRY_CHAR		'_'			/* The character to use to fill in the blank places */

typedef struct _GntEntry			GntEntry;
typedef struct _GntEntryPriv		GntEntryPriv;
typedef struct _GntEntryClass	GntEntryClass;

typedef enum
{
	GNT_ENTRY_FLAG_ALPHA    = 1 << 0,  /* Only alpha */
	GNT_ENTRY_FLAG_INT      = 1 << 1,  /* Only integer */
	GNT_ENTRY_FLAG_NO_SPACE = 1 << 2,  /* No blank space is allowed */
	GNT_ENTRY_FLAG_NO_PUNCT = 1 << 3,  /* No punctuations */
	GNT_ENTRY_FLAG_MASK     = 1 << 4,  /* Mask the inputs */
} GntEntryFlag;

#define GNT_ENTRY_FLAG_ALL    (GNT_ENTRY_FLAG_ALPHA | GNT_ENTRY_FLAG_INT)

struct _GntEntry
{
	GntWidget parent;

	GntEntryFlag flag;

	char *start;
	char *end;
	char *scroll;   /* Current scrolling position */
	char *cursor;   /* Cursor location */
	                /* 0 <= cursor - scroll < widget-width */
	
	size_t buffer;  /* Size of the buffer */
	
	int max;        /* 0 means infinite */
	gboolean masked;

	GList *history; /* History of the strings. User can use this by pressing ctrl+up/down */
	int histlength; /* How long can the history be? */

	GList *suggests;    /* List of suggestions */
	gboolean word;      /* Are the suggestions for only a word, or for the whole thing? */
	gboolean always;    /* Should the list of suggestions show at all times, or only on tab-press? */
	GntWidget *ddown;   /* The dropdown with the suggested list */
};

struct _GntEntryClass
{
	GntWidgetClass parent;

	void (*text_changed)(GntEntry *entry);
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
GType gnt_entry_get_gtype(void);

/**
 * 
 * @param text
 *
 * @return
 */
GntWidget * gnt_entry_new(const char *text);

/**
 * 
 * @param entry
 * @param max
 */
void gnt_entry_set_max(GntEntry *entry, int max);

/**
 * 
 * @param entry
 * @param text
 */
void gnt_entry_set_text(GntEntry *entry, const char *text);

/**
 * 
 * @param entry
 * @param flag
 */
void gnt_entry_set_flag(GntEntry *entry, GntEntryFlag flag);

const char *gnt_entry_get_text(GntEntry *entry);

/**
 * 
 * @param entry
 */
void gnt_entry_clear(GntEntry *entry);

/**
 * 
 * @param entry
 * @param set
 */
void gnt_entry_set_masked(GntEntry *entry, gboolean set);

/**
 * 
 * @param entry
 * @param text
 */
void gnt_entry_add_to_history(GntEntry *entry, const char *text);

/**
 * 
 * @param entry
 * @param num
 */
void gnt_entry_set_history_length(GntEntry *entry, int num);

/**
 * 
 * @param entry
 * @param word
 */
void gnt_entry_set_word_suggest(GntEntry *entry, gboolean word);

/**
 * 
 * @param entry
 * @param always
 */
void gnt_entry_set_always_suggest(GntEntry *entry, gboolean always);

/**
 * 
 * @param entry
 * @param text
 */
void gnt_entry_add_suggest(GntEntry *entry, const char *text);

/**
 * 
 * @param entry
 * @param text
 */
void gnt_entry_remove_suggest(GntEntry *entry, const char *text);

G_END_DECLS

#endif /* GNT_ENTRY_H */
