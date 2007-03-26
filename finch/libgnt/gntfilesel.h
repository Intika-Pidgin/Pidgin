#ifndef GNT_FILE_SEL_H
#define GNT_FILE_SEL_H

#include "gntwindow.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_FILE_SEL				(gnt_file_sel_get_gtype())
#define GNT_FILE_SEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_FILE_SEL, GntFileSel))
#define GNT_FILE_SEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_FILE_SEL, GntFileSelClass))
#define GNT_IS_FILE_SEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_FILE_SEL))
#define GNT_IS_FILE_SEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_FILE_SEL))
#define GNT_FILE_SEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_FILE_SEL, GntFileSelClass))

#define GNT_FILE_SEL_FLAGS(obj)				(GNT_FILE_SEL(obj)->priv.flags)
#define GNT_FILE_SEL_SET_FLAGS(obj, flags)		(GNT_FILE_SEL_FLAGS(obj) |= flags)
#define GNT_FILE_SEL_UNSET_FLAGS(obj, flags)	(GNT_FILE_SEL_FLAGS(obj) &= ~(flags))

typedef struct _GnFileSel			GntFileSel;
typedef struct _GnFileSelPriv		GntFileSelPriv;
typedef struct _GnFileSelClass		GntFileSelClass;

struct _GnFileSel
{
	GntWindow parent;

	GntWidget *dirs;     /* list of files */
	GntWidget *files;    /* list of directories */
	GntWidget *location; /* location entry */

	GntWidget *select;   /* select button */
	GntWidget *cancel;   /* cancel button */

	char *current; /* Full path of the current location */
	/* XXX: someone should make these useful */
	gboolean must_exist; /* Make sure the selected file (the name entered in 'location') exists */
	gboolean dirsonly;   /* Show only directories */
};

struct _GnFileSelClass
{
	GntWindowClass parent;

	void (*file_selected)(GntFileSel *sel, const char *path, const char *filename);
	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_file_sel_get_gtype(void);

GntWidget *gnt_file_sel_new(void);

gboolean gnt_file_sel_set_current_location(GntFileSel *sel, const char *path);

const char *gnt_file_sel_get_current_location(GntFileSel *sel);

void gnt_file_sel_set_dirs_only(GntFileSel *sel, gboolean dirs);

gboolean gnt_file_sel_get_dirs_only(GntFileSel *sel);

void gnt_file_sel_set_must_exist(GntFileSel *sel, gboolean must);

gboolean gnt_file_sel_get_must_exist(GntFileSel *sel);

char *gnt_file_sel_get_selected_file(GntFileSel *sel);  /* The returned value should be free'd */

G_END_DECLS

#endif /* GNT_FILE_SEL_H */
