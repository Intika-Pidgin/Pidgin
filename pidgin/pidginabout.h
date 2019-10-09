#ifndef PIDGIN_ABOUT_H
#define PIDGIN_ABOUT_H

/**
 * SECTION:pidginabout
 * @section_id: pidgin-about
 * @short_description: <filename>pidginabout.h</filename>
 * @title: About Dialog
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_ABOUT_DIALOG (pidgin_about_dialog_get_type())
G_DECLARE_FINAL_TYPE(PidginAboutDialog, pidgin_about_dialog, PIDGIN,
		ABOUT_DIALOG, GtkDialog)

GtkWidget *pidgin_about_dialog_new(void);

G_END_DECLS

#endif /* PIDGIN_ABOUT_H */

