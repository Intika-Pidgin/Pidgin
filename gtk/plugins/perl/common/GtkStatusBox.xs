#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
gtk_gaim_status_box_new()

Gtk::Widget
gtk_gaim_status_box_new_with_account(account)
	Gaim::Account account

void
gtk_gaim_status_box_add(status_box, type, pixbuf, text, sec_text, data)
	Gaim::GtkUI::StatusBox status_box
	Gaim::GtkUI::StatusBox::ItemType type
	GdkPixbuf pixbuf
	const char * text
	const char * sec_text
	gpointer data
*/

MODULE = Gaim::GtkUI::StatusBox  PACKAGE = Gaim::GtkUI::StatusBox  PREFIX = gtk_gaim_status_box_
PROTOTYPES: ENABLE

void
gtk_gaim_status_box_add_separator(status_box)
	Gaim::GtkUI::StatusBox status_box

void
gtk_gaim_status_box_set_connecting(status_box, connecting)
	Gaim::GtkUI::StatusBox status_box
	gboolean connecting

void
gtk_gaim_status_box_pulse_connecting(status_box)
	Gaim::GtkUI::StatusBox status_box

void
gtk_gaim_status_box_set_buddy_icon(status_box, filename)
	Gaim::GtkUI::StatusBox status_box
	const char * filename

const char *
gtk_gaim_status_box_get_buddy_icon(status_box)
	Gaim::GtkUI::StatusBox status_box

gchar_own *
gtk_gaim_status_box_get_message(status_box)
	Gaim::GtkUI::StatusBox status_box
