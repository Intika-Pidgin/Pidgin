/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#include "chat.h"
#include "core.h"
#include "plugins.h"
#include "purpleaccountusersplit.h"

#include "gtalk.h"

static const char *
gtalk_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "google-talk";
}

static void
gtalk_protocol_init(GTalkProtocol *self)
{
	PurpleProtocol *protocol = PURPLE_PROTOCOL(self);
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;
	GList *encryption_values = NULL;

	protocol->id   = "prpl-gtalk";
	protocol->name = "Google Talk (XMPP)";

	/* Translators: 'domain' is used here in the context of Internet domains, e.g. pidgin.im */
	split = purple_account_user_split_new(_("Domain"), "gmail.com", '@');
	purple_account_user_split_set_reverse(split, FALSE);
	protocol->user_splits = g_list_append(protocol->user_splits, split);

	split = purple_account_user_split_new(_("Resource"), "", '/');
	purple_account_user_split_set_reverse(split, FALSE);
	protocol->user_splits = g_list_append(protocol->user_splits, split);

#define ADD_VALUE(list, desc, v) { \
	PurpleKeyValuePair *kvp = purple_key_value_pair_new_full((desc), g_strdup((v)), g_free); \
	list = g_list_prepend(list, kvp); \
}

	ADD_VALUE(encryption_values, _("Require encryption"), "require_tls");
	ADD_VALUE(encryption_values, _("Use encryption if available"), "opportunistic_tls");
	ADD_VALUE(encryption_values, _("Use old-style SSL"), "old_ssl");
	encryption_values = g_list_reverse(encryption_values);

#undef ADD_VALUE

	option = purple_account_option_list_new(_("Connection security"), "connection_security", encryption_values);
	protocol->account_options = g_list_append(protocol->account_options,
						   option);

	option = purple_account_option_bool_new(
						_("Allow plaintext auth over unencrypted streams"),
						"auth_plain_in_clear", FALSE);
	protocol->account_options = g_list_append(protocol->account_options,
						   option);

	option = purple_account_option_int_new(_("Connect port"), "port", 5222);
	protocol->account_options = g_list_append(protocol->account_options,
						   option);

	option = purple_account_option_string_new(_("Connect server"),
						  "connect_server", NULL);
	protocol->account_options = g_list_append(protocol->account_options,
						  option);

	option = purple_account_option_string_new(_("File transfer proxies"),
						  "ft_proxies", NULL);
	protocol->account_options = g_list_append(protocol->account_options,
						  option);

	option = purple_account_option_string_new(_("BOSH URL"),
						  "bosh_url", NULL);
	protocol->account_options = g_list_append(protocol->account_options,
						  option);

	/* this should probably be part of global smiley theme settings later on,
	  shared with MSN */
	option = purple_account_option_bool_new(_("Show Custom Smileys"),
		"custom_smileys", TRUE);
	protocol->account_options = g_list_append(protocol->account_options,
		option);
}

static void
gtalk_protocol_class_init(GTalkProtocolClass *klass)
{
	PurpleProtocolClass *protocol_class = PURPLE_PROTOCOL_CLASS(klass);

	protocol_class->list_icon = gtalk_list_icon;
}

static void
gtalk_protocol_class_finalize(G_GNUC_UNUSED GTalkProtocolClass *klass)
{
}

static void
gtalk_protocol_server_iface_init(PurpleProtocolServerInterface *server_iface)
{
	/* disable xmpp functions not available for gtalk */
	server_iface->register_user   = NULL;
	server_iface->unregister_user = NULL;
}

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
        GTalkProtocol, gtalk_protocol, JABBER_TYPE_PROTOCOL, 0,

        G_IMPLEMENT_INTERFACE_DYNAMIC(PURPLE_TYPE_PROTOCOL_SERVER,
                                      gtalk_protocol_server_iface_init));

/* This exists solely because the above macro makes gtalk_protocol_register_type
 * static. */
void
gtalk_protocol_register(PurplePlugin *plugin)
{
	gtalk_protocol_register_type(G_TYPE_MODULE(plugin));
}
