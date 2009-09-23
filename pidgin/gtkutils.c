/**
 * @file gtkutils.c GTK+ utility functions
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#define _PIDGIN_GTKUTILS_C_

#include "internal.h"
#include "pidgin.h"

#ifndef _WIN32
# include <X11/Xlib.h>
#else
# ifdef small
#  undef small
# endif
#endif /*_WIN32*/

#ifdef USE_GTKSPELL
# include <gtkspell/gtkspell.h>
# ifdef _WIN32
#  include "wspell.h"
# endif
#endif

#include <gdk/gdkkeysyms.h>

#include "conversation.h"
#include "debug.h"
#include "desktopitem.h"
#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "sound.h"
#include "util.h"

#include "gtkaccount.h"
#include "gtkprefs.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "pidginstock.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "pidgin/minidialog.h"

typedef struct {
	GtkWidget *menu;
	gint default_item;
} AopMenu;

static guint accels_save_timer = 0;
static GList *gnome_url_handlers = NULL;

static gboolean
url_clicked_idle_cb(gpointer data)
{
	purple_notify_uri(NULL, data);
	g_free(data);
	return FALSE;
}

static gboolean
url_clicked_cb(GtkIMHtml *unused, GtkIMHtmlLink *link)
{
	const char *uri = gtk_imhtml_link_get_url(link);
	g_idle_add(url_clicked_idle_cb, g_strdup(uri));
	return TRUE;
}

static GtkIMHtmlFuncs gtkimhtml_cbs = {
	(GtkIMHtmlGetImageFunc)purple_imgstore_find_by_id,
	(GtkIMHtmlGetImageDataFunc)purple_imgstore_get_data,
	(GtkIMHtmlGetImageSizeFunc)purple_imgstore_get_size,
	(GtkIMHtmlGetImageFilenameFunc)purple_imgstore_get_filename,
	purple_imgstore_ref_by_id,
	purple_imgstore_unref_by_id,
};

void
pidgin_setup_imhtml(GtkWidget *imhtml)
{
	PangoFontDescription *desc = NULL;
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	pidgin_themes_smiley_themeize(imhtml);

	gtk_imhtml_set_funcs(GTK_IMHTML(imhtml), &gtkimhtml_cbs);

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font")) {
		const char *font = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
		desc = pango_font_description_from_string(font);
	} else if (purple_running_gnome()) {
		/* Use the GNOME "document" font, if applicable */
		char *path;

		if ((path = g_find_program_in_path("gconftool-2"))) {
			char *font = NULL;
			char *err = NULL;
			g_free(path);
			if (g_spawn_command_line_sync(
					"gconftool-2 -g /desktop/gnome/interface/document_font_name",
					&font, &err, NULL, NULL)) {
				desc = pango_font_description_from_string(font);
			}
			g_free(err);
			g_free(font);
		}
	}

	if (desc) {
		gtk_widget_modify_font(imhtml, desc);
		pango_font_description_free(desc);
	}
}

static
void pidgin_window_init(GtkWindow *wnd, const char *title, guint border_width, const char *role, gboolean resizable)
{
	if (title)
		gtk_window_set_title(wnd, title);
#ifdef _WIN32
	else
		gtk_window_set_title(wnd, PIDGIN_ALERT_TITLE);
#endif
	gtk_container_set_border_width(GTK_CONTAINER(wnd), border_width);
	if (role)
		gtk_window_set_role(wnd, role);
	gtk_window_set_resizable(wnd, resizable);
}

GtkWidget *
pidgin_create_window(const char *title, guint border_width, const char *role, gboolean resizable)
{
	GtkWindow *wnd = NULL;

	wnd = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	pidgin_window_init(wnd, title, border_width, role, resizable);

	return GTK_WIDGET(wnd);
}

GtkWidget *
pidgin_create_dialog(const char *title, guint border_width, const char *role, gboolean resizable)
{
	GtkWindow *wnd = NULL;

	wnd = GTK_WINDOW(gtk_dialog_new());
	pidgin_window_init(wnd, title, border_width, role, resizable);
	g_object_set(G_OBJECT(wnd), "has-separator", FALSE, NULL);

	return GTK_WIDGET(wnd);
}

GtkWidget *
pidgin_dialog_get_vbox_with_properties(GtkDialog *dialog, gboolean homogeneous, gint spacing)
{
	GtkBox *vbox = GTK_BOX(GTK_DIALOG(dialog)->vbox);
	gtk_box_set_homogeneous(vbox, homogeneous);
	gtk_box_set_spacing(vbox, spacing);
	return GTK_WIDGET(vbox);
}

GtkWidget *pidgin_dialog_get_vbox(GtkDialog *dialog)
{
	return GTK_DIALOG(dialog)->vbox;
}

GtkWidget *pidgin_dialog_get_action_area(GtkDialog *dialog)
{
	return GTK_DIALOG(dialog)->action_area;
}

GtkWidget *pidgin_dialog_add_button(GtkDialog *dialog, const char *label,
		GCallback callback, gpointer callbackdata)
{
	GtkWidget *button = gtk_button_new_from_stock(label);
	GtkWidget *bbox = pidgin_dialog_get_action_area(dialog);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	if (callback)
		g_signal_connect(G_OBJECT(button), "clicked", callback, callbackdata);
	gtk_widget_show(button);
	return button;
}

GtkWidget *
pidgin_create_imhtml(gboolean editable, GtkWidget **imhtml_ret, GtkWidget **toolbar_ret, GtkWidget **sw_ret)
{
	GtkWidget *frame;
	GtkWidget *imhtml;
	GtkWidget *sep;
	GtkWidget *sw;
	GtkWidget *toolbar = NULL;
	GtkWidget *vbox;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	if (editable) {
		toolbar = gtk_imhtmltoolbar_new();
		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
		gtk_widget_show(toolbar);

		sep = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
		g_signal_connect_swapped(G_OBJECT(toolbar), "show", G_CALLBACK(gtk_widget_show), sep);
		g_signal_connect_swapped(G_OBJECT(toolbar), "hide", G_CALLBACK(gtk_widget_hide), sep);
		gtk_widget_show(sep);
	}

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_imhtml_set_editable(GTK_IMHTML(imhtml), editable);
	gtk_imhtml_set_format_functions(GTK_IMHTML(imhtml), GTK_IMHTML_ALL ^ GTK_IMHTML_IMAGE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(imhtml), GTK_WRAP_WORD_CHAR);
#ifdef USE_GTKSPELL
	if (editable && purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck"))
		pidgin_setup_gtkspell(GTK_TEXT_VIEW(imhtml));
#endif
	gtk_widget_show(imhtml);

	if (editable) {
		gtk_imhtmltoolbar_attach(GTK_IMHTMLTOOLBAR(toolbar), imhtml);
		gtk_imhtmltoolbar_associate_smileys(GTK_IMHTMLTOOLBAR(toolbar), "default");
	}
	pidgin_setup_imhtml(imhtml);

	gtk_container_add(GTK_CONTAINER(sw), imhtml);

	if (imhtml_ret != NULL)
		*imhtml_ret = imhtml;

	if (editable && (toolbar_ret != NULL))
		*toolbar_ret = toolbar;

	if (sw_ret != NULL)
		*sw_ret = sw;

	return frame;
}

void
pidgin_set_sensitive_if_input(GtkWidget *entry, GtkWidget *dialog)
{
	const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
									  (*text != '\0'));
}

void
pidgin_toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle)
{
	gboolean sensitivity;

	if (to_toggle == NULL)
		return;

	sensitivity = GTK_WIDGET_IS_SENSITIVE(to_toggle);

	gtk_widget_set_sensitive(to_toggle, !sensitivity);
}

void
pidgin_toggle_sensitive_array(GtkWidget *w, GPtrArray *data)
{
	gboolean sensitivity;
	gpointer element;
	int i;

	for (i=0; i < data->len; i++) {
		element = g_ptr_array_index(data,i);
		if (element == NULL)
			continue;

		sensitivity = GTK_WIDGET_IS_SENSITIVE(element);

		gtk_widget_set_sensitive(element, !sensitivity);
	}
}

void
pidgin_toggle_showhide(GtkWidget *widget, GtkWidget *to_toggle)
{
	if (to_toggle == NULL)
		return;

	if (GTK_WIDGET_VISIBLE(to_toggle))
		gtk_widget_hide(to_toggle);
	else
		gtk_widget_show(to_toggle);
}

GtkWidget *pidgin_separator(GtkWidget *menu)
{
	GtkWidget *menuitem;

	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	return menuitem;
}

GtkWidget *pidgin_new_item(GtkWidget *menu, const char *str)
{
	GtkWidget *menuitem;
	GtkWidget *label;

	menuitem = gtk_menu_item_new();
	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	label = gtk_label_new(str);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_pattern(GTK_LABEL(label), "_");
	gtk_container_add(GTK_CONTAINER(menuitem), label);
	gtk_widget_show(label);
/* FIXME: Go back and fix this
	gtk_widget_add_accelerator(menuitem, "activate", accel, str[0],
				   GDK_MOD1_MASK, GTK_ACCEL_LOCKED);
*/
	pidgin_set_accessible_label (menuitem, label);
	return menuitem;
}

GtkWidget *pidgin_new_check_item(GtkWidget *menu, const char *str,
		GCallback cb, gpointer data, gboolean checked)
{
	GtkWidget *menuitem;
	menuitem = gtk_check_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), checked);

	if (cb)
		g_signal_connect(G_OBJECT(menuitem), "activate", cb, data);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
pidgin_pixbuf_toolbar_button_from_stock(const char *icon)
{
	GtkWidget *button, *image, *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);

	return button;
}

GtkWidget *
pidgin_pixbuf_button_from_stock(const char *text, const char *icon,
							  PidginButtonOrientation style)
{
	GtkWidget *button, *image, *label, *bbox, *ibox, *lbox = NULL;

	button = gtk_button_new();

	if (style == PIDGIN_BUTTON_HORIZONTAL) {
		bbox = gtk_hbox_new(FALSE, 0);
		ibox = gtk_hbox_new(FALSE, 0);
		if (text)
			lbox = gtk_hbox_new(FALSE, 0);
	} else {
		bbox = gtk_vbox_new(FALSE, 0);
		ibox = gtk_vbox_new(FALSE, 0);
		if (text)
			lbox = gtk_vbox_new(FALSE, 0);
	}

	gtk_container_add(GTK_CONTAINER(button), bbox);

	if (icon) {
		gtk_box_pack_start_defaults(GTK_BOX(bbox), ibox);
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_end(GTK_BOX(ibox), image, FALSE, TRUE, 0);
	}

	if (text) {
		gtk_box_pack_start_defaults(GTK_BOX(bbox), lbox);
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(lbox), label, FALSE, TRUE, 0);
		pidgin_set_accessible_label (button, label);
	}

	gtk_widget_show_all(bbox);

	return button;
}


GtkWidget *pidgin_new_item_from_stock(GtkWidget *menu, const char *str, const char *icon, GCallback cb, gpointer data, guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	/*
	GtkWidget *hbox;
	GtkWidget *label;
	*/
	GtkWidget *image;

	if (icon == NULL)
		menuitem = gtk_menu_item_new_with_mnemonic(str);
	else
		menuitem = gtk_image_menu_item_new_with_mnemonic(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (cb)
		g_signal_connect(G_OBJECT(menuitem), "activate", cb, data);

	if (icon != NULL) {
		image = gtk_image_new_from_stock(icon, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	}
/* FIXME: this isn't right
	if (mod) {
		label = gtk_label_new(mod);
		gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}
*/
/*
	if (accel_key) {
		gtk_widget_add_accelerator(menuitem, "activate", accel, accel_key,
					   accel_mods, GTK_ACCEL_LOCKED);
	}
*/

	gtk_widget_show_all(menuitem);

	return menuitem;
}

GtkWidget *
pidgin_make_frame(GtkWidget *parent, const char *title)
{
	GtkWidget *vbox, *label, *hbox;
	char *labeltitle;

	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(parent), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	label = gtk_label_new(NULL);

	labeltitle = g_strdup_printf("<span weight=\"bold\">%s</span>", title);
	gtk_label_set_markup(GTK_LABEL(label), labeltitle);
	g_free(labeltitle);

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	pidgin_set_accessible_label (vbox, label);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	return vbox;
}

static gpointer
aop_option_menu_get_selected(GtkWidget *optmenu, GtkWidget **p_item)
{
	GtkWidget *menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu));
	GtkWidget *item = gtk_menu_get_active(GTK_MENU(menu));
	if (p_item)
		(*p_item) = item;
	return item ? g_object_get_data(G_OBJECT(item), "aop_per_item_data") : NULL;
}

static void
aop_menu_cb(GtkWidget *optmenu, GCallback cb)
{
	GtkWidget *item;
	gpointer per_item_data;

	per_item_data = aop_option_menu_get_selected(optmenu, &item);

	if (cb != NULL) {
		((void (*)(GtkWidget *, gpointer, gpointer))cb)(item, per_item_data, g_object_get_data(G_OBJECT(optmenu), "user_data"));
	}
}

static GtkWidget *
aop_menu_item_new(GtkSizeGroup *sg, GdkPixbuf *pixbuf, const char *lbl, gpointer per_item_data, const char *data)
{
	GtkWidget *item;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *label;

	item = gtk_menu_item_new();
	gtk_widget_show(item);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);

	/* Create the image */
	if (pixbuf == NULL)
		image = gtk_image_new();
	else
		image = gtk_image_new_from_pixbuf(pixbuf);
	gtk_widget_show(image);

	if (sg)
		gtk_size_group_add_widget(sg, image);

	/* Create the label */
	label = gtk_label_new (lbl);
	gtk_widget_show (label);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	gtk_container_add(GTK_CONTAINER(item), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	g_object_set_data(G_OBJECT (item), data, per_item_data);
	g_object_set_data(G_OBJECT (item), "aop_per_item_data", per_item_data);

	pidgin_set_accessible_label(item, label);

	return item;
}

static GdkPixbuf *
pidgin_create_prpl_icon_from_prpl(PurplePlugin *prpl, PidginPrplIconSize size, PurpleAccount *account)
{
	PurplePluginProtocolInfo *prpl_info;
	const char *protoname = NULL;
	char *tmp;
	char *filename = NULL;
	GdkPixbuf *pixbuf;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info->list_icon == NULL)
		return NULL;

	protoname = prpl_info->list_icon(account, NULL);
	if (protoname == NULL)
		return NULL;

	/*
	 * Status icons will be themeable too, and then it will look up
	 * protoname from the theme
	 */
	tmp = g_strconcat(protoname, ".png", NULL);

	filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols",
				    size == PIDGIN_PRPL_ICON_SMALL ? "16" :
				    size == PIDGIN_PRPL_ICON_MEDIUM ? "22" : "48",
				    tmp, NULL);
	g_free(tmp);

	pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	g_free(filename);

	return pixbuf;
}

static GtkWidget *
aop_option_menu_new(AopMenu *aop_menu, GCallback cb, gpointer user_data)
{
	GtkWidget *optmenu;

	optmenu = gtk_option_menu_new();
	gtk_widget_show(optmenu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), aop_menu->menu);

	if (aop_menu->default_item != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), aop_menu->default_item);

	g_object_set_data_full(G_OBJECT(optmenu), "aop_menu", aop_menu, (GDestroyNotify)g_free);
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);

	g_signal_connect(G_OBJECT(optmenu), "changed", G_CALLBACK(aop_menu_cb), cb);

	return optmenu;
}

static void
aop_option_menu_replace_menu(GtkWidget *optmenu, AopMenu *new_aop_menu)
{
	if (gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu)))
		gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), new_aop_menu->menu);

	if (new_aop_menu->default_item != -1)
		gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), new_aop_menu->default_item);

	g_object_set_data_full(G_OBJECT(optmenu), "aop_menu", new_aop_menu, (GDestroyNotify)g_free);
}

static void
aop_option_menu_select_by_data(GtkWidget *optmenu, gpointer data)
{
	guint idx;
	GList *llItr = NULL;

	for (idx = 0, llItr = GTK_MENU_SHELL(gtk_option_menu_get_menu(GTK_OPTION_MENU(optmenu)))->children;
	     llItr != NULL;
	     llItr = llItr->next, idx++) {
		if (data == g_object_get_data(G_OBJECT(llItr->data), "aop_per_item_data")) {
			gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), idx);
			break;
		}
	}
}

static AopMenu *
create_protocols_menu(const char *default_proto_id)
{
	AopMenu *aop_menu = NULL;
	PurplePluginProtocolInfo *prpl_info;
	PurplePlugin *plugin;
	GdkPixbuf *pixbuf = NULL;
	GtkSizeGroup *sg;
	GList *p;
	const char *gtalk_name = NULL;
	int i;

	aop_menu = g_malloc0(sizeof(AopMenu));
	aop_menu->default_item = -1;
	aop_menu->menu = gtk_menu_new();
	gtk_widget_show(aop_menu->menu);
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if (purple_find_prpl("prpl-jabber"))
		gtalk_name = _("Google Talk");

	for (p = purple_plugins_get_protocols(), i = 0;
		 p != NULL;
		 p = p->next, i++) {

		plugin = (PurplePlugin *)p->data;
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

		if (gtalk_name && strcmp(gtalk_name, plugin->info->name) < 0) {
			char *filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols",
			                                  "16", "google-talk.png", NULL);
			GtkWidget *item;

			pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
			g_free(filename);

			gtk_menu_shell_append(GTK_MENU_SHELL(aop_menu->menu),
				item = aop_menu_item_new(sg, pixbuf, gtalk_name, "prpl-jabber", "protocol"));
			g_object_set_data(G_OBJECT(item), "fake", GINT_TO_POINTER(1));

			if (pixbuf)
				g_object_unref(pixbuf);

			gtalk_name = NULL;
			i++;
		}

		pixbuf = pidgin_create_prpl_icon_from_prpl(plugin, PIDGIN_PRPL_ICON_SMALL, NULL);

		gtk_menu_shell_append(GTK_MENU_SHELL(aop_menu->menu),
			aop_menu_item_new(sg, pixbuf, plugin->info->name, plugin->info->id, "protocol"));

		if (pixbuf)
			g_object_unref(pixbuf);

		if (default_proto_id != NULL && !strcmp(plugin->info->id, default_proto_id))
			aop_menu->default_item = i;
	}

	g_object_unref(sg);

	return aop_menu;
}

GtkWidget *
pidgin_protocol_option_menu_new(const char *id, GCallback cb,
								  gpointer user_data)
{
	return aop_option_menu_new(create_protocols_menu(id), cb, user_data);
}

const char *
pidgin_protocol_option_menu_get_selected(GtkWidget *optmenu)
{
	return (const char *)aop_option_menu_get_selected(optmenu, NULL);
}

PurpleAccount *
pidgin_account_option_menu_get_selected(GtkWidget *optmenu)
{
	return (PurpleAccount *)aop_option_menu_get_selected(optmenu, NULL);
}

static AopMenu *
create_account_menu(PurpleAccount *default_account,
					PurpleFilterAccountFunc filter_func, gboolean show_all)
{
	AopMenu *aop_menu = NULL;
	PurpleAccount *account;
	GdkPixbuf *pixbuf = NULL;
	GList *list;
	GList *p;
	GtkSizeGroup *sg;
	int i;
	char buf[256];

	if (show_all)
		list = purple_accounts_get_all();
	else
		list = purple_connections_get_all();

	aop_menu = g_malloc0(sizeof(AopMenu));
	aop_menu->default_item = -1;
	aop_menu->menu = gtk_menu_new();
	gtk_widget_show(aop_menu->menu);
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	for (p = list, i = 0; p != NULL; p = p->next, i++) {
		PurplePlugin *plugin;

		if (show_all)
			account = (PurpleAccount *)p->data;
		else {
			PurpleConnection *gc = (PurpleConnection *)p->data;

			account = purple_connection_get_account(gc);
		}

		if (filter_func && !filter_func(account)) {
			i--;
			continue;
		}

		plugin = purple_find_prpl(purple_account_get_protocol_id(account));

		pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);

		if (pixbuf) {
			if (purple_account_is_disconnected(account) && show_all &&
					purple_connections_get_all())
				gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);
		}

		if (purple_account_get_alias(account)) {
			g_snprintf(buf, sizeof(buf), "%s (%s) (%s)",
					   purple_account_get_username(account),
					   purple_account_get_alias(account),
					   purple_account_get_protocol_name(account));
		} else {
			g_snprintf(buf, sizeof(buf), "%s (%s)",
					   purple_account_get_username(account),
					   purple_account_get_protocol_name(account));
		}

		gtk_menu_shell_append(GTK_MENU_SHELL(aop_menu->menu),
			aop_menu_item_new(sg, pixbuf, buf, account, "account"));

		if (pixbuf)
			g_object_unref(pixbuf);

		if (default_account && account == default_account)
			aop_menu->default_item = i;
	}

	g_object_unref(sg);

	return aop_menu;
}

static void
regenerate_account_menu(GtkWidget *optmenu)
{
	gboolean show_all;
	PurpleAccount *account;
	PurpleFilterAccountFunc filter_func;

	account = (PurpleAccount *)aop_option_menu_get_selected(optmenu, NULL);
	show_all = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(optmenu), "show_all"));
	filter_func = g_object_get_data(G_OBJECT(optmenu), "filter_func");

	aop_option_menu_replace_menu(optmenu, create_account_menu(account, filter_func, show_all));
}

static void
account_menu_sign_on_off_cb(PurpleConnection *gc, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static void
account_menu_added_removed_cb(PurpleAccount *account, GtkWidget *optmenu)
{
	regenerate_account_menu(optmenu);
}

static gboolean
account_menu_destroyed_cb(GtkWidget *optmenu, GdkEvent *event,
						  void *user_data)
{
	purple_signals_disconnect_by_handle(optmenu);

	return FALSE;
}

void
pidgin_account_option_menu_set_selected(GtkWidget *optmenu, PurpleAccount *account)
{
	aop_option_menu_select_by_data(optmenu, account);
}

GtkWidget *
pidgin_account_option_menu_new(PurpleAccount *default_account,
								 gboolean show_all, GCallback cb,
								 PurpleFilterAccountFunc filter_func,
								 gpointer user_data)
{
	GtkWidget *optmenu;

	/* Create the option menu */
	optmenu = aop_option_menu_new(create_account_menu(default_account, filter_func, show_all), cb, user_data);

	g_signal_connect(G_OBJECT(optmenu), "destroy",
					 G_CALLBACK(account_menu_destroyed_cb), NULL);

	/* Register the purple sign on/off event callbacks. */
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						optmenu, PURPLE_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	purple_signal_connect(purple_connections_get_handle(), "signed-off",
						optmenu, PURPLE_CALLBACK(account_menu_sign_on_off_cb),
						optmenu);
	purple_signal_connect(purple_accounts_get_handle(), "account-added",
						optmenu, PURPLE_CALLBACK(account_menu_added_removed_cb),
						optmenu);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
						optmenu, PURPLE_CALLBACK(account_menu_added_removed_cb),
						optmenu);

	/* Set some data. */
	g_object_set_data(G_OBJECT(optmenu), "user_data", user_data);
	g_object_set_data(G_OBJECT(optmenu), "show_all", GINT_TO_POINTER(show_all));
	g_object_set_data(G_OBJECT(optmenu), "filter_func", filter_func);

	return optmenu;
}

gboolean
pidgin_check_if_dir(const char *path, GtkFileSelection *filesel)
{
	char *dirname = NULL;

	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != G_DIR_SEPARATOR) {
			dirname = g_strconcat(path, G_DIR_SEPARATOR_S, NULL);
		}
		gtk_file_selection_set_filename(filesel, (dirname != NULL) ? dirname : path);
		g_free(dirname);
		return TRUE;
	}

	return FALSE;
}

void
pidgin_setup_gtkspell(GtkTextView *textview)
{
#ifdef USE_GTKSPELL
	GError *error = NULL;
	char *locale = NULL;

	g_return_if_fail(textview != NULL);
	g_return_if_fail(GTK_IS_TEXT_VIEW(textview));

	if (gtkspell_new_attach(textview, locale, &error) == NULL && error)
	{
		purple_debug_warning("gtkspell", "Failed to setup GtkSpell: %s\n",
						   error->message);
		g_error_free(error);
	}
#endif /* USE_GTKSPELL */
}

void
pidgin_save_accels_cb(GtkAccelGroup *accel_group, guint arg1,
                         GdkModifierType arg2, GClosure *arg3,
                         gpointer data)
{
	purple_debug(PURPLE_DEBUG_MISC, "accels",
	           "accel changed, scheduling save.\n");

	if (!accels_save_timer)
		accels_save_timer = purple_timeout_add_seconds(5, pidgin_save_accels,
		                                  NULL);
}

gboolean
pidgin_save_accels(gpointer data)
{
	char *filename = NULL;

	filename = g_build_filename(purple_user_dir(), G_DIR_SEPARATOR_S,
	                            "accels", NULL);
	purple_debug(PURPLE_DEBUG_MISC, "accels", "saving accels to %s\n", filename);
	gtk_accel_map_save(filename);
	g_free(filename);

	accels_save_timer = 0;
	return FALSE;
}

void
pidgin_load_accels()
{
	char *filename = NULL;

	filename = g_build_filename(purple_user_dir(), G_DIR_SEPARATOR_S,
	                            "accels", NULL);
	gtk_accel_map_load(filename);
	g_free(filename);
}

static void
show_retrieveing_info(PurpleConnection *conn, const char *name)
{
	PurpleNotifyUserInfo *info = purple_notify_user_info_new();
	purple_notify_user_info_add_pair(info, _("Information"), _("Retrieving..."));
	purple_notify_userinfo(conn, name, info, NULL, NULL);
	purple_notify_user_info_destroy(info);
}

void pidgin_retrieve_user_info(PurpleConnection *conn, const char *name)
{
	show_retrieveing_info(conn, name);
	serv_get_info(conn, name);
}

void pidgin_retrieve_user_info_in_chat(PurpleConnection *conn, const char *name, int chat)
{
	char *who = NULL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (chat < 0) {
		pidgin_retrieve_user_info(conn, name);
		return;
	}

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(conn->prpl);
	if (prpl_info != NULL && prpl_info->get_cb_real_name)
		who = prpl_info->get_cb_real_name(conn, chat, name);
	if (prpl_info == NULL || prpl_info->get_cb_info == NULL) {
		pidgin_retrieve_user_info(conn, who ? who : name);
		g_free(who);
		return;
	}

	show_retrieveing_info(conn, who ? who : name);
	prpl_info->get_cb_info(conn, chat, name);
	g_free(who);
}

gboolean
pidgin_parse_x_im_contact(const char *msg, gboolean all_accounts,
							PurpleAccount **ret_account, char **ret_protocol,
							char **ret_username, char **ret_alias)
{
	char *protocol = NULL;
	char *username = NULL;
	char *alias    = NULL;
	char *str;
	char *c, *s;
	gboolean valid;

	g_return_val_if_fail(msg          != NULL, FALSE);
	g_return_val_if_fail(ret_protocol != NULL, FALSE);
	g_return_val_if_fail(ret_username != NULL, FALSE);

	s = str = g_strdup(msg);

	while (*s != '\r' && *s != '\n' && *s != '\0')
	{
		char *key, *value;

		key = s;

		/* Grab the key */
		while (*s != '\r' && *s != '\n' && *s != '\0' && *s != ' ')
			s++;

		if (*s == '\r') s++;

		if (*s == '\n')
		{
			s++;
			continue;
		}

		if (*s != '\0') *s++ = '\0';

		/* Clear past any whitespace */
		while (*s != '\0' && *s == ' ')
			s++;

		/* Now let's grab until the end of the line. */
		value = s;

		while (*s != '\r' && *s != '\n' && *s != '\0')
			s++;

		if (*s == '\r') *s++ = '\0';
		if (*s == '\n') *s++ = '\0';

		if ((c = strchr(key, ':')) != NULL)
		{
			if (!g_ascii_strcasecmp(key, "X-IM-Username:"))
				username = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Protocol:"))
				protocol = g_strdup(value);
			else if (!g_ascii_strcasecmp(key, "X-IM-Alias:"))
				alias = g_strdup(value);
		}
	}

	if (username != NULL && protocol != NULL)
	{
		valid = TRUE;

		*ret_username = username;
		*ret_protocol = protocol;

		if (ret_alias != NULL)
			*ret_alias = alias;

		/* Check for a compatible account. */
		if (ret_account != NULL)
		{
			GList *list;
			PurpleAccount *account = NULL;
			GList *l;
			const char *protoname;

			if (all_accounts)
				list = purple_accounts_get_all();
			else
				list = purple_connections_get_all();

			for (l = list; l != NULL; l = l->next)
			{
				PurpleConnection *gc;
				PurplePluginProtocolInfo *prpl_info = NULL;
				PurplePlugin *plugin;

				if (all_accounts)
				{
					account = (PurpleAccount *)l->data;

					plugin = purple_plugins_find_with_id(
						purple_account_get_protocol_id(account));

					if (plugin == NULL)
					{
						account = NULL;

						continue;
					}

					prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
				}
				else
				{
					gc = (PurpleConnection *)l->data;
					account = purple_connection_get_account(gc);

					prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
				}

				protoname = prpl_info->list_icon(account, NULL);

				if (!strcmp(protoname, protocol))
					break;

				account = NULL;
			}

			/* Special case for AIM and ICQ */
			if (account == NULL && (!strcmp(protocol, "aim") ||
									!strcmp(protocol, "icq")))
			{
				for (l = list; l != NULL; l = l->next)
				{
					PurpleConnection *gc;
					PurplePluginProtocolInfo *prpl_info = NULL;
					PurplePlugin *plugin;

					if (all_accounts)
					{
						account = (PurpleAccount *)l->data;

						plugin = purple_plugins_find_with_id(
							purple_account_get_protocol_id(account));

						if (plugin == NULL)
						{
							account = NULL;

							continue;
						}

						prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
					}
					else
					{
						gc = (PurpleConnection *)l->data;
						account = purple_connection_get_account(gc);

						prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
					}

					protoname = prpl_info->list_icon(account, NULL);

					if (!strcmp(protoname, "aim") || !strcmp(protoname, "icq"))
						break;

					account = NULL;
				}
			}

			*ret_account = account;
		}
	}
	else
	{
		valid = FALSE;

		g_free(username);
		g_free(protocol);
		g_free(alias);
	}

	g_free(str);

	return valid;
}

void
pidgin_set_accessible_label (GtkWidget *w, GtkWidget *l)
{
	AtkObject *acc;
	const gchar *label_text;
	const gchar *existing_name;

	acc = gtk_widget_get_accessible (w);

	/* If this object has no name, set it's name with the label text */
	existing_name = atk_object_get_name (acc);
	if (!existing_name) {
		label_text = gtk_label_get_text (GTK_LABEL(l));
		if (label_text)
			atk_object_set_name (acc, label_text);
	}

	pidgin_set_accessible_relations(w, l);
}

void
pidgin_set_accessible_relations (GtkWidget *w, GtkWidget *l)
{
	AtkObject *acc, *label;
	AtkObject *rel_obj[1];
	AtkRelationSet *set;
	AtkRelation *relation;

	acc = gtk_widget_get_accessible (w);
	label = gtk_widget_get_accessible (l);

	/* Make sure mnemonics work */
	gtk_label_set_mnemonic_widget(GTK_LABEL(l), w);

	/* Create the labeled-by relation */
	set = atk_object_ref_relation_set (acc);
	rel_obj[0] = label;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
	g_object_unref(set);

	/* Create the label-for relation */
	set = atk_object_ref_relation_set (label);
	rel_obj[0] = acc;
	relation = atk_relation_new (rel_obj, 1, ATK_RELATION_LABEL_FOR);
	atk_relation_set_add (set, relation);
	g_object_unref (relation);
	g_object_unref(set);
}

void
pidgin_menu_position_func_helper(GtkMenu *menu,
							gint *x,
							gint *y,
							gboolean *push_in,
							gpointer data)
{
#if GTK_CHECK_VERSION(2,2,0)
	GtkWidget *widget;
	GtkRequisition requisition;
	GdkScreen *screen;
	GdkRectangle monitor;
	gint monitor_num;
	gint space_left, space_right, space_above, space_below;
	gint needed_width;
	gint needed_height;
	gint xthickness;
	gint ythickness;
	gboolean rtl;

	g_return_if_fail(GTK_IS_MENU(menu));

	widget     = GTK_WIDGET(menu);
	screen     = gtk_widget_get_screen(widget);
	xthickness = widget->style->xthickness;
	ythickness = widget->style->ythickness;
	rtl        = (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL);

	/*
	 * We need the requisition to figure out the right place to
	 * popup the menu. In fact, we always need to ask here, since
	 * if a size_request was queued while we weren't popped up,
	 * the requisition won't have been recomputed yet.
	 */
	gtk_widget_size_request (widget, &requisition);

	monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);

	push_in = FALSE;

	/*
	 * The placement of popup menus horizontally works like this (with
	 * RTL in parentheses)
	 *
	 * - If there is enough room to the right (left) of the mouse cursor,
	 *   position the menu there.
	 *
	 * - Otherwise, if if there is enough room to the left (right) of the
	 *   mouse cursor, position the menu there.
	 *
	 * - Otherwise if the menu is smaller than the monitor, position it
	 *   on the side of the mouse cursor that has the most space available
	 *
	 * - Otherwise (if there is simply not enough room for the menu on the
	 *   monitor), position it as far left (right) as possible.
	 *
	 * Positioning in the vertical direction is similar: first try below
	 * mouse cursor, then above.
	 */
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	space_left = *x - monitor.x;
	space_right = monitor.x + monitor.width - *x - 1;
	space_above = *y - monitor.y;
	space_below = monitor.y + monitor.height - *y - 1;

	/* position horizontally */

	/* the amount of space we need to position the menu. Note the
	 * menu is offset "xthickness" pixels
	 */
	needed_width = requisition.width - xthickness;

	if (needed_width <= space_left ||
	    needed_width <= space_right)
	{
		if ((rtl  && needed_width <= space_left) ||
		    (!rtl && needed_width >  space_right))
		{
			/* position left */
			*x = *x + xthickness - requisition.width + 1;
		}
		else
		{
			/* position right */
			*x = *x - xthickness;
		}

		/* x is clamped on-screen further down */
	}
	else if (requisition.width <= monitor.width)
	{
		/* the menu is too big to fit on either side of the mouse
		 * cursor, but smaller than the monitor. Position it on
		 * the side that has the most space
		 */
		if (space_left > space_right)
		{
			/* left justify */
			*x = monitor.x;
		}
		else
		{
			/* right justify */
			*x = monitor.x + monitor.width - requisition.width;
		}
	}
	else /* menu is simply too big for the monitor */
	{
		if (rtl)
		{
			/* right justify */
			*x = monitor.x + monitor.width - requisition.width;
		}
		else
		{
			/* left justify */
			*x = monitor.x;
		}
	}

	/* Position vertically. The algorithm is the same as above, but
	 * simpler because we don't have to take RTL into account.
	 */
	needed_height = requisition.height - ythickness;

	if (needed_height <= space_above ||
	    needed_height <= space_below)
	{
		if (needed_height <= space_below)
			*y = *y - ythickness;
		else
			*y = *y + ythickness - requisition.height + 1;

		*y = CLAMP (*y, monitor.y,
			   monitor.y + monitor.height - requisition.height);
	}
	else if (needed_height > space_below && needed_height > space_above)
	{
		if (space_below >= space_above)
			*y = monitor.y + monitor.height - requisition.height;
		else
			*y = monitor.y;
	}
	else
	{
		*y = monitor.y;
	}
#endif
}


void
pidgin_treeview_popup_menu_position_func(GtkMenu *menu,
										   gint *x,
										   gint *y,
										   gboolean *push_in,
										   gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkTreeView *tv = GTK_TREE_VIEW(data);
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	GdkRectangle rect;
	gint ythickness = GTK_WIDGET(menu)->style->ythickness;

	gdk_window_get_origin (widget->window, x, y);
	gtk_tree_view_get_cursor (tv, &path, &col);
	gtk_tree_view_get_cell_area (tv, path, col, &rect);

	*x += rect.x+rect.width;
	*y += rect.y+rect.height+ythickness;
	pidgin_menu_position_func_helper(menu, x, y, push_in, data);
}

enum {
	DND_FILE_TRANSFER,
	DND_IM_IMAGE,
	DND_BUDDY_ICON
};

typedef struct {
	char *filename;
	PurpleAccount *account;
	char *who;
} _DndData;

static void dnd_image_ok_callback(_DndData *data, int choice)
{
	gchar *filedata;
	size_t size;
	struct stat st;
	GError *err = NULL;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	GtkTextIter iter;
	int id;
	PurpleBuddy *buddy;
	PurpleContact *contact;
	switch (choice) {
	case DND_BUDDY_ICON:
		if (g_stat(data->filename, &st)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"),
						data->filename, g_strerror(errno));
			purple_notify_error(NULL, NULL,
					  _("Failed to load image"),
					  str);
			g_free(str);

			break;
		}

		buddy = purple_find_buddy(data->account, data->who);
		if (!buddy) {
			purple_debug_info("custom-icon", "You can only set custom icons for people on your buddylist.\n");
			break;
		}
		contact = purple_buddy_get_contact(buddy);
		purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, data->filename);
		break;
	case DND_FILE_TRANSFER:
		serv_send_file(purple_account_get_connection(data->account), data->who, data->filename);
		break;
	case DND_IM_IMAGE:
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, data->account, data->who);
		gtkconv = PIDGIN_CONVERSATION(conv);

		if (!g_file_get_contents(data->filename, &filedata, &size,
					 &err)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"), data->filename, err->message);
			purple_notify_error(NULL, NULL,
					  _("Failed to load image"),
					  str);

			g_error_free(err);
			g_free(str);

			break;
		}
		id = purple_imgstore_add_with_id(filedata, size, data->filename);

		gtk_text_buffer_get_iter_at_mark(GTK_IMHTML(gtkconv->entry)->text_buffer, &iter,
						 gtk_text_buffer_get_insert(GTK_IMHTML(gtkconv->entry)->text_buffer));
		gtk_imhtml_insert_image_at_iter(GTK_IMHTML(gtkconv->entry), id, &iter);
		purple_imgstore_unref_by_id(id);

		break;
	}
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void dnd_image_cancel_callback(_DndData *data, int choice)
{
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void dnd_set_icon_ok_cb(_DndData *data)
{
	dnd_image_ok_callback(data, DND_BUDDY_ICON);
}

static void dnd_set_icon_cancel_cb(_DndData *data)
{
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

void
pidgin_dnd_file_manage(GtkSelectionData *sd, PurpleAccount *account, const char *who)
{
	GdkPixbuf *pb;
	GList *files = purple_uri_list_extract_filenames((const gchar *)sd->data);
	PurpleConnection *gc = purple_account_get_connection(account);
	PurplePluginProtocolInfo *prpl_info = NULL;
#ifndef _WIN32
	PurpleDesktopItem *item;
#endif
	gchar *filename = NULL;
	gchar *basename = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(who != NULL);

	for ( ; files; files = g_list_delete_link(files, files)) {
		g_free(filename);
		g_free(basename);

		filename = files->data;
		basename = g_path_get_basename(filename);

		/* XXX - Make ft API support creating a transfer with more than one file */
		if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
			continue;
		}

		/* XXX - make ft api suupport sending a directory */
		/* Are we dealing with a directory? */
		if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
			char *str, *str2;

			str = g_strdup_printf(_("Cannot send folder %s."), basename);
			str2 = g_strdup_printf(_("%s cannot transfer a folder. You will need to send the files within individually."), PIDGIN_NAME);

			purple_notify_error(NULL, NULL,
					  str, str2);

			g_free(str);
			g_free(str2);
			continue;
		}

		/* Are we dealing with an image? */
		pb = gdk_pixbuf_new_from_file(filename, NULL);
		if (pb) {
			_DndData *data = g_malloc(sizeof(_DndData));
			gboolean ft = FALSE, im = FALSE;

			data->who = g_strdup(who);
			data->filename = g_strdup(filename);
			data->account = account;

			if (gc)
				prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

			if (prpl_info && prpl_info->options & OPT_PROTO_IM_IMAGE)
				im = TRUE;

			if (prpl_info && prpl_info->can_receive_file)
				ft = prpl_info->can_receive_file(gc, who);
			else if (prpl_info && prpl_info->send_file)
				ft = TRUE;

			if (im && ft)
				purple_request_choice(NULL, NULL,
						    _("You have dragged an image"),
						    _("You can send this image as a file transfer, "
						      "embed it into this message, or use it as the buddy icon for this user."),
						    DND_FILE_TRANSFER, "OK", (GCallback)dnd_image_ok_callback,
						    "Cancel", (GCallback)dnd_image_cancel_callback,
							account, who, NULL,
							data,
							_("Set as buddy icon"), DND_BUDDY_ICON,
						    _("Send image file"), DND_FILE_TRANSFER,
						    _("Insert in message"), DND_IM_IMAGE,
							NULL);
			else if (!(im || ft))
				purple_request_yes_no(NULL, NULL, _("You have dragged an image"),
							_("Would you like to set it as the buddy icon for this user?"),
							PURPLE_DEFAULT_ACTION_NONE,
							account, who, NULL,
							data, (GCallback)dnd_set_icon_ok_cb, (GCallback)dnd_set_icon_cancel_cb);
			else
				purple_request_choice(NULL, NULL,
						    _("You have dragged an image"),
						    (ft ? _("You can send this image as a file transfer, or use it as the buddy icon for this user.") :
						    _("You can insert this image into this message, or use it as the buddy icon for this user")),
						    (ft ? DND_FILE_TRANSFER : DND_IM_IMAGE),
							"OK", (GCallback)dnd_image_ok_callback,
						    "Cancel", (GCallback)dnd_image_cancel_callback,
							account, who, NULL,
							data,
						    _("Set as buddy icon"), DND_BUDDY_ICON,
						    (ft ? _("Send image file") : _("Insert in message")), (ft ? DND_FILE_TRANSFER : DND_IM_IMAGE),
							NULL);
			g_object_unref(G_OBJECT(pb));

			g_free(basename);
			while (files) {
				g_free(files->data);
				files = g_list_delete_link(files, files);
			}
			return;
		}

#ifndef _WIN32
		/* Are we trying to send a .desktop file? */
		else if (purple_str_has_suffix(basename, ".desktop") && (item = purple_desktop_item_new_from_file(filename))) {
			PurpleDesktopItemType dtype;
			char key[64];
			const char *itemname = NULL;

#if GTK_CHECK_VERSION(2,6,0)
			const char * const *langs;
			int i;
			langs = g_get_language_names();
			for (i = 0; langs[i]; i++) {
				g_snprintf(key, sizeof(key), "Name[%s]", langs[i]);
				itemname = purple_desktop_item_get_string(item, key);
				break;
			}
#else
			const char *lang = g_getenv("LANG");
			char *dot;
			dot = strchr(lang, '.');
			if (dot)
				*dot = '\0';
			g_snprintf(key, sizeof(key), "Name[%s]", lang);
			itemname = purple_desktop_item_get_string(item, key);
#endif
			if (!itemname)
				itemname = purple_desktop_item_get_string(item, "Name");

			dtype = purple_desktop_item_get_entry_type(item);
			switch (dtype) {
				PurpleConversation *conv;
				PidginConversation *gtkconv;

			case PURPLE_DESKTOP_ITEM_TYPE_LINK:
				conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, who);
				gtkconv =  PIDGIN_CONVERSATION(conv);
				gtk_imhtml_insert_link(GTK_IMHTML(gtkconv->entry),
						       gtk_text_buffer_get_insert(GTK_IMHTML(gtkconv->entry)->text_buffer),
						       purple_desktop_item_get_string(item, "URL"), itemname);
				break;
			default:
				/* I don't know if we really want to do anything here.  Most of the desktop item types are crap like
				 * "MIME Type" (I have no clue how that would be a desktop item) and "Comment"... nothing we can really
				 * send.  The only logical one is "Application," but do we really want to send a binary and nothing else?
				 * Probably not.  I'll just give an error and return. */
				/* The original patch sent the icon used by the launcher.  That's probably wrong */
				purple_notify_error(NULL, NULL, _("Cannot send launcher"),
				                    _("You dragged a desktop launcher. Most "
				                      "likely you wanted to send the target "
				                      "of this launcher instead of this "
				                      "launcher itself."));
				break;
			}
			purple_desktop_item_unref(item);
			g_free(basename);
			while (files) {
				g_free(files->data);
				files = g_list_delete_link(files, files);
			}
			return;
		}
#endif /* _WIN32 */

		/* Everything is fine, let's send */
		serv_send_file(gc, who, filename);
	}

	g_free(filename);
	g_free(basename);
}

void pidgin_buddy_icon_get_scale_size(GdkPixbuf *buf, PurpleBuddyIconSpec *spec, PurpleIconScaleRules rules, int *width, int *height)
{
	*width = gdk_pixbuf_get_width(buf);
	*height = gdk_pixbuf_get_height(buf);

	if ((spec == NULL) || !(spec->scale_rules & rules))
		return;

	purple_buddy_icon_get_scale_size(spec, width, height);

	/* and now for some arbitrary sanity checks */
	if(*width > 100)
		*width = 100;
	if(*height > 100)
		*height = 100;
}

GdkPixbuf * pidgin_create_status_icon(PurpleStatusPrimitive prim, GtkWidget *w, const char *size)
{
	GtkIconSize icon_size = gtk_icon_size_from_name(size);
	GdkPixbuf *pixbuf = NULL;
	const char *stock = pidgin_stock_id_from_status_primitive(prim);

	pixbuf = gtk_widget_render_icon (w, stock ? stock : PIDGIN_STOCK_STATUS_AVAILABLE,
			icon_size, "GtkWidget");
	return pixbuf;
}

static const char *
stock_id_from_status_primitive_idle(PurpleStatusPrimitive prim, gboolean idle)
{
	const char *stock = NULL;
	switch (prim) {
		case PURPLE_STATUS_UNSET:
			stock = NULL;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			stock = idle ? PIDGIN_STOCK_STATUS_BUSY_I : PIDGIN_STOCK_STATUS_BUSY;
			break;
		case PURPLE_STATUS_AWAY:
			stock = idle ? PIDGIN_STOCK_STATUS_AWAY_I : PIDGIN_STOCK_STATUS_AWAY;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			stock = idle ? PIDGIN_STOCK_STATUS_XA_I : PIDGIN_STOCK_STATUS_XA;
			break;
		case PURPLE_STATUS_INVISIBLE:
			stock = PIDGIN_STOCK_STATUS_INVISIBLE;
			break;
		case PURPLE_STATUS_OFFLINE:
			stock = idle ? PIDGIN_STOCK_STATUS_OFFLINE_I : PIDGIN_STOCK_STATUS_OFFLINE;
			break;
		default:
			stock = idle ? PIDGIN_STOCK_STATUS_AVAILABLE_I : PIDGIN_STOCK_STATUS_AVAILABLE;
			break;
	}
	return stock;
}

const char *
pidgin_stock_id_from_status_primitive(PurpleStatusPrimitive prim)
{
	return stock_id_from_status_primitive_idle(prim, FALSE);
}

const char *
pidgin_stock_id_from_presence(PurplePresence *presence)
{
	PurpleStatus *status;
	PurpleStatusType *type;
	PurpleStatusPrimitive prim;
	gboolean idle;

	g_return_val_if_fail(presence, NULL);

	status = purple_presence_get_active_status(presence);
	type = purple_status_get_type(status);
	prim = purple_status_type_get_primitive(type);

	idle = purple_presence_is_idle(presence);

	return stock_id_from_status_primitive_idle(prim, idle);
}

GdkPixbuf *
pidgin_create_prpl_icon(PurpleAccount *account, PidginPrplIconSize size)
{
	PurplePlugin *prpl;

	g_return_val_if_fail(account != NULL, NULL);

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));
	if (prpl == NULL)
		return NULL;
	return pidgin_create_prpl_icon_from_prpl(prpl, size, account);
}

static void
menu_action_cb(GtkMenuItem *item, gpointer object)
{
	gpointer data;
	void (*callback)(gpointer, gpointer);

	callback = g_object_get_data(G_OBJECT(item), "purplecallback");
	data = g_object_get_data(G_OBJECT(item), "purplecallbackdata");

	if (callback)
		callback(object, data);
}

GtkWidget *
pidgin_append_menu_action(GtkWidget *menu, PurpleMenuAction *act,
                            gpointer object)
{
	GtkWidget *menuitem;

	if (act == NULL) {
		return pidgin_separator(menu);
	}

	if (act->children == NULL) {
		menuitem = gtk_menu_item_new_with_mnemonic(act->label);

		if (act->callback != NULL) {
			g_object_set_data(G_OBJECT(menuitem),
							  "purplecallback",
							  act->callback);
			g_object_set_data(G_OBJECT(menuitem),
							  "purplecallbackdata",
							  act->data);
			g_signal_connect(G_OBJECT(menuitem), "activate",
							 G_CALLBACK(menu_action_cb),
							 object);
		} else {
			gtk_widget_set_sensitive(menuitem, FALSE);
		}

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	} else {
		GList *l = NULL;
		GtkWidget *submenu = NULL;
		GtkAccelGroup *group;

		menuitem = gtk_menu_item_new_with_mnemonic(act->label);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

		group = gtk_menu_get_accel_group(GTK_MENU(menu));
		if (group) {
			char *path = g_strdup_printf("%s/%s", GTK_MENU_ITEM(menuitem)->accel_path, act->label);
			gtk_menu_set_accel_path(GTK_MENU(submenu), path);
			g_free(path);
			gtk_menu_set_accel_group(GTK_MENU(submenu), group);
		}

		for (l = act->children; l; l = l->next) {
			PurpleMenuAction *act = (PurpleMenuAction *)l->data;

			pidgin_append_menu_action(submenu, act, object);
		}
		g_list_free(act->children);
		act->children = NULL;
	}
	purple_menu_action_free(act);
	return menuitem;
}

#if GTK_CHECK_VERSION(2,3,0)
# define NEW_STYLE_COMPLETION
#endif

typedef struct
{
	GtkWidget *entry;
	GtkWidget *accountopt;

	PidginFilterBuddyCompletionEntryFunc filter_func;
	gpointer filter_func_user_data;

#ifdef NEW_STYLE_COMPLETION
	GtkListStore *store;
#else
	GCompletion *completion;
	gboolean completion_started;
	GList *log_items;
#endif /* NEW_STYLE_COMPLETION */
} PidginCompletionData;

#ifndef NEW_STYLE_COMPLETION
static gboolean
completion_entry_event(GtkEditable *entry, GdkEventKey *event,
					   PidginCompletionData *data)
{
	int pos, end_pos;

	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Tab)
	{
		gtk_editable_get_selection_bounds(entry, &pos, &end_pos);

		if (data->completion_started &&
			pos != end_pos && pos > 1 &&
			end_pos == strlen(gtk_entry_get_text(GTK_ENTRY(entry))))
		{
			gtk_editable_select_region(entry, 0, 0);
			gtk_editable_set_position(entry, -1);

			return TRUE;
		}
	}
	else if (event->type == GDK_KEY_PRESS && event->length > 0)
	{
		char *prefix, *nprefix;

		gtk_editable_get_selection_bounds(entry, &pos, &end_pos);

		if (data->completion_started &&
			pos != end_pos && pos > 1 &&
			end_pos == strlen(gtk_entry_get_text(GTK_ENTRY(entry))))
		{
			char *temp;

			temp = gtk_editable_get_chars(entry, 0, pos);
			prefix = g_strconcat(temp, event->string, NULL);
			g_free(temp);
		}
		else if (pos == end_pos && pos > 1 &&
				 end_pos == strlen(gtk_entry_get_text(GTK_ENTRY(entry))))
		{
			prefix = g_strconcat(gtk_entry_get_text(GTK_ENTRY(entry)),
								 event->string, NULL);
		}
		else
			return FALSE;

		pos = strlen(prefix);
		nprefix = NULL;

		g_completion_complete(data->completion, prefix, &nprefix);

		if (nprefix != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(entry), nprefix);
			gtk_editable_set_position(entry, pos);
			gtk_editable_select_region(entry, pos, -1);

			data->completion_started = TRUE;

			g_free(nprefix);
			g_free(prefix);

			return TRUE;
		}

		g_free(prefix);
	}

	return FALSE;
}

static void
destroy_completion_data(GtkWidget *w, PidginCompletionData *data)
{
	g_list_foreach(data->completion->items, (GFunc)g_free, NULL);
	g_completion_free(data->completion);

	g_free(data);
}
#endif /* !NEW_STYLE_COMPLETION */

#ifdef NEW_STYLE_COMPLETION
static gboolean buddyname_completion_match_func(GtkEntryCompletion *completion,
		const gchar *key, GtkTreeIter *iter, gpointer user_data)
{
	GtkTreeModel *model;
	GValue val1;
	GValue val2;
	const char *tmp;

	model = gtk_entry_completion_get_model (completion);

	val1.g_type = 0;
	gtk_tree_model_get_value(model, iter, 2, &val1);
	tmp = g_value_get_string(&val1);
	if (tmp != NULL && purple_str_has_prefix(tmp, key))
	{
		g_value_unset(&val1);
		return TRUE;
	}
	g_value_unset(&val1);

	val2.g_type = 0;
	gtk_tree_model_get_value(model, iter, 3, &val2);
	tmp = g_value_get_string(&val2);
	if (tmp != NULL && purple_str_has_prefix(tmp, key))
	{
		g_value_unset(&val2);
		return TRUE;
	}
	g_value_unset(&val2);

	return FALSE;
}

static gboolean buddyname_completion_match_selected_cb(GtkEntryCompletion *completion,
		GtkTreeModel *model, GtkTreeIter *iter, PidginCompletionData *data)
{
	GValue val;
	GtkWidget *optmenu = data->accountopt;
	PurpleAccount *account;

	val.g_type = 0;
	gtk_tree_model_get_value(model, iter, 1, &val);
	gtk_entry_set_text(GTK_ENTRY(data->entry), g_value_get_string(&val));
	g_value_unset(&val);

	gtk_tree_model_get_value(model, iter, 4, &val);
	account = g_value_get_pointer(&val);
	g_value_unset(&val);

	if (account == NULL)
		return TRUE;

	if (optmenu != NULL)
		aop_option_menu_select_by_data(optmenu, account);

	return TRUE;
}

static void
add_buddyname_autocomplete_entry(GtkListStore *store, const char *buddy_alias, const char *contact_alias,
								  const PurpleAccount *account, const char *buddyname)
{
	GtkTreeIter iter;
	gboolean completion_added = FALSE;
	gchar *normalized_buddyname;
	gchar *tmp;

	tmp = g_utf8_normalize(buddyname, -1, G_NORMALIZE_DEFAULT);
	normalized_buddyname = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	/* There's no sense listing things like: 'xxx "xxx"'
	   when the name and buddy alias match. */
	if (buddy_alias && strcmp(buddy_alias, buddyname)) {
		char *completion_entry = g_strdup_printf("%s \"%s\"", buddyname, buddy_alias);
		char *tmp2 = g_utf8_normalize(buddy_alias, -1, G_NORMALIZE_DEFAULT);

		tmp = g_utf8_casefold(tmp2, -1);
		g_free(tmp2);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				0, completion_entry,
				1, buddyname,
				2, normalized_buddyname,
				3, tmp,
				4, account,
				-1);
		g_free(completion_entry);
		g_free(tmp);
		completion_added = TRUE;
	}

	/* There's no sense listing things like: 'xxx "xxx"'
	   when the name and contact alias match. */
	if (contact_alias && strcmp(contact_alias, buddyname)) {
		/* We don't want duplicates when the contact and buddy alias match. */
		if (!buddy_alias || strcmp(contact_alias, buddy_alias)) {
			char *completion_entry = g_strdup_printf("%s \"%s\"",
							buddyname, contact_alias);
			char *tmp2 = g_utf8_normalize(contact_alias, -1, G_NORMALIZE_DEFAULT);

			tmp = g_utf8_casefold(tmp2, -1);
			g_free(tmp2);

			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
					0, completion_entry,
					1, buddyname,
					2, normalized_buddyname,
					3, tmp,
					4, account,
					-1);
			g_free(completion_entry);
			g_free(tmp);
			completion_added = TRUE;
		}
	}

	if (completion_added == FALSE) {
		/* Add the buddy's name. */
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				0, buddyname,
				1, buddyname,
				2, normalized_buddyname,
				3, NULL,
				4, account,
				-1);
	}

	g_free(normalized_buddyname);
}
#endif /* NEW_STYLE_COMPLETION */

static void get_log_set_name(PurpleLogSet *set, gpointer value, PidginCompletionData *data)
{
	PidginFilterBuddyCompletionEntryFunc filter_func = data->filter_func;
	gpointer user_data = data->filter_func_user_data;

	/* 1. Don't show buddies because we will have gotten them already.
	 * 2. The boxes that use this autocomplete code handle only IMs. */
	if (!set->buddy && set->type == PURPLE_LOG_IM) {
		PidginBuddyCompletionEntry entry;
		entry.is_buddy = FALSE;
		entry.entry.logged_buddy = set;

		if (filter_func(&entry, user_data)) {
#ifdef NEW_STYLE_COMPLETION
			add_buddyname_autocomplete_entry(data->store,
												NULL, NULL, set->account, set->name);
#else
			/* Steal the name for the GCompletion. */
			data->log_items = g_list_append(data->log_items, set->name);
			set->name = set->normalized_name = NULL;
#endif /* NEW_STYLE_COMPLETION */
		}
	}
}

static void
add_completion_list(PidginCompletionData *data)
{
	PurpleBlistNode *gnode, *cnode, *bnode;
	PidginFilterBuddyCompletionEntryFunc filter_func = data->filter_func;
	gpointer user_data = data->filter_func_user_data;
	GHashTable *sets;

#ifdef NEW_STYLE_COMPLETION
	gtk_list_store_clear(data->store);
#else
	GList *item = g_list_append(NULL, NULL);

	g_list_foreach(data->completion->items, (GFunc)g_free, NULL);
	g_completion_clear_items(data->completion);
#endif /* NEW_STYLE_COMPLETION */

	for (gnode = purple_get_blist()->root; gnode != NULL; gnode = gnode->next)
	{
		if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
			continue;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
		{
			if (!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
			{
				PidginBuddyCompletionEntry entry;
				entry.is_buddy = TRUE;
				entry.entry.buddy = (PurpleBuddy *) bnode;

				if (filter_func(&entry, user_data)) {
#ifdef NEW_STYLE_COMPLETION
					add_buddyname_autocomplete_entry(data->store,
														((PurpleContact *)cnode)->alias,
														purple_buddy_get_contact_alias(entry.entry.buddy),
														entry.entry.buddy->account,
														entry.entry.buddy->name
													 );
#else
					item->data = g_strdup(entry.entry.buddy->name);
					g_completion_add_items(data->completion, item);
#endif /* NEW_STYLE_COMPLETION */
				}
			}
		}
	}

#ifndef NEW_STYLE_COMPLETION
	g_list_free(item);
	data->log_items = NULL;
#endif /* NEW_STYLE_COMPLETION */

	sets = purple_log_get_log_sets();
	g_hash_table_foreach(sets, (GHFunc)get_log_set_name, data);
	g_hash_table_destroy(sets);

#ifndef NEW_STYLE_COMPLETION
	g_completion_add_items(data->completion, data->log_items);
	g_list_free(data->log_items);
#endif /* NEW_STYLE_COMPLETION */
}

static void
buddyname_autocomplete_destroyed_cb(GtkWidget *widget, gpointer data)
{
	g_free(data);
	purple_signals_disconnect_by_handle(widget);
}

static void
repopulate_autocomplete(gpointer something, gpointer data)
{
	add_completion_list(data);
}

void
pidgin_setup_screenname_autocomplete_with_filter(GtkWidget *entry, GtkWidget *accountopt, PidginFilterBuddyCompletionEntryFunc filter_func, gpointer user_data)
{
	PidginCompletionData *data;

#ifdef NEW_STYLE_COMPLETION
	/*
	 * Store the displayed completion value, the buddy name, the UTF-8
	 * normalized & casefolded buddy name, the UTF-8 normalized &
	 * casefolded value for comparison, and the account.
	 */
	GtkListStore *store;

	GtkEntryCompletion *completion;

	data = g_new0(PidginCompletionData, 1);
	store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	data->entry = entry;
	data->accountopt = accountopt;
	if (filter_func == NULL) {
		data->filter_func = pidgin_screenname_autocomplete_default_filter;
		data->filter_func_user_data = NULL;
	} else {
		data->filter_func = filter_func;
		data->filter_func_user_data = user_data;
	}
	data->store = store;

	add_completion_list(data);

	/* Sort the completion list by buddy name */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
	                                     1, GTK_SORT_ASCENDING);

	completion = gtk_entry_completion_new();
	gtk_entry_completion_set_match_func(completion, buddyname_completion_match_func, NULL, NULL);

	g_signal_connect(G_OBJECT(completion), "match-selected",
		G_CALLBACK(buddyname_completion_match_selected_cb), data);

	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	g_object_unref(completion);

	gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
	g_object_unref(store);

	gtk_entry_completion_set_text_column(completion, 0);

#else /* !NEW_STYLE_COMPLETION */

	data = g_new0(PidginCompletionData, 1);

	data->entry = entry;
	data->accountopt = accountopt;
	if (filter_func == NULL) {
		data->filter_func = pidgin_screenname_autocomplete_default_filter;
		data->filter_func_user_data = NULL;
	} else {
		data->filter_func = filter_func;
		data->filter_func_user_data = user_data;
	}
	data->completion = g_completion_new(NULL);
	data->completion_started = FALSE;

	add_completion_list(data);

	g_completion_set_compare(data->completion, g_ascii_strncasecmp);

	g_signal_connect(G_OBJECT(entry), "event",
					 G_CALLBACK(completion_entry_event), data);
	g_signal_connect(G_OBJECT(entry), "destroy",
					 G_CALLBACK(destroy_completion_data), data);

#endif /* !NEW_STYLE_COMPLETION */

	purple_signal_connect(purple_connections_get_handle(), "signed-on", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);

	purple_signal_connect(purple_accounts_get_handle(), "account-added", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed", entry,
						PURPLE_CALLBACK(repopulate_autocomplete), data);

	g_signal_connect(G_OBJECT(entry), "destroy", G_CALLBACK(buddyname_autocomplete_destroyed_cb), data);
}

gboolean
pidgin_screenname_autocomplete_default_filter(const PidginBuddyCompletionEntry *completion_entry, gpointer all_accounts) {
	gboolean all = GPOINTER_TO_INT(all_accounts);

	if (completion_entry->is_buddy) {
		return all || purple_account_is_connected(completion_entry->entry.buddy->account);
	} else {
		return all || (completion_entry->entry.logged_buddy->account != NULL && purple_account_is_connected(completion_entry->entry.logged_buddy->account));
	}
}

void
pidgin_setup_screenname_autocomplete(GtkWidget *entry, GtkWidget *accountopt, gboolean all) {
	pidgin_setup_screenname_autocomplete_with_filter(entry, accountopt, pidgin_screenname_autocomplete_default_filter, GINT_TO_POINTER(all));
}



void pidgin_set_cursor(GtkWidget *widget, GdkCursorType cursor_type)
{
	GdkCursor *cursor;

	g_return_if_fail(widget != NULL);
	if (widget->window == NULL)
		return;

	cursor = gdk_cursor_new(cursor_type);
	gdk_window_set_cursor(widget->window, cursor);
	gdk_cursor_unref(cursor);

#if GTK_CHECK_VERSION(2,4,0)
	gdk_display_flush(gdk_drawable_get_display(GDK_DRAWABLE(widget->window)));
#else
	gdk_flush();
#endif
}

void pidgin_clear_cursor(GtkWidget *widget)
{
	g_return_if_fail(widget != NULL);
	if (widget->window == NULL)
		return;

	gdk_window_set_cursor(widget->window, NULL);
}

struct _icon_chooser {
	GtkWidget *icon_filesel;
	GtkWidget *icon_preview;
	GtkWidget *icon_text;

	void (*callback)(const char*,gpointer);
	gpointer data;
};

#if !GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
static void
icon_filesel_delete_cb(GtkWidget *w, struct _icon_chooser *dialog)
{
	if (dialog->icon_filesel != NULL)
		gtk_widget_destroy(dialog->icon_filesel);

	if (dialog->callback)
		dialog->callback(NULL, dialog->data);

	g_free(dialog);
}
#endif /* FILECHOOSER */



#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
static void
icon_filesel_choose_cb(GtkWidget *widget, gint response, struct _icon_chooser *dialog)
{
	char *filename, *current_folder;

	if (response != GTK_RESPONSE_ACCEPT) {
		if (response == GTK_RESPONSE_CANCEL) {
			gtk_widget_destroy(dialog->icon_filesel);
		}
		dialog->icon_filesel = NULL;
		if (dialog->callback)
			dialog->callback(NULL, dialog->data);
		g_free(dialog);
		return;
	}

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog->icon_filesel));
	current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog->icon_filesel));
	if (current_folder != NULL) {
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", current_folder);
		g_free(current_folder);
	}

#else /* FILECHOOSER */
static void
icon_filesel_choose_cb(GtkWidget *w, struct _icon_chooser *dialog)
{
	char *filename, *current_folder;

	filename = g_strdup(gtk_file_selection_get_filename(
				GTK_FILE_SELECTION(dialog->icon_filesel)));

	/* If they typed in a directory, change there */
	if (pidgin_check_if_dir(filename,
				GTK_FILE_SELECTION(dialog->icon_filesel)))
	{
		g_free(filename);
		return;
	}

	current_folder = g_path_get_dirname(filename);
	if (current_folder != NULL) {
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", current_folder);
		g_free(current_folder);
	}

#endif /* FILECHOOSER */
#if 0 /* mismatched curly braces */
	}
#endif
	if (dialog->callback)
		dialog->callback(filename, dialog->data);
	gtk_widget_destroy(dialog->icon_filesel);
	g_free(filename);
	g_free(dialog);
 }


static void
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
icon_preview_change_cb(GtkFileChooser *widget, struct _icon_chooser *dialog)
#else /* FILECHOOSER */
icon_preview_change_cb(GtkTreeSelection *sel, struct _icon_chooser *dialog)
#endif /* FILECHOOSER */
{
	GdkPixbuf *pixbuf, *scale;
	int height, width;
	char *basename, *markup, *size;
	struct stat st;
	char *filename;

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	filename = gtk_file_chooser_get_preview_filename(
					GTK_FILE_CHOOSER(dialog->icon_filesel));
#else /* FILECHOOSER */
	filename = g_strdup(gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(dialog->icon_filesel)));
#endif /* FILECHOOSER */

	if (!filename || g_stat(filename, &st) || !(pixbuf = gdk_pixbuf_new_from_file(filename, NULL)))
	{
		gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_preview), NULL);
		gtk_label_set_markup(GTK_LABEL(dialog->icon_text), "");
		g_free(filename);
		return;
	}

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	basename = g_path_get_basename(filename);
	size = purple_str_size_to_units(st.st_size);
	markup = g_strdup_printf(_("<b>File:</b> %s\n"
							   "<b>File size:</b> %s\n"
							   "<b>Image size:</b> %dx%d"),
							 basename, size, width, height);

	scale = gdk_pixbuf_scale_simple(pixbuf, width * 50 / height,
									50, GDK_INTERP_BILINEAR);
	gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_preview), scale);
	gtk_label_set_markup(GTK_LABEL(dialog->icon_text), markup);

	g_object_unref(G_OBJECT(pixbuf));
	g_object_unref(G_OBJECT(scale));
	g_free(filename);
	g_free(basename);
	g_free(size);
	g_free(markup);
}


GtkWidget *pidgin_buddy_icon_chooser_new(GtkWindow *parent, void(*callback)(const char *, gpointer), gpointer data) {
	struct _icon_chooser *dialog = g_new0(struct _icon_chooser, 1);

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	GtkWidget *vbox;
#else
	GtkWidget *hbox;
	GtkWidget *tv;
	GtkTreeSelection *sel;
#endif /* FILECHOOSER */
	const char *current_folder;

	dialog->callback = callback;
	dialog->data = data;

	current_folder = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder");
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */

	dialog->icon_filesel = gtk_file_chooser_dialog_new(_("Buddy Icon"),
							   parent,
							   GTK_FILE_CHOOSER_ACTION_OPEN,
							   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							   NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog->icon_filesel), GTK_RESPONSE_ACCEPT);
	if ((current_folder != NULL) && (*current_folder != '\0'))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog->icon_filesel),
						    current_folder);

	dialog->icon_preview = gtk_image_new();
	dialog->icon_text = gtk_label_new(NULL);

	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_set_size_request(GTK_WIDGET(vbox), -1, 50);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(dialog->icon_preview), TRUE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(dialog->icon_text), FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);

	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog->icon_filesel), vbox);
	gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(dialog->icon_filesel), TRUE);
	gtk_file_chooser_set_use_preview_label(GTK_FILE_CHOOSER(dialog->icon_filesel), FALSE);

	g_signal_connect(G_OBJECT(dialog->icon_filesel), "update-preview",
					 G_CALLBACK(icon_preview_change_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "response",
					 G_CALLBACK(icon_filesel_choose_cb), dialog);
	icon_preview_change_cb(NULL, dialog);
#else /* FILECHOOSER */
	dialog->icon_filesel = gtk_file_selection_new(_("Buddy Icon"));
	dialog->icon_preview = gtk_image_new();
	dialog->icon_text = gtk_label_new(NULL);
	if ((current_folder != NULL) && (*current_folder != '\0'))
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog->icon_filesel),
										current_folder);

	gtk_widget_set_size_request(GTK_WIDGET(dialog->icon_preview),-1, 50);
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(
		GTK_BOX(GTK_FILE_SELECTION(dialog->icon_filesel)->main_vbox),
		hbox, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), dialog->icon_preview,
			 FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), dialog->icon_text, FALSE, FALSE, 0);

	tv = GTK_FILE_SELECTION(dialog->icon_filesel)->file_list;
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));

	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(icon_preview_change_cb), dialog);
	g_signal_connect(
		G_OBJECT(GTK_FILE_SELECTION(dialog->icon_filesel)->ok_button),
		"clicked",
		G_CALLBACK(icon_filesel_choose_cb), dialog);
	g_signal_connect(
		G_OBJECT(GTK_FILE_SELECTION(dialog->icon_filesel)->cancel_button),
		"clicked",
		G_CALLBACK(icon_filesel_delete_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "destroy",
					 G_CALLBACK(icon_filesel_delete_cb), dialog);
#endif /* FILECHOOSER */

#ifdef _WIN32
	g_signal_connect(G_OBJECT(dialog->icon_filesel), "show",
		G_CALLBACK(winpidgin_ensure_onscreen), dialog->icon_filesel);
#endif

	return dialog->icon_filesel;
}


#if GTK_CHECK_VERSION(2,2,0)
static gboolean
str_array_match(char **a, char **b)
{
	int i, j;

	if (!a || !b)
		return FALSE;
	for (i = 0; a[i] != NULL; i++)
		for (j = 0; b[j] != NULL; j++)
			if (!g_ascii_strcasecmp(a[i], b[j]))
				return TRUE;
	return FALSE;
}
#endif

gpointer
pidgin_convert_buddy_icon(PurplePlugin *plugin, const char *path, size_t *len)
{
	PurplePluginProtocolInfo *prpl_info;
#if GTK_CHECK_VERSION(2,2,0)
	char **prpl_formats;
	int width, height;
	char **pixbuf_formats = NULL;
	GdkPixbufFormat *format;
	GdkPixbuf *pixbuf;
#if !GTK_CHECK_VERSION(2,4,0)
	GdkPixbufLoader *loader;
#endif
#endif
	gchar *contents;
	gsize length;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

	g_return_val_if_fail(prpl_info->icon_spec.format != NULL, NULL);


#if GTK_CHECK_VERSION(2,2,0)
#if GTK_CHECK_VERSION(2,4,0)
	format = gdk_pixbuf_get_file_info(path, &width, &height);
#else
	loader = gdk_pixbuf_loader_new();
	if (g_file_get_contents(path, &contents, &length, NULL)) {
		gdk_pixbuf_loader_write(loader, contents, length, NULL);
		g_free(contents);
	}
	gdk_pixbuf_loader_close(loader, NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	format = gdk_pixbuf_loader_get_format(loader);
	g_object_unref(G_OBJECT(loader));
#endif
	if (format == NULL)
		return NULL;

	pixbuf_formats = gdk_pixbuf_format_get_extensions(format);
	prpl_formats = g_strsplit(prpl_info->icon_spec.format,",",0);
	if (str_array_match(pixbuf_formats, prpl_formats) &&                  /* This is an acceptable format AND */
		 (!(prpl_info->icon_spec.scale_rules & PURPLE_ICON_SCALE_SEND) ||   /* The prpl doesn't scale before it sends OR */
		  (prpl_info->icon_spec.min_width <= width &&
		   prpl_info->icon_spec.max_width >= width &&
		   prpl_info->icon_spec.min_height <= height &&
		   prpl_info->icon_spec.max_height >= height)))                   /* The icon is the correct size */
#endif
	{
#if GTK_CHECK_VERSION(2,2,0)
		g_strfreev(prpl_formats);
		g_strfreev(pixbuf_formats);
#endif
		/* We don't need to scale the image. */

		contents = NULL;
		if (!g_file_get_contents(path, &contents, &length, NULL))
		{
			g_free(contents);
#if GTK_CHECK_VERSION(2,2,0) && !GTK_CHECK_VERSION(2,4,0)
		g_object_unref(G_OBJECT(pixbuf));
#endif
			return NULL;
		}
	}
#if GTK_CHECK_VERSION(2,2,0)
	else
	{
		int i;
		GError *error = NULL;
		GdkPixbuf *scale;
		gboolean success = FALSE;
		char *filename = NULL;

		g_strfreev(pixbuf_formats);

		pixbuf = gdk_pixbuf_new_from_file(path, &error);
		if (error) {
			purple_debug_error("buddyicon", "Could not open icon for conversion: %s\n", error->message);
			g_error_free(error);
			g_strfreev(prpl_formats);
			return NULL;
		}

		if ((prpl_info->icon_spec.scale_rules & PURPLE_ICON_SCALE_SEND) &&
			(width < prpl_info->icon_spec.min_width ||
			 width > prpl_info->icon_spec.max_width ||
			 height < prpl_info->icon_spec.min_height ||
			 height > prpl_info->icon_spec.max_height))
		{
			int new_width = width;
			int new_height = height;

			purple_buddy_icon_get_scale_size(&prpl_info->icon_spec, &new_width, &new_height);

			scale = gdk_pixbuf_scale_simple(pixbuf, new_width, new_height,
					GDK_INTERP_HYPER);
			g_object_unref(G_OBJECT(pixbuf));
			pixbuf = scale;
		}

		for (i = 0; prpl_formats[i]; i++) {
			FILE *fp;

			g_free(filename);
			fp = purple_mkstemp(&filename, TRUE);
			if (!fp)
			{
				g_free(filename);
				return NULL;
			}
			fclose(fp);

			purple_debug_info("buddyicon", "Converting buddy icon to %s as %s\n", prpl_formats[i], filename);
			/* The "compression" param wasn't supported until gdk-pixbuf 2.8.
			 * Using it in previous versions causes the save to fail (and an assert message).  */
			if ((gdk_pixbuf_major_version > 2 || (gdk_pixbuf_major_version == 2
						&& gdk_pixbuf_minor_version >= 8))
					&& strcmp(prpl_formats[i], "png") == 0) {
				if (gdk_pixbuf_save(pixbuf, filename, prpl_formats[i],
						&error, "compression", "9", NULL)) {
					success = TRUE;
					break;
				}
			} else if (gdk_pixbuf_save(pixbuf, filename, prpl_formats[i],
					&error, NULL)) {
				success = TRUE;
				break;
			}

			/* The NULL checking is necessary due to this bug:
			 * http://bugzilla.gnome.org/show_bug.cgi?id=405539 */
			purple_debug_warning("buddyicon", "Could not convert to %s: %s\n", prpl_formats[i],
				(error && error->message) ? error->message : "Unknown error");
			g_error_free(error);
			error = NULL;
		}
		g_strfreev(prpl_formats);
		g_object_unref(G_OBJECT(pixbuf));
		if (!success) {
			purple_debug_error("buddyicon", "Could not convert icon to usable format.\n");
			return NULL;
		}

		contents = NULL;
		if (!g_file_get_contents(filename, &contents, &length, NULL))
		{
			purple_debug_error("buddyicon",
					"Could not read '%s', which we just wrote to disk.\n",
					filename);

			g_free(contents);
			g_free(filename);
			return NULL;
		}

		g_unlink(filename);
		g_free(filename);
	}

	/* Check the image size */
	/*
	 * TODO: If the file is too big, it would be cool if we checked if
	 *       the prpl supported jpeg, and then we could convert to that
	 *       and use a lower quality setting.
	 */
	if ((prpl_info->icon_spec.max_filesize != 0) &&
	    (length > prpl_info->icon_spec.max_filesize))
	{
		gchar *tmp;
		tmp = g_strdup_printf(_("The file '%s' is too large for %s.  Please try a smaller image.\n"),
				path, plugin->info->name);
		purple_notify_error(NULL, _("Icon Error"),
				_("Could not set icon"), tmp);
		purple_debug_info("buddyicon",
				"'%s' was converted to an image which is %" G_GSIZE_FORMAT
				" bytes, but the maximum icon size for %s is %" G_GSIZE_FORMAT
				" bytes\n", path, length, plugin->info->name,
				prpl_info->icon_spec.max_filesize);
		g_free(tmp);
		return NULL;
	}

	if (len)
		*len = length;
	return contents;
#else
	/*
	 * The chosen icon wasn't the right size, and we're using
	 * GTK+ 2.0 so we can't scale it.
	 */
	return NULL;
#endif
}

#if !GTK_CHECK_VERSION(2,6,0)
static void
_gdk_file_scale_size_prepared_cb (GdkPixbufLoader *loader,
		  int              width,
		  int              height,
		  gpointer         data)
{
	struct {
		gint width;
		gint height;
		gboolean preserve_aspect_ratio;
	} *info = data;

	g_return_if_fail (width > 0 && height > 0);

	if (info->preserve_aspect_ratio &&
		(info->width > 0 || info->height > 0)) {
		if (info->width < 0)
		{
			width = width * (double)info->height/(double)height;
			height = info->height;
		}
		else if (info->height < 0)
		{
			height = height * (double)info->width/(double)width;
			width = info->width;
		}
		else if ((double)height * (double)info->width >
				 (double)width * (double)info->height) {
			width = 0.5 + (double)width * (double)info->height / (double)height;
			height = info->height;
		} else {
			height = 0.5 + (double)height * (double)info->width / (double)width;
			width = info->width;
		}
	} else {
			if (info->width > 0)
				width = info->width;
			if (info->height > 0)
				height = info->height;
	}

#if GTK_CHECK_VERSION(2,2,0) /* 2.0 users are going to have very strangely sized things */
	gdk_pixbuf_loader_set_size (loader, width, height);
#else
#warning  nosnilmot could not be bothered to fix this properly for you
#warning  ... good luck ... your images may end up strange sizes
#endif
}

GdkPixbuf *
gdk_pixbuf_new_from_file_at_scale(const char *filename, int width, int height,
				  				  gboolean preserve_aspect_ratio,
								  GError **error)
{
	GdkPixbufLoader *loader;
	GdkPixbuf       *pixbuf;
	guchar buffer [4096];
	int length;
	FILE *f;
	struct {
		gint width;
		gint height;
		gboolean preserve_aspect_ratio;
	} info;
	GdkPixbufAnimation *animation;
	GdkPixbufAnimationIter *iter;
	gboolean has_frame;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (width > 0 || width == -1, NULL);
	g_return_val_if_fail (height > 0 || height == -1, NULL);

	f = g_fopen (filename, "rb");
	if (!f) {
		gint save_errno = errno;
		gchar *display_name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
		g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (save_errno),
					 _("Failed to open file '%s': %s"),
					 display_name ? display_name : "(unknown)",
					 g_strerror (save_errno));
		g_free (display_name);
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();

	info.width = width;
	info.height = height;
	info.preserve_aspect_ratio = preserve_aspect_ratio;

	g_signal_connect (loader, "size-prepared", G_CALLBACK (_gdk_file_scale_size_prepared_cb), &info);

	has_frame = FALSE;
	while (!has_frame && !feof (f) && !ferror (f)) {
		length = fread (buffer, 1, sizeof (buffer), f);
		if (length > 0)
			if (!gdk_pixbuf_loader_write (loader, buffer, length, error)) {
				gdk_pixbuf_loader_close (loader, NULL);
				fclose (f);
				g_object_unref (loader);
				return NULL;
			}

		animation = gdk_pixbuf_loader_get_animation (loader);
		if (animation) {
			iter = gdk_pixbuf_animation_get_iter (animation, 0);
			if (!gdk_pixbuf_animation_iter_on_currently_loading_frame (iter)) {
				has_frame = TRUE;
			}
			g_object_unref (iter);
		}
	}

	fclose (f);

	if (!gdk_pixbuf_loader_close (loader, error) && !has_frame) {
		g_object_unref (loader);
		return NULL;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (!pixbuf) {
		gchar *display_name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
		g_object_unref (loader);
		g_set_error (error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
					 _("Failed to load image '%s': reason not known, probably a corrupt image file"),
					 display_name ? display_name : "(unknown)");
		g_free (display_name);
		return NULL;
	}

	g_object_ref (pixbuf);

	g_object_unref (loader);

	return pixbuf;
}
#endif /* ! Gtk 2.6.0 */

void pidgin_set_custom_buddy_icon(PurpleAccount *account, const char *who, const char *filename)
{
	PurpleBuddy *buddy;
	PurpleContact *contact;

	buddy = purple_find_buddy(account, who);
	if (!buddy) {
		purple_debug_info("custom-icon", "You can only set custom icon for someone in your buddylist.\n");
		return;
	}

	contact = purple_buddy_get_contact(buddy);
	purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, filename);
}

char *pidgin_make_pretty_arrows(const char *str)
{
	char *ret;
	char **split = g_strsplit(str, "->", -1);
	ret = g_strjoinv("\342\207\250", split);
	g_strfreev(split);

	split = g_strsplit(ret, "<-", -1);
	g_free(ret);
	ret = g_strjoinv("\342\207\246", split);
	g_strfreev(split);

	return ret;
}

void pidgin_set_urgent(GtkWindow *window, gboolean urgent)
{
#if GTK_CHECK_VERSION(2,8,0)
	gtk_window_set_urgency_hint(window, urgent);
#elif defined _WIN32
	winpidgin_window_flash(window, urgent);
#elif defined GDK_WINDOWING_X11
	GdkWindow *gdkwin;
	XWMHints *hints;

	g_return_if_fail(window != NULL);

	gdkwin = GTK_WIDGET(window)->window;

	g_return_if_fail(gdkwin != NULL);

	hints = XGetWMHints(GDK_WINDOW_XDISPLAY(gdkwin),
	                    GDK_WINDOW_XWINDOW(gdkwin));
	if(!hints)
		hints = XAllocWMHints();

	if (urgent)
		hints->flags |= XUrgencyHint;
	else
		hints->flags &= ~XUrgencyHint;
	XSetWMHints(GDK_WINDOW_XDISPLAY(gdkwin),
	            GDK_WINDOW_XWINDOW(gdkwin), hints);
	XFree(hints);
#else
	/* do something else? */
#endif
}

static GSList *minidialogs = NULL;

static void *
pidgin_utils_get_handle(void)
{
	static int handle;

	return &handle;
}

static void connection_signed_off_cb(PurpleConnection *gc)
{
	GSList *list, *l_next;
	for (list = minidialogs; list; list = l_next) {
		l_next = list->next;
		if (g_object_get_data(G_OBJECT(list->data), "gc") == gc) {
				gtk_widget_destroy(GTK_WIDGET(list->data));
		}
	}
}

static void alert_killed_cb(GtkWidget *widget)
{
	minidialogs = g_slist_remove(minidialogs, widget);
}

struct _old_button_clicked_cb_data
{
	PidginUtilMiniDialogCallback cb;
	gpointer data;
};

static void
old_mini_dialog_button_clicked_cb(PidginMiniDialog *mini_dialog,
                                  GtkButton *button,
                                  gpointer user_data)
{
	struct _old_button_clicked_cb_data *data = user_data;
	data->cb(data->data, button);
}

static void
old_mini_dialog_destroy_cb(GtkWidget *dialog,
                           GList *cb_datas)
{
	while (cb_datas != NULL)
	{
		g_free(cb_datas->data);
		cb_datas = g_list_delete_link(cb_datas, cb_datas);
	}
}

GtkWidget *
pidgin_make_mini_dialog(PurpleConnection *gc,
                        const char *icon_name,
                        const char *primary,
                        const char *secondary,
                        void *user_data,
                        ...)
{
	PidginMiniDialog *mini_dialog;
	const char *button_text;
	GList *cb_datas = NULL;
	va_list args;
	static gboolean first_call = TRUE;

	if (first_call) {
		first_call = FALSE;
		purple_signal_connect(purple_connections_get_handle(), "signed-off",
		                      pidgin_utils_get_handle(),
		                      PURPLE_CALLBACK(connection_signed_off_cb), NULL);
	}

	mini_dialog = pidgin_mini_dialog_new(primary, secondary, icon_name);
	g_object_set_data(G_OBJECT(mini_dialog), "gc" ,gc);
	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		G_CALLBACK(alert_killed_cb), NULL);

	va_start(args, user_data);
	while ((button_text = va_arg(args, char*))) {
		struct _old_button_clicked_cb_data *data = NULL;
		PidginMiniDialogCallback wrapper_cb = NULL;
		PidginUtilMiniDialogCallback callback =
			va_arg(args, PidginUtilMiniDialogCallback);

		if (callback != NULL) {
			data = g_new0(struct _old_button_clicked_cb_data, 1);
			data->cb = callback;
			data->data = user_data;
			wrapper_cb = old_mini_dialog_button_clicked_cb;
		}
		pidgin_mini_dialog_add_button(mini_dialog, button_text,
			wrapper_cb, data);
		cb_datas = g_list_append(cb_datas, data);
	}
	va_end(args);

	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		G_CALLBACK(old_mini_dialog_destroy_cb), cb_datas);

	return GTK_WIDGET(mini_dialog);
}

/*
 * "This is so dead sexy."
 * "Two thumbs up."
 * "Best movie of the year."
 *
 * This is the function that handles CTRL+F searching in the buddy list.
 * It finds the top-most buddy/group/chat/whatever containing the
 * entered string.
 *
 * It's somewhat ineffecient, because we strip all the HTML from the
 * "name" column of the buddy list (because the GtkTreeModel does not
 * contain the screen name in a non-markedup format).  But the alternative
 * is to add an extra column to the GtkTreeModel.  And this function is
 * used rarely, so it shouldn't matter TOO much.
 */
gboolean pidgin_tree_view_search_equal_func(GtkTreeModel *model, gint column,
			const gchar *key, GtkTreeIter *iter, gpointer data)
{
	gchar *enteredstring;
	gchar *tmp;
	gchar *withmarkup;
	gchar *nomarkup;
	gchar *normalized;
	gboolean result;
	size_t i;
	size_t len;
	PangoLogAttr *log_attrs;
	gchar *word;

	if (g_ascii_strcasecmp(key, "Global Thermonuclear War") == 0)
	{
		purple_notify_info(NULL, "WOPR",
				"Wouldn't you prefer a nice game of chess?", NULL);
		return FALSE;
	}

	gtk_tree_model_get(model, iter, column, &withmarkup, -1);
	if (withmarkup == NULL)   /* This is probably a separator */
		return TRUE;

	tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
	enteredstring = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	nomarkup = purple_markup_strip_html(withmarkup);
	tmp = g_utf8_normalize(nomarkup, -1, G_NORMALIZE_DEFAULT);
	g_free(nomarkup);
	normalized = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	if (purple_str_has_prefix(normalized, enteredstring))
	{
		g_free(withmarkup);
		g_free(enteredstring);
		g_free(normalized);
		return FALSE;
	}


	/* Use Pango to separate by words. */
	len = g_utf8_strlen(normalized, -1);
	log_attrs = g_new(PangoLogAttr, len + 1);

	pango_get_log_attrs(normalized, strlen(normalized), -1, NULL, log_attrs, len + 1);

	word = normalized;
	result = TRUE;
	for (i = 0; i < (len - 1) ; i++)
	{
		if (log_attrs[i].is_word_start &&
		    purple_str_has_prefix(word, enteredstring))
		{
			result = FALSE;
			break;
		}
		word = g_utf8_next_char(word);
	}
	g_free(log_attrs);

/* The non-Pango version. */
#if 0
	word = normalized;
	result = TRUE;
	while (word[0] != '\0')
	{
		gunichar c = g_utf8_get_char(word);
		if (!g_unichar_isalnum(c))
		{
			word = g_utf8_find_next_char(word, NULL);
			if (purple_str_has_prefix(word, enteredstring))
			{
				result = FALSE;
				break;
			}
		}
		else
			word = g_utf8_find_next_char(word, NULL);
	}
#endif

	g_free(withmarkup);
	g_free(enteredstring);
	g_free(normalized);

	return result;
}


gboolean pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf) {
        int width, height, rowstride, i;
        unsigned char *pixels;
        unsigned char *row;

        if (!gdk_pixbuf_get_has_alpha(pixbuf))
                return TRUE;

        width = gdk_pixbuf_get_width (pixbuf);
        height = gdk_pixbuf_get_height (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);

        row = pixels;
        for (i = 3; i < rowstride; i+=4) {
                if (row[i] < 0xfe)
                        return FALSE;
        }

        for (i = 1; i < height - 1; i++) {
                row = pixels + (i*rowstride);
                if (row[3] < 0xfe || row[rowstride-1] < 0xfe) {
                        return FALSE;
            }
        }

        row = pixels + ((height-1) * rowstride);
        for (i = 3; i < rowstride; i+=4) {
                if (row[i] < 0xfe)
                        return FALSE;
        }

        return TRUE;
}

void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf) {
	int width, height, rowstride;
        guchar *pixels;
        if (!gdk_pixbuf_get_has_alpha(pixbuf))
                return;
        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);
        rowstride = gdk_pixbuf_get_rowstride(pixbuf);
        pixels = gdk_pixbuf_get_pixels(pixbuf);

        if (width < 6 || height < 6)
                return;
        /* Top left */
        pixels[3] = 0;
        pixels[7] = 0x80;
        pixels[11] = 0xC0;
        pixels[rowstride + 3] = 0x80;
        pixels[rowstride * 2 + 3] = 0xC0;

        /* Top right */
        pixels[width * 4 - 1] = 0;
        pixels[width * 4 - 5] = 0x80;
        pixels[width * 4 - 9] = 0xC0;
        pixels[rowstride + (width * 4) - 1] = 0x80;
        pixels[(2 * rowstride) + (width * 4) - 1] = 0xC0;

        /* Bottom left */
        pixels[(height - 1) * rowstride + 3] = 0;
        pixels[(height - 1) * rowstride + 7] = 0x80;
        pixels[(height - 1) * rowstride + 11] = 0xC0;
        pixels[(height - 2) * rowstride + 3] = 0x80;
        pixels[(height - 3) * rowstride + 3] = 0xC0;

        /* Bottom right */
        pixels[height * rowstride - 1] = 0;
        pixels[(height - 1) * rowstride - 1] = 0x80;
        pixels[(height - 2) * rowstride - 1] = 0xC0;
        pixels[height * rowstride - 5] = 0x80;
        pixels[height * rowstride - 9] = 0xC0;
}

const char *pidgin_get_dim_grey_string(GtkWidget *widget) {
	static char dim_grey_string[8] = "";
	GtkStyle *style;

	if (!widget)
		return "dim grey";

 	style = gtk_widget_get_style(widget);
	if (!style)
		return "dim grey";

	snprintf(dim_grey_string, sizeof(dim_grey_string), "#%02x%02x%02x",
	style->text_aa[GTK_STATE_NORMAL].red >> 8,
	style->text_aa[GTK_STATE_NORMAL].green >> 8,
	style->text_aa[GTK_STATE_NORMAL].blue >> 8);
	return dim_grey_string;
}

#if !GTK_CHECK_VERSION(2,2,0)
GtkTreePath *
gtk_tree_path_new_from_indices (gint first_index, ...)
{
	int arg;
	va_list args;
	GtkTreePath *path;

	path = gtk_tree_path_new ();

	va_start (args, first_index);
	arg = first_index;

	while (arg != -1)
	{
		gtk_tree_path_append_index (path, arg);
		arg = va_arg (args, gint);
	}

	va_end (args);

	return path;
}
#endif

static void
combo_box_changed_cb(GtkComboBox *combo_box, GtkEntry *entry)
{
	char *text = gtk_combo_box_get_active_text(combo_box);
	gtk_entry_set_text(entry, text ? text : "");
	g_free(text);
}

static gboolean
entry_key_pressed_cb(GtkWidget *entry, GdkEventKey *key, GtkComboBox *combo)
{
	if (key->keyval == GDK_Down || key->keyval == GDK_Up) {
		gtk_combo_box_popup(combo);
		return TRUE;
	}
	return FALSE;
}

GtkWidget *
pidgin_text_combo_box_entry_new(const char *default_item, GList *items)
{
	GtkComboBox *ret = NULL;
	GtkWidget *the_entry = NULL;

	ret = GTK_COMBO_BOX(gtk_combo_box_new_text());
	the_entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(ret), the_entry);

	if (default_item)
		gtk_entry_set_text(GTK_ENTRY(the_entry), default_item);

	for (; items != NULL ; items = items->next) {
		char *text = items->data;
		if (text && *text)
			gtk_combo_box_append_text(ret, text);
	}

	g_signal_connect(G_OBJECT(ret), "changed", (GCallback)combo_box_changed_cb, the_entry);
	g_signal_connect_after(G_OBJECT(the_entry), "key-press-event", G_CALLBACK(entry_key_pressed_cb), ret);

	return GTK_WIDGET(ret);
}

const char *pidgin_text_combo_box_entry_get_text(GtkWidget *widget)
{
	return gtk_entry_get_text(GTK_ENTRY(GTK_BIN((widget))->child));
}

void pidgin_text_combo_box_entry_set_text(GtkWidget *widget, const char *text)
{
	gtk_entry_set_text(GTK_ENTRY(GTK_BIN((widget))->child), (text));
}

GtkWidget *
pidgin_add_widget_to_vbox(GtkBox *vbox, const char *widget_label, GtkSizeGroup *sg, GtkWidget *widget, gboolean expand, GtkWidget **p_label)
{
	GtkWidget *hbox;
	GtkWidget *label = NULL;

	if (widget_label) {
		hbox = gtk_hbox_new(FALSE, 5);
		gtk_widget_show(hbox);
		gtk_box_pack_start(vbox, hbox, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(widget_label);
		gtk_widget_show(label);
		if (sg) {
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			gtk_size_group_add_widget(sg, label);
		}
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	} else {
		hbox = GTK_WIDGET(vbox);
	}

	gtk_widget_show(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, expand, TRUE, 0);
	if (label) {
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
		pidgin_set_accessible_label (widget, label);
	}

	if (p_label)
		(*p_label) = label;
	return hbox;
}

gboolean pidgin_auto_parent_window(GtkWidget *widget)
{
#if 0
	/* This looks at the most recent window that received focus, and makes
	 * that the parent window. */
#ifndef _WIN32
	static GdkAtom _WindowTime = GDK_NONE;
	static GdkAtom _Cardinal = GDK_NONE;
	GList *windows = NULL;
	GtkWidget *parent = NULL;
	time_t window_time = 0;

	windows = gtk_window_list_toplevels();

	if (_WindowTime == GDK_NONE) {
		_WindowTime = gdk_x11_xatom_to_atom(gdk_x11_get_xatom_by_name("_NET_WM_USER_TIME"));
	}
	if (_Cardinal == GDK_NONE) {
		_Cardinal = gdk_atom_intern("CARDINAL", FALSE);
	}

	while (windows) {
		GtkWidget *window = windows->data;
		guchar *data = NULL;
		int al = 0;
		time_t value;

		windows = g_list_delete_link(windows, windows);

		if (window == widget ||
				!GTK_WIDGET_VISIBLE(window))
			continue;

		if (!gdk_property_get(window->window, _WindowTime, _Cardinal, 0, sizeof(time_t), FALSE,
				NULL, NULL, &al, &data))
			continue;
		value = *(time_t *)data;
		if (window_time < value) {
			window_time = value;
			parent = window;
		}
		g_free(data);
	}
	if (windows)
		g_list_free(windows);
	if (parent) {
		if (!gtk_get_current_event() && gtk_window_has_toplevel_focus(GTK_WINDOW(parent))) {
			/* The window is in focus, and the new window was not triggered by a keypress/click
			 * event. So do not set it transient, to avoid focus stealing and all that.
			 */
			return FALSE;
		}
		gtk_window_set_transient_for(GTK_WINDOW(widget), GTK_WINDOW(parent));
		return TRUE;
	}
	return FALSE;
#endif
#else
#if GTK_CHECK_VERSION(2,4,0)
	/* This finds the currently active window and makes that the parent window. */
	GList *windows = NULL;
	GtkWidget *parent = NULL;
	GdkEvent *event = gtk_get_current_event();
	GdkWindow *menu = NULL;

	if (event == NULL)
		/* The window was not triggered by a user action. */
		return FALSE;

	/* We need to special case events from a popup menu. */
	if (event->type == GDK_BUTTON_RELEASE) {
		/* XXX: Neither of the following works:
			menu = event->button.window;
			menu = gdk_window_get_parent(event->button.window);
			menu = gdk_window_get_toplevel(event->button.window);
		*/
	} else if (event->type == GDK_KEY_PRESS)
		menu = event->key.window;

	windows = gtk_window_list_toplevels();
	while (windows) {
		GtkWidget *window = windows->data;
		windows = g_list_delete_link(windows, windows);

		if (window == widget ||
				!GTK_WIDGET_VISIBLE(window)) {
			continue;
		}

		if (gtk_window_has_toplevel_focus(GTK_WINDOW(window)) ||
				(menu && menu == window->window)) {
			parent = window;
			break;
		}
	}
	if (windows)
		g_list_free(windows);
	if (parent) {
		gtk_window_set_transient_for(GTK_WINDOW(widget), GTK_WINDOW(parent));
		return TRUE;
	}
#endif
	return FALSE;
#endif
}

GdkPixbuf * pidgin_pixbuf_from_imgstore(PurpleStoredImage *image)
{
	GdkPixbuf *pixbuf;
	GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(loader, purple_imgstore_get_data(image),
			purple_imgstore_get_size(image), NULL);
	gdk_pixbuf_loader_close(loader, NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
	if (pixbuf)
		g_object_ref(pixbuf);
	g_object_unref(loader);
	return pixbuf;
}

static void url_copy(GtkWidget *w, gchar *url)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard(w, GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);

	clipboard = gtk_widget_get_clipboard(w, GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
}

static gboolean
link_context_menu(GtkIMHtml *imhtml, GtkIMHtmlLink *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	const char *url;

	url = gtk_imhtml_link_get_url(link);

	/* Open Link */
	img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Open Link"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gtk_imhtml_link_activate), link);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* Copy Link Location */
	img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Link Location"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(url_copy), (gpointer)url);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	return TRUE;
}

static gboolean
copy_email_address(GtkIMHtml *imhtml, GtkIMHtmlLink *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	const char *text;
	char *address;
#define MAILTOSIZE  (sizeof("mailto:") - 1)

	text = gtk_imhtml_link_get_url(link);
	g_return_val_if_fail(text && strlen(text) > MAILTOSIZE, FALSE);
	address = (char*)text + MAILTOSIZE;

	/* Copy Email Address */
	img = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Copy Email Address"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(url_copy), address);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	return TRUE;
}

static void
file_open_uri(GtkIMHtml *imhtml, const char *uri)
{
	/* Copied from gtkft.c:open_button_cb */
#ifdef _WIN32
	/* If using Win32... */
	int code;
	if (G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wc_filename = g_utf8_to_utf16(
				uri, -1, NULL, NULL, NULL);

		code = (int)ShellExecuteW(NULL, NULL, wc_filename, NULL, NULL,
				SW_SHOW);

		g_free(wc_filename);
	} else {
		char *l_filename = g_locale_from_utf8(
				uri, -1, NULL, NULL, NULL);

		code = (int)ShellExecuteA(NULL, NULL, l_filename, NULL, NULL,
				SW_SHOW);

		g_free(l_filename);
	}

	if (code == SE_ERR_ASSOCINCOMPLETE || code == SE_ERR_NOASSOC)
	{
		purple_notify_error(imhtml, NULL,
				_("There is no application configured to open this type of file."), NULL);
	}
	else if (code < 32)
	{
		purple_notify_error(imhtml, NULL,
				_("An error occurred while opening the file."), NULL);
		purple_debug_warning("gtkutils", "filename: %s; code: %d\n", uri, code);
	}
#else
	char *command = NULL;
	char *tmp = NULL;
	GError *error = NULL;

	if (purple_running_gnome())
	{
		char *escaped = g_shell_quote(uri);
		command = g_strdup_printf("gnome-open %s", escaped);
		g_free(escaped);
	}
	else if (purple_running_kde())
	{
		char *escaped = g_shell_quote(uri);

		if (purple_str_has_suffix(uri, ".desktop"))
			command = g_strdup_printf("kfmclient openURL %s 'text/plain'", escaped);
		else
			command = g_strdup_printf("kfmclient openURL %s", escaped);
		g_free(escaped);
	}
	else
	{
		purple_notify_uri(NULL, uri);
		return;
	}

	if (purple_program_is_valid(command))
	{
		gint exit_status;
		if (!g_spawn_command_line_sync(command, NULL, NULL, &exit_status, &error))
		{
			tmp = g_strdup_printf(_("Error launching %s: %s"),
							uri, error->message);
			purple_notify_error(imhtml, NULL, _("Unable to open file."), tmp);
			g_free(tmp);
			g_error_free(error);
		}
		if (exit_status != 0)
		{
			char *primary = g_strdup_printf(_("Error running %s"), command);
			char *secondary = g_strdup_printf(_("Process returned error code %d"),
									exit_status);
			purple_notify_error(imhtml, NULL, primary, secondary);
			g_free(tmp);
		}
	}
#endif
}

#define FILELINKSIZE  (sizeof("file://") - 1)
static gboolean
file_clicked_cb(GtkIMHtml *imhtml, GtkIMHtmlLink *link)
{
	const char *uri = gtk_imhtml_link_get_url(link) + FILELINKSIZE;
	file_open_uri(imhtml, uri);
	return TRUE;
}

static gboolean
open_containing_cb(GtkIMHtml *imhtml, const char *url)
{
	char *dir = g_path_get_dirname(url + FILELINKSIZE);
	file_open_uri(imhtml, dir);
	g_free(dir);
	return TRUE;
}

static gboolean
file_context_menu(GtkIMHtml *imhtml, GtkIMHtmlLink *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	const char *url;

	url = gtk_imhtml_link_get_url(link);

	/* Open File */
	img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Open File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gtk_imhtml_link_activate), link);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* Open Containing Directory */
#if GTK_CHECK_VERSION(2,6,0)
	img = gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("Open _Containing Directory"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
#else
	item = gtk_menu_item_new_with_mnemonic(_("Open _Containing Directory"));
#endif
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(open_containing_cb), (gpointer)url);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	return TRUE;
}

#define AUDIOLINKSIZE  (sizeof("audio://") - 1)
static gboolean
audio_clicked_cb(GtkIMHtml *imhtml, GtkIMHtmlLink *link)
{
	const char *uri;
	PidginConversation *conv = g_object_get_data(G_OBJECT(imhtml), "gtkconv");
	if (!conv) /* no playback in debug window */
		return TRUE;
	uri = gtk_imhtml_link_get_url(link) + AUDIOLINKSIZE;
	purple_sound_play_file(uri, NULL);
	return TRUE;
}

static void
savefile_write_cb(gpointer user_data, char *file)
{
	char *temp_file = user_data;
	gchar *contents;
	gsize length;
	GError *error = NULL;

	if (!g_file_get_contents(temp_file, &contents, &length, &error)) {
		purple_debug_error("gtkutils", "Unable to read contents of %s: %s\n",
		                   temp_file, error->message);
		g_error_free(error);
		return;
	}

	if (!purple_util_write_data_to_file_absolute(file, contents, length)) {
		purple_debug_error("gtkutils", "Unable to write contents to %s\n",
		                   file);
	}
}

static gboolean
save_file_cb(GtkWidget *item, const char *url)
{
	PidginConversation *conv = g_object_get_data(G_OBJECT(item), "gtkconv");
	if (!conv)
		return TRUE;
	purple_request_file(conv->active_conv, _("Save File"), NULL, TRUE,
	                    G_CALLBACK(savefile_write_cb), NULL,
	                    conv->active_conv->account, NULL, conv->active_conv,
	                    (void *)url);
	return TRUE;
}

static gboolean
audio_context_menu(GtkIMHtml *imhtml, GtkIMHtmlLink *link, GtkWidget *menu)
{
	GtkWidget *img, *item;
	const char *url;
	PidginConversation *conv = g_object_get_data(G_OBJECT(imhtml), "gtkconv");
	if (!conv) /* No menu in debug window */
		return TRUE;

	url = gtk_imhtml_link_get_url(link);

	/* Play Sound */
#if GTK_CHECK_VERSION(2,6,0)
	img = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Play Sound"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
#else
	item = gtk_menu_item_new_with_mnemonic(_("_Play Sound"));
#endif
	g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(gtk_imhtml_link_activate), link);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* Save File */
	img = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Save File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(save_file_cb), (gpointer)(url+AUDIOLINKSIZE));
	g_object_set_data(G_OBJECT(item), "gtkconv", conv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	return TRUE;
}

/* XXX: The following two functions are for demonstration purposes only! */
static gboolean
open_dialog(GtkIMHtml *imhtml, GtkIMHtmlLink *link)
{
	const char *url;
	const char *str;

	url = gtk_imhtml_link_get_url(link);
	if (!url || strlen(url) < sizeof("open://"))
		return FALSE;

	str = url + sizeof("open://") - 1;

	if (strcmp(str, "accounts") == 0)
		pidgin_accounts_window_show();
	else if (strcmp(str, "prefs") == 0)
		pidgin_prefs_show();
	else
		return FALSE;
	return TRUE;
}

static gboolean
dummy(GtkIMHtml *imhtml, GtkIMHtmlLink *link, GtkWidget *menu)
{
	return TRUE;
}

static gboolean
register_gnome_url_handlers(void)
{
	char *tmp;
	char *err;
	char *c;
	char *start;

	tmp = g_find_program_in_path("gconftool-2");
	if (tmp == NULL)
		return FALSE;

	g_free(tmp);
	tmp = NULL;

	if (!g_spawn_command_line_sync("gconftool-2 --all-dirs /desktop/gnome/url-handlers",
	                               &tmp, &err, NULL, NULL))
	{
		g_free(tmp);
		g_free(err);
		g_return_val_if_reached(FALSE);
	}
	g_free(err);
	err = NULL;

	for (c = start = tmp ; *c ; c++)
	{
		/* Skip leading spaces. */
		if (c == start && *c == ' ')
			start = c + 1;
		else if (*c == '\n')
		{
			*c = '\0';
			if (g_str_has_prefix(start, "/desktop/gnome/url-handlers/"))
			{
				char *cmd;
				char *tmp2 = NULL;
				char *protocol;

				/* If there is an enabled boolean, honor it. */
				cmd = g_strdup_printf("gconftool-2 -g %s/enabled", start);
				if (g_spawn_command_line_sync(cmd, &tmp2, &err, NULL, NULL))
				{
					g_free(err);
					err = NULL;
					if (!strcmp(tmp2, "false\n"))
					{
						g_free(tmp2);
						g_free(cmd);
						start = c + 1;
						continue;
					}
				}
				g_free(cmd);
				g_free(tmp2);

				start += sizeof("/desktop/gnome/url-handlers/") - 1;

				protocol = g_strdup_printf("%s:", start);
				gnome_url_handlers = g_list_prepend(gnome_url_handlers, protocol);
				gtk_imhtml_class_register_protocol(protocol, url_clicked_cb, link_context_menu);
			}
			start = c + 1;
		}
	}
	g_free(tmp);

	return (gnome_url_handlers != NULL);
}

void pidgin_utils_init(void)
{
	gtk_imhtml_class_register_protocol("http://", url_clicked_cb, link_context_menu);
	gtk_imhtml_class_register_protocol("https://", url_clicked_cb, link_context_menu);
	gtk_imhtml_class_register_protocol("ftp://", url_clicked_cb, link_context_menu);
	gtk_imhtml_class_register_protocol("gopher://", url_clicked_cb, link_context_menu);
	gtk_imhtml_class_register_protocol("mailto:", url_clicked_cb, copy_email_address);

	gtk_imhtml_class_register_protocol("file://", file_clicked_cb, file_context_menu);
	gtk_imhtml_class_register_protocol("audio://", audio_clicked_cb, audio_context_menu);

	/* Example custom URL handler. */
	gtk_imhtml_class_register_protocol("open://", open_dialog, dummy);

	/* If we're under GNOME, try registering the system URL handlers. */
	if (purple_running_gnome())
		register_gnome_url_handlers();
}

void pidgin_utils_uninit(void)
{
	gtk_imhtml_class_register_protocol("open://", NULL, NULL);

	/* If we have GNOME handlers registered, unregister them. */
	if (gnome_url_handlers)
	{
		GList *l;
		for (l = gnome_url_handlers ; l ; l = l->next)
		{
			gtk_imhtml_class_register_protocol((char *)l->data, NULL, NULL);
			g_free(l->data);
		}
		g_list_free(gnome_url_handlers);
		gnome_url_handlers = NULL;
		return;
	}

	gtk_imhtml_class_register_protocol("audio://", NULL, NULL);
	gtk_imhtml_class_register_protocol("file://", NULL, NULL);

	gtk_imhtml_class_register_protocol("http://", NULL, NULL);
	gtk_imhtml_class_register_protocol("https://", NULL, NULL);
	gtk_imhtml_class_register_protocol("ftp://", NULL, NULL);
	gtk_imhtml_class_register_protocol("mailto:", NULL, NULL);
	gtk_imhtml_class_register_protocol("gopher://", NULL, NULL);
}

