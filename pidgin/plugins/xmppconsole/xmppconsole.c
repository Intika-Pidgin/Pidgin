/*
 * Purple - XMPP debugging tool
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 *
 */

#include "internal.h"
#include "gtkplugin.h"
#include "version.h"
#include "protocol.h"
#include "xmlnode.h"

#include "gtkutils.h"

#include <gdk/gdkkeysyms.h>

#include "gtk3compat.h"

#define PLUGIN_ID      "gtk-xmpp"
#define PLUGIN_DOMAIN  (g_quark_from_static_string(PLUGIN_ID))

typedef struct {
	PurpleConnection *gc;
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *dropdown;
	GtkTextBuffer *buffer;
	struct {
		GtkTextTag *info;
		GtkTextTag *incoming;
		GtkTextTag *outgoing;
		GtkTextTag *bracket;
		GtkTextTag *tag;
		GtkTextTag *attr;
		GtkTextTag *value;
		GtkTextTag *xmlns;
	} tags;
	GtkWidget *entry;
	GtkTextBuffer *entry_buffer;
	GtkWidget *sw;
	int count;
	GList *accounts;

	struct {
		GtkPopover *popover;
		GtkEntry *to;
		GtkComboBoxText *type;
	} iq;

	struct {
		GtkPopover *popover;
		GtkEntry *to;
		GtkComboBoxText *type;
		GtkComboBoxText *show;
		GtkEntry *status;
		GtkEntry *priority;
	} presence;

	struct {
		GtkPopover *popover;
		GtkEntry *to;
		GtkComboBoxText *type;
		GtkEntry *body;
		GtkEntry *subject;
		GtkEntry *thread;
	} message;
} XmppConsole;

XmppConsole *console = NULL;
static void *xmpp_console_handle = NULL;

static const gchar *xmpp_prpls[] = {
	"prpl-jabber", "prpl-gtalk", NULL
};

static gboolean
xmppconsole_is_xmpp_account(PurpleAccount *account)
{
	const gchar *prpl_name;
	int i;

	prpl_name = purple_account_get_protocol_id(account);

	i = 0;
	while (xmpp_prpls[i] != NULL) {
		if (purple_strequal(xmpp_prpls[i], prpl_name))
			return TRUE;
		i++;
	}

	return FALSE;
}

static void
purple_xmlnode_append_to_buffer(PurpleXmlNode *node, gint indent_level, GtkTextIter *iter, GtkTextTag *tag)
{
	PurpleXmlNode *c;
	gboolean need_end = FALSE, pretty = TRUE;
	gint i;

	g_return_if_fail(node != NULL);

	for (i = 0; i < indent_level; i++) {
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "\t", 1, tag, NULL);
	}

	gtk_text_buffer_insert_with_tags(console->buffer, iter, "<", 1,
	                                 tag, console->tags.bracket, NULL);
	gtk_text_buffer_insert_with_tags(console->buffer, iter, node->name, -1,
	                                 tag, console->tags.tag, NULL);

	if (node->xmlns) {
		if ((!node->parent ||
		     !node->parent->xmlns ||
		     !purple_strequal(node->xmlns, node->parent->xmlns)) &&
		    !purple_strequal(node->xmlns, "jabber:client"))
		{
			gtk_text_buffer_insert_with_tags(console->buffer, iter, " ", 1,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "xmlns", 5,
			                                 tag, console->tags.attr, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "='", 2,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, node->xmlns, -1,
			                                 tag, console->tags.xmlns, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "'", 1,
			                                 tag, NULL);
		}
	}
	for (c = node->child; c; c = c->next)
	{
		if (c->type == PURPLE_XMLNODE_TYPE_ATTRIB) {
			gtk_text_buffer_insert_with_tags(console->buffer, iter, " ", 1,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, c->name, -1,
			                                 tag, console->tags.attr, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "='", 2,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, c->data, -1,
			                                 tag, console->tags.value, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "'", 1,
			                                 tag, NULL);
		} else if (c->type == PURPLE_XMLNODE_TYPE_TAG || c->type == PURPLE_XMLNODE_TYPE_DATA) {
			if (c->type == PURPLE_XMLNODE_TYPE_DATA)
				pretty = FALSE;
			need_end = TRUE;
		}
	}

	if (need_end) {
		gtk_text_buffer_insert_with_tags(console->buffer, iter, ">", 1,
		                                 tag, console->tags.bracket, NULL);
		if (pretty) {
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "\n", 1,
			                                 tag, NULL);
		}

		for (c = node->child; c; c = c->next)
		{
			if (c->type == PURPLE_XMLNODE_TYPE_TAG) {
				purple_xmlnode_append_to_buffer(c, indent_level + 1, iter, tag);
			} else if (c->type == PURPLE_XMLNODE_TYPE_DATA && c->data_sz > 0) {
				gtk_text_buffer_insert_with_tags(console->buffer, iter, c->data, c->data_sz,
				                                 tag, NULL);
			}
		}

		if (pretty) {
			for (i = 0; i < indent_level; i++) {
				gtk_text_buffer_insert_with_tags(console->buffer, iter, "\t", 1, tag, NULL);
			}
		}
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "<", 1,
		                                 tag, console->tags.bracket, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "/", 1,
		                                 tag, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, node->name, -1,
		                                 tag, console->tags.tag, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, ">", 1,
		                                 tag, console->tags.bracket, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "\n", 1,
		                                 tag, NULL);
	} else {
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "/", 1,
		                                 tag, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, ">", 1,
		                                 tag, console->tags.bracket, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "\n", 1,
		                                 tag, NULL);
	}
}

static void
purple_xmlnode_received_cb(PurpleConnection *gc, PurpleXmlNode **packet, gpointer null)
{
	GtkTextIter iter;

	if (!console || console->gc != gc)
		return;

	gtk_text_buffer_get_end_iter(console->buffer, &iter);
	purple_xmlnode_append_to_buffer(*packet, 0, &iter, console->tags.incoming);
}

static void
purple_xmlnode_sent_cb(PurpleConnection *gc, char **packet, gpointer null)
{
	GtkTextIter iter;
	PurpleXmlNode *node;

	if (!console || console->gc != gc)
		return;
	node = purple_xmlnode_from_str(*packet, -1);

	if (!node)
		return;

	gtk_text_buffer_get_end_iter(console->buffer, &iter);
	purple_xmlnode_append_to_buffer(node, 0, &iter, console->tags.outgoing);
	purple_xmlnode_free(node);
}

static gboolean
message_send_cb(GtkWidget *widget, GdkEventKey *event, gpointer p)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;
	gchar *text;
	GtkTextIter start, end;

	if (event->keyval != GDK_KEY_KP_Enter && event->keyval != GDK_KEY_Return)
		return FALSE;

	gc = console->gc;

	if (gc)
		protocol = purple_connection_get_protocol(gc);

	gtk_text_buffer_get_bounds(console->entry_buffer, &start, &end);
	text = gtk_text_buffer_get_text(console->entry_buffer, &start, &end, FALSE);

	if (protocol)
		purple_protocol_server_iface_send_raw(protocol, gc, text, strlen(text));

	g_free(text);
	gtk_text_buffer_set_text(console->entry_buffer, "", 0);

	return TRUE;
}

static void
entry_changed_cb(GtkTextBuffer *buffer, void *data)
{
	GtkTextIter start, end;
	char *xmlstr, *str;
	GtkTextIter iter;
	int wrapped_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;
	PurpleXmlNode *node;
	GtkStyleContext *style;

	wrapped_lines = 1;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(console->entry), &iter, &oneline);
	while (gtk_text_view_forward_display_line(GTK_TEXT_VIEW(console->entry),
	                                          &iter)) {
		wrapped_lines++;
	}

	lines = gtk_text_buffer_get_line_count(buffer);

	/* Show a maximum of 64 lines */
	lines = MIN(lines, 6);
	wrapped_lines = MIN(wrapped_lines, 6);

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(console->entry));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(console->entry));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(console->entry));

	height = (oneline.height + pad_top + pad_bottom) * lines;
	height += (oneline.height + pad_inside) * (wrapped_lines - lines);

	gtk_widget_set_size_request(console->sw, -1, height + 6);

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	str = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if (!str) {
		return;
	}

	xmlstr = g_strdup_printf("<xml>%s</xml>", str);
	node = purple_xmlnode_from_str(xmlstr, -1);
	style = gtk_widget_get_style_context(console->entry);
	if (node) {
		gtk_style_context_remove_class(style, GTK_STYLE_CLASS_ERROR);
	} else {
		gtk_style_context_add_class(style, GTK_STYLE_CLASS_ERROR);
	}
	g_free(str);
	g_free(xmlstr);
	if (node)
		purple_xmlnode_free(node);
}

static void
load_text_and_set_caret(const gchar *pre_text, const gchar *post_text)
{
	GtkTextIter where;
	GtkTextMark *mark;

	gtk_text_buffer_begin_user_action(console->entry_buffer);

	gtk_text_buffer_set_text(console->entry_buffer, pre_text, -1);

	gtk_text_buffer_get_end_iter(console->entry_buffer, &where);
	mark = gtk_text_buffer_create_mark(console->entry_buffer, NULL, &where, TRUE);

	gtk_text_buffer_insert(console->entry_buffer, &where, post_text, -1);

	gtk_text_buffer_get_iter_at_mark(console->entry_buffer, &where, mark);
	gtk_text_buffer_place_cursor(console->entry_buffer, &where);
	gtk_text_buffer_delete_mark(console->entry_buffer, mark);

	gtk_text_buffer_end_user_action(console->entry_buffer);
}

static void
popover_closed_cb(GtkPopover *popover, gpointer data)
{
	GtkToggleToolButton *button = GTK_TOGGLE_TOOL_BUTTON(data);

	gtk_toggle_tool_button_set_active(button, FALSE);
}

static void
toggle_button_toggled_cb(GtkToolButton *button, gpointer data)
{
	GtkPopover *popover = GTK_POPOVER(data);

	gtk_popover_popup(popover);
}

static void
iq_clicked_cb(GtkWidget *w, gpointer data)
{
	XmppConsole *console = (XmppConsole *)data;
	const gchar *to;
	char *stanza;

	to = gtk_entry_get_text(console->iq.to);
	stanza = g_strdup_printf(
	        "<iq %s%s%s id='console%x' type='%s'>", to && *to ? "to='" : "",
	        to && *to ? to : "", to && *to ? "'" : "", g_random_int(),
	        gtk_combo_box_text_get_active_text(console->iq.type));
	load_text_and_set_caret(stanza, "</iq>");
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);

	/* Reset everything. */
	gtk_entry_set_text(console->iq.to, "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->iq.type), 0);
	gtk_popover_popdown(console->iq.popover);
}

static void
presence_clicked_cb(GtkWidget *w, gpointer data)
{
	XmppConsole *console = (XmppConsole *)data;
	const gchar *to, *status, *priority;
	gchar *type, *show;
	char *stanza;

	to = gtk_entry_get_text(console->presence.to);
	type = gtk_combo_box_text_get_active_text(console->presence.type);
	if (purple_strequal(type, "default")) {
		g_free(type);
		type = g_strdup("");
	}
	show = gtk_combo_box_text_get_active_text(console->presence.show);
	if (purple_strequal(show, "default")) {
		g_free(show);
		show = g_strdup("");
	}
	status = gtk_entry_get_text(console->presence.status);
	priority = gtk_entry_get_text(console->presence.priority);
	if (purple_strequal(priority, "0"))
		priority = "";

	stanza = g_strdup_printf("<presence %s%s%s id='console%x' %s%s%s>"
	                         "%s%s%s%s%s%s%s%s%s",
	                         *to ? "to='" : "",
	                         *to ? to : "",
	                         *to ? "'" : "",
	                         g_random_int(),

	                         *type ? "type='" : "",
	                         *type ? type : "",
	                         *type ? "'" : "",

	                         *show ? "<show>" : "",
	                         *show ? show : "",
	                         *show ? "</show>" : "",

	                         *status ? "<status>" : "",
	                         *status ? status : "",
	                         *status ? "</status>" : "",

	                         *priority ? "<priority>" : "",
	                         *priority ? priority : "",
	                         *priority ? "</priority>" : "");

	load_text_and_set_caret(stanza, "</presence>");
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);
	g_free(type);
	g_free(show);

	/* Reset everything. */
	gtk_entry_set_text(console->presence.to, "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->presence.type), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->presence.show), 0);
	gtk_entry_set_text(console->presence.status, "");
	gtk_entry_set_text(console->presence.priority, "0");
	gtk_popover_popdown(console->presence.popover);
}

static void
message_clicked_cb(GtkWidget *w, gpointer data)
{
	XmppConsole *console = (XmppConsole *)data;
	const gchar *to, *body, *thread, *subject;
	char *stanza;

	to = gtk_entry_get_text(console->message.to);
	body = gtk_entry_get_text(console->message.body);
	thread = gtk_entry_get_text(console->message.thread);
	subject = gtk_entry_get_text(console->message.subject);

	stanza = g_strdup_printf(
	        "<message %s%s%s id='console%x' type='%s'>"
	        "%s%s%s%s%s%s%s%s%s",

	        *to ? "to='" : "", *to ? to : "", *to ? "'" : "",
	        g_random_int(),
	        gtk_combo_box_text_get_active_text(console->message.type),

	        *body ? "<body>" : "", *body ? body : "",
	        *body ? "</body>" : "",

	        *subject ? "<subject>" : "", *subject ? subject : "",
	        *subject ? "</subject>" : "",

	        *thread ? "<thread>" : "", *thread ? thread : "",
	        *thread ? "</thread>" : "");

	load_text_and_set_caret(stanza, "</message>");
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);

	/* Reset everything. */
	gtk_entry_set_text(console->message.to, "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->message.type), 0);
	gtk_entry_set_text(console->message.body, "");
	gtk_entry_set_text(console->message.subject, "0");
	gtk_entry_set_text(console->message.thread, "0");
	gtk_popover_popdown(console->message.popover);
}

static void
signing_on_cb(PurpleConnection *gc)
{
	PurpleAccount *account;

	if (!console)
		return;

	account = purple_connection_get_account(gc);
	if (!xmppconsole_is_xmpp_account(account))
		return;

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(console->dropdown),
		purple_account_get_username(account));
	console->accounts = g_list_append(console->accounts, gc);
	console->count++;

	if (console->count == 1) {
		console->gc = gc;
		gtk_text_buffer_set_text(console->buffer, "", 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(console->dropdown), 0);
	} else
		gtk_widget_show_all(console->hbox);
}

static void
signed_off_cb(PurpleConnection *gc)
{
	int i;

	if (!console)
		return;

	i = g_list_index(console->accounts, gc);
	if (i == -1)
		return;

	gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(console->dropdown), i);
	console->accounts = g_list_remove(console->accounts, gc);
	console->count--;

	if (gc == console->gc) {
		GtkTextIter end;
		gtk_text_buffer_get_end_iter(console->buffer, &end);
		gtk_text_buffer_insert_with_tags(console->buffer, &end, _("Logged out."), -1,
		                                 console->tags.info, NULL);
		console->gc = NULL;
	}
}

static void
console_destroy(GtkWidget *window, gpointer nul)
{
	g_list_free(console->accounts);
	g_free(console);
	console = NULL;
}

static void
dropdown_changed_cb(GtkComboBox *widget, gpointer nul)
{
	if (!console)
		return;

	console->gc = g_list_nth_data(console->accounts, gtk_combo_box_get_active(GTK_COMBO_BOX(console->dropdown)));
	gtk_text_buffer_set_text(console->buffer, "", 0);
}

static void
create_console(PurplePluginAction *action)
{
	GtkBuilder *builder;
	GList *connections;
	GtkCssProvider *entry_css;
	GtkStyleContext *context;

	if (console) {
		gtk_window_present(GTK_WINDOW(console->window));
		return;
	}

	console = g_new0(XmppConsole, 1);

	builder = gtk_builder_new_from_resource(
	        "/im/pidgin/Pidgin/Plugin/XMPPConsole/console.ui");
	gtk_builder_set_translation_domain(builder, PACKAGE);
	console->window = GTK_WIDGET(
	        gtk_builder_get_object(builder, "PidginXmppConsole"));
	gtk_builder_add_callback_symbol(builder, "console_destroy",
	                                G_CALLBACK(console_destroy));

	console->hbox = GTK_WIDGET(gtk_builder_get_object(builder, "hbox"));
	console->dropdown =
	        GTK_WIDGET(gtk_builder_get_object(builder, "dropdown"));
	for (connections = purple_connections_get_all(); connections; connections = connections->next) {
		PurpleConnection *gc = connections->data;
		if (xmppconsole_is_xmpp_account(purple_connection_get_account(gc))) {
			console->count++;
			console->accounts = g_list_append(console->accounts, gc);
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(console->dropdown),
						  purple_account_get_username(purple_connection_get_account(gc)));
			if (!console->gc)
				console->gc = gc;
		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->dropdown), 0);
	gtk_builder_add_callback_symbol(builder, "dropdown_changed_cb",
	                                G_CALLBACK(dropdown_changed_cb));

	console->buffer =
	        GTK_TEXT_BUFFER(gtk_builder_get_object(builder, "buffer"));
	console->tags.info =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.info"));
	console->tags.incoming =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.incoming"));
	console->tags.outgoing =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.outgoing"));
	console->tags.bracket =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.bracket"));
	console->tags.tag =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.tag"));
	console->tags.attr =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.attr"));
	console->tags.value =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.value"));
	console->tags.xmlns =
	        GTK_TEXT_TAG(gtk_builder_get_object(builder, "tags.xmlns"));

	if (console->count == 0) {
		GtkTextIter start, end;
		gtk_text_buffer_set_text(console->buffer, _("Not connected to XMPP"), -1);
		gtk_text_buffer_get_bounds(console->buffer, &start, &end);
		gtk_text_buffer_apply_tag(console->buffer, console->tags.info, &start, &end);
	}

	/* Popover for <iq/> button. */
	console->iq.popover =
	        GTK_POPOVER(gtk_builder_get_object(builder, "iq.popover"));
	console->iq.to = GTK_ENTRY(gtk_builder_get_object(builder, "iq.to"));
	console->iq.type =
	        GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "iq.type"));

	/* Popover for <presence/> button. */
	console->presence.popover = GTK_POPOVER(
	        gtk_builder_get_object(builder, "presence.popover"));
	console->presence.to =
	        GTK_ENTRY(gtk_builder_get_object(builder, "presence.to"));
	console->presence.type = GTK_COMBO_BOX_TEXT(
	        gtk_builder_get_object(builder, "presence.type"));
	console->presence.show = GTK_COMBO_BOX_TEXT(
	        gtk_builder_get_object(builder, "presence.show"));
	console->presence.status =
	        GTK_ENTRY(gtk_builder_get_object(builder, "presence.status"));
	console->presence.priority =
	        GTK_ENTRY(gtk_builder_get_object(builder, "presence.priority"));

	/* Popover for <message/> button. */
	console->message.popover =
	        GTK_POPOVER(gtk_builder_get_object(builder, "message.popover"));
	console->message.to =
	        GTK_ENTRY(gtk_builder_get_object(builder, "message.to"));
	console->message.type = GTK_COMBO_BOX_TEXT(
	        gtk_builder_get_object(builder, "message.type"));
	console->message.body =
	        GTK_ENTRY(gtk_builder_get_object(builder, "message.body"));
	console->message.subject =
	        GTK_ENTRY(gtk_builder_get_object(builder, "message.subject"));
	console->message.thread =
	        GTK_ENTRY(gtk_builder_get_object(builder, "message.thread"));

	gtk_builder_add_callback_symbols(
	        builder, "toggle_button_toggled_cb",
	        G_CALLBACK(toggle_button_toggled_cb), "popover_closed_cb",
	        G_CALLBACK(popover_closed_cb), "iq_clicked_cb",
	        G_CALLBACK(iq_clicked_cb), "presence_clicked_cb",
	        G_CALLBACK(presence_clicked_cb), "message_clicked_cb",
	        G_CALLBACK(message_clicked_cb), NULL);

	console->entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry"));
	entry_css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(entry_css,
	                                "textview." GTK_STYLE_CLASS_ERROR " text {background-color:#ffcece;}",
	                                -1, NULL);
	context = gtk_widget_get_style_context(console->entry);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(entry_css),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	console->entry_buffer = GTK_TEXT_BUFFER(
	        gtk_builder_get_object(builder, "entry_buffer"));
	gtk_builder_add_callback_symbol(builder, "message_send_cb",
	                                G_CALLBACK(message_send_cb));

	console->sw = GTK_WIDGET(gtk_builder_get_object(builder, "sw"));
	gtk_builder_add_callback_symbol(builder, "entry_changed_cb",
	                                G_CALLBACK(entry_changed_cb));

	entry_changed_cb(console->entry_buffer, NULL);

	gtk_widget_show_all(console->window);
	if (console->count < 2)
		gtk_widget_hide(console->hbox);

	gtk_builder_connect_signals(builder, console);
	g_object_unref(builder);
}

static GList *
actions(PurplePlugin *plugin)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("XMPP Console"), create_console);
	l = g_list_append(l, act);

	return l;
}

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Sean Egan <seanegan@gmail.com>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           PLUGIN_ID,
		"name",         N_("XMPP Console"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol utility"),
		"summary",      N_("Send and receive raw XMPP stanzas."),
		"description",  N_("This plugin is useful for debugging XMPP servers "
		                   "or clients."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"actions-cb",   actions,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	int i;
	gboolean any_registered = FALSE;

	xmpp_console_handle = plugin;

	i = 0;
	while (xmpp_prpls[i] != NULL) {
		PurpleProtocol *xmpp;

		xmpp = purple_protocols_find(xmpp_prpls[i]);
		i++;

		if (!xmpp)
			continue;
		any_registered = TRUE;

		purple_signal_connect(xmpp, "jabber-receiving-xmlnode",
			xmpp_console_handle,
			PURPLE_CALLBACK(purple_xmlnode_received_cb), NULL);
		purple_signal_connect(xmpp, "jabber-sending-text",
			xmpp_console_handle,
			PURPLE_CALLBACK(purple_xmlnode_sent_cb), NULL);
	}

	if (!any_registered) {
		g_set_error_literal(error, PLUGIN_DOMAIN, 0,
		                    _("No XMPP protocol is loaded."));
		return FALSE;
	}

	purple_signal_connect(purple_connections_get_handle(), "signing-on",
		plugin, PURPLE_CALLBACK(signing_on_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-off",
		plugin, PURPLE_CALLBACK(signed_off_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (console)
		gtk_widget_destroy(console->window);
	return TRUE;
}

PURPLE_PLUGIN_INIT(xmppconsole, plugin_query, plugin_load, plugin_unload);
