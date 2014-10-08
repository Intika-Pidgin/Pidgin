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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"

#include "debug.h"
#include "caps.h"
#include "iq.h"
#include "presence.h"
#include "util.h"
#include "xdata.h"

#include "ciphers/md5hash.h"
#include "ciphers/sha1hash.h"

#define JABBER_CAPS_FILENAME "xmpp-caps.xml"

typedef struct _JabberDataFormField {
	gchar *var;
	GList *values;
} JabberDataFormField;

static GHashTable *capstable = NULL; /* JabberCapsTuple -> JabberCapsClientInfo */
static GHashTable *nodetable = NULL; /* char *node -> JabberCapsNodeExts */
static guint       save_timer = 0;

/* Free a GList of allocated char* */
static void
free_string_glist(GList *list)
{
	while (list) {
		g_free(list->data);
		list = g_list_delete_link(list, list);
	}
}

static JabberCapsNodeExts*
jabber_caps_node_exts_ref(JabberCapsNodeExts *exts)
{
	g_return_val_if_fail(exts != NULL, NULL);

	++exts->ref;
	return exts;
}

static void
jabber_caps_node_exts_unref(JabberCapsNodeExts *exts)
{
	if (exts == NULL)
		return;

	g_return_if_fail(exts->ref != 0);

	if (--exts->ref != 0)
		return;

	g_hash_table_destroy(exts->exts);
	g_free(exts);
}

static guint jabber_caps_hash(gconstpointer data) {
	const JabberCapsTuple *key = data;
	guint nodehash = g_str_hash(key->node);
	guint verhash  = g_str_hash(key->ver);
	/*
	 * 'hash' was optional in XEP-0115 v1.4 and g_str_hash crashes on NULL >:O.
	 * Okay, maybe I've played too much Zelda, but that looks like
	 * a Deku Shrub...
	 */
	guint hashhash = (key->hash ? g_str_hash(key->hash) : 0);
	return nodehash ^ verhash ^ hashhash;
}

static gboolean jabber_caps_compare(gconstpointer v1, gconstpointer v2) {
	const JabberCapsTuple *name1 = v1;
	const JabberCapsTuple *name2 = v2;

	return g_str_equal(name1->node, name2->node) &&
	       g_str_equal(name1->ver, name2->ver) &&
	       purple_strequal(name1->hash, name2->hash);
}

static void
jabber_caps_client_info_destroy(JabberCapsClientInfo *info)
{
	if (info == NULL)
		return;

	while(info->identities) {
		JabberIdentity *id = info->identities->data;
		g_free(id->category);
		g_free(id->type);
		g_free(id->name);
		g_free(id->lang);
		g_free(id);
		info->identities = g_list_delete_link(info->identities, info->identities);
	}

	free_string_glist(info->features);

	while (info->forms) {
		purple_xmlnode_free(info->forms->data);
		info->forms = g_list_delete_link(info->forms, info->forms);
	}

	jabber_caps_node_exts_unref(info->exts);

	g_free((char *)info->tuple.node);
	g_free((char *)info->tuple.ver);
	g_free((char *)info->tuple.hash);

	g_free(info);
}

/* NOTE: Takes a reference to the exts, unref it if you don't really want to
 * keep it around. */
static JabberCapsNodeExts*
jabber_caps_find_exts_by_node(const char *node)
{
	JabberCapsNodeExts *exts;
	if (NULL == (exts = g_hash_table_lookup(nodetable, node))) {
		exts = g_new0(JabberCapsNodeExts, 1);
		exts->exts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
		                                   (GDestroyNotify)free_string_glist);
		g_hash_table_insert(nodetable, g_strdup(node), jabber_caps_node_exts_ref(exts));
	}

	return jabber_caps_node_exts_ref(exts);
}

static void
exts_to_xmlnode(gconstpointer key, gconstpointer value, gpointer user_data)
{
	const char *identifier = key;
	const GList *features = value, *node;
	PurpleXmlNode *client = user_data, *ext, *feature;

	ext = purple_xmlnode_new_child(client, "ext");
	purple_xmlnode_set_attrib(ext, "identifier", identifier);

	for (node = features; node; node = node->next) {
		feature = purple_xmlnode_new_child(ext, "feature");
		purple_xmlnode_set_attrib(feature, "var", (const gchar *)node->data);
	}
}

static void jabber_caps_store_client(gpointer key, gpointer value, gpointer user_data) {
	const JabberCapsTuple *tuple = key;
	const JabberCapsClientInfo *props = value;
	PurpleXmlNode *root = user_data;
	PurpleXmlNode *client = purple_xmlnode_new_child(root, "client");
	GList *iter;

	purple_xmlnode_set_attrib(client, "node", tuple->node);
	purple_xmlnode_set_attrib(client, "ver", tuple->ver);
	if (tuple->hash)
		purple_xmlnode_set_attrib(client, "hash", tuple->hash);
	for(iter = props->identities; iter; iter = g_list_next(iter)) {
		JabberIdentity *id = iter->data;
		PurpleXmlNode *identity = purple_xmlnode_new_child(client, "identity");
		purple_xmlnode_set_attrib(identity, "category", id->category);
		purple_xmlnode_set_attrib(identity, "type", id->type);
		if (id->name)
			purple_xmlnode_set_attrib(identity, "name", id->name);
		if (id->lang)
			purple_xmlnode_set_attrib(identity, "lang", id->lang);
	}

	for(iter = props->features; iter; iter = g_list_next(iter)) {
		const char *feat = iter->data;
		PurpleXmlNode *feature = purple_xmlnode_new_child(client, "feature");
		purple_xmlnode_set_attrib(feature, "var", feat);
	}

	for(iter = props->forms; iter; iter = g_list_next(iter)) {
		/* FIXME: See #7814 */
		PurpleXmlNode *xdata = iter->data;
		purple_xmlnode_insert_child(client, purple_xmlnode_copy(xdata));
	}

	/* TODO: Ideally, only save this once-per-node... */
	if (props->exts)
		g_hash_table_foreach(props->exts->exts, (GHFunc)exts_to_xmlnode, client);
}

static gboolean
do_jabber_caps_store(gpointer data)
{
	char *str;
	int length = 0;
	PurpleXmlNode *root = purple_xmlnode_new("capabilities");
	g_hash_table_foreach(capstable, jabber_caps_store_client, root);
	str = purple_xmlnode_to_formatted_str(root, &length);
	purple_xmlnode_free(root);
	purple_util_write_data_to_file(JABBER_CAPS_FILENAME, str, length);
	g_free(str);

	save_timer = 0;
	return FALSE;
}

static void
schedule_caps_save(void)
{
	if (save_timer == 0)
		save_timer = purple_timeout_add_seconds(5, do_jabber_caps_store, NULL);
}

static void
jabber_caps_load(void)
{
	PurpleXmlNode *capsdata = purple_util_read_xml_from_file(JABBER_CAPS_FILENAME, "XMPP capabilities cache");
	PurpleXmlNode *client;

	if(!capsdata)
		return;

	if (!g_str_equal(capsdata->name, "capabilities")) {
		purple_xmlnode_free(capsdata);
		return;
	}

	for (client = capsdata->child; client; client = client->next) {
		if (client->type != PURPLE_XMLNODE_TYPE_TAG)
			continue;
		if (g_str_equal(client->name, "client")) {
			JabberCapsClientInfo *value = g_new0(JabberCapsClientInfo, 1);
			JabberCapsTuple *key = (JabberCapsTuple*)&value->tuple;
			PurpleXmlNode *child;
			JabberCapsNodeExts *exts = NULL;
			key->node = g_strdup(purple_xmlnode_get_attrib(client,"node"));
			key->ver  = g_strdup(purple_xmlnode_get_attrib(client,"ver"));
			key->hash = g_strdup(purple_xmlnode_get_attrib(client,"hash"));

			/* v1.3 capabilities */
			if (key->hash == NULL)
				exts = jabber_caps_find_exts_by_node(key->node);

			for (child = client->child; child; child = child->next) {
				if (child->type != PURPLE_XMLNODE_TYPE_TAG)
					continue;
				if (g_str_equal(child->name, "feature")) {
					const char *var = purple_xmlnode_get_attrib(child, "var");
					if(!var)
						continue;
					value->features = g_list_append(value->features,g_strdup(var));
				} else if (g_str_equal(child->name, "identity")) {
					const char *category = purple_xmlnode_get_attrib(child, "category");
					const char *type = purple_xmlnode_get_attrib(child, "type");
					const char *name = purple_xmlnode_get_attrib(child, "name");
					const char *lang = purple_xmlnode_get_attrib(child, "lang");
					JabberIdentity *id;

					if (!category || !type)
						continue;

					id = g_new0(JabberIdentity, 1);
					id->category = g_strdup(category);
					id->type = g_strdup(type);
					id->name = g_strdup(name);
					id->lang = g_strdup(lang);

					value->identities = g_list_append(value->identities,id);
				} else if (g_str_equal(child->name, "x")) {
					/* TODO: See #7814 -- this might cause problems if anyone
					 * ever actually specifies forms. In fact, for this to
					 * work properly, that bug needs to be fixed in
					 * purple_xmlnode_from_str, not the output version... */
					value->forms = g_list_append(value->forms, purple_xmlnode_copy(child));
				} else if (g_str_equal(child->name, "ext")) {
					if (key->hash != NULL)
						purple_debug_warning("jabber", "Ignoring exts when reading new-style caps\n");
					else {
						/* TODO: Do we care about reading in the identities listed here? */
						const char *identifier = purple_xmlnode_get_attrib(child, "identifier");
						PurpleXmlNode *node;
						GList *features = NULL;

						if (!identifier)
							continue;

						for (node = child->child; node; node = node->next) {
							if (node->type != PURPLE_XMLNODE_TYPE_TAG)
								continue;
							if (g_str_equal(node->name, "feature")) {
								const char *var = purple_xmlnode_get_attrib(node, "var");
								if (!var)
									continue;
								features = g_list_prepend(features, g_strdup(var));
							}
						}

						if (features) {
							g_hash_table_insert(exts->exts, g_strdup(identifier),
							                    features);
						} else
							purple_debug_warning("jabber", "Caps ext %s had no features.\n",
							                     identifier);
					}
				}
			}

			value->exts = exts;
			g_hash_table_replace(capstable, key, value);

		}
	}
	purple_xmlnode_free(capsdata);
}

void jabber_caps_init(void)
{
	nodetable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)jabber_caps_node_exts_unref);
	capstable = g_hash_table_new_full(jabber_caps_hash, jabber_caps_compare, NULL, (GDestroyNotify)jabber_caps_client_info_destroy);
	jabber_caps_load();
}

void jabber_caps_uninit(void)
{
	if (save_timer != 0) {
		purple_timeout_remove(save_timer);
		save_timer = 0;
		do_jabber_caps_store(NULL);
	}
	g_hash_table_destroy(capstable);
	g_hash_table_destroy(nodetable);
	capstable = nodetable = NULL;
}

gboolean jabber_caps_exts_known(const JabberCapsClientInfo *info,
                                char **exts)
{
	int i;
	g_return_val_if_fail(info != NULL, FALSE);

	if (!exts)
		return TRUE;

	for (i = 0; exts[i]; ++i) {
		/* Hack since we advertise the ext along with v1.5 caps but don't
		 * store any exts */
		if (g_str_equal(exts[i], "voice-v1") && !info->exts)
			continue;
		if (!info->exts ||
				!g_hash_table_lookup(info->exts->exts, exts[i]))
			return FALSE;
	}

	return TRUE;
}

typedef struct _jabber_caps_cbplususerdata {
	guint ref;

	jabber_caps_get_info_cb cb;
	gpointer cb_data;

	char *who;
	char *node;
	char *ver;
	char *hash;

	JabberCapsClientInfo *info;

	GList *exts;
	guint extOutstanding;
	JabberCapsNodeExts *node_exts;
} jabber_caps_cbplususerdata;

static jabber_caps_cbplususerdata*
cbplususerdata_ref(jabber_caps_cbplususerdata *data)
{
	g_return_val_if_fail(data != NULL, NULL);

	++data->ref;
	return data;
}

static void
cbplususerdata_unref(jabber_caps_cbplususerdata *data)
{
	if (data == NULL)
		return;

	g_return_if_fail(data->ref != 0);

	if (--data->ref > 0)
		return;

	g_free(data->who);
	g_free(data->node);
	g_free(data->ver);
	g_free(data->hash);

	/* If we have info here, it's already in the capstable, so don't free it */
	if (data->exts)
		free_string_glist(data->exts);
	if (data->node_exts)
		jabber_caps_node_exts_unref(data->node_exts);
	g_free(data);
}

static void
jabber_caps_get_info_complete(jabber_caps_cbplususerdata *userdata)
{
	if (userdata->cb) {
		userdata->cb(userdata->info, userdata->exts, userdata->cb_data);
		userdata->info = NULL;
		userdata->exts = NULL;
	}

	if (userdata->ref != 1)
		purple_debug_warning("jabber", "Lost a reference to caps cbdata: %d\n",
		                     userdata->ref);
}

static void
jabber_caps_client_iqcb(JabberStream *js, const char *from, JabberIqType type,
                        const char *id, PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query = purple_xmlnode_get_child_with_namespace(packet, "query",
		NS_DISCO_INFO);
	jabber_caps_cbplususerdata *userdata = data;
	JabberCapsClientInfo *info = NULL, *value;
	JabberCapsTuple key;

	if (!query || type == JABBER_IQ_ERROR) {
		/* Any outstanding exts will be dealt with via ref-counting */
		userdata->cb(NULL, NULL, userdata->cb_data);
		cbplususerdata_unref(userdata);
		return;
	}

	/* check hash */
	info = jabber_caps_parse_client_info(query);

	/* Only validate if these are v1.5 capabilities */
	if (userdata->hash) {
		gchar *hash = NULL;
		PurpleHash *hasher = NULL;
		/*
		 * TODO: If you add *any* hash here, make sure the checksum buffer
		 * size in jabber_caps_calculate_hash is large enough. The cipher API
		 * doesn't seem to offer a "Get the hash size" function(?).
		 */
		if (g_str_equal(userdata->hash, "sha-1")) {
			hasher = purple_sha1_hash_new();
		} else if (g_str_equal(userdata->hash, "md5")) {
			hasher = purple_md5_hash_new();
		}
		hash = jabber_caps_calculate_hash(info, hasher);
		g_object_unref(hasher);

		if (!hash || !g_str_equal(hash, userdata->ver)) {
			purple_debug_warning("jabber", "Could not validate caps info from "
			                     "%s. Expected %s, got %s\n",
			                     purple_xmlnode_get_attrib(packet, "from"),
			                     userdata->ver, hash ? hash : "(null)");

			userdata->cb(NULL, NULL, userdata->cb_data);
			jabber_caps_client_info_destroy(info);
			cbplususerdata_unref(userdata);
			g_free(hash);
			return;
		}

		g_free(hash);
	}

	if (!userdata->hash && userdata->node_exts) {
		/* If the ClientInfo doesn't have information about the exts, give them
		 * ours (along with our ref) */
		info->exts = userdata->node_exts;
		userdata->node_exts = NULL;
	}

	key.node = userdata->node;
	key.ver  = userdata->ver;
	key.hash = userdata->hash;

	/* Use the copy of this data already in the table if it exists or insert
	 * a new one if we need to */
	if ((value = g_hash_table_lookup(capstable, &key))) {
		jabber_caps_client_info_destroy(info);
		info = value;
	} else {
		JabberCapsTuple *n_key = (JabberCapsTuple *)&info->tuple;

		if (G_UNLIKELY(n_key == NULL)) {
			g_warn_if_reached();
			jabber_caps_client_info_destroy(info);
			return;
		}

		n_key->node = userdata->node;
		n_key->ver  = userdata->ver;
		n_key->hash = userdata->hash;
		userdata->node = userdata->ver = userdata->hash = NULL;

		/* The capstable gets a reference */
		g_hash_table_insert(capstable, n_key, info);
		schedule_caps_save();
	}

	userdata->info = info;

	if (userdata->extOutstanding == 0)
		jabber_caps_get_info_complete(userdata);

	cbplususerdata_unref(userdata);
}

typedef struct {
	const char *name;
	jabber_caps_cbplususerdata *data;
} ext_iq_data;

static void
jabber_caps_ext_iqcb(JabberStream *js, const char *from, JabberIqType type,
                     const char *id, PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query = purple_xmlnode_get_child_with_namespace(packet, "query",
		NS_DISCO_INFO);
	PurpleXmlNode *child;
	ext_iq_data *userdata = data;
	GList *features = NULL;
	JabberCapsNodeExts *node_exts;

	if (!query || type == JABBER_IQ_ERROR) {
		cbplususerdata_unref(userdata->data);
		g_free(userdata);
		return;
	}

	node_exts = (userdata->data->info ? userdata->data->info->exts :
	                                    userdata->data->node_exts);

	/* TODO: I don't see how this can actually happen, but it crashed khc. */
	if (!node_exts) {
		purple_debug_error("jabber", "Couldn't find JabberCapsNodeExts. If you "
				"see this, please tell darkrain42 and save your debug log.\n"
				"JabberCapsClientInfo = %p\n", userdata->data->info);


		/* Try once more to find the exts and then fail */
		node_exts = jabber_caps_find_exts_by_node(userdata->data->node);
		if (node_exts) {
			purple_debug_info("jabber", "Found the exts on the second try.\n");
			if (userdata->data->info)
				userdata->data->info->exts = node_exts;
			else
				userdata->data->node_exts = node_exts;
		} else {
			cbplususerdata_unref(userdata->data);
			g_free(userdata);
			g_return_if_reached();
		}
	}

	/* So, we decrement this after checking for an error, which means that
	 * if there *is* an error, we'll never call the callback passed to
	 * jabber_caps_get_info. We will still free all of our data, though.
	 */
	--userdata->data->extOutstanding;

	for (child = purple_xmlnode_get_child(query, "feature"); child;
	        child = purple_xmlnode_get_next_twin(child)) {
		const char *var = purple_xmlnode_get_attrib(child, "var");
		if (var)
			features = g_list_prepend(features, g_strdup(var));
	}

	g_hash_table_insert(node_exts->exts, g_strdup(userdata->name), features);
	schedule_caps_save();

	/* Are we done? */
	if (userdata->data->info && userdata->data->extOutstanding == 0)
		jabber_caps_get_info_complete(userdata->data);

	cbplususerdata_unref(userdata->data);
	g_free(userdata);
}

void jabber_caps_get_info(JabberStream *js, const char *who, const char *node,
        const char *ver, const char *hash, char **exts,
        jabber_caps_get_info_cb cb, gpointer user_data)
{
	JabberCapsClientInfo *info;
	JabberCapsTuple key;
	jabber_caps_cbplususerdata *userdata;

	if (exts && hash) {
		purple_debug_misc("jabber", "Ignoring exts in new-style caps from %s\n",
		                  who);
		g_strfreev(exts);
		exts = NULL;
	}

	/* Using this in a read-only fashion, so the cast is OK */
	key.node = (char *)node;
	key.ver = (char *)ver;
	key.hash = (char *)hash;

	info = g_hash_table_lookup(capstable, &key);
	if (info && hash) {
		/* v1.5 - We already have all the information we care about */
		if (cb)
			cb(info, NULL, user_data);
		return;
	}

	userdata = g_new0(jabber_caps_cbplususerdata, 1);
	/* We start out with 0 references. Every query takes one */
	userdata->cb = cb;
	userdata->cb_data = user_data;
	userdata->who = g_strdup(who);
	userdata->node = g_strdup(node);
	userdata->ver = g_strdup(ver);
	userdata->hash = g_strdup(hash);

	if (info) {
		userdata->info = info;
	} else {
		/* If we don't have the basic information about the client, we need
		 * to fetch it. */
		JabberIq *iq;
		PurpleXmlNode *query;
		char *nodever;

		iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_INFO);
		query = purple_xmlnode_get_child_with_namespace(iq->node, "query",
					NS_DISCO_INFO);
		nodever = g_strdup_printf("%s#%s", node, ver);
		purple_xmlnode_set_attrib(query, "node", nodever);
		g_free(nodever);
		purple_xmlnode_set_attrib(iq->node, "to", who);

		cbplususerdata_ref(userdata);

		jabber_iq_set_callback(iq, jabber_caps_client_iqcb, userdata);
		jabber_iq_send(iq);
	}

	/* Are there any exts that we don't recognize? */
	if (exts) {
		JabberCapsNodeExts *node_exts;
		int i;

		if (info) {
			if (info->exts)
				node_exts = info->exts;
			else
				node_exts = info->exts = jabber_caps_find_exts_by_node(node);
		} else
			/* We'll put it in later once we have the client info */
			node_exts = userdata->node_exts = jabber_caps_find_exts_by_node(node);

		for (i = 0; exts[i]; ++i) {
			userdata->exts = g_list_prepend(userdata->exts, exts[i]);
			/* Look it up if we don't already know what it means */
			if (!g_hash_table_lookup(node_exts->exts, exts[i])) {
				JabberIq *iq;
				PurpleXmlNode *query;
				char *nodeext;
				ext_iq_data *cbdata = g_new(ext_iq_data, 1);

				cbdata->name = exts[i];
				cbdata->data = cbplususerdata_ref(userdata);

				iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_INFO);
				query = purple_xmlnode_get_child_with_namespace(iq->node, "query",
				            NS_DISCO_INFO);
				nodeext = g_strdup_printf("%s#%s", node, exts[i]);
				purple_xmlnode_set_attrib(query, "node", nodeext);
				g_free(nodeext);
				purple_xmlnode_set_attrib(iq->node, "to", who);

				jabber_iq_set_callback(iq, jabber_caps_ext_iqcb, cbdata);
				jabber_iq_send(iq);

				++userdata->extOutstanding;
			}
			exts[i] = NULL;
		}
		/* All the strings are now part of the GList, so don't need
		 * g_strfreev. */
		g_free(exts);
	}

	if (userdata->info && userdata->extOutstanding == 0) {
		/* Start with 1 ref so the below functions are happy */
		userdata->ref = 1;

		/* We have everything we need right now */
		jabber_caps_get_info_complete(userdata);
		cbplususerdata_unref(userdata);
	}
}

static gint
jabber_xdata_compare(gconstpointer a, gconstpointer b)
{
	const PurpleXmlNode *aformtypefield = a;
	const PurpleXmlNode *bformtypefield = b;
	char *aformtype;
	char *bformtype;
	int result;

	aformtype = jabber_x_data_get_formtype(aformtypefield);
	bformtype = jabber_x_data_get_formtype(bformtypefield);

	result = strcmp(aformtype, bformtype);
	g_free(aformtype);
	g_free(bformtype);
	return result;
}

JabberCapsClientInfo *jabber_caps_parse_client_info(PurpleXmlNode *query)
{
	PurpleXmlNode *child;
	JabberCapsClientInfo *info;

	if (!query || !g_str_equal(query->name, "query") ||
			!purple_strequal(query->xmlns, NS_DISCO_INFO))
		return NULL;

	info = g_new0(JabberCapsClientInfo, 1);

	for(child = query->child; child; child = child->next) {
		if (child->type != PURPLE_XMLNODE_TYPE_TAG)
			continue;
		if (g_str_equal(child->name, "identity")) {
			/* parse identity */
			const char *category = purple_xmlnode_get_attrib(child, "category");
			const char *type = purple_xmlnode_get_attrib(child, "type");
			const char *name = purple_xmlnode_get_attrib(child, "name");
			const char *lang = purple_xmlnode_get_attrib(child, "lang");
			JabberIdentity *id;

			if (!category || !type)
				continue;

			id = g_new0(JabberIdentity, 1);
			id->category = g_strdup(category);
			id->type = g_strdup(type);
			id->name = g_strdup(name);
			id->lang = g_strdup(lang);

			info->identities = g_list_append(info->identities, id);
		} else if (g_str_equal(child->name, "feature")) {
			/* parse feature */
			const char *var = purple_xmlnode_get_attrib(child, "var");
			if (var)
				info->features = g_list_prepend(info->features, g_strdup(var));
		} else if (g_str_equal(child->name, "x")) {
			if (purple_strequal(child->xmlns, "jabber:x:data")) {
				/* x-data form */
				PurpleXmlNode *dataform = purple_xmlnode_copy(child);
				info->forms = g_list_append(info->forms, dataform);
			}
		}
	}
	return info;
}

static gint jabber_caps_xdata_field_compare(gconstpointer a, gconstpointer b)
{
	const JabberDataFormField *ac = a;
	const JabberDataFormField *bc = b;

	return strcmp(ac->var, bc->var);
}

static GList* jabber_caps_xdata_get_fields(const PurpleXmlNode *x)
{
	GList *fields = NULL;
	PurpleXmlNode *field;

	if (!x)
		return NULL;

	for (field = purple_xmlnode_get_child(x, "field"); field; field = purple_xmlnode_get_next_twin(field)) {
		PurpleXmlNode *value;
		JabberDataFormField *xdatafield = g_new0(JabberDataFormField, 1);
		xdatafield->var = g_strdup(purple_xmlnode_get_attrib(field, "var"));

		for (value = purple_xmlnode_get_child(field, "value"); value; value = purple_xmlnode_get_next_twin(value)) {
			gchar *val = purple_xmlnode_get_data(value);
			xdatafield->values = g_list_prepend(xdatafield->values, val);
		}

		xdatafield->values = g_list_sort(xdatafield->values, (GCompareFunc)strcmp);
		fields = g_list_prepend(fields, xdatafield);
	}

	fields = g_list_sort(fields, jabber_caps_xdata_field_compare);
	return fields;
}

static void
append_escaped_string(PurpleHash *hash, const gchar *str)
{
	g_return_if_fail(hash != NULL);

	if (str && *str) {
		char *tmp = g_markup_escape_text(str, -1);
		purple_hash_append(hash, (const guchar *)tmp, strlen(tmp));
		g_free(tmp);
	}

	purple_hash_append(hash, (const guchar *)"<", 1);
}

gchar *jabber_caps_calculate_hash(JabberCapsClientInfo *info, PurpleHash *hash)
{
	GList *node;
	guint8 checksum[20];
	gsize checksum_size = 20;
	gboolean success;

	if (!info || !hash)
		return NULL;

	/* sort identities, features and x-data forms */
	info->identities = g_list_sort(info->identities, jabber_identity_compare);
	info->features = g_list_sort(info->features, (GCompareFunc)strcmp);
	info->forms = g_list_sort(info->forms, jabber_xdata_compare);

	/* Add identities to the hash data */
	for (node = info->identities; node; node = node->next) {
		JabberIdentity *id = (JabberIdentity*)node->data;
		char *category = g_markup_escape_text(id->category, -1);
		char *type = g_markup_escape_text(id->type, -1);
		char *lang = NULL;
		char *name = NULL;
		char *tmp;

		if (id->lang)
			lang = g_markup_escape_text(id->lang, -1);
		if (id->name)
			name = g_markup_escape_text(id->name, -1);

		tmp = g_strconcat(category, "/", type, "/", lang ? lang : "",
		                  "/", name ? name : "", "<", NULL);

		purple_hash_append(hash, (const guchar *)tmp, strlen(tmp));

		g_free(tmp);
		g_free(category);
		g_free(type);
		g_free(lang);
		g_free(name);
	}

	/* concat features to the verification string */
	for (node = info->features; node; node = node->next) {
		append_escaped_string(hash, node->data);
	}

	/* concat x-data forms to the verification string */
	for(node = info->forms; node; node = node->next) {
		PurpleXmlNode *data = (PurpleXmlNode *)node->data;
		gchar *formtype = jabber_x_data_get_formtype(data);
		GList *fields = jabber_caps_xdata_get_fields(data);

		/* append FORM_TYPE's field value to the verification string */
		append_escaped_string(hash, formtype);
		g_free(formtype);

		while (fields) {
			JabberDataFormField *field = (JabberDataFormField*)fields->data;

			if (!g_str_equal(field->var, "FORM_TYPE")) {
				/* Append the "var" attribute */
				append_escaped_string(hash, field->var);
				/* Append <value/> elements' cdata */
				while (field->values) {
					append_escaped_string(hash, field->values->data);
					g_free(field->values->data);
					field->values = g_list_delete_link(field->values,
					                                   field->values);
				}
			} else {
				g_list_foreach(field->values, (GFunc) g_free, NULL);
				g_list_free(field->values);
			}

			g_free(field->var);
			g_free(field);

			fields = g_list_delete_link(fields, fields);
		}
	}

	/* generate hash */
	success = purple_hash_digest(hash, checksum, checksum_size);
	checksum_size = purple_hash_get_digest_size(hash);

	return (success ? purple_base64_encode(checksum, checksum_size) : NULL);
}

void jabber_caps_calculate_own_hash(JabberStream *js) {
	JabberCapsClientInfo info;
	PurpleHash *hasher;
	GList *iter = 0;
	GList *features = 0;

	if (!jabber_identities && !jabber_features) {
		/* This really shouldn't ever happen */
		purple_debug_warning("jabber", "No features or identities, cannot calculate own caps hash.\n");
		g_free(js->caps_hash);
		js->caps_hash = NULL;
		return;
	}

	/* build the currently-supported list of features */
	if (jabber_features) {
		for (iter = jabber_features; iter; iter = iter->next) {
			JabberFeature *feat = iter->data;
			if(!feat->is_enabled || feat->is_enabled(js, feat->namespace)) {
				features = g_list_append(features, feat->namespace);
			}
		}
	}

	info.features = features;
	/* TODO: This copy can go away, I think, since jabber_identities
	 * is pre-sorted, so the sort in calculate_hash should be idempotent.
	 * However, I want to test that. --darkrain
	 */
	info.identities = g_list_copy(jabber_identities);
	info.forms = NULL;

	g_free(js->caps_hash);
	hasher = purple_sha1_hash_new();
	js->caps_hash = jabber_caps_calculate_hash(&info, hasher);
	g_object_unref(hasher);
	g_list_free(info.identities);
	g_list_free(info.features);
}

const gchar* jabber_caps_get_own_hash(JabberStream *js)
{
	if (!js->caps_hash)
		jabber_caps_calculate_own_hash(js);

	return js->caps_hash;
}

void jabber_caps_broadcast_change()
{
	GList *node, *accounts = purple_accounts_get_all_active();

	for (node = accounts; node; node = node->next) {
		PurpleAccount *account = node->data;
		const char *prpl_id = purple_account_get_protocol_id(account);
		if (g_str_equal("prpl-jabber", prpl_id) && purple_account_is_connected(account)) {
			PurpleConnection *gc = purple_account_get_connection(account);
			jabber_presence_send(purple_connection_get_protocol_data(gc), TRUE);
		}
	}

	g_list_free(accounts);
}

