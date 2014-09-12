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
 *
 */
#include "internal.h"
#include "glibcompat.h"
#include "pidgin.h"

#include "debug.h"
#include "http.h"
#include "nat-pmp.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "sound.h"
#include "sound-theme.h"
#include "stun.h"
#include "theme-manager.h"
#include "upnp.h"
#include "util.h"
#include "network.h"
#include "keyring.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkconv-theme.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksmiley-theme.h"
#include "gtksound.h"
#include "gtkstatus-icon-theme.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidginstock.h"
#ifdef USE_VV
#include "media-gst.h"
#if GST_CHECK_VERSION(1,0,0)
#include <gst/video/videooverlay.h>
#else
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/propertyprobe.h>
#endif
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_QUARTZ
#include <gdk/gdkquartz.h>
#endif
#endif

#include "gtk3compat.h"

#define PROXYHOST 0
#define PROXYPORT 1
#define PROXYUSER 2
#define PROXYPASS 3

#define PREFS_OPTIMAL_ICON_SIZE 32

/* 25MB */
#define PREFS_MAX_DOWNLOADED_THEME_SIZE 26214400

struct theme_info {
	gchar *type;
	gchar *extension;
	gchar *original_name;
};

/* Main dialog */
static GtkWidget *prefs = NULL;

/* Notebook */
static GtkWidget *prefsnotebook = NULL;
static int notebook_page = 0;

/* Conversations page */
static GtkWidget *sample_webview = NULL;

/* Themes page */
static GtkWidget *prefs_sound_themes_combo_box;
static GtkWidget *prefs_blist_themes_combo_box;
static GtkWidget *prefs_conv_themes_combo_box;
static GtkWidget *prefs_conv_variants_combo_box;
static GtkWidget *prefs_status_themes_combo_box;
static GtkWidget *prefs_smiley_themes_combo_box;
static PurpleHttpConnection *prefs_conv_themes_running_request = NULL;

/* Keyrings page */
static GtkWidget *keyring_page_instance = NULL;
static GtkComboBox *keyring_combo = NULL;
static GtkBox *keyring_vbox = NULL;
static PurpleRequestFields *keyring_settings = NULL;
static GList *keyring_settings_fields = NULL;
static GtkWidget *keyring_apply = NULL;

/* Sound theme specific */
static GtkWidget *sound_entry = NULL;
static int sound_row_sel = 0;
static gboolean prefs_sound_themes_loading;

/* These exist outside the lifetime of the prefs dialog */
static GtkListStore *prefs_sound_themes;
static GtkListStore *prefs_blist_themes;
static GtkListStore *prefs_conv_themes;
static GtkListStore *prefs_conv_variants;
static GtkListStore *prefs_status_icon_themes;
static GtkListStore *prefs_smiley_themes;

#ifdef USE_VV

static const gchar *AUDIO_SRC_PLUGINS[] = {
	"alsasrc",	"ALSA",
	"directsoundsrc", "DirectSound",
	/* "esdmon",	"ESD", ? */
	"osssrc",	"OSS",
	"pulsesrc",	"PulseAudio",
	"sndiosrc",	"sndio",
	/* "audiotestsrc wave=silence", "Silence", */
	/* Translators: This is a noun that refers to one possible audio input
	   plugin. The plugin can be used by the user to sanity check basic audio
	   functionality within Pidgin.  */
	"audiotestsrc",	N_("Test Sound"),
	NULL
};

static const gchar *AUDIO_SINK_PLUGINS[] = {
	"alsasink",	"ALSA",
	"directsoundsink", "DirectSound",
	/* "gconfaudiosink", "GConf", */
	"artsdsink",	"aRts",
	"esdsink",	"ESD",
	"osssink",	"OSS",
	"pulsesink",	"PulseAudio",
	"sndiosink",	"sndio",
	NULL
};

static const gchar *VIDEO_SRC_PLUGINS[] = {
	"videodisabledsrc",	N_("Disabled"),
	/* Translators: This is a noun that refers to one possible video input
	   plugin. The plugin can be used by the user to sanity check basic video
	   functionality within Pidgin. */
	"videotestsrc",	N_("Test Input"),
	"dshowvideosrc","DirectDraw",
	"ksvideosrc",	"KS Video",
	"qcamsrc",	"Quickcam",
	"v4lsrc",	"Video4Linux",
	"v4l2src",	"Video4Linux2",
	"v4lmjpegsrc",	"Video4Linux MJPEG",
	NULL
};

static const gchar *VIDEO_SINK_PLUGINS[] = {
	/* "aasink",	"AALib", Didn't work for me */
	"directdrawsink", "DirectDraw",
	/* "gconfvideosink", "GConf", */
	"glimagesink",	"OpenGL",
	"ximagesink",	"X Window System",
	"xvimagesink",	"X Window System (Xv)",
	NULL
};

static GtkWidget *voice_level;
static GtkWidget *voice_threshold;
static GtkWidget *voice_volume;
static GstElement *voice_pipeline;

static GtkWidget *video_drawing_area;
static GstElement *video_pipeline;

#endif

/*
 * PROTOTYPES
 */
static void delete_prefs(GtkWidget *, void *);

static void
update_spin_value(GtkWidget *w, GtkWidget *spin)
{
	const char *key = g_object_get_data(G_OBJECT(spin), "val");
	int value;

	value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));

	purple_prefs_set_int(key, value);
}

GtkWidget *
pidgin_prefs_labeled_spin_button(GtkWidget *box, const gchar *title,
		const char *key, int min, int max, GtkSizeGroup *sg)
{
	GtkWidget *spin;
	GtkAdjustment *adjust;
	int val;

	val = purple_prefs_get_int(key);

	adjust = GTK_ADJUSTMENT(gtk_adjustment_new(val, min, max, 1, 1, 0));
	spin = gtk_spin_button_new(adjust, 1, 0);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	if (max < 10000)
		gtk_widget_set_size_request(spin, 50, -1);
	else
		gtk_widget_set_size_request(spin, 60, -1);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
					 G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);

	return pidgin_add_widget_to_vbox(GTK_BOX(box), title, sg, spin, FALSE, NULL);
}

static void
entry_set(GtkEntry *entry, gpointer data)
{
	const char *key = (const char*)data;

	purple_prefs_set_string(key, gtk_entry_get_text(entry));
}

GtkWidget *
pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

GtkWidget *
pidgin_prefs_labeled_password(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

/* TODO: Maybe move this up somewheres... */
enum {
	PREF_DROPDOWN_TEXT,
	PREF_DROPDOWN_VALUE,
	PREF_DROPDOWN_COUNT
};

typedef struct
{
	PurplePrefType type;
	union {
		const char *string;
		int integer;
		gboolean boolean;
	} value;
} PidginPrefValue;

typedef void (*PidginPrefsDropdownCallback)(GtkComboBox *combo_box,
	PidginPrefValue value);

static void
dropdown_set(GtkComboBox *combo_box, gpointer _cb)
{
	PidginPrefsDropdownCallback cb = _cb;
	GtkTreeIter iter;
	GtkTreeModel *tree_model;
	PidginPrefValue active;

	tree_model = gtk_combo_box_get_model(combo_box);
	if (!gtk_combo_box_get_active_iter(combo_box, &iter))
		return;
	active.type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo_box),
		"type"));

	g_object_set_data(G_OBJECT(combo_box), "previously_active",
		g_object_get_data(G_OBJECT(combo_box), "current_active"));
	g_object_set_data(G_OBJECT(combo_box), "current_active",
		GINT_TO_POINTER(gtk_combo_box_get_active(combo_box)));

	if (active.type == PURPLE_PREF_INT) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&active.value.integer, -1);
	}
	else if (active.type == PURPLE_PREF_STRING) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&active.value.string, -1);
	}
	else if (active.type == PURPLE_PREF_BOOLEAN) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&active.value.boolean, -1);
	}

	cb(combo_box, active);
}

static void pidgin_prefs_dropdown_revert_active(GtkComboBox *combo_box)
{
	gint previously_active;

	g_return_if_fail(combo_box != NULL);

	previously_active = GPOINTER_TO_INT(g_object_get_data(
		G_OBJECT(combo_box), "previously_active"));
	g_object_set_data(G_OBJECT(combo_box), "current_active",
		GINT_TO_POINTER(previously_active));

	gtk_combo_box_set_active(combo_box, previously_active);
}

static GtkWidget *
pidgin_prefs_dropdown_from_list_with_cb(GtkWidget *box, const gchar *title,
	GtkComboBox **dropdown_out, GList *menuitems,
	PidginPrefValue initial, PidginPrefsDropdownCallback cb)
{
	GtkWidget *dropdown;
	GtkWidget *label = NULL;
	gchar *text;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;
	GtkCellRenderer *renderer;
	gpointer current_active;

	g_return_val_if_fail(menuitems != NULL, NULL);

	if (initial.type == PURPLE_PREF_INT) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_INT);
	} else if (initial.type == PURPLE_PREF_STRING) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_STRING);
	} else if (initial.type == PURPLE_PREF_BOOLEAN) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	} else {
		g_warn_if_reached();
		return NULL;
	}

	dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	if (dropdown_out != NULL)
		*dropdown_out = GTK_COMBO_BOX(dropdown);
	g_object_set_data(G_OBJECT(dropdown), "type", GINT_TO_POINTER(initial.type));

	while (menuitems != NULL && (text = (char *)menuitems->data) != NULL) {
		int int_value  = 0;
		const char *str_value  = NULL;
		gboolean bool_value = FALSE;

		menuitems = g_list_next(menuitems);
		g_return_val_if_fail(menuitems != NULL, NULL);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   PREF_DROPDOWN_TEXT, text,
		                   -1);

		if (initial.type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, int_value,
			                   -1);
		}
		else if (initial.type == PURPLE_PREF_STRING) {
			str_value = (const char *)menuitems->data;
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, str_value,
			                   -1);
		}
		else if (initial.type == PURPLE_PREF_BOOLEAN) {
			bool_value = (gboolean)GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, bool_value,
			                   -1);
		}

		if ((initial.type == PURPLE_PREF_INT &&
			initial.value.integer == int_value) ||
			(initial.type == PURPLE_PREF_STRING &&
			!g_strcmp0(initial.value.string, str_value)) ||
			(initial.type == PURPLE_PREF_BOOLEAN &&
			(initial.value.boolean == bool_value))) {

			active = iter;
		}

		menuitems = g_list_next(menuitems);
	}

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dropdown), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dropdown), renderer,
	                               "text", 0,
	                               NULL);

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dropdown), &active);
	current_active = GINT_TO_POINTER(gtk_combo_box_get_active(GTK_COMBO_BOX(
		dropdown)));
	g_object_set_data(G_OBJECT(dropdown), "current_active", current_active);
	g_object_set_data(G_OBJECT(dropdown), "previously_active", current_active);

	g_signal_connect(G_OBJECT(dropdown), "changed",
		G_CALLBACK(dropdown_set), cb);

	pidgin_add_widget_to_vbox(GTK_BOX(box), title, NULL, dropdown, FALSE, &label);

	return label;
}

static void
pidgin_prefs_dropdown_from_list_cb(GtkComboBox *combo_box,
	PidginPrefValue value)
{
	const char *key;

	key = g_object_get_data(G_OBJECT(combo_box), "key");

	if (value.type == PURPLE_PREF_INT) {
		purple_prefs_set_int(key, value.value.integer);
	} else if (value.type == PURPLE_PREF_STRING) {
		purple_prefs_set_string(key, value.value.string);
	} else if (value.type == PURPLE_PREF_BOOLEAN) {
		purple_prefs_set_bool(key, value.value.boolean);
	} else {
		g_return_if_reached();
	}
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		PurplePrefType type, const char *key, GList *menuitems)
{
	PidginPrefValue initial;
	GtkComboBox *dropdown = NULL;
	GtkWidget *label;

	initial.type = type;
	if (type == PURPLE_PREF_INT) {
		initial.value.integer = purple_prefs_get_int(key);
	} else if (type == PURPLE_PREF_STRING) {
		initial.value.string = purple_prefs_get_string(key);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		initial.value.boolean = purple_prefs_get_bool(key);
	} else {
		g_return_val_if_reached(NULL);
	}

	label = pidgin_prefs_dropdown_from_list_with_cb(box, title, &dropdown,
		menuitems, initial, pidgin_prefs_dropdown_from_list_cb);

	g_object_set_data(G_OBJECT(dropdown), "key", (gpointer)key);

	return label;
}

GtkWidget *
pidgin_prefs_dropdown(GtkWidget *box, const gchar *title, PurplePrefType type,
			   const char *key, ...)
{
	va_list ap;
	GList *menuitems = NULL;
	GtkWidget *dropdown = NULL;
	char *name;
	int int_value;
	const char *str_value;

	g_return_val_if_fail(type == PURPLE_PREF_BOOLEAN || type == PURPLE_PREF_INT ||
			type == PURPLE_PREF_STRING, NULL);

	va_start(ap, key);
	while ((name = va_arg(ap, char *)) != NULL) {

		menuitems = g_list_prepend(menuitems, name);

		if (type == PURPLE_PREF_INT || type == PURPLE_PREF_BOOLEAN) {
			int_value = va_arg(ap, int);
			menuitems = g_list_prepend(menuitems, GINT_TO_POINTER(int_value));
		}
		else {
			str_value = va_arg(ap, const char *);
			menuitems = g_list_prepend(menuitems, (char *)str_value);
		}
	}
	va_end(ap);

	g_return_val_if_fail(menuitems != NULL, NULL);

	menuitems = g_list_reverse(menuitems);

	dropdown = pidgin_prefs_dropdown_from_list(box, title, type, key,
			menuitems);

	g_list_free(menuitems);

	return dropdown;
}

static void keyring_page_cleanup(void);

static void
delete_prefs(GtkWidget *asdf, void *gdsa)
{
	/* Cancel HTTP requests */
	purple_http_conn_cancel(prefs_conv_themes_running_request);
	prefs_conv_themes_running_request = NULL;

	/* Close any "select sound" request dialogs */
	purple_request_close_with_handle(prefs);

	purple_notify_close_with_handle(prefs);

	/* Unregister callbacks. */
	purple_prefs_disconnect_by_handle(prefs);

	/* NULL-ify globals */
	sound_entry = NULL;
	sound_row_sel = 0;
	prefs_sound_themes_loading = FALSE;

	prefs_sound_themes_combo_box = NULL;
	prefs_blist_themes_combo_box = NULL;
	prefs_conv_themes_combo_box = NULL;
	prefs_conv_variants_combo_box = NULL;
	prefs_status_themes_combo_box = NULL;
	prefs_smiley_themes_combo_box = NULL;

	keyring_page_cleanup();

	sample_webview = NULL;

#ifdef USE_VV
	voice_level = NULL;
	voice_threshold = NULL;
	voice_volume = NULL;
	video_drawing_area = NULL;
#endif

	notebook_page = 0;
	prefsnotebook = NULL;
	prefs = NULL;
}

static gchar *
get_theme_markup(const char *name, gboolean custom, const char *author,
				 const char *description)
{

	return g_strdup_printf("<b>%s</b>%s%s%s%s\n<span foreground='dim grey'>%s</span>",
						   name, custom ? " " : "", custom ? _("(Custom)") : "",
						   author != NULL ? " - " : "", author != NULL ? author : "",
						   description != NULL ? description : "");
}

static void
smileys_refresh_theme_list(void)
{
	GList *it;
	GtkTreeIter iter;
	gchar *description;

	description = get_theme_markup(_("none"), FALSE, _("Penguin Pimps"),
		_("Selecting this disables graphical emoticons."));
	gtk_list_store_append(prefs_smiley_themes, &iter);
	gtk_list_store_set(prefs_smiley_themes, &iter,
		0, NULL, 1, description, 2, "none", -1);
	g_free(description);

	for (it = pidgin_smiley_theme_get_all(); it; it = g_list_next(it)) {
		PidginSmileyTheme *theme = it->data;

		description = get_theme_markup(
			_(pidgin_smiley_theme_get_name(theme)), FALSE,
			_(pidgin_smiley_theme_get_author(theme)),
			_(pidgin_smiley_theme_get_description(theme)));

		gtk_list_store_append(prefs_smiley_themes, &iter);
		gtk_list_store_set(prefs_smiley_themes, &iter,
			0, pidgin_smiley_theme_get_icon(theme),
			1, description,
			2, pidgin_smiley_theme_get_name(theme),
			-1);

		g_free(description);
	}
}

/* Rebuild the markup for the sound theme selection for "(Custom)" themes */
static void
pref_sound_generate_markup(void)
{
	gboolean print_custom, customized;
	const gchar *author, *description, *current_theme;
	gchar *name, *markup;
	PurpleSoundTheme *theme;
	GtkTreeIter iter;

	customized = pidgin_sound_is_customized();
	current_theme = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(prefs_sound_themes), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &iter, 2, &name, -1);

			print_custom = customized && name && g_str_equal(current_theme, name);

			if (!name || *name == '\0') {
				g_free(name);
				name = g_strdup(_("Default"));
				author = _("Penguin Pimps");
				description = _("The default Pidgin sound theme");
			} else {
				theme = PURPLE_SOUND_THEME(purple_theme_manager_find_theme(name, "sound"));
				author = purple_theme_get_author(PURPLE_THEME(theme));
				description = purple_theme_get_description(PURPLE_THEME(theme));
			}

			markup = get_theme_markup(name, print_custom, author, description);

			gtk_list_store_set(prefs_sound_themes, &iter, 1, markup, -1);

			g_free(name);
			g_free(markup);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(prefs_sound_themes), &iter));
	}
}

/* adds the themes to the theme list from the manager so they can be displayed in prefs */
static void
prefs_themes_sort(PurpleTheme *theme)
{
	GdkPixbuf *pixbuf = NULL;
	GtkTreeIter iter;
	gchar *image_full = NULL, *markup;
	const gchar *name, *author, *description;

	if (PURPLE_IS_SOUND_THEME(theme)){

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = pidgin_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
			g_free(image_full);
		} else
			pixbuf = NULL;

		gtk_list_store_append(prefs_sound_themes, &iter);
		gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, purple_theme_get_name(theme), -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

	} else if (PIDGIN_IS_BLIST_THEME(theme) || PIDGIN_IS_STATUS_ICON_THEME(theme)){
		GtkListStore *store;

		if (PIDGIN_IS_BLIST_THEME(theme))
			store = prefs_blist_themes;
		else
			store = prefs_status_icon_themes;

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = pidgin_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
			g_free(image_full);
		} else
			pixbuf = NULL;

		name = purple_theme_get_name(theme);
		author = purple_theme_get_author(theme);
		description = purple_theme_get_description(theme);

		markup = get_theme_markup(name, FALSE, author, description);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pixbuf, 1, markup, 2, name, -1);

		g_free(markup);
		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

	} else if (PIDGIN_IS_CONV_THEME(theme)) {
		/* No image available? */

		name = purple_theme_get_name(theme);
		/* No author available */
		/* No description available */

		markup = get_theme_markup(name, FALSE, NULL, NULL);

		gtk_list_store_append(prefs_conv_themes, &iter);
		gtk_list_store_set(prefs_conv_themes, &iter, 1, markup, 2, name, -1);
	}
}

static void
prefs_set_active_theme_combo(GtkWidget *combo_box, GtkListStore *store, const gchar *current_theme)
{
	GtkTreeIter iter;
	gchar *theme = NULL;
	gboolean unset = TRUE;

	if (current_theme && *current_theme && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &theme, -1);

			if (g_str_equal(current_theme, theme)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo_box), &iter);
				unset = FALSE;
			}

			g_free(theme);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
	}

	if (unset)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
}

static void
prefs_themes_refresh(void)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *tmp;
	GtkTreeIter iter;

	prefs_sound_themes_loading = TRUE;
	/* refresh the list of themes in the manager */
	purple_theme_manager_refresh();

	tmp = g_build_filename(PURPLE_DATADIR, "icons", "hicolor", "32x32",
		"apps", "pidgin.png", NULL);
	pixbuf = pidgin_pixbuf_new_from_file_at_scale(tmp, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
	g_free(tmp);

	/* sound themes */
	gtk_list_store_clear(prefs_sound_themes);
	gtk_list_store_append(prefs_sound_themes, &iter);
	gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, "", -1);

	/* blist themes */
	gtk_list_store_clear(prefs_blist_themes);
	gtk_list_store_append(prefs_blist_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin buddy list theme"));
	gtk_list_store_set(prefs_blist_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);

	/* conversation themes */
	gtk_list_store_clear(prefs_conv_themes);
	gtk_list_store_append(prefs_conv_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin conversation theme"));
	gtk_list_store_set(prefs_conv_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);

	/* conversation theme variants */
	gtk_list_store_clear(prefs_conv_variants);

	/* status icon themes */
	gtk_list_store_clear(prefs_status_icon_themes);
	gtk_list_store_append(prefs_status_icon_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin status icon theme"));
	gtk_list_store_set(prefs_status_icon_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);
	if (pixbuf)
		g_object_unref(G_OBJECT(pixbuf));

	/* smiley themes */
	gtk_list_store_clear(prefs_smiley_themes);

	purple_theme_manager_for_each_theme(prefs_themes_sort);
	pref_sound_generate_markup();
	smileys_refresh_theme_list();

	/* set active */
	prefs_set_active_theme_combo(prefs_sound_themes_combo_box, prefs_sound_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme"));
	prefs_set_active_theme_combo(prefs_blist_themes_combo_box, prefs_blist_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/theme"));
	prefs_set_active_theme_combo(prefs_conv_themes_combo_box, prefs_conv_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/theme"));
	prefs_set_active_theme_combo(prefs_status_themes_combo_box, prefs_status_icon_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/status/icon-theme"));
	prefs_set_active_theme_combo(prefs_smiley_themes_combo_box, prefs_smiley_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/smileys/theme"));
	prefs_sound_themes_loading = FALSE;
}

/* init all the theme variables so that the themes can be sorted later and used by pref pages */
static void
prefs_themes_init(void)
{
	prefs_sound_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_blist_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_conv_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_conv_variants = gtk_list_store_new(1, G_TYPE_STRING);

	prefs_status_icon_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_smiley_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
}

/*
 * prefs_theme_find_theme:
 * @path: A directory containing a theme.  The theme could be at the
 *        top level of this directory or in any subdirectory thereof.
 * @type: The type of theme to load.  The loader for this theme type
 *        will be used and this loader will determine what constitutes a
 *        "theme."
 *
 * Attempt to load the given directory as a theme.  If we are unable to
 * open the path as a theme then we recurse into path and attempt to
 * load each subdirectory that we encounter.
 *
 * Returns: A new reference to a #PurpleTheme.
 */
static PurpleTheme *
prefs_theme_find_theme(const gchar *path, const gchar *type)
{
	PurpleTheme *theme = purple_theme_manager_load_theme(path, type);
	GDir *dir = g_dir_open(path, 0, NULL);
	const gchar *next;

	while (!PURPLE_IS_THEME(theme) && (next = g_dir_read_name(dir))) {
		gchar *next_path = g_build_filename(path, next, NULL);

		if (g_file_test(next_path, G_FILE_TEST_IS_DIR))
			theme = prefs_theme_find_theme(next_path, type);

		g_free(next_path);
	}

	g_dir_close(dir);

	return theme;
}

/* Eww. Seriously ewww. But thanks, grim! This is taken from guifications2 */
static gboolean
purple_theme_file_copy(const gchar *source, const gchar *destination)
{
	FILE *src, *dest;
	gint chr = EOF;

	if(!(src = g_fopen(source, "rb")))
		return FALSE;
	if(!(dest = g_fopen(destination, "wb"))) {
		fclose(src);
		return FALSE;
	}

	while((chr = fgetc(src)) != EOF) {
		fputc(chr, dest);
	}

	fclose(dest);
	fclose(src);

	return TRUE;
}

static void
free_theme_info(struct theme_info *info)
{
	if (info != NULL) {
		g_free(info->type);
		g_free(info->extension);
		g_free(info->original_name);
		g_free(info);
	}
}

/* installs a theme, info is freed by function */
static void
theme_install_theme(char *path, struct theme_info *info)
{
#ifndef _WIN32
	gchar *command;
#endif
	gchar *destdir;
	const char *tail;
	gboolean is_smiley_theme, is_archive;
	PurpleTheme *theme = NULL;

	if (info == NULL)
		return;

	/* check the extension */
	tail = info->extension ? info->extension : strrchr(path, '.');

	if (!tail) {
		free_theme_info(info);
		return;
	}

	is_archive = !g_ascii_strcasecmp(tail, ".gz") || !g_ascii_strcasecmp(tail, ".tgz");

	/* Just to be safe */
	g_strchomp(path);

	if ((is_smiley_theme = g_str_equal(info->type, "smiley")))
		destdir = g_build_filename(purple_user_dir(), "smileys", NULL);
	else
		destdir = g_build_filename(purple_user_dir(), "themes", "temp", NULL);

	/* We'll check this just to make sure. This also lets us do something different on
	 * other platforms, if need be */
	if (is_archive) {
#ifndef _WIN32
		gchar *path_escaped = g_shell_quote(path);
		gchar *destdir_escaped = g_shell_quote(destdir);

		if (!g_file_test(destdir, G_FILE_TEST_IS_DIR))
			purple_build_dir(destdir, S_IRUSR | S_IWUSR | S_IXUSR);

		command = g_strdup_printf("tar > /dev/null xzf %s -C %s", path_escaped, destdir_escaped);
		g_free(path_escaped);
		g_free(destdir_escaped);

		/* Fire! */
		if (system(command)) {
			purple_notify_error(NULL, NULL, _("Theme failed to unpack."), NULL, NULL);
			g_free(command);
			g_free(destdir);
			free_theme_info(info);
			return;
		}
#else
		if (!winpidgin_gz_untar(path, destdir)) {
			purple_notify_error(NULL, NULL, _("Theme failed to unpack."), NULL, NULL);
			g_free(destdir);
			free_theme_info(info);
			return;
		}
#endif
	}

	if (is_smiley_theme) {
		/* just extract the folder to the smiley directory */
		prefs_themes_refresh();

	} else if (is_archive) {
		theme = prefs_theme_find_theme(destdir, info->type);

		if (PURPLE_IS_THEME(theme)) {
			/* create the location for the theme */
			gchar *theme_dest = g_build_filename(purple_user_dir(), "themes",
						 purple_theme_get_name(theme),
						 "purple", info->type, NULL);

			if (!g_file_test(theme_dest, G_FILE_TEST_IS_DIR))
				purple_build_dir(theme_dest, S_IRUSR | S_IWUSR | S_IXUSR);

			g_free(theme_dest);
			theme_dest = g_build_filename(purple_user_dir(), "themes",
						 purple_theme_get_name(theme),
						 "purple", info->type, NULL);

			/* move the entire directory to new location */
			if (g_rename(purple_theme_get_dir(theme), theme_dest)) {
				purple_debug_error("gtkprefs", "Error renaming %s to %s: "
						"%s\n", purple_theme_get_dir(theme), theme_dest,
						g_strerror(errno));
			}

			g_free(theme_dest);
			if (g_remove(destdir) != 0) {
				purple_debug_error("gtkprefs",
					"couldn't remove temp (dest) path\n");
			}
			g_object_unref(theme);

			prefs_themes_refresh();

		} else {
			/* something was wrong with the theme archive */
			g_unlink(destdir);
			purple_notify_error(NULL, NULL, _("Theme failed to load."), NULL, NULL);
		}

	} else { /* just a single file so copy it to a new temp directory and attempt to load it*/
		gchar *temp_path, *temp_file;

		temp_path = g_build_filename(purple_user_dir(), "themes", "temp", "sub_folder", NULL);

		if (info->original_name != NULL) {
			/* name was changed from the original (probably a dnd) change it back before loading */
			temp_file = g_build_filename(temp_path, info->original_name, NULL);

		} else {
			gchar *source_name = g_path_get_basename(path);
			temp_file = g_build_filename(temp_path, source_name, NULL);
			g_free(source_name);
		}

		if (!g_file_test(temp_path, G_FILE_TEST_IS_DIR))
			purple_build_dir(temp_path, S_IRUSR | S_IWUSR | S_IXUSR);

		if (purple_theme_file_copy(path, temp_file)) {
			/* find the theme, could be in subfolder */
			theme = prefs_theme_find_theme(temp_path, info->type);

			if (PURPLE_IS_THEME(theme)) {
				gchar *theme_dest = g_build_filename(purple_user_dir(), "themes",
							 purple_theme_get_name(theme),
							 "purple", info->type, NULL);

				if(!g_file_test(theme_dest, G_FILE_TEST_IS_DIR))
					purple_build_dir(theme_dest, S_IRUSR | S_IWUSR | S_IXUSR);

				if (g_rename(purple_theme_get_dir(theme), theme_dest)) {
					purple_debug_error("gtkprefs", "Error renaming %s to %s: "
							"%s\n", purple_theme_get_dir(theme), theme_dest,
							g_strerror(errno));
				}

				g_free(theme_dest);
				g_object_unref(theme);

				prefs_themes_refresh();
			} else {
				if (g_remove(temp_path) != 0) {
					purple_debug_error("gtkprefs",
						"couldn't remove temp path");
				}
				purple_notify_error(NULL, NULL, _("Theme failed to load."), NULL, NULL);
			}
		} else {
			purple_notify_error(NULL, NULL, _("Theme failed to copy."), NULL, NULL);
		}

		g_free(temp_file);
		g_free(temp_path);
	}

	g_free(destdir);
	free_theme_info(info);
}

static void
theme_got_url(PurpleHttpConnection *http_conn, PurpleHttpResponse *response,
	gpointer _info)
{
	struct theme_info *info = _info;
	const gchar *themedata;
	size_t len;
	FILE *f;
	gchar *path;
	size_t wc;

	g_assert(http_conn == prefs_conv_themes_running_request);
	prefs_conv_themes_running_request = NULL;

	if (!purple_http_response_is_successful(response)) {
		free_theme_info(info);
		return;
	}

	themedata = purple_http_response_get_data(response, &len);

	f = purple_mkstemp(&path, TRUE);
	wc = fwrite(themedata, len, 1, f);
	if (wc != 1) {
		purple_debug_warning("theme_got_url", "Unable to write theme data.\n");
		fclose(f);
		g_unlink(path);
		g_free(path);
		free_theme_info(info);
		return;
	}
	fclose(f);

	theme_install_theme(path, info);

	g_unlink(path);
	g_free(path);
}

static void
theme_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
		GtkSelectionData *sd, guint info, guint t, gpointer user_data)
{
	gchar *name = g_strchomp((gchar *)gtk_selection_data_get_data(sd));

	if ((gtk_selection_data_get_length(sd) >= 0)
	 && (gtk_selection_data_get_format(sd) == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */
		gchar *temp;
		struct theme_info *info =  g_new0(struct theme_info, 1);
		info->type = g_strdup((gchar *)user_data);
		info->extension = g_strdup(g_strrstr(name,"."));
		temp = g_strrstr(name, "/");
		info->original_name = temp ? g_strdup(++temp) : NULL;

		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp;
			/* It looks like we're dealing with a local file. Let's
			 * just untar it in the right place */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug(PURPLE_DEBUG_ERROR, "theme dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				free_theme_info(info);
				return;
			}
			theme_install_theme(tmp, info);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7) ||
			!g_ascii_strncasecmp(name, "https://", 8)) {
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			PurpleHttpRequest *hr;
			purple_http_conn_cancel(prefs_conv_themes_running_request);

			hr = purple_http_request_new(name);
			purple_http_request_set_max_len(hr,
				PREFS_MAX_DOWNLOADED_THEME_SIZE);
			prefs_conv_themes_running_request = purple_http_request(
				NULL, hr, theme_got_url, info);
			purple_http_request_unref(hr);
		} else
			free_theme_info(info);

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}

/* builds a theme combo box from a list store with colums: icon preview, markup, theme name */
static GtkWidget *
prefs_build_theme_combo_box(GtkListStore *store, const char *current_theme, const char *type)
{
	GtkCellRenderer *cell_rend;
	GtkWidget *combo_box;
	GtkTargetEntry te[3] = {
		{"text/plain", 0, 0},
		{"text/uri-list", 0, 1},
		{"STRING", 0, 2}
	};

	g_return_val_if_fail(store != NULL && current_theme != NULL, NULL);

	combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

	cell_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_fixed_size(cell_rend, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combo_box), cell_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "pixbuf", 0, NULL);

	cell_rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combo_box), cell_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "markup", 1, NULL);
	g_object_set(cell_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_drag_dest_set(combo_box, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, te,
					sizeof(te) / sizeof(GtkTargetEntry) , GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(combo_box), "drag_data_received", G_CALLBACK(theme_dnd_recv), (gpointer) type);

	return combo_box;
}

/* sets the current sound theme */
static void
prefs_set_sound_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	gint i;
	gchar *pref;
	gchar *new_theme;
	GtkTreeIter new_iter;

	if(gtk_combo_box_get_active_iter(combo_box, &new_iter) && !prefs_sound_themes_loading) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &new_iter, 2, &new_theme, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/sound/theme", new_theme);

		/* New theme removes all customization */
		for(i = 0; i < PURPLE_NUM_SOUNDS; i++){
			pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						pidgin_sound_get_event_option(i));
			purple_prefs_set_path(pref, "");
			g_free(pref);
		}

		/* gets rid of the "(Custom)" from the last selection */
		pref_sound_generate_markup();

		gtk_entry_set_text(GTK_ENTRY(sound_entry), _("(default)"));

		g_free(new_theme);
	}
}

/* sets the current smiley theme */
static void
prefs_set_smiley_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	gchar *new_theme;
	GtkTreeIter new_iter;

	if (gtk_combo_box_get_active_iter(combo_box, &new_iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_smiley_themes), &new_iter, 2, &new_theme, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/smileys/theme", new_theme);

#if 0
		/* TODO: update smileys in sample_webview input box. */
		update_smileys_in_webview_input_box(sample_webview);
#endif

		g_free(new_theme);
	}
}


/* Does same as normal sort, except "none" is sorted first */
static gint pidgin_sort_smileys (GtkTreeModel	*model,
						GtkTreeIter		*a,
						GtkTreeIter		*b,
						gpointer		userdata)
{
	gint ret = 0;
	gchar *name1 = NULL, *name2 = NULL;

	gtk_tree_model_get(model, a, 2, &name1, -1);
	gtk_tree_model_get(model, b, 2, &name2, -1);

	if (name1 == NULL || name2 == NULL) {
		if (!(name1 == NULL && name2 == NULL))
			ret = (name1 == NULL) ? -1: 1;
	} else if (!g_ascii_strcasecmp(name1, "none")) {
		if (!g_utf8_collate(name1, name2))
			ret = 0;
		else
			/* Sort name1 first */
			ret = -1;
	} else if (!g_ascii_strcasecmp(name2, "none")) {
		/* Sort name2 first */
		ret = 1;
	} else {
		/* Neither string is "none", default to normal sort */
		ret = purple_utf8_strcasecmp(name1, name2);
	}

	g_free(name1);
	g_free(name2);

	return ret;
}

/* sets the current buddy list theme */
static void
prefs_set_blist_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginBlistTheme *theme =  NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if(gtk_combo_box_get_active_iter(combo_box, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_blist_themes), &iter, 2, &name, -1);

		if(!name || !g_str_equal(name, ""))
			theme = PIDGIN_BLIST_THEME(purple_theme_manager_find_theme(name, "blist"));

		g_free(name);

		pidgin_blist_set_theme(theme);
	}
}

/* sets the current conversation theme variant */
static void
prefs_set_conv_variant_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginConvTheme *theme =  NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(prefs_conv_themes_combo_box), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(prefs_conv_themes), &iter, 2, &name, -1);
		if (name && *name)
			theme = PIDGIN_CONV_THEME(purple_theme_manager_find_theme(name, "conversation"));
		else
			theme = PIDGIN_CONV_THEME(pidgin_conversations_get_default_theme());
		g_free(name);

		if (gtk_combo_box_get_active_iter(combo_box, &iter)) {
			gtk_tree_model_get(GTK_TREE_MODEL(prefs_conv_variants), &iter, 0, &name, -1);
			pidgin_conversation_theme_set_variant(theme, name);
			g_free(name);
		}
	}
}

/* sets the current conversation theme */
static void
prefs_set_conv_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo_box, &iter)) {
		gchar *name = NULL;
		PidginConvTheme *theme;
		const char *current_variant;
		const GList *variants;
		gboolean unset = TRUE;

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_conv_themes), &iter, 2, &name, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/theme", name);

		g_signal_handlers_block_by_func(prefs_conv_variants_combo_box,
		                                prefs_set_conv_variant_cb, NULL);

		/* Update list of variants */
		gtk_list_store_clear(prefs_conv_variants);

		if (name && *name)
			theme = PIDGIN_CONV_THEME(purple_theme_manager_find_theme(name, "conversation"));
		else
			theme = PIDGIN_CONV_THEME(pidgin_conversations_get_default_theme());

		current_variant = pidgin_conversation_theme_get_variant(theme);

		variants = pidgin_conversation_theme_get_variants(theme);
		for (; variants && current_variant; variants = g_list_next(variants)) {
			gtk_list_store_append(prefs_conv_variants, &iter);
			gtk_list_store_set(prefs_conv_variants, &iter, 0, variants->data, -1);
	
			if (g_str_equal(variants->data, current_variant)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(prefs_conv_variants_combo_box), &iter);
				unset = FALSE;
			}
		}

		if (unset)
			gtk_combo_box_set_active(GTK_COMBO_BOX(prefs_conv_variants_combo_box), 0);

		g_signal_handlers_unblock_by_func(prefs_conv_variants_combo_box,
		                                  prefs_set_conv_variant_cb, NULL);
		g_free(name);
	}
}

/* sets the current icon theme */
static void
prefs_set_status_icon_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginStatusIconTheme *theme = NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if(gtk_combo_box_get_active_iter(combo_box, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_status_icon_themes), &iter, 2, &name, -1);

		if(!name || !g_str_equal(name, ""))
			theme = PIDGIN_STATUS_ICON_THEME(purple_theme_manager_find_theme(name, "status-icon"));

		g_free(name);

		pidgin_stock_load_status_icon_theme(theme);
		pidgin_blist_refresh(purple_blist_get_buddy_list());
	}
}

static GtkWidget *
add_theme_prefs_combo(GtkWidget *vbox,
                      GtkSizeGroup *combo_sg, GtkSizeGroup *label_sg,
                      GtkListStore *theme_store,
                      GCallback combo_box_cb, gpointer combo_box_cb_user_data,
                      const char *label_str, const char *prefs_path,
                      const char *theme_type)
{
	GtkWidget *label;
	GtkWidget *combo_box = NULL;
	GtkWidget *themesel_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);

	label = gtk_label_new(label_str);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(label_sg, label);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), label, FALSE, FALSE, 0);

	combo_box = prefs_build_theme_combo_box(theme_store,
						purple_prefs_get_string(prefs_path),
						theme_type);
	g_signal_connect(G_OBJECT(combo_box), "changed",
						(GCallback)combo_box_cb, combo_box_cb_user_data);
	gtk_size_group_add_widget(combo_sg, combo_box);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), combo_box, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), themesel_hbox, FALSE, FALSE, 0);

	return combo_box;
}

static GtkWidget *
add_child_theme_prefs_combo(GtkWidget *vbox, GtkSizeGroup *combo_sg,
                             GtkSizeGroup *label_sg, GtkListStore *theme_store,
                             GCallback combo_box_cb, gpointer combo_box_cb_user_data,
                             const char *label_str)
{
	GtkWidget *label;
	GtkWidget *combo_box;
	GtkWidget *themesel_hbox;
	GtkCellRenderer *cell_rend;

	themesel_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), themesel_hbox, FALSE, FALSE, 0);

	label = gtk_label_new(label_str);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_size_group_add_widget(label_sg, label);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), label, FALSE, FALSE, 0);

	combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(theme_store));

	cell_rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), cell_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "text", 0, NULL);
	g_object_set(cell_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	g_signal_connect(G_OBJECT(combo_box), "changed",
						(GCallback)combo_box_cb, combo_box_cb_user_data);
	gtk_size_group_add_widget(combo_sg, combo_box);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), combo_box, TRUE, TRUE, 0);

	return combo_box;
}

static GtkWidget *
theme_page(void)
{
	GtkWidget *label;
	GtkWidget *ret, *vbox;
	GtkSizeGroup *label_sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup *combo_sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Theme Selections"));

	/* Instructions */
	label = gtk_label_new(_("Select a theme that you would like to use from "
							"the lists below.\nNew themes can be installed by "
							"dragging and dropping them onto the theme list."));

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);
	gtk_widget_show(label);

	/* Buddy List Themes */
	prefs_blist_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_blist_themes,
		(GCallback)prefs_set_blist_theme_cb, NULL,
		_("Buddy List Theme:"), PIDGIN_PREFS_ROOT "/blist/theme", "blist");

	/* Conversation Themes */
	prefs_conv_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_conv_themes,
		(GCallback)prefs_set_conv_theme_cb, NULL,
		_("Conversation Theme:"), PIDGIN_PREFS_ROOT "/conversations/theme", "conversation");

	/* Conversation Theme Variants */
	prefs_conv_variants_combo_box = add_child_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_conv_variants,
		(GCallback)prefs_set_conv_variant_cb, NULL, _("\tVariant:"));

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(prefs_conv_variants),
	                                     0, GTK_SORT_ASCENDING);

	/* Status Icon Themes */
	prefs_status_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_status_icon_themes,
		(GCallback)prefs_set_status_icon_theme_cb, NULL,
		_("Status Icon Theme:"), PIDGIN_PREFS_ROOT "/status/icon-theme", "icon");

	/* Sound Themes */
	prefs_sound_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_sound_themes,
		(GCallback)prefs_set_sound_theme_cb, NULL,
		_("Sound Theme:"), PIDGIN_PREFS_ROOT "/sound/theme", "sound");

	/* Smiley Themes */
	prefs_smiley_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_smiley_themes,
		(GCallback)prefs_set_smiley_theme_cb, NULL,
		_("Smiley Theme:"), PIDGIN_PREFS_ROOT "/smileys/theme", "smiley");

	/* Custom sort so "none" theme is at top of list */
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(prefs_smiley_themes),
	                                2, pidgin_sort_smileys, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(prefs_smiley_themes),
										 2, GTK_SORT_ASCENDING);

	gtk_widget_show_all(ret);

	return ret;
}

static void
formatting_toggle_cb(PidginWebView *webview, PidginWebViewButtons buttons, void *data)
{
	gboolean bold, italic, uline, strike;

	pidgin_webview_get_current_format(webview, &bold, &italic, &uline, &strike);

	if (buttons & PIDGIN_WEBVIEW_BOLD)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold",
		                      bold);
	if (buttons & PIDGIN_WEBVIEW_ITALIC)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic",
		                      italic);
	if (buttons & PIDGIN_WEBVIEW_UNDERLINE)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline",
		                      uline);
	if (buttons & PIDGIN_WEBVIEW_STRIKE)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike",
		                      strike);

	if (buttons & PIDGIN_WEBVIEW_GROW || buttons & PIDGIN_WEBVIEW_SHRINK)
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/font_size",
		                     pidgin_webview_get_current_fontsize(webview));
	if (buttons & PIDGIN_WEBVIEW_FACE) {
		char *face = pidgin_webview_get_current_fontface(webview);

		if (face)
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", face);
		else
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");

		g_free(face);
	}

	if (buttons & PIDGIN_WEBVIEW_FORECOLOR) {
		char *color = pidgin_webview_get_current_forecolor(webview);

		if (color)
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", color);
		else
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");

		g_free(color);
	}

	if (buttons & PIDGIN_WEBVIEW_BACKCOLOR) {
		char *color = pidgin_webview_get_current_backcolor(webview);

		if (color)
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", color);
		else
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");

		g_free(color);
	}
}

static void
formatting_clear_cb(PidginWebView *webview, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike", FALSE);

	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/font_size", 3);

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");
}

static void
conversation_usetabs_cb(const char *name, PurplePrefType type,
						gconstpointer value, gpointer data)
{
	gboolean usetabs = GPOINTER_TO_INT(value);

	if (usetabs)
		gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
	else
		gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
}


#define CONVERSATION_CLOSE_ACCEL_PATH "<Actions>/ConversationActions/Close"

/* Filled in in keyboard_shortcuts(). */
static GtkAccelKey ctrl_w = { 0, 0, 0 };
static GtkAccelKey escape = { 0, 0, 0 };

static guint escape_closes_conversation_cb_id = 0;

static gboolean
accel_is_escape(GtkAccelKey *k)
{
	return (k->accel_key == escape.accel_key
		&& k->accel_mods == escape.accel_mods);
}

/* Update the tickybox in Preferences when the keybinding for Conversation ->
 * Close is changed via Gtk.
 */
static void
conversation_close_accel_changed_cb (GtkAccelMap    *object,
                                     gchar          *accel_path,
                                     guint           accel_key,
                                     GdkModifierType accel_mods,
                                     gpointer        checkbox_)
{
	GtkToggleButton *checkbox = GTK_TOGGLE_BUTTON(checkbox_);
	GtkAccelKey new = { accel_key, accel_mods, 0 };

	g_signal_handler_block(checkbox, escape_closes_conversation_cb_id);
	gtk_toggle_button_set_active(checkbox, accel_is_escape(&new));
	g_signal_handler_unblock(checkbox, escape_closes_conversation_cb_id);
}


static void
escape_closes_conversation_cb(GtkWidget *w,
                              gpointer unused)
{
	gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
	gboolean changed;
	GtkAccelKey *new_key = active ? &escape : &ctrl_w;

	changed = gtk_accel_map_change_entry(CONVERSATION_CLOSE_ACCEL_PATH,
		new_key->accel_key, new_key->accel_mods, TRUE);

	/* If another path is already bound to the new accelerator,
	 * _change_entry tries to delete that binding (because it was passed
	 * replace=TRUE).  If that other path is locked, then _change_entry
	 * will fail.  We don't ever lock any accelerator paths, so this case
	 * should never arise.
	 */
	if(!changed)
		purple_debug_warning("gtkprefs", "Escape accel failed to change\n");
}


/* Creates preferences for keyboard shortcuts that it's hard to change with the
 * standard Gtk accelerator-changing mechanism.
 */
static void
keyboard_shortcuts(GtkWidget *page)
{
	GtkWidget *vbox = pidgin_make_frame(page, _("Keyboard Shortcuts"));
	GtkWidget *checkbox;
	GtkAccelKey current = { 0, 0, 0 };
	GtkAccelMap *map = gtk_accel_map_get();

	/* Maybe it would be better just to hardcode the values?
	 * -- resiak, 2007-04-30
	 */
	if (ctrl_w.accel_key == 0)
	{
		gtk_accelerator_parse ("<Control>w", &(ctrl_w.accel_key),
			&(ctrl_w.accel_mods));
		g_assert(ctrl_w.accel_key != 0);

		gtk_accelerator_parse ("Escape", &(escape.accel_key),
			&(escape.accel_mods));
		g_assert(escape.accel_key != 0);
	}

	checkbox = gtk_check_button_new_with_mnemonic(
		_("Cl_ose conversations with the Escape key"));
	gtk_accel_map_lookup_entry(CONVERSATION_CLOSE_ACCEL_PATH, &current);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
		accel_is_escape(&current));

	escape_closes_conversation_cb_id = g_signal_connect(checkbox,
		"clicked", G_CALLBACK(escape_closes_conversation_cb), NULL);

	g_signal_connect_object(map, "changed::" CONVERSATION_CLOSE_ACCEL_PATH,
		G_CALLBACK(conversation_close_accel_changed_cb), checkbox, (GConnectFlags)0);

	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
}

static GtkWidget *
interface_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkSizeGroup *sg;
	GList *names = NULL;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* System Tray */
	vbox = pidgin_make_frame(ret, _("System Tray Icon"));
	label = pidgin_prefs_dropdown(vbox, _("_Show system tray icon:"), PURPLE_PREF_STRING,
					PIDGIN_PREFS_ROOT "/docklet/show",
					_("Always"), "always",
					_("On unread messages"), "pending",
					_("Never"), "never",
					NULL);
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	vbox = pidgin_make_frame(ret, _("Conversation Window"));
	label = pidgin_prefs_dropdown(vbox, _("_Hide new IM conversations:"),
					PURPLE_PREF_STRING, PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
					_("Never"), "never",
					_("When away"), "away",
					_("Always"), "always",
					NULL);
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

#ifdef _WIN32
	pidgin_prefs_checkbox(_("Minimi_ze new conversation windows"), PIDGIN_PREFS_ROOT "/win32/minimize_new_convs", vbox);
#endif

	/* All the tab options! */
	vbox = pidgin_make_frame(ret, _("Tabs"));

	pidgin_prefs_checkbox(_("Show IMs and chats in _tabbed windows"),
							PIDGIN_PREFS_ROOT "/conversations/tabs", vbox);

	/*
	 * Connect a signal to the above preference.  When conversations are not
	 * shown in a tabbed window then all tabbing options should be disabled.
	 */
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/conversations/tabs",
	                            conversation_usetabs_cb, vbox2);
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"))
		gtk_widget_set_sensitive(vbox2, FALSE);

	pidgin_prefs_checkbox(_("Show close b_utton on tabs"),
				PIDGIN_PREFS_ROOT "/conversations/close_on_tabs", vbox2);

	label = pidgin_prefs_dropdown(vbox2, _("_Placement:"), PURPLE_PREF_INT,
					PIDGIN_PREFS_ROOT "/conversations/tab_side",
					_("Top"), GTK_POS_TOP,
					_("Bottom"), GTK_POS_BOTTOM,
					_("Left"), GTK_POS_LEFT,
					_("Right"), GTK_POS_RIGHT,
					_("Left Vertical"), GTK_POS_LEFT|8,
					_("Right Vertical"), GTK_POS_RIGHT|8,
					NULL);
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	names = pidgin_conv_placement_get_options();
	label = pidgin_prefs_dropdown_from_list(vbox2, _("N_ew conversations:"),
				PURPLE_PREF_STRING, PIDGIN_PREFS_ROOT "/conversations/placement", names);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	gtk_size_group_add_widget(sg, label);

	g_list_free(names);

	keyboard_shortcuts(ret);

	gtk_widget_show_all(ret);
	g_object_unref(sg);
	return ret;
}

#ifdef _WIN32
static void
apply_custom_font(void)
{
	PangoFontDescription *desc = NULL;
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font")) {
		const char *font = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
		desc = pango_font_description_from_string(font);
	}

	gtk_widget_modify_font(sample_webview, desc);
	if (desc)
		pango_font_description_free(desc);

}
static void
pidgin_custom_font_set(GtkFontButton *font_button, gpointer nul)
{

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/custom_font",
				gtk_font_button_get_font_name(font_button));

	apply_custom_font();
}
#endif

static GtkWidget *
conv_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *iconpref1;
	GtkWidget *iconpref2;
	GtkWidget *webview;
	GtkWidget *frame;
#if 0
	GtkWidget *hbox;
	GtkWidget *checkbox;
	GtkWidget *spin_button;
#endif

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Conversations"));

	pidgin_prefs_dropdown(vbox, _("Chat notification:"),
		PURPLE_PREF_INT, PIDGIN_PREFS_ROOT "/conversations/notification_chat",
		_("On unseen events"), PIDGIN_UNSEEN_EVENT,
		_("On unseen text"), PIDGIN_UNSEEN_TEXT,
		_("On unseen text and the nick was said"), PIDGIN_UNSEEN_NICK,
		NULL);

	pidgin_prefs_checkbox(_("Show _formatting on incoming messages"),
				PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting", vbox);
	pidgin_prefs_checkbox(_("Close IMs immediately when the tab is closed"),
				PIDGIN_PREFS_ROOT "/conversations/im/close_immediately", vbox);

	iconpref1 = pidgin_prefs_checkbox(_("Show _detailed information"),
			PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons", vbox);
	iconpref2 = pidgin_prefs_checkbox(_("Enable buddy ic_on animation"),
			PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons", vbox);
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		gtk_widget_set_sensitive(iconpref2, FALSE);
	g_signal_connect(G_OBJECT(iconpref1), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), iconpref2);

	pidgin_prefs_checkbox(_("_Notify buddies that you are typing to them"),
			"/purple/conversations/im/send_typing", vbox);
	pidgin_prefs_checkbox(_("Highlight _misspelled words"),
			PIDGIN_PREFS_ROOT "/conversations/spellcheck", vbox);

	pidgin_prefs_checkbox(_("Use smooth-scrolling"), PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling", vbox);

#ifdef _WIN32
	pidgin_prefs_checkbox(_("F_lash window when IMs are received"), PIDGIN_PREFS_ROOT "/win32/blink_im", vbox);
#endif

#if 0
	/* TODO: it's not implemented */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);

	checkbox = pidgin_prefs_checkbox(_("Resize incoming custom smileys"),
			PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys", hbox);

	spin_button = pidgin_prefs_labeled_spin_button(hbox,
		_("Maximum size:"),
		PIDGIN_PREFS_ROOT "/conversations/custom_smileys_size",
		16, 512, NULL);

	if (!purple_prefs_get_bool(
				PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);

	g_signal_connect(G_OBJECT(checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), NULL, NULL, hbox, TRUE, NULL);
#endif

	pidgin_prefs_labeled_spin_button(vbox,
		_("Minimum input area height in lines:"),
		PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines",
		1, 8, NULL);

#ifdef _WIN32
	{
	GtkWidget *fontpref, *font_button, *hbox;
	const char *font_name;
	vbox = pidgin_make_frame(ret, _("Font"));

	fontpref = pidgin_prefs_checkbox(_("Use font from _theme"),
									 PIDGIN_PREFS_ROOT "/conversations/use_theme_font", vbox);

	font_name = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
	if ((font_name == NULL) || (*font_name == '\0')) {
		font_button = gtk_font_button_new();
	} else {
		font_button = gtk_font_button_new_with_font(font_name);
	}

	gtk_font_button_set_show_style(GTK_FONT_BUTTON(font_button), TRUE);
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Conversation _font:"), NULL, font_button, FALSE, NULL);
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font"))
		gtk_widget_set_sensitive(hbox, FALSE);
	g_signal_connect(G_OBJECT(fontpref), "clicked", G_CALLBACK(pidgin_toggle_sensitive), hbox);
	g_signal_connect(G_OBJECT(fontpref), "clicked", G_CALLBACK(apply_custom_font), hbox);
	g_signal_connect(G_OBJECT(font_button), "font-set", G_CALLBACK(pidgin_custom_font_set), NULL);

	}
#endif

	vbox = pidgin_make_frame(ret, _("Default Formatting"));

	frame = pidgin_create_webview(TRUE, &webview, NULL);
	gtk_widget_show(frame);
	gtk_widget_set_name(webview, "pidgin_prefs_font_webview");
	gtk_widget_set_size_request(frame, 450, -1);
	pidgin_webview_set_whole_buffer_formatting_only(PIDGIN_WEBVIEW(webview), TRUE);
	pidgin_webview_set_format_functions(PIDGIN_WEBVIEW(webview),
	                                 PIDGIN_WEBVIEW_BOLD |
	                                 PIDGIN_WEBVIEW_ITALIC |
	                                 PIDGIN_WEBVIEW_UNDERLINE |
	                                 PIDGIN_WEBVIEW_STRIKE |
	                                 PIDGIN_WEBVIEW_GROW |
	                                 PIDGIN_WEBVIEW_SHRINK |
	                                 PIDGIN_WEBVIEW_FACE |
	                                 PIDGIN_WEBVIEW_FORECOLOR |
	                                 PIDGIN_WEBVIEW_BACKCOLOR);

	pidgin_webview_append_html(PIDGIN_WEBVIEW(webview),
	                        _("This is how your outgoing message text will "
	                          "appear when you use protocols that support "
	                          "formatting."));

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	pidgin_webview_setup_entry(PIDGIN_WEBVIEW(webview),
	                        PURPLE_CONNECTION_FLAG_HTML |
	                        PURPLE_CONNECTION_FLAG_FORMATTING_WBFO);

	g_signal_connect_after(G_OBJECT(webview), "format-toggled",
	                       G_CALLBACK(formatting_toggle_cb), NULL);
	g_signal_connect_after(G_OBJECT(webview), "format-cleared",
	                       G_CALLBACK(formatting_clear_cb), NULL);
	sample_webview = webview;

	gtk_widget_show(ret);

	return ret;
}

static void
network_ip_changed(GtkEntry *entry, gpointer data)
{
#if GTK_CHECK_VERSION(3,0,0)
	const gchar *text = gtk_entry_get_text(entry);
	GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(entry));

	if (text && *text) {
		if (purple_ip_address_is_valid(text)) {
			purple_network_set_public_ip(text);
			gtk_style_context_add_class(context, "good-ip");
			gtk_style_context_remove_class(context, "bad-ip");
		} else {
			gtk_style_context_add_class(context, "bad-ip");
			gtk_style_context_remove_class(context, "good-ip");
		}

	} else {
		purple_network_set_public_ip("");
		gtk_style_context_remove_class(context, "bad-ip");
		gtk_style_context_remove_class(context, "good-ip");
	}
#else
	const gchar *text = gtk_entry_get_text(entry);
	GdkColor color;

	if (text && *text) {
		if (purple_ip_address_is_valid(text)) {
			color.red = 0xAFFF;
			color.green = 0xFFFF;
			color.blue = 0xAFFF;

			purple_network_set_public_ip(text);
		} else {
			color.red = 0xFFFF;
			color.green = 0xAFFF;
			color.blue = 0xAFFF;
		}

		gtk_widget_modify_base(GTK_WIDGET(entry), GTK_STATE_NORMAL, &color);

	} else {
		purple_network_set_public_ip("");
		gtk_widget_modify_base(GTK_WIDGET(entry), GTK_STATE_NORMAL, NULL);
	}
#endif
}

static gboolean
network_stun_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/stun_server",
		gtk_entry_get_text(entry));
	purple_network_set_stun_server(gtk_entry_get_text(entry));

	return FALSE;
}

static gboolean
network_turn_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/turn_server",
		gtk_entry_get_text(entry));
	purple_network_set_turn_server(gtk_entry_get_text(entry));

	return FALSE;
}

static void
proxy_changed_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GtkWidget *frame = data;
	const char *proxy = value;

	if (strcmp(proxy, "none") && strcmp(proxy, "envvar"))
		gtk_widget_show_all(frame);
	else
		gtk_widget_hide(frame);
}

static void
proxy_print_option(GtkEntry *entry, int entrynum)
{
	if (entrynum == PROXYHOST)
		purple_prefs_set_string("/purple/proxy/host", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPORT)
		purple_prefs_set_int("/purple/proxy/port", atoi(gtk_entry_get_text(entry)));
	else if (entrynum == PROXYUSER)
		purple_prefs_set_string("/purple/proxy/username", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPASS)
		purple_prefs_set_string("/purple/proxy/password", gtk_entry_get_text(entry));
}

static void
proxy_button_clicked_cb(GtkWidget *button, gchar *program)
{
	GError *err = NULL;

	if (g_spawn_command_line_async(program, &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start proxy configuration program."), err->message, NULL);
	g_error_free(err);
}

#ifndef _WIN32
static void
browser_button_clicked_cb(GtkWidget *button, gchar *path)
{
	GError *err = NULL;

	if (g_spawn_command_line_async(path, &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start browser configuration program."), err->message, NULL);
	g_error_free(err);
}
#endif

static void
auto_ip_button_clicked_cb(GtkWidget *button, gpointer null)
{
	const char *ip;
	PurpleStunNatDiscovery *stun;
	char *auto_ip_text;

	/* purple_network_get_my_ip will return the IP that was set by the user with
	   purple_network_set_public_ip, so make a lookup for the auto-detected IP
	   ourselves. */

	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		/* Check if STUN discovery was already done */
		stun = purple_stun_discover(NULL);
		if ((stun != NULL) && (stun->status == PURPLE_STUN_STATUS_DISCOVERED)) {
			ip = stun->publicip;
		} else {
			/* Attempt to get the IP from a NAT device using UPnP */
			ip = purple_upnp_get_public_ip();
			if (ip == NULL) {
				/* Attempt to get the IP from a NAT device using NAT-PMP */
				ip = purple_pmp_get_public_ip();
				if (ip == NULL) {
					/* Just fetch the IP of the local system */
					ip = purple_network_get_local_system_ip(-1);
				}
			}
		}
	}
	else
		ip = _("Disabled");

	auto_ip_text = g_strdup_printf(_("Use _automatically detected IP address: %s"), ip);
	gtk_button_set_label(GTK_BUTTON(button), auto_ip_text);
	g_free(auto_ip_text);
}

static GtkWidget *
network_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox, *hbox, *entry;
	GtkWidget *label, *auto_ip_checkbox, *ports_checkbox, *spin_button;
	GtkSizeGroup *sg;
#if GTK_CHECK_VERSION(3,0,0)
	GtkStyleContext *context;
	GtkCssProvider *ip_css;
	const gchar ip_style[] =
		".bad-ip {"
			"color: @error_fg_color;"
			"text-shadow: 0 1px @error_text_shadow;"
			"background-image: none;"
			"background-color: @error_bg_color;"
		"}"
		".good-ip {"
			"color: @question_fg_color;"
			"text-shadow: 0 1px @question_text_shadow;"
			"background-image: none;"
			"background-color: @success_color;"
		"}";
#endif

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame (ret, _("IP Address"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(
			"/purple/network/stun_server"));
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			G_CALLBACK(network_stun_server_changed_cb), NULL);
	gtk_widget_show(entry);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("ST_UN server:"),
			sg, entry, TRUE, NULL);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	label = gtk_label_new(NULL);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_size_group_add_widget(sg, label);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label),
			_("<span style=\"italic\">Example: stunserver.org</span>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_container_add(GTK_CONTAINER(hbox), label);

	auto_ip_checkbox = pidgin_prefs_checkbox("Use _automatically detected IP address",
	                                         "/purple/network/auto_ip", vbox);
	g_signal_connect(G_OBJECT(auto_ip_checkbox), "clicked",
	                 G_CALLBACK(auto_ip_button_clicked_cb), NULL);
	auto_ip_button_clicked_cb(auto_ip_checkbox, NULL); /* Update label */

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_network_get_public_ip());
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(network_ip_changed), NULL);

#if GTK_CHECK_VERSION(3,0,0)
	ip_css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(ip_css, ip_style, -1, NULL);
	context = gtk_widget_get_style_context(entry);
	gtk_style_context_add_provider(context,
	                               GTK_STYLE_PROVIDER(ip_css),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#endif

	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Public _IP:"),
			sg, entry, TRUE, NULL);

	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		gtk_widget_set_sensitive(GTK_WIDGET(hbox), FALSE);
	}

	g_signal_connect(G_OBJECT(auto_ip_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), hbox);

	g_object_unref(sg);

	vbox = pidgin_make_frame (ret, _("Ports"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	pidgin_prefs_checkbox(_("_Enable automatic router port forwarding"),
			"/purple/network/map_ports", vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);

	ports_checkbox = pidgin_prefs_checkbox(_("_Manually specify range of ports to listen on:"),
			"/purple/network/ports_range_use", hbox);

	spin_button = pidgin_prefs_labeled_spin_button(hbox, _("_Start:"),
			"/purple/network/ports_range_start", 0, 65535, sg);
	if (!purple_prefs_get_bool("/purple/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	spin_button = pidgin_prefs_labeled_spin_button(hbox, _("_End:"),
			"/purple/network/ports_range_end", 0, 65535, sg);
	if (!purple_prefs_get_bool("/purple/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), NULL, NULL, hbox, TRUE, NULL);

	g_object_unref(sg);

	/* TURN server */
	vbox = pidgin_make_frame(ret, _("Relay Server (TURN)"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(
			"/purple/network/turn_server"));
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			G_CALLBACK(network_turn_server_changed_cb), NULL);
	gtk_widget_show(entry);

	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_TURN server:"),
			sg, entry, TRUE, NULL);

	pidgin_prefs_labeled_spin_button(hbox, _("_UDP Port:"),
		"/purple/network/turn_port", 0, 65535, NULL);

	pidgin_prefs_labeled_spin_button(hbox, _("T_CP Port:"),
		"/purple/network/turn_port_tcp", 0, 65535, NULL);

	hbox = pidgin_prefs_labeled_entry(vbox, _("Use_rname:"),
		"/purple/network/turn_username", sg);
	pidgin_prefs_labeled_password(hbox, _("Pass_word:"),
		"/purple/network/turn_password", NULL);

	gtk_widget_show_all(ret);
	g_object_unref(sg);

	return ret;
}

#ifndef _WIN32
static gboolean
manual_browser_set(GtkWidget *entry, GdkEventFocus *event, gpointer data)
{
	const char *program = gtk_entry_get_text(GTK_ENTRY(entry));

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", program);

	/* carry on normally */
	return FALSE;
}

static GList *
get_available_browsers(void)
{
	struct browser {
		char *name;
		char *command;
	};

	/* Sorted reverse alphabetically */
	static const struct browser possible_browsers[] = {
		{N_("Seamonkey"), "seamonkey"},
		{N_("Opera"), "opera"},
		{N_("Mozilla"), "mozilla"},
		{N_("Konqueror"), "kfmclient"},
		{N_("Google Chrome"), "google-chrome"},
		/* Do not move the line below.  Code below expects gnome-open to be in
		 * this list immediately after xdg-open! */
		{N_("Desktop Default"), "xdg-open"},
		{N_("GNOME Default"), "gnome-open"},
		{N_("Galeon"), "galeon"},
		{N_("Firefox"), "firefox"},
		{N_("Firebird"), "mozilla-firebird"},
		{N_("Epiphany"), "epiphany"},
		/* Translators: please do not translate "chromium-browser" here! */
		{N_("Chromium (chromium-browser)"), "chromium-browser"},
		/* Translators: please do not translate "chrome" here! */
		{N_("Chromium (chrome)"), "chrome"}
	};
	static const int num_possible_browsers = G_N_ELEMENTS(possible_browsers);

	GList *browsers = NULL;
	int i = 0;
	char *browser_setting = (char *)purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser");

	browsers = g_list_prepend(browsers, (gpointer)"custom");
	browsers = g_list_prepend(browsers, (gpointer)_("Manual"));

	for (i = 0; i < num_possible_browsers; i++) {
		if (purple_program_is_valid(possible_browsers[i].command)) {
			browsers = g_list_prepend(browsers,
									  possible_browsers[i].command);
			browsers = g_list_prepend(browsers, (gpointer)_(possible_browsers[i].name));
			if(browser_setting && !strcmp(possible_browsers[i].command, browser_setting))
				browser_setting = NULL;
			/* If xdg-open is valid, prefer it over gnome-open and skip forward */
			if(!strcmp(possible_browsers[i].command, "xdg-open")) {
				if (browser_setting && !strcmp("gnome-open", browser_setting)) {
					purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", possible_browsers[i].command);
					browser_setting = NULL;
				}
				i++;
			}
		}
	}

	if(browser_setting)
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", "custom");

	return browsers;
}

static void
browser_changed1_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, strcmp(browser, "custom"));
}

static void
browser_changed2_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, !strcmp(browser, "custom"));
}

static GtkWidget *
browser_page(void)
{
	GtkWidget *ret, *vbox, *hbox, *label, *entry, *browser_button;
	GtkSizeGroup *sg;
	GList *browsers = NULL;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame (ret, _("Browser Selection"));

	if (purple_running_gnome()) {
		gchar *path;

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
		label = gtk_label_new(_("Browser preferences are configured in GNOME preferences"));
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		path = g_find_program_in_path("gnome-control-center");
		if (path != NULL) {
			gchar *tmp = g_strdup_printf("%s info", path);
			g_free(path);
			path = tmp;
		} else {
			path = g_find_program_in_path("gnome-default-applications-properties");
		}

		if (path == NULL) {
			label = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(label),
								 _("<b>Browser configuration program was not found.</b>"));
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		} else {
			browser_button = gtk_button_new_with_mnemonic(_("Configure _Browser"));
			g_signal_connect_data(G_OBJECT(browser_button), "clicked",
			                      G_CALLBACK(browser_button_clicked_cb), path,
			                      (GClosureNotify)g_free, 0);
			gtk_box_pack_start(GTK_BOX(hbox), browser_button, FALSE, FALSE, 0);
		}

		gtk_widget_show_all(ret);
	} else {
		sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

		browsers = get_available_browsers();
		if (browsers != NULL) {
			label = pidgin_prefs_dropdown_from_list(vbox,_("_Browser:"), PURPLE_PREF_STRING,
											 PIDGIN_PREFS_ROOT "/browsers/browser",
											 browsers);
			g_list_free(browsers);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			gtk_size_group_add_widget(sg, label);

			hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			label = pidgin_prefs_dropdown(hbox, _("_Open link in:"), PURPLE_PREF_INT,
				PIDGIN_PREFS_ROOT "/browsers/place",
				_("Browser default"), PIDGIN_BROWSER_DEFAULT,
				_("New window"), PIDGIN_BROWSER_NEW_WINDOW,
				_("New tab"), PIDGIN_BROWSER_NEW_TAB,
				NULL);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			gtk_size_group_add_widget(sg, label);
			gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

			if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"), "custom"))
				gtk_widget_set_sensitive(hbox, FALSE);
			purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/browsers/browser",
										browser_changed1_cb, hbox);
		}

		entry = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(entry),
						   purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/manual_command"));
		g_signal_connect(G_OBJECT(entry), "focus-out-event",
						 G_CALLBACK(manual_browser_set), NULL);
		hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Manual:\n(%s for URL)"), sg, entry, TRUE, NULL);
		if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"), "custom"))
			gtk_widget_set_sensitive(hbox, FALSE);
		purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/browsers/browser",
				browser_changed2_cb, hbox);

		gtk_widget_show_all(ret);
		g_object_unref(sg);
	}

	return ret;
}
#endif /*_WIN32*/

static GtkWidget *
proxy_page(void)
{
	GtkWidget *ret = NULL, *vbox = NULL, *hbox = NULL;
	GtkWidget *table = NULL, *entry = NULL, *proxy_button = NULL;
	GtkLabel *label = NULL;
	GtkWidget *prefs_proxy_frame = NULL;
	PurpleProxyInfo *proxy_info;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);
	vbox = pidgin_make_frame(ret, _("Proxy Server"));
	prefs_proxy_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);

	if(purple_running_gnome()) {
		gchar *path = NULL;

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
		label = GTK_LABEL(gtk_label_new(_("Proxy preferences "
			"are configured in GNOME preferences")));
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		path = g_find_program_in_path("gnome-network-properties");
		if (path == NULL)
			path = g_find_program_in_path("gnome-network-preferences");
		if (path == NULL) {
			path = g_find_program_in_path("gnome-control-center");
			if (path != NULL) {
				char *tmp = g_strdup_printf("%s network", path);
				g_free(path);
				path = tmp;
			}
		}

		if (path == NULL) {
			label = GTK_LABEL(gtk_label_new(NULL));
			gtk_label_set_markup(label, _("<b>Proxy configuration "
				"program was not found.</b>"));
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
		} else {
			proxy_button = gtk_button_new_with_mnemonic(_("Configure _Proxy"));
			g_signal_connect(G_OBJECT(proxy_button), "clicked",
							 G_CALLBACK(proxy_button_clicked_cb),
							 path);
			gtk_box_pack_start(GTK_BOX(hbox), proxy_button, FALSE, FALSE, 0);
		}

		/* NOTE: path leaks, but only when the prefs window is destroyed,
		         which is never */
		gtk_widget_show_all(ret);
	} else {
		GtkWidget *prefs_proxy_subframe = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

		/* This is a global option that affects SOCKS4 usage even with
		 * account-specific proxy settings */
		pidgin_prefs_checkbox(_("Use remote _DNS with SOCKS4 proxies"),
							  "/purple/proxy/socks4_remotedns", prefs_proxy_frame);
		gtk_box_pack_start(GTK_BOX(vbox), prefs_proxy_frame, 0, 0, 0);

		pidgin_prefs_dropdown(prefs_proxy_frame, _("Proxy t_ype:"), PURPLE_PREF_STRING,
					"/purple/proxy/type",
					_("No proxy"), "none",
					_("SOCKS 4"), "socks4",
					_("SOCKS 5"), "socks5",
					_("Tor/Privacy (SOCKS5)"), "tor",
					_("HTTP"), "http",
					_("Use Environmental Settings"), "envvar",
					NULL);
		gtk_box_pack_start(GTK_BOX(prefs_proxy_frame), prefs_proxy_subframe, 0, 0, 0);
		proxy_info = purple_global_proxy_get_info();

		gtk_widget_show_all(ret);

		purple_prefs_connect_callback(prefs, "/purple/proxy/type",
					    proxy_changed_cb, prefs_proxy_subframe);

		table = gtk_table_new(4, 2, FALSE);
		gtk_container_set_border_width(GTK_CONTAINER(table), 0);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 10);
		gtk_container_add(GTK_CONTAINER(prefs_proxy_subframe), table);

		label = GTK_LABEL(gtk_label_new_with_mnemonic(_("_Host:")));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
			0, 1, 0, 1, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(label, entry);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYHOST);

		if (proxy_info != NULL && purple_proxy_info_get_host(proxy_info))
			gtk_entry_set_text(GTK_ENTRY(entry),
					   purple_proxy_info_get_host(proxy_info));

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		pidgin_set_accessible_label(entry, label);

		label = GTK_LABEL(gtk_label_new_with_mnemonic(_("P_ort:")));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
			2, 3, 0, 1, GTK_FILL, 0, 0, 0);

		entry = gtk_spin_button_new_with_range(0, 65535, 1);
		gtk_label_set_mnemonic_widget(label, entry);
		gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYPORT);

		if (proxy_info != NULL && purple_proxy_info_get_port(proxy_info) != 0) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry),
				purple_proxy_info_get_port(proxy_info));
		}
		pidgin_set_accessible_label(entry, label);

		label = GTK_LABEL(gtk_label_new_with_mnemonic(_("User_name:")));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
			0, 1, 1, 2, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(label, entry);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYUSER);

		if (proxy_info != NULL && purple_proxy_info_get_username(proxy_info) != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry),
						   purple_proxy_info_get_username(proxy_info));

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		pidgin_set_accessible_label(entry, label);

		label = GTK_LABEL(gtk_label_new_with_mnemonic(_("Pa_ssword:")));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
			2, 3, 1, 2, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(label, entry);
		gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 1, 2, GTK_FILL , 0, 0, 0);
		gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYPASS);

		if (proxy_info != NULL && purple_proxy_info_get_password(proxy_info) != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry),
					   purple_proxy_info_get_password(proxy_info));
		pidgin_set_accessible_label(entry, label);

		proxy_changed_cb("/purple/proxy/type", PURPLE_PREF_STRING,
			purple_prefs_get_string("/purple/proxy/type"),
			prefs_proxy_subframe);

	}

	return ret;
}

static GtkWidget *
logging_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GList *names;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);


	vbox = pidgin_make_frame (ret, _("Logging"));
	names = purple_log_logger_get_options();

	pidgin_prefs_dropdown_from_list(vbox, _("Log _format:"), PURPLE_PREF_STRING,
				 "/purple/logging/format", names);

	g_list_free(names);

	pidgin_prefs_checkbox(_("Log all _instant messages"),
				  "/purple/logging/log_ims", vbox);
	pidgin_prefs_checkbox(_("Log all c_hats"),
				  "/purple/logging/log_chats", vbox);
	pidgin_prefs_checkbox(_("Log all _status changes to system log"),
				  "/purple/logging/log_system", vbox);

	gtk_widget_show_all(ret);

	return ret;
}

/*** keyring page *******************************************************/

static void
keyring_page_settings_changed(GtkWidget *widget, gpointer _setting)
{
	PurpleRequestField *setting = _setting;
	PurpleRequestFieldType field_type;

	gtk_widget_set_sensitive(keyring_apply, TRUE);

	field_type = purple_request_field_get_field_type(setting);

	if (field_type == PURPLE_REQUEST_FIELD_BOOLEAN) {
		purple_request_field_bool_set_value(setting,
			gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(widget)));
	} else if (field_type == PURPLE_REQUEST_FIELD_STRING) {
		purple_request_field_string_set_value(setting,
			gtk_entry_get_text(GTK_ENTRY(widget)));
	} else if (field_type == PURPLE_REQUEST_FIELD_INTEGER) {
		purple_request_field_int_set_value(setting,
			gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(widget)));
	} else
		g_return_if_reached();
}

static GtkWidget *
keyring_page_add_settings_field(GtkBox *vbox, PurpleRequestField *setting,
	GtkSizeGroup *sg)
{
	GtkWidget *widget, *hbox;
	PurpleRequestFieldType field_type;
	const gchar *label;

	label = purple_request_field_get_label(setting);

	field_type = purple_request_field_get_field_type(setting);
	if (field_type == PURPLE_REQUEST_FIELD_BOOLEAN) {
		widget = gtk_check_button_new_with_label(label);
		label = NULL;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			purple_request_field_bool_get_value(setting));
		g_signal_connect(G_OBJECT(widget), "toggled",
			G_CALLBACK(keyring_page_settings_changed), setting);
	} else if (field_type == PURPLE_REQUEST_FIELD_STRING) {
		widget = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(widget),
			purple_request_field_string_get_value(setting));
		if (purple_request_field_string_is_masked(setting))
			gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
		g_signal_connect(G_OBJECT(widget), "changed",
			G_CALLBACK(keyring_page_settings_changed), setting);
	} else if (field_type == PURPLE_REQUEST_FIELD_INTEGER) {
		widget = gtk_spin_button_new_with_range(
			purple_request_field_int_get_lower_bound(setting),
			purple_request_field_int_get_upper_bound(setting), 1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			purple_request_field_int_get_value(setting));
		g_signal_connect(G_OBJECT(widget), "value-changed",
			G_CALLBACK(keyring_page_settings_changed), setting);
	} else {
		purple_debug_error("gtkprefs", "Unsupported field type\n");
		return NULL;
	}

	hbox = pidgin_add_widget_to_vbox(vbox, label, sg, widget,
		FALSE, NULL);
	return ((void*)hbox == (void*)vbox) ? widget : hbox;
}

/* XXX: it could be available for all plugins, not keyrings only */
static GList *
keyring_page_add_settings(PurpleRequestFields *settings)
{
	GList *it, *groups, *added_fields;
	GtkSizeGroup *sg;

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	added_fields = NULL;
	groups = purple_request_fields_get_groups(settings);
	for (it = g_list_first(groups); it != NULL; it = g_list_next(it)) {
		GList *it2, *fields;
		GtkBox *vbox;
		PurpleRequestFieldGroup *group;
		const gchar *group_title;

		group = it->data;
		group_title = purple_request_field_group_get_title(group);
		if (group_title) {
			vbox = GTK_BOX(pidgin_make_frame(
				GTK_WIDGET(keyring_vbox), group_title));
			added_fields = g_list_prepend(added_fields,
				g_object_get_data(G_OBJECT(vbox), "main-vbox"));
		} else
			vbox = keyring_vbox;

		fields = purple_request_field_group_get_fields(group);
		for (it2 = g_list_first(fields); it2 != NULL;
			it2 = g_list_next(it2)) {
			GtkWidget *added = keyring_page_add_settings_field(vbox,
				it2->data, sg);
			if (added == NULL || vbox != keyring_vbox)
				continue;
			added_fields = g_list_prepend(added_fields, added);
		}
	}

	g_object_unref(sg);

	return added_fields;
}

static void
keyring_page_settings_apply(GtkButton *button, gpointer _unused)
{
	if (!purple_keyring_apply_settings(prefs, keyring_settings))
		return;

	gtk_widget_set_sensitive(keyring_apply, FALSE);
}

static void
keyring_page_update_settings()
{
	if (keyring_settings != NULL)
		purple_request_fields_destroy(keyring_settings);
	keyring_settings = purple_keyring_read_settings();
	if (!keyring_settings)
		return;

	keyring_settings_fields = keyring_page_add_settings(keyring_settings);

	keyring_apply = gtk_button_new_with_mnemonic(_("_Apply"));
	gtk_box_pack_start(keyring_vbox, keyring_apply, FALSE, FALSE, 1);
	gtk_widget_set_sensitive(keyring_apply, FALSE);
	keyring_settings_fields = g_list_prepend(keyring_settings_fields,
		keyring_apply);
	g_signal_connect(G_OBJECT(keyring_apply), "clicked",
		G_CALLBACK(keyring_page_settings_apply), NULL);

	gtk_widget_show_all(keyring_page_instance);
}

static void
keyring_page_pref_set_inuse(GError *error, gpointer _keyring_page_instance)
{
	PurpleKeyring *in_use = purple_keyring_get_inuse();

	if (_keyring_page_instance != keyring_page_instance) {
		purple_debug_info("gtkprefs", "pref window already closed\n");
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(keyring_combo), TRUE);

	if (error != NULL) {
		pidgin_prefs_dropdown_revert_active(keyring_combo);
		purple_notify_error(NULL, _("Keyring"),
			_("Failed to set new keyring"), error->message, NULL);
		return;
	}

	g_return_if_fail(in_use != NULL);
	purple_prefs_set_string("/purple/keyring/active",
		purple_keyring_get_id(in_use));

	keyring_page_update_settings();
}

static void
keyring_page_pref_changed(GtkComboBox *combo_box, PidginPrefValue value)
{
	const char *keyring_id;
	PurpleKeyring *keyring;
	GList *it;

	g_return_if_fail(combo_box != NULL);
	g_return_if_fail(value.type == PURPLE_PREF_STRING);

	keyring_id = value.value.string;
	keyring = purple_keyring_find_keyring_by_id(keyring_id);
	if (keyring == NULL) {
		pidgin_prefs_dropdown_revert_active(keyring_combo);
		purple_notify_error(NULL, _("Keyring"),
			_("Selected keyring is disabled"), NULL, NULL);
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(combo_box), FALSE);

	for (it = keyring_settings_fields; it != NULL; it = g_list_next(it))
	{
		GtkWidget *widget = it->data;
		gtk_container_remove(
			GTK_CONTAINER(gtk_widget_get_parent(widget)), widget);
	}
	gtk_widget_show_all(keyring_page_instance);
	g_list_free(keyring_settings_fields);
	keyring_settings_fields = NULL;
	if (keyring_settings)
		purple_request_fields_destroy(keyring_settings);
	keyring_settings = NULL;

	purple_keyring_set_inuse(keyring, FALSE, keyring_page_pref_set_inuse,
		keyring_page_instance);
}

static void
keyring_page_cleanup(void)
{
	keyring_page_instance = NULL;
	keyring_combo = NULL;
	keyring_vbox = NULL;
	g_list_free(keyring_settings_fields);
	keyring_settings_fields = NULL;
	if (keyring_settings)
		purple_request_fields_destroy(keyring_settings);
	keyring_settings = NULL;
	keyring_apply = NULL;
}

static GtkWidget *
keyring_page(void)
{
	GList *names;
	PidginPrefValue initial;

	g_return_val_if_fail(keyring_page_instance == NULL,
		keyring_page_instance);

	keyring_page_instance = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(keyring_page_instance),
		PIDGIN_HIG_BORDER);

	/* Keyring selection */
	keyring_vbox = GTK_BOX(pidgin_make_frame(keyring_page_instance,
		_("Keyring")));
	names = purple_keyring_get_options();
	initial.type = PURPLE_PREF_STRING;
	initial.value.string = purple_prefs_get_string("/purple/keyring/active");
	pidgin_prefs_dropdown_from_list_with_cb(GTK_WIDGET(keyring_vbox),
		_("Keyring:"), &keyring_combo, names, initial,
		keyring_page_pref_changed);
	g_list_free(names);

	keyring_page_update_settings();

	gtk_widget_show_all(keyring_page_instance);

	return keyring_page_instance;
}

/*** keyring page - end *************************************************/

static gint
sound_cmd_yeah(GtkEntry *entry, gpointer d)
{
	purple_prefs_set_path(PIDGIN_PREFS_ROOT "/sound/command",
			gtk_entry_get_text(GTK_ENTRY(entry)));
	return TRUE;
}

static void
sound_changed1_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox, !strcmp(method, "custom"));
}

static void
sound_changed2_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *vbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(vbox, strcmp(method, "none"));
}

#ifdef USE_GSTREAMER
static void
sound_changed3_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox,
			!strcmp(method, "automatic") ||
			!strcmp(method, "alsa") ||
			!strcmp(method, "esd") ||
			!strcmp(method, "waveform") ||
			!strcmp(method, "directsound"));
}
#endif /* USE_GSTREAMER */


static void
event_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	char *pref;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
						2, &pref,
						-1);

	purple_prefs_set_bool(pref, !gtk_cell_renderer_toggle_get_active(cell));
	g_free(pref);

	gtk_list_store_set(GTK_LIST_STORE (model), &iter,
					   0, !gtk_cell_renderer_toggle_get_active(cell),
					   -1);

	gtk_tree_path_free(path);
}

static void
test_sound(GtkWidget *button, gpointer i_am_NULL)
{
	char *pref;
	gboolean temp_enabled;
	gboolean temp_mute;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/enabled/%s",
			pidgin_sound_get_event_option(sound_row_sel));

	temp_enabled = purple_prefs_get_bool(pref);
	temp_mute = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/sound/mute");

	if (!temp_enabled) purple_prefs_set_bool(pref, TRUE);
	if (temp_mute) purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", FALSE);

	purple_sound_play_event(sound_row_sel, NULL);

	if (!temp_enabled) purple_prefs_set_bool(pref, FALSE);
	if (temp_mute) purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", TRUE);

	g_free(pref);
}

/*
 * Resets a sound file back to default.
 */
static void
reset_sound(GtkWidget *button, gpointer i_am_also_NULL)
{
	gchar *pref;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	purple_prefs_set_path(pref, "");
	g_free(pref);

	gtk_entry_set_text(GTK_ENTRY(sound_entry), _("(default)"));

	pref_sound_generate_markup();
}

static void
sound_chosen_cb(void *user_data, const char *filename)
{
	gchar *pref;
	int sound;

	sound = GPOINTER_TO_INT(user_data);

	/* Set it -- and forget it */
	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound));
	purple_prefs_set_path(pref, filename);
	g_free(pref);

	/*
	 * If the sound we just changed is still the currently selected
	 * sound, then update the box showing the file name.
	 */
	if (sound == sound_row_sel)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), filename);

	pref_sound_generate_markup();
}

static void
select_sound(GtkWidget *button, gpointer being_NULL_is_fun)
{
	gchar *pref;
	const char *filename;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	filename = purple_prefs_get_path(pref);
	g_free(pref);

	if (*filename == '\0')
		filename = NULL;

	purple_request_file(prefs, _("Sound Selection"), filename, FALSE,
		G_CALLBACK(sound_chosen_cb), NULL, NULL,
		GINT_TO_POINTER(sound_row_sel));
}

#ifdef USE_GSTREAMER
static gchar *
prefs_sound_volume_format(GtkScale *scale, gdouble val)
{
	if(val < 15) {
		return g_strdup_printf(_("Quietest"));
	} else if(val < 30) {
		return g_strdup_printf(_("Quieter"));
	} else if(val < 45) {
		return g_strdup_printf(_("Quiet"));
	} else if(val < 55) {
		return g_strdup_printf(_("Normal"));
	} else if(val < 70) {
		return g_strdup_printf(_("Loud"));
	} else if(val < 85) {
		return g_strdup_printf(_("Louder"));
	} else {
		return g_strdup_printf(_("Loudest"));
	}
}

static void
prefs_sound_volume_changed(GtkRange *range)
{
	int val = (int)gtk_range_get_value(GTK_RANGE(range));
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/sound/volume", val);
}
#endif

static void
prefs_sound_sel(GtkTreeSelection *sel, GtkTreeModel *model)
{
	GtkTreeIter  iter;
	GValue val;
	const char *file;
	char *pref;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 3, &val);
	sound_row_sel = g_value_get_uint(&val);

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
			pidgin_sound_get_event_option(sound_row_sel));
	file = purple_prefs_get_path(pref);
	g_free(pref);
	if (sound_entry)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), (file && *file != '\0') ? file : _("(default)"));
	g_value_unset (&val);

	pref_sound_generate_markup();
}


static void
mute_changed_cb(const char *pref_name,
                PurplePrefType pref_type,
                gconstpointer val,
                gpointer data)
{
	GtkToggleButton *button = data;
	gboolean muted = GPOINTER_TO_INT(val);

	g_return_if_fail(!strcmp (pref_name, PIDGIN_PREFS_ROOT "/sound/mute"));

	/* Block the handler that re-sets the preference. */
	g_signal_handlers_block_matched(button, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, (gpointer)pref_name);
	gtk_toggle_button_set_active (button, muted);
	g_signal_handlers_unblock_matched(button, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, (gpointer)pref_name);
}


static GtkWidget *
sound_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox, *vbox2, *button, *parent, *parent_parent, *parent_parent_parent;
#ifdef USE_GSTREAMER
	GtkWidget *sw;
#endif
	GtkSizeGroup *sg;
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkListStore *event_store;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	GtkWidget *hbox;
	int j;
	const char *file;
	char *pref;
	GtkWidget *dd;
	GtkWidget *entry;
	const char *cmd;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox2 = pidgin_make_frame(ret, _("Sound Options"));

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2), vbox, FALSE, FALSE, 0);

	dd = pidgin_prefs_dropdown(vbox2, _("_Method:"), PURPLE_PREF_STRING,
			PIDGIN_PREFS_ROOT "/sound/method",
			_("Automatic"), "automatic",
#ifdef USE_GSTREAMER
#ifdef _WIN32
/*			"WaveForm", "waveform", */
			"DirectSound", "directsound",
#else
			"ESD", "esd",
			"ALSA", "alsa",
#endif /* _WIN32 */
#endif /* USE_GSTREAMER */
#ifdef _WIN32
			"PlaySound", "playsoundw",
#else
			_("Console beep"), "beep",
			_("Command"), "custom",
#endif /* _WIN32 */
			_("No sounds"), "none",
			NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
	cmd = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/sound/command");
	if(cmd)
		gtk_entry_set_text(GTK_ENTRY(entry), cmd);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(sound_cmd_yeah), NULL);

	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Sound c_ommand:\n(%s for filename)"), sg, entry, TRUE, NULL);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
								sound_changed1_cb, hbox);
	gtk_widget_set_sensitive(hbox,
			!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"),
					"custom"));

	button = pidgin_prefs_checkbox(_("M_ute sounds"), PIDGIN_PREFS_ROOT "/sound/mute", vbox);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/mute", mute_changed_cb, button);

	pidgin_prefs_checkbox(_("Sounds when conversation has _focus"),
				   PIDGIN_PREFS_ROOT "/sound/conv_focus", vbox);
	pidgin_prefs_dropdown(vbox, _("_Enable sounds:"),
				 PURPLE_PREF_INT, "/purple/sound/while_status",
				_("Only when available"), 1,
				_("Only when not available"), 2,
				_("Always"), 3,
				NULL);

#ifdef USE_GSTREAMER
	sw = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
		0.0, 100.0, 5.0);
	gtk_range_set_increments(GTK_RANGE(sw), 5.0, 25.0);
	gtk_range_set_value(GTK_RANGE(sw), purple_prefs_get_int(PIDGIN_PREFS_ROOT "/sound/volume"));
	g_signal_connect (G_OBJECT (sw), "format-value",
			  G_CALLBACK (prefs_sound_volume_format),
			  NULL);
	g_signal_connect (G_OBJECT (sw), "value-changed",
			  G_CALLBACK (prefs_sound_volume_changed),
			  NULL);
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("V_olume:"), NULL, sw, TRUE, NULL);

	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
								sound_changed3_cb, hbox);
	sound_changed3_cb(PIDGIN_PREFS_ROOT "/sound/method", PURPLE_PREF_STRING,
			  purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), hbox);
#endif

	gtk_widget_set_sensitive(vbox,
			strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), "none"));
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
								sound_changed2_cb, vbox);
	vbox = pidgin_make_frame(ret, _("Sound Events"));

	/* The following is an ugly hack to make the frame expand so the
	 * sound events list is big enough to be usable */
	parent = gtk_widget_get_parent(vbox);
	parent_parent = gtk_widget_get_parent(parent);
	parent_parent_parent = gtk_widget_get_parent(parent_parent);
	gtk_box_set_child_packing(GTK_BOX(parent), vbox, TRUE, TRUE, 0,
			GTK_PACK_START);
	gtk_box_set_child_packing(GTK_BOX(parent_parent),
			parent, TRUE, TRUE, 0, GTK_PACK_START);
	gtk_box_set_child_packing(GTK_BOX(parent_parent_parent),
			parent_parent, TRUE, TRUE, 0, GTK_PACK_START);

	/* SOUND SELECTION */
	event_store = gtk_list_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);

	for (j=0; j < PURPLE_NUM_SOUNDS; j++) {
		char *pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/enabled/%s",
					     pidgin_sound_get_event_option(j));
		const char *label = pidgin_sound_get_event_label(j);

		if (label == NULL) {
			g_free(pref);
			continue;
		}

		gtk_list_store_append (event_store, &iter);
		gtk_list_store_set(event_store, &iter,
				   0, purple_prefs_get_bool(pref),
				   1, _(label),
				   2, pref,
				   3, j,
				   -1);
		g_free(pref);
	}

	event_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(event_store));

	rend = gtk_cell_renderer_toggle_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));
	g_signal_connect (G_OBJECT (sel), "changed",
			  G_CALLBACK (prefs_sound_sel),
			  NULL);
	g_signal_connect (G_OBJECT(rend), "toggled",
			  G_CALLBACK(event_toggled), event_store);
	path = gtk_tree_path_new_first();
	gtk_tree_selection_select_path(sel, path);
	gtk_tree_path_free(path);

	col = gtk_tree_view_column_new_with_attributes (_("Play"),
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes (_("Event"),
							rend,
							"text", 1,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	g_object_unref(G_OBJECT(event_store));
	gtk_box_pack_start(GTK_BOX(vbox),
		pidgin_make_scrollable(event_view, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, 100),
		TRUE, TRUE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	sound_entry = gtk_entry_new();
	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
			       pidgin_sound_get_event_option(0));
	file = purple_prefs_get_path(pref);
	g_free(pref);
	gtk_entry_set_text(GTK_ENTRY(sound_entry), (file && *file != '\0') ? file : _("(default)"));
	gtk_editable_set_editable(GTK_EDITABLE(sound_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), sound_entry, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);

	button = gtk_button_new_with_mnemonic(_("_Browse..."));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(select_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_mnemonic(_("Pre_view"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(test_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_mnemonic(_("_Reset"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(reset_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	gtk_widget_show_all(ret);
	g_object_unref(sg);

	return ret;
}


static void
set_idle_away(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/idleaway", purple_savedstatus_get_creation_time(status));
}

static void
set_startupstatus(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/startup", purple_savedstatus_get_creation_time(status));
}

static GtkWidget *
away_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *dd;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *menu;
	GtkSizeGroup *sg;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Idle stuff */
	vbox = pidgin_make_frame(ret, _("Idle"));

	dd = pidgin_prefs_dropdown(vbox, _("_Report idle time:"),
		PURPLE_PREF_STRING, "/purple/away/idle_reporting",
		_("Never"), "none",
		_("From last sent message"), "purple",
#if defined(USE_SCREENSAVER) || defined(HAVE_IOKIT)
		_("Based on keyboard or mouse use"), "system",
#endif
		NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	pidgin_prefs_labeled_spin_button(vbox,
			_("_Minutes before becoming idle:"), "/purple/away/mins_before_away",
			1, 24 * 60, sg);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = pidgin_prefs_checkbox(_("Change to this status when _idle:"),
						   "/purple/away/away_when_idle", hbox);
	gtk_size_group_add_widget(sg, button);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_idleaway(), G_CALLBACK(set_idle_away));
	gtk_size_group_add_widget(sg, menu);
	gtk_box_pack_start(GTK_BOX(hbox), menu, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);

	if(!purple_prefs_get_bool("/purple/away/away_when_idle"))
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);

	/* Away stuff */
	vbox = pidgin_make_frame(ret, _("Away"));

	dd = pidgin_prefs_dropdown(vbox, _("_Auto-reply:"),
		PURPLE_PREF_STRING, "/purple/away/auto_reply",
		_("Never"), "never",
		_("When away"), "away",
		_("When both away and idle"), "awayidle",
		NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	/* Signon status stuff */
	vbox = pidgin_make_frame(ret, _("Status at Startup"));

	button = pidgin_prefs_checkbox(_("Use status from last _exit at startup"),
		"/purple/savedstatus/startup_current_status", vbox);
	gtk_size_group_add_widget(sg, button);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_startup(), G_CALLBACK(set_startupstatus));
	gtk_size_group_add_widget(sg, menu);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Status to a_pply at startup:"), sg, menu, TRUE, &label);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), label);

	if(purple_prefs_get_bool("/purple/savedstatus/startup_current_status")) {
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	}

	gtk_widget_show_all(ret);
	g_object_unref(sg);

	return ret;
}

#ifdef USE_VV
static GList *
get_vv_element_devices(const gchar *element_name)
{
	GList *ret = NULL;
	GstElement *element;
	GObjectClass *klass;
#if !GST_CHECK_VERSION(1,0,0)
	GstPropertyProbe *probe;
	const GParamSpec *pspec;
	guint i;
	GValueArray *array;
	enum {
		PROBE_NONE,
		PROBE_DEVICE,
		PROBE_NAME
	} probe_attr;
#endif

	ret = g_list_prepend(ret, g_strdup(_("Default")));
	ret = g_list_prepend(ret, g_strdup(""));

	if (!strcmp(element_name, "<custom>") || (*element_name == '\0')) {
		return g_list_reverse(ret);
	}

	if (g_strcmp0(element_name, "videodisabledsrc") == 0) {
		/* Translators: This string refers to 'static' or 'snow' sometimes
		   seen when trying to tune a TV to a non-existant analog station. */
		ret = g_list_prepend(ret, g_strdup(_("Random noise")));
		ret = g_list_prepend(ret, g_strdup("snow"));

		return g_list_reverse(ret);
	}

	element = gst_element_factory_make(element_name, "test");
	if (!element) {
		purple_debug_info("vvconfig", "'%s' - unable to find element\n",
			element_name);
		return g_list_reverse(ret);
	}

	klass = G_OBJECT_GET_CLASS (element);
	if (!klass) {
		purple_debug_info("vvconfig", "'%s' - unable to find GObject "
			"Class\n", element_name);
		return g_list_reverse(ret);
	}

#if GST_CHECK_VERSION(1,0,0)
	purple_debug_info("vvconfig", "'%s' - gstreamer-1.0 doesn't suport "
		"property probing\n", element_name);
#else
	if (g_object_class_find_property(klass, "device"))
		probe_attr = PROBE_DEVICE;
	else if (g_object_class_find_property(klass, "device-index") &&
		g_object_class_find_property(klass, "device-name"))
		probe_attr = PROBE_NAME;
	else
		probe_attr = PROBE_NONE;
	
	if (!GST_IS_PROPERTY_PROBE(element))
		probe_attr = PROBE_NONE;
	
	if (probe_attr == PROBE_NONE)
	{
		purple_debug_info("vvconfig", "'%s' - no possibility to probe "
			"for devices\n", element_name);
		gst_object_unref(element);
		return g_list_reverse(ret);
	}

	probe = GST_PROPERTY_PROBE(element);

	if (probe_attr == PROBE_DEVICE)
		pspec = gst_property_probe_get_property(probe, "device");
	else /* probe_attr == PROBE_NAME */
		pspec = gst_property_probe_get_property(probe, "device-name");

	if (!pspec) {
		purple_debug_info("vvconfig", "'%s' - creating probe failed\n",
			element_name);
		gst_object_unref(element);
		return g_list_reverse(ret);
	}

	/* Set autoprobe[-fps] to FALSE to avoid delays when probing. */
	if (g_object_class_find_property(klass, "autoprobe"))
		g_object_set(G_OBJECT(element), "autoprobe", FALSE, NULL);
	if (g_object_class_find_property(klass, "autoprobe-fps"))
		g_object_set(G_OBJECT(element), "autoprobe-fps", FALSE, NULL);

	array = gst_property_probe_probe_and_get_values(probe, pspec);
	if (array == NULL) {
		purple_debug_info("vvconfig", "'%s' has no devices\n",
			element_name);
		gst_object_unref(element);
		return g_list_reverse(ret);
	}

	for (i = 0; i < array->n_values; i++) {
		GValue *device;
		const gchar *name;
		const gchar *device_name;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		/* GValueArray is in gstreamer-0.10 API */
		device = g_value_array_get_nth(array, i);
G_GNUC_END_IGNORE_DEPRECATIONS

		if (probe_attr == PROBE_DEVICE) {
			g_object_set_property(G_OBJECT(element), "device",
				device);
			if (gst_element_set_state(element, GST_STATE_READY)
				!= GST_STATE_CHANGE_SUCCESS)
			{
				purple_debug_warning("vvconfig", "Error "
					"changing state of %s\n", element_name);
				continue;
			}

			g_object_get(G_OBJECT(element), "device-name", &name,
				NULL);
			device_name = g_strdup(g_value_get_string(device));
		} else /* probe_attr == PROBE_NAME */ {
			name = g_strdup(g_value_get_string(device));
			device_name = g_strdup_printf("%d", i);
		}

		if (name == NULL)
			name = _("Unknown");
		purple_debug_info("vvconfig", "Found device %s: %s for %s\n",
			device_name, name, element_name);
		ret = g_list_prepend(ret, (gpointer)name);
		ret = g_list_prepend(ret, (gpointer)device_name);
		gst_element_set_state(element, GST_STATE_NULL);
	}
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	/* GValueArray is in gstreamer-0.10 API */
	g_value_array_free(array);
G_GNUC_END_IGNORE_DEPRECATIONS
#endif

	gst_object_unref(element);
	return g_list_reverse(ret);
}

static GList *
get_vv_element_plugins(const gchar **plugins)
{
	GList *ret = NULL;

	ret = g_list_prepend(ret, (gpointer)_("Default"));
	ret = g_list_prepend(ret, "");
	for (; plugins[0] && plugins[1]; plugins += 2) {
#if GST_CHECK_VERSION(1,0,0)
		if (gst_registry_check_feature_version(gst_registry_get(),
			plugins[0], 0, 0, 0)
#else
		if (gst_default_registry_check_feature_version(plugins[0], 0, 0, 0)
#endif
			|| g_strcmp0(plugins[0], "videodisabledsrc") == 0)
		{
			ret = g_list_prepend(ret, (gpointer)_(plugins[1]));
			ret = g_list_prepend(ret, (gpointer)plugins[0]);
		}
	}

	return g_list_reverse(ret);
}

static GstElement *
create_test_element(PurpleMediaElementType type)
{
	PurpleMediaElementInfo *element_info;

	element_info = purple_media_manager_get_active_element(purple_media_manager_get(), type);

	g_return_val_if_fail(element_info, NULL);

	return purple_media_element_info_call_create(element_info,
		NULL, NULL, NULL);
}

static void
vv_test_switch_page_cb(GtkNotebook *notebook, GtkWidget *page, guint num, gpointer data)
{
	GtkWidget *test = data;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(test), FALSE);
}

static GstElement *
create_voice_pipeline(void)
{
	GstElement *pipeline;
	GstElement *src, *sink;
	GstElement *volume;
	GstElement *level;
	GstElement *valve;

	pipeline = gst_pipeline_new("voicetest");

	src = create_test_element(PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SRC);
	sink = create_test_element(PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SINK);
	volume = gst_element_factory_make("volume", "volume");
	level = gst_element_factory_make("level", "level");
	valve = gst_element_factory_make("valve", "valve");

	gst_bin_add_many(GST_BIN(pipeline), src, volume, level, valve, sink, NULL);
	gst_element_link_many(src, volume, level, valve, sink, NULL);

	purple_debug_info("gtkprefs", "create_voice_pipeline: setting pipeline "
		"state to GST_STATE_PLAYING - it may hang here on win32\n");
	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
	purple_debug_info("gtkprefs", "create_voice_pipeline: state is set\n");

	return pipeline;
}

static void
on_volume_change_cb(GtkWidget *w, gdouble value, gpointer data)
{
	GstElement *volume;

	if (!voice_pipeline)
		return;

	volume = gst_bin_get_by_name(GST_BIN(voice_pipeline), "volume");
	g_object_set(volume, "volume",
	             gtk_scale_button_get_value(GTK_SCALE_BUTTON(w)) * 10.0, NULL);
}

static gdouble
gst_msg_db_to_percent(GstMessage *msg, gchar *value_name)
{
	const GValue *list;
	const GValue *value;
	gdouble value_db;
	gdouble percent;

	list = gst_structure_get_value(gst_message_get_structure(msg), value_name);
#if GST_CHECK_VERSION(1,0,0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	value = g_value_array_get_nth(g_value_get_boxed(list), 0);
G_GNUC_END_IGNORE_DEPRECATIONS
#else
	value = gst_value_list_get_value(list, 0);
#endif
	value_db = g_value_get_double(value);
	percent = pow(10, value_db / 20);
	return (percent > 1.0) ? 1.0 : percent;
}

static gboolean
gst_bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
	if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
		gst_structure_has_name(gst_message_get_structure(msg), "level")) {

		GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
		gchar *name = gst_element_get_name(src);

		if (!strcmp(name, "level")) {
			gdouble percent;
			gdouble threshold;
			GstElement *valve;

			percent = gst_msg_db_to_percent(msg, "rms");
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(voice_level), percent);

			percent = gst_msg_db_to_percent(msg, "decay");
			threshold = gtk_range_get_value(GTK_RANGE(voice_threshold)) / 100.0;
			valve = gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(src)), "valve");
			g_object_set(valve, "drop", (percent < threshold), NULL);
			g_object_set(voice_level, "text",
			             (percent < threshold) ? _("DROP") : " ", NULL);
		}

		g_free(name);
	}

	return TRUE;
}

static void
voice_test_destroy_cb(GtkWidget *w, gpointer data)
{
	if (!voice_pipeline)
		return;

	gst_element_set_state(voice_pipeline, GST_STATE_NULL);
	gst_object_unref(voice_pipeline);
	voice_pipeline = NULL;
}

static void
enable_voice_test(void)
{
	GstBus *bus;

	voice_pipeline = create_voice_pipeline();
	bus = gst_pipeline_get_bus(GST_PIPELINE(voice_pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message", G_CALLBACK(gst_bus_cb), NULL);
	gst_object_unref(bus);
}

static void
toggle_voice_test_cb(GtkToggleButton *test, gpointer data)
{
	if (gtk_toggle_button_get_active(test)) {
		gtk_widget_set_sensitive(voice_level, TRUE);
		enable_voice_test();

		g_signal_connect(voice_volume, "value-changed",
		                 G_CALLBACK(on_volume_change_cb), NULL);
		g_signal_connect(test, "destroy",
		                 G_CALLBACK(voice_test_destroy_cb), NULL);
		g_signal_connect(prefsnotebook, "switch-page",
		                 G_CALLBACK(vv_test_switch_page_cb), test);
	} else {
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(voice_level), 0.0);
		gtk_widget_set_sensitive(voice_level, FALSE);
		g_object_disconnect(voice_volume, "any-signal::value-changed",
		                    G_CALLBACK(on_volume_change_cb), NULL,
		                    NULL);
		g_object_disconnect(test, "any-signal::destroy",
		                    G_CALLBACK(voice_test_destroy_cb), NULL,
		                    NULL);
		g_object_disconnect(prefsnotebook, "any-signal::switch-page",
		                    G_CALLBACK(vv_test_switch_page_cb), test,
		                    NULL);
		voice_test_destroy_cb(NULL, NULL);
	}
}

static void
volume_changed_cb(GtkScaleButton *button, gdouble value, gpointer data)
{
	purple_prefs_set_int("/purple/media/audio/volume/input", value * 100);
}

static void
threshold_value_changed_cb(GtkScale *scale, GtkWidget *label)
{
	int value;
	char *tmp;

	value = (int)gtk_range_get_value(GTK_RANGE(scale));
	tmp = g_strdup_printf(_("Silence threshold: %d%%"), value);
	gtk_label_set_label(GTK_LABEL(label), tmp);
	g_free(tmp);

	purple_prefs_set_int("/purple/media/audio/silence_threshold", value);
}

static void
make_voice_test(GtkWidget *vbox)
{
	GtkWidget *test;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *level;
	GtkWidget *volume;
	GtkWidget *threshold;
	char *tmp;

	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_CAT_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new(_("Volume:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	volume = gtk_volume_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), volume, TRUE, TRUE, 0);
	gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume),
			purple_prefs_get_int("/purple/media/audio/volume/input") / 100.0);
	g_signal_connect(volume, "value-changed",
	                 G_CALLBACK(volume_changed_cb), NULL);

	tmp = g_strdup_printf(_("Silence threshold: %d%%"),
	                      purple_prefs_get_int("/purple/media/audio/silence_threshold"));
	label = gtk_label_new(tmp);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	g_free(tmp);
	threshold = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
		0, 100, 1);
	gtk_box_pack_start(GTK_BOX(vbox), threshold, FALSE, FALSE, 0);
	gtk_range_set_value(GTK_RANGE(threshold),
			purple_prefs_get_int("/purple/media/audio/silence_threshold"));
	gtk_scale_set_draw_value(GTK_SCALE(threshold), FALSE);
	g_signal_connect(threshold, "value-changed",
	                 G_CALLBACK(threshold_value_changed_cb), label);

	test = gtk_toggle_button_new_with_label(_("Test Audio"));
	gtk_box_pack_start(GTK_BOX(vbox), test, FALSE, FALSE, 0);

	level = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), level, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(level, FALSE);

	voice_volume = volume;
	voice_level = level;
	voice_threshold = threshold;
	g_signal_connect(test, "toggled",
	                 G_CALLBACK(toggle_voice_test_cb), NULL);
}

static GstElement *
create_video_pipeline(void)
{
	GstElement *pipeline;
	GstElement *src, *sink;

	pipeline = gst_pipeline_new("videotest");
	src = create_test_element(PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SRC);
	sink = create_test_element(PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SINK);

	g_object_set_data(G_OBJECT(pipeline), "sink", sink);

	gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
	gst_element_link_many(src, sink, NULL);

	return pipeline;
}

static void
video_test_destroy_cb(GtkWidget *w, gpointer data)
{
	if (!video_pipeline)
		return;

	gst_element_set_state(video_pipeline, GST_STATE_NULL);
	gst_object_unref(video_pipeline);
	video_pipeline = NULL;
}

static void
window_id_cb(GstBus *bus, GstMessage *msg, gulong window_id)
{
	if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ELEMENT
#if GST_CHECK_VERSION(1,0,0)
	 || !gst_is_video_overlay_prepare_window_handle_message(msg))
#else
	/* there may be have-xwindow-id also, in case something went wrong */
	 || !gst_structure_has_name(msg->structure, "prepare-xwindow-id"))
#endif
		return;

	g_signal_handlers_disconnect_matched(bus,
	                                     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	                                     0, 0, NULL, window_id_cb,
	                                     (gpointer)window_id);

#if GST_CHECK_VERSION(1,0,0)
	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg)),
	                                    window_id);
#elif GST_CHECK_VERSION(0,10,31)
	gst_x_overlay_set_window_handle(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)),
	                                window_id);
#else
	gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(GST_MESSAGE_SRC(msg)),
	                             window_id);
#endif
}

static void
enable_video_test(void)
{
	GstBus *bus;
	GdkWindow *window = gtk_widget_get_window(video_drawing_area);
	gulong window_id = 0;

#ifdef GDK_WINDOWING_WIN32
	if (GDK_IS_WIN32_WINDOW(window))
		window_id = GPOINTER_TO_UINT(GDK_WINDOW_HWND(window));
	else
#endif
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_WINDOW(window))
		window_id = gdk_x11_window_get_xid(window);
	else
#endif
#ifdef GDK_WINDOWING_QUARTZ
	if (GDK_IS_QUARTZ_WINDOW(window))
		window_id = (gulong)gdk_quartz_window_get_nsview(window);
	else
#endif
		g_warning("Unsupported GDK backend");
#if !(defined(GDK_WINDOWING_WIN32) \
   || defined(GDK_WINDOWING_X11) \
   || defined(GDK_WINDOWING_QUARTZ))
#	error "Unsupported GDK windowing system"
#endif

	video_pipeline = create_video_pipeline();
	bus = gst_pipeline_get_bus(GST_PIPELINE(video_pipeline));
#if GST_CHECK_VERSION(1,0,0)
	gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, NULL, NULL);
#else
	gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, NULL);
#endif
	g_signal_connect(bus, "sync-message::element",
	                 G_CALLBACK(window_id_cb), (gpointer)window_id);
	gst_object_unref(bus);

	gst_element_set_state(GST_ELEMENT(video_pipeline), GST_STATE_PLAYING);
}

static void
toggle_video_test_cb(GtkToggleButton *test, gpointer data)
{
	if (gtk_toggle_button_get_active(test)) {
		enable_video_test();
		g_signal_connect(test, "destroy",
		                 G_CALLBACK(video_test_destroy_cb), NULL);
		g_signal_connect(prefsnotebook, "switch-page",
		                 G_CALLBACK(vv_test_switch_page_cb), test);
	} else {
		g_object_disconnect(test, "any-signal::destroy",
		                    G_CALLBACK(video_test_destroy_cb), NULL,
		                    NULL);
		g_object_disconnect(prefsnotebook, "any-signal::switch-page",
		                    G_CALLBACK(vv_test_switch_page_cb), test,
		                    NULL);
		video_test_destroy_cb(NULL, NULL);
	}
}

static void
make_video_test(GtkWidget *vbox)
{
	GtkWidget *test;
	GtkWidget *video;
#if GTK_CHECK_VERSION(3,0,0)
	GdkRGBA color = {0.0, 0.0, 0.0, 1.0};
#else
	GdkColor color = {0, 0, 0, 0};
#endif

	video_drawing_area = video = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox), video, TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3,0,0)
	gtk_widget_override_background_color(video, GTK_STATE_FLAG_NORMAL, &color);
#else
	gtk_widget_modify_bg(video, GTK_STATE_NORMAL, &color);
#endif
	gtk_widget_set_size_request(GTK_WIDGET(video), 240, 180);

	test = gtk_toggle_button_new_with_label(_("Test Video"));
	gtk_box_pack_start(GTK_BOX(vbox), test, FALSE, FALSE, 0);

	g_signal_connect(test, "toggled",
	                 G_CALLBACK(toggle_video_test_cb), NULL);
}

static void
vv_plugin_changed_cb(const gchar *name, PurplePrefType type,
                     gconstpointer value, gpointer data)
{
	GtkWidget *vbox = data;
	GtkSizeGroup *sg;
	GtkWidget *widget;
	gchar *pref;
	GList *devices;

	sg = g_object_get_data(G_OBJECT(vbox), "size-group");
	widget = g_object_get_data(G_OBJECT(vbox), "device-hbox");
	gtk_widget_destroy(widget);

	pref = g_strdup(name);
	strcpy(pref + strlen(pref) - strlen("plugin"), "device");
	devices = get_vv_element_devices(value);
	if (g_list_find_custom(devices, purple_prefs_get_string(pref),
		(GCompareFunc)strcmp) == NULL)
	{
		GList *next = g_list_next(devices);
		if (next)
			purple_prefs_set_string(pref, next->data);
	}
	widget = pidgin_prefs_dropdown_from_list(vbox, _("_Device"),
	                                         PURPLE_PREF_STRING, pref, devices);
	g_list_free_full(devices, g_free);
	gtk_size_group_add_widget(sg, widget);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5);

	g_object_set_data(G_OBJECT(vbox), "device-hbox",
	                  gtk_widget_get_parent(widget));
	g_signal_connect_swapped(widget, "destroy", G_CALLBACK(g_free), pref);

	/* Refresh test viewers */
	if (strstr(name, "audio") && voice_pipeline) {
		voice_test_destroy_cb(NULL, NULL);
		enable_voice_test();
	} else if(strstr(name, "video") && video_pipeline) {
		video_test_destroy_cb(NULL, NULL);
		enable_video_test();
	}
}

static void
make_vv_frame(GtkWidget *parent, GtkSizeGroup *sg,
              const gchar *name, const gchar **plugin_strs,
              const gchar *plugin_pref, const gchar *device_pref)
{
	GtkWidget *vbox, *widget;
	GList *plugins, *devices;

	vbox = pidgin_make_frame(parent, name);

	/* Setup plugin preference */
	plugins = get_vv_element_plugins(plugin_strs);
	widget = pidgin_prefs_dropdown_from_list(vbox, _("_Plugin"),
	                                         PURPLE_PREF_STRING, plugin_pref,
	                                         plugins);
	g_list_free(plugins);
	gtk_size_group_add_widget(sg, widget);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5);

	/* Setup device preference */
	devices = get_vv_element_devices(purple_prefs_get_string(plugin_pref));
	if (g_list_find_custom(devices, purple_prefs_get_string(device_pref),
		(GCompareFunc)strcmp) == NULL)
	{
		GList *next = g_list_next(devices);
		if (next)
			purple_prefs_set_string(device_pref, next->data);
	}
	widget = pidgin_prefs_dropdown_from_list(vbox, _("_Device"),
	                                         PURPLE_PREF_STRING, device_pref,
	                                         devices);
	g_list_free_full(devices, g_free);
	gtk_size_group_add_widget(sg, widget);
	gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5);

	widget = gtk_widget_get_parent(widget);
	g_object_set_data(G_OBJECT(vbox), "size-group", sg);
	g_object_set_data(G_OBJECT(vbox), "device-hbox", widget);
	purple_prefs_connect_callback(vbox, plugin_pref, vv_plugin_changed_cb,
	                              vbox);
	g_signal_connect_swapped(vbox, "destroy",
	                         G_CALLBACK(purple_prefs_disconnect_by_handle), vbox);
}

static GtkWidget *
vv_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkSizeGroup *sg;

	ret = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = pidgin_make_frame(ret, _("Audio"));
	make_vv_frame(vbox, sg, _("Input"), AUDIO_SRC_PLUGINS,
	              PIDGIN_PREFS_ROOT "/vvconfig/audio/src/plugin",
	              PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device");
	make_vv_frame(vbox, sg, _("Output"), AUDIO_SINK_PLUGINS,
	              PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/plugin",
	              PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device");
	make_voice_test(vbox);

	vbox = pidgin_make_frame(ret, _("Video"));
	make_vv_frame(vbox, sg, _("Input"), VIDEO_SRC_PLUGINS,
	              PIDGIN_PREFS_ROOT "/vvconfig/video/src/plugin",
	              PIDGIN_PREFS_ROOT "/vvconfig/video/src/device");
	make_vv_frame(vbox, sg, _("Output"), VIDEO_SINK_PLUGINS,
	              PIDGIN_PREFS_ROOT "/vvconfig/video/sink/plugin",
	              PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device");
	make_video_test(vbox);

	gtk_widget_show_all(ret);

	return ret;
}
#endif

static int
prefs_notebook_add_page(const char *text, GtkWidget *page, int ind)
{
	return gtk_notebook_append_page(GTK_NOTEBOOK(prefsnotebook), page, gtk_label_new(text));
}

static void
prefs_notebook_init(void)
{
	prefs_notebook_add_page(_("Interface"), interface_page(), notebook_page++);

#ifndef _WIN32
	/* We use the registered default browser in windows */
	/* if the user is running Mac OS X, hide the browsers tab */
	if(purple_running_osx() == FALSE)
		prefs_notebook_add_page(_("Browser"), browser_page(), notebook_page++);
#endif

	prefs_notebook_add_page(_("Conversations"), conv_page(), notebook_page++);
	prefs_notebook_add_page(_("Logging"), logging_page(), notebook_page++);
	prefs_notebook_add_page(_("Network"), network_page(), notebook_page++);
	prefs_notebook_add_page(_("Proxy"), proxy_page(), notebook_page++);
	prefs_notebook_add_page(_("Password Storage"), keyring_page(), notebook_page++);

	prefs_notebook_add_page(_("Sounds"), sound_page(), notebook_page++);
	prefs_notebook_add_page(_("Status / Idle"), away_page(), notebook_page++);
	prefs_notebook_add_page(_("Themes"), theme_page(), notebook_page++);
#ifdef USE_VV
	prefs_notebook_add_page(_("Voice/Video"), vv_page(), notebook_page++);
#endif
}

void
pidgin_prefs_show(void)
{
	GtkWidget *vbox;
	GtkWidget *notebook;
	GtkWidget *button;

	if (prefs) {
		gtk_window_present(GTK_WINDOW(prefs));
		return;
	}

	/* copy the preferences to tmp values...
	 * I liked "take affect immediately" Oh well :-( */
	/* (that should have been "effect," right?) */

	/* Back to instant-apply! I win!  BU-HAHAHA! */

	/* Create the window */
#if GTK_CHECK_VERSION(3,0,0)
	prefs = pidgin_create_dialog(_("Preferences"), 0, "preferences", FALSE);
#else
	prefs = pidgin_create_dialog(_("Preferences"), PIDGIN_HIG_BORDER, "preferences", FALSE);
#endif
	g_signal_connect(G_OBJECT(prefs), "destroy",
					 G_CALLBACK(delete_prefs), NULL);

	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(prefs), FALSE, PIDGIN_HIG_BORDER);

	/* The notebook */
	prefsnotebook = notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
	gtk_box_pack_start(GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(prefsnotebook);

	button = pidgin_dialog_add_button(GTK_DIALOG(prefs), GTK_STOCK_CLOSE, NULL, NULL);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), prefs);

	prefs_notebook_init();

	/* Refresh the list of themes before showing the preferences window */
	prefs_themes_refresh();

	/* Show everything. */
	gtk_widget_show(prefs);
}

static void
set_bool_pref(GtkWidget *w, const char *key)
{
	purple_prefs_set_bool(key,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
}

GtkWidget *
pidgin_prefs_checkbox(const char *text, const char *key, GtkWidget *page)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
								 purple_prefs_get_bool(key));

	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(set_bool_pref), (char *)key);

	gtk_widget_show(button);

	return button;
}

static void
smiley_theme_pref_cb(const char *name, PurplePrefType type,
					 gconstpointer value, gpointer data)
{
	const gchar *theme_name = value;
	GList *themes, *it;

	if (g_strcmp0(theme_name, "none") == 0) {
		purple_smiley_theme_set_current(NULL);
		return;
	}

	/* XXX: could be cached when initializing prefs view */
	themes = pidgin_smiley_theme_get_all();

	for (it = themes; it; it = g_list_next(it)) {
		PidginSmileyTheme *theme = it->data;

		if (g_strcmp0(pidgin_smiley_theme_get_name(theme), theme_name))
			continue;

		purple_smiley_theme_set_current(PURPLE_SMILEY_THEME(theme));
	}
}

void
pidgin_prefs_init(void)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "");
	purple_prefs_add_none("/plugins/gtk");

#ifndef _WIN32
	/* Browsers */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/browsers");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/browsers/place", PIDGIN_BROWSER_DEFAULT);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/browser", "xdg-open");
#endif

	/* Plugins */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/plugins");
	purple_prefs_add_path_list(PIDGIN_PREFS_ROOT "/plugins/loaded", NULL);

	/* File locations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/filelocations");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_save_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_open_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", "");

	/* Themes */
	prefs_themes_init();

	/* Conversation Themes */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/theme", "Default");

	/* Smiley Themes */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/smileys");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/smileys/theme", "Default");

	/* Smiley Callbacks */
	purple_prefs_connect_callback(&prefs, PIDGIN_PREFS_ROOT "/smileys/theme",
								smiley_theme_pref_cb, NULL);

#ifdef USE_VV
	/* Voice/Video */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio/src");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/plugin", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/plugin", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video/src");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/plugin", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video/sink");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/plugin", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device", "");
#endif

	pidgin_prefs_update_old();
}

void
pidgin_prefs_update_old(void)
{
	/* Rename some old prefs */
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_ims", "/purple/logging/log_ims");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_chats", "/purple/logging/log_chats");
	purple_prefs_rename("/purple/conversations/placement",
					  PIDGIN_PREFS_ROOT "/conversations/placement");

	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/im/raise_on_events", "/plugins/gtk/X11/notify/method_raise");

	purple_prefs_rename_boolean_toggle(PIDGIN_PREFS_ROOT "/conversations/ignore_colors",
									 PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting");

	/*
	 * This path pref changed to a string, so migrate. I know this will
	 * break things for and confuse users that use multiple versions with
	 * the same config directory, but I'm not inclined to want to deal with
	 * that at the moment. -- rekkanoryo
	 */
	if (purple_prefs_exists(PIDGIN_PREFS_ROOT "/browsers/command") &&
		purple_prefs_get_pref_type(PIDGIN_PREFS_ROOT "/browsers/command") ==
			PURPLE_PREF_PATH)
	{
		const char *str = purple_prefs_get_path(
			PIDGIN_PREFS_ROOT "/browsers/command");
		purple_prefs_set_string(
			PIDGIN_PREFS_ROOT "/browsers/manual_command", str);
		purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/command");
	}

	/* Remove some no-longer-used prefs */
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/auto_expand_contacts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/button_style");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/grey_idle_buddies");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_group_count");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_warning_level");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/tooltip_delay");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/button_type");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ctrl_enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/escape_closes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/html_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/icons_on_tabs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/send_formatting");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_smileys");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_urls_as_links");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/smiley_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_bgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_fgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_font");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_size");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/old_tab_complete");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/tab_completion");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/hide_on_send");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/color_nicks");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_fonts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_font_sizes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/passthrough_unknown_commands");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/debug/timestamps");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/idle");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/logging/individual_logs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/signon");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/silent_signon");

	/* Convert old queuing prefs to hide_new 3-way pref. */
	if (purple_prefs_exists("/plugins/gtk/docklet/queue_messages") &&
	    purple_prefs_get_bool("/plugins/gtk/docklet/queue_messages"))
	{
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "always");
	}
	else if (purple_prefs_exists(PIDGIN_PREFS_ROOT "/away/queue_messages") &&
	         purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/away/queue_messages"))
	{
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "away");
	}
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away/queue_messages");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away");
	purple_prefs_remove("/plugins/gtk/docklet/queue_messages");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_height");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_height");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/x",
			PIDGIN_PREFS_ROOT "/conversations/im/x");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/y",
			PIDGIN_PREFS_ROOT "/conversations/im/y");

	/* Fixup vvconfig plugin prefs */
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/src/plugin")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/plugin",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/src/plugin"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/src/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/src/device"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/sink/plugin")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/plugin",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/sink/plugin"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/sink/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/sink/device"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/video/src/plugin")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/plugin",
				purple_prefs_get_string("/plugins/core/vvconfig/video/src/plugin"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/video/src/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/device",
				purple_prefs_get_string("/plugins/core/vvconfig/video/src/device"));
	}
	if (purple_prefs_exists("/plugins/gtk/vvconfig/video/sink/plugin")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/plugin",
				purple_prefs_get_string("/plugins/gtk/vvconfig/video/sink/plugin"));
	}
	if (purple_prefs_exists("/plugins/gtk/vvconfig/video/sink/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device",
				purple_prefs_get_string("/plugins/gtk/vvconfig/video/sink/device"));
	}

	purple_prefs_remove("/plugins/core/vvconfig");
	purple_prefs_remove("/plugins/gtk/vvconfig");

#ifndef _WIN32
	/* Added in 3.0.0. */
	if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/browsers/place") == 1) {
		/* If the "open link in" pref is set to the old value for "existing
		   window" then change it to "default." */
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/browsers/place",
				PIDGIN_BROWSER_DEFAULT);
	}

	/* Added in 3.0.0. */
	if (g_str_equal(
			purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"),
			"netscape")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", "xdg-open");
	}
#endif /* !_WIN32 */
}

