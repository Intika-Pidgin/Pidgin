/**
 * @file rawudp.c
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

#include "rawudp.h"
#include "jingle.h"
#include "debug.h"

#include <string.h>

struct _JingleRawUdp
{
	JingleTransport parent;
};

typedef struct
{
	GList *local_candidates;
	GList *remote_candidates;
} JingleRawUdpPrivate;

enum {
	PROP_0,
	PROP_LOCAL_CANDIDATES,
	PROP_REMOTE_CANDIDATES,
	PROP_LAST
};
static GParamSpec *properties[PROP_LAST];

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
	JingleRawUdp,
	jingle_rawudp,
	JINGLE_TYPE_TRANSPORT,
	0,
	G_ADD_PRIVATE_DYNAMIC(JingleRawUdp)
);

/******************************************************************************
 * JingleRawUdp Transport Implementation
 *****************************************************************************/
static GList *
jingle_rawudp_get_remote_candidates(JingleTransport *transport)
{
	JingleRawUdp *rawudp = JINGLE_RAWUDP(transport);
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(rawudp);
	GList *candidates = priv->remote_candidates;
	GList *ret = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		JingleRawUdpCandidate *candidate = candidates->data;
		ret = g_list_append(ret, purple_media_candidate_new("",
					candidate->component,
					PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX,
					PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
					candidate->ip, candidate->port));
	}

	return ret;
}

static JingleRawUdpCandidate *
jingle_rawudp_get_remote_candidate_by_id(JingleRawUdp *rawudp, gchar *id)
{
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(rawudp);

	GList *iter = priv->remote_candidates;
	for (; iter; iter = g_list_next(iter)) {
		JingleRawUdpCandidate *candidate = iter->data;
		if (purple_strequal(candidate->id, id)) {
			return candidate;
		}
	}
	return NULL;
}

static void
jingle_rawudp_add_remote_candidate(JingleRawUdp *rawudp, JingleRawUdpCandidate *candidate)
{
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(rawudp);
	JingleRawUdpCandidate *rawudp_candidate =
			jingle_rawudp_get_remote_candidate_by_id(rawudp, candidate->id);
	if (rawudp_candidate != NULL) {
		priv->remote_candidates = g_list_remove(
				priv->remote_candidates, rawudp_candidate);
		g_boxed_free(JINGLE_TYPE_RAWUDP_CANDIDATE, rawudp_candidate);
	}
	priv->remote_candidates = g_list_append(priv->remote_candidates, candidate);

	g_object_notify_by_pspec(G_OBJECT(rawudp), properties[PROP_REMOTE_CANDIDATES]);
}

static PurpleXmlNode *
jingle_rawudp_to_xml_internal(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action)
{
	PurpleXmlNode *node = JINGLE_TRANSPORT_CLASS(jingle_rawudp_parent_class)->to_xml(transport, content, action);

	if (action == JINGLE_SESSION_INITIATE ||
			action == JINGLE_TRANSPORT_INFO ||
			action == JINGLE_SESSION_ACCEPT) {
		JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(JINGLE_RAWUDP(transport));
		GList *iter = priv->local_candidates;

		for (; iter; iter = g_list_next(iter)) {
			JingleRawUdpCandidate *candidate = iter->data;
			PurpleXmlNode *xmltransport;
			gchar *generation, *component, *port;

			if (candidate->rem_known == TRUE)
				continue;
			candidate->rem_known = TRUE;

			xmltransport = purple_xmlnode_new_child(node, "candidate");
			generation = g_strdup_printf("%d", candidate->generation);
			component = g_strdup_printf("%d", candidate->component);
			port = g_strdup_printf("%d", candidate->port);

			purple_xmlnode_set_attrib(xmltransport, "generation", generation);
			purple_xmlnode_set_attrib(xmltransport, "component", component);
			purple_xmlnode_set_attrib(xmltransport, "id", candidate->id);
			purple_xmlnode_set_attrib(xmltransport, "ip", candidate->ip);
			purple_xmlnode_set_attrib(xmltransport, "port", port);

			g_free(port);
			g_free(generation);
		}
	}

	return node;
}

static JingleTransport *
jingle_rawudp_parse_internal(PurpleXmlNode *rawudp)
{
	JingleTransport *transport = JINGLE_TRANSPORT_CLASS(jingle_rawudp_parent_class)->parse(rawudp);
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(JINGLE_RAWUDP(transport));
	PurpleXmlNode *candidate = purple_xmlnode_get_child(rawudp, "candidate");
	JingleRawUdpCandidate *rawudp_candidate = NULL;

	for (; candidate; candidate = purple_xmlnode_get_next_twin(candidate)) {
		const gchar *id = purple_xmlnode_get_attrib(candidate, "id");
		const gchar *generation = purple_xmlnode_get_attrib(candidate, "generation");
		const gchar *component = purple_xmlnode_get_attrib(candidate, "component");
		const gchar *ip = purple_xmlnode_get_attrib(candidate, "ip");
		const gchar *port = purple_xmlnode_get_attrib(candidate, "port");

		if (!id || !generation || !component || !ip || !port)
			continue;

		rawudp_candidate = jingle_rawudp_candidate_new(
				id,
				atoi(generation),
				atoi(component),
				ip,
				atoi(port));
		rawudp_candidate->rem_known = TRUE;
		jingle_rawudp_add_remote_candidate(JINGLE_RAWUDP(transport), rawudp_candidate);
	}

	if (rawudp_candidate != NULL &&
			g_list_length(priv->remote_candidates) == 1) {
		/* manufacture rtcp candidate */
		rawudp_candidate = g_boxed_copy(JINGLE_TYPE_RAWUDP_CANDIDATE, rawudp_candidate);
		rawudp_candidate->component = 2;
		rawudp_candidate->port = rawudp_candidate->port + 1;
		rawudp_candidate->rem_known = TRUE;
		jingle_rawudp_add_remote_candidate(JINGLE_RAWUDP(transport), rawudp_candidate);
	}

	return transport;
}

static void
jingle_rawudp_add_local_candidate(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate)
{
	JingleRawUdp *rawudp = JINGLE_RAWUDP(transport);
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(rawudp);
	gchar *ip;
	JingleRawUdpCandidate *rawudp_candidate;
	GList *iter;

	ip = purple_media_candidate_get_ip(candidate);
	rawudp_candidate = jingle_rawudp_candidate_new(id, generation,
			purple_media_candidate_get_component_id(candidate),
			ip, purple_media_candidate_get_port(candidate));
	g_free(ip);

	for (iter = priv->local_candidates; iter; iter = g_list_next(iter)) {
		JingleRawUdpCandidate *c = iter->data;
		if (purple_strequal(c->id, id)) {
			generation = c->generation + 1;

			g_boxed_free(JINGLE_TYPE_RAWUDP_CANDIDATE, c);
			priv->local_candidates = g_list_delete_link(
					priv->local_candidates, iter);

			rawudp_candidate->generation = generation;

			priv->local_candidates = g_list_append(
					priv->local_candidates, rawudp_candidate);

			g_object_notify_by_pspec(G_OBJECT(rawudp), properties[PROP_LOCAL_CANDIDATES]);

			return;
		}
	}

	priv->local_candidates = g_list_append(
			priv->local_candidates, rawudp_candidate);

	g_object_notify_by_pspec(G_OBJECT(rawudp), properties[PROP_LOCAL_CANDIDATES]);
}

/******************************************************************************
 * JingleRawUdp GObject Stuff
 *****************************************************************************/
static void
jingle_rawudp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	JingleRawUdp *rawudp = JINGLE_RAWUDP(object);
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(rawudp);

	switch (prop_id) {
		case PROP_LOCAL_CANDIDATES:
			priv->local_candidates = g_value_get_pointer(value);
			break;
		case PROP_REMOTE_CANDIDATES:
			priv->remote_candidates = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_rawudp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	JingleRawUdp *rawudp = JINGLE_RAWUDP(object);
	JingleRawUdpPrivate *priv = jingle_rawudp_get_instance_private(rawudp);

	switch (prop_id) {
		case PROP_LOCAL_CANDIDATES:
			g_value_set_pointer(value, priv->local_candidates);
			break;
		case PROP_REMOTE_CANDIDATES:
			g_value_set_pointer(value, priv->remote_candidates);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_rawudp_init (JingleRawUdp *rawudp)
{
}

static void
jingle_rawudp_finalize (GObject *rawudp)
{
/*	JingleRawUdpPrivate *priv = JINGLE_RAWUDP_GET_PRIVATE(rawudp); */
	purple_debug_info("jingle","jingle_rawudp_finalize\n");

	G_OBJECT_CLASS(jingle_rawudp_parent_class)->finalize(rawudp);
}

static void
jingle_rawudp_class_finalize(JingleRawUdpClass *klass) {
}

static void
jingle_rawudp_class_init (JingleRawUdpClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	JingleTransportClass *transport_class = JINGLE_TRANSPORT_CLASS(klass);

	obj_class->finalize = jingle_rawudp_finalize;
	obj_class->set_property = jingle_rawudp_set_property;
	obj_class->get_property = jingle_rawudp_get_property;

	transport_class->to_xml = jingle_rawudp_to_xml_internal;
	transport_class->parse = jingle_rawudp_parse_internal;
	transport_class->transport_type = JINGLE_TRANSPORT_RAWUDP;
	transport_class->add_local_candidate = jingle_rawudp_add_local_candidate;
	transport_class->get_remote_candidates = jingle_rawudp_get_remote_candidates;

	properties[PROP_LOCAL_CANDIDATES] = g_param_spec_pointer("local-candidates",
			"Local candidates",
			"The local candidates for this transport.",
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_REMOTE_CANDIDATES] = g_param_spec_pointer("remote-candidates",
			"Remote candidates",
			"The remote candidates for this transport.",
			G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

/******************************************************************************
 * JingleRawUdpCandidate Boxed Type
 *****************************************************************************/
static JingleRawUdpCandidate *
jingle_rawudp_candidate_copy(JingleRawUdpCandidate *candidate)
{
	JingleRawUdpCandidate *new_candidate = g_new0(JingleRawUdpCandidate, 1);
	new_candidate->generation = candidate->generation;
	new_candidate->component = candidate->component;
	new_candidate->id = g_strdup(candidate->id);
	new_candidate->ip = g_strdup(candidate->ip);
	new_candidate->port = candidate->port;

	new_candidate->rem_known = candidate->rem_known;
	return new_candidate;
}

static void
jingle_rawudp_candidate_free(JingleRawUdpCandidate *candidate)
{
	g_free(candidate->id);
	g_free(candidate->ip);
}

G_DEFINE_BOXED_TYPE(JingleRawUdpCandidate, jingle_rawudp_candidate,
		jingle_rawudp_candidate_copy, jingle_rawudp_candidate_free)

/******************************************************************************
 * Public API
 *****************************************************************************/
void
jingle_rawudp_register(PurplePlugin *plugin) {
	jingle_rawudp_register_type(G_TYPE_MODULE(plugin));
}

JingleRawUdpCandidate *
jingle_rawudp_candidate_new(const gchar *id, guint generation, guint component, const gchar *ip, guint port)
{
	JingleRawUdpCandidate *candidate = g_new0(JingleRawUdpCandidate, 1);
	candidate->generation = generation;
	candidate->component = component;
	candidate->id = g_strdup(id);
	candidate->ip = g_strdup(ip);
	candidate->port = port;

	candidate->rem_known = FALSE;
	return candidate;
}
