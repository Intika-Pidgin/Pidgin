/**
 * @file transport.c
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

#include "transport.h"
#include "jingle.h"
#include "debug.h"

#include <string.h>

G_DEFINE_DYNAMIC_TYPE(JingleTransport, jingle_transport, G_TYPE_OBJECT);

/******************************************************************************
 * Transport Implementation
 *****************************************************************************/
static JingleTransport *
jingle_transport_parse_internal(PurpleXmlNode *transport)
{
	const gchar *type = purple_xmlnode_get_namespace(transport);
	return jingle_transport_create(type);
}

static void
jingle_transport_add_local_candidate_internal(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate)
{
	/* Nothing to do */
}

static PurpleXmlNode *
jingle_transport_to_xml_internal(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action)
{
	PurpleXmlNode *node = purple_xmlnode_new_child(content, "transport");
	purple_xmlnode_set_namespace(node, jingle_transport_get_transport_type(transport));
	return node;
}

static GList *
jingle_transport_get_remote_candidates_internal(JingleTransport *transport)
{
	return NULL;
}

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/
static void
jingle_transport_init (JingleTransport *transport)
{
}

static void
jingle_transport_class_finalize (JingleTransportClass *klass)
{
}

static void
jingle_transport_class_init (JingleTransportClass *klass)
{
	klass->to_xml = jingle_transport_to_xml_internal;
	klass->parse = jingle_transport_parse_internal;
	klass->add_local_candidate = jingle_transport_add_local_candidate_internal;
	klass->get_remote_candidates = jingle_transport_get_remote_candidates_internal;
}

/******************************************************************************
 * Public API
 *****************************************************************************/
void
jingle_transport_register(PurplePlugin *plugin) {
	jingle_transport_register_type(G_TYPE_MODULE(plugin));
}

JingleTransport *
jingle_transport_create(const gchar *type)
{
	return g_object_new(jingle_get_type(type), NULL);
}

const gchar *
jingle_transport_get_transport_type(JingleTransport *transport)
{
	return JINGLE_TRANSPORT_GET_CLASS(transport)->transport_type;
}

void
jingle_transport_add_local_candidate(JingleTransport *transport, const gchar *id,
                                     guint generation, PurpleMediaCandidate *candidate)
{
	JINGLE_TRANSPORT_GET_CLASS(transport)->add_local_candidate(transport, id,
	                                                           generation, candidate);
}

GList *
jingle_transport_get_remote_candidates(JingleTransport *transport)
{
	return JINGLE_TRANSPORT_GET_CLASS(transport)->get_remote_candidates(transport);
}

JingleTransport *
jingle_transport_parse(PurpleXmlNode *transport)
{
	const gchar *type_name = purple_xmlnode_get_namespace(transport);
	GType type = jingle_get_type(type_name);
	if (type == G_TYPE_NONE)
		return NULL;

	return JINGLE_TRANSPORT_CLASS(g_type_class_ref(type))->parse(transport);
}

PurpleXmlNode *
jingle_transport_to_xml(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action)
{
	g_return_val_if_fail(transport != NULL, NULL);
	g_return_val_if_fail(JINGLE_IS_TRANSPORT(transport), NULL);
	return JINGLE_TRANSPORT_GET_CLASS(transport)->to_xml(transport, content, action);
}
