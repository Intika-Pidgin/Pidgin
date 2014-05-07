/**
 * @file iceudp.c
 *
 * purple
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
 */

#include "internal.h"
#include "glibcompat.h"

#include "iceudp.h"
#include "jingle.h"
#include "debug.h"

#include <string.h>

struct _JingleIceUdpPrivate
{
	GList *local_candidates;
	GList *remote_candidates;
};

#define JINGLE_ICEUDP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), JINGLE_TYPE_ICEUDP, JingleIceUdpPrivate))

static void jingle_iceudp_class_init (JingleIceUdpClass *klass);
static void jingle_iceudp_init (JingleIceUdp *iceudp);
static void jingle_iceudp_finalize (GObject *object);
static void jingle_iceudp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void jingle_iceudp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static JingleTransport *jingle_iceudp_parse_internal(PurpleXmlNode *iceudp);
static PurpleXmlNode *jingle_iceudp_to_xml_internal(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action);
static void jingle_iceudp_add_local_candidate(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate);
static GList *jingle_iceudp_get_remote_candidates(JingleTransport *transport);

enum {
	PROP_0,
	PROP_LOCAL_CANDIDATES,
	PROP_REMOTE_CANDIDATES,
	PROP_LAST
};

static JingleTransportClass *parent_class = NULL;
static GParamSpec *properties[PROP_LAST];

static JingleIceUdpCandidate *
jingle_iceudp_candidate_copy(JingleIceUdpCandidate *candidate)
{
	JingleIceUdpCandidate *new_candidate = g_new0(JingleIceUdpCandidate, 1);
	new_candidate->id = g_strdup(candidate->id);
	new_candidate->component = candidate->component;
	new_candidate->foundation = g_strdup(candidate->foundation);
	new_candidate->generation = candidate->generation;
	new_candidate->ip = g_strdup(candidate->ip);
	new_candidate->network = candidate->network;
	new_candidate->port = candidate->port;
	new_candidate->priority = candidate->priority;
	new_candidate->protocol = g_strdup(candidate->protocol);
	new_candidate->type = g_strdup(candidate->type);

	new_candidate->username = g_strdup(candidate->username);
	new_candidate->password = g_strdup(candidate->password);

	new_candidate->rem_known = candidate->rem_known;

	return new_candidate;
}

static void
jingle_iceudp_candidate_free(JingleIceUdpCandidate *candidate)
{
	g_free(candidate->foundation);
	g_free(candidate->id);
	g_free(candidate->ip);
	g_free(candidate->protocol);
	g_free(candidate->reladdr);
	g_free(candidate->type);

	g_free(candidate->username);
	g_free(candidate->password);
}

GType
jingle_iceudp_candidate_get_type()
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("JingleIceUdpCandidate",
				(GBoxedCopyFunc)jingle_iceudp_candidate_copy,
				(GBoxedFreeFunc)jingle_iceudp_candidate_free);
	}
	return type;
}

JingleIceUdpCandidate *
jingle_iceudp_candidate_new(const gchar *id,
		guint component, const gchar *foundation,
		guint generation, const gchar *ip,
		guint network, guint port, guint priority,
		const gchar *protocol, const gchar *type,
		const gchar *username, const gchar *password)
{
	JingleIceUdpCandidate *candidate = g_new0(JingleIceUdpCandidate, 1);
	candidate->id = g_strdup(id);
	candidate->component = component;
	candidate->foundation = g_strdup(foundation);
	candidate->generation = generation;
	candidate->ip = g_strdup(ip);
	candidate->network = network;
	candidate->port = port;
	candidate->priority = priority;
	candidate->protocol = g_strdup(protocol);
	candidate->type = g_strdup(type);

	candidate->username = g_strdup(username);
	candidate->password = g_strdup(password);

	candidate->rem_known = FALSE;
	return candidate;
}

GType
jingle_iceudp_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(JingleIceUdpClass),
			NULL,
			NULL,
			(GClassInitFunc) jingle_iceudp_class_init,
			NULL,
			NULL,
			sizeof(JingleIceUdp),
			0,
			(GInstanceInitFunc) jingle_iceudp_init,
			NULL
		};
		type = g_type_register_static(JINGLE_TYPE_TRANSPORT, "JingleIceUdp", &info, 0);
	}
	return type;
}

static void
jingle_iceudp_class_init (JingleIceUdpClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = jingle_iceudp_finalize;
	gobject_class->set_property = jingle_iceudp_set_property;
	gobject_class->get_property = jingle_iceudp_get_property;
	klass->parent_class.to_xml = jingle_iceudp_to_xml_internal;
	klass->parent_class.parse = jingle_iceudp_parse_internal;
	klass->parent_class.transport_type = JINGLE_TRANSPORT_ICEUDP;
	klass->parent_class.add_local_candidate = jingle_iceudp_add_local_candidate;
	klass->parent_class.get_remote_candidates = jingle_iceudp_get_remote_candidates;

	g_type_class_add_private(klass, sizeof(JingleIceUdpPrivate));

	properties[PROP_LOCAL_CANDIDATES] = g_param_spec_pointer("local-candidates",
			"Local candidates",
			"The local candidates for this transport.",
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_REMOTE_CANDIDATES] = g_param_spec_pointer("remote-candidates",
			"Remote candidates",
			"The remote candidates for this transport.",
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(gobject_class, PROP_LAST, properties);
}

static void
jingle_iceudp_init (JingleIceUdp *iceudp)
{
	iceudp->priv = JINGLE_ICEUDP_GET_PRIVATE(iceudp);
	iceudp->priv->local_candidates = NULL;
	iceudp->priv->remote_candidates = NULL;
}

static void
jingle_iceudp_finalize (GObject *iceudp)
{
/*	JingleIceUdpPrivate *priv = JINGLE_ICEUDP_GET_PRIVATE(iceudp); */
	purple_debug_info("jingle","jingle_iceudp_finalize\n");

	G_OBJECT_CLASS(parent_class)->finalize(iceudp);
}

static void
jingle_iceudp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	JingleIceUdp *iceudp;

	g_return_if_fail(object != NULL);
	g_return_if_fail(JINGLE_IS_ICEUDP(object));

	iceudp = JINGLE_ICEUDP(object);

	switch (prop_id) {
		case PROP_LOCAL_CANDIDATES:
			iceudp->priv->local_candidates =
					g_value_get_pointer(value);
			break;
		case PROP_REMOTE_CANDIDATES:
			iceudp->priv->remote_candidates =
					g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_iceudp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	JingleIceUdp *iceudp;

	g_return_if_fail(object != NULL);
	g_return_if_fail(JINGLE_IS_ICEUDP(object));

	iceudp = JINGLE_ICEUDP(object);

	switch (prop_id) {
		case PROP_LOCAL_CANDIDATES:
			g_value_set_pointer(value, iceudp->priv->local_candidates);
			break;
		case PROP_REMOTE_CANDIDATES:
			g_value_set_pointer(value, iceudp->priv->remote_candidates);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_iceudp_add_local_candidate(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate)
{
	JingleIceUdp *iceudp = JINGLE_ICEUDP(transport);
	PurpleMediaCandidateType type;
	gchar *ip;
	gchar *username;
	gchar *password;
	JingleIceUdpCandidate *iceudp_candidate;
	GList *iter;

	ip = purple_media_candidate_get_ip(candidate);
	username = purple_media_candidate_get_username(candidate);
	password = purple_media_candidate_get_password(candidate);
	type = purple_media_candidate_get_candidate_type(candidate);

	iceudp_candidate = jingle_iceudp_candidate_new(id,
			purple_media_candidate_get_component_id(candidate),
			purple_media_candidate_get_foundation(candidate),
			generation, ip, 0,
			purple_media_candidate_get_port(candidate),
			purple_media_candidate_get_priority(candidate), "udp",
			type == PURPLE_MEDIA_CANDIDATE_TYPE_HOST ? "host" :
			type == PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX ? "srflx" :
			type == PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX ? "prflx" :
			type == PURPLE_MEDIA_CANDIDATE_TYPE_RELAY ? "relay" :
			"", username, password);
	iceudp_candidate->reladdr = purple_media_candidate_get_base_ip(candidate);
	iceudp_candidate->relport = purple_media_candidate_get_base_port(candidate);

	g_free(password);
	g_free(username);
	g_free(ip);

	for (iter = iceudp->priv->local_candidates; iter; iter = g_list_next(iter)) {
		JingleIceUdpCandidate *c = iter->data;
		if (!strcmp(c->id, id)) {
			generation = c->generation + 1;

			g_boxed_free(JINGLE_TYPE_ICEUDP_CANDIDATE, c);
			iceudp->priv->local_candidates = g_list_delete_link(
					iceudp->priv->local_candidates, iter);

			iceudp_candidate->generation = generation;

			iceudp->priv->local_candidates = g_list_append(
					iceudp->priv->local_candidates, iceudp_candidate);

			g_object_notify_by_pspec(G_OBJECT(iceudp), properties[PROP_LOCAL_CANDIDATES]);

			return;
		}
	}

	iceudp->priv->local_candidates = g_list_append(
			iceudp->priv->local_candidates, iceudp_candidate);

	g_object_notify_by_pspec(G_OBJECT(iceudp), properties[PROP_LOCAL_CANDIDATES]);
}

static GList *
jingle_iceudp_get_remote_candidates(JingleTransport *transport)
{
	JingleIceUdp *iceudp = JINGLE_ICEUDP(transport);
	GList *candidates = iceudp->priv->remote_candidates;
	GList *ret = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		JingleIceUdpCandidate *candidate = candidates->data;
		PurpleMediaCandidate *new_candidate = purple_media_candidate_new(
					candidate->foundation, candidate->component,
					!strcmp(candidate->type, "host") ?
						PURPLE_MEDIA_CANDIDATE_TYPE_HOST :
						!strcmp(candidate->type, "srflx") ?
							PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX :
							!strcmp(candidate->type, "prflx") ?
								PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX :
								!strcmp(candidate->type, "relay") ?
									PURPLE_MEDIA_CANDIDATE_TYPE_RELAY : 0,
					PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
					candidate->ip, candidate->port);
		g_object_set(new_candidate,
		             "base-ip", candidate->reladdr,
		             "base-port", candidate->relport,
		             "username", candidate->username,
		             "password", candidate->password,
		             "priority", candidate->priority,
		             NULL);
		ret = g_list_append(ret, new_candidate);
	}

	return ret;
}

static JingleIceUdpCandidate *
jingle_iceudp_get_remote_candidate_by_id(JingleIceUdp *iceudp,
		const gchar *id)
{
	GList *iter = iceudp->priv->remote_candidates;
	for (; iter; iter = g_list_next(iter)) {
		JingleIceUdpCandidate *candidate = iter->data;
		if (!strcmp(candidate->id, id)) {
			return candidate;
		}
	}
	return NULL;
}

static void
jingle_iceudp_add_remote_candidate(JingleIceUdp *iceudp, JingleIceUdpCandidate *candidate)
{
	JingleIceUdpPrivate *priv = JINGLE_ICEUDP_GET_PRIVATE(iceudp);
	JingleIceUdpCandidate *iceudp_candidate =
			jingle_iceudp_get_remote_candidate_by_id(iceudp,
					candidate->id);
	if (iceudp_candidate != NULL) {
		priv->remote_candidates = g_list_remove(
				priv->remote_candidates, iceudp_candidate);
		g_boxed_free(JINGLE_TYPE_ICEUDP_CANDIDATE, iceudp_candidate);
	}
	priv->remote_candidates = g_list_append(priv->remote_candidates, candidate);

	g_object_notify_by_pspec(G_OBJECT(iceudp), properties[PROP_REMOTE_CANDIDATES]);
}

static JingleTransport *
jingle_iceudp_parse_internal(PurpleXmlNode *iceudp)
{
	JingleTransport *transport = parent_class->parse(iceudp);
	PurpleXmlNode *candidate = purple_xmlnode_get_child(iceudp, "candidate");
	JingleIceUdpCandidate *iceudp_candidate = NULL;

	const gchar *username = purple_xmlnode_get_attrib(iceudp, "ufrag");
	const gchar *password = purple_xmlnode_get_attrib(iceudp, "pwd");

	for (; candidate; candidate = purple_xmlnode_get_next_twin(candidate)) {
		const gchar *relport = purple_xmlnode_get_attrib(candidate, "rel-port");
		const gchar *component = purple_xmlnode_get_attrib(candidate, "component");
		const gchar *foundation = purple_xmlnode_get_attrib(candidate, "foundation");
		const gchar *generation = purple_xmlnode_get_attrib(candidate, "generation");
		const gchar *id = purple_xmlnode_get_attrib(candidate, "id");
		const gchar *ip = purple_xmlnode_get_attrib(candidate, "ip");
		const gchar *network = purple_xmlnode_get_attrib(candidate, "network");
		const gchar *port = purple_xmlnode_get_attrib(candidate, "port");
		const gchar *priority = purple_xmlnode_get_attrib(candidate, "priority");
		const gchar *protocol = purple_xmlnode_get_attrib(candidate, "protocol");
		const gchar *type = purple_xmlnode_get_attrib(candidate, "type");

		if (!component || !foundation || !generation || !id || !ip ||
				!network || !port || !priority || !protocol || !type)
			continue;

		iceudp_candidate = jingle_iceudp_candidate_new(
				id,
				atoi(component),
				foundation,
				atoi(generation),
				ip,
				atoi(network),
				atoi(port),
				atoi(priority),
				protocol,
				type,
				username, password);
		iceudp_candidate->reladdr = g_strdup(
				purple_xmlnode_get_attrib(candidate, "rel-addr"));
		iceudp_candidate->relport =
				relport != NULL ? atoi(relport) : 0;
		iceudp_candidate->rem_known = TRUE;
		jingle_iceudp_add_remote_candidate(JINGLE_ICEUDP(transport), iceudp_candidate);
	}

	return transport;
}

static PurpleXmlNode *
jingle_iceudp_to_xml_internal(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action)
{
	PurpleXmlNode *node = parent_class->to_xml(transport, content, action);

	if (action == JINGLE_SESSION_INITIATE ||
			action == JINGLE_SESSION_ACCEPT ||
			action == JINGLE_TRANSPORT_INFO ||
			action == JINGLE_CONTENT_ADD ||
			action == JINGLE_TRANSPORT_REPLACE) {
		JingleIceUdpPrivate *priv = JINGLE_ICEUDP_GET_PRIVATE(transport);
		GList *iter = priv->local_candidates;
		gboolean used_candidate = FALSE;

		for (; iter; iter = g_list_next(iter)) {
			JingleIceUdpCandidate *candidate = iter->data;
			PurpleXmlNode *xmltransport;
			gchar *component, *generation, *network,
					*port, *priority;

			if (candidate->rem_known == TRUE)
				continue;

			used_candidate = TRUE;
			candidate->rem_known = TRUE;

			xmltransport = purple_xmlnode_new_child(node, "candidate");
			component = g_strdup_printf("%d", candidate->component);
			generation = g_strdup_printf("%d",
					candidate->generation);
			network = g_strdup_printf("%d", candidate->network);
			port = g_strdup_printf("%d", candidate->port);
			priority = g_strdup_printf("%d", candidate->priority);

			purple_xmlnode_set_attrib(xmltransport, "component", component);
			purple_xmlnode_set_attrib(xmltransport, "foundation", candidate->foundation);
			purple_xmlnode_set_attrib(xmltransport, "generation", generation);
			purple_xmlnode_set_attrib(xmltransport, "id", candidate->id);
			purple_xmlnode_set_attrib(xmltransport, "ip", candidate->ip);
			purple_xmlnode_set_attrib(xmltransport, "network", network);
			purple_xmlnode_set_attrib(xmltransport, "port", port);
			purple_xmlnode_set_attrib(xmltransport, "priority", priority);
			purple_xmlnode_set_attrib(xmltransport, "protocol", candidate->protocol);

			if (candidate->reladdr != NULL &&
					(strcmp(candidate->ip, candidate->reladdr) ||
					(candidate->port != candidate->relport))) {
				gchar *relport = g_strdup_printf("%d",
						candidate->relport);
				purple_xmlnode_set_attrib(xmltransport, "rel-addr",
						candidate->reladdr);
				purple_xmlnode_set_attrib(xmltransport, "rel-port",
						relport);
				g_free(relport);
			}

			purple_xmlnode_set_attrib(xmltransport, "type", candidate->type);

			g_free(component);
			g_free(generation);
			g_free(network);
			g_free(port);
			g_free(priority);
		}

		if (used_candidate == TRUE) {
			JingleIceUdpCandidate *candidate =
					priv->local_candidates->data;
			purple_xmlnode_set_attrib(node, "pwd", candidate->password);
			purple_xmlnode_set_attrib(node, "ufrag", candidate->username);
		}
	}

	return node;
}

