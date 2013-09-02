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

/* libaim is the AIM protocol plugin. It is linked against liboscar,
 * which contains all the shared implementation code with libicq
 */

#include "core.h"
#include "plugins.h"
#include "signals.h"

#include "libaim.h"
#include "oscarcommon.h"

static PurpleProtocol *my_protocol = NULL;

static void
aim_protocol_base_init(AIMProtocolClass *klass)
{
	PurpleProtocolClass *proto_class = PURPLE_PROTOCOL_CLASS(klass);
	PurpleAccountOption *option;

	proto_class->id        = "aim";
	proto_class->name      = "AIM";

	oscar_init_protocol_options(proto_class);

	option = purple_account_option_bool_new(_("Allow multiple simultaneous logins"), "allow_multiple_logins",
											OSCAR_DEFAULT_ALLOW_MULTIPLE_LOGINS);
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);

	option = purple_account_option_string_new(_("Server"), "server", oscar_get_login_server(FALSE, TRUE));
	proto_class->protocol_options = g_list_append(proto_class->protocol_options, option);
}

static void
aim_protocol_interface_init(PurpleProtocolInterface *iface)
{
	iface->list_icon            = oscar_list_icon_aim;
	iface->add_permit           = oscar_add_permit;
	iface->rem_permit           = oscar_rem_permit;
	iface->set_permit_deny      = oscar_set_aim_permdeny;
	iface->get_max_message_size = oscar_get_max_message_size;
}

static void aim_protocol_base_finalize(AIMProtocolClass *klass) { }

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "protocol-aim",
		"name",         "AIM Protocol",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("AIM Protocol Plugin"),
		"description",  N_("AIM Protocol Plugin"),
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	my_protocol = purple_protocols_add(AIM_TYPE_PROTOCOL, error);
	if (!my_protocol)
		return FALSE;

	purple_signal_connect(purple_get_core(), "uri-handler", my_protocol,
		PURPLE_CALLBACK(oscar_uri_handler), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (!purple_protocols_remove(my_protocol, error))
		return FALSE;

	return TRUE;
}

extern PurplePlugin *_oscar_plugin;

PURPLE_PROTOCOL_DEFINE_EXTENDED(_oscar_plugin, AIMProtocol, aim_protocol,
                                OSCAR_TYPE_PROTOCOL, 0);

PURPLE_PLUGIN_INIT_VAL(_oscar_plugin, aim, plugin_query, plugin_load,
                       plugin_unload);
