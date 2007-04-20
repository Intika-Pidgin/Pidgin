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

GType gnt_clipboard_get_gtype(void);

gchar *gnt_clipboard_get_string(GntClipboard *clip);

void gnt_clipboard_set_string(GntClipboard *clip, gchar *string);

G_END_DECLS

#endif
