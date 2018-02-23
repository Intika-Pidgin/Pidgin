/*
 * purple - Jabber Protocol Plugin
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

#include "account.h"
#include "debug.h"
#include "request.h"
#include "util.h"
#include "xmlnode.h"

#include "jabber.h"
#include "auth.h"

static xmlnode *finish_webex_authentication(JabberStream *js)
{
	xmlnode *auth;

	auth = xmlnode_new("auth");
	xmlnode_set_namespace(auth, NS_XMPP_SASL);

	xmlnode_set_attrib(auth, "xmlns:ga", "http://www.google.com/talk/protocol/auth");
	xmlnode_set_attrib(auth, "ga:client-uses-full-bind-result", "true");

	xmlnode_set_attrib(auth, "mechanism", "WEBEX-TOKEN");
	xmlnode_insert_data(auth, purple_connection_get_password(js->gc), -1);

	return auth;
}

static JabberSaslState
jabber_webex_start(JabberStream *js, xmlnode *packet, xmlnode **response, char **error)
{
	*response = finish_webex_authentication(js);
	return JABBER_SASL_STATE_OK;
}

static JabberSaslMech webex_token_mech = {
	101, /* priority */
	"WEBEX-TOKEN", /* name */
	jabber_webex_start,
	NULL, /* handle_challenge */
	NULL, /* handle_success */
	NULL, /* handle_failure */
	NULL  /* dispose */
};

JabberSaslMech *jabber_auth_get_webex_token_mech(void)
{
	return &webex_token_mech;
}
