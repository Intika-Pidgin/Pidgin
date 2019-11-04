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
#include "pidgin.h"

#ifdef HAVE_X11
# include <X11/Xlib.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <talkatu.h>

#include "account.h"
#include "attention.h"
#include "action.h"
#include "cmds.h"
#include "core.h"
#include "debug.h"
#include "glibcompat.h"
#include "idle.h"
#include "image-store.h"
#include "log.h"
#include "notify.h"
#include "plugins.h"
#include "protocol.h"
#include "request.h"
#include "smiley-parser.h"
#include "util.h"
#include "version.h"

#include "gtkinternal.h"
#include "gtkdnd-hints.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkconvwin.h"
#include "gtkdialogs.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkstyle.h"
#include "gtkutils.h"
#include "pidgingdkpixbuf.h"
#include "pidgininvitedialog.h"
#include "pidginlog.h"
#include "pidginmessage.h"
#include "pidginstock.h"
#include "pidgintooltip.h"

#include "gtknickcolors.h"

#define GTK_TOOLTIPS_VAR gtkconv->tooltips
#include "gtk3compat.h"

#define ADD_MESSAGE_HISTORY_AT_ONCE 100

/*
 * A GTK+ Instant Message pane.
 */
struct _PidginImPane
{
	GtkWidget *block;
	GtkWidget *send_file;
	GtkWidget *sep1;
	GtkWidget *sep2;
	GtkWidget *check;
	GtkWidget *progress;
	guint32 typing_timer;

	/* Buddy icon stuff */
	GtkWidget *icon_container;
	GtkWidget *icon;
	gboolean show_icon;
	gboolean animate;
	GdkPixbufAnimation *anim;
	GdkPixbufAnimationIter *iter;
	guint32 icon_timer;
};

/*
 * GTK+ Chat panes.
 */
struct _PidginChatPane
{
	GtkWidget *count;
	GtkWidget *list;
	GtkWidget *topic_text;
};

#define CLOSE_CONV_TIMEOUT_SECS  (10 * 60)

#define AUTO_RESPONSE "&lt;AUTO-REPLY&gt; : "

typedef enum
{
	PIDGIN_CONV_SET_TITLE			= 1 << 0,
	PIDGIN_CONV_BUDDY_ICON			= 1 << 1,
	PIDGIN_CONV_MENU			= 1 << 2,
	PIDGIN_CONV_TAB_ICON			= 1 << 3,
	PIDGIN_CONV_TOPIC			= 1 << 4,
	PIDGIN_CONV_SMILEY_THEME		= 1 << 5,
	PIDGIN_CONV_COLORIZE_TITLE		= 1 << 6,
	PIDGIN_CONV_E2EE			= 1 << 7
}PidginConvFields;

enum {
	CONV_ICON_COLUMN,
	CONV_TEXT_COLUMN,
	CONV_EMBLEM_COLUMN,
	CONV_PROTOCOL_ICON_COLUMN,
	CONV_NUM_COLUMNS
} PidginInfopaneColumns;

#define	PIDGIN_CONV_ALL	((1 << 7) - 1)

/* XXX: These color defines shouldn't really be here. But the nick-color
 * generation algorithm uses them, so keeping these around until we fix that. */
#define DEFAULT_SEND_COLOR "#204a87"
#define DEFAULT_HIGHLIGHT_COLOR "#AF7F00"

#define BUDDYICON_SIZE_MIN    32
#define BUDDYICON_SIZE_MAX    96

#define MIN_LUMINANCE_CONTRAST_RATIO 4.5

#define NICK_COLOR_GENERATE_COUNT 220
static GArray *generated_nick_colors = NULL;

/* These probably won't conflict with any WebKit values. */
#define PIDGIN_DRAG_BLIST_NODE (1337)
#define PIDGIN_DRAG_IM_CONTACT (31337)

static GtkWidget *invite_dialog = NULL;
static GtkWidget *warn_close_dialog = NULL;

static PidginConvWindow *hidden_convwin = NULL;
static GList *window_list = NULL;

/* Lists of status icons at all available sizes for use as window icons */
static GList *available_list = NULL;
static GList *away_list = NULL;
static GList *busy_list = NULL;
static GList *xa_list = NULL;
static GList *offline_list = NULL;
static GHashTable *protocol_lists = NULL;
static GHashTable *e2ee_stock = NULL;

static gboolean update_send_to_selection(PidginConvWindow *win);
static void generate_send_to_items(PidginConvWindow *win);

/* Prototypes. <-- because Paco-Paco hates this comment. */
static gboolean infopane_entry_activate(PidginConversation *gtkconv);
static void got_typing_keypress(PidginConversation *gtkconv, gboolean first);
static void gray_stuff_out(PidginConversation *gtkconv);
static void add_chat_user_common(PurpleChatConversation *chat, PurpleChatUser *cb, const char *old_name);
static void pidgin_conv_updated(PurpleConversation *conv, PurpleConversationUpdateType type);
static void conv_set_unseen(PurpleConversation *gtkconv, PidginUnseenState state);
static void gtkconv_set_unseen(PidginConversation *gtkconv, PidginUnseenState state);
static void update_typing_icon(PidginConversation *gtkconv);
static void update_typing_message(PidginConversation *gtkconv, const char *message);
gboolean pidgin_conv_has_focus(PurpleConversation *conv);
static GArray* generate_nick_colors(guint numcolors, GdkRGBA background);
gdouble luminance(GdkRGBA color);
static gboolean color_is_visible(GdkRGBA foreground, GdkRGBA background, gdouble min_contrast_ratio);
static GtkTextTag *get_buddy_tag(PurpleChatConversation *chat, const char *who, PurpleMessageFlags flag, gboolean create);
static void pidgin_conv_update_fields(PurpleConversation *conv, PidginConvFields fields);
static void focus_out_from_menubar(GtkWidget *wid, PidginConvWindow *win);
static void pidgin_conv_tab_pack(PidginConvWindow *win, PidginConversation *gtkconv);
static gboolean infopane_press_cb(GtkWidget *widget, GdkEventButton *e, PidginConversation *conv);
static void hide_conv(PidginConversation *gtkconv, gboolean closetimer);

static void pidgin_conv_set_position_size(PidginConvWindow *win, int x, int y,
		int width, int height);
static gboolean pidgin_conv_xy_to_right_infopane(PidginConvWindow *win, int x, int y);

static const GdkRGBA *
get_nick_color(PidginConversation *gtkconv, const gchar *name)
{
	static GdkRGBA col;

	if (name == NULL) {
		col.red = col.green = col.blue = 0;
		col.alpha = 1;
		return &col;
	}

	col = g_array_index(gtkconv->nick_colors, GdkRGBA,
		g_str_hash(name) % gtkconv->nick_colors->len);

	return &col;
}

static PurpleBlistNode *
get_conversation_blist_node(PurpleConversation *conv)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleBlistNode *node = NULL;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		node = PURPLE_BLIST_NODE(purple_blist_find_buddy(account, purple_conversation_get_name(conv)));
		node = node ? node->parent : NULL;
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		node = PURPLE_BLIST_NODE(purple_blist_find_chat(account, purple_conversation_get_name(conv)));
	}

	return node;
}

/**************************************************************************
 * Callbacks
 **************************************************************************/

static gboolean
close_this_sucker(gpointer data)
{
	PidginConversation *gtkconv = data;
	GList *list = g_list_copy(gtkconv->convs);
	g_list_free_full(list, g_object_unref);
	return FALSE;
}

static gboolean
close_conv_cb(GtkButton *button, PidginConversation *gtkconv)
{
	/* We are going to destroy the conversations immediately only if the 'close immediately'
	 * preference is selected. Otherwise, close the conversation after a reasonable timeout
	 * (I am going to consider 10 minutes as a 'reasonable timeout' here.
	 * For chats, close immediately if the chat is not in the buddylist, or if the chat is
	 * not marked 'Persistent' */
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account = purple_conversation_get_account(conv);
	const char *name = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/close_immediately"))
			close_this_sucker(gtkconv);
		else
			hide_conv(gtkconv, TRUE);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *chat = purple_blist_find_chat(account, name);
		if (!chat ||
				!purple_blist_node_get_bool(&chat->node, "gtk-persistent"))
			close_this_sucker(gtkconv);
		else
			hide_conv(gtkconv, FALSE);
	}

	return TRUE;
}

static gboolean
lbox_size_allocate_cb(GtkWidget *w, GtkAllocation *allocation, gpointer data)
{
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width", allocation->width == 1 ? 0 : allocation->width);

	return FALSE;
}

static const char *
pidgin_get_cmd_prefix(void)
{
	return "/";
}

static PurpleCmdRet
say_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	purple_conversation_send(conv, args[0]);

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
me_command_cb(PurpleConversation *conv,
              const char *cmd, char **args, char **error, void *data)
{
	char *tmp;

	tmp = g_strdup_printf("/me %s", args[0]);
	purple_conversation_send(conv, tmp);

	g_free(tmp);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
debug_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	char *tmp, *markup;

	if (!g_ascii_strcasecmp(args[0], "version")) {
		tmp = g_strdup_printf("Using Pidgin v%s with libpurple v%s.",
				DISPLAY_VERSION, purple_core_get_version());
	} else if (!g_ascii_strcasecmp(args[0], "plugins")) {
		/* Show all the loaded plugins, including plugins marked internal.
		 * This is intentional, since third party protocols are often sources of bugs, and some
		 * plugin loaders can also be buggy.
		 */
		GString *str = g_string_new("Loaded Plugins: ");
		const GList *plugins = purple_plugins_get_loaded();
		if (plugins) {
			for (; plugins; plugins = plugins->next) {
				GPluginPluginInfo *info = GPLUGIN_PLUGIN_INFO(
				        purple_plugin_get_info(
				                PURPLE_PLUGIN(plugins->data)));
				str = g_string_append(
				        str,
				        gplugin_plugin_info_get_name(info));

				if (plugins->next)
					str = g_string_append(str, ", ");
			}
		} else {
			str = g_string_append(str, "(none)");
		}

		tmp = g_string_free(str, FALSE);
	} else if (!g_ascii_strcasecmp(args[0], "unsafe")) {
		if (purple_debug_is_unsafe()) {
			purple_debug_set_unsafe(FALSE);
			purple_conversation_write_system_message(conv,
				_("Unsafe debugging is now disabled."),
				PURPLE_MESSAGE_NO_LOG);
		} else {
			purple_debug_set_unsafe(TRUE);
			purple_conversation_write_system_message(conv,
				_("Unsafe debugging is now enabled."),
				PURPLE_MESSAGE_NO_LOG);
		}

		return PURPLE_CMD_RET_OK;
	} else if (!g_ascii_strcasecmp(args[0], "verbose")) {
		if (purple_debug_is_verbose()) {
			purple_debug_set_verbose(FALSE);
			purple_conversation_write_system_message(conv,
				_("Verbose debugging is now disabled."),
				PURPLE_MESSAGE_NO_LOG);
		} else {
			purple_debug_set_verbose(TRUE);
			purple_conversation_write_system_message(conv,
				_("Verbose debugging is now enabled."),
				PURPLE_MESSAGE_NO_LOG);
		}

		return PURPLE_CMD_RET_OK;
	} else {
		purple_conversation_write_system_message(conv,
			_("Supported debug options are: plugins, version, unsafe, verbose"),
			PURPLE_MESSAGE_NO_LOG);
		return PURPLE_CMD_RET_OK;
	}

	markup = g_markup_escape_text(tmp, -1);
	purple_conversation_send(conv, markup);

	g_free(tmp);
	g_free(markup);
	return PURPLE_CMD_RET_OK;
}

static void clear_conversation_scrollback_cb(PurpleConversation *conv,
                                             void *data)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	if (PIDGIN_CONVERSATION(conv)) {
		gtkconv->last_flags = 0;
	}
}

static PurpleCmdRet
clear_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	purple_conversation_clear_message_history(conv);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
clearall_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GList *l;
	for (l = purple_conversations_get_all(); l != NULL; l = l->next)
		purple_conversation_clear_message_history(PURPLE_CONVERSATION(l->data));

	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
help_command_cb(PurpleConversation *conv,
                 const char *cmd, char **args, char **error, void *data)
{
	GList *l, *text;
	GString *s;

	if (args[0] != NULL) {
		s = g_string_new("");
		text = purple_cmd_help(conv, args[0]);

		if (text) {
			for (l = text; l; l = l->next)
				if (l->next)
					g_string_append_printf(s, "%s\n", (char *)l->data);
				else
					g_string_append_printf(s, "%s", (char *)l->data);
		} else {
			g_string_append(s, _("No such command (in this context)."));
		}
	} else {
		s = g_string_new(_("Use \"/help &lt;command&gt;\" for help with a "
				"specific command.<br/>The following commands are available "
				"in this context:<br/>"));

		text = purple_cmd_list(conv);
		for (l = text; l; l = l->next)
			if (l->next)
				g_string_append_printf(s, "%s, ", (char *)l->data);
			else
				g_string_append_printf(s, "%s.", (char *)l->data);
		g_list_free(text);
	}

	purple_conversation_write_system_message(conv, s->str, PURPLE_MESSAGE_NO_LOG);
	g_string_free(s, TRUE);

	return PURPLE_CMD_RET_OK;
}

static void
send_history_add(PidginConversation *gtkconv, const char *message)
{
	GList *first;

	first = g_list_first(gtkconv->send_history);
	g_free(first->data);
	first->data = g_strdup(message);
	gtkconv->send_history = g_list_prepend(first, NULL);
}

static gboolean
check_for_and_do_command(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	GtkWidget *view = NULL;
	GtkTextBuffer *buffer = NULL;
	gchar *cmd;
	const gchar *prefix;
	gboolean retval = FALSE;

	gtkconv = PIDGIN_CONVERSATION(conv);
	prefix = pidgin_get_cmd_prefix();

	view = talkatu_editor_get_view(TALKATU_EDITOR(gtkconv->editor));
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

	cmd = talkatu_buffer_get_plain_text(TALKATU_BUFFER(buffer));

	if (cmd && g_str_has_prefix(cmd, prefix)) {
		PurpleCmdStatus status;
		char *error, *cmdline, *markup, *send_history;

		send_history = talkatu_markup_get_html(buffer, NULL);
		send_history_add(gtkconv, send_history);
		g_free(send_history);

		cmdline = cmd + strlen(prefix);

		if (purple_strequal(cmdline, "xyzzy")) {
			purple_conversation_write_system_message(conv,
				"Nothing happens", PURPLE_MESSAGE_NO_LOG);
			g_free(cmd);
			return TRUE;
		}

		/* Docs are unclear on whether or not prefix should be removed from
		 * the markup so, ignoring for now.  Notably if the markup is
		 * `<b>/foo arg1</b>` we now have to move the bold tag around?
		 * - gk 20190709 */
		markup = talkatu_markup_get_html(buffer, NULL);
		status = purple_cmd_do_command(conv, cmdline, markup, &error);
		g_free(markup);

		switch (status) {
			case PURPLE_CMD_STATUS_OK:
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_NOT_FOUND:
				{
					PurpleProtocol *protocol = NULL;
					PurpleConnection *gc;

					if ((gc = purple_conversation_get_connection(conv)))
						protocol = purple_connection_get_protocol(gc);

					if ((protocol != NULL) && (purple_protocol_get_options(protocol) & OPT_PROTO_SLASH_COMMANDS_NATIVE)) {
						char *spaceslash;

						/* If the first word in the entered text has a '/' in it, then the user
						 * probably didn't mean it as a command. So send the text as message. */
						spaceslash = cmdline;
						while (*spaceslash && *spaceslash != ' ' && *spaceslash != '/')
							spaceslash++;

						if (*spaceslash != '/') {
							purple_conversation_write_system_message(conv,
								_("Unknown command."), PURPLE_MESSAGE_NO_LOG);
							retval = TRUE;
						}
					}
					break;
				}
			case PURPLE_CMD_STATUS_WRONG_ARGS:
				purple_conversation_write_system_message(conv,
					_("Syntax Error:  You typed the wrong "
					"number of arguments to that command."),
					PURPLE_MESSAGE_NO_LOG);
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_FAILED:
				purple_conversation_write_system_message(conv,
					error ? error : _("Your command failed for an unknown reason."),
					PURPLE_MESSAGE_NO_LOG);
				g_free(error);
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_WRONG_TYPE:
				if(PURPLE_IS_IM_CONVERSATION(conv))
					purple_conversation_write_system_message(conv,
						_("That command only works in chats, not IMs."),
						PURPLE_MESSAGE_NO_LOG);
				else
					purple_conversation_write_system_message(conv,
						_("That command only works in IMs, not chats."),
						PURPLE_MESSAGE_NO_LOG);
				retval = TRUE;
				break;
			case PURPLE_CMD_STATUS_WRONG_PROTOCOL:
				purple_conversation_write_system_message(conv,
					_("That command doesn't work on this protocol."),
					PURPLE_MESSAGE_NO_LOG);
				retval = TRUE;
				break;
		}
	}

	g_free(cmd);

	return retval;
}

static void
send_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleMessageFlags flags = 0;
	GtkTextBuffer *buffer = NULL;
	gchar *content;

	account = purple_conversation_get_account(conv);

	buffer = talkatu_editor_get_buffer(TALKATU_EDITOR(gtkconv->editor));

	if (check_for_and_do_command(conv)) {
		talkatu_buffer_clear(TALKATU_BUFFER(buffer));
		return;
	}

	if (PURPLE_IS_CHAT_CONVERSATION(conv) &&
		purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv))) {
		return;
	}

	if (!purple_account_is_connected(account)) {
		return;
	}

	content = talkatu_markup_get_html(buffer, NULL);
	if (purple_strequal(content, "")) {
		g_free(content);
		return;
	}

	purple_idle_touch();

	/* XXX: is there a better way to tell if the message has images? */
	// if (strstr(buf, "<img ") != NULL)
	// 	flags |= PURPLE_MESSAGE_IMAGES;

	purple_conversation_send_with_flags(conv, content, flags);

	g_free(content);

	talkatu_buffer_clear(TALKATU_BUFFER(buffer));
	gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);
}

static void
add_remove_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleAccount *account;
	const char *name;
	PurpleConversation *conv = gtkconv->active_conv;

	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b;

		b = purple_blist_find_buddy(account, name);
		if (b != NULL)
			pidgin_dialogs_remove_buddy(b);
		else if (account != NULL && purple_account_is_connected(account))
			purple_blist_request_add_buddy(account, (char *)name, NULL, NULL);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *c;

		c = purple_blist_find_chat(account, name);
		if (c != NULL)
			pidgin_dialogs_remove_chat(c);
		else if (account != NULL && purple_account_is_connected(account))
			purple_blist_request_add_chat(account, NULL, NULL, name);
	}
}

static void chat_do_info(PidginConversation *gtkconv, const char *who)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(gtkconv->active_conv);
	PurpleConnection *gc;

	if ((gc = purple_conversation_get_connection(gtkconv->active_conv))) {
		pidgin_retrieve_user_info_in_chat(gc, who, purple_chat_conversation_get_id(chat));
	}
}


static void
info_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		pidgin_retrieve_user_info(purple_conversation_get_connection(conv),
					  purple_conversation_get_name(conv));
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		/* Get info of the person currently selected in the GtkTreeView */
		PidginChatPane *gtkchat;
		GtkTreeIter iter;
		GtkTreeModel *model;
		GtkTreeSelection *sel;
		char *name;

		gtkchat = gtkconv->u.chat;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

		if (gtk_tree_selection_get_selected(sel, NULL, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &name, -1);
		else
			return;

		chat_do_info(gtkconv, name);
		g_free(name);
	}
}

static void
block_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv);

	if (account != NULL && purple_account_is_connected(account))
		pidgin_request_add_block(account, purple_conversation_get_name(conv));
}

static void
unblock_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv);

	if (account != NULL && purple_account_is_connected(account))
		pidgin_request_add_permit(account, purple_conversation_get_name(conv));
}

static void
do_invite(GtkWidget *w, int resp, gpointer data)
{
	PidginInviteDialog *dialog = PIDGIN_INVITE_DIALOG(w);
	PurpleChatConversation *chat = pidgin_invite_dialog_get_conversation(dialog);
	const gchar *contact, *message;

	if (resp == GTK_RESPONSE_ACCEPT) {
		contact = pidgin_invite_dialog_get_contact(dialog);
		if (!g_ascii_strcasecmp(contact, ""))
			return;

		message = pidgin_invite_dialog_get_message(dialog);

		purple_serv_chat_invite(purple_conversation_get_connection(PURPLE_CONVERSATION(chat)),
						 purple_chat_conversation_get_id(chat),
						 message, contact);
	}

	g_clear_pointer(&invite_dialog, gtk_widget_destroy);
}

static void
invite_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(gtkconv->active_conv);

	if (invite_dialog == NULL) {
		invite_dialog = pidgin_invite_dialog_new(chat);

		/* Connect the signals. */
		g_signal_connect(G_OBJECT(invite_dialog), "response",
						 G_CALLBACK(do_invite), NULL);
	}

	gtk_widget_show_all(invite_dialog);
}

static void
menu_new_conv_cb(GtkAction *action, gpointer data)
{
	pidgin_dialogs_im();
}

static void
menu_join_chat_cb(GtkAction *action, gpointer data)
{
	pidgin_blist_joinchat_show();
}

static void
savelog_writefile_cb(void *user_data, const char *filename)
{
	PurpleConversation *conv = (PurpleConversation *)user_data;
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	GtkTextBuffer *buffer = NULL;
	FILE *fp;
	const char *name;
	gchar *text;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		purple_notify_error(PIDGIN_CONVERSATION(conv), NULL,
			_("Unable to open file."), NULL,
			purple_request_cpar_from_conversation(conv));
		return;
	}

	name = purple_conversation_get_name(conv);

	fprintf(fp, "<html>\n");
	fprintf(fp, "<head>\n");
	fprintf(fp, "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n");
	fprintf(fp, "<title>%s</title>\n", name);
	fprintf(fp, "</head>\n");

	fprintf(fp, "<body>\n");
	fprintf(fp, _("<h1>Conversation with %s</h1>\n"), name);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->history));
	text = talkatu_markup_get_html(buffer, NULL);
	fprintf(fp, "%s", text);
	g_free(text);
	fprintf(fp, "\n</body>\n");

	fprintf(fp, "</html>\n");
	fclose(fp);
}

/*
 * It would be kinda cool if this gave the option of saving a
 * plaintext v. HTML file.
 */
static void
menu_save_as_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleBuddy *buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
	const char *name;
	gchar *buf;
	gchar *c;

	if (buddy != NULL)
		name = purple_buddy_get_contact_alias(buddy);
	else
		name = purple_normalize(account, purple_conversation_get_name(conv));

	buf = g_strdup_printf("%s.html", name);
	for (c = buf ; *c ; c++)
	{
		if (*c == '/' || *c == '\\')
			*c = ' ';
	}
	purple_request_file(PIDGIN_CONVERSATION(conv), _("Save Conversation"),
		buf, TRUE, G_CALLBACK(savelog_writefile_cb), NULL,
		purple_request_cpar_from_conversation(conv), conv);

	g_free(buf);
}

static void
menu_view_log_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;
	PurpleLogType type;
	PidginBuddyList *gtkblist;
	const char *name;
	PurpleAccount *account;
	GSList *buddies;
	GSList *cur;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (PURPLE_IS_IM_CONVERSATION(conv))
		type = PURPLE_LOG_IM;
	else if (PURPLE_IS_CHAT_CONVERSATION(conv))
		type = PURPLE_LOG_CHAT;
	else
		return;

	gtkblist = pidgin_blist_get_default_gtk_blist();

	pidgin_set_cursor(gtkblist->window, GDK_WATCH);
	pidgin_set_cursor(win->window, GDK_WATCH);

	name = purple_conversation_get_name(conv);
	account = purple_conversation_get_account(conv);

	buddies = purple_blist_find_buddies(account, name);
	for (cur = buddies; cur != NULL; cur = cur->next)
	{
		PurpleBlistNode *node = cur->data;
		if ((node != NULL) && ((node->prev != NULL) || (node->next != NULL)))
		{
			pidgin_log_show_contact((PurpleContact *)node->parent);
			g_slist_free(buddies);
			pidgin_clear_cursor(gtkblist->window);
			pidgin_clear_cursor(win->window);
			return;
		}
	}
	g_slist_free(buddies);

	pidgin_log_show(type, name, account);

	pidgin_clear_cursor(gtkblist->window);
	pidgin_clear_cursor(win->window);
}

static void
menu_clear_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);
	purple_conversation_clear_message_history(conv);
}

static void
menu_find_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *gtkwin = data;
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(gtkwin);
	gtk_widget_show_all(gtkconv->quickfind_container);
	gtk_widget_grab_focus(gtkconv->quickfind_entry);
}

#ifdef USE_VV
static void
menu_initiate_media_call_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = (PidginConvWindow *)data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);
	PurpleAccount *account = purple_conversation_get_account(conv);

	purple_protocol_initiate_media(account,
			purple_conversation_get_name(conv),
			action == win->menu->audio_call ? PURPLE_MEDIA_AUDIO :
			action == win->menu->video_call ? PURPLE_MEDIA_VIDEO :
			action == win->menu->audio_video_call ? PURPLE_MEDIA_AUDIO |
			PURPLE_MEDIA_VIDEO : PURPLE_MEDIA_NONE);
}
#endif

static void
menu_send_file_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		purple_serv_send_file(purple_conversation_get_connection(conv), purple_conversation_get_name(conv), NULL);
	}

}

static void
menu_get_attention_cb(GObject *obj, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		int index;
		if ((GtkAction *)obj == win->menu->get_attention)
			index = 0;
		else
			index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(obj), "index"));
		purple_protocol_send_attention(purple_conversation_get_connection(conv),
			purple_conversation_get_name(conv), index);
	}
}

static void
menu_add_pounce_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_gtkconv(win)->active_conv;

	pidgin_pounce_editor_show(purple_conversation_get_account(conv),
								purple_conversation_get_name(conv), NULL);
}

static void
menu_alias_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;
	PurpleAccount *account;
	const char *name;

	conv    = pidgin_conv_window_get_active_conversation(win);
	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b;

		b = purple_blist_find_buddy(account, name);
		if (b != NULL)
			pidgin_dialogs_alias_buddy(b);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *c;

		c = purple_blist_find_chat(account, name);
		if (c != NULL)
			pidgin_dialogs_alias_chat(c);
	}
}

static void
menu_get_info_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	info_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_invite_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	invite_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_block_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	block_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_unblock_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	unblock_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static void
menu_add_remove_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	add_remove_cb(NULL, PIDGIN_CONVERSATION(conv));
}

static gboolean
close_already(gpointer data)
{
	g_object_unref(data);
	return FALSE;
}

static void
hide_conv(PidginConversation *gtkconv, gboolean closetimer)
{
	GList *list;

	purple_signal_emit(pidgin_conversations_get_handle(),
			"conversation-hiding", gtkconv);

	for (list = g_list_copy(gtkconv->convs); list; list = g_list_delete_link(list, list)) {
		PurpleConversation *conv = list->data;
		if (closetimer) {
			guint timer = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "close-timer"));
			if (timer)
				g_source_remove(timer);
			timer = g_timeout_add_seconds(CLOSE_CONV_TIMEOUT_SECS, close_already, conv);
			g_object_set_data(G_OBJECT(conv), "close-timer", GINT_TO_POINTER(timer));
		}
		pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
		pidgin_conv_window_add_gtkconv(hidden_convwin, gtkconv);
	}
}

static void
menu_close_conv_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;

	close_conv_cb(NULL, PIDGIN_CONVERSATION(pidgin_conv_window_get_active_conversation(win)));
}

static void
menu_logging_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;
	gboolean logging;
	PurpleBlistNode *node;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL)
		return;

	logging = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));

	if (logging == purple_conversation_is_logging(conv))
		return;

	node = get_conversation_blist_node(conv);

	if (logging)
	{
		/* Enable logging first so the message below can be logged. */
		purple_conversation_set_logging(conv, TRUE);

		purple_conversation_write_system_message(conv,
			_("Logging started. Future messages in this conversation will be logged."), 0);
	}
	else
	{
		purple_conversation_write_system_message(conv,
			_("Logging stopped. Future messages in this conversation will not be logged."), 0);

		/* Disable the logging second, so that the above message can be logged. */
		purple_conversation_set_logging(conv, FALSE);
	}

	/* Save the setting IFF it's different than the pref. */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (logging == purple_prefs_get_bool("/purple/logging/log_ims"))
			purple_blist_node_remove_setting(node, "enable-logging");
		else
			purple_blist_node_set_bool(node, "enable-logging", logging);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		if (logging == purple_prefs_get_bool("/purple/logging/log_chats"))
			purple_blist_node_remove_setting(node, "enable-logging");
		else
			purple_blist_node_set_bool(node, "enable-logging", logging);
	}
}

static void
menu_toolbar_cb(GtkAction *action, gpointer data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar",
	                    gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)));
}

static void
menu_sounds_cb(GtkAction *action, gpointer data)
{
	PidginConvWindow *win = data;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PurpleBlistNode *node;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (!conv)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gtkconv->make_sound =
		gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
	node = get_conversation_blist_node(conv);
	if (node)
		purple_blist_node_set_bool(node, "gtk-mute-sound", !gtkconv->make_sound);
}

static void
chat_do_im(PidginConversation *gtkconv, const char *who)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleProtocol *protocol = NULL;
	gchar *real_who = NULL;

	account = purple_conversation_get_account(conv);
	g_return_if_fail(account != NULL);

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	protocol = purple_connection_get_protocol(gc);

	if (protocol)
		real_who = purple_protocol_chat_iface_get_user_real_name(protocol, gc,
				purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)), who);

	if(!who && !real_who)
		return;

	pidgin_dialogs_im_with_user(account, real_who ? real_who : who);

	g_free(real_who);
}

static void pidgin_conv_chat_update_user(PurpleChatUser *chatuser);

static void
ignore_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(gtkconv->active_conv);
	const char *name;

	name = g_object_get_data(G_OBJECT(w), "user_data");

	if (name == NULL)
		return;

	if (purple_chat_conversation_is_ignored_user(chat, name))
		purple_chat_conversation_unignore(chat, name);
	else
		purple_chat_conversation_ignore(chat, name);

	pidgin_conv_chat_update_user(purple_chat_conversation_find_user(chat, name));
}

static void
menu_chat_im_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	const char *who = g_object_get_data(G_OBJECT(w), "user_data");

	chat_do_im(gtkconv, who);
}

static void
menu_chat_send_file_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleProtocol *protocol;
	PurpleConversation *conv = gtkconv->active_conv;
	const char *who = g_object_get_data(G_OBJECT(w), "user_data");
	PurpleConnection *gc  = purple_conversation_get_connection(conv);
	gchar *real_who = NULL;

	g_return_if_fail(gc != NULL);

	protocol = purple_connection_get_protocol(gc);

	if (protocol)
		real_who = purple_protocol_chat_iface_get_user_real_name(protocol, gc,
				purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)), who);

	purple_serv_send_file(gc, real_who ? real_who : who, NULL);
	g_free(real_who);
}

static void
menu_chat_info_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	char *who;

	who = g_object_get_data(G_OBJECT(w), "user_data");

	chat_do_info(gtkconv, who);
}

static void
menu_chat_add_remove_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;
	PurpleBuddy *b;
	char *name;

	account = purple_conversation_get_account(conv);
	name    = g_object_get_data(G_OBJECT(w), "user_data");
	b       = purple_blist_find_buddy(account, name);

	if (b != NULL)
		pidgin_dialogs_remove_buddy(b);
	else if (account != NULL && purple_account_is_connected(account))
		purple_blist_request_add_buddy(account, name, NULL, NULL);

	gtk_widget_grab_focus(PIDGIN_CONVERSATION(conv)->entry);
}

static GtkWidget *
create_chat_menu(PurpleChatConversation *chat, const char *who, PurpleConnection *gc)
{
	static GtkWidget *menu = NULL;
	PurpleProtocol *protocol = NULL;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	PurpleAccount *account = purple_conversation_get_account(conv);
	gboolean is_me = FALSE;
	GtkWidget *button;
	PurpleBuddy *buddy = NULL;

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if (menu)
		gtk_widget_destroy(menu);

	if (purple_strequal(purple_chat_conversation_get_nick(chat), purple_normalize(account, who)))
		is_me = TRUE;

	menu = gtk_menu_new();

	if (!is_me) {
                button = pidgin_new_menu_item(menu, _("IM"),
                                PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW,
                                G_CALLBACK(menu_chat_im_cb),
                                PIDGIN_CONVERSATION(conv));

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);


		if (protocol && PURPLE_IS_PROTOCOL_XFER(protocol))
		{
			gboolean can_receive_file = TRUE;

			button = pidgin_new_menu_item(menu, _("Send File"),
				PIDGIN_STOCK_TOOLBAR_SEND_FILE, G_CALLBACK(menu_chat_send_file_cb),
				PIDGIN_CONVERSATION(conv));

			if (gc == NULL) {
				can_receive_file = FALSE;
			} else {
				gchar *real_who = NULL;
				real_who = purple_protocol_chat_iface_get_user_real_name(protocol, gc,
					purple_chat_conversation_get_id(chat), who);

				if (!purple_protocol_xfer_can_receive(
						PURPLE_PROTOCOL_XFER(protocol),
						gc, real_who ? real_who : who)) {
					can_receive_file = FALSE;
				}

				g_free(real_who);
			}

			if (!can_receive_file)
				gtk_widget_set_sensitive(button, FALSE);
			else
				g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
		}


		if (purple_chat_conversation_is_ignored_user(chat, who))
			button = pidgin_new_menu_item(menu, _("Un-Ignore"),
                                        PIDGIN_STOCK_IGNORE, G_CALLBACK(ignore_cb),
                                        PIDGIN_CONVERSATION(conv));
		else
			button = pidgin_new_menu_item(menu, _("Ignore"),
                                        PIDGIN_STOCK_IGNORE, G_CALLBACK(ignore_cb),
                                        PIDGIN_CONVERSATION(conv));

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, get_info)) {
		button = pidgin_new_menu_item(menu, _("Info"),
                                PIDGIN_STOCK_TOOLBAR_USER_INFO,
                                G_CALLBACK(menu_chat_info_cb),
                                PIDGIN_CONVERSATION(conv));

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (!is_me && protocol && !(purple_protocol_get_options(protocol) & OPT_PROTO_UNIQUE_CHATNAME) && PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, add_buddy)) {
		if ((buddy = purple_blist_find_buddy(account, who)) != NULL)
			button = pidgin_new_menu_item(menu, _("Remove"),
                                        GTK_STOCK_REMOVE,
                                        G_CALLBACK(menu_chat_add_remove_cb),
                                        PIDGIN_CONVERSATION(conv));
		else
			button = pidgin_new_menu_item(menu, _("Add"),
                                        GTK_STOCK_ADD,
                                        G_CALLBACK(menu_chat_add_remove_cb),
                                        PIDGIN_CONVERSATION(conv));

		if (gc == NULL)
			gtk_widget_set_sensitive(button, FALSE);
		else
			g_object_set_data_full(G_OBJECT(button), "user_data", g_strdup(who), g_free);
	}

	if (buddy != NULL)
	{
		if (purple_account_is_connected(account))
			pidgin_append_blist_node_proto_menu(menu, purple_account_get_connection(account),
												  (PurpleBlistNode *)buddy);
		pidgin_append_blist_node_extended_menu(menu, (PurpleBlistNode *)buddy);
		gtk_widget_show_all(menu);
	}

	return menu;
}


static gint
gtkconv_chat_popup_menu_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *menu;
	gchar *who;

	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = purple_account_get_connection(account);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));
	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);
	menu = create_chat_menu (PURPLE_CHAT_CONVERSATION(conv), who, gc);
	pidgin_menu_popup_at_treeview_selection(menu, widget);
	g_free(who);

	return TRUE;
}


static gint
right_click_chat_cb(GtkWidget *widget, GdkEventButton *event,
					PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleAccount *account;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	gchar *who;
	int x, y;

	gtkchat = gtkconv->u.chat;
	account = purple_conversation_get_account(conv);
	gc      = purple_account_get_connection(account);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(gtkchat->list),
								  event->x, event->y, &path, &column, &x, &y);

	if (path == NULL)
		return FALSE;

	gtk_tree_selection_select_path(GTK_TREE_SELECTION(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list))), path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(gtkchat->list),
							 path, NULL, FALSE);
	gtk_widget_grab_focus(GTK_WIDGET(gtkchat->list));

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);

	/* emit chat-nick-clicked signal */
	if (event->type == GDK_BUTTON_PRESS) {
		gint plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
					pidgin_conversations_get_handle(), "chat-nick-clicked",
					conv, who, event->button));
		if (plugin_return)
			goto handled;
	}

	if (event->button == GDK_BUTTON_PRIMARY && event->type == GDK_2BUTTON_PRESS) {
		chat_do_im(gtkconv, who);
	} else if (gdk_event_triggers_context_menu((GdkEvent *)event)) {
		GtkWidget *menu = create_chat_menu (PURPLE_CHAT_CONVERSATION(conv), who, gc);
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
	}

handled:
	g_free(who);
	gtk_tree_path_free(path);

	return TRUE;
}

static void
activate_list_cb(GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *column, PidginConversation *gtkconv)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *who;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);
	chat_do_im(gtkconv, who);

	g_free(who);
}

static void
move_to_next_unread_tab(PidginConversation *gtkconv, gboolean forward)
{
	PidginConversation *next_gtkconv = NULL, *most_active = NULL;
	PidginUnseenState unseen_state = PIDGIN_UNSEEN_NONE;
	PidginConvWindow *win;
	int initial, i, total, diff;

	win   = gtkconv->win;
	initial = gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
	                                gtkconv->tab_cont);
	total = pidgin_conv_window_get_gtkconv_count(win);
	/* By adding total here, the moduli calculated later will always have two
	 * positive arguments. x % y where x < 0 is not guaranteed to return a
	 * positive number.
	 */
	diff = (forward ? 1 : -1) + total;

	for (i = (initial + diff) % total; i != initial; i = (i + diff) % total) {
		next_gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, i);
		if (next_gtkconv->unseen_state > unseen_state) {
			most_active = next_gtkconv;
			unseen_state = most_active->unseen_state;
			if(PIDGIN_UNSEEN_NICK == unseen_state) /* highest possible state */
				break;
		}
	}

	if (most_active == NULL) { /* no new messages */
		i = (i + diff) % total;
		most_active = pidgin_conv_window_get_gtkconv_at_index(win, i);
	}

	if (most_active != NULL && most_active != gtkconv)
		pidgin_conv_window_switch_gtkconv(win, most_active);
}

static gboolean
gtkconv_cycle_focus(PidginConversation *gtkconv, GtkDirectionType dir)
{
	PurpleConversation *conv = gtkconv->active_conv;
	gboolean chat = PURPLE_IS_CHAT_CONVERSATION(conv);
	GtkWidget *next = NULL;
	struct {
		GtkWidget *from;
		GtkWidget *to;
	} transitions[] = {
		{gtkconv->entry, gtkconv->history},
		{gtkconv->history, chat ? gtkconv->u.chat->list : gtkconv->entry},
		{chat ? gtkconv->u.chat->list : NULL, gtkconv->entry},
		{NULL, NULL}
	}, *ptr;

	for (ptr = transitions; !next && ptr->from; ptr++) {
		GtkWidget *from, *to;
		if (dir == GTK_DIR_TAB_FORWARD) {
			from = ptr->from;
			to = ptr->to;
		} else {
			from = ptr->to;
			to = ptr->from;
		}
		if (gtk_widget_is_focus(from))
			next = to;
	}

	if (next)
		gtk_widget_grab_focus(next);
	return !!next;
}

static void
update_typing_inserting(PidginConversation *gtkconv)
{
	GtkTextBuffer *buffer = NULL;
	gboolean is_empty = FALSE;

	g_return_if_fail(gtkconv != NULL);

	buffer = talkatu_editor_get_buffer(TALKATU_EDITOR(gtkconv->editor));
	is_empty = talkatu_buffer_get_is_empty(TALKATU_BUFFER(buffer));

	got_typing_keypress(gtkconv, is_empty);
}

static gboolean
update_typing_deleting_cb(PidginConversation *gtkconv)
{
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(gtkconv->active_conv);
	GtkTextBuffer *buffer = NULL;

	buffer = talkatu_editor_get_buffer(TALKATU_EDITOR(gtkconv->editor));

	if (!talkatu_buffer_get_is_empty(TALKATU_BUFFER(buffer))) {
		/* We deleted all the text, so turn off typing. */
		purple_im_conversation_stop_send_typed_timeout(im);

		purple_serv_send_typing(purple_conversation_get_connection(gtkconv->active_conv),
						 purple_conversation_get_name(gtkconv->active_conv),
						 PURPLE_IM_NOT_TYPING);
	} else {
		/* We're deleting, but not all of it, so it counts as typing. */
		got_typing_keypress(gtkconv, FALSE);
	}

	return FALSE;
}

static void
update_typing_deleting(PidginConversation *gtkconv)
{
	GtkTextBuffer *buffer = NULL;

	g_return_if_fail(gtkconv != NULL);

	buffer = talkatu_editor_get_buffer(TALKATU_EDITOR(gtkconv->editor));

	if (!talkatu_buffer_get_is_empty(TALKATU_BUFFER(buffer))) {
		g_timeout_add(0, (GSourceFunc)update_typing_deleting_cb, gtkconv);
	}
}

static gboolean
conv_keypress_common(PidginConversation *gtkconv, GdkEventKey *event)
{
	PidginConvWindow *win;
	int curconv;

	win     = gtkconv->win;
	curconv = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));

	/* clear any tooltips */
	pidgin_tooltip_destroy();

	/* If CTRL was held down... */
	if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval) {
			case GDK_KEY_Page_Down:
			case GDK_KEY_KP_Page_Down:
			case ']':
				if (!pidgin_conv_window_get_gtkconv_at_index(win, curconv + 1))
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
				else
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), curconv + 1);
				return TRUE;
				break;

			case GDK_KEY_Page_Up:
			case GDK_KEY_KP_Page_Up:
			case '[':
				if (!pidgin_conv_window_get_gtkconv_at_index(win, curconv - 1))
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), -1);
				else
					gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), curconv - 1);
				return TRUE;
				break;

			case GDK_KEY_Tab:
			case GDK_KEY_KP_Tab:
			case GDK_KEY_ISO_Left_Tab:
				if (event->state & GDK_SHIFT_MASK) {
					move_to_next_unread_tab(gtkconv, FALSE);
				} else {
					move_to_next_unread_tab(gtkconv, TRUE);
				}

				return TRUE;
				break;

			case GDK_KEY_comma:
				gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
						gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), curconv),
						curconv - 1);
				return TRUE;
				break;

			case GDK_KEY_period:
				gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
						gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), curconv),
						(curconv + 1) % gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)));
				return TRUE;
				break;
			case GDK_KEY_F6:
				if (gtkconv_cycle_focus(gtkconv, event->state & GDK_SHIFT_MASK ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD))
					return TRUE;
				break;
		} /* End of switch */
	}

	/* If ALT (or whatever) was held down... */
	else if (event->state & GDK_MOD1_MASK)
	{
		if (event->keyval > '0' && event->keyval <= '9')
		{
			guint switchto = event->keyval - '1';
			if (switchto < pidgin_conv_window_get_gtkconv_count(win))
				gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), switchto);

			return TRUE;
		}
	}

	/* If neither CTRL nor ALT were held down... */
	else
	{
		switch (event->keyval) {
		case GDK_KEY_F2:
			if (gtk_widget_is_focus(GTK_WIDGET(win->notebook))) {
				infopane_entry_activate(gtkconv);
				return TRUE;
			}
			break;
		case GDK_KEY_F6:
			if (gtkconv_cycle_focus(gtkconv, event->state & GDK_SHIFT_MASK ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD))
				return TRUE;
			break;
		}
	}
	return FALSE;
}

static gboolean
entry_key_press_cb(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	gtkconv  = (PidginConversation *)data;
	conv     = gtkconv->active_conv;

	if (conv_keypress_common(gtkconv, event))
		return TRUE;

	/* If CTRL was held down... */
	if (event->state & GDK_CONTROL_MASK) {
	}
	/* If ALT (or whatever) was held down... */
	else if (event->state & GDK_MOD1_MASK) 	{
	}

	/* If neither CTRL nor ALT were held down... */
	else {
		switch (event->keyval) {
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			if (gtkconv->entry != entry)
				break;
			{
				gint plugin_return;
				plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
							pidgin_conversations_get_handle(), "chat-nick-autocomplete",
							conv, event->state & GDK_SHIFT_MASK));
				return plugin_return;
			}
			break;

		case GDK_KEY_Page_Up:
		case GDK_KEY_KP_Page_Up:
			talkatu_history_page_up(TALKATU_HISTORY(gtkconv->history));
			return TRUE;
			break;

		case GDK_KEY_Page_Down:
		case GDK_KEY_KP_Page_Down:
			talkatu_history_page_down(TALKATU_HISTORY(gtkconv->history));
			return TRUE;
			break;

		case GDK_KEY_KP_Enter:
		case GDK_KEY_Return:
			send_cb(entry, gtkconv);
			return TRUE;
			break;

		}
	}

	if (PURPLE_IS_IM_CONVERSATION(conv) &&
			purple_prefs_get_bool("/purple/conversations/im/send_typing")) {

		switch (event->keyval) {
		case GDK_KEY_BackSpace:
		case GDK_KEY_Delete:
		case GDK_KEY_KP_Delete:
			update_typing_deleting(gtkconv);
			break;
		default:
			update_typing_inserting(gtkconv);
		}
	}

	return FALSE;
}

/*
 * If someone tries to type into the conversation backlog of a
 * conversation window then we yank focus from the conversation backlog
 * and give it to the text entry box so that people can type
 * all the live long day and it will get entered into the entry box.
 */
static gboolean
refocus_entry_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GtkWidget *view = NULL;
	PidginConversation *gtkconv = data;

	/* If we have a valid key for the conversation display, then exit */
	if ((event->state & GDK_CONTROL_MASK) ||
		(event->keyval == GDK_KEY_F6) ||
		(event->keyval == GDK_KEY_F10) ||
		(event->keyval == GDK_KEY_Menu) ||
		(event->keyval == GDK_KEY_Shift_L) ||
		(event->keyval == GDK_KEY_Shift_R) ||
		(event->keyval == GDK_KEY_Control_L) ||
		(event->keyval == GDK_KEY_Control_R) ||
		(event->keyval == GDK_KEY_Escape) ||
		(event->keyval == GDK_KEY_Up) ||
		(event->keyval == GDK_KEY_Down) ||
		(event->keyval == GDK_KEY_Left) ||
		(event->keyval == GDK_KEY_Right) ||
		(event->keyval == GDK_KEY_Page_Up) ||
		(event->keyval == GDK_KEY_KP_Page_Up) ||
		(event->keyval == GDK_KEY_Page_Down) ||
		(event->keyval == GDK_KEY_KP_Page_Down) ||
		(event->keyval == GDK_KEY_Home) ||
		(event->keyval == GDK_KEY_End) ||
		(event->keyval == GDK_KEY_Tab) ||
		(event->keyval == GDK_KEY_KP_Tab) ||
		(event->keyval == GDK_KEY_ISO_Left_Tab))
	{
		if (event->type == GDK_KEY_PRESS)
			return conv_keypress_common(gtkconv, event);
		return FALSE;
	}

	view = talkatu_editor_get_view(TALKATU_EDITOR(gtkconv->editor));
	gtk_widget_grab_focus(view);
	gtk_widget_event(view, (GdkEvent *)event);

	return TRUE;
}

static void
regenerate_options_items(PidginConvWindow *win);

void
pidgin_conv_switch_active_conversation(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PurpleConversation *old_conv;
	PurpleConnectionFlags features;

	g_return_if_fail(conv != NULL);

	gtkconv = PIDGIN_CONVERSATION(conv);
	old_conv = gtkconv->active_conv;

	purple_debug_info("gtkconv", "setting active conversation on toolbar %p\n",
		conv);

	if (old_conv == conv)
		return;

	purple_conversation_close_logs(old_conv);
	gtkconv->active_conv = conv;

	purple_conversation_set_logging(conv,
		gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(gtkconv->win->menu->logging)));

	purple_signal_emit(pidgin_conversations_get_handle(), "conversation-switched", conv);

	gray_stuff_out(gtkconv);
	update_typing_icon(gtkconv);
	g_object_set_data(G_OBJECT(gtkconv->entry), "transient_buddy", NULL);
	regenerate_options_items(gtkconv->win);

	gtk_window_set_title(GTK_WINDOW(gtkconv->win->window),
	                     gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
}

static void
menu_conv_sel_send_cb(GObject *m, gpointer data)
{
	PurpleAccount *account = g_object_get_data(m, "purple_account");
	gchar *name = g_object_get_data(m, "purple_buddy_name");
	PurpleIMConversation *im;

	if (gtk_check_menu_item_get_active((GtkCheckMenuItem*) m) == FALSE)
		return;

	im = purple_im_conversation_new(account, name);
	pidgin_conv_switch_active_conversation(PURPLE_CONVERSATION(im));
}

/**************************************************************************
 * A bunch of buddy icon functions
 **************************************************************************/

static GList *get_protocol_icon_list(PurpleAccount *account)
{
	GList *l = NULL;
	PurpleProtocol *protocol =
			purple_protocols_find(purple_account_get_protocol_id(account));
	const char *protoname = purple_protocol_class_list_icon(protocol, account, NULL);
	l = g_hash_table_lookup(protocol_lists, protoname);
	if (l)
		return l;

	l = g_list_prepend(l, pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_LARGE));
	l = g_list_prepend(l, pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_MEDIUM));
	l = g_list_prepend(l, pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL));

	g_hash_table_insert(protocol_lists, g_strdup(protoname), l);
	return l;
}

static GList *
pidgin_conv_get_tab_icons(PurpleConversation *conv)
{
	PurpleAccount *account = NULL;
	const char *name = NULL;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b = purple_blist_find_buddy(account, name);
		if (b != NULL) {
			PurplePresence *p;
			p = purple_buddy_get_presence(b);
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_AWAY))
				return away_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_UNAVAILABLE))
				return busy_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_EXTENDED_AWAY))
				return xa_list;
			if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_OFFLINE))
				return offline_list;
			else
				return available_list;
		}
	}

	return get_protocol_icon_list(account);
}

static const char *
pidgin_conv_get_icon_stock(PurpleConversation *conv)
{
	PurpleAccount *account = NULL;
	const char *stock = NULL;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	g_return_val_if_fail(account != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		const char *name = NULL;
		PurpleBuddy *b;
		name = purple_conversation_get_name(conv);
		b = purple_blist_find_buddy(account, name);
		if (b != NULL) {
			PurplePresence *p = purple_buddy_get_presence(b);
			PurpleStatus *active = purple_presence_get_active_status(p);
			PurpleStatusType *type = purple_status_get_status_type(active);
			PurpleStatusPrimitive prim = purple_status_type_get_primitive(type);
			stock = pidgin_stock_id_from_status_primitive(prim);
		} else {
			stock = PIDGIN_STOCK_STATUS_PERSON;
		}
	} else {
		stock = PIDGIN_STOCK_STATUS_CHAT;
	}

	return stock;
}

static GdkPixbuf *
pidgin_conv_get_icon(PurpleConversation *conv, GtkWidget *parent, const char *icon_size)
{
	PurpleAccount *account = NULL;
	const char *name = NULL;
	const char *stock = NULL;
	GdkPixbuf *status = NULL;
	GtkIconSize size;

	g_return_val_if_fail(conv != NULL, NULL);

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	/* Use the buddy icon, if possible */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b = purple_blist_find_buddy(account, name);
		if (b != NULL) {
			/* I hate this hack.  It fixes a bug where the pending message icon
			 * displays in the conv tab even though it shouldn't.
			 * A better solution would be great. */
			purple_blist_update_node(NULL, PURPLE_BLIST_NODE(b));
		}
	}

	stock = pidgin_conv_get_icon_stock(conv);
	size = gtk_icon_size_from_name(icon_size);
	status = gtk_widget_render_icon (parent, stock, size, "GtkWidget");
	return status;
}

GdkPixbuf *
pidgin_conv_get_tab_icon(PurpleConversation *conv, gboolean small_icon)
{
	const char *icon_size = small_icon ? PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC : PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL;
	return pidgin_conv_get_icon(conv, PIDGIN_CONVERSATION(conv)->icon, icon_size);
}


static void
update_tab_icon(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	PidginConvWindow *win;
	GList *l;
	GdkPixbuf *emblem = NULL;
	const char *status = NULL;
	const char *infopane_status = NULL;

	g_return_if_fail(conv != NULL);

	gtkconv = PIDGIN_CONVERSATION(conv);
	win = gtkconv->win;
	if (conv != gtkconv->active_conv)
		return;

	status = infopane_status = pidgin_conv_get_icon_stock(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *b = purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
		if (b)
			emblem = pidgin_blist_get_emblem((PurpleBlistNode*)b);
	}

	g_return_if_fail(status != NULL);

	g_object_set(G_OBJECT(gtkconv->icon), "stock", status, NULL);
	g_object_set(G_OBJECT(gtkconv->menu_icon), "stock", status, NULL);

	gtk_list_store_set(GTK_LIST_STORE(gtkconv->infopane_model),
			&(gtkconv->infopane_iter),
			CONV_ICON_COLUMN, infopane_status, -1);

	gtk_list_store_set(GTK_LIST_STORE(gtkconv->infopane_model),
			&(gtkconv->infopane_iter),
			CONV_EMBLEM_COLUMN, emblem, -1);
	if (emblem)
		g_object_unref(emblem);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons")) {
		emblem = pidgin_create_protocol_icon(purple_conversation_get_account(gtkconv->active_conv), PIDGIN_PROTOCOL_ICON_SMALL);
	} else {
		emblem = NULL;
	}

	gtk_list_store_set(GTK_LIST_STORE(gtkconv->infopane_model),
			&(gtkconv->infopane_iter),
			CONV_PROTOCOL_ICON_COLUMN, emblem, -1);
	if (emblem)
		g_object_unref(emblem);

	/* XXX seanegan Why do I have to do this? */
	gtk_widget_queue_resize(gtkconv->infopane);
	gtk_widget_queue_draw(gtkconv->infopane);

	if (pidgin_conv_window_is_active_conversation(conv) &&
		(!PURPLE_IS_IM_CONVERSATION(conv) || gtkconv->u.im->anim == NULL))
	{
		l = pidgin_conv_get_tab_icons(conv);

		gtk_window_set_icon_list(GTK_WINDOW(win->window), l);
	}
}

static gboolean
redraw_icon(gpointer data)
{
	PidginConversation *gtkconv = (PidginConversation *)data;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account;

	GdkPixbuf *buf;
	GdkPixbuf *scale;
	gint delay;
	int scale_width, scale_height;
	int size;

	gtkconv = PIDGIN_CONVERSATION(conv);
	account = purple_conversation_get_account(conv);

	if (!(account && purple_account_get_connection(account))) {
		gtkconv->u.im->icon_timer = 0;
		return FALSE;
	}

	gdk_pixbuf_animation_iter_advance(gtkconv->u.im->iter, NULL);
	buf = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);

	scale_width = gdk_pixbuf_get_width(buf);
	scale_height = gdk_pixbuf_get_height(buf);

	gtk_widget_get_size_request(gtkconv->u.im->icon_container, NULL, &size);
	size = MIN(size, MIN(scale_width, scale_height));
	size = CLAMP(size, BUDDYICON_SIZE_MIN, BUDDYICON_SIZE_MAX);

	if (scale_width == scale_height) {
		scale_width = scale_height = size;
	} else if (scale_height > scale_width) {
		scale_width = size * scale_width / scale_height;
		scale_height = size;
	} else {
		scale_height = size * scale_height / scale_width;
		scale_width = size;
	}

	scale = gdk_pixbuf_scale_simple(buf, scale_width, scale_height,
		GDK_INTERP_BILINEAR);
	if (pidgin_gdk_pixbuf_is_opaque(scale))
		pidgin_gdk_pixbuf_make_round(scale);

	gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->u.im->icon), scale);
	g_object_unref(G_OBJECT(scale));
	gtk_widget_queue_draw(gtkconv->u.im->icon);

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);

	if (delay < 100)
		delay = 100;

	gtkconv->u.im->icon_timer = g_timeout_add(delay, redraw_icon, gtkconv);

	return FALSE;
}

static void
start_anim(GtkWidget *widget, PidginConversation *gtkconv)
{
	int delay;

	if (gtkconv->u.im->anim == NULL)
		return;

	if (gtkconv->u.im->icon_timer != 0)
		return;

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim))
		return;

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);

	if (delay < 100)
		delay = 100;

	gtkconv->u.im->icon_timer = g_timeout_add(delay, redraw_icon, gtkconv);
}

static void
remove_icon(GtkWidget *widget, PidginConversation *gtkconv)
{
	GList *children;
	GtkWidget *event;
	PurpleConversation *conv = gtkconv->active_conv;

	g_return_if_fail(conv != NULL);

	gtk_widget_set_size_request(gtkconv->u.im->icon_container, -1, BUDDYICON_SIZE_MIN);
	children = gtk_container_get_children(GTK_CONTAINER(gtkconv->u.im->icon_container));
	if (children) {
		/* We know there's only one child here. It'd be nice to shortcut to the
		   event box, but we can't change the PidginConversation until 3.0 */
		event = (GtkWidget *)children->data;
		gtk_container_remove(GTK_CONTAINER(gtkconv->u.im->icon_container), event);
		g_list_free(children);
	}

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->icon_timer = 0;
	gtkconv->u.im->icon = NULL;
	gtkconv->u.im->anim = NULL;
	gtkconv->u.im->iter = NULL;
	gtkconv->u.im->show_icon = FALSE;
}

static void
saveicon_writefile_cb(void *user_data, const char *filename)
{
	PidginConversation *gtkconv = (PidginConversation *)user_data;
	PurpleIMConversation *im = PURPLE_IM_CONVERSATION(gtkconv->active_conv);
	PurpleBuddyIcon *icon;
	const void *data;
	size_t len;

	icon = purple_im_conversation_get_icon(im);
	data = purple_buddy_icon_get_data(icon, &len);

	if ((len <= 0) || (data == NULL) || !purple_util_write_data_to_file_absolute(filename, data, len)) {
		purple_notify_error(gtkconv, NULL, _("Unable to save icon file to disk."), NULL, NULL);
	}
}

static void
custom_icon_sel_cb(const char *filename, gpointer data)
{
	if (filename) {
		PurpleContact *contact = data;

		purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, filename);
	}
	g_object_set_data(G_OBJECT(data), "buddy-icon-chooser", NULL);
}

static void
set_custom_icon_cb(GtkWidget *widget, PurpleContact *contact)
{
	GtkFileChooserNative *win = NULL;

	/* Should not happen as menu item should be disabled. */
	g_return_if_fail(contact != NULL);

	win = g_object_get_data(G_OBJECT(contact), "buddy-icon-chooser");
	if (win == NULL) {
		GtkMenu *menu = GTK_MENU(gtk_widget_get_parent(widget));
		GtkWidget *toplevel =
		        gtk_widget_get_toplevel(gtk_menu_get_attach_widget(menu));
		win = pidgin_buddy_icon_chooser_new(GTK_WINDOW(toplevel),
		                                    custom_icon_sel_cb, contact);
		g_object_set_data_full(G_OBJECT(contact), "buddy-icon-chooser", win,
		                       (GDestroyNotify)g_object_unref);
	}
	gtk_native_dialog_show(GTK_NATIVE_DIALOG(win));
}

static void
change_size_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	int size = 0;
	PurpleConversation *conv = gtkconv->active_conv;
	GSList *buddies;

	gtk_widget_get_size_request(gtkconv->u.im->icon_container, NULL, &size);

	if (size == BUDDYICON_SIZE_MAX) {
		size = BUDDYICON_SIZE_MIN;
	} else {
		size = BUDDYICON_SIZE_MAX;
	}

	gtk_widget_set_size_request(gtkconv->u.im->icon_container, -1, size);
	pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));

	buddies = purple_blist_find_buddies(purple_conversation_get_account(conv),
			purple_conversation_get_name(conv));
	for (; buddies; buddies = g_slist_delete_link(buddies, buddies)) {
		PurpleBuddy *buddy = buddies->data;
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		purple_blist_node_set_int((PurpleBlistNode*)contact, "pidgin-infopane-iconsize", size);
	}
}

static void
remove_custom_icon_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	const gchar *name;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleContact *contact;
	PurpleConversation *conv = gtkconv->active_conv;

	account = purple_conversation_get_account(conv);
	name = purple_conversation_get_name(conv);
	buddy = purple_blist_find_buddy(account, name);
	if (!buddy) {
		return;
	}
	contact = purple_buddy_get_contact(buddy);

	purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, NULL);
}

static void
icon_menu_save_cb(GtkWidget *widget, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	const gchar *ext;
	gchar *buf;

	g_return_if_fail(conv != NULL);

	ext = purple_buddy_icon_get_extension(purple_im_conversation_get_icon(PURPLE_IM_CONVERSATION(conv)));

	buf = g_strdup_printf("%s.%s", purple_normalize(purple_conversation_get_account(conv), purple_conversation_get_name(conv)), ext);

	purple_request_file(gtkconv, _("Save Icon"), buf, TRUE,
		G_CALLBACK(saveicon_writefile_cb), NULL,
		purple_request_cpar_from_conversation(conv), gtkconv);

	g_free(buf);
}

static void
stop_anim(GtkWidget *widget, PidginConversation *gtkconv)
{
	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;
}


static void
toggle_icon_animate_cb(GtkWidget *w, PidginConversation *gtkconv)
{
	gtkconv->u.im->animate =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w));

	if (gtkconv->u.im->animate)
		start_anim(NULL, gtkconv);
	else
		stop_anim(NULL, gtkconv);
}

static gboolean
icon_menu(GtkWidget *widget, GdkEventButton *e, PidginConversation *gtkconv)
{
	GtkWidget *menu = NULL;
	GList *old_menus = NULL;
	PurpleConversation *conv;
	PurpleBuddy *buddy;

	if (e->button == GDK_BUTTON_PRIMARY && e->type == GDK_BUTTON_PRESS) {
		change_size_cb(NULL, gtkconv);
		return TRUE;
	}

	if (!gdk_event_triggers_context_menu((GdkEvent *)e)) {
		return FALSE;
	}

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	while ((old_menus = gtk_menu_get_for_attach_widget(widget)) != NULL) {
		menu = old_menus->data;
		gtk_menu_detach(GTK_MENU(menu));
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();
	gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);

	if (gtkconv->u.im->anim &&
		!(gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)))
	{
		pidgin_new_check_item(menu, _("Animate"),
							G_CALLBACK(toggle_icon_animate_cb), gtkconv,
							gtkconv->u.im->icon_timer);
	}

	conv = gtkconv->active_conv;
	buddy = purple_blist_find_buddy(purple_conversation_get_account(conv),
	                                purple_conversation_get_name(conv));

	pidgin_new_menu_item(menu, _("Hide Icon"), NULL,
                        G_CALLBACK(remove_icon), gtkconv);

	pidgin_new_menu_item(menu, _("Save Icon As..."), GTK_STOCK_SAVE_AS,
                        G_CALLBACK(icon_menu_save_cb), gtkconv);

	if (buddy) {
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		pidgin_new_menu_item(menu, _("Set Custom Icon..."), NULL,
		                     G_CALLBACK(set_custom_icon_cb), contact);
	} else {
		GtkWidget *item =
		        pidgin_new_menu_item(menu, _("Set Custom Icon..."), NULL,
		                             G_CALLBACK(set_custom_icon_cb), NULL);
		gtk_widget_set_sensitive(item, FALSE);
	}

	pidgin_new_menu_item(menu, _("Change Size"), NULL,
                        G_CALLBACK(change_size_cb), gtkconv);

	/* Is there a custom icon for this person? */
	if (buddy)
	{
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		if (contact && purple_buddy_icons_node_has_custom_icon((PurpleBlistNode*)contact))
		{
			pidgin_new_menu_item(menu, _("Remove Custom Icon"),
                                        NULL, G_CALLBACK(remove_custom_icon_cb),
                                        gtkconv);
		}
	}

	gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)e);

	return TRUE;
}

/**************************************************************************
 * End of the bunch of buddy icon functions
 **************************************************************************/
void
pidgin_conv_present_conversation(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	GdkModifierType state;

	pidgin_conv_attach_to_conversation(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);

	pidgin_conv_switch_active_conversation(conv);
	/* Switch the tab only if the user initiated the event by pressing
	 * a button or hitting a key. */
	if (gtk_get_current_event_state(&state))
		pidgin_conv_window_switch_gtkconv(gtkconv->win, gtkconv);
	gtk_window_present(GTK_WINDOW(gtkconv->win->window));
}

static GList *
pidgin_conversations_get_unseen(GList *l,
									PidginUnseenState min_state,
									gboolean hidden_only,
									guint max_count)
{
	GList *r = NULL;
	guint c = 0;

	for (; l != NULL && (max_count == 0 || c < max_count); l = l->next) {
		PurpleConversation *conv = (PurpleConversation*)l->data;
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

		if(gtkconv == NULL || gtkconv->active_conv != conv)
			continue;

		if (gtkconv->unseen_state >= min_state &&
		    (!hidden_only || gtkconv->win == hidden_convwin)) {

			r = g_list_prepend(r, conv);
			c++;
		}
	}

	return r;
}

GList *
pidgin_conversations_get_unseen_all(PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	return pidgin_conversations_get_unseen(purple_conversations_get_all(),
			min_state, hidden_only, max_count);
}

GList *
pidgin_conversations_get_unseen_ims(PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	return pidgin_conversations_get_unseen(purple_conversations_get_ims(),
			min_state, hidden_only, max_count);
}

GList *
pidgin_conversations_get_unseen_chats(PidginUnseenState min_state,
										gboolean hidden_only,
										guint max_count)
{
	return pidgin_conversations_get_unseen(purple_conversations_get_chats(),
			min_state, hidden_only, max_count);
}

static void
unseen_conv_menu_cb(GtkMenuItem *item, PurpleConversation *conv)
{
	g_return_if_fail(conv != NULL);
	pidgin_conv_present_conversation(conv);
}

static void
unseen_all_conv_menu_cb(GtkMenuItem *item, GList *list)
{
	g_return_if_fail(list != NULL);
	/* Do not free the list from here. It will be freed from the
	 * 'destroy' callback on the menuitem. */
	while (list) {
		pidgin_conv_present_conversation(list->data);
		list = list->next;
	}
}

guint
pidgin_conversations_fill_menu(GtkWidget *menu, GList *convs)
{
	GList *l;
	guint ret=0;

	g_return_val_if_fail(menu != NULL, 0);
	g_return_val_if_fail(convs != NULL, 0);

	for (l = convs; l != NULL ; l = l->next) {
		PurpleConversation *conv = (PurpleConversation*)l->data;
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

		GtkWidget *icon = gtk_image_new_from_stock(pidgin_conv_get_icon_stock(conv),
				gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC));
		GtkWidget *item;
		gchar *text = g_strdup_printf("%s (%d)",
				gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)),
				gtkconv->unseen_count);

		item = gtk_image_menu_item_new_with_label(text);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), icon);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(unseen_conv_menu_cb), conv);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_free(text);
		ret++;
	}

	if (convs->next) {
		/* There are more than one conversation. Add an option to show all conversations. */
		GtkWidget *item;
		GList *list = g_list_copy(convs);

		pidgin_separator(menu);

		item = gtk_menu_item_new_with_label(_("Show All"));
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(unseen_all_conv_menu_cb), list);
		g_signal_connect_swapped(G_OBJECT(item), "destroy", G_CALLBACK(g_list_free), list);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	return ret;
}

PidginConvWindow *
pidgin_conv_get_window(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, NULL);
	return gtkconv->win;
}

static GtkActionEntry menu_entries[] =
/* TODO: fill out tooltips... */
{
	/* Conversation menu */
	{ "ConversationMenu", NULL, N_("_Conversation"), NULL, NULL, NULL },
	{ "NewInstantMessage", PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, N_("New Instant _Message..."), "<control>M", NULL, G_CALLBACK(menu_new_conv_cb) },
	{ "JoinAChat", PIDGIN_STOCK_CHAT, N_("Join a _Chat..."), NULL, NULL, G_CALLBACK(menu_join_chat_cb) },
	{ "Find", GTK_STOCK_FIND, N_("_Find..."), NULL, NULL, G_CALLBACK(menu_find_cb) },
	{ "ViewLog", NULL, N_("View _Log"), NULL, NULL, G_CALLBACK(menu_view_log_cb) },
	{ "SaveAs", GTK_STOCK_SAVE_AS, N_("_Save As..."), NULL, NULL, G_CALLBACK(menu_save_as_cb) },
	{ "ClearScrollback", GTK_STOCK_CLEAR, N_("Clea_r Scrollback"), "<control>L", NULL, G_CALLBACK(menu_clear_cb) },

#ifdef USE_VV
	{ "MediaMenu", NULL, N_("M_edia"), NULL, NULL, NULL },
	{ "AudioCall", PIDGIN_STOCK_TOOLBAR_AUDIO_CALL, N_("_Audio Call"), NULL, NULL, G_CALLBACK(menu_initiate_media_call_cb) },
	{ "VideoCall", PIDGIN_STOCK_TOOLBAR_VIDEO_CALL, N_("_Video Call"), NULL, NULL, G_CALLBACK(menu_initiate_media_call_cb) },
	{ "AudioVideoCall", PIDGIN_STOCK_TOOLBAR_VIDEO_CALL, N_("Audio/Video _Call"), NULL, NULL, G_CALLBACK(menu_initiate_media_call_cb) },
#endif

	{ "SendFile", PIDGIN_STOCK_TOOLBAR_SEND_FILE, N_("Se_nd File..."), NULL, NULL, G_CALLBACK(menu_send_file_cb) },
	{ "GetAttention", PIDGIN_STOCK_TOOLBAR_SEND_ATTENTION, N_("Get _Attention"), NULL, NULL, G_CALLBACK(menu_get_attention_cb) },
	{ "AddBuddyPounce", NULL, N_("Add Buddy _Pounce..."), NULL, NULL, G_CALLBACK(menu_add_pounce_cb) },
	{ "GetInfo", PIDGIN_STOCK_TOOLBAR_USER_INFO, N_("_Get Info"), "<control>O", NULL, G_CALLBACK(menu_get_info_cb) },
	{ "Invite", NULL, N_("In_vite..."), NULL, NULL, G_CALLBACK(menu_invite_cb) },
	{ "MoreMenu", NULL, N_("M_ore"), NULL, NULL, NULL },
	{ "Alias", NULL, N_("Al_ias..."), NULL, NULL, G_CALLBACK(menu_alias_cb) },
	{ "Block", PIDGIN_STOCK_TOOLBAR_BLOCK, N_("_Block..."), NULL, NULL, G_CALLBACK(menu_block_cb) },
	{ "Unblock", PIDGIN_STOCK_TOOLBAR_UNBLOCK, N_("_Unblock..."), NULL, NULL, G_CALLBACK(menu_unblock_cb) },
	{ "Add", GTK_STOCK_ADD, N_("_Add..."), NULL, NULL, G_CALLBACK(menu_add_remove_cb) },
	{ "Remove", GTK_STOCK_REMOVE, N_("_Remove..."), NULL, NULL, G_CALLBACK(menu_add_remove_cb) },
	{ "InsertLink", PIDGIN_STOCK_TOOLBAR_INSERT_LINK, N_("Insert Lin_k..."), NULL, NULL, NULL },
	{ "InsertImage", PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE, N_("Insert Imag_e..."), NULL, NULL, NULL },
	{ "Close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W", NULL, G_CALLBACK(menu_close_conv_cb) },

	/* Options */
	{ "OptionsMenu", NULL, N_("_Options"), NULL, NULL, NULL },
};

/* Toggle items */
static const GtkToggleActionEntry menu_toggle_entries[] = {
	{ "EnableLogging", NULL, N_("Enable _Logging"), NULL, NULL, G_CALLBACK(menu_logging_cb), FALSE },
	{ "EnableSounds", NULL, N_("Enable _Sounds"), NULL, NULL, G_CALLBACK(menu_sounds_cb), FALSE },
	{ "ShowFormattingToolbars", NULL, N_("Show Formatting _Toolbars"), NULL, NULL, G_CALLBACK(menu_toolbar_cb), FALSE },
};

static const char *conversation_menu =
"<ui>"
	"<menubar name='Conversation'>"
		"<menu action='ConversationMenu'>"
			"<menuitem action='NewInstantMessage'/>"
			"<menuitem action='JoinAChat'/>"
			"<separator/>"
			"<menuitem action='Find'/>"
			"<menuitem action='ViewLog'/>"
			"<menuitem action='SaveAs'/>"
			"<menuitem action='ClearScrollback'/>"
			"<separator/>"
#ifdef USE_VV
			"<menu action='MediaMenu'>"
				"<menuitem action='AudioCall'/>"
				"<menuitem action='VideoCall'/>"
				"<menuitem action='AudioVideoCall'/>"
			"</menu>"
#endif
			"<menuitem action='SendFile'/>"
			"<menuitem action='GetAttention'/>"
			"<menuitem action='AddBuddyPounce'/>"
			"<menuitem action='GetInfo'/>"
			"<menuitem action='Invite'/>"
			"<menu action='MoreMenu'/>"
			"<separator/>"
			"<menuitem action='Alias'/>"
			"<menuitem action='Block'/>"
			"<menuitem action='Unblock'/>"
			"<menuitem action='Add'/>"
			"<menuitem action='Remove'/>"
			"<separator/>"
			"<menuitem action='InsertLink'/>"
			"<menuitem action='InsertImage'/>"
			"<separator/>"
			"<menuitem action='Close'/>"
		"</menu>"
		"<menu action='OptionsMenu'>"
			"<menuitem action='EnableLogging'/>"
			"<menuitem action='EnableSounds'/>"
			"<separator/>"
			"<menuitem action='ShowFormattingToolbars'/>"
		"</menu>"
	"</menubar>"
"</ui>";

static void
sound_method_pref_changed_cb(const char *name, PurplePrefType type,
							 gconstpointer value, gpointer data)
{
	PidginConvWindow *win = data;
	const char *method = value;

	if (purple_strequal(method, "none"))
	{
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu->sounds),
		                             FALSE);
		gtk_action_set_sensitive(win->menu->sounds, FALSE);
	}
	else
	{
		PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

		if (gtkconv != NULL)
			gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu->sounds),
			                             gtkconv->make_sound);
		gtk_action_set_sensitive(win->menu->sounds, TRUE);
	}
}

/* Returns TRUE if some items were added to the menu, FALSE otherwise */
static gboolean
populate_menu_with_options(GtkWidget *menu, PidginConversation *gtkconv, gboolean all)
{
	GList *list;
	PurpleConversation *conv;
	PurpleAccount *account;
	PurpleBlistNode *node = NULL;
	PurpleChat *chat = NULL;
	PurpleBuddy *buddy = NULL;
	gboolean ret;

	conv = gtkconv->active_conv;
	account = purple_conversation_get_account(conv);

	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		chat = purple_blist_find_chat(account, purple_conversation_get_name(conv));

		if ((chat == NULL) && (gtkconv->history != NULL)) {
			chat = g_object_get_data(G_OBJECT(gtkconv->history), "transient_chat");
		}

		if ((chat == NULL) && (gtkconv->history != NULL)) {
			GHashTable *components;
			PurpleAccount *account = purple_conversation_get_account(conv);
			PurpleProtocol *protocol =
					purple_protocols_find(purple_account_get_protocol_id(account));
			if (purple_account_get_connection(account) != NULL &&
					PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, info_defaults)) {
				components = purple_protocol_chat_iface_info_defaults(protocol, purple_account_get_connection(account),
						purple_conversation_get_name(conv));
			} else {
				components = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, g_free);
				g_hash_table_replace(components, g_strdup("channel"),
						g_strdup(purple_conversation_get_name(conv)));
			}
			chat = purple_chat_new(account, NULL, components);
			purple_blist_node_set_transient((PurpleBlistNode *)chat, TRUE);
			g_object_set_data_full(G_OBJECT(gtkconv->history), "transient_chat",
					chat, (GDestroyNotify)purple_blist_remove_chat);
		}
	} else {
		if (!purple_account_is_connected(account))
			return FALSE;

		buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
		if (!buddy && gtkconv->history) {
			buddy = g_object_get_data(G_OBJECT(gtkconv->history), "transient_buddy");

			if (!buddy) {
				buddy = purple_buddy_new(account, purple_conversation_get_name(conv), NULL);
				purple_blist_node_set_transient((PurpleBlistNode *)buddy, TRUE);
				g_object_set_data_full(G_OBJECT(gtkconv->history), "transient_buddy",
						buddy, (GDestroyNotify)g_object_unref);
			}
		}
	}

	if (chat)
		node = (PurpleBlistNode *)chat;
	else if (buddy)
		node = (PurpleBlistNode *)buddy;

	/* Now add the stuff */
	if (all) {
		if (buddy)
			pidgin_blist_make_buddy_menu(menu, buddy, TRUE);
		else if (chat) {
			/* XXX: */
		}
	} else if (node) {
		if (purple_account_is_connected(account))
			pidgin_append_blist_node_proto_menu(menu, purple_account_get_connection(account), node);
		pidgin_append_blist_node_extended_menu(menu, node);
	}

	if ((list = gtk_container_get_children(GTK_CONTAINER(menu))) == NULL) {
		ret = FALSE;
	} else {
		g_list_free(list);
		ret = TRUE;
	}
	return ret;
}

static void
regenerate_media_items(PidginConvWindow *win)
{
#ifdef USE_VV
	PurpleAccount *account;
	PurpleConversation *conv;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL) {
		purple_debug_error("gtkconv", "couldn't get active conversation"
				" when regenerating media items\n");
		return;
	}

	account = purple_conversation_get_account(conv);

	if (account == NULL) {
		purple_debug_error("gtkconv", "couldn't get account when"
				" regenerating media items\n");
		return;
	}

	/*
	 * Check if account support voice and/or calls, and
	 * if the current buddy	supports it.
	 */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleMediaCaps caps =
				purple_protocol_get_media_caps(account,
				purple_conversation_get_name(conv));

		gtk_action_set_sensitive(win->menu->audio_call,
				caps & PURPLE_MEDIA_CAPS_AUDIO
				? TRUE : FALSE);
		gtk_action_set_sensitive(win->menu->video_call,
				caps & PURPLE_MEDIA_CAPS_VIDEO
				? TRUE : FALSE);
		gtk_action_set_sensitive(win->menu->audio_video_call,
				caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO
				? TRUE : FALSE);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		/* for now, don't care about chats... */
		gtk_action_set_sensitive(win->menu->audio_call, FALSE);
		gtk_action_set_sensitive(win->menu->video_call, FALSE);
		gtk_action_set_sensitive(win->menu->audio_video_call, FALSE);
	} else {
		gtk_action_set_sensitive(win->menu->audio_call, FALSE);
		gtk_action_set_sensitive(win->menu->video_call, FALSE);
		gtk_action_set_sensitive(win->menu->audio_video_call, FALSE);
	}
#endif
}

static void
regenerate_attention_items(PidginConvWindow *win)
{
	GtkWidget *attention;
	GtkWidget *menu;
	PurpleConversation *conv;
	PurpleConnection *pc;
	PurpleProtocol *protocol = NULL;
	GList *list;

	conv = pidgin_conv_window_get_active_conversation(win);
	if (!conv)
		return;

	attention = gtk_ui_manager_get_widget(win->menu->ui,
	                                      "/Conversation/ConversationMenu/GetAttention");

	/* Remove the previous entries */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(attention), NULL);

	pc = purple_conversation_get_connection(conv);
	if (pc != NULL)
		protocol = purple_connection_get_protocol(pc);

	if (protocol && PURPLE_IS_PROTOCOL_ATTENTION(protocol)) {
		list = purple_protocol_attention_get_types(PURPLE_PROTOCOL_ATTENTION(protocol), purple_connection_get_account(pc));

		/* Multiple attention types */
		if (list && list->next) {
			int index = 0;

			menu = gtk_menu_new();
			while (list) {
				PurpleAttentionType *type;
				GtkWidget *menuitem;

				type = list->data;

				menuitem = gtk_menu_item_new_with_label(purple_attention_type_get_name(type));
				g_object_set_data(G_OBJECT(menuitem), "index", GINT_TO_POINTER(index));
				g_signal_connect(G_OBJECT(menuitem), "activate",
				                 G_CALLBACK(menu_get_attention_cb),
				                 win);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

				index++;
				list = g_list_delete_link(list, list);
			}

			gtk_menu_item_set_submenu(GTK_MENU_ITEM(attention), menu);
			gtk_widget_show_all(menu);
		}
	}
}

static void
regenerate_options_items(PidginConvWindow *win)
{
	GtkWidget *menu;
	PidginConversation *gtkconv;
	GList *list;
	GtkWidget *more_menu;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	more_menu = gtk_ui_manager_get_widget(win->menu->ui,
	                                      "/Conversation/ConversationMenu/MoreMenu");
	gtk_widget_show(more_menu);
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(more_menu));

	/* Remove the previous entries */
	list = gtk_container_get_children(GTK_CONTAINER(menu));
	g_list_free_full(list, (GDestroyNotify)gtk_widget_destroy);

	if (!populate_menu_with_options(menu, gtkconv, FALSE))
	{
		GtkWidget *item = gtk_menu_item_new_with_label(_("No actions available"));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_set_sensitive(item, FALSE);
	}

	gtk_widget_show_all(menu);
}

static void
remove_from_list(GtkWidget *widget, PidginConvWindow *win)
{
	GList *list = g_object_get_data(G_OBJECT(win->window), "plugin-actions");
	list = g_list_remove(list, widget);
	g_object_set_data(G_OBJECT(win->window), "plugin-actions", list);
}

static void
regenerate_plugins_items(PidginConvWindow *win)
{
	GList *action_items;
	GtkWidget *menu;
	GList *list;
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	GtkWidget *item;

	if (win->window == NULL || win == hidden_convwin)
		return;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	if (gtkconv == NULL)
		return;

	conv = gtkconv->active_conv;
	action_items = g_object_get_data(G_OBJECT(win->window), "plugin-actions");

	/* Remove the old menuitems */
	while (action_items) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(action_items->data),
					G_CALLBACK(remove_from_list), win);
		gtk_widget_destroy(action_items->data);
		action_items = g_list_delete_link(action_items, action_items);
	}

	item = gtk_ui_manager_get_widget(win->menu->ui, "/Conversation/OptionsMenu");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));

	list = purple_conversation_get_extended_menu(conv);
	if (list) {
		action_items = g_list_prepend(NULL, (item = pidgin_separator(menu)));
		g_signal_connect(G_OBJECT(item), "destroy", G_CALLBACK(remove_from_list), win);
	}

	for(; list; list = g_list_delete_link(list, list)) {
		PurpleActionMenu *act = (PurpleActionMenu *) list->data;
		item = pidgin_append_menu_action(menu, act, conv);
		action_items = g_list_prepend(action_items, item);
		gtk_widget_show_all(item);
		g_signal_connect(G_OBJECT(item), "destroy", G_CALLBACK(remove_from_list), win);
	}
	g_object_set_data(G_OBJECT(win->window), "plugin-actions", action_items);
}

static void menubar_activated(GtkWidget *item, gpointer data)
{
	PidginConvWindow *win = data;
	regenerate_media_items(win);
	regenerate_options_items(win);
	regenerate_plugins_items(win);
	regenerate_attention_items(win);

	/* The following are to make sure the 'More' submenu is not regenerated every time
	 * the focus shifts from 'Conversations' to some other menu and back. */
	g_signal_handlers_block_by_func(G_OBJECT(item), G_CALLBACK(menubar_activated), data);
	g_signal_connect(G_OBJECT(win->menu->menubar), "deactivate", G_CALLBACK(focus_out_from_menubar), data);
}

static void
focus_out_from_menubar(GtkWidget *wid, PidginConvWindow *win)
{
	/* The menubar has been deactivated. Make sure the 'More' submenu is regenerated next time
	 * the 'Conversation' menu pops up. */
	GtkWidget *menuitem = gtk_ui_manager_get_widget(win->menu->ui, "/Conversation/ConversationMenu");
	g_signal_handlers_unblock_by_func(G_OBJECT(menuitem), G_CALLBACK(menubar_activated), win);
	g_signal_handlers_disconnect_by_func(G_OBJECT(win->menu->menubar),
				G_CALLBACK(focus_out_from_menubar), win);
}

static GtkWidget *
setup_menubar(PidginConvWindow *win)
{
	GtkAccelGroup *accel_group;
	const char *method;
	GtkActionGroup *action_group;
	GError *error;
	GtkWidget *menuitem;

	action_group = gtk_action_group_new("ConversationActions");
	gtk_action_group_set_translation_domain(action_group, PACKAGE);
	gtk_action_group_add_actions(action_group,
	                             menu_entries,
	                             G_N_ELEMENTS(menu_entries),
	                             win);
	gtk_action_group_add_toggle_actions(action_group,
	                                    menu_toggle_entries,
	                                    G_N_ELEMENTS(menu_toggle_entries),
	                                    win);

	win->menu->ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(win->menu->ui, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group(win->menu->ui);
	gtk_window_add_accel_group(GTK_WINDOW(win->window), accel_group);
	g_signal_connect(G_OBJECT(accel_group), "accel-changed",
	                 G_CALLBACK(pidgin_save_accels_cb), NULL);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string(win->menu->ui, conversation_menu, -1, &error))
	{
		g_message("building menus failed: %s", error->message);
		g_error_free(error);
		exit(EXIT_FAILURE);
	}

	win->menu->menubar =
		gtk_ui_manager_get_widget(win->menu->ui, "/Conversation");

	/* Make sure the 'Conversation ⇨ More' menuitems are regenerated whenever
	 * the 'Conversation' menu pops up because the entries can change after the
	 * conversation is created. */
	menuitem = gtk_ui_manager_get_widget(win->menu->ui, "/Conversation/ConversationMenu");
	g_signal_connect(G_OBJECT(menuitem), "activate", G_CALLBACK(menubar_activated), win);

	win->menu->view_log =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/ViewLog");

#ifdef USE_VV
	win->menu->audio_call =
		gtk_ui_manager_get_action(win->menu->ui,
					    "/Conversation/ConversationMenu/MediaMenu/AudioCall");
	win->menu->video_call =
		gtk_ui_manager_get_action(win->menu->ui,
					    "/Conversation/ConversationMenu/MediaMenu/VideoCall");
	win->menu->audio_video_call =
		gtk_ui_manager_get_action(win->menu->ui,
					    "/Conversation/ConversationMenu/MediaMenu/AudioVideoCall");
#endif

	/* --- */

	win->menu->send_file =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/SendFile");

	win->menu->get_attention =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/GetAttention");

	win->menu->add_pounce =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/AddBuddyPounce");

	/* --- */

	win->menu->get_info =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/GetInfo");

	win->menu->invite =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/Invite");

	/* --- */

	win->menu->alias =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/Alias");

	win->menu->block =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/Block");

	win->menu->unblock =
		gtk_ui_manager_get_action(win->menu->ui,
					    "/Conversation/ConversationMenu/Unblock");

	win->menu->add =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/Add");

	win->menu->remove =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/ConversationMenu/Remove");

	/* --- */

	win->menu->insert_link =
		gtk_ui_manager_get_action(win->menu->ui,
				"/Conversation/ConversationMenu/InsertLink");

	win->menu->insert_image =
		gtk_ui_manager_get_action(win->menu->ui,
				"/Conversation/ConversationMenu/InsertImage");

	/* --- */

	win->menu->logging =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/OptionsMenu/EnableLogging");
	win->menu->sounds =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/OptionsMenu/EnableSounds");
	method = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method");
	if (purple_strequal(method, "none"))
	{
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu->sounds),
		                               FALSE);
		gtk_action_set_sensitive(win->menu->sounds, FALSE);
	}
	purple_prefs_connect_callback(win, PIDGIN_PREFS_ROOT "/sound/method",
				    sound_method_pref_changed_cb, win);

	win->menu->show_formatting_toolbar =
		gtk_ui_manager_get_action(win->menu->ui,
		                          "/Conversation/OptionsMenu/ShowFormattingToolbars");

	win->menu->tray = pidgin_menu_tray_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(win->menu->menubar),
	                      win->menu->tray);
	gtk_widget_show(win->menu->tray);

	gtk_widget_show(win->menu->menubar);

	return win->menu->menubar;
}


/**************************************************************************
 * Utility functions
 **************************************************************************/

static void
got_typing_keypress(PidginConversation *gtkconv, gboolean first)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleIMConversation *im;

	/*
	 * We know we got something, so we at least have to make sure we don't
	 * send PURPLE_IM_TYPED any time soon.
	 */

	im = PURPLE_IM_CONVERSATION(conv);

	purple_im_conversation_stop_send_typed_timeout(im);
	purple_im_conversation_start_send_typed_timeout(im);

	/* Check if we need to send another PURPLE_IM_TYPING message */
	if (first || (purple_im_conversation_get_type_again(im) != 0 &&
				  time(NULL) > purple_im_conversation_get_type_again(im)))
	{
		unsigned int timeout;
		timeout = purple_serv_send_typing(purple_conversation_get_connection(conv),
								   purple_conversation_get_name(conv),
								   PURPLE_IM_TYPING);
		purple_im_conversation_set_type_again(im, timeout);
	}
}

#if 0
static gboolean
typing_animation(gpointer data) {
	PidginConversation *gtkconv = data;
	PidginConvWindow *gtkwin = gtkconv->win;
	const char *stock_id = NULL;

	if(gtkconv != pidgin_conv_window_get_active_gtkconv(gtkwin)) {
		return FALSE;
	}

	switch (rand() % 5) {
	case 0:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING0;
		break;
	case 1:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING1;
		break;
	case 2:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING2;
		break;
	case 3:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING3;
		break;
	case 4:
		stock_id = PIDGIN_STOCK_ANIMATION_TYPING4;
		break;
	}
	if (gtkwin->menu->typing_icon == NULL) {
		gtkwin->menu->typing_icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);
		pidgin_menu_tray_append(PIDGIN_MENU_TRAY(gtkwin->menu->tray),
		                        gtkwin->menu->typing_icon,
		                        _("User is typing..."));
	} else {
		gtk_image_set_from_stock(GTK_IMAGE(gtkwin->menu->typing_icon), stock_id, GTK_ICON_SIZE_MENU);
	}
	gtk_widget_show(gtkwin->menu->typing_icon);
	return TRUE;
}
#endif

static void
update_typing_message(PidginConversation *gtkconv, const char *message)
{
	/* TODO WEBKIT: this is not handled at all */
#if 0
	GtkTextBuffer *buffer;
	GtkTextMark *stmark, *enmark;

	if (g_object_get_data(G_OBJECT(gtkconv->imhtml), "disable-typing-notification"))
		return;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
	stmark = gtk_text_buffer_get_mark(buffer, "typing-notification-start");
	enmark = gtk_text_buffer_get_mark(buffer, "typing-notification-end");
	if (stmark && enmark) {
		GtkTextIter start, end;
		gtk_text_buffer_get_iter_at_mark(buffer, &start, stmark);
		gtk_text_buffer_get_iter_at_mark(buffer, &end, enmark);
		gtk_text_buffer_delete_mark(buffer, stmark);
		gtk_text_buffer_delete_mark(buffer, enmark);
		gtk_text_buffer_delete(buffer, &start, &end);
	} else if (message && *message == '\n' && message[1] == ' ' && message[2] == '\0')
		message = NULL;

#ifdef RESERVE_LINE
	if (!message)
		message = "\n ";   /* The blank space is required to avoid a GTK+/Pango bug */
#endif

	if (message) {
		GtkTextIter iter;
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "typing-notification-start", &iter, TRUE);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, message, -1, "TYPING-NOTIFICATION", NULL);
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_create_mark(buffer, "typing-notification-end", &iter, TRUE);
	}
#endif /* if 0 */
}

static void
update_typing_icon(PidginConversation *gtkconv)
{
	PurpleIMConversation *im;
	char *message = NULL;

	if (!PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv))
		return;

	im = PURPLE_IM_CONVERSATION(gtkconv->active_conv);

	if (purple_im_conversation_get_typing_state(im) == PURPLE_IM_NOT_TYPING) {
#ifdef RESERVE_LINE
		update_typing_message(gtkconv, NULL);
#else
		update_typing_message(gtkconv, "\n ");
#endif
		return;
	}

	if (purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPING) {
		message = g_strdup_printf(_("\n%s is typing..."), purple_conversation_get_title(PURPLE_CONVERSATION(im)));
	} else {
		message = g_strdup_printf(_("\n%s has stopped typing"), purple_conversation_get_title(PURPLE_CONVERSATION(im)));
	}

	update_typing_message(gtkconv, message);
	g_free(message);
}

static gboolean
update_send_to_selection(PidginConvWindow *win)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	GtkWidget *menu;
	GList *child;
	PurpleBuddy *b;

	conv = pidgin_conv_window_get_active_conversation(win);

	if (conv == NULL)
		return FALSE;

	account = purple_conversation_get_account(conv);

	if (account == NULL)
		return FALSE;

	if (win->menu->send_to == NULL)
		return FALSE;

	if (!(b = purple_blist_find_buddy(account, purple_conversation_get_name(conv))))
		return FALSE;

	gtk_widget_show(win->menu->send_to);

	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(win->menu->send_to));

	for (child = gtk_container_get_children(GTK_CONTAINER(menu));
		 child != NULL;
		 child = g_list_delete_link(child, child)) {

		GtkWidget *item = child->data;
		PurpleBuddy *item_buddy;
		PurpleAccount *item_account = g_object_get_data(G_OBJECT(item), "purple_account");
		gchar *buddy_name = g_object_get_data(G_OBJECT(item),
		                                      "purple_buddy_name");
		item_buddy = purple_blist_find_buddy(item_account, buddy_name);

		if (b == item_buddy) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
			g_list_free(child);
			break;
		}
	}

	return FALSE;
}

static gboolean
send_to_item_enter_notify_cb(GtkWidget *menuitem, GdkEventCrossing *event, GtkWidget *label)
{
	gtk_widget_set_sensitive(GTK_WIDGET(label), TRUE);
	return FALSE;
}

static gboolean
send_to_item_leave_notify_cb(GtkWidget *menuitem, GdkEventCrossing *event, GtkWidget *label)
{
	gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	return FALSE;
}

static GtkWidget *
e2ee_state_to_gtkimage(PurpleE2eeState *state)
{
	PurpleImage *img;

	img = _pidgin_e2ee_stock_icon_get(
		purple_e2ee_state_get_stock_icon(state));
	if (!img)
		return NULL;

	return gtk_image_new_from_pixbuf(pidgin_pixbuf_from_image(img));
}

static void
create_sendto_item(GtkWidget *menu, GtkSizeGroup *sg, GSList **group,
	PurpleBuddy *buddy, PurpleAccount *account, const char *name,
	gboolean e2ee_enabled)
{
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *image;
	GtkWidget *e2ee_image = NULL;
	GtkWidget *menuitem;
	GdkPixbuf *pixbuf;
	gchar *text;

	/* Create a pixmap for the protocol icon. */
	pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);

	/* Now convert it to GtkImage */
	if (pixbuf == NULL)
		image = gtk_image_new();
	else
	{
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(G_OBJECT(pixbuf));
	}

	if (e2ee_enabled) {
		PurpleIMConversation *im;
		PurpleE2eeState *state = NULL;

		im = purple_conversations_find_im_with_account(
			purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
		if (im)
			state = purple_conversation_get_e2ee_state(PURPLE_CONVERSATION(im));
		if (state)
			e2ee_image = e2ee_state_to_gtkimage(state);
		else
			e2ee_image = gtk_image_new();
	}

	gtk_size_group_add_widget(sg, image);

	/* Make our menu item */
	text = g_strdup_printf("%s (%s)", name, purple_account_get_name_for_display(account));
	menuitem = gtk_radio_menu_item_new_with_label(*group, text);
	g_free(text);
	*group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));

	/* Do some evil, see some evil, speak some evil. */
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	label = gtk_bin_get_child(GTK_BIN(menuitem));
	g_object_ref(label);
	gtk_container_remove(GTK_CONTAINER(menuitem), label);

	gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 4);
	if (e2ee_image)
		gtk_box_pack_start(GTK_BOX(box), e2ee_image, FALSE, FALSE, 0);

	if (buddy != NULL &&
	    !purple_presence_is_online(purple_buddy_get_presence(buddy)))
	{
		gtk_widget_set_sensitive(label, FALSE);

		/* Set the label sensitive when the menuitem is highlighted and
		 * insensitive again when the mouse leaves it. This way, it
		 * doesn't appear weird from the highlighting of the embossed
		 * (insensitive style) text.*/
		g_signal_connect(menuitem, "enter-notify-event",
				 G_CALLBACK(send_to_item_enter_notify_cb), label);
		g_signal_connect(menuitem, "leave-notify-event",
				 G_CALLBACK(send_to_item_leave_notify_cb), label);
	}

	g_object_unref(label);

	gtk_container_add(GTK_CONTAINER(menuitem), box);

	gtk_widget_show(label);
	gtk_widget_show(image);
	if (e2ee_image)
		gtk_widget_show(e2ee_image);
	gtk_widget_show(box);

	/* Set our data and callbacks. */
	g_object_set_data(G_OBJECT(menuitem), "purple_account", account);
	g_object_set_data_full(G_OBJECT(menuitem), "purple_buddy_name", g_strdup(name), g_free);

	g_signal_connect(G_OBJECT(menuitem), "activate",
	                 G_CALLBACK(menu_conv_sel_send_cb), NULL);

	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
}

static gboolean
compare_buddy_presence(PurplePresence *p1, PurplePresence *p2)
{
	/* This is necessary because multiple PurpleBuddy's don't share the same
	 * PurplePresence anymore.
	 */
	PurpleBuddy *b1 = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(p1));
	PurpleBuddy *b2 = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(p2));
	if (purple_buddy_get_account(b1) == purple_buddy_get_account(b2) &&
			purple_strequal(purple_buddy_get_name(b1), purple_buddy_get_name(b2)))
		return FALSE;
	return TRUE;
}

static void
generate_send_to_items(PidginConvWindow *win)
{
	GtkWidget *menu;
	GSList *group = NULL;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	PidginConversation *gtkconv;
	GSList *l, *buds;

	g_return_if_fail(win != NULL);

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	g_return_if_fail(gtkconv != NULL);

	if (win->menu->send_to != NULL)
		gtk_widget_destroy(win->menu->send_to);

	/* Build the Send To menu */
	win->menu->send_to = gtk_menu_item_new_with_mnemonic(_("S_end To"));
	gtk_widget_show(win->menu->send_to);

	menu = gtk_menu_new();
	gtk_menu_shell_insert(GTK_MENU_SHELL(win->menu->menubar),
	                      win->menu->send_to, 2);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(win->menu->send_to), menu);

	gtk_widget_show(menu);

	if (PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv)) {
		buds = purple_blist_find_buddies(purple_conversation_get_account(gtkconv->active_conv), purple_conversation_get_name(gtkconv->active_conv));

		if (buds == NULL)
		{
			/* The user isn't on the buddy list. So we don't create any sendto menu. */
		}
		else
		{
			gboolean e2ee_enabled = FALSE;
			GList *list = NULL, *iter;
			for (l = buds; l != NULL; l = l->next)
			{
				PurpleBlistNode *node;

				node = PURPLE_BLIST_NODE(purple_buddy_get_contact(PURPLE_BUDDY(l->data)));

				for (node = node->child; node != NULL; node = node->next)
				{
					PurpleBuddy *buddy = (PurpleBuddy *)node;
					PurpleAccount *account;
					PurpleIMConversation *im;

					if (!PURPLE_IS_BUDDY(node))
						continue;

					im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
					if (im && purple_conversation_get_e2ee_state(PURPLE_CONVERSATION(im)) != NULL)
						e2ee_enabled = TRUE;

					account = purple_buddy_get_account(buddy);
					/* TODO WEBKIT: (I'm not actually sure if this is webkit-related --Mark Doliner) */
					if (purple_account_is_connected(account) /*|| account == purple_conversation_get_account(gtkconv->active_conv)*/)
					{
						/* Use the PurplePresence to get unique buddies. */
						PurplePresence *presence = purple_buddy_get_presence(buddy);
						if (!g_list_find_custom(list, presence, (GCompareFunc)compare_buddy_presence))
							list = g_list_prepend(list, presence);
					}
				}
			}

			/* Create the sendto menu only if it has more than one item to show */
			if (list && list->next) {
				/* Loop over the list backwards so we get the items in the right order,
				 * since we did a g_list_prepend() earlier. */
				for (iter = g_list_last(list); iter != NULL; iter = iter->prev) {
					PurplePresence *pre = iter->data;
					PurpleBuddy *buddy = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(pre));
					create_sendto_item(menu, sg, &group, buddy,
							purple_buddy_get_account(buddy), purple_buddy_get_name(buddy), e2ee_enabled);
				}
			}
			g_list_free(list);
			g_slist_free(buds);
		}
	}

	g_object_unref(sg);

	gtk_widget_show(win->menu->send_to);
	/* TODO: This should never be insensitive.  Possibly hidden or not. */
	if (!group)
		gtk_widget_set_sensitive(win->menu->send_to, FALSE);
	update_send_to_selection(win);
}

PurpleImage *
_pidgin_e2ee_stock_icon_get(const gchar *stock_name)
{
	gchar filename[100], *path;
	PurpleImage *image;

	/* core is quitting */
	if (e2ee_stock == NULL)
		return NULL;

	if (g_hash_table_lookup_extended(e2ee_stock, stock_name, NULL, (gpointer*)&image))
		return image;

	g_snprintf(filename, sizeof(filename), "e2ee-%s.png", stock_name);
	path = g_build_filename(PURPLE_DATADIR, "pidgin", "icons",
		"hicolor", "16x16", "status", filename, NULL);
	image = purple_image_new_from_file(path, NULL);
	g_free(path);

	g_hash_table_insert(e2ee_stock, g_strdup(stock_name), image);
	return image;
}

static void
generate_e2ee_controls(PidginConvWindow *win)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PurpleE2eeState *state;
	PurpleE2eeProvider *provider;
	GtkWidget *menu;
	GList *menu_actions, *it;
	GtkWidget *e2ee_image;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	g_return_if_fail(gtkconv != NULL);

	conv = gtkconv->active_conv;
	g_return_if_fail(conv != NULL);

	if (win->menu->e2ee != NULL) {
		gtk_widget_destroy(win->menu->e2ee);
		win->menu->e2ee = NULL;
	}

	provider = purple_e2ee_provider_get_main();
	state = purple_conversation_get_e2ee_state(conv);
	if (state == NULL || provider == NULL)
		return;
	if (purple_e2ee_state_get_provider(state) != provider)
		return;

	win->menu->e2ee = gtk_image_menu_item_new_with_label(
		purple_e2ee_provider_get_name(provider));

	menu = gtk_menu_new();
	gtk_menu_shell_insert(GTK_MENU_SHELL(win->menu->menubar),
		win->menu->e2ee, 3);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(win->menu->e2ee), menu);

	e2ee_image = e2ee_state_to_gtkimage(state);
	if (e2ee_image) {
		gtk_image_menu_item_set_image(
			GTK_IMAGE_MENU_ITEM(win->menu->e2ee), e2ee_image);
	}

	gtk_widget_set_tooltip_text(win->menu->e2ee,
		purple_e2ee_state_get_name(state));

	menu_actions = purple_e2ee_provider_get_conv_menu_actions(provider, conv);
	for (it = menu_actions; it; it = g_list_next(it)) {
		PurpleActionMenu *action = it->data;

		gtk_widget_show_all(
			pidgin_append_menu_action(menu, action, conv));
	}
	g_list_free(menu_actions);

	gtk_widget_show(win->menu->e2ee);
	gtk_widget_show(menu);
}

static const char *
get_chat_user_status_icon(PurpleChatConversation *chat, const char *name, PurpleChatUserFlags flags)
{
	const char *image = NULL;

	if (flags & PURPLE_CHAT_USER_FOUNDER) {
		image = PIDGIN_STOCK_STATUS_FOUNDER;
	} else if (flags & PURPLE_CHAT_USER_OP) {
		image = PIDGIN_STOCK_STATUS_OPERATOR;
	} else if (flags & PURPLE_CHAT_USER_HALFOP) {
		image = PIDGIN_STOCK_STATUS_HALFOP;
	} else if (flags & PURPLE_CHAT_USER_VOICE) {
		image = PIDGIN_STOCK_STATUS_VOICE;
	} else if ((!flags) && purple_chat_conversation_is_ignored_user(chat, name)) {
		image = PIDGIN_STOCK_STATUS_IGNORED;
	} else {
		return NULL;
	}
	return image;
}

static void
deleting_chat_user_cb(PurpleChatUser *cb)
{
	GtkTreeRowReference *ref = purple_chat_user_get_ui_data(cb);

	if (ref) {
		gtk_tree_row_reference_free(ref);
		purple_chat_user_set_ui_data(cb, NULL);
	}
}

static void
add_chat_user_common(PurpleChatConversation *chat, PurpleChatUser *cb, const char *old_name)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PidginChatPane *gtkchat;
	PurpleConnection *gc;
	PurpleProtocol *protocol;
	GtkTreeModel *tm;
	GtkListStore *ls;
	GtkTreePath *newpath;
	const char *stock;
	GtkTreeIter iter;
	gboolean is_me = FALSE;
	gboolean is_buddy;
	const gchar *name, *alias;
	gchar *tmp, *alias_key;
	PurpleChatUserFlags flags;
	GdkRGBA *color = NULL;

	alias = purple_chat_user_get_alias(cb);
	name  = purple_chat_user_get_name(cb);
	flags = purple_chat_user_get_flags(cb);

	conv    = PURPLE_CONVERSATION(chat);
	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	gc      = purple_conversation_get_connection(conv);

	if (!gc || !(protocol = purple_connection_get_protocol(gc)))
		return;

	tm = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
	ls = GTK_LIST_STORE(tm);

	stock = get_chat_user_status_icon(chat, name, flags);

	if (purple_strequal(purple_chat_conversation_get_nick(chat), purple_normalize(purple_conversation_get_account(conv), old_name != NULL ? old_name : name)))
		is_me = TRUE;

	is_buddy = purple_chat_user_is_buddy(cb);

	tmp = g_utf8_casefold(alias, -1);
	alias_key = g_utf8_collate_key(tmp, -1);
	g_free(tmp);

	if (is_me) {
#if 0
		/* TODO WEBKIT: No tags in webkit stuff, yet. */
		GtkTextTag *tag = gtk_text_tag_table_lookup(
				gtk_text_buffer_get_tag_table(GTK_IMHTML(gtkconv->webview)->text_buffer),
				"send-name");
		g_object_get(tag, "foreground-rgba", &color, NULL);
#endif /* if 0 */
	} else {
		GtkTextTag *tag;
		if ((tag = get_buddy_tag(chat, name, 0, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_NORMAL, NULL);
		if ((tag = get_buddy_tag(chat, name, PURPLE_MESSAGE_NICK, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_NORMAL, NULL);
		color = (GdkRGBA*)get_nick_color(gtkconv, name);
	}

	gtk_list_store_insert_with_values(ls, &iter,
/*
* The GTK docs are mute about the effects of the "row" value for performance.
* X-Chat hardcodes their value to 0 (prepend) and -1 (append), so we will too.
* It *might* be faster to search the gtk_list_store and set row accurately,
* but no one in #gtk+ seems to know anything about it either.
* Inserting in the "wrong" location has no visible ill effects. - F.P.
*/
			-1, /* "row" */
			CHAT_USERS_ICON_STOCK_COLUMN,  stock,
			CHAT_USERS_ALIAS_COLUMN, alias,
			CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
			CHAT_USERS_NAME_COLUMN,  name,
			CHAT_USERS_FLAGS_COLUMN, flags,
			CHAT_USERS_COLOR_COLUMN, color,
			CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
			-1);

	if (purple_chat_user_get_ui_data(cb)) {
		GtkTreeRowReference *ref = purple_chat_user_get_ui_data(cb);
		gtk_tree_row_reference_free(ref);
	}

	newpath = gtk_tree_model_get_path(tm, &iter);
	purple_chat_user_set_ui_data(cb, gtk_tree_row_reference_new(tm, newpath));
	gtk_tree_path_free(newpath);

#if 0
	if (is_me && color)
		gdk_rgba_free(color);
#endif
	g_free(alias_key);
}

static void topic_callback(GtkWidget *w, PidginConversation *gtkconv)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;
	PurpleConversation *conv = gtkconv->active_conv;
	PidginChatPane *gtkchat;
	char *new_topic;
	const char *current_topic;

	gc      = purple_conversation_get_connection(conv);

	if(!gc || !(protocol = purple_connection_get_protocol(gc)))
		return;

	if(!PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, set_topic))
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	new_topic = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtkchat->topic_text)));
	current_topic = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));

	if(current_topic && !g_utf8_collate(new_topic, current_topic)){
		g_free(new_topic);
		return;
	}

	if (current_topic)
		gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), current_topic);
	else
		gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), "");

	purple_protocol_chat_iface_set_topic(protocol, gc, purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)),
			new_topic);

	g_free(new_topic);
}

static gint
sort_chat_users(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata)
{
	PurpleChatUserFlags f1 = 0, f2 = 0;
	char *user1 = NULL, *user2 = NULL;
	gboolean buddy1 = FALSE, buddy2 = FALSE;
	gint ret = 0;

	gtk_tree_model_get(model, a,
	                   CHAT_USERS_ALIAS_KEY_COLUMN, &user1,
	                   CHAT_USERS_FLAGS_COLUMN, &f1,
	                   CHAT_USERS_WEIGHT_COLUMN, &buddy1,
	                   -1);
	gtk_tree_model_get(model, b,
	                   CHAT_USERS_ALIAS_KEY_COLUMN, &user2,
	                   CHAT_USERS_FLAGS_COLUMN, &f2,
	                   CHAT_USERS_WEIGHT_COLUMN, &buddy2,
	                   -1);

	/* Only sort by membership levels */
	f1 &= PURPLE_CHAT_USER_VOICE | PURPLE_CHAT_USER_HALFOP | PURPLE_CHAT_USER_OP |
			PURPLE_CHAT_USER_FOUNDER;
	f2 &= PURPLE_CHAT_USER_VOICE | PURPLE_CHAT_USER_HALFOP | PURPLE_CHAT_USER_OP |
			PURPLE_CHAT_USER_FOUNDER;

	ret = g_strcmp0(user1, user2);

	if (user1 != NULL && user2 != NULL) {
		if (f1 != f2) {
			/* sort more important users first */
			ret = (f1 > f2) ? -1 : 1;
		} else if (buddy1 != buddy2) {
			ret = (buddy1 > buddy2) ? -1 : 1;
		}
	}

	g_free(user1);
	g_free(user2);

	return ret;
}

static void
update_chat_alias(PurpleBuddy *buddy, PurpleChatConversation *chat, PurpleConnection *gc, PurpleProtocol *protocol)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	PurpleAccount *account = purple_conversation_get_account(PURPLE_CONVERSATION(chat));
	GtkTreeModel *model;
	char *normalized_name;
	GtkTreeIter iter;
	int f;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(chat != NULL);

	/* This is safe because this callback is only used in chats, not IMs. */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkconv->u.chat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	normalized_name = g_strdup(purple_normalize(account, purple_buddy_get_name(buddy)));

	do {
		char *name;

		gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

		if (purple_strequal(normalized_name, purple_normalize(account, name))) {
			const char *alias = name;
			char *tmp;
			char *alias_key = NULL;
			PurpleBuddy *buddy2;

			if (!purple_strequal(purple_chat_conversation_get_nick(chat), purple_normalize(account, name))) {
				/* This user is not me, so look into updating the alias. */

				if ((buddy2 = purple_blist_find_buddy(account, name)) != NULL) {
					alias = purple_buddy_get_contact_alias(buddy2);
				}

				tmp = g_utf8_casefold(alias, -1);
				alias_key = g_utf8_collate_key(tmp, -1);
				g_free(tmp);

				gtk_list_store_set(GTK_LIST_STORE(model), &iter,
								CHAT_USERS_ALIAS_COLUMN, alias,
								CHAT_USERS_ALIAS_KEY_COLUMN, alias_key,
								-1);
				g_free(alias_key);
			}
			g_free(name);
			break;
		}

		f = gtk_tree_model_iter_next(model, &iter);

		g_free(name);
	} while (f != 0);

	g_free(normalized_name);
}

static void
blist_node_aliased_cb(PurpleBlistNode *node, const char *old_alias, PurpleChatConversation *chat)
{
	PurpleConnection *gc;
	PurpleProtocol *protocol;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);

	g_return_if_fail(node != NULL);
	g_return_if_fail(conv != NULL);

	gc = purple_conversation_get_connection(conv);
	g_return_if_fail(gc != NULL);
	g_return_if_fail(purple_connection_get_protocol(gc) != NULL);
	protocol = purple_connection_get_protocol(gc);

	if (purple_protocol_get_options(protocol) & OPT_PROTO_UNIQUE_CHATNAME)
		return;

	if (PURPLE_IS_CONTACT(node))
	{
		PurpleBlistNode *bnode;

		for(bnode = node->child; bnode; bnode = bnode->next) {

			if(!PURPLE_IS_BUDDY(bnode))
				continue;

			update_chat_alias((PurpleBuddy *)bnode, chat, gc, protocol);
		}
	}
	else if (PURPLE_IS_BUDDY(node))
		update_chat_alias((PurpleBuddy *)node, chat, gc, protocol);
	else if (PURPLE_IS_CHAT(node) &&
			purple_conversation_get_account(conv) == purple_chat_get_account((PurpleChat*)node))
	{
		if (old_alias == NULL || g_utf8_collate(old_alias, purple_conversation_get_title(conv)) == 0)
			pidgin_conv_update_fields(conv, PIDGIN_CONV_SET_TITLE);
	}
}

static void
buddy_cb_common(PurpleBuddy *buddy, PurpleChatConversation *chat, gboolean is_buddy)
{
	GtkTreeModel *model;
	char *normalized_name;
	GtkTreeIter iter;
	GtkTextTag *texttag;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	int f;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(conv != NULL);

	/* Do nothing if the buddy does not belong to the conv's account */
	if (purple_buddy_get_account(buddy) != purple_conversation_get_account(conv))
		return;

	/* This is safe because this callback is only used in chats, not IMs. */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(PIDGIN_CONVERSATION(conv)->u.chat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	normalized_name = g_strdup(purple_normalize(purple_conversation_get_account(conv), purple_buddy_get_name(buddy)));

	do {
		char *name;

		gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &name, -1);

		if (purple_strequal(normalized_name, purple_normalize(purple_conversation_get_account(conv), name))) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			                   CHAT_USERS_WEIGHT_COLUMN, is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, -1);
			g_free(name);
			break;
		}

		f = gtk_tree_model_iter_next(model, &iter);

		g_free(name);
	} while (f != 0);

	g_free(normalized_name);

	blist_node_aliased_cb((PurpleBlistNode *)buddy, NULL, chat);

	texttag = get_buddy_tag(chat, purple_buddy_get_name(buddy), 0, FALSE); /* XXX: do we want the normalized name? */
	if (texttag) {
		g_object_set(texttag, "weight", is_buddy ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);
	}
}

static void
buddy_added_cb(PurpleBlistNode *node, PurpleChatConversation *chat)
{
	if (!PURPLE_IS_BUDDY(node))
		return;

	buddy_cb_common(PURPLE_BUDDY(node), chat, TRUE);
}

static void
buddy_removed_cb(PurpleBlistNode *node, PurpleChatConversation *chat)
{
	if (!PURPLE_IS_BUDDY(node))
		return;

	/* If there's another buddy for the same "dude" on the list, do nothing. */
	if (purple_blist_find_buddy(purple_buddy_get_account(PURPLE_BUDDY(node)),
		                  purple_buddy_get_name(PURPLE_BUDDY(node))) != NULL)
		return;

	buddy_cb_common(PURPLE_BUDDY(node), chat, FALSE);
}

static void
minimum_entry_lines_pref_cb(const char *name,
                            PurplePrefType type,
                            gconstpointer value,
                            gpointer data)
{
#if 0
	GList *l = purple_conversations_get_all();
	PurpleConversation *conv;
	while (l != NULL)
	{
		conv = (PurpleConversation *)l->data;

		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			resize_webview_cb(PIDGIN_CONVERSATION(conv));
		l = l->next;
	}
#endif
}

static void
setup_chat_topic(PidginConversation *gtkconv, GtkWidget *vbox)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	PurpleProtocol *protocol = purple_connection_get_protocol(gc);
	if (purple_protocol_get_options(protocol) & OPT_PROTO_CHAT_TOPIC)
	{
		GtkWidget *hbox, *label;
		PidginChatPane *gtkchat = gtkconv->u.chat;

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		label = gtk_label_new(_("Topic:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		gtkchat->topic_text = gtk_entry_new();
		gtk_widget_set_size_request(gtkchat->topic_text, -1, BUDDYICON_SIZE_MIN);

		if(!PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, set_topic)) {
			gtk_editable_set_editable(GTK_EDITABLE(gtkchat->topic_text), FALSE);
		} else {
			g_signal_connect(G_OBJECT(gtkchat->topic_text), "activate",
					G_CALLBACK(topic_callback), gtkconv);
		}

		gtk_box_pack_start(GTK_BOX(hbox), gtkchat->topic_text, TRUE, TRUE, 0);
		g_signal_connect(G_OBJECT(gtkchat->topic_text), "key_press_event",
			             G_CALLBACK(entry_key_press_cb), gtkconv);
	}
}

static gboolean
pidgin_conv_userlist_create_tooltip(GtkWidget *tipwindow, GtkTreePath *path,
		gpointer userdata, int *w, int *h)
{
	PidginConversation *gtkconv = userdata;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkconv->u.chat->list));
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleBlistNode *node;
	PurpleProtocol *protocol;
	PurpleAccount *account = purple_conversation_get_account(conv);
	char *who = NULL;

	if (purple_account_get_connection(account) == NULL)
		return FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, CHAT_USERS_NAME_COLUMN, &who, -1);

	protocol = purple_connection_get_protocol(purple_account_get_connection(account));
	node = (PurpleBlistNode*)(purple_blist_find_buddy(purple_conversation_get_account(conv), who));
	if (node && protocol && (purple_protocol_get_options(protocol) & OPT_PROTO_UNIQUE_CHATNAME))
		pidgin_blist_draw_tooltip(node, gtkconv->infopane);

	g_free(who);
	return FALSE;
}

static void
setup_chat_userlist(PidginConversation *gtkconv, GtkWidget *hpaned)
{
	PidginChatPane *gtkchat = gtkconv->u.chat;
	GtkWidget *lbox, *list;
	GtkListStore *ls;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	int ul_width;
	void *blist_handle = purple_blist_get_handle();
	PurpleConversation *conv = gtkconv->active_conv;

	/* Build the right pane. */
	lbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_paned_pack2(GTK_PANED(hpaned), lbox, FALSE, TRUE);
	gtk_widget_show(lbox);

	/* Setup the label telling how many people are in the room. */
	gtkchat->count = gtk_label_new(_("0 people in room"));
	gtk_label_set_ellipsize(GTK_LABEL(gtkchat->count), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start(GTK_BOX(lbox), gtkchat->count, FALSE, FALSE, 0);
	gtk_widget_show(gtkchat->count);

	/* Setup the list of users. */

	ls = gtk_list_store_new(CHAT_USERS_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
							G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
							GDK_TYPE_RGBA, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(ls), CHAT_USERS_ALIAS_KEY_COLUMN,
									sort_chat_users, NULL, NULL);

	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	/* Allow a user to specify gtkrc settings for the chat userlist only */
	gtk_widget_set_name(list, "pidgin_conv_userlist");

	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(G_OBJECT(rend),
				 "stock-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL),
				 NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
			"stock-id", CHAT_USERS_ICON_STOCK_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);
	ul_width = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width");
	gtk_widget_set_size_request(lbox, ul_width, -1);

	/* Hack to prevent completely collapsed userlist coming back with a 1 pixel width.
	 * I would have liked to use the GtkPaned "max-position", but for some reason that didn't work */
	if (ul_width == 0)
		gtk_paned_set_position(GTK_PANED(hpaned), 999999);

	g_signal_connect(G_OBJECT(list), "button_press_event",
					 G_CALLBACK(right_click_chat_cb), gtkconv);
	g_signal_connect(G_OBJECT(list), "row-activated",
					 G_CALLBACK(activate_list_cb), gtkconv);
	g_signal_connect(G_OBJECT(list), "popup-menu",
			 G_CALLBACK(gtkconv_chat_popup_menu_cb), gtkconv);
	g_signal_connect(G_OBJECT(lbox), "size-allocate", G_CALLBACK(lbox_size_allocate_cb), gtkconv);

	pidgin_tooltip_setup_for_treeview(list, gtkconv,
			pidgin_conv_userlist_create_tooltip, NULL);

	rend = gtk_cell_renderer_text_new();
	g_object_set(rend,
				 "foreground-set", TRUE,
				 "weight-set", TRUE,
				 NULL);
	g_object_set(G_OBJECT(rend), "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
	                                               "text", CHAT_USERS_ALIAS_COLUMN,
	                                               "foreground-rgba", CHAT_USERS_COLOR_COLUMN,
	                                               "weight", CHAT_USERS_WEIGHT_COLUMN,
	                                               NULL);

	purple_signal_connect(blist_handle, "blist-node-added",
						gtkchat, PURPLE_CALLBACK(buddy_added_cb), conv);
	purple_signal_connect(blist_handle, "blist-node-removed",
						gtkchat, PURPLE_CALLBACK(buddy_removed_cb), conv);
	purple_signal_connect(blist_handle, "blist-node-aliased",
						gtkchat, PURPLE_CALLBACK(blist_node_aliased_cb), conv);

	gtk_tree_view_column_set_expand(col, TRUE);
	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
	gtk_widget_show(list);

	gtkchat->list = list;

	gtk_box_pack_start(GTK_BOX(lbox),
		pidgin_make_scrollable(list, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1),
		TRUE, TRUE, 0);
}

static gboolean
pidgin_conv_create_tooltip(GtkWidget *tipwindow, gpointer userdata, int *w, int *h)
{
	PurpleBlistNode *node = NULL;
	PurpleConversation *conv;
	PidginConversation *gtkconv = userdata;

	conv = gtkconv->active_conv;
	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		node = (PurpleBlistNode*)(purple_blist_find_chat(purple_conversation_get_account(conv), purple_conversation_get_name(conv)));
		if (!node)
			node = g_object_get_data(G_OBJECT(gtkconv->history), "transient_chat");
	} else {
		node = (PurpleBlistNode*)(purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv)));
#if 0
		/* Using the transient blist nodes to show the tooltip doesn't quite work yet. */
		if (!node)
			node = g_object_get_data(G_OBJECT(gtkconv->webview), "transient_buddy");
#endif
	}

	if (node)
		pidgin_blist_draw_tooltip(node, gtkconv->infopane);
	return FALSE;
}

static GtkWidget *
setup_common_pane(PidginConversation *gtkconv)
{
	GtkWidget *vbox, *sw, *event_box, *view;
	GtkCellRenderer *rend;
	GtkTreePath *path;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleBuddy *buddy;
	gboolean chat = PURPLE_IS_CHAT_CONVERSATION(conv);
	int buddyicon_size = 0;

	/* Setup the top part of the pane */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_show(vbox);

	/* Setup the info pane */
	event_box = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
	gtk_widget_show(event_box);
	gtkconv->infopane_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), event_box, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event_box), gtkconv->infopane_hbox);
	gtk_widget_show(gtkconv->infopane_hbox);
	gtk_widget_add_events(event_box,
	                      GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(event_box), "button-press-event",
	                 G_CALLBACK(infopane_press_cb), gtkconv);

	pidgin_tooltip_setup_for_widget(event_box, gtkconv,
		pidgin_conv_create_tooltip, NULL);

	gtkconv->infopane = gtk_cell_view_new();
	gtkconv->infopane_model = gtk_list_store_new(CONV_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF);
	gtk_cell_view_set_model(GTK_CELL_VIEW(gtkconv->infopane),
				GTK_TREE_MODEL(gtkconv->infopane_model));
	g_object_unref(gtkconv->infopane_model);
	gtk_list_store_append(gtkconv->infopane_model, &(gtkconv->infopane_iter));
	gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox), gtkconv->infopane, TRUE, TRUE, 0);
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(gtkconv->infopane), path);
	gtk_tree_path_free(path);

	if (chat) {
		/* This empty widget is used to ensure that the infopane is consistently
		   sized for chat windows. The correct fix is to put an icon in the chat
		   window as well, because that would make "Set Custom Icon" consistent
		   for both the buddy list and the chat window, but PidginConversation
		   is pretty much stuck until 3.0. */
		GtkWidget *sizing_vbox;
		sizing_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_size_request(sizing_vbox, -1, BUDDYICON_SIZE_MIN);
		gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox), sizing_vbox, FALSE, FALSE, 0);
		gtk_widget_show(sizing_vbox);
	}
	else {
		gtkconv->u.im->icon_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

		if ((buddy = purple_blist_find_buddy(purple_conversation_get_account(conv),
						purple_conversation_get_name(conv))) != NULL) {
			PurpleContact *contact = purple_buddy_get_contact(buddy);
			if (contact) {
				buddyicon_size = purple_blist_node_get_int((PurpleBlistNode*)contact, "pidgin-infopane-iconsize");
			}
		}
		buddyicon_size = CLAMP(buddyicon_size, BUDDYICON_SIZE_MIN, BUDDYICON_SIZE_MAX);
		gtk_widget_set_size_request(gtkconv->u.im->icon_container, -1, buddyicon_size);

		gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox),
				   gtkconv->u.im->icon_container, FALSE, FALSE, 0);

		gtk_widget_show(gtkconv->u.im->icon_container);
	}

	gtk_widget_show(gtkconv->infopane);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "stock-id", CONV_ICON_COLUMN, NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0,
			"stock-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL),
			NULL);

	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "markup", CONV_TEXT_COLUMN, NULL);
	g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);

	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "pixbuf", CONV_PROTOCOL_ICON_COLUMN, NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 3, "ypad", 0, NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(gtkconv->infopane), rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(gtkconv->infopane), rend, "pixbuf", CONV_EMBLEM_COLUMN, NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0, NULL);

	/* Setup the history widget */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(sw),
		GTK_POLICY_NEVER,
		GTK_POLICY_ALWAYS
	);

	gtkconv->history_buffer = talkatu_history_buffer_new();
	gtkconv->history = talkatu_history_new();
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(gtkconv->history), gtkconv->history_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gtkconv->history), GTK_WRAP_WORD);
	gtk_container_add(GTK_CONTAINER(sw), gtkconv->history);

	if (chat) {
		GtkWidget *hpaned;

		/* Add the topic */
		setup_chat_topic(gtkconv, vbox);

		/* Add the talkatu history */
		hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
		gtk_widget_show(hpaned);
		gtk_paned_pack1(GTK_PANED(hpaned), sw, TRUE, TRUE);

		/* Now add the userlist */
		setup_chat_userlist(gtkconv, hpaned);
	} else {
		gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	}
	gtk_widget_show_all(sw);

	g_object_set_data(G_OBJECT(gtkconv->history), "gtkconv", gtkconv);

	g_signal_connect(G_OBJECT(gtkconv->history), "key_press_event",
	                 G_CALLBACK(refocus_entry_cb), gtkconv);
	g_signal_connect(G_OBJECT(gtkconv->history), "key_release_event",
	                 G_CALLBACK(refocus_entry_cb), gtkconv);

	gtkconv->lower_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), gtkconv->lower_hbox, FALSE, FALSE, 0);
	gtk_widget_show(gtkconv->lower_hbox);

	/* Setup the entry widget and all signals */
	gtkconv->editor = talkatu_editor_new();
	talkatu_editor_set_buffer(TALKATU_EDITOR(gtkconv->editor), talkatu_html_buffer_new());
	gtk_box_pack_start(GTK_BOX(gtkconv->lower_hbox), gtkconv->editor, TRUE, TRUE, 0);

	view = talkatu_editor_get_view(TALKATU_EDITOR(gtkconv->editor));
	gtk_widget_set_name(view, "pidgin_conv_entry");
	talkatu_view_set_send_binding(TALKATU_VIEW(view), TALKATU_VIEW_SEND_BINDING_RETURN | TALKATU_VIEW_SEND_BINDING_KP_ENTER);
	g_signal_connect(
		G_OBJECT(view),
		"send-message",
		G_CALLBACK(send_cb),
		gtkconv
	);

	if (!chat) {
		/* For sending typing notifications for IMs */
		gtkconv->u.im->typing_timer = 0;
		gtkconv->u.im->animate = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons");
		gtkconv->u.im->show_icon = TRUE;
	}

	return vbox;
}

static PidginConversation *
pidgin_conv_find_gtkconv(PurpleConversation * conv)
{
	PurpleBuddy *bud = purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
	PurpleContact *c;
	PurpleBlistNode *cn, *bn;

	if (!bud)
		return NULL;

	if (!(c = purple_buddy_get_contact(bud)))
		return NULL;

	cn = PURPLE_BLIST_NODE(c);
	for (bn = purple_blist_node_get_first_child(cn); bn; bn = purple_blist_node_get_sibling_next(bn)) {
		PurpleBuddy *b = PURPLE_BUDDY(bn);
		PurpleIMConversation *im;
		if ((im = purple_conversations_find_im_with_account(purple_buddy_get_name(b), purple_buddy_get_account(b)))) {
			if (PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im)))
				return PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
		}
	}

	return NULL;
}

static void
buddy_update_cb(PurpleBlistNode *bnode, gpointer null)
{
	GList *list;

	g_return_if_fail(bnode);
	if (!PURPLE_IS_BUDDY(bnode))
		return;

	for (list = pidgin_conv_windows_get_list(); list; list = list->next)
	{
		PidginConvWindow *win = list->data;
		PurpleConversation *conv = pidgin_conv_window_get_active_conversation(win);

		if (!PURPLE_IS_IM_CONVERSATION(conv))
			continue;

		pidgin_conv_update_fields(conv, PIDGIN_CONV_MENU);
	}
}

static gboolean
ignore_middle_click(GtkWidget *widget, GdkEventButton *e, gpointer null)
{
	/* A click on the pane is propagated to the notebook containing the pane.
	 * So if Stu accidentally aims high and middle clicks on the pane-handle,
	 * it causes a conversation tab to close. Let's stop that from happening.
	 */
	if (e->button == GDK_BUTTON_MIDDLE && e->type == GDK_BUTTON_PRESS)
		return TRUE;
	return FALSE;
}

/**************************************************************************
 * Conversation UI operations
 **************************************************************************/
static void
private_gtkconv_new(PurpleConversation *conv, gboolean hidden)
{
	PidginConversation *gtkconv;
	GtkWidget *pane = NULL;
	GtkWidget *tab_cont;
	PurpleBlistNode *convnode;

	if (PURPLE_IS_IM_CONVERSATION(conv) && (gtkconv = pidgin_conv_find_gtkconv(conv))) {
		purple_conversation_set_ui_data(conv, gtkconv);
		if (!g_list_find(gtkconv->convs, conv))
			gtkconv->convs = g_list_prepend(gtkconv->convs, conv);
		pidgin_conv_switch_active_conversation(conv);
		return;
	}

	gtkconv = g_new0(PidginConversation, 1);
	purple_conversation_set_ui_data(conv, gtkconv);
	gtkconv->active_conv = conv;
	gtkconv->convs = g_list_prepend(gtkconv->convs, conv);
	gtkconv->send_history = g_list_append(NULL, NULL);

	/* Setup some initial variables. */
	gtkconv->unseen_state = PIDGIN_UNSEEN_NONE;
	gtkconv->unseen_count = 0;
	gtkconv->last_flags = 0;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		gtkconv->u.im = g_malloc0(sizeof(PidginImPane));
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		gtkconv->u.chat = g_malloc0(sizeof(PidginChatPane));
	}
	pane = setup_common_pane(gtkconv);

	if (pane == NULL) {
		if (PURPLE_IS_CHAT_CONVERSATION(conv))
			g_free(gtkconv->u.chat);
		else if (PURPLE_IS_IM_CONVERSATION(conv))
			g_free(gtkconv->u.im);

		g_free(gtkconv);
		purple_conversation_set_ui_data(conv, NULL);
		return;
	}

	g_signal_connect(G_OBJECT(pane), "button_press_event",
	                 G_CALLBACK(ignore_middle_click), NULL);

	/* Setup the container for the tab. */
	gtkconv->tab_cont = tab_cont = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	g_object_set_data(G_OBJECT(tab_cont), "PidginConversation", gtkconv);
	gtk_container_set_border_width(GTK_CONTAINER(tab_cont), PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(tab_cont), pane, TRUE, TRUE, 0);
	gtk_widget_show(pane);

	convnode = get_conversation_blist_node(conv);
	if (convnode == NULL || !purple_blist_node_get_bool(convnode, "gtk-mute-sound"))
		gtkconv->make_sound = TRUE;

	if (convnode != NULL && purple_blist_node_has_setting(convnode, "enable-logging")) {
		gboolean logging = purple_blist_node_get_bool(convnode, "enable-logging");
		purple_conversation_set_logging(conv, logging);
	}

	talkatu_editor_set_toolbar_visible(
		TALKATU_EDITOR(gtkconv->editor),
		purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar")
	);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		gtk_widget_show(gtkconv->infopane_hbox);
	else
		gtk_widget_hide(gtkconv->infopane_hbox);


	g_signal_connect_swapped(G_OBJECT(pane), "focus",
	                         G_CALLBACK(gtk_widget_grab_focus),
	                         gtkconv->editor);

	if (hidden)
		pidgin_conv_window_add_gtkconv(hidden_convwin, gtkconv);
	else
		pidgin_conv_placement_place(gtkconv);

	if (generated_nick_colors == NULL) {
		GdkColor color;
		GdkRGBA rgba;
		color = gtk_widget_get_style(gtkconv->history)->base[GTK_STATE_NORMAL];
		rgba.red = color.red / 65535.0;
		rgba.green = color.green / 65535.0;
		rgba.blue = color.blue / 65535.0;
		rgba.alpha = 1.0;
		generated_nick_colors = generate_nick_colors(NICK_COLOR_GENERATE_COUNT, rgba);
	}

	gtkconv->nick_colors = g_array_ref(generated_nick_colors);
}

static void
pidgin_conv_new_hidden(PurpleConversation *conv)
{
	private_gtkconv_new(conv, TRUE);
}

void
pidgin_conv_new(PurpleConversation *conv)
{
	private_gtkconv_new(conv, FALSE);
	if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		purple_signal_emit(pidgin_conversations_get_handle(),
				"conversation-displayed", PIDGIN_CONVERSATION(conv));
}

static void
received_im_msg_cb(PurpleAccount *account, char *sender, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags)
{
	PurpleConversationUiOps *ui_ops = pidgin_conversations_get_conv_ui_ops();
	gboolean hide = FALSE;
	guint timer;

	/* create hidden conv if hide_new pref is always */
	if (purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "always"))
		hide = TRUE;

	/* create hidden conv if hide_new pref is away and account is away */
	if (purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away") &&
	    !purple_status_is_available(purple_account_get_active_status(account)))
		hide = TRUE;

	if (conv && PIDGIN_IS_PIDGIN_CONVERSATION(conv) && !hide) {
		PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
		if (gtkconv->win == hidden_convwin) {
			pidgin_conv_attach_to_conversation(gtkconv->active_conv);
		}
		return;
	}

	if (hide) {
		ui_ops->create_conversation = pidgin_conv_new_hidden;
		purple_im_conversation_new(account, sender);
		ui_ops->create_conversation = pidgin_conv_new;
	}

	/* Somebody wants to keep this conversation around, so don't time it out */
	if (conv) {
		timer = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "close-timer"));
		if (timer) {
			g_source_remove(timer);
			g_object_set_data(G_OBJECT(conv), "close-timer", GINT_TO_POINTER(0));
		}
	}
}

static void
pidgin_conv_destroy(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

	gtkconv->convs = g_list_remove(gtkconv->convs, conv);
	/* Don't destroy ourselves until all our convos are gone */
	if (gtkconv->convs) {
		/* Make sure the destroyed conversation is not the active one */
		if (gtkconv->active_conv == conv) {
			gtkconv->active_conv = gtkconv->convs->data;
			purple_conversation_update(gtkconv->active_conv, PURPLE_CONVERSATION_UPDATE_FEATURES);
		}
		return;
	}

	pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);

	/* If the "Save Conversation" or "Save Icon" dialogs are open then close them */
	purple_request_close_with_handle(gtkconv);
	purple_notify_close_with_handle(gtkconv);

	gtk_widget_destroy(gtkconv->tab_cont);
	g_object_unref(gtkconv->tab_cont);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		if (gtkconv->u.im->icon_timer != 0)
			g_source_remove(gtkconv->u.im->icon_timer);

		if (gtkconv->u.im->anim != NULL)
			g_object_unref(G_OBJECT(gtkconv->u.im->anim));

		if (gtkconv->u.im->typing_timer != 0)
			g_source_remove(gtkconv->u.im->typing_timer);

		g_free(gtkconv->u.im);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		purple_signals_disconnect_by_handle(gtkconv->u.chat);
		g_free(gtkconv->u.chat);
	}

	gtkconv->send_history = g_list_first(gtkconv->send_history);
	g_list_free_full(gtkconv->send_history, g_free);

	if (gtkconv->attach_timer) {
		g_source_remove(gtkconv->attach_timer);
	}

	g_array_unref(gtkconv->nick_colors);

	g_free(gtkconv);
}

#if 0
static const char *
get_text_tag_color(GtkTextTag *tag)
{
	GdkRGBA *color = NULL;
	gboolean set = FALSE;
	static char colcode[] = "#XXXXXX";
	if (tag)
		g_object_get(G_OBJECT(tag), "foreground-set", &set, "foreground-rgba", &color, NULL);
	if (set && color)
		g_snprintf(colcode, sizeof(colcode), "#%02x%02x%02x",
				(unsigned int)(color->red * 255),
				(unsigned int)(color->green * 255),
				(unsigned int)(color->blue * 255));
	else
		colcode[0] = '\0';
	if (color)
		gdk_rgba_free(color);
	return colcode;
}

/* The callback for an event on a link tag. */
static gboolean buddytag_event(GtkTextTag *tag, GObject *imhtml,
		GdkEvent *event, GtkTextIter *arg2, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS
			|| event->type == GDK_2BUTTON_PRESS) {
		GdkEventButton *btn_event = (GdkEventButton*) event;
		PurpleConversation *conv = data;
		char *buddyname;
		gchar *name;

		g_object_get(G_OBJECT(tag), "name", &name, NULL);

		/* strlen("BUDDY " or "HILIT ") == 6 */
		g_return_val_if_fail((name != NULL) && (strlen(name) > 6), FALSE);

		buddyname = name + 6;

		/* emit chat-nick-clicked signal */
		if (event->type == GDK_BUTTON_PRESS) {
			gint plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
						pidgin_conversations_get_handle(), "chat-nick-clicked",
						data, buddyname, btn_event->button));
			if (plugin_return) {
				g_free(name);
				return TRUE;
			}
		}

		if (btn_event->button == GDK_BUTTON_PRIMARY && event->type == GDK_2BUTTON_PRESS) {
			chat_do_im(PIDGIN_CONVERSATION(conv), buddyname);
			g_free(name);

			return TRUE;
		} else if (btn_event->button == GDK_BUTTON_MIDDLE && event->type == GDK_2BUTTON_PRESS) {
			chat_do_info(PIDGIN_CONVERSATION(conv), buddyname);
			g_free(name);

			return TRUE;
		} else if (gdk_event_triggers_context_menu(event)) {
			GtkTextIter start, end;

			/* we shouldn't display the popup
			 * if the user has selected something: */
			if (!gtk_text_buffer_get_selection_bounds(
						gtk_text_iter_get_buffer(arg2),
						&start, &end)) {
				GtkWidget *menu = NULL;
				PurpleConnection *gc =
					purple_conversation_get_connection(conv);

				menu = create_chat_menu(conv, buddyname, gc);
				gtk_menu_popup_at_pointer(GTK_MENU(menu), event);

				g_free(name);

				/* Don't propagate the event any further */
				return TRUE;
			}
		}

		g_free(name);
	}

	return FALSE;
}
#endif

static GtkTextTag *get_buddy_tag(PurpleChatConversation *chat, const char *who, PurpleMessageFlags flag,
		gboolean create)
{
/* TODO WEBKIT */
#if 0
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	GtkTextTag *buddytag;
	gchar *str;
	gboolean highlight = (flag & PURPLE_MESSAGE_NICK);
	GtkTextBuffer *buffer = GTK_IMHTML(gtkconv->imhtml)->text_buffer;

	str = g_strdup_printf(highlight ? "HILIT %s" : "BUDDY %s", who);

	buddytag = gtk_text_tag_table_lookup(
			gtk_text_buffer_get_tag_table(buffer), str);

	if (buddytag == NULL && create) {
		if (highlight)
			buddytag = gtk_text_buffer_create_tag(buffer, str,
					"foreground", get_text_tag_color(gtk_text_tag_table_lookup(
							gtk_text_buffer_get_tag_table(buffer), "highlight-name")),
					"weight", PANGO_WEIGHT_BOLD,
					NULL);
		else
			buddytag = gtk_text_buffer_create_tag(
					buffer, str,
					"foreground-rgba", get_nick_color(gtkconv, who),
					"weight", purple_blist_find_buddy(purple_conversation_get_account(conv), who) ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
					NULL);

		g_object_set_data(G_OBJECT(buddytag), "cursor", "");
		g_signal_connect(G_OBJECT(buddytag), "event",
				G_CALLBACK(buddytag_event), conv);
	}

	g_free(str);

	return buddytag;
#endif /* if 0 */
	return NULL;
}

static gboolean
writing_msg(PurpleConversation *conv, PurpleMessage *msg, gpointer _unused)
{
	PidginConversation *gtkconv;

	g_return_val_if_fail(msg != NULL, FALSE);

	if (!(purple_message_get_flags(msg) & PURPLE_MESSAGE_ACTIVE_ONLY))
		return FALSE;

	g_return_val_if_fail(conv != NULL, FALSE);
	gtkconv = PIDGIN_CONVERSATION(conv);
	g_return_val_if_fail(gtkconv != NULL, FALSE);

	if (conv == gtkconv->active_conv)
		return FALSE;

	purple_debug_info("gtkconv",
		"Suppressing message for an inactive conversation");

	return TRUE;
}

static void
pidgin_conv_write_conv(PurpleConversation *conv, PurpleMessage *pmsg)
{
	PidginMessage *pidgin_msg = NULL;
	PurpleMessageFlags flags;
	PidginConversation *gtkconv;
	PurpleConnection *gc;
	PurpleAccount *account;
	gboolean plugin_return;

	g_return_if_fail(conv != NULL);
	gtkconv = PIDGIN_CONVERSATION(conv);
	g_return_if_fail(gtkconv != NULL);
	flags = purple_message_get_flags(pmsg);

#if 0
	if (gtkconv->attach_timer) {
		/* We are currently in the process of filling up the buffer with the message
		 * history of the conversation. So we do not need to add the message here.
		 * Instead, this message will be added to the message-list, which in turn will
		 * be processed and displayed by the attach-callback.
		 */
		return;
	}

	if (conv != gtkconv->active_conv)
	{
		/* Set the active conversation to the one that just messaged us. */
		/* TODO: consider not doing this if the account is offline or something */
		if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV))
			pidgin_conv_switch_active_conversation(conv);
	}
#endif

	account = purple_conversation_get_account(conv);
	g_return_if_fail(account != NULL);
	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL || !(flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)));

	plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
		pidgin_conversations_get_handle(),
		(PURPLE_IS_IM_CONVERSATION(conv) ? "displaying-im-msg" : "displaying-chat-msg"),
		conv, pmsg));
	if (plugin_return)
	{
		return;
	}

	pidgin_msg = pidgin_message_new(pmsg);
	talkatu_history_buffer_write_message(
		TALKATU_HISTORY_BUFFER(gtkconv->history_buffer),
		TALKATU_MESSAGE(pidgin_msg)
	);

	/* Tab highlighting stuff */
	if (!(flags & PURPLE_MESSAGE_SEND) && !pidgin_conv_has_focus(conv))
	{
		PidginUnseenState unseen = PIDGIN_UNSEEN_NONE;

		if ((flags & PURPLE_MESSAGE_NICK) == PURPLE_MESSAGE_NICK)
			unseen = PIDGIN_UNSEEN_NICK;
		else if (((flags & PURPLE_MESSAGE_SYSTEM) == PURPLE_MESSAGE_SYSTEM) ||
			  ((flags & PURPLE_MESSAGE_ERROR) == PURPLE_MESSAGE_ERROR))
			unseen = PIDGIN_UNSEEN_EVENT;
		else if ((flags & PURPLE_MESSAGE_NO_LOG) == PURPLE_MESSAGE_NO_LOG)
			unseen = PIDGIN_UNSEEN_NO_LOG;
		else
			unseen = PIDGIN_UNSEEN_TEXT;

		gtkconv_set_unseen(gtkconv, unseen);
	}

	/* on rejoin only request message history from after this message */
	if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV) &&
		PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *chat = purple_blist_find_chat(
			purple_conversation_get_account(conv),
			purple_conversation_get_name(conv));
		if (chat) {
			GHashTable *comps = purple_chat_get_components(chat);
			time_t now, history_since, prev_history_since = 0;
			struct tm *history_since_tm;
			const char *history_since_s, *prev_history_since_s;

			history_since = purple_message_get_time(pmsg) + 1;

			prev_history_since_s = g_hash_table_lookup(comps,
				"history_since");
			if (prev_history_since_s != NULL)
				prev_history_since = purple_str_to_time(
					prev_history_since_s, TRUE, NULL, NULL,
					NULL);

			now = time(NULL);
			/* in case of incorrectly stored timestamps */
			if (prev_history_since > now)
				prev_history_since = now;
			/* in case of delayed messages */
			if (history_since < prev_history_since)
				history_since = prev_history_since;

			history_since_tm = gmtime(&history_since);
			history_since_s = purple_utf8_strftime(
				"%Y-%m-%dT%H:%M:%SZ", history_since_tm);
			if (!purple_strequal(prev_history_since_s,
				history_since_s))
				g_hash_table_replace(comps,
					g_strdup("history_since"),
					g_strdup(history_since_s));
		}
	}

	purple_signal_emit(pidgin_conversations_get_handle(),
		(PURPLE_IS_IM_CONVERSATION(conv) ? "displayed-im-msg" : "displayed-chat-msg"),
		conv, pmsg);
	update_typing_message(gtkconv, NULL);
}

static gboolean get_iter_from_chatuser(PurpleChatUser *cb, GtkTreeIter *iter)
{
	GtkTreeRowReference *ref;
	GtkTreePath *path;
	GtkTreeModel *model;

	g_return_val_if_fail(cb != NULL, FALSE);

	ref = purple_chat_user_get_ui_data(cb);
	if (!ref)
		return FALSE;

	if ((path = gtk_tree_row_reference_get_path(ref)) == NULL)
		return FALSE;

	model = gtk_tree_row_reference_get_model(ref);
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), iter, path)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	gtk_tree_path_free(path);
	return TRUE;
}

static void
pidgin_conv_chat_add_users(PurpleChatConversation *chat, GList *cbuddies, gboolean new_arrivals)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkListStore *ls;
	GList *l;

	char tmp[BUF_LONG];
	int num_users;

	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	num_users = purple_chat_conversation_get_users_count(chat);

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users),
			   num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);

	ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)));

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),  GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID,
										 GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID);

	l = cbuddies;
	while (l != NULL) {
		add_chat_user_common(chat, (PurpleChatUser *)l->data, NULL);
		l = l->next;
	}

	/* Currently GTK+ maintains our sorted list after it's in the tree.
	 * This may change if it turns out we can manage it faster ourselves.
	 */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),  CHAT_USERS_ALIAS_KEY_COLUMN,
										 GTK_SORT_ASCENDING);
}

static void
pidgin_conv_chat_rename_user(PurpleChatConversation *chat, const char *old_name,
			      const char *new_name, const char *new_alias)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	PurpleChatUser *old_chatuser, *new_chatuser;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTextTag *tag;

	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	if ((tag = get_buddy_tag(chat, old_name, 0, FALSE)))
		g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);
	if ((tag = get_buddy_tag(chat, old_name, PURPLE_MESSAGE_NICK, FALSE)))
		g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);

	old_chatuser = purple_chat_conversation_find_user(chat, old_name);
	if (!old_chatuser)
		return;

	if (get_iter_from_chatuser(old_chatuser, &iter)) {
		GtkTreeRowReference *ref = purple_chat_user_get_ui_data(old_chatuser);

		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		gtk_tree_row_reference_free(ref);
		purple_chat_user_set_ui_data(old_chatuser, NULL);
	}

	g_return_if_fail(new_alias != NULL);

	new_chatuser = purple_chat_conversation_find_user(chat, new_name);

	add_chat_user_common(chat, new_chatuser, old_name);
}

static void
pidgin_conv_chat_remove_users(PurpleChatConversation *chat, GList *users)
{
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *l;
	char tmp[BUF_LONG];
	int num_users;
	gboolean f;
	GtkTextTag *tag;

	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	num_users = purple_chat_conversation_get_users_count(chat);

	for (l = users; l != NULL; l = l->next) {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

		if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
			/* XXX: Break? */
			continue;

		do {
			char *val;

			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
							   CHAT_USERS_NAME_COLUMN, &val, -1);

			if (!purple_utf8_strcasecmp((char *)l->data, val)) {
				f = gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
			}
			else
				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

			g_free(val);
		} while (f);

		if ((tag = get_buddy_tag(chat, l->data, 0, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);
		if ((tag = get_buddy_tag(chat, l->data, PURPLE_MESSAGE_NICK, FALSE)))
			g_object_set(G_OBJECT(tag), "style", PANGO_STYLE_ITALIC, NULL);
	}

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users), num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);
}

static void
pidgin_conv_chat_update_user(PurpleChatUser *chatuser)
{
	PurpleChatConversation *chat;
	PidginConversation *gtkconv;
	PidginChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (!chatuser)
		return;

	chat = purple_chat_user_get_chat(chatuser);
	gtkconv = PIDGIN_CONVERSATION(PURPLE_CONVERSATION(chat));
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
		return;

	if (get_iter_from_chatuser(chatuser, &iter)) {
		GtkTreeRowReference *ref = purple_chat_user_get_ui_data(chatuser);
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		gtk_tree_row_reference_free(ref);
		purple_chat_user_set_ui_data(chatuser, NULL);
	}

	add_chat_user_common(chat, chatuser, NULL);
}

gboolean
pidgin_conv_has_focus(PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
	PidginConvWindow *win;
	gboolean has_focus;

	win = gtkconv->win;

	g_object_get(G_OBJECT(win->window), "has-toplevel-focus", &has_focus, NULL);

	if (has_focus && pidgin_conv_window_is_active_conversation(conv))
		return TRUE;

	return FALSE;
}

/*
 * Makes sure all the menu items and all the buttons are hidden/shown and
 * sensitive/insensitive.  This is called after changing tabs and when an
 * account signs on or off.
 */
static void
gray_stuff_out(PidginConversation *gtkconv)
{
	PidginConvWindow *win;
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleConnection *gc;
	PurpleProtocol *protocol = NULL;
	GdkPixbuf *window_icon = NULL;
//	PidginWebViewButtons buttons;
	PurpleAccount *account;

	win     = pidgin_conv_get_window(gtkconv);
	gc      = purple_conversation_get_connection(conv);
	account = purple_conversation_get_account(conv);

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if (win->menu->send_to != NULL)
		update_send_to_selection(win);

	/*
	 * Handle hiding and showing stuff based on what type of conv this is.
	 * Stuff that Purple IMs support in general should be shown for IM
	 * conversations.  Stuff that Purple chats support in general should be
	 * shown for chat conversations.  It doesn't matter whether the protocol
	 * supports it or not--that only affects if the button or menu item
	 * is sensitive or not.
	 */
	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		/* Show stuff that applies to IMs, hide stuff that applies to chats */

		/* Deal with menu items */
		gtk_action_set_visible(win->menu->view_log, TRUE);
		gtk_action_set_visible(win->menu->send_file, TRUE);
		gtk_action_set_visible(win->menu->get_attention, TRUE);
		gtk_action_set_visible(win->menu->add_pounce, TRUE);
		gtk_action_set_visible(win->menu->get_info, TRUE);
		gtk_action_set_visible(win->menu->invite, FALSE);
		gtk_action_set_visible(win->menu->alias, TRUE);
		if (purple_account_privacy_check(account, purple_conversation_get_name(conv))) {
			gtk_action_set_visible(win->menu->unblock, FALSE);
			gtk_action_set_visible(win->menu->block, TRUE);
		} else {
			gtk_action_set_visible(win->menu->block, FALSE);
			gtk_action_set_visible(win->menu->unblock, TRUE);
		}

		if (purple_blist_find_buddy(account, purple_conversation_get_name(conv)) == NULL) {
			gtk_action_set_visible(win->menu->add, TRUE);
			gtk_action_set_visible(win->menu->remove, FALSE);
		} else {
			gtk_action_set_visible(win->menu->remove, TRUE);
			gtk_action_set_visible(win->menu->add, FALSE);
		}

		gtk_action_set_visible(win->menu->insert_link, TRUE);
		gtk_action_set_visible(win->menu->insert_image, TRUE);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		/* Show stuff that applies to Chats, hide stuff that applies to IMs */

		/* Deal with menu items */
		gtk_action_set_visible(win->menu->view_log, TRUE);
		gtk_action_set_visible(win->menu->send_file, FALSE);
		gtk_action_set_visible(win->menu->get_attention, FALSE);
		gtk_action_set_visible(win->menu->add_pounce, FALSE);
		gtk_action_set_visible(win->menu->get_info, FALSE);
		gtk_action_set_visible(win->menu->invite, TRUE);
		gtk_action_set_visible(win->menu->alias, TRUE);
		gtk_action_set_visible(win->menu->block, FALSE);
		gtk_action_set_visible(win->menu->unblock, FALSE);

		if ((account == NULL) || purple_blist_find_chat(account, purple_conversation_get_name(conv)) == NULL) {
			/* If the chat is NOT in the buddy list */
			gtk_action_set_visible(win->menu->add, TRUE);
			gtk_action_set_visible(win->menu->remove, FALSE);
		} else {
			/* If the chat IS in the buddy list */
			gtk_action_set_visible(win->menu->add, FALSE);
			gtk_action_set_visible(win->menu->remove, TRUE);
		}

		gtk_action_set_visible(win->menu->insert_link, TRUE);
		gtk_action_set_visible(win->menu->insert_image, TRUE);
	}

	/*
	 * Handle graying stuff out based on whether an account is connected
	 * and what features that account supports.
	 */
	if ((gc != NULL) &&
		(!PURPLE_IS_CHAT_CONVERSATION(conv) ||
		 !purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv)) ))
	{
		PurpleConnectionFlags features = purple_conversation_get_features(conv);
		/* Account is online */
		/* Deal with the toolbar */
#if 0
		if (features & PURPLE_CONNECTION_FLAG_HTML)
		{
			buttons = PIDGIN_WEBVIEW_ALL; /* Everything on */
			if (features & PURPLE_CONNECTION_FLAG_NO_BGCOLOR)
				buttons &= ~PIDGIN_WEBVIEW_BACKCOLOR;
			if (features & PURPLE_CONNECTION_FLAG_NO_FONTSIZE)
			{
				buttons &= ~PIDGIN_WEBVIEW_GROW;
				buttons &= ~PIDGIN_WEBVIEW_SHRINK;
			}
			if (features & PURPLE_CONNECTION_FLAG_NO_URLDESC)
				buttons &= ~PIDGIN_WEBVIEW_LINKDESC
		} else {
			buttons = PIDGIN_WEBVIEW_SMILEY | PIDGIN_WEBVIEW_IMAGE;
		}

		if (features & PURPLE_CONNECTION_FLAG_NO_IMAGES)
			buttons &= ~PIDGIN_WEBVIEW_IMAGE;

		if (features & PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY)
			buttons |= PIDGIN_WEBVIEW_CUSTOM_SMILEY;
		else
			buttons &= ~PIDGIN_WEBVIEW_CUSTOM_SMILEY;

		pidgin_webview_set_format_functions(PIDGIN_WEBVIEW(gtkconv->entry), buttons);
#endif

		/* Deal with menu items */
		gtk_action_set_sensitive(win->menu->view_log, TRUE);
		gtk_action_set_sensitive(win->menu->add_pounce, TRUE);
		gtk_action_set_sensitive(win->menu->get_info, (PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, get_info)));
		gtk_action_set_sensitive(win->menu->invite, (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, invite)));
		gtk_action_set_sensitive(win->menu->insert_link, (features & PURPLE_CONNECTION_FLAG_HTML));
		gtk_action_set_sensitive(win->menu->insert_image, !(features & PURPLE_CONNECTION_FLAG_NO_IMAGES));

		if (PURPLE_IS_IM_CONVERSATION(conv))
		{
			gboolean can_send_file = FALSE;
			const gchar *name = purple_conversation_get_name(conv);

			if (PURPLE_IS_PROTOCOL_XFER(protocol) &&
			    purple_protocol_xfer_can_receive(PURPLE_PROTOCOL_XFER(protocol), gc, name)
			) {
				can_send_file = TRUE;
			}

			gtk_action_set_sensitive(win->menu->add, (PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, add_buddy)));
			gtk_action_set_sensitive(win->menu->remove, (PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, remove_buddy)));
			gtk_action_set_sensitive(win->menu->send_file, can_send_file);
			gtk_action_set_sensitive(win->menu->get_attention, (PURPLE_IS_PROTOCOL_ATTENTION(protocol)));
			gtk_action_set_sensitive(win->menu->alias,
									 (account != NULL) &&
									 (purple_blist_find_buddy(account, purple_conversation_get_name(conv)) != NULL));
		}
		else if (PURPLE_IS_CHAT_CONVERSATION(conv))
		{
			gtk_action_set_sensitive(win->menu->add, (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, join)));
			gtk_action_set_sensitive(win->menu->remove, (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, join)));
			gtk_action_set_sensitive(win->menu->alias,
									 (account != NULL) &&
									 (purple_blist_find_chat(account, purple_conversation_get_name(conv)) != NULL));
		}

	} else {
		/* Account is offline */
		/* Or it's a chat that we've left. */

		/* Then deal with menu items */
		gtk_action_set_sensitive(win->menu->view_log, TRUE);
		gtk_action_set_sensitive(win->menu->send_file, FALSE);
		gtk_action_set_sensitive(win->menu->get_attention, FALSE);
		gtk_action_set_sensitive(win->menu->add_pounce, TRUE);
		gtk_action_set_sensitive(win->menu->get_info, FALSE);
		gtk_action_set_sensitive(win->menu->invite, FALSE);
		gtk_action_set_sensitive(win->menu->alias, FALSE);
		gtk_action_set_sensitive(win->menu->add, FALSE);
		gtk_action_set_sensitive(win->menu->remove, FALSE);
		gtk_action_set_sensitive(win->menu->insert_link, TRUE);
		gtk_action_set_sensitive(win->menu->insert_image, FALSE);
	}

	/*
	 * Update the window's icon
	 */
	if (pidgin_conv_window_is_active_conversation(conv))
	{
		GList *l = NULL;
		if (PURPLE_IS_IM_CONVERSATION(conv) &&
				(gtkconv->u.im->anim))
		{
			PurpleBuddy *buddy = purple_blist_find_buddy(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
			window_icon =
				gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);

			if (buddy &&  !PURPLE_BUDDY_IS_ONLINE(buddy))
				gdk_pixbuf_saturate_and_pixelate(window_icon, window_icon, 0.0, FALSE);

			g_object_ref(window_icon);
			l = g_list_append(l, window_icon);
		} else {
			l = pidgin_conv_get_tab_icons(conv);
		}
		gtk_window_set_icon_list(GTK_WINDOW(win->window), l);
		if (window_icon != NULL) {
			g_object_unref(G_OBJECT(window_icon));
			g_list_free(l);
		}
	}
}

static void
pidgin_conv_update_fields(PurpleConversation *conv, PidginConvFields fields)
{
	PidginConversation *gtkconv;
	PidginConvWindow *win;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return;
	win = pidgin_conv_get_window(gtkconv);
	if (!win)
		return;

	if (fields & PIDGIN_CONV_SET_TITLE)
	{
		purple_conversation_autoset_title(conv);
	}

	if (fields & PIDGIN_CONV_BUDDY_ICON)
	{
		if (PURPLE_IS_IM_CONVERSATION(conv))
			pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));
	}

	if (fields & PIDGIN_CONV_MENU)
	{
		gray_stuff_out(PIDGIN_CONVERSATION(conv));
		generate_send_to_items(win);
		regenerate_plugins_items(win);
	}

	if (fields & PIDGIN_CONV_E2EE)
		generate_e2ee_controls(win);

	if (fields & PIDGIN_CONV_TAB_ICON)
	{
		update_tab_icon(conv);
		generate_send_to_items(win);		/* To update the icons in SendTo menu */
	}

	if ((fields & PIDGIN_CONV_TOPIC) &&
				PURPLE_IS_CHAT_CONVERSATION(conv))
	{
		const char *topic;
		PidginChatPane *gtkchat = gtkconv->u.chat;

		if (gtkchat->topic_text != NULL)
		{
			topic = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));

			gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text), topic ? topic : "");
			gtk_widget_set_tooltip_text(gtkchat->topic_text,
			                            topic ? topic : "");
		}
	}

	if ((fields & PIDGIN_CONV_COLORIZE_TITLE) ||
			(fields & PIDGIN_CONV_SET_TITLE) ||
			(fields & PIDGIN_CONV_TOPIC))
	{
		char *title;
		PurpleIMConversation *im = NULL;
		PurpleAccount *account = purple_conversation_get_account(conv);
		PurpleBuddy *buddy = NULL;
		char *markup = NULL;
		AtkObject *accessibility_obj;
		/* I think this is a little longer than it needs to be but I'm lazy. */
		char *style;

		if (PURPLE_IS_IM_CONVERSATION(conv))
			im = PURPLE_IM_CONVERSATION(conv);

		if ((account == NULL) ||
			!purple_account_is_connected(account) ||
			(PURPLE_IS_CHAT_CONVERSATION(conv)
				&& purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv))))
			title = g_strdup_printf("(%s)", purple_conversation_get_title(conv));
		else
			title = g_strdup(purple_conversation_get_title(conv));

		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
			if (buddy) {
				markup = pidgin_blist_get_name_markup(buddy, FALSE, FALSE);
			} else {
				markup = title;
			}
		} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
			const char *topic = gtkconv->u.chat->topic_text
				? gtk_entry_get_text(GTK_ENTRY(gtkconv->u.chat->topic_text))
				: NULL;
			const char *title = purple_conversation_get_title(conv);
			const char *name = purple_conversation_get_name(conv);

			char *topic_esc, *unaliased, *unaliased_esc, *title_esc;

			topic_esc = topic ? g_markup_escape_text(topic, -1) : NULL;
			unaliased = g_utf8_collate(title, name) ? g_strdup_printf("(%s)", name) : NULL;
			unaliased_esc = unaliased ? g_markup_escape_text(unaliased, -1) : NULL;
			title_esc = g_markup_escape_text(title, -1);

			markup = g_strdup_printf("%s%s<span size='smaller'>%s</span>%s<span color='%s' size='smaller'>%s</span>",
						title_esc,
						unaliased_esc ? " " : "",
						unaliased_esc ? unaliased_esc : "",
						topic_esc  && *topic_esc ? "\n" : "",
						pidgin_get_dim_grey_string(gtkconv->infopane),
						topic_esc ? topic_esc : "");

			g_free(title_esc);
			g_free(topic_esc);
			g_free(unaliased);
			g_free(unaliased_esc);
		}
		gtk_list_store_set(gtkconv->infopane_model, &(gtkconv->infopane_iter),
				CONV_TEXT_COLUMN, markup, -1);
	        /* XXX seanegan Why do I have to do this? */
		gtk_widget_queue_draw(gtkconv->infopane);

		if (title != markup)
			g_free(markup);

		if (!gtk_widget_get_realized(gtkconv->tab_label))
			gtk_widget_realize(gtkconv->tab_label);

		accessibility_obj = gtk_widget_get_accessible(gtkconv->tab_cont);
		if (im != NULL &&
		    purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPING) {
			atk_object_set_description(accessibility_obj, _("Typing"));
			style = "tab-label-typing";
		} else if (im != NULL &&
		         purple_im_conversation_get_typing_state(im) == PURPLE_IM_TYPED) {
			atk_object_set_description(accessibility_obj, _("Stopped Typing"));
			style = "tab-label-typed";
		} else if (gtkconv->unseen_state == PIDGIN_UNSEEN_NICK)	{
			atk_object_set_description(accessibility_obj, _("Nick Said"));
			style = "tab-label-attention";
		} else if (gtkconv->unseen_state == PIDGIN_UNSEEN_TEXT)	{
			atk_object_set_description(accessibility_obj, _("Unread Messages"));
			if (PURPLE_IS_CHAT_CONVERSATION(gtkconv->active_conv))
				style = "tab-label-unreadchat";
			else
				style = "tab-label-attention";
		} else if (gtkconv->unseen_state == PIDGIN_UNSEEN_EVENT) {
			atk_object_set_description(accessibility_obj, _("New Event"));
			style = "tab-label-event";
		} else {
			style = "tab-label";
		}

		gtk_widget_set_name(gtkconv->tab_label, style);
		gtk_label_set_text(GTK_LABEL(gtkconv->tab_label), title);
		gtk_widget_set_state_flags(gtkconv->tab_label, GTK_STATE_FLAG_ACTIVE, TRUE);

		if (gtkconv->unseen_state == PIDGIN_UNSEEN_TEXT ||
				gtkconv->unseen_state == PIDGIN_UNSEEN_NICK ||
				gtkconv->unseen_state == PIDGIN_UNSEEN_EVENT) {
			PangoAttrList *list = pango_attr_list_new();
			PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
			attr->start_index = 0;
			attr->end_index = -1;
			pango_attr_list_insert(list, attr);
			gtk_label_set_attributes(GTK_LABEL(gtkconv->tab_label), list);
			pango_attr_list_unref(list);
		} else
			gtk_label_set_attributes(GTK_LABEL(gtkconv->tab_label), NULL);

		if (pidgin_conv_window_is_active_conversation(conv))
			update_typing_icon(gtkconv);

		gtk_label_set_text(GTK_LABEL(gtkconv->menu_label), title);
		if (pidgin_conv_window_is_active_conversation(conv)) {
			const char* current_title = gtk_window_get_title(GTK_WINDOW(win->window));
			if (current_title == NULL || !purple_strequal(current_title, title))
				gtk_window_set_title(GTK_WINDOW(win->window), title);
		}

		g_free(title);
	}
}

static void
pidgin_conv_updated(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	PidginConvFields flags = 0;

	g_return_if_fail(conv != NULL);

	if (type == PURPLE_CONVERSATION_UPDATE_ACCOUNT)
	{
		flags = PIDGIN_CONV_ALL;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_TYPING ||
	         type == PURPLE_CONVERSATION_UPDATE_UNSEEN ||
	         type == PURPLE_CONVERSATION_UPDATE_TITLE)
	{
		flags = PIDGIN_CONV_COLORIZE_TITLE;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_TOPIC)
	{
		flags = PIDGIN_CONV_TOPIC;
	}
	else if (type == PURPLE_CONVERSATION_ACCOUNT_ONLINE ||
	         type == PURPLE_CONVERSATION_ACCOUNT_OFFLINE)
	{
		flags = PIDGIN_CONV_MENU | PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_SET_TITLE;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_AWAY)
	{
		flags = PIDGIN_CONV_TAB_ICON;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_ADD ||
	         type == PURPLE_CONVERSATION_UPDATE_REMOVE ||
	         type == PURPLE_CONVERSATION_UPDATE_CHATLEFT)
	{
		flags = PIDGIN_CONV_SET_TITLE | PIDGIN_CONV_MENU;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_ICON)
	{
		flags = PIDGIN_CONV_BUDDY_ICON;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_FEATURES)
	{
		flags = PIDGIN_CONV_MENU;
	}
	else if (type == PURPLE_CONVERSATION_UPDATE_E2EE)
	{
		flags = PIDGIN_CONV_E2EE | PIDGIN_CONV_MENU;
	}

	pidgin_conv_update_fields(conv, flags);
}

static void
wrote_msg_update_unseen_cb(PurpleConversation *conv, PurpleMessage *msg,
	gpointer _unused)
{
	PidginConversation *gtkconv = conv ? PIDGIN_CONVERSATION(conv) : NULL;
	PurpleMessageFlags flags;
	if (conv == NULL || (gtkconv && gtkconv->win != hidden_convwin))
		return;
	flags = purple_message_get_flags(msg);
	if (flags & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)) {
		PidginUnseenState unseen = PIDGIN_UNSEEN_NONE;

		if ((flags & PURPLE_MESSAGE_NICK) == PURPLE_MESSAGE_NICK)
			unseen = PIDGIN_UNSEEN_NICK;
		else if (((flags & PURPLE_MESSAGE_SYSTEM) == PURPLE_MESSAGE_SYSTEM) ||
			  ((flags & PURPLE_MESSAGE_ERROR) == PURPLE_MESSAGE_ERROR))
			unseen = PIDGIN_UNSEEN_EVENT;
		else if ((flags & PURPLE_MESSAGE_NO_LOG) == PURPLE_MESSAGE_NO_LOG)
			unseen = PIDGIN_UNSEEN_NO_LOG;
		else
			unseen = PIDGIN_UNSEEN_TEXT;

		conv_set_unseen(conv, unseen);
	}
}

static PurpleConversationUiOps conversation_ui_ops =
{
	pidgin_conv_new,
	pidgin_conv_destroy,              /* destroy_conversation */
	NULL,                              /* write_chat           */
	NULL,                             /* write_im             */
	pidgin_conv_write_conv,           /* write_conv           */
	pidgin_conv_chat_add_users,       /* chat_add_users       */
	pidgin_conv_chat_rename_user,     /* chat_rename_user     */
	pidgin_conv_chat_remove_users,    /* chat_remove_users    */
	pidgin_conv_chat_update_user,     /* chat_update_user     */
	pidgin_conv_present_conversation, /* present              */
	pidgin_conv_has_focus,            /* has_focus            */
	NULL,                             /* send_confirm         */
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleConversationUiOps *
pidgin_conversations_get_conv_ui_ops(void)
{
	return &conversation_ui_ops;
}

/**************************************************************************
 * Public conversation utility functions
 **************************************************************************/
void
pidgin_conv_update_buddy_icon(PurpleIMConversation *im)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PidginConvWindow *win;

	PurpleBuddy *buddy;

	PurpleImage *custom_img = NULL;
	gconstpointer data = NULL;
	size_t len;

	GdkPixbuf *buf;

	GList *children;
	GtkWidget *event;
	GdkPixbuf *scale;
	int scale_width, scale_height;
	int size = 0;

	PurpleAccount *account;

	PurpleBuddyIcon *icon;

	conv = PURPLE_CONVERSATION(im);

	g_return_if_fail(conv != NULL);
	g_return_if_fail(PIDGIN_IS_PIDGIN_CONVERSATION(conv));

	gtkconv = PIDGIN_CONVERSATION(conv);
	win = gtkconv->win;
	if (conv != gtkconv->active_conv)
		return;

	if (!gtkconv->u.im->show_icon)
		return;

	account = purple_conversation_get_account(conv);

	/* Remove the current icon stuff */
	children = gtk_container_get_children(GTK_CONTAINER(gtkconv->u.im->icon_container));
	if (children) {
		/* We know there's only one child here. It'd be nice to shortcut to the
		   event box, but we can't change the PidginConversation until 3.0 */
		event = (GtkWidget *)children->data;
		gtk_container_remove(GTK_CONTAINER(gtkconv->u.im->icon_container), event);
		g_list_free(children);
	}

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	gtkconv->u.im->anim = NULL;

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->iter = NULL;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		return;

	if (purple_conversation_get_connection(conv) == NULL)
		return;

	buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
	if (buddy)
	{
		PurpleContact *contact = purple_buddy_get_contact(buddy);
		if (contact) {
			custom_img = purple_buddy_icons_node_find_custom_icon((PurpleBlistNode*)contact);
			if (custom_img) {
				/* There is a custom icon for this user */
				data = purple_image_get_data(custom_img);
				len = purple_image_get_data_size(custom_img);
			}
		}
	}

	if (data == NULL) {
		icon = purple_im_conversation_get_icon(im);
		if (icon == NULL)
		{
			gtk_widget_set_size_request(gtkconv->u.im->icon_container,
			                            -1, BUDDYICON_SIZE_MIN);
			return;
		}

		data = purple_buddy_icon_get_data(icon, &len);
		if (data == NULL)
		{
			gtk_widget_set_size_request(gtkconv->u.im->icon_container,
			                            -1, BUDDYICON_SIZE_MIN);
			return;
		}
	}

	gtkconv->u.im->anim = pidgin_pixbuf_anim_from_data(data, len);
	if (custom_img)
		g_object_unref(custom_img);

	if (!gtkconv->u.im->anim) {
		purple_debug_error("gtkconv", "Couldn't load icon for conv %s\n",
				purple_conversation_get_name(conv));
		return;
	}

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)) {
		GdkPixbuf *stat;
		gtkconv->u.im->iter = NULL;
		stat = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
		buf = gdk_pixbuf_add_alpha(stat, FALSE, 0, 0, 0);
	} else {
		GdkPixbuf *stat;
		gtkconv->u.im->iter =
			gdk_pixbuf_animation_get_iter(gtkconv->u.im->anim, NULL); /* LEAK */
		stat = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);
		buf = gdk_pixbuf_add_alpha(stat, FALSE, 0, 0, 0);
		if (gtkconv->u.im->animate)
			start_anim(NULL, gtkconv);
	}

	scale_width = gdk_pixbuf_get_width(buf);
	scale_height = gdk_pixbuf_get_height(buf);

	gtk_widget_get_size_request(gtkconv->u.im->icon_container, NULL, &size);
	size = MIN(size, MIN(scale_width, scale_height));

	/* Some sanity checks */
	size = CLAMP(size, BUDDYICON_SIZE_MIN, BUDDYICON_SIZE_MAX);
	if (scale_width == scale_height) {
		scale_width = scale_height = size;
	} else if (scale_height > scale_width) {
		scale_width = size * scale_width / scale_height;
		scale_height = size;
	} else {
		scale_height = size * scale_height / scale_width;
		scale_width = size;
	}
	scale = gdk_pixbuf_scale_simple(buf, scale_width, scale_height,
				GDK_INTERP_BILINEAR);
	g_object_unref(buf);
	if (pidgin_gdk_pixbuf_is_opaque(scale))
		pidgin_gdk_pixbuf_make_round(scale);

	event = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(gtkconv->u.im->icon_container), event);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(event), FALSE);
	gtk_widget_add_events(event,
	                      GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(event), "button-press-event",
					 G_CALLBACK(icon_menu), gtkconv);

	pidgin_tooltip_setup_for_widget(event, gtkconv, pidgin_conv_create_tooltip, NULL);
	gtk_widget_show(event);

	gtkconv->u.im->icon = gtk_image_new_from_pixbuf(scale);
	gtk_container_add(GTK_CONTAINER(event), gtkconv->u.im->icon);
	gtk_widget_show(gtkconv->u.im->icon);

	g_object_unref(G_OBJECT(scale));

	/* The buddy icon code needs badly to be fixed. */
	if(pidgin_conv_window_is_active_conversation(conv))
	{
		buf = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
		if (buddy && !PURPLE_BUDDY_IS_ONLINE(buddy))
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);
		gtk_window_set_icon(GTK_WINDOW(win->window), buf);
	}
}

void
pidgin_conv_update_buttons_by_protocol(PurpleConversation *conv)
{
	PidginConvWindow *win;

	if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		return;

	win = PIDGIN_CONVERSATION(conv)->win;

	if (win != NULL && pidgin_conv_window_is_active_conversation(conv))
		gray_stuff_out(PIDGIN_CONVERSATION(conv));
}

static gboolean
pidgin_conv_xy_to_right_infopane(PidginConvWindow *win, int x, int y)
{
	gint pane_x, pane_y, x_rel;
	PidginConversation *gtkconv;
	GtkAllocation allocation;

	gdk_window_get_origin(gtk_widget_get_window(win->notebook),
	                      &pane_x, &pane_y);
	x_rel = x - pane_x;
	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	gtk_widget_get_allocation(gtkconv->infopane, &allocation);
	return (x_rel > allocation.x + allocation.width / 2);
}

int
pidgin_conv_get_tab_at_xy(PidginConvWindow *win, int x, int y, gboolean *to_right)
{
	gint nb_x, nb_y, x_rel, y_rel;
	GtkNotebook *notebook;
	GtkWidget *page, *tab;
	gint i, page_num = -1;
	gint count;
	gboolean horiz;

	if (to_right)
		*to_right = FALSE;

	notebook = GTK_NOTEBOOK(win->notebook);

	gdk_window_get_origin(gtk_widget_get_window(win->notebook), &nb_x, &nb_y);
	x_rel = x - nb_x;
	y_rel = y - nb_y;

	horiz = (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP ||
			gtk_notebook_get_tab_pos(notebook) == GTK_POS_BOTTOM);

	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

	for (i = 0; i < count; i++) {
		GtkAllocation allocation;

		page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
		tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), page);
		gtk_widget_get_allocation(tab, &allocation);

		/* Make sure the tab is not hidden beyond an arrow */
		if (!gtk_widget_is_drawable(tab) && gtk_notebook_get_show_tabs(notebook))
			continue;

		if (horiz) {
			if (x_rel >= allocation.x - PIDGIN_HIG_BOX_SPACE &&
					x_rel <= allocation.x + allocation.width + PIDGIN_HIG_BOX_SPACE) {
				page_num = i;

				if (to_right && x_rel >= allocation.x + allocation.width/2)
					*to_right = TRUE;

				break;
			}
		} else {
			if (y_rel >= allocation.y - PIDGIN_HIG_BOX_SPACE &&
					y_rel <= allocation.y + allocation.height + PIDGIN_HIG_BOX_SPACE) {
				page_num = i;

				if (to_right && y_rel >= allocation.y + allocation.height/2)
					*to_right = TRUE;

				break;
			}
		}
	}

	if (page_num == -1) {
		/* Add after the last tab */
		page_num = count - 1;
	}

	return page_num;
}

static void
close_on_tabs_pref_cb(const char *name, PurplePrefType type,
					  gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

		if (value)
			gtk_widget_show(gtkconv->close);
		else
			gtk_widget_hide(gtkconv->close);
	}
}

static void
spellcheck_pref_cb(const char *name, PurplePrefType type,
				   gconstpointer value, gpointer data)
{
	GList *cl;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	for (cl = purple_conversations_get_all(); cl != NULL; cl = cl->next) {

		conv = (PurpleConversation *)cl->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

# warning toggle spell checking when talkatu #60 is done.
	}
}

static void
tab_side_pref_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GList *gtkwins, *gtkconvs;
	GtkPositionType pos;
	PidginConvWindow *gtkwin;

	pos = GPOINTER_TO_INT(value);

	for (gtkwins = pidgin_conv_windows_get_list(); gtkwins != NULL; gtkwins = gtkwins->next) {
		gtkwin = gtkwins->data;
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(gtkwin->notebook), pos&~8);
		for (gtkconvs = gtkwin->gtkconvs; gtkconvs != NULL; gtkconvs = gtkconvs->next) {
			pidgin_conv_tab_pack(gtkwin, gtkconvs->data);
		}
	}
}

static void
show_formatting_toolbar_pref_cb(const char *name, PurplePrefType type,
								gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginConvWindow *win;
	gboolean visible = (gboolean)GPOINTER_TO_INT(value);

	for (l = purple_conversations_get_all(); l != NULL; l = l->next)
	{
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);
		win     = gtkconv->win;

		gtk_toggle_action_set_active(
		        GTK_TOGGLE_ACTION(win->menu->show_formatting_toolbar),
		        visible
		);

		talkatu_editor_set_toolbar_visible(TALKATU_EDITOR(gtkconv->editor), visible);
	}
}

static void
animate_buddy_icons_pref_cb(const char *name, PurplePrefType type,
							gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginConvWindow *win;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		return;

	/* Set the "animate" flag for each icon based on the new preference */
	for (l = purple_conversations_get_ims(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;
		gtkconv = PIDGIN_CONVERSATION(conv);
		if (gtkconv)
			gtkconv->u.im->animate = GPOINTER_TO_INT(value);
	}

	/* Now either stop or start animation for the active conversation in each window */
	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;
		conv = pidgin_conv_window_get_active_conversation(win);
		pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));
	}
}

static void
show_buddy_icons_pref_cb(const char *name, PurplePrefType type,
						 gconstpointer value, gpointer data)
{
	GList *l;

	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		PurpleConversation *conv = l->data;
		if (!PIDGIN_CONVERSATION(conv))
			continue;
		if (GPOINTER_TO_INT(value))
			gtk_widget_show(PIDGIN_CONVERSATION(conv)->infopane_hbox);
		else
			gtk_widget_hide(PIDGIN_CONVERSATION(conv)->infopane_hbox);

		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));
		}
	}

	/* Make the tabs show/hide correctly */
	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		PidginConvWindow *win = l->data;
		if (pidgin_conv_window_get_gtkconv_count(win) == 1)
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
						   GPOINTER_TO_INT(value) == 0);
	}
}

static void
show_protocol_icons_pref_cb(const char *name, PurplePrefType type,
						gconstpointer value, gpointer data)
{
	GList *l;
	for (l = purple_conversations_get_all(); l != NULL; l = l->next) {
		PurpleConversation *conv = l->data;
		if (PIDGIN_CONVERSATION(conv))
			update_tab_icon(conv);
	}
}

static void
conv_placement_usetabs_cb(const char *name, PurplePrefType type,
						  gconstpointer value, gpointer data)
{
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");
}

static void
account_status_changed_cb(PurpleAccount *account, PurpleStatus *oldstatus,
                          PurpleStatus *newstatus)
{
	GList *l;
	PurpleConversation *conv = NULL;
	PidginConversation *gtkconv;

	if(!purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away"))
		return;

	if(purple_status_is_available(oldstatus) || !purple_status_is_available(newstatus))
		return;

	for (l = hidden_convwin->gtkconvs; l; ) {
		gtkconv = l->data;
		l = l->next;

		conv = gtkconv->active_conv;
		if (PURPLE_IS_CHAT_CONVERSATION(conv) ||
				account != purple_conversation_get_account(conv))
			continue;

		pidgin_conv_attach_to_conversation(conv);

		/* TODO: do we need to do anything for any other conversations that are in the same gtkconv here?
		 * I'm a little concerned that not doing so will cause the "pending" indicator in the gtkblist not to be cleared. -DAA*/
		purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
	}
}

static void
hide_new_pref_cb(const char *name, PurplePrefType type,
				 gconstpointer value, gpointer data)
{
	GList *l;
	PurpleConversation *conv = NULL;
	PidginConversation *gtkconv;
	gboolean when_away = FALSE;

	if(!hidden_convwin)
		return;

	if(purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "always"))
		return;

	if(purple_strequal(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new"), "away"))
		when_away = TRUE;

	for (l = hidden_convwin->gtkconvs; l; )
	{
		gtkconv = l->data;
		l = l->next;

		conv = gtkconv->active_conv;

		if (PURPLE_IS_CHAT_CONVERSATION(conv) ||
				gtkconv->unseen_count == 0 ||
				(when_away && !purple_status_is_available(
							purple_account_get_active_status(
							purple_conversation_get_account(conv)))))
			continue;

		pidgin_conv_attach_to_conversation(conv);
	}
}


static void
conv_placement_pref_cb(const char *name, PurplePrefType type,
					   gconstpointer value, gpointer data)
{
	PidginConvPlacementFunc func;

	if (!purple_strequal(name, PIDGIN_PREFS_ROOT "/conversations/placement"))
		return;

	func = pidgin_conv_placement_get_fnc(value);

	if (func == NULL)
		return;

	pidgin_conv_placement_set_current_func(func);
}

static PidginConversation *
get_gtkconv_with_contact(PurpleContact *contact)
{
	PurpleBlistNode *node;

	node = ((PurpleBlistNode*)contact)->child;

	for (; node; node = node->next)
	{
		PurpleBuddy *buddy = (PurpleBuddy*)node;
		PurpleIMConversation *im;
		im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
		if (im)
			return PIDGIN_CONVERSATION(PURPLE_CONVERSATION(im));
	}
	return NULL;
}

static void
account_signed_off_cb(PurpleConnection *gc, gpointer event)
{
	GList *iter;

	for (iter = purple_conversations_get_all(); iter; iter = iter->next)
	{
		PurpleConversation *conv = iter->data;

		/* This seems fine in theory, but we also need to cover the
		 * case of this account matching one of the other buddies in
		 * one of the contacts containing the buddy corresponding to
		 * a conversation.  It's easier to just update them all. */
		/* if (purple_conversation_get_account(conv) == account) */
			pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON |
							PIDGIN_CONV_MENU | PIDGIN_CONV_COLORIZE_TITLE);

		if (PURPLE_CONNECTION_IS_CONNECTED(gc) &&
				PURPLE_IS_CHAT_CONVERSATION(conv) &&
				purple_conversation_get_account(conv) == purple_connection_get_account(gc) &&
				g_object_get_data(G_OBJECT(conv), "want-to-rejoin")) {
			GHashTable *comps = NULL;
			PurpleChat *chat = purple_blist_find_chat(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
			if (chat == NULL) {
				PurpleProtocol *protocol = purple_connection_get_protocol(gc);
				comps = purple_protocol_chat_iface_info_defaults(protocol, gc, purple_conversation_get_name(conv));
			} else {
				comps = purple_chat_get_components(chat);
			}
			purple_serv_join_chat(gc, comps);
			if (chat == NULL && comps != NULL)
				g_hash_table_destroy(comps);
		}
	}
}

static void
account_signing_off(PurpleConnection *gc)
{
	GList *list = purple_conversations_get_chats();
	PurpleAccount *account = purple_connection_get_account(gc);

	/* We are about to sign off. See which chats we are currently in, and mark
	 * them for rejoin on reconnect. */
	while (list) {
		PurpleConversation *conv = list->data;
		if (!purple_chat_conversation_has_left(PURPLE_CHAT_CONVERSATION(conv)) &&
				purple_conversation_get_account(conv) == account) {
			g_object_set_data(G_OBJECT(conv), "want-to-rejoin", GINT_TO_POINTER(TRUE));
			purple_conversation_write_system_message(conv,
				_("The account has disconnected and you are no "
				"longer in this chat. You will automatically "
				"rejoin the chat when the account reconnects."),
				PURPLE_MESSAGE_NO_LOG);
		}
		list = list->next;
	}
}

static void
update_buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old, PurpleStatus *newstatus)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;

	gtkconv = get_gtkconv_with_contact(purple_buddy_get_contact(buddy));
	if (gtkconv)
	{
		conv = gtkconv->active_conv;
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON
		                              | PIDGIN_CONV_COLORIZE_TITLE
		                              | PIDGIN_CONV_BUDDY_ICON);
		if ((purple_status_is_online(old) ^ purple_status_is_online(newstatus)) != 0)
			pidgin_conv_update_fields(conv, PIDGIN_CONV_MENU);
	}
}

static void
update_buddy_privacy_changed(PurpleBuddy *buddy)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;

	gtkconv = get_gtkconv_with_contact(purple_buddy_get_contact(buddy));
	if (gtkconv) {
		conv = gtkconv->active_conv;
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON | PIDGIN_CONV_MENU);
	}
}

static void
update_buddy_idle_changed(PurpleBuddy *buddy, gboolean old, gboolean newidle)
{
	PurpleIMConversation *im;

	im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
	if (im)
		pidgin_conv_update_fields(PURPLE_CONVERSATION(im), PIDGIN_CONV_TAB_ICON);
}

static void
update_buddy_icon(PurpleBuddy *buddy)
{
	PurpleIMConversation *im;

	im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
	if (im)
		pidgin_conv_update_fields(PURPLE_CONVERSATION(im), PIDGIN_CONV_BUDDY_ICON);
}

static void
update_buddy_sign(PurpleBuddy *buddy, const char *which)
{
	PurplePresence *presence;
	PurpleStatus *on, *off;

	presence = purple_buddy_get_presence(buddy);
	if (!presence)
		return;
	off = purple_presence_get_status(presence, "offline");
	on = purple_presence_get_status(presence, "available");

	if (*(which+1) == 'f')
		update_buddy_status_changed(buddy, on, off);
	else
		update_buddy_status_changed(buddy, off, on);
}

static void
update_conversation_switched(PurpleConversation *conv)
{
	pidgin_conv_update_fields(conv, PIDGIN_CONV_TAB_ICON |
		PIDGIN_CONV_SET_TITLE | PIDGIN_CONV_MENU |
		PIDGIN_CONV_BUDDY_ICON | PIDGIN_CONV_E2EE );
}

static void
update_buddy_typing(PurpleAccount *account, const char *who)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(who, account));
	if (!conv)
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (gtkconv && gtkconv->active_conv == conv)
		pidgin_conv_update_fields(conv, PIDGIN_CONV_COLORIZE_TITLE);
}

static void
update_chat(PurpleChatConversation *chat)
{
	pidgin_conv_update_fields(PURPLE_CONVERSATION(chat), PIDGIN_CONV_TOPIC |
					PIDGIN_CONV_MENU | PIDGIN_CONV_SET_TITLE);
}

static void
update_chat_topic(PurpleChatConversation *chat, const char *old, const char *new)
{
	pidgin_conv_update_fields(PURPLE_CONVERSATION(chat), PIDGIN_CONV_TOPIC);
}

/* Message history stuff */

/* Compare two PurpleMessages, according to time in ascending order. */
static int
message_compare(PurpleMessage *m1, PurpleMessage *m2)
{
	guint64 t1 = purple_message_get_time(m1), t2 = purple_message_get_time(m2);
	return (t1 > t2) - (t1 < t2);
}

/* Adds some message history to the gtkconv. This happens in a idle-callback. */
static gboolean
add_message_history_to_gtkconv(gpointer data)
{
	PidginConversation *gtkconv = data;
	int count = 0;
	int timer = gtkconv->attach_timer;
	time_t when = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(gtkconv->editor), "attach-start-time"));
	gboolean im = (PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv));

	gtkconv->attach_timer = 0;
	while (gtkconv->attach_current && count < ADD_MESSAGE_HISTORY_AT_ONCE) {
		PurpleMessage *msg = gtkconv->attach_current->data;
		if (!im && when && (guint64)when < purple_message_get_time(msg)) {
			g_object_set_data(G_OBJECT(gtkconv->editor), "attach-start-time", NULL);
		}
		/* XXX: should it be gtkconv->active_conv? */
		pidgin_conv_write_conv(gtkconv->active_conv, msg);
		if (im) {
			gtkconv->attach_current = g_list_delete_link(gtkconv->attach_current, gtkconv->attach_current);
		} else {
			gtkconv->attach_current = gtkconv->attach_current->prev;
		}
		count++;
	}
	gtkconv->attach_timer = timer;
	if (gtkconv->attach_current)
		return TRUE;

	g_source_remove(gtkconv->attach_timer);
	gtkconv->attach_timer = 0;
	if (im) {
		/* Print any message that was sent while the old history was being added back. */
		GList *msgs = NULL;
		GList *iter = gtkconv->convs;
		for (; iter; iter = iter->next) {
			PurpleConversation *conv = iter->data;
			GList *history = purple_conversation_get_message_history(conv);
			for (; history; history = history->next) {
				PurpleMessage *msg = history->data;
				if (purple_message_get_time(msg) > (guint64)when)
					msgs = g_list_prepend(msgs, msg);
			}
		}
		msgs = g_list_sort(msgs, (GCompareFunc)message_compare);
		for (; msgs; msgs = g_list_delete_link(msgs, msgs)) {
			PurpleMessage *msg = msgs->data;
			/* XXX: see above - should it be active_conv? */
			pidgin_conv_write_conv(gtkconv->active_conv, msg);
		}
		g_object_set_data(G_OBJECT(gtkconv->editor), "attach-start-time", NULL);
	}

	g_object_set_data(G_OBJECT(gtkconv->editor), "attach-start-time", NULL);
	purple_signal_emit(pidgin_conversations_get_handle(),
			"conversation-displayed", gtkconv);
	return FALSE;
}

static void
pidgin_conv_attach(PurpleConversation *conv)
{
	int timer;
	g_object_set_data(G_OBJECT(conv), "unseen-count", NULL);
	g_object_set_data(G_OBJECT(conv), "unseen-state", NULL);
	purple_conversation_set_ui_ops(conv, pidgin_conversations_get_conv_ui_ops());
	if (!PIDGIN_CONVERSATION(conv))
		private_gtkconv_new(conv, FALSE);
	timer = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "close-timer"));
	if (timer) {
		g_source_remove(timer);
		g_object_set_data(G_OBJECT(conv), "close-timer", NULL);
	}
}

gboolean pidgin_conv_attach_to_conversation(PurpleConversation *conv)
{
	GList *list;
	PidginConversation *gtkconv;

	if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
		/* This is pretty much always the case now. */
		gtkconv = PIDGIN_CONVERSATION(conv);
		if (gtkconv->win != hidden_convwin)
			return FALSE;
		pidgin_conv_window_remove_gtkconv(hidden_convwin, gtkconv);
		pidgin_conv_placement_place(gtkconv);
		purple_signal_emit(pidgin_conversations_get_handle(),
				"conversation-displayed", gtkconv);
		list = gtkconv->convs;
		while (list) {
			pidgin_conv_attach(list->data);
			list = list->next;
		}
		return TRUE;
	}

	pidgin_conv_attach(conv);
	gtkconv = PIDGIN_CONVERSATION(conv);

	list = purple_conversation_get_message_history(conv);
	if (list) {
		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			GList *convs;
			list = g_list_copy(list);
			for (convs = purple_conversations_get_ims(); convs; convs = convs->next)
				if (convs->data != conv &&
						pidgin_conv_find_gtkconv(convs->data) == gtkconv) {
					pidgin_conv_attach(convs->data);
					list = g_list_concat(list, g_list_copy(purple_conversation_get_message_history(convs->data)));
				}
			list = g_list_sort(list, (GCompareFunc)message_compare);
			gtkconv->attach_current = list;
			list = g_list_last(list);
		} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
			gtkconv->attach_current = g_list_last(list);
		}

		g_object_set_data(G_OBJECT(gtkconv->editor), "attach-start-time",
			GINT_TO_POINTER(purple_message_get_time(list->data)));
		gtkconv->attach_timer = g_idle_add(add_message_history_to_gtkconv, gtkconv);
	} else {
		purple_signal_emit(pidgin_conversations_get_handle(),
				"conversation-displayed", gtkconv);
	}

	if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		GList *users;
		PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(conv);
		pidgin_conv_update_fields(conv, PIDGIN_CONV_TOPIC);
		users = purple_chat_conversation_get_users(chat);
		pidgin_conv_chat_add_users(chat, users, TRUE);
		g_list_free(users);
	}

	return TRUE;
}

void *
pidgin_conversations_get_handle(void)
{
	static int handle;

	return &handle;
}

static void
pidgin_conversations_pre_uninit(void);

void
pidgin_conversations_init(void)
{
	void *handle = pidgin_conversations_get_handle();
	void *blist_handle = purple_blist_get_handle();

	e2ee_stock = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, g_object_unref);

	/* Conversations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/close_on_tabs", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/spellcheck", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting", TRUE);
	/* TODO: it's about *remote* smileys, not local ones */
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/custom_smileys_size", 96);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines", 2);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar", TRUE);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/placement", "last");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/placement_number", 1);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/font_size", 3);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/tabs", TRUE);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/tab_side", GTK_POS_TOP);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/scrollback_lines", 4000);

#ifdef _WIN32
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font", TRUE);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/custom_font", "");
#endif

	/* Conversations -> Chat */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/chat");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/entry_height", 54);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/userlist_width", 80);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/x", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/y", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/width", 340);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/chat/height", 390);

	/* Conversations -> IM */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/im");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/x", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/y", 0);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/width", 340);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/height", 390);

	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons", TRUE);

	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/im/entry_height", 54);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons", TRUE);

	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "never");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/im/close_immediately", TRUE);

#ifdef _WIN32
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/win32/minimize_new_convs", FALSE);
#endif

	/* Connect callbacks. */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/close_on_tabs",
								close_on_tabs_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar",
								show_formatting_toolbar_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/spellcheck",
								spellcheck_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/tab_side",
								tab_side_pref_cb, NULL);

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/tabs",
								conv_placement_usetabs_cb, NULL);

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/placement",
								conv_placement_pref_cb, NULL);
	purple_prefs_trigger_callback(PIDGIN_PREFS_ROOT "/conversations/placement");

	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines",
		minimum_entry_lines_pref_cb, NULL);

	/* IM callbacks */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons",
								animate_buddy_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons",
								show_buddy_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_protocol_icons",
								show_protocol_icons_pref_cb, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
								hide_new_pref_cb, NULL);

	/**********************************************************************
	 * Register signals
	 **********************************************************************/
	purple_signal_register(handle, "conversation-dragging",
	                     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
	                     G_TYPE_POINTER, /* pointer to a (PidginConvWindow *) */
	                     G_TYPE_POINTER); /* pointer to a (PidginConvWindow *) */

	purple_signal_register(handle, "conversation-timestamp",
#if SIZEOF_TIME_T == 4
	                     purple_marshal_POINTER__POINTER_INT_BOOLEAN,
#elif SIZEOF_TIME_T == 8
			     purple_marshal_POINTER__POINTER_INT64_BOOLEAN,
#else
#error Unkown size of time_t
#endif
	                     G_TYPE_STRING, 3, PURPLE_TYPE_CONVERSATION,
#if SIZEOF_TIME_T == 4
	                     G_TYPE_INT,
#elif SIZEOF_TIME_T == 8
	                     G_TYPE_INT64,
#else
# error Unknown size of time_t
#endif
	                     G_TYPE_BOOLEAN);

	purple_signal_register(handle, "displaying-im-msg",
		purple_marshal_BOOLEAN__POINTER_POINTER,
		G_TYPE_BOOLEAN, 2, PURPLE_TYPE_CONVERSATION, PURPLE_TYPE_MESSAGE);

	purple_signal_register(handle, "displayed-im-msg",
		purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
		PURPLE_TYPE_CONVERSATION, PURPLE_TYPE_MESSAGE);

	purple_signal_register(handle, "displaying-chat-msg",
		purple_marshal_BOOLEAN__POINTER_POINTER,
		G_TYPE_BOOLEAN, 2, PURPLE_TYPE_CONVERSATION, PURPLE_TYPE_MESSAGE);

	purple_signal_register(handle, "displayed-chat-msg",
		purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
		PURPLE_TYPE_CONVERSATION, PURPLE_TYPE_MESSAGE);

	purple_signal_register(handle, "conversation-switched",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "conversation-hiding",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 G_TYPE_POINTER); /* (PidginConversation *) */

	purple_signal_register(handle, "conversation-displayed",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 G_TYPE_POINTER); /* (PidginConversation *) */

	purple_signal_register(handle, "chat-nick-autocomplete",
						 purple_marshal_BOOLEAN__POINTER_BOOLEAN,
						 G_TYPE_BOOLEAN, 1, PURPLE_TYPE_CONVERSATION);

	purple_signal_register(handle, "chat-nick-clicked",
						 purple_marshal_BOOLEAN__POINTER_POINTER_UINT,
						 G_TYPE_BOOLEAN, 3, PURPLE_TYPE_CONVERSATION,
						 G_TYPE_STRING, G_TYPE_UINT);

	purple_signal_register(handle, "conversation-window-created",
		purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
		G_TYPE_POINTER); /* (PidginConvWindow *) */


	/**********************************************************************
	 * Register commands
	 **********************************************************************/
	purple_cmd_register("say", "S", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  say_command_cb, _("say &lt;message&gt;:  Send a message normally as if you weren't using a command."), NULL);
	purple_cmd_register("me", "S", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  me_command_cb, _("me &lt;action&gt;:  Send an IRC style action to a buddy or chat."), NULL);
	purple_cmd_register("debug", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  debug_command_cb, _("debug &lt;option&gt;:  Send various debug information to the current conversation."), NULL);
	purple_cmd_register("clear", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  clear_command_cb, _("clear: Clears the conversation scrollback."), NULL);
	purple_cmd_register("clearall", "", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM, NULL,
	                  clearall_command_cb, _("clear: Clears all conversation scrollbacks."), NULL);
	purple_cmd_register("help", "w", PURPLE_CMD_P_DEFAULT,
	                  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, NULL,
	                  help_command_cb, _("help &lt;command&gt;:  Help on a specific command."), NULL);

	/**********************************************************************
	 * UI operations
	 **********************************************************************/

	purple_signal_connect(purple_connections_get_handle(), "signed-on", handle,
						G_CALLBACK(account_signed_off_cb),
						GINT_TO_POINTER(PURPLE_CONVERSATION_ACCOUNT_ONLINE));
	purple_signal_connect(purple_connections_get_handle(), "signed-off", handle,
						G_CALLBACK(account_signed_off_cb),
						GINT_TO_POINTER(PURPLE_CONVERSATION_ACCOUNT_OFFLINE));
	purple_signal_connect(purple_connections_get_handle(), "signing-off", handle,
						G_CALLBACK(account_signing_off), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "writing-im-msg",
		handle, G_CALLBACK(writing_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "writing-chat-msg",
		handle, G_CALLBACK(writing_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg",
						handle, G_CALLBACK(received_im_msg_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "cleared-message-history",
	                      handle, G_CALLBACK(clear_conversation_scrollback_cb), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "deleting-chat-user",
	                      handle, G_CALLBACK(deleting_chat_user_cb), NULL);

	purple_conversations_set_ui_ops(&conversation_ui_ops);

	hidden_convwin = pidgin_conv_window_new();
	window_list = g_list_remove(window_list, hidden_convwin);

	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed",
						handle, PURPLE_CALLBACK(account_status_changed_cb), NULL);

	purple_signal_connect_priority(purple_get_core(), "quitting", handle,
		PURPLE_CALLBACK(pidgin_conversations_pre_uninit), NULL, PURPLE_SIGNAL_PRIORITY_HIGHEST);

	/* Callbacks to update a conversation */
	purple_signal_connect(blist_handle, "blist-node-added", handle,
						G_CALLBACK(buddy_update_cb), NULL);
	purple_signal_connect(blist_handle, "blist-node-removed", handle,
						G_CALLBACK(buddy_update_cb), NULL);
	purple_signal_connect(blist_handle, "buddy-signed-on",
						handle, PURPLE_CALLBACK(update_buddy_sign), "on");
	purple_signal_connect(blist_handle, "buddy-signed-off",
						handle, PURPLE_CALLBACK(update_buddy_sign), "off");
	purple_signal_connect(blist_handle, "buddy-status-changed",
						handle, PURPLE_CALLBACK(update_buddy_status_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-privacy-changed",
						handle, PURPLE_CALLBACK(update_buddy_privacy_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-idle-changed",
						handle, PURPLE_CALLBACK(update_buddy_idle_changed), NULL);
	purple_signal_connect(blist_handle, "buddy-icon-changed",
						handle, PURPLE_CALLBACK(update_buddy_icon), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing",
						handle, PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "buddy-typing-stopped",
						handle, PURPLE_CALLBACK(update_buddy_typing), NULL);
	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-switched",
						handle, PURPLE_CALLBACK(update_conversation_switched), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-left", handle,
						PURPLE_CALLBACK(update_chat), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-joined", handle,
						PURPLE_CALLBACK(update_chat), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-topic-changed", handle,
						PURPLE_CALLBACK(update_chat_topic), NULL);
	purple_signal_connect_priority(purple_conversations_get_handle(), "conversation-updated", handle,
						PURPLE_CALLBACK(pidgin_conv_updated), NULL,
						PURPLE_SIGNAL_PRIORITY_LOWEST);
	purple_signal_connect(purple_conversations_get_handle(), "wrote-im-msg", handle,
			PURPLE_CALLBACK(wrote_msg_update_unseen_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "wrote-chat-msg", handle,
			PURPLE_CALLBACK(wrote_msg_update_unseen_cb), NULL);
}

static void
pidgin_conversations_pre_uninit(void)
{
	g_hash_table_destroy(e2ee_stock);
	e2ee_stock = NULL;
}

/* Invalidate the first tab color set */
static gboolean tab_color_fuse = TRUE;

static void
pidgin_conversations_set_tab_colors(void)
{
	/* Set default tab colors */
	GString *str;
	GtkSettings *settings;
	GtkStyle *parent, *now;
	struct {
		const char *stylename;
		const char *labelname;
		const char *color;
	} styles[] = {
		{"pidgin_tab_label_typing_default", "tab-label-typing", "#4e9a06"},
		{"pidgin_tab_label_typed_default", "tab-label-typed", "#c4a000"},
		{"pidgin_tab_label_attention_default", "tab-label-attention", "#006aff"},
		{"pidgin_tab_label_unreadchat_default", "tab-label-unreadchat", "#cc0000"},
		{"pidgin_tab_label_event_default", "tab-label-event", "#888a85"},
		{NULL, NULL, NULL}
	};
	int iter;

	if(tab_color_fuse) {
		tab_color_fuse = FALSE;
		return;
	}

	str = g_string_new(NULL);
	settings = gtk_settings_get_default();
	parent = gtk_rc_get_style_by_paths(settings, "tab-container.tab-label*",
	                                   NULL, G_TYPE_NONE);

	for (iter = 0; styles[iter].stylename; iter++) {
		now = gtk_rc_get_style_by_paths(settings, styles[iter].labelname, NULL, G_TYPE_NONE);
		if (parent == now ||
				(parent && now && parent->rc_style == now->rc_style)) {
			GdkRGBA color;
			gchar *color_str;

			gdk_rgba_parse(&color, styles[iter].color);
			pidgin_style_adjust_contrast(gtk_widget_get_default_style(), &color);

			color_str = gdk_rgba_to_string(&color);
			g_string_append_printf(str, "style \"%s\" {\n"
					"fg[ACTIVE] = \"%s\"\n"
					"}\n"
					"widget \"*%s\" style \"%s\"\n",
					styles[iter].stylename,
					color_str,
					styles[iter].labelname, styles[iter].stylename);
			g_free(color_str);
		}
	}
	gtk_rc_parse_string(str->str);
	g_string_free(str, TRUE);
	gtk_rc_reset_styles(settings);
}

void
pidgin_conversations_uninit(void)
{
	purple_prefs_disconnect_by_handle(pidgin_conversations_get_handle());
	purple_signals_disconnect_by_handle(pidgin_conversations_get_handle());
	purple_signals_unregister_by_instance(pidgin_conversations_get_handle());
}

/**************************************************************************
 * PidginConversation GBoxed code
 **************************************************************************/
static PidginConversation *
pidgin_conversation_ref(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, NULL);

	gtkconv->box_count++;

	return gtkconv;
}

static void
pidgin_conversation_unref(PidginConversation *gtkconv)
{
	g_return_if_fail(gtkconv != NULL);
	g_return_if_fail(gtkconv->box_count >= 0);

	if (!gtkconv->box_count--)
		pidgin_conv_destroy(gtkconv->active_conv);
}

GType
pidgin_conversation_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PidginConversation",
				(GBoxedCopyFunc)pidgin_conversation_ref,
				(GBoxedFreeFunc)pidgin_conversation_unref);
	}

	return type;
}
















/* down here is where gtkconvwin.c ought to start. except they share like every freaking function,
 * and touch each others' private members all day long */

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
#include "pidgin.h"


#include <gdk/gdkkeysyms.h>

#include "account.h"
#include "cmds.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "protocol.h"
#include "request.h"
#include "util.h"

#include "gtkdnd-hints.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkmenutray.h"
#include "gtkpounce.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkutils.h"
#include "pidginstock.h"

static void
do_close(GtkWidget *w, int resp, PidginConvWindow *win)
{
	gtk_widget_destroy(warn_close_dialog);
	warn_close_dialog = NULL;

	if (resp == GTK_RESPONSE_OK)
		pidgin_conv_window_destroy(win);
}

static void
build_warn_close_dialog(PidginConvWindow *gtkwin)
{
	GtkWidget *label, *vbox, *hbox, *img;

	g_return_if_fail(warn_close_dialog == NULL);

	warn_close_dialog = gtk_dialog_new_with_buttons(_("Confirm close"),
							GTK_WINDOW(gtkwin->window), GTK_DIALOG_MODAL,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(warn_close_dialog),
	                                GTK_RESPONSE_OK);

	gtk_container_set_border_width(GTK_CONTAINER(warn_close_dialog),
	                               6);
	gtk_window_set_resizable(GTK_WINDOW(warn_close_dialog), FALSE);

	/* Setup the outside spacing. */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(warn_close_dialog));

	gtk_box_set_spacing(GTK_BOX(vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	img = gtk_image_new_from_icon_name("dialog-warning",
			GTK_ICON_SIZE_DIALOG);

	/* Setup the inner hbox and put the dialog's icon in it. */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_widget_set_halign(img, GTK_ALIGN_START);
	gtk_widget_set_valign(img, GTK_ALIGN_START);

	/* Setup the right vbox. */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(_("You have unread messages. Are you sure you want to close the window?"));
	gtk_widget_set_size_request(label, 350, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* Connect the signals. */
	g_signal_connect(G_OBJECT(warn_close_dialog), "response",
	                 G_CALLBACK(do_close), gtkwin);

}

/**************************************************************************
 * Callbacks
 **************************************************************************/

static gboolean
close_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginConvWindow *win = d;
	GList *l;

	/* If there are unread messages then show a warning dialog */
	for (l = pidgin_conv_window_get_gtkconvs(win);
	     l != NULL; l = l->next)
	{
		PidginConversation *gtkconv = l->data;
		if (PURPLE_IS_IM_CONVERSATION(gtkconv->active_conv) &&
				gtkconv->unseen_state >= PIDGIN_UNSEEN_TEXT)
		{
			build_warn_close_dialog(win);
			gtk_widget_show_all(warn_close_dialog);

			return TRUE;
		}
	}

	pidgin_conv_window_destroy(win);

	return TRUE;
}

static void
conv_set_unseen(PurpleConversation *conv, PidginUnseenState state)
{
	int unseen_count = 0;
	PidginUnseenState unseen_state = PIDGIN_UNSEEN_NONE;

	if(g_object_get_data(G_OBJECT(conv), "unseen-count"))
		unseen_count = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "unseen-count"));

	if(g_object_get_data(G_OBJECT(conv), "unseen-state"))
		unseen_state = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(conv), "unseen-state"));

	if (state == PIDGIN_UNSEEN_NONE)
	{
		unseen_count = 0;
		unseen_state = PIDGIN_UNSEEN_NONE;
	}
	else
	{
		if (state >= PIDGIN_UNSEEN_TEXT)
			unseen_count++;

		if (state > unseen_state)
			unseen_state = state;
	}

	g_object_set_data(G_OBJECT(conv), "unseen-count", GINT_TO_POINTER(unseen_count));
	g_object_set_data(G_OBJECT(conv), "unseen-state", GINT_TO_POINTER(unseen_state));

	purple_conversation_update(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
}

static void
gtkconv_set_unseen(PidginConversation *gtkconv, PidginUnseenState state)
{
	if (state == PIDGIN_UNSEEN_NONE)
	{
		gtkconv->unseen_count = 0;
		gtkconv->unseen_state = PIDGIN_UNSEEN_NONE;
	}
	else
	{
		if (state >= PIDGIN_UNSEEN_TEXT)
			gtkconv->unseen_count++;

		if (state > gtkconv->unseen_state)
			gtkconv->unseen_state = state;
	}

	g_object_set_data(G_OBJECT(gtkconv->active_conv), "unseen-count", GINT_TO_POINTER(gtkconv->unseen_count));
	g_object_set_data(G_OBJECT(gtkconv->active_conv), "unseen-state", GINT_TO_POINTER(gtkconv->unseen_state));

	purple_conversation_update(gtkconv->active_conv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
}

/*
 * When a conversation window is focused, we know the user
 * has looked at it so we know there are no longer unseen
 * messages.
 */
static gboolean
focus_win_cb(GtkWidget *w, GdkEventFocus *e, gpointer d)
{
	PidginConvWindow *win = d;
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	if (gtkconv)
		gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);

	return FALSE;
}

static void
notebook_init_grab(PidginConvWindow *gtkwin, GtkWidget *widget, GdkEvent *event)
{
	static GdkCursor *cursor = NULL;
	GdkDevice *device;

	gtkwin->in_drag = TRUE;

	if (gtkwin->drag_leave_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
		                            gtkwin->drag_leave_signal);
		gtkwin->drag_leave_signal = 0;
	}

	if (cursor == NULL) {
		GdkDisplay *display = gtk_widget_get_display(gtkwin->notebook);
		cursor = gdk_cursor_new_for_display(display, GDK_FLEUR);
	}

	/* Grab the pointer */
	gtk_grab_add(gtkwin->notebook);
	device = gdk_event_get_device(event);
	if (!gdk_display_device_is_grabbed(gdk_device_get_display(device),
	                                   device)) {
		gdk_seat_grab(gdk_event_get_seat(event),
		              gtk_widget_get_window(gtkwin->notebook),
		              GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE, cursor, event,
		              NULL, NULL);
	}
}

static gboolean
notebook_motion_cb(GtkWidget *widget, GdkEventButton *e, PidginConvWindow *win)
{

	/*
	* Make sure the user moved the mouse far enough for the
	* drag to be initiated.
	*/
	if (win->in_predrag) {
		if (e->x_root <  win->drag_min_x ||
		    e->x_root >= win->drag_max_x ||
		    e->y_root <  win->drag_min_y ||
		    e->y_root >= win->drag_max_y) {

			    win->in_predrag = FALSE;
			    notebook_init_grab(win, widget, (GdkEvent *)e);
		    }
	}
	else { /* Otherwise, draw the arrows. */
		PidginConvWindow *dest_win;
		GtkNotebook *dest_notebook;
		GtkWidget *tab;
		gint page_num;
		gboolean horiz_tabs = FALSE;
		gboolean to_right = FALSE;

		/* Get the window that the cursor is over. */
		dest_win = pidgin_conv_window_get_at_event((GdkEvent *)e);

		if (dest_win == NULL) {
			pidgin_dnd_hints_hide_all();

			return TRUE;
		}

		dest_notebook = GTK_NOTEBOOK(dest_win->notebook);

		if (gtk_notebook_get_show_tabs(dest_notebook)) {
			page_num = pidgin_conv_get_tab_at_xy(dest_win,
			                                      e->x_root, e->y_root, &to_right);
			to_right = to_right && (win != dest_win);
			tab = pidgin_conv_window_get_gtkconv_at_index(dest_win, page_num)->tabby;
		} else {
			page_num = 0;
			to_right = pidgin_conv_xy_to_right_infopane(dest_win, e->x_root, e->y_root);
			tab = pidgin_conv_window_get_gtkconv_at_index(dest_win, page_num)->infopane_hbox;
		}

		if (gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_TOP ||
				gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_BOTTOM) {
			horiz_tabs = TRUE;
		}

		if (gtk_notebook_get_show_tabs(dest_notebook) == FALSE && win == dest_win)
		{
			/* dragging a tab from a single-tabbed window over its own window */
			pidgin_dnd_hints_hide_all();
			return TRUE;
		} else if (horiz_tabs) {
			if (((gpointer)win == (gpointer)dest_win && win->drag_tab < page_num) || to_right) {
				pidgin_dnd_hints_show_relative(HINT_ARROW_DOWN, tab, HINT_POSITION_RIGHT, HINT_POSITION_TOP);
				pidgin_dnd_hints_show_relative(HINT_ARROW_UP, tab, HINT_POSITION_RIGHT, HINT_POSITION_BOTTOM);
			} else {
				pidgin_dnd_hints_show_relative(HINT_ARROW_DOWN, tab, HINT_POSITION_LEFT, HINT_POSITION_TOP);
				pidgin_dnd_hints_show_relative(HINT_ARROW_UP, tab, HINT_POSITION_LEFT, HINT_POSITION_BOTTOM);
			}
		} else {
			if (((gpointer)win == (gpointer)dest_win && win->drag_tab < page_num) || to_right) {
				pidgin_dnd_hints_show_relative(HINT_ARROW_RIGHT, tab, HINT_POSITION_LEFT, HINT_POSITION_BOTTOM);
				pidgin_dnd_hints_show_relative(HINT_ARROW_LEFT, tab, HINT_POSITION_RIGHT, HINT_POSITION_BOTTOM);
			} else {
				pidgin_dnd_hints_show_relative(HINT_ARROW_RIGHT, tab, HINT_POSITION_LEFT, HINT_POSITION_TOP);
				pidgin_dnd_hints_show_relative(HINT_ARROW_LEFT, tab, HINT_POSITION_RIGHT, HINT_POSITION_TOP);
			}
		}
	}

	return TRUE;
}

static gboolean
notebook_leave_cb(GtkWidget *widget, GdkEventCrossing *e, PidginConvWindow *win)
{
	if (win->in_drag)
		return FALSE;

	if (e->x_root <  win->drag_min_x ||
	    e->x_root >= win->drag_max_x ||
	    e->y_root <  win->drag_min_y ||
	    e->y_root >= win->drag_max_y) {

		win->in_predrag = FALSE;
		notebook_init_grab(win, widget, (GdkEvent *)e);
	}

	return TRUE;
}

/*
 * THANK YOU GALEON!
 */

static gboolean
infopane_press_cb(GtkWidget *widget, GdkEventButton *e, PidginConversation *gtkconv)
{
	if (e->type == GDK_2BUTTON_PRESS && e->button == GDK_BUTTON_PRIMARY) {
		if (infopane_entry_activate(gtkconv))
			return TRUE;
	}

	if (e->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (e->button == GDK_BUTTON_PRIMARY) {
		int nb_x, nb_y;
		GtkAllocation allocation;

		gtk_widget_get_allocation(gtkconv->infopane_hbox, &allocation);

		if (gtkconv->win->in_drag)
			return TRUE;

		gtkconv->win->in_predrag = TRUE;
		gtkconv->win->drag_tab = gtk_notebook_page_num(GTK_NOTEBOOK(gtkconv->win->notebook), gtkconv->tab_cont);

		gdk_window_get_origin(gtk_widget_get_window(gtkconv->infopane_hbox), &nb_x, &nb_y);

		gtkconv->win->drag_min_x = allocation.x + nb_x;
		gtkconv->win->drag_min_y = allocation.y + nb_y;
		gtkconv->win->drag_max_x = allocation.width + gtkconv->win->drag_min_x;
		gtkconv->win->drag_max_y = allocation.height + gtkconv->win->drag_min_y;

		gtkconv->win->drag_motion_signal = g_signal_connect(G_OBJECT(gtkconv->win->notebook), "motion_notify_event",
								    G_CALLBACK(notebook_motion_cb), gtkconv->win);
		gtkconv->win->drag_leave_signal = g_signal_connect(G_OBJECT(gtkconv->win->notebook), "leave_notify_event",
								    G_CALLBACK(notebook_leave_cb), gtkconv->win);
		return FALSE;
	}

	if (gdk_event_triggers_context_menu((GdkEvent *)e)) {
		/* Right click was pressed. Popup the context menu. */
		GtkWidget *menu = gtk_menu_new(), *sub;
		gboolean populated = populate_menu_with_options(menu, gtkconv, TRUE);

		sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(gtkconv->win->menu->send_to));
		if (sub && gtk_widget_is_sensitive(gtkconv->win->menu->send_to)) {
			GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("S_end To"));
			if (populated)
				pidgin_separator(menu);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), sub);
			gtk_widget_show(item);
			gtk_widget_show_all(sub);
		} else if (!populated) {
			gtk_widget_destroy(menu);
			return FALSE;
		}

		gtk_widget_show_all(menu);
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)e);
		return TRUE;
	}
	return FALSE;
}

static gboolean
notebook_press_cb(GtkWidget *widget, GdkEventButton *e, PidginConvWindow *win)
{
	gint nb_x, nb_y;
	int tab_clicked;
	GtkWidget *page;
	GtkWidget *tab;
	GtkAllocation allocation;

	if (e->button == GDK_BUTTON_MIDDLE && e->type == GDK_BUTTON_PRESS) {
		PidginConversation *gtkconv;
		tab_clicked = pidgin_conv_get_tab_at_xy(win, e->x_root, e->y_root, NULL);

		if (tab_clicked == -1)
			return FALSE;

		gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, tab_clicked);
		close_conv_cb(NULL, gtkconv);
		return TRUE;
	}


	if (e->button != GDK_BUTTON_PRIMARY || e->type != GDK_BUTTON_PRESS)
		return FALSE;


	if (win->in_drag) {
		purple_debug(PURPLE_DEBUG_WARNING, "gtkconv",
		           "Already in the middle of a window drag at tab_press_cb\n");
		return TRUE;
	}

	/*
	* Make sure a tab was actually clicked. The arrow buttons
	* mess things up.
	*/
	tab_clicked = pidgin_conv_get_tab_at_xy(win, e->x_root, e->y_root, NULL);

	if (tab_clicked == -1)
		return FALSE;

	/*
	* Get the relative position of the press event, with regards to
	* the position of the notebook.
	*/
	gdk_window_get_origin(gtk_widget_get_window(win->notebook), &nb_x, &nb_y);

	/* Reset the min/max x/y */
	win->drag_min_x = 0;
	win->drag_min_y = 0;
	win->drag_max_x = 0;
	win->drag_max_y = 0;

	/* Find out which tab was dragged. */
	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), tab_clicked);
	tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(win->notebook), page);

	gtk_widget_get_allocation(tab, &allocation);

	win->drag_min_x = allocation.x      + nb_x;
	win->drag_min_y = allocation.y      + nb_y;
	win->drag_max_x = allocation.width  + win->drag_min_x;
	win->drag_max_y = allocation.height + win->drag_min_y;

	/* Make sure the click occurred in the tab. */
	if (e->x_root <  win->drag_min_x ||
	    e->x_root >= win->drag_max_x ||
	    e->y_root <  win->drag_min_y ||
	    e->y_root >= win->drag_max_y) {

		return FALSE;
	}

	win->in_predrag = TRUE;
	win->drag_tab = tab_clicked;

	/* Connect the new motion signals. */
	win->drag_motion_signal =
		g_signal_connect(G_OBJECT(widget), "motion_notify_event",
		                 G_CALLBACK(notebook_motion_cb), win);

	win->drag_leave_signal =
		g_signal_connect(G_OBJECT(widget), "leave_notify_event",
		                 G_CALLBACK(notebook_leave_cb), win);

	return FALSE;
}

static gboolean
notebook_release_cb(GtkWidget *widget, GdkEventButton *e, PidginConvWindow *win)
{
	PidginConvWindow *dest_win;
	GtkNotebook *dest_notebook;
	PidginConversation *active_gtkconv;
	PidginConversation *gtkconv;
	gint dest_page_num = 0;
	gboolean new_window = FALSE;
	gboolean to_right = FALSE;
	GdkDevice *device;

	/*
	* Don't check to make sure that the event's window matches the
	* widget's, because we may be getting an event passed on from the
	* close button.
	*/
	if (e->button != GDK_BUTTON_PRIMARY && e->type != GDK_BUTTON_RELEASE)
		return FALSE;

	device = gdk_event_get_device((GdkEvent *)e);
	if (gdk_display_device_is_grabbed(gdk_device_get_display(device), device)) {
		gdk_seat_ungrab(gdk_event_get_seat((GdkEvent *)e));
		gtk_grab_remove(widget);
	}

	if (!win->in_predrag && !win->in_drag)
		return FALSE;

	/* Disconnect the motion signal. */
	if (win->drag_motion_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
		                            win->drag_motion_signal);

		win->drag_motion_signal = 0;
	}

	/*
	* If we're in a pre-drag, we'll also need to disconnect the leave
	* signal.
	*/
	if (win->in_predrag) {
		win->in_predrag = FALSE;

		if (win->drag_leave_signal) {
			g_signal_handler_disconnect(G_OBJECT(widget),
			                            win->drag_leave_signal);

			win->drag_leave_signal = 0;
		}
	}

	/* If we're not in drag...        */
	/* We're perfectly normal people! */
	if (!win->in_drag)
		return FALSE;

	win->in_drag = FALSE;

	pidgin_dnd_hints_hide_all();

	dest_win = pidgin_conv_window_get_at_event((GdkEvent *)e);

	active_gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	if (dest_win == NULL) {
		/* If the current window doesn't have any other conversations,
		* there isn't much point transferring the conv to a new window. */
		if (pidgin_conv_window_get_gtkconv_count(win) > 1) {
			/* Make a new window to stick this to. */
			dest_win = pidgin_conv_window_new();
			new_window = TRUE;
		}
	}

	if (dest_win == NULL)
		return FALSE;

	purple_signal_emit(pidgin_conversations_get_handle(),
	                 "conversation-dragging", win, dest_win);

	/* Get the destination page number. */
	if (!new_window) {
		dest_notebook = GTK_NOTEBOOK(dest_win->notebook);
		if (gtk_notebook_get_show_tabs(dest_notebook)) {
			dest_page_num = pidgin_conv_get_tab_at_xy(dest_win,
			                                           e->x_root, e->y_root, &to_right);
		} else {
			dest_page_num = 0;
			to_right = pidgin_conv_xy_to_right_infopane(dest_win, e->x_root, e->y_root);
		}
	}

	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, win->drag_tab);

	if (win == dest_win) {
		gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, dest_page_num);
	} else {
		pidgin_conv_window_remove_gtkconv(win, gtkconv);
		pidgin_conv_window_add_gtkconv(dest_win, gtkconv);
		gtk_notebook_reorder_child(GTK_NOTEBOOK(dest_win->notebook), gtkconv->tab_cont, dest_page_num + to_right);
		pidgin_conv_window_switch_gtkconv(dest_win, gtkconv);
		if (new_window) {
			gint win_width, win_height;

			gtk_window_get_size(GTK_WINDOW(dest_win->window),
			                    &win_width, &win_height);
#ifdef _WIN32  /* only override window manager placement on Windows */
			gtk_window_move(GTK_WINDOW(dest_win->window),
			                e->x_root - (win_width  / 2),
			                e->y_root - (win_height / 2));
#endif

			pidgin_conv_window_show(dest_win);
		}
	}

	gtk_widget_grab_focus(active_gtkconv->editor);

	return TRUE;
}


static void
before_switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
                      gpointer user_data)
{
	PidginConvWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	win = user_data;
	conv = pidgin_conv_window_get_active_conversation(win);

	g_return_if_fail(conv != NULL);

	if (!PURPLE_IS_IM_CONVERSATION(conv))
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	if (gtkconv->u.im->typing_timer != 0) {
		g_source_remove(gtkconv->u.im->typing_timer);
		gtkconv->u.im->typing_timer = 0;
	}

	stop_anim(NULL, gtkconv);
}

static void
close_window(GtkWidget *w, PidginConvWindow *win)
{
	close_win_cb(w, NULL, win);
}

static void
detach_tab_cb(GtkWidget *w, PidginConvWindow *win)
{
	PidginConvWindow *new_window;
	PidginConversation *gtkconv;

	gtkconv = win->clicked_tab;

	if (!gtkconv)
		return;

	/* Nothing to do if there's only one tab in the window */
	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		return;

	pidgin_conv_window_remove_gtkconv(win, gtkconv);

	new_window = pidgin_conv_window_new();
	pidgin_conv_window_add_gtkconv(new_window, gtkconv);
	pidgin_conv_window_show(new_window);
}

static void
close_others_cb(GtkWidget *w, PidginConvWindow *win)
{
	GList *iter;
	PidginConversation *gtkconv;

	gtkconv = win->clicked_tab;

	if (!gtkconv)
		return;

	for (iter = pidgin_conv_window_get_gtkconvs(win); iter; )
	{
		PidginConversation *gconv = iter->data;
		iter = iter->next;

		if (gconv != gtkconv)
		{
			close_conv_cb(NULL, gconv);
		}
	}
}

static void
close_tab_cb(GtkWidget *w, PidginConvWindow *win)
{
	PidginConversation *gtkconv;

	gtkconv = win->clicked_tab;

	if (gtkconv)
		close_conv_cb(NULL, gtkconv);
}

static void
notebook_menu_switch_cb(GtkWidget *item, GtkWidget *child)
{
	GtkNotebook *notebook;
	int index;

	notebook = GTK_NOTEBOOK(gtk_widget_get_parent(child));
	index = gtk_notebook_page_num(notebook, child);
	gtk_notebook_set_current_page(notebook, index);
}

static void
notebook_menu_update_label_cb(GtkWidget *child, GParamSpec *pspec,
                              GtkNotebook *notebook)
{
	GtkWidget *item;
	GtkWidget *label;

	item = g_object_get_data(G_OBJECT(child), "popup-menu-item");
	label = gtk_bin_get_child(GTK_BIN(item));
	if (label)
		gtk_container_remove(GTK_CONTAINER(item), label);

	label = gtk_notebook_get_menu_label(notebook, child);
	if (label) {
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(item), label);
		gtk_widget_show(item);
	} else {
		gtk_widget_hide(item);
	}
}

static void
notebook_add_tab_to_menu_cb(GtkNotebook *notebook, GtkWidget *child,
                            guint page_num, PidginConvWindow *win)
{
	GtkWidget *item;
	GtkWidget *label;

	item = gtk_menu_item_new();
	label = gtk_notebook_get_menu_label(notebook, child);
	if (label) {
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(item), label);
		gtk_widget_show(item);
	}

	g_signal_connect(child, "child-notify::menu-label",
	                 G_CALLBACK(notebook_menu_update_label_cb), notebook); 
	g_signal_connect(item, "activate",
	                 G_CALLBACK(notebook_menu_switch_cb), child);
	g_object_set_data(G_OBJECT(child), "popup-menu-item", item);

	gtk_menu_shell_insert(GTK_MENU_SHELL(win->notebook_menu), item, page_num);
}

static void
notebook_remove_tab_from_menu_cb(GtkNotebook *notebook, GtkWidget *child,
                                 guint page_num, PidginConvWindow *win)
{
	GtkWidget *item;

	/* Disconnecting the "child-notify::menu-label" signal. */
	g_signal_handlers_disconnect_by_data(child, notebook);

	item = g_object_get_data(G_OBJECT(child), "popup-menu-item");
	gtk_container_remove(GTK_CONTAINER(win->notebook_menu), item);
}


static void
notebook_reorder_tab_in_menu_cb(GtkNotebook *notebook, GtkWidget *child,
                                guint page_num, PidginConvWindow *win)
{
	GtkWidget *item;

	item = g_object_get_data(G_OBJECT(child), "popup-menu-item");
	gtk_menu_reorder_child(GTK_MENU(win->notebook_menu), item, page_num);
}

static gboolean
notebook_right_click_menu_cb(GtkNotebook *notebook, GdkEventButton *event,
                             PidginConvWindow *win)
{
	GtkWidget *menu;
	PidginConversation *gtkconv;

	if (!gdk_event_triggers_context_menu((GdkEvent *)event))
		return FALSE;

	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win,
			pidgin_conv_get_tab_at_xy(win, event->x_root, event->y_root, NULL));

	win->clicked_tab = gtkconv;

	menu = win->notebook_menu;

	gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

	return TRUE;
}

static void
remove_edit_entry(PidginConversation *gtkconv, GtkWidget *entry)
{
	g_signal_handlers_disconnect_matched(G_OBJECT(entry), G_SIGNAL_MATCH_DATA,
				0, 0, NULL, NULL, gtkconv);
	gtk_widget_show(gtkconv->infopane);
	gtk_widget_grab_focus(gtkconv->editor);
	gtk_widget_destroy(entry);
}

static gboolean
alias_focus_cb(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	remove_edit_entry(user_data, widget);
	return FALSE;
}

static gboolean
alias_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_KEY_Escape) {
		remove_edit_entry(user_data, widget);
		return TRUE;
	}
	return FALSE;
}

static void
alias_cb(GtkEntry *entry, gpointer user_data)
{
	PidginConversation *gtkconv;
	PurpleConversation *conv;
	PurpleAccount *account;
	const char *name;

	gtkconv = (PidginConversation *)user_data;
	if (gtkconv == NULL) {
		return;
	}
	conv    = gtkconv->active_conv;
	account = purple_conversation_get_account(conv);
	name    = purple_conversation_get_name(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *buddy;
		buddy = purple_blist_find_buddy(account, name);
		if (buddy != NULL) {
			purple_buddy_set_local_alias(buddy, gtk_entry_get_text(entry));
		}
		purple_serv_alias_buddy(buddy);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		gtk_entry_set_text(GTK_ENTRY(gtkconv->u.chat->topic_text), gtk_entry_get_text(entry));
		topic_callback(NULL, gtkconv);
	}
	remove_edit_entry(user_data, GTK_WIDGET(entry));
}

static gboolean
infopane_entry_activate(PidginConversation *gtkconv)
{
	GtkWidget *entry = NULL;
	PurpleConversation *conv = gtkconv->active_conv;
	const char *text = NULL;

	if (!gtk_widget_get_visible(gtkconv->infopane)) {
		/* There's already an entry for alias. Let's not create another one. */
		return FALSE;
	}

	if (!purple_account_is_connected(purple_conversation_get_account(gtkconv->active_conv))) {
		/* Do not allow aliasing someone on a disconnected account. */
		return FALSE;
	}

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		PurpleBuddy *buddy = purple_blist_find_buddy(purple_conversation_get_account(gtkconv->active_conv), purple_conversation_get_name(gtkconv->active_conv));
		if (!buddy)
			/* This buddy isn't in your buddy list, so we can't alias him */
			return FALSE;

		text = purple_buddy_get_contact_alias(buddy);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleConnection *gc;
		PurpleProtocol *protocol = NULL;

		gc = purple_conversation_get_connection(conv);
		if (gc != NULL)
			protocol = purple_connection_get_protocol(gc);
		if (protocol && !PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, set_topic))
			/* This protocol doesn't support setting the chat room topic */
			return FALSE;

		text = purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(conv));
	}

	/* alias label */
	entry = gtk_entry_new();
	gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
	gtk_entry_set_alignment(GTK_ENTRY(entry), 0.5);

	gtk_box_pack_start(GTK_BOX(gtkconv->infopane_hbox), entry, TRUE, TRUE, 0);
	/* after the tab label */
	gtk_box_reorder_child(GTK_BOX(gtkconv->infopane_hbox), entry, 0);

	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(alias_cb), gtkconv);
	g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(alias_focus_cb), gtkconv);
	g_signal_connect(G_OBJECT(entry), "key-press-event", G_CALLBACK(alias_key_press_cb), gtkconv);

	if (text != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), text);
	gtk_widget_show(entry);
	gtk_widget_hide(gtkconv->infopane);
	gtk_widget_grab_focus(entry);

	return TRUE;
}

static gboolean
window_keypress_cb(GtkWidget *widget, GdkEventKey *event, PidginConvWindow *win)
{
	PidginConversation *gtkconv = pidgin_conv_window_get_active_gtkconv(win);

	return conv_keypress_common(gtkconv, event);
}

static void
switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
               gpointer user_data)
{
	PidginConvWindow *win;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	const char *sound_method;

	win = user_data;
	gtkconv = pidgin_conv_window_get_gtkconv_at_index(win, page_num);
	conv = gtkconv->active_conv;

	g_return_if_fail(conv != NULL);

	/* clear unseen flag if conversation is not hidden */
	if(!pidgin_conv_is_hidden(gtkconv)) {
		gtkconv_set_unseen(gtkconv, PIDGIN_UNSEEN_NONE);
	}

	/* Update the menubar */

	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(gtkconv->win->menu->logging),
	                             purple_conversation_is_logging(conv));

	generate_send_to_items(win);
	generate_e2ee_controls(win);
	regenerate_options_items(win);
	regenerate_plugins_items(win);

	pidgin_conv_switch_active_conversation(conv);

	sound_method = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method");
	if (!purple_strequal(sound_method, "none"))
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu->sounds),
		                             gtkconv->make_sound);

	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(win->menu->show_formatting_toolbar),
	                             purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_formatting_toolbar"));

	/*
	 * We pause icons when they are not visible.  If this icon should
	 * be animated then start it back up again.
	 */
	if (PURPLE_IS_IM_CONVERSATION(conv) &&
	    (gtkconv->u.im->animate))
		start_anim(NULL, gtkconv);

	purple_signal_emit(pidgin_conversations_get_handle(), "conversation-switched", conv);
}

/**************************************************************************
 * GTK+ window ops
 **************************************************************************/

GList *
pidgin_conv_windows_get_list()
{
	return window_list;
}

static GList*
make_status_icon_list(const char *stock, GtkWidget *w)
{
	GList *l = NULL;
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL), "GtkWindow"));
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL), "GtkWindow"));
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MEDIUM), "GtkWindow"));
	l = g_list_append(l,
		gtk_widget_render_icon(w, stock,
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_LARGE), "GtkWindow"));
	return l;
}

static void
create_icon_lists(GtkWidget *w)
{
	available_list = make_status_icon_list(PIDGIN_STOCK_STATUS_AVAILABLE, w);
	busy_list = make_status_icon_list(PIDGIN_STOCK_STATUS_BUSY, w);
	xa_list = make_status_icon_list(PIDGIN_STOCK_STATUS_XA, w);
	offline_list = make_status_icon_list(PIDGIN_STOCK_STATUS_OFFLINE, w);
	away_list = make_status_icon_list(PIDGIN_STOCK_STATUS_AWAY, w);
	protocol_lists = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
plugin_changed_cb(PurplePlugin *p, gpointer data)
{
	regenerate_plugins_items(data);
}

static gboolean gtk_conv_configure_cb(GtkWidget *w, GdkEventConfigure *event, gpointer data) {
	int x, y;

	if (gtk_widget_get_visible(w))
		gtk_window_get_position(GTK_WINDOW(w), &x, &y);
	else
		return FALSE; /* carry on normally */

	/* Workaround for GTK+ bug # 169811 - "configure_event" is fired
	* when the window is being maximized */
	if (gdk_window_get_state(gtk_widget_get_window(w)) & GDK_WINDOW_STATE_MAXIMIZED)
		return FALSE;

	/* don't save off-screen positioning */
	if (x + event->width < 0 ||
	    y + event->height < 0 ||
	    x > gdk_screen_width() ||
	    y > gdk_screen_height())
		return FALSE; /* carry on normally */

	/* store the position */
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/x", x);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/y", y);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/width",  event->width);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/height", event->height);

	/* continue to handle event normally */
	return FALSE;

}

static void
pidgin_conv_set_position_size(PidginConvWindow *win, int conv_x, int conv_y,
		int conv_width, int conv_height)
{
	/* if the window exists, is hidden, we're saving positions, and the
	 * position is sane... */
	if (win && win->window &&
			!gtk_widget_get_visible(win->window) && conv_width != 0) {

#ifdef _WIN32  /* only override window manager placement on Windows */
		/* ...check position is on screen... */
		if (conv_x >= gdk_screen_width())
			conv_x = gdk_screen_width() - 100;
		else if (conv_x + conv_width < 0)
			conv_x = 100;

		if (conv_y >= gdk_screen_height())
			conv_y = gdk_screen_height() - 100;
		else if (conv_y + conv_height < 0)
			conv_y = 100;

		/* ...and move it back. */
		gtk_window_move(GTK_WINDOW(win->window), conv_x, conv_y);
#endif
		gtk_window_resize(GTK_WINDOW(win->window), conv_width, conv_height);
	}
}

static void
pidgin_conv_restore_position(PidginConvWindow *win) {
	pidgin_conv_set_position_size(win,
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/x"),
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/y"),
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/width"),
		purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/height"));
}

PidginConvWindow *
pidgin_conv_window_new()
{
	PidginConvWindow *win;
	GtkPositionType pos;
	GtkWidget *testidea;
	GtkWidget *menubar;
	GtkWidget *menu;
	GtkWidget *item;
	GdkModifierType state;

	win = g_malloc0(sizeof(PidginConvWindow));
	win->menu = g_malloc0(sizeof(PidginConvWindowMenu));

	window_list = g_list_append(window_list, win);

	/* Create the window. */
	win->window = pidgin_create_window(NULL, 0, "conversation", TRUE);
	/*_pidgin_widget_set_accessible_name(win->window, "Conversations");*/
	if (!gtk_get_current_event_state(&state))
		gtk_window_set_focus_on_map(GTK_WINDOW(win->window), FALSE);

	/* Etan: I really think this entire function call should happen only
	 * when we are on Windows but I was informed that back before we used
	 * to save the window position we stored the window size, so I'm
	 * leaving it for now. */
#if TRUE || defined(_WIN32)
	pidgin_conv_restore_position(win);
#endif

	if (available_list == NULL) {
		create_icon_lists(win->window);
	}

	g_signal_connect(G_OBJECT(win->window), "delete_event",
	                 G_CALLBACK(close_win_cb), win);
	g_signal_connect(G_OBJECT(win->window), "focus_in_event",
	                 G_CALLBACK(focus_win_cb), win);

	/* Intercept keystrokes from the menu items */
	g_signal_connect(G_OBJECT(win->window), "key_press_event",
					 G_CALLBACK(window_keypress_cb), win);


	/* Create the notebook. */
	win->notebook = gtk_notebook_new();

	pos = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side");

	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(win->notebook), pos);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(win->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(win->notebook), TRUE);

	menu = win->notebook_menu = gtk_menu_new();

	pidgin_separator(GTK_WIDGET(menu));

	item = gtk_menu_item_new_with_label(_("Close other tabs"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_others_cb), win);

	item = gtk_menu_item_new_with_label(_("Close all tabs"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_window), win);

	pidgin_separator(menu);

	item = gtk_menu_item_new_with_label(_("Detach this tab"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(detach_tab_cb), win);

	item = gtk_menu_item_new_with_label(_("Close this tab"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
					G_CALLBACK(close_tab_cb), win);

	g_signal_connect(G_OBJECT(win->notebook), "page-added",
	                 G_CALLBACK(notebook_add_tab_to_menu_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "page-removed",
	                 G_CALLBACK(notebook_remove_tab_from_menu_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "page-reordered",
	                 G_CALLBACK(notebook_reorder_tab_in_menu_cb), win);

	g_signal_connect(G_OBJECT(win->notebook), "button-press-event",
					G_CALLBACK(notebook_right_click_menu_cb), win);

	gtk_widget_show(win->notebook);

	g_signal_connect(G_OBJECT(win->notebook), "switch_page",
	                 G_CALLBACK(before_switch_conv_cb), win);
	g_signal_connect_after(G_OBJECT(win->notebook), "switch_page",
	                       G_CALLBACK(switch_conv_cb), win);

	/* Setup the tab drag and drop signals. */
	gtk_widget_add_events(win->notebook,
	                      GDK_BUTTON1_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(win->notebook), "button_press_event",
	                 G_CALLBACK(notebook_press_cb), win);
	g_signal_connect(G_OBJECT(win->notebook), "button_release_event",
	                 G_CALLBACK(notebook_release_cb), win);

	testidea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	/* Setup the menubar. */
	menubar = setup_menubar(win);
	gtk_box_pack_start(GTK_BOX(testidea), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(testidea), win->notebook, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(win->window), testidea);

	gtk_widget_show(testidea);

	/* Update the plugin actions when plugins are (un)loaded */
	purple_signal_connect(purple_plugins_get_handle(), "plugin-load",
			win, PURPLE_CALLBACK(plugin_changed_cb), win);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload",
			win, PURPLE_CALLBACK(plugin_changed_cb), win);


#ifdef _WIN32
	g_signal_connect(G_OBJECT(win->window), "show",
	                 G_CALLBACK(winpidgin_ensure_onscreen), win->window);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/win32/minimize_new_convs")
			&& !gtk_get_current_event_state(&state))
		gtk_window_iconify(GTK_WINDOW(win->window));
#endif

	purple_signal_emit(pidgin_conversations_get_handle(),
		"conversation-window-created", win);

	/* Fix colours */
	pidgin_conversations_set_tab_colors();

	return win;
}

void
pidgin_conv_window_destroy(PidginConvWindow *win)
{
	if (win->gtkconvs) {
		GList *iter = win->gtkconvs;
		while (iter)
		{
			PidginConversation *gtkconv = iter->data;
			iter = iter->next;
			close_conv_cb(NULL, gtkconv);
		}
		return;
	}

	purple_prefs_disconnect_by_handle(win);
	window_list = g_list_remove(window_list, win);

	gtk_widget_destroy(win->notebook_menu);
	gtk_widget_destroy(win->window);

	g_object_unref(G_OBJECT(win->menu->ui));

	purple_notify_close_with_handle(win);
	purple_signals_disconnect_by_handle(win);

	g_free(win->menu);
	g_free(win);
}

void
pidgin_conv_window_show(PidginConvWindow *win)
{
	gtk_widget_show(win->window);
}

void
pidgin_conv_window_hide(PidginConvWindow *win)
{
	gtk_widget_hide(win->window);
}

void
pidgin_conv_window_raise(PidginConvWindow *win)
{
	gdk_window_raise(GDK_WINDOW(gtk_widget_get_window(win->window)));
}

void
pidgin_conv_window_switch_gtkconv(PidginConvWindow *win, PidginConversation *gtkconv)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook),
	                              gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
		                              gtkconv->tab_cont));
}

static gboolean
gtkconv_tab_set_tip(GtkWidget *widget, GdkEventCrossing *event, PidginConversation *gtkconv)
{
/* PANGO_VERSION_CHECK macro was introduced in 1.15. So we need this double check. */
#ifndef PANGO_VERSION_CHECK
#define pango_layout_is_ellipsized(l) TRUE
#elif !PANGO_VERSION_CHECK(1,16,0)
#define pango_layout_is_ellipsized(l) TRUE
#endif
	PangoLayout *layout;

	layout = gtk_label_get_layout(GTK_LABEL(gtkconv->tab_label));
	if (pango_layout_is_ellipsized(layout))
		gtk_widget_set_tooltip_text(widget, gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
	else
		gtk_widget_set_tooltip_text(widget, NULL);

	return FALSE;
}

static void
set_default_tab_colors(GtkWidget *widget)
{
	GString *str;
	GtkCssProvider *provider;
	GError *error = NULL;
	int iter;

	struct {
		const char *labelname;
		const char *color;
	} styles[] = {
		{"tab-label-typing", "#4e9a06"},
		{"tab-label-typed", "#c4a000"},
		{"tab-label-attention", "#006aff"},
		{"tab-label-unreadchat", "#cc0000"},
		{"tab-label-event", "#888a85"},
		{NULL, NULL}
	};

	str = g_string_new(NULL);

	for (iter = 0; styles[iter].labelname; iter++) {
		g_string_append_printf(str,
		                       "#%s {\n"
		                       "	color: %s;\n"
		                       "}\n",
		                       styles[iter].labelname,
		                       styles[iter].color);
	}

	provider = gtk_css_provider_new();

	gtk_css_provider_load_from_data(provider, str->str, str->len, &error);

	gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
	                               GTK_STYLE_PROVIDER(provider),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	if (error)
		g_error_free(error);
	g_string_free(str, TRUE);
}

void
pidgin_conv_window_add_gtkconv(PidginConvWindow *win, PidginConversation *gtkconv)
{
	PurpleConversation *conv = gtkconv->active_conv;
	PidginConversation *focus_gtkconv;
	GtkWidget *tab_cont = gtkconv->tab_cont;
	const gchar *tmp_lab;

	win->gtkconvs = g_list_append(win->gtkconvs, gtkconv);
	gtkconv->win = win;

	if (win->gtkconvs && win->gtkconvs->next && win->gtkconvs->next->next == NULL)
		pidgin_conv_tab_pack(win, ((PidginConversation*)win->gtkconvs->data));


	/* Close button. */
	gtkconv->close = pidgin_create_small_button(gtk_label_new("×"));
	gtk_widget_set_tooltip_text(gtkconv->close, _("Close conversation"));

	g_signal_connect(gtkconv->close, "clicked", G_CALLBACK (close_conv_cb), gtkconv);

	/* Status icon. */
	gtkconv->icon = gtk_image_new();
	gtkconv->menu_icon = gtk_image_new();
	g_object_set(G_OBJECT(gtkconv->icon),
			"icon-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC),
			NULL);
	g_object_set(G_OBJECT(gtkconv->menu_icon),
			"icon-size", gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC),
			NULL);
	gtk_widget_show(gtkconv->icon);
	update_tab_icon(conv);

	/* Tab label. */
	gtkconv->tab_label = gtk_label_new(tmp_lab = purple_conversation_get_title(conv));
	set_default_tab_colors(gtkconv->tab_label);
	gtk_widget_set_name(gtkconv->tab_label, "tab-label");

	gtkconv->menu_tabby = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtkconv->menu_label = gtk_label_new(tmp_lab);
	gtk_box_pack_start(GTK_BOX(gtkconv->menu_tabby), gtkconv->menu_icon, FALSE, FALSE, 0);

	gtk_widget_show_all(gtkconv->menu_icon);

	gtk_box_pack_start(GTK_BOX(gtkconv->menu_tabby), gtkconv->menu_label, TRUE, TRUE, 0);
	gtk_widget_show(gtkconv->menu_label);
	gtk_label_set_xalign(GTK_LABEL(gtkconv->menu_label), 0);
	gtk_label_set_yalign(GTK_LABEL(gtkconv->menu_label), 0);

	gtk_widget_show(gtkconv->menu_tabby);

	if (PURPLE_IS_IM_CONVERSATION(conv))
		pidgin_conv_update_buddy_icon(PURPLE_IM_CONVERSATION(conv));

	/* Build and set conversations tab */
	pidgin_conv_tab_pack(win, gtkconv);

	gtk_notebook_set_menu_label(GTK_NOTEBOOK(win->notebook), tab_cont, gtkconv->menu_tabby);

	gtk_widget_show(tab_cont);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1) {
		/* Er, bug in notebooks? Switch to the page manually. */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
	} else {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), TRUE);
	}

	focus_gtkconv = g_list_nth_data(pidgin_conv_window_get_gtkconvs(win),
	                             gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook)));
	gtk_widget_grab_focus(focus_gtkconv->editor);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		update_send_to_selection(win);
}

static void
pidgin_conv_tab_pack(PidginConvWindow *win, PidginConversation *gtkconv)
{
	gboolean tabs_side = FALSE;
	gint angle = 0;
	GtkWidget *first, *third, *ebox, *parent;

	if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == GTK_POS_LEFT ||
	    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == GTK_POS_RIGHT)
		tabs_side = TRUE;
	else if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == (GTK_POS_LEFT|8))
		angle = 90;
	else if (purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") == (GTK_POS_RIGHT|8))
		angle = 270;

	if (!angle) {
		g_object_set(G_OBJECT(gtkconv->tab_label), "ellipsize", PANGO_ELLIPSIZE_END,  NULL);
		gtk_label_set_width_chars(GTK_LABEL(gtkconv->tab_label), 4);
	} else {
		g_object_set(G_OBJECT(gtkconv->tab_label), "ellipsize", PANGO_ELLIPSIZE_NONE, NULL);
		gtk_label_set_width_chars(GTK_LABEL(gtkconv->tab_label), -1);
	}

	if (tabs_side) {
		gtk_label_set_width_chars(
			GTK_LABEL(gtkconv->tab_label),
			MIN(g_utf8_strlen(gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)), -1), 12)
		);
	}

	gtk_label_set_angle(GTK_LABEL(gtkconv->tab_label), angle);

	if (angle)
		gtkconv->tabby = gtk_box_new(GTK_ORIENTATION_VERTICAL, PIDGIN_HIG_BOX_SPACE);
	else
		gtkconv->tabby = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_set_name(gtkconv->tabby, "tab-container");

	/* select the correct ordering for verticle tabs */
	if (angle == 90) {
		first = gtkconv->close;
		third = gtkconv->icon;
	} else {
		first = gtkconv->icon;
		third = gtkconv->close;
	}

	ebox = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), FALSE);
	gtk_container_add(GTK_CONTAINER(ebox), gtkconv->tabby);
	g_signal_connect(G_OBJECT(ebox), "enter-notify-event",
			G_CALLBACK(gtkconv_tab_set_tip), gtkconv);

	parent = gtk_widget_get_parent(gtkconv->tab_label);
	if (parent != NULL) {
		/* reparent old widgets on preference changes */
		g_object_ref(first);
		g_object_ref(gtkconv->tab_label);
		g_object_ref(third);
		gtk_container_remove(GTK_CONTAINER(parent), first);
		gtk_container_remove(GTK_CONTAINER(parent), gtkconv->tab_label);
		gtk_container_remove(GTK_CONTAINER(parent), third);
	}

	gtk_box_pack_start(GTK_BOX(gtkconv->tabby), first,              FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtkconv->tabby), gtkconv->tab_label, TRUE,  TRUE,  0);
	gtk_box_pack_start(GTK_BOX(gtkconv->tabby), third,              FALSE, FALSE, 0);

	if (parent == NULL) {
		/* Add this pane to the conversation's notebook. */
		gtk_notebook_append_page(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, ebox);
	} else {
		/* reparent old widgets on preference changes */
		g_object_unref(first);
		g_object_unref(gtkconv->tab_label);
		g_object_unref(third);

		/* Reset the tabs label to the new version */
		gtk_notebook_set_tab_label(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont, ebox);
	}

	gtk_container_child_set(GTK_CONTAINER(win->notebook), gtkconv->tab_cont,
	                        "tab-expand", !tabs_side && !angle,
	                        "tab-fill", TRUE, NULL);

	if (pidgin_conv_window_get_gtkconv_count(win) == 1)
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
					   purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs") &&
                                           (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons") ||
                                           purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/tab_side") != GTK_POS_TOP));

	/* show the widgets */
/*	gtk_widget_show(gtkconv->icon); */
	gtk_widget_show(gtkconv->tab_label);
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/close_on_tabs"))
		gtk_widget_show(gtkconv->close);
	gtk_widget_show(gtkconv->tabby);
	gtk_widget_show(ebox);
}

void
pidgin_conv_window_remove_gtkconv(PidginConvWindow *win, PidginConversation *gtkconv)
{
	unsigned int index;

	index = gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook), gtkconv->tab_cont);

	g_object_ref_sink(G_OBJECT(gtkconv->tab_cont));

	gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), index);

	win->gtkconvs = g_list_remove(win->gtkconvs, gtkconv);

	g_signal_handlers_disconnect_matched(win->window, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, gtkconv);

	if (win->gtkconvs && win->gtkconvs->next == NULL)
		pidgin_conv_tab_pack(win, win->gtkconvs->data);

	if (!win->gtkconvs && win != hidden_convwin)
		pidgin_conv_window_destroy(win);
}

PidginConversation *
pidgin_conv_window_get_gtkconv_at_index(const PidginConvWindow *win, int index)
{
	GtkWidget *tab_cont;

	if (index == -1)
		index = 0;
	tab_cont = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), index);
	return tab_cont ? g_object_get_data(G_OBJECT(tab_cont), "PidginConversation") : NULL;
}

PidginConversation *
pidgin_conv_window_get_active_gtkconv(const PidginConvWindow *win)
{
	int index;
	GtkWidget *tab_cont;

	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));
	if (index == -1)
		index = 0;
	tab_cont = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), index);
	if (!tab_cont)
		return NULL;
	return g_object_get_data(G_OBJECT(tab_cont), "PidginConversation");
}


PurpleConversation *
pidgin_conv_window_get_active_conversation(const PidginConvWindow *win)
{
	PidginConversation *gtkconv;

	gtkconv = pidgin_conv_window_get_active_gtkconv(win);
	return gtkconv ? gtkconv->active_conv : NULL;
}

gboolean
pidgin_conv_window_is_active_conversation(const PurpleConversation *conv)
{
	return conv == pidgin_conv_window_get_active_conversation(PIDGIN_CONVERSATION(conv)->win);
}

gboolean
pidgin_conv_window_has_focus(PidginConvWindow *win)
{
	gboolean has_focus = FALSE;

	g_object_get(G_OBJECT(win->window), "has-toplevel-focus", &has_focus, NULL);

	return has_focus;
}

PidginConvWindow *
pidgin_conv_window_get_at_event(GdkEvent *event)
{
	PidginConvWindow *win;
	GdkWindow *gdkwin;
	GList *l;
	int x, y;

	gdkwin = gdk_device_get_window_at_position(gdk_event_get_device(event),
	                                           &x, &y);

	if (gdkwin)
		gdkwin = gdk_window_get_toplevel(gdkwin);

	for (l = pidgin_conv_windows_get_list(); l != NULL; l = l->next) {
		win = l->data;

		if (gdkwin == gtk_widget_get_window(win->window))
			return win;
	}

	return NULL;
}

GList *
pidgin_conv_window_get_gtkconvs(PidginConvWindow *win)
{
	return win->gtkconvs;
}

guint
pidgin_conv_window_get_gtkconv_count(PidginConvWindow *win)
{
	return g_list_length(win->gtkconvs);
}

PidginConvWindow *
pidgin_conv_window_first_im(void)
{
	GList *wins, *convs;
	PidginConvWindow *win;
	PidginConversation *conv;

	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_IM_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}

PidginConvWindow *
pidgin_conv_window_last_im(void)
{
	GList *wins, *convs;
	PidginConvWindow *win;
	PidginConversation *conv;

	for (wins = g_list_last(pidgin_conv_windows_get_list());
	     wins != NULL;
	     wins = wins->prev) {

		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_IM_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}

PidginConvWindow *
pidgin_conv_window_first_chat(void)
{
	GList *wins, *convs;
	PidginConvWindow *win;
	PidginConversation *conv;

	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}

PidginConvWindow *
pidgin_conv_window_last_chat(void)
{
	GList *wins, *convs;
	PidginConvWindow *win;
	PidginConversation *conv;

	for (wins = g_list_last(pidgin_conv_windows_get_list());
	     wins != NULL;
	     wins = wins->prev) {

		win = wins->data;

		for (convs = win->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {

			conv = convs->data;

			if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv))
				return win;
		}
	}

	return NULL;
}


/**************************************************************************
 * Conversation placement functions
 **************************************************************************/
typedef struct
{
	char *id;
	char *name;
	PidginConvPlacementFunc fnc;

} ConvPlacementData;

static GList *conv_placement_fncs = NULL;
static PidginConvPlacementFunc place_conv = NULL;

/* This one places conversations in the last made window. */
static void
conv_placement_last_created_win(PidginConversation *conv)
{
	PidginConvWindow *win;

	GList *l = g_list_last(pidgin_conv_windows_get_list());
	win = l ? l->data : NULL;;

	if (win == NULL) {
		win = pidgin_conv_window_new();

		g_signal_connect(G_OBJECT(win->window), "configure_event",
				G_CALLBACK(gtk_conv_configure_cb), NULL);

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);
	} else {
		pidgin_conv_window_add_gtkconv(win, conv);
	}
}

/* This one places conversations in the last made window of the same type. */
static gboolean
conv_placement_last_created_win_type_configured_cb(GtkWidget *w,
		GdkEventConfigure *event, PidginConversation *conv)
{
	int x, y;
	GList *all;

	if (gtk_widget_get_visible(w))
		gtk_window_get_position(GTK_WINDOW(w), &x, &y);
	else
		return FALSE; /* carry on normally */

	/* Workaround for GTK+ bug # 169811 - "configure_event" is fired
	* when the window is being maximized */
	if (gdk_window_get_state(gtk_widget_get_window(w)) & GDK_WINDOW_STATE_MAXIMIZED)
		return FALSE;

	/* don't save off-screen positioning */
	if (x + event->width < 0 ||
	    y + event->height < 0 ||
	    x > gdk_screen_width() ||
	    y > gdk_screen_height())
		return FALSE; /* carry on normally */

	for (all = conv->convs; all != NULL; all = all->next) {
		if (PURPLE_IS_IM_CONVERSATION(conv->active_conv) != PURPLE_IS_IM_CONVERSATION(all->data)) {
			/* this window has different types of conversation, don't save */
			return FALSE;
		}
	}

	if (PURPLE_IS_IM_CONVERSATION(conv->active_conv)) {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/x", x);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/y", y);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/im/height", event->height);
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv)) {
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/x", x);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/y", y);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/width",  event->width);
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/chat/height", event->height);
	}

	return FALSE;
}

static void
conv_placement_last_created_win_type(PidginConversation *conv)
{
	PidginConvWindow *win;

	if (PURPLE_IS_IM_CONVERSATION(conv->active_conv))
		win = pidgin_conv_window_last_im();
	else
		win = pidgin_conv_window_last_chat();

	if (win == NULL) {
		win = pidgin_conv_window_new();

		if (PURPLE_IS_IM_CONVERSATION(conv->active_conv) ||
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/width") == 0) {
			pidgin_conv_set_position_size(win,
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/x"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/y"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/width"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/im/height"));
		} else if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv)) {
			pidgin_conv_set_position_size(win,
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/x"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/y"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/width"),
				purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/chat/height"));
		}

		pidgin_conv_window_add_gtkconv(win, conv);
		pidgin_conv_window_show(win);

		g_signal_connect(G_OBJECT(win->window), "configure_event",
				G_CALLBACK(conv_placement_last_created_win_type_configured_cb), conv);
	} else
		pidgin_conv_window_add_gtkconv(win, conv);
}

/* This one places each conversation in its own window. */
static void
conv_placement_new_window(PidginConversation *conv)
{
	PidginConvWindow *win;

	win = pidgin_conv_window_new();

	g_signal_connect(G_OBJECT(win->window), "configure_event",
			G_CALLBACK(gtk_conv_configure_cb), NULL);

	pidgin_conv_window_add_gtkconv(win, conv);

	pidgin_conv_window_show(win);
}

static PurpleGroup *
conv_get_group(PidginConversation *conv)
{
	PurpleGroup *group = NULL;

	if (PURPLE_IS_IM_CONVERSATION(conv->active_conv)) {
		PurpleBuddy *buddy;

		buddy = purple_blist_find_buddy(purple_conversation_get_account(conv->active_conv),
		                        purple_conversation_get_name(conv->active_conv));

		if (buddy != NULL)
			group = purple_buddy_get_group(buddy);

	} else if (PURPLE_IS_CHAT_CONVERSATION(conv->active_conv)) {
		PurpleChat *chat;

		chat = purple_blist_find_chat(purple_conversation_get_account(conv->active_conv),
		                            purple_conversation_get_name(conv->active_conv));

		if (chat != NULL)
			group = purple_chat_get_group(chat);
	}

	return group;
}

/*
 * This groups things by, well, group. Buddies from groups will always be
 * grouped together, and a buddy from a group not belonging to any currently
 * open windows will get a new window.
 */
static void
conv_placement_by_group(PidginConversation *conv)
{
	PurpleGroup *group = NULL;
	GList *wl, *cl;

	group = conv_get_group(conv);

	/* Go through the list of IMs and find one with this group. */
	for (wl = pidgin_conv_windows_get_list(); wl != NULL; wl = wl->next) {
		PidginConvWindow *win2;
		PidginConversation *conv2;
		PurpleGroup *group2 = NULL;

		win2 = wl->data;

		for (cl = win2->gtkconvs;
		     cl != NULL;
		     cl = cl->next) {
			conv2 = cl->data;

			group2 = conv_get_group(conv2);

			if (group == group2) {
				pidgin_conv_window_add_gtkconv(win2, conv);

				return;
			}
		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

/* This groups things by account.  Otherwise, the same semantics as above */
static void
conv_placement_by_account(PidginConversation *conv)
{
	GList *wins, *convs;
	PurpleAccount *account;

	account = purple_conversation_get_account(conv->active_conv);

	/* Go through the list of IMs and find one with this group. */
	for (wins = pidgin_conv_windows_get_list(); wins != NULL; wins = wins->next) {
		PidginConvWindow *win2;
		PidginConversation *conv2;

		win2 = wins->data;

		for (convs = win2->gtkconvs;
		     convs != NULL;
		     convs = convs->next) {
			conv2 = convs->data;

			if (account == purple_conversation_get_account(conv2->active_conv)) {
				pidgin_conv_window_add_gtkconv(win2, conv);
				return;
			}
		}
	}

	/* Make a new window. */
	conv_placement_new_window(conv);
}

static ConvPlacementData *
get_conv_placement_data(const char *id)
{
	ConvPlacementData *data = NULL;
	GList *n;

	for (n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		if (purple_strequal(data->id, id))
			return data;
	}

	return NULL;
}

static void
add_conv_placement_fnc(const char *id, const char *name,
                       PidginConvPlacementFunc fnc)
{
	ConvPlacementData *data;

	data = g_new(ConvPlacementData, 1);

	data->id = g_strdup(id);
	data->name = g_strdup(name);
	data->fnc  = fnc;

	conv_placement_fncs = g_list_append(conv_placement_fncs, data);
}

static void
ensure_default_funcs(void)
{
	if (conv_placement_fncs == NULL) {
		add_conv_placement_fnc("last", _("Last created window"),
		                       conv_placement_last_created_win);
		add_conv_placement_fnc("im_chat", _("Separate IM and Chat windows"),
		                       conv_placement_last_created_win_type);
		add_conv_placement_fnc("new", _("New window"),
		                       conv_placement_new_window);
		add_conv_placement_fnc("group", _("By group"),
		                       conv_placement_by_group);
		add_conv_placement_fnc("account", _("By account"),
		                       conv_placement_by_account);
	}
}

GList *
pidgin_conv_placement_get_options(void)
{
	GList *n, *list = NULL;
	ConvPlacementData *data;

	ensure_default_funcs();

	for (n = conv_placement_fncs; n; n = n->next) {
		data = n->data;
		list = g_list_append(list, data->name);
		list = g_list_append(list, data->id);
	}

	return list;
}


void
pidgin_conv_placement_add_fnc(const char *id, const char *name,
                            PidginConvPlacementFunc fnc)
{
	g_return_if_fail(id   != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(fnc  != NULL);

	ensure_default_funcs();

	add_conv_placement_fnc(id, name, fnc);
}

void
pidgin_conv_placement_remove_fnc(const char *id)
{
	ConvPlacementData *data = get_conv_placement_data(id);

	if (data == NULL)
		return;

	conv_placement_fncs = g_list_remove(conv_placement_fncs, data);

	g_free(data->id);
	g_free(data->name);
	g_free(data);
}

const char *
pidgin_conv_placement_get_name(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->name;
}

PidginConvPlacementFunc
pidgin_conv_placement_get_fnc(const char *id)
{
	ConvPlacementData *data;

	ensure_default_funcs();

	data = get_conv_placement_data(id);

	if (data == NULL)
		return NULL;

	return data->fnc;
}

void
pidgin_conv_placement_set_current_func(PidginConvPlacementFunc func)
{
	g_return_if_fail(func != NULL);

	/* If tabs are enabled, set the function, otherwise, NULL it out. */
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"))
		place_conv = func;
	else
		place_conv = NULL;
}

PidginConvPlacementFunc
pidgin_conv_placement_get_current_func(void)
{
	return place_conv;
}

void
pidgin_conv_placement_place(PidginConversation *gtkconv)
{
	if (place_conv)
		place_conv(gtkconv);
	else
		conv_placement_new_window(gtkconv);
}

gboolean
pidgin_conv_is_hidden(PidginConversation *gtkconv)
{
	g_return_val_if_fail(gtkconv != NULL, FALSE);

	return (gtkconv->win == hidden_convwin);
}


gdouble luminance(GdkRGBA color)
{
	gdouble r, g, b;
	gdouble rr, gg, bb;
	gdouble cutoff = 0.03928, scale = 12.92;
	gdouble a = 0.055, d = 1.055, p = 2.2;

	rr = color.red;
	gg = color.green;
	bb = color.blue;

	r = (rr  > cutoff) ? pow((rr+a)/d, p) : rr/scale;
	g = (gg  > cutoff) ? pow((gg+a)/d, p) : gg/scale;
	b = (bb  > cutoff) ? pow((bb+a)/d, p) : bb/scale;

	return (r*0.2126 + g*0.7152 + b*0.0722);
}

/* Algorithm from https://www.w3.org/TR/2008/REC-WCAG20-20081211/relative-luminance.xml */
static gboolean
color_is_visible(GdkRGBA foreground, GdkRGBA background, gdouble min_contrast_ratio)
{
	gdouble lfg, lbg, lmin, lmax;
	gdouble luminosity_ratio;
	gdouble nr, dr;

	lfg = luminance(foreground); 
	lbg = luminance(background);

	if (lfg > lbg)
		lmax = lfg, lmin = lbg;
	else
		lmax = lbg, lmin = lfg;

	nr = lmax + 0.05, dr = lmin - 0.05;
	if (dr < 0.005 && dr > -0.005)
		dr += 0.01;

	luminosity_ratio = nr/dr;
	if ( luminosity_ratio < 0) 
		luminosity_ratio *= -1.0;
	return (luminosity_ratio > min_contrast_ratio);
}


static GArray*
generate_nick_colors(guint numcolors, GdkRGBA background)
{
	guint i = 0, j = 0;
	GArray *colors = g_array_new(FALSE, FALSE, sizeof(GdkRGBA));
	GdkRGBA nick_highlight;
	GdkRGBA send_color;
	time_t breakout_time;

	gdk_rgba_parse(&nick_highlight, DEFAULT_HIGHLIGHT_COLOR);
	gdk_rgba_parse(&send_color, DEFAULT_SEND_COLOR);

	pidgin_style_adjust_contrast(NULL, &nick_highlight);
	pidgin_style_adjust_contrast(NULL, &send_color);

	srand(background.red * 65535 + background.green * 65535 + background.blue * 65535 + 1);

	breakout_time = time(NULL) + 3;

	/* first we look through the list of "good" colors: colors that differ from every other color in the
	 * list.  only some of them will differ from the background color though. lets see if we can find
	 * numcolors of them that do
	 */
	while (i < numcolors && j < PIDGIN_NUM_NICK_SEED_COLORS && time(NULL) < breakout_time)
	{
		GdkRGBA color = nick_seed_colors[j];

		if (color_is_visible(color, background,     MIN_LUMINANCE_CONTRAST_RATIO) &&
			color_is_visible(color, nick_highlight, MIN_LUMINANCE_CONTRAST_RATIO) &&
			color_is_visible(color, send_color,     MIN_LUMINANCE_CONTRAST_RATIO))
		{
			g_array_append_val(colors, color);
			i++;
		}
		j++;
	}

	/* we might not have found numcolors in the last loop.  if we did, we'll never enter this one.
	 * if we did not, lets just find some colors that don't conflict with the background.  its
	 * expensive to find colors that not only don't conflict with the background, but also do not
	 * conflict with each other.
	 */
	while(i < numcolors && time(NULL) < breakout_time)
	{
		GdkRGBA color = {g_random_double_range(0, 1), g_random_double_range(0, 1), g_random_double_range(0, 1), 1};

		if (color_is_visible(color, background,     MIN_LUMINANCE_CONTRAST_RATIO) &&
			color_is_visible(color, nick_highlight, MIN_LUMINANCE_CONTRAST_RATIO) &&
			color_is_visible(color, send_color,     MIN_LUMINANCE_CONTRAST_RATIO))
		{
			g_array_append_val(colors, color);
			i++;
		}
	}

	if (i < numcolors) {
		purple_debug_warning("gtkconv", "Unable to generate enough random colors before timeout. %u colors found.\n", i);
	}

	if( i == 0 ) {
		/* To remove errors caused by an empty array. */
		GdkRGBA color = {0.5, 0.5, 0.5, 1.0};
		g_array_append_val(colors, color);
	}

	return colors;
}

/**************************************************************************
 * PidginConvWindow GBoxed code
 **************************************************************************/
static PidginConvWindow *
pidgin_conv_window_ref(PidginConvWindow *win)
{
	g_return_val_if_fail(win != NULL, NULL);

	win->box_count++;

	return win;
}

static void
pidgin_conv_window_unref(PidginConvWindow *win)
{
	g_return_if_fail(win != NULL);
	g_return_if_fail(win->box_count >= 0);

	if (!win->box_count--)
		pidgin_conv_window_destroy(win);
}

GType
pidgin_conv_window_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PidginConvWindow",
				(GBoxedCopyFunc)pidgin_conv_window_ref,
				(GBoxedFreeFunc)pidgin_conv_window_unref);
	}

	return type;
}
