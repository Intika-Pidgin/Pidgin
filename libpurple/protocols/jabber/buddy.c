/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "cipher.h"
#include "debug.h"
#include "imgstore.h"
#include "prpl.h"
#include "notify.h"
#include "request.h"
#include "util.h"
#include "xmlnode.h"

#include "buddy.h"
#include "chat.h"
#include "jabber.h"
#include "iq.h"
#include "presence.h"
#include "xdata.h"

typedef struct {
	long idle_seconds;
} JabberBuddyInfoResource;

typedef struct {
	JabberStream *js;
	JabberBuddy *jb;
	char *jid;
	GSList *ids;
	GHashTable *resources;
	int timeout_handle;
	char *vcard_text;
	GSList *vcard_imgids;
} JabberBuddyInfo;

void jabber_buddy_free(JabberBuddy *jb)
{
	g_return_if_fail(jb != NULL);

	if(jb->error_msg)
		g_free(jb->error_msg);
	while(jb->resources)
		jabber_buddy_resource_free(jb->resources->data);

	g_free(jb);
}

JabberBuddy *jabber_buddy_find(JabberStream *js, const char *name,
		gboolean create)
{
	JabberBuddy *jb;
	const char *realname;

	if (js->buddies == NULL)
		return NULL;

	if(!(realname = jabber_normalize(js->gc->account, name)))
		return NULL;

	jb = g_hash_table_lookup(js->buddies, realname);

	if(!jb && create) {
		jb = g_new0(JabberBuddy, 1);
		g_hash_table_insert(js->buddies, g_strdup(realname), jb);
	}

	return jb;
}


JabberBuddyResource *jabber_buddy_find_resource(JabberBuddy *jb,
		const char *resource)
{
	JabberBuddyResource *jbr = NULL;
	GList *l;

	if(!jb)
		return NULL;

	for(l = jb->resources; l; l = l->next)
	{
		if(!jbr && !resource) {
			jbr = l->data;
		} else if(!resource) {
			if(((JabberBuddyResource *)l->data)->priority >= jbr->priority)
				jbr = l->data;
		} else if(((JabberBuddyResource *)l->data)->name) {
			if(!strcmp(((JabberBuddyResource *)l->data)->name, resource)) {
				jbr = l->data;
				break;
			}
		}
	}

	return jbr;
}

JabberBuddyResource *jabber_buddy_track_resource(JabberBuddy *jb, const char *resource,
		int priority, JabberBuddyState state, const char *status)
{
	JabberBuddyResource *jbr = jabber_buddy_find_resource(jb, resource);

	if(!jbr) {
		jbr = g_new0(JabberBuddyResource, 1);
		jbr->jb = jb;
		jbr->name = g_strdup(resource);
		jbr->capabilities = JABBER_CAP_XHTML;
		jb->resources = g_list_append(jb->resources, jbr);
	}
	jbr->priority = priority;
	jbr->state = state;
	if(jbr->status)
		g_free(jbr->status);
        if (status)
		jbr->status = g_markup_escape_text(status, -1);
	else
		jbr->status = NULL;

	return jbr;
}

void jabber_buddy_resource_free(JabberBuddyResource *jbr)
{
	g_return_if_fail(jbr != NULL);

	jbr->jb->resources = g_list_remove(jbr->jb->resources, jbr);

	g_free(jbr->name);
	g_free(jbr->status);
	g_free(jbr->thread_id);
	g_free(jbr->client.name);
	g_free(jbr->client.version);
	g_free(jbr->client.os);
	g_free(jbr);
}

void jabber_buddy_remove_resource(JabberBuddy *jb, const char *resource)
{
	JabberBuddyResource *jbr = jabber_buddy_find_resource(jb, resource);

	if(!jbr)
		return;

	jabber_buddy_resource_free(jbr);
}

const char *jabber_buddy_get_status_msg(JabberBuddy *jb)
{
	JabberBuddyResource *jbr;

	if(!jb)
		return NULL;

	jbr = jabber_buddy_find_resource(jb, NULL);

	if(!jbr)
		return NULL;

	return jbr->status;
}

/*******
 * This is the old vCard stuff taken from the old prpl.  vCards, by definition
 * are a temporary thing until jabber can get its act together and come up
 * with a format for user information, hence the namespace of 'vcard-temp'
 *
 * Since I don't feel like putting that much work into something that's
 * _supposed_ to go away, i'm going to just copy the kludgy old code here,
 * and make it purdy when jabber comes up with a standards-track JEP to
 * replace vcard-temp
 *                                 --Nathan
 *******/

/*---------------------------------------*/
/* Jabber "set info" (vCard) support     */
/*---------------------------------------*/

/*
 * V-Card format:
 *
 *  <vCard prodid='' version='' xmlns=''>
 *    <FN></FN>
 *    <N>
 *	<FAMILY/>
 *	<GIVEN/>
 *    </N>
 *    <NICKNAME/>
 *    <URL/>
 *    <ADR>
 *	<STREET/>
 *	<EXTADD/>
 *	<LOCALITY/>
 *	<REGION/>
 *	<PCODE/>
 *	<COUNTRY/>
 *    </ADR>
 *    <TEL/>
 *    <EMAIL/>
 *    <ORG>
 *	<ORGNAME/>
 *	<ORGUNIT/>
 *    </ORG>
 *    <TITLE/>
 *    <ROLE/>
 *    <DESC/>
 *    <BDAY/>
 *  </vCard>
 *
 * See also:
 *
 *	http://docs.jabber.org/proto/html/vcard-temp.html
 *	http://www.vcard-xml.org/dtd/vCard-XML-v2-20010520.dtd
 */

/*
 * Cross-reference user-friendly V-Card entry labels to vCard XML tags
 * and attributes.
 *
 * Order is (or should be) unimportant.  For example: we have no way of
 * knowing in what order real data will arrive.
 *
 * Format: Label, Pre-set text, "visible" flag, "editable" flag, XML tag
 *         name, XML tag's parent tag "path" (relative to vCard node).
 *
 *         List is terminated by a NULL label pointer.
 *
 *	   Entries with no label text, but with XML tag and parent tag
 *	   entries, are used by V-Card XML construction routines to
 *	   "automagically" construct the appropriate XML node tree.
 *
 * Thoughts on future direction/expansion
 *
 *	This is a "simple" vCard.
 *
 *	It is possible for nodes other than the "vCard" node to have
 *      attributes.  Should that prove necessary/desirable, add an
 *      "attributes" pointer to the vcard_template struct, create the
 *      necessary tag_attr structs, and add 'em to the vcard_dflt_data
 *      array.
 *
 *	The above changes will (obviously) require changes to the vCard
 *      construction routines.
 */

struct vcard_template {
	char *label;			/* label text pointer */
	char *text;			/* entry text pointer */
	int  visible;			/* should entry field be "visible?" */
	int  editable;			/* should entry field be editable? */
	char *tag;			/* tag text */
	char *ptag;			/* parent tag "path" text */
	char *url;			/* vCard display format if URL */
} vcard_template_data[] = {
	{N_("Full Name"),          NULL, TRUE, TRUE, "FN",        NULL,  NULL},
	{N_("Family Name"),        NULL, TRUE, TRUE, "FAMILY",    "N",   NULL},
	{N_("Given Name"),         NULL, TRUE, TRUE, "GIVEN",     "N",   NULL},
	{N_("Nickname"),           NULL, TRUE, TRUE, "NICKNAME",  NULL,  NULL},
	{N_("URL"),                NULL, TRUE, TRUE, "URL",       NULL,  "<A HREF=\"%s\">%s</A>"},
	{N_("Street Address"),     NULL, TRUE, TRUE, "STREET",    "ADR", NULL},
	{N_("Extended Address"),   NULL, TRUE, TRUE, "EXTADD",    "ADR", NULL},
	{N_("Locality"),           NULL, TRUE, TRUE, "LOCALITY",  "ADR", NULL},
	{N_("Region"),             NULL, TRUE, TRUE, "REGION",    "ADR", NULL},
	{N_("Postal Code"),        NULL, TRUE, TRUE, "PCODE",     "ADR", NULL},
	{N_("Country"),            NULL, TRUE, TRUE, "CTRY",      "ADR", NULL},
	{N_("Telephone"),          NULL, TRUE, TRUE, "NUMBER",    "TEL",  NULL},
	{N_("E-Mail"),             NULL, TRUE, TRUE, "USERID",    "EMAIL",  "<A HREF=\"mailto:%s\">%s</A>"},
	{N_("Organization Name"),  NULL, TRUE, TRUE, "ORGNAME",   "ORG", NULL},
	{N_("Organization Unit"),  NULL, TRUE, TRUE, "ORGUNIT",   "ORG", NULL},
	{N_("Title"),              NULL, TRUE, TRUE, "TITLE",     NULL,  NULL},
	{N_("Role"),               NULL, TRUE, TRUE, "ROLE",      NULL,  NULL},
	{N_("Birthday"),           NULL, TRUE, TRUE, "BDAY",      NULL,  NULL},
	{N_("Description"),        NULL, TRUE, TRUE, "DESC",      NULL,  NULL},
	{"", NULL, TRUE, TRUE, "N",     NULL, NULL},
	{"", NULL, TRUE, TRUE, "ADR",   NULL, NULL},
	{"", NULL, TRUE, TRUE, "ORG",   NULL, NULL},
	{NULL, NULL, 0, 0, NULL, NULL, NULL}
};

/*
 * The "vCard" tag's attribute list...
 */
struct tag_attr {
	char *attr;
	char *value;
} vcard_tag_attr_list[] = {
	{"prodid",   "-//HandGen//NONSGML vGen v1.0//EN"},
	{"version",  "2.0",                             },
	{"xmlns",    "vcard-temp",                      },
	{NULL, NULL},
};


/*
 * Insert a tag node into an xmlnode tree, recursively inserting parent tag
 * nodes as necessary
 *
 * Returns pointer to inserted node
 *
 * Note to hackers: this code is designed to be re-entrant (it's recursive--it
 * calls itself), so don't put any "static"s in here!
 */
static xmlnode *insert_tag_to_parent_tag(xmlnode *start, const char *parent_tag, const char *new_tag)
{
	xmlnode *x = NULL;

	/*
	 * If the parent tag wasn't specified, see if we can get it
	 * from the vCard template struct.
	 */
	if(parent_tag == NULL) {
		struct vcard_template *vc_tp = vcard_template_data;

		while(vc_tp->label != NULL) {
			if(strcmp(vc_tp->tag, new_tag) == 0) {
				parent_tag = vc_tp->ptag;
				break;
			}
			++vc_tp;
		}
	}

	/*
	 * If we have a parent tag...
	 */
	if(parent_tag != NULL ) {
		/*
		 * Try to get the parent node for a tag
		 */
		if((x = xmlnode_get_child(start, parent_tag)) == NULL) {
			/*
			 * Descend?
			 */
			char *grand_parent = g_strdup(parent_tag);
			char *parent;

			if((parent = strrchr(grand_parent, '/')) != NULL) {
				*(parent++) = '\0';
				x = insert_tag_to_parent_tag(start, grand_parent, parent);
			} else {
				x = xmlnode_new_child(start, grand_parent);
			}
			g_free(grand_parent);
		} else {
			/*
			 * We found *something* to be the parent node.
			 * Note: may be the "root" node!
			 */
			xmlnode *y;
			if((y = xmlnode_get_child(x, new_tag)) != NULL) {
				return(y);
			}
		}
	}

	/*
	 * insert the new tag into its parent node
	 */
	return(xmlnode_new_child((x == NULL? start : x), new_tag));
}

/*
 * Send vCard info to Jabber server
 */
void jabber_set_info(PurpleConnection *gc, const char *info)
{
	JabberIq *iq;
	JabberStream *js = gc->proto_data;
	xmlnode *vc_node;
	char *avatar_file = NULL;
	struct tag_attr *tag_attr;

	g_free(js->avatar_hash);
	js->avatar_hash = NULL;

	/*
	 * Send only if there's actually any *information* to send
	 */
	vc_node = info ? xmlnode_from_str(info, -1) : NULL;
	avatar_file = purple_buddy_icons_get_full_path(purple_account_get_buddy_icon(gc->account));

	if(!vc_node) {
		vc_node = xmlnode_new("vCard");
		for(tag_attr = vcard_tag_attr_list; tag_attr->attr != NULL; ++tag_attr)
			xmlnode_set_attrib(vc_node, tag_attr->attr, tag_attr->value);
	}

	if (vc_node->name &&
			!g_ascii_strncasecmp(vc_node->name, "vCard", 5)) {
		GError *error = NULL;
		gchar *avatar_data_tmp;
		guchar *avatar_data;
		gsize avatar_len;

		if(avatar_file && g_file_get_contents(avatar_file, &avatar_data_tmp, &avatar_len, &error)) {
			xmlnode *photo, *binval;
			gchar *enc;
			int i;
			unsigned char hashval[20];
			char *p, hash[41];

			avatar_data = (guchar *) avatar_data_tmp;
			photo = xmlnode_new_child(vc_node, "PHOTO");
			binval = xmlnode_new_child(photo, "BINVAL");
			enc = purple_base64_encode(avatar_data, avatar_len);

			purple_cipher_digest_region("sha1", (guchar *)avatar_data,
									  avatar_len, sizeof(hashval),
									  hashval, NULL);

			p = hash;
			for(i=0; i<20; i++, p+=2)
				snprintf(p, 3, "%02x", hashval[i]);
			js->avatar_hash = g_strdup(hash);

			xmlnode_insert_data(binval, enc, -1);
			g_free(enc);
			g_free(avatar_data);
		} else if (error != NULL) {
			g_error_free(error);
		}
		g_free(avatar_file);

		iq = jabber_iq_new(js, JABBER_IQ_SET);
		xmlnode_insert_child(iq->node, vc_node);
		jabber_iq_send(iq);
	} else {
		xmlnode_free(vc_node);
	}
}

void jabber_set_buddy_icon(PurpleConnection *gc, const char *iconfile)
{
	PurplePresence *gpresence;
	PurpleStatus *status;

	jabber_set_info(gc, purple_account_get_user_info(gc->account));

	gpresence = purple_account_get_presence(gc->account);
	status = purple_presence_get_active_status(gpresence);
	jabber_presence_send(gc->account, status);
}

/*
 * This is the callback from the "ok clicked" for "set vCard"
 *
 * Formats GSList data into XML-encoded string and returns a pointer
 * to said string.
 *
 * g_free()'ing the returned string space is the responsibility of
 * the caller.
 */
static void
jabber_format_info(PurpleConnection *gc, PurpleRequestFields *fields)
{
	xmlnode *vc_node;
	PurpleRequestField *field;
	const char *text;
	char *p;
	const struct vcard_template *vc_tp;
	struct tag_attr *tag_attr;

	vc_node = xmlnode_new("vCard");

	for(tag_attr = vcard_tag_attr_list; tag_attr->attr != NULL; ++tag_attr)
		xmlnode_set_attrib(vc_node, tag_attr->attr, tag_attr->value);

	for (vc_tp = vcard_template_data; vc_tp->label != NULL; vc_tp++) {
		if (*vc_tp->label == '\0')
			continue;

		field = purple_request_fields_get_field(fields, vc_tp->tag);
		text  = purple_request_field_string_get_value(field);


		if (text != NULL && *text != '\0') {
			xmlnode *xp;

			purple_debug(PURPLE_DEBUG_INFO, "jabber",
					"Setting %s to '%s'\n", vc_tp->tag, text);

			if ((xp = insert_tag_to_parent_tag(vc_node,
											   NULL, vc_tp->tag)) != NULL) {

				xmlnode_insert_data(xp, text, -1);
			}
		}
	}

	p = xmlnode_to_str(vc_node, NULL);
	xmlnode_free(vc_node);

	purple_account_set_user_info(purple_connection_get_account(gc), p);
	serv_set_info(gc, p);

	g_free(p);
}

/*
 * This gets executed by the proto action
 *
 * Creates a new PurpleRequestFields struct, gets the XML-formatted user_info
 * string (if any) into GSLists for the (multi-entry) edit dialog and
 * calls the set_vcard dialog.
 */
void jabber_setup_set_info(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	const struct vcard_template *vc_tp;
	const char *user_info;
	char *cdata = NULL;
	xmlnode *x_vc_data = NULL;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	/*
	 * Get existing, XML-formatted, user info
	 */
	if((user_info = purple_account_get_user_info(gc->account)) != NULL)
		x_vc_data = xmlnode_from_str(user_info, -1);

	/*
	 * Set up GSLists for edit with labels from "template," data from user info
	 */
	for(vc_tp = vcard_template_data; vc_tp->label != NULL; ++vc_tp) {
		xmlnode *data_node;
		if((vc_tp->label)[0] == '\0')
			continue;

		if (x_vc_data != NULL) {
			if(vc_tp->ptag == NULL) {
				data_node = xmlnode_get_child(x_vc_data, vc_tp->tag);
			} else {
				gchar *tag = g_strdup_printf("%s/%s", vc_tp->ptag, vc_tp->tag);
				data_node = xmlnode_get_child(x_vc_data, tag);
				g_free(tag);
			}
			if(data_node)
				cdata = xmlnode_get_data(data_node);
		}

		if(strcmp(vc_tp->tag, "DESC") == 0) {
			field = purple_request_field_string_new(vc_tp->tag,
												  _(vc_tp->label), cdata,
												  TRUE);
		} else {
			field = purple_request_field_string_new(vc_tp->tag,
												  _(vc_tp->label), cdata,
												  FALSE);
		}

		g_free(cdata);
		cdata = NULL;

		purple_request_field_group_add_field(group, field);
	}

	if(x_vc_data != NULL)
		xmlnode_free(x_vc_data);

	purple_request_fields(gc, _("Edit Jabber vCard"),
						_("Edit Jabber vCard"),
						_("All items below are optional. Enter only the "
						  "information with which you feel comfortable."),
						fields,
						_("Save"), G_CALLBACK(jabber_format_info),
						_("Cancel"), NULL,
						gc);
}

/*---------------------------------------*/
/* End Jabber "set info" (vCard) support */
/*---------------------------------------*/

/******
 * end of that ancient crap that needs to die
 ******/

static void jabber_buddy_info_destroy(JabberBuddyInfo *jbi)
{
	/* Remove the timeout, which would otherwise trigger jabber_buddy_get_info_timeout() */
	if (jbi->timeout_handle > 0)
		purple_timeout_remove(jbi->timeout_handle);

	g_free(jbi->jid);
	g_hash_table_destroy(jbi->resources);
	g_free(jbi->vcard_text);
	g_free(jbi);
}

static void jabber_buddy_info_show_if_ready(JabberBuddyInfo *jbi)
{
	char *resource_name, *tmp;
	JabberBuddyResource *jbr;
	JabberBuddyInfoResource *jbir = NULL;
	GList *resources;
	PurpleNotifyUserInfo *user_info;

	/* not yet */
	if(jbi->ids)
		return;

	user_info = purple_notify_user_info_new();
	resource_name = jabber_get_resource(jbi->jid);

	if(resource_name) {
		jbr = jabber_buddy_find_resource(jbi->jb, resource_name);
		jbir = g_hash_table_lookup(jbi->resources, resource_name);
		if(jbr) {
			char *purdy = NULL;
			if(jbr->status)
				purdy = purple_strdup_withhtml(jbr->status);
			tmp = g_strdup_printf("%s%s%s", jabber_buddy_state_get_name(jbr->state),
							(purdy ? ": " : ""),
							(purdy ? purdy : ""));
			purple_notify_user_info_add_pair(user_info, _("Status"), tmp);
			g_free(tmp);
			g_free(purdy);
		} else {
			purple_notify_user_info_add_pair(user_info, _("Status"), _("Unknown"));
		}
		if(jbir) {
			if(jbir->idle_seconds > 0) {
				char *idle = purple_str_seconds_to_string(jbir->idle_seconds);
				purple_notify_user_info_add_pair(user_info, _("Idle"), idle);
				g_free(idle);
			}
		}
		if(jbr && jbr->client.name) {
			tmp = g_strdup_printf("%s%s%s", jbr->client.name,
								  (jbr->client.version ? " " : ""),
								  (jbr->client.version ? jbr->client.version : ""));
			purple_notify_user_info_add_pair(user_info, _("Client"), tmp);
			g_free(tmp);

			if(jbr->client.os) {
				purple_notify_user_info_add_pair(user_info, _("Operating System"), jbr->client.os);
			}
		}
	} else {
		for(resources = jbi->jb->resources; resources; resources = resources->next) {
			char *purdy = NULL;
			jbr = resources->data;
			if(jbr->status)
				purdy = purple_strdup_withhtml(jbr->status);
			if(jbr->name)
				purple_notify_user_info_add_pair(user_info, _("Resource"), jbr->name);
			tmp = g_strdup_printf("%d", jbr->priority);
			purple_notify_user_info_add_pair(user_info, _("Priority"), tmp);
			g_free(tmp);

			tmp = g_strdup_printf("%s%s%s", jabber_buddy_state_get_name(jbr->state),
								  (purdy ? ": " : ""),
								  (purdy ? purdy : ""));
			purple_notify_user_info_add_pair(user_info, _("Status"), tmp);
			g_free(tmp);
			g_free(purdy);

			if(jbr->name)
				jbir = g_hash_table_lookup(jbi->resources, jbr->name);

			if(jbir) {
				if(jbir->idle_seconds > 0) {
					char *idle = purple_str_seconds_to_string(jbir->idle_seconds);
					purple_notify_user_info_add_pair(user_info, _("Idle"), idle);
					g_free(idle);
				}
			}
			if(jbr && jbr->client.name) {
				tmp = g_strdup_printf("%s%s%s", jbr->client.name,
									  (jbr->client.version ? " " : ""),
									  (jbr->client.version ? jbr->client.version : ""));
				purple_notify_user_info_add_pair(user_info,
											   _("Client"), tmp);
				g_free(tmp);

				if(jbr->client.os) {
					purple_notify_user_info_add_pair(user_info, _("Operating System"), jbr->client.os);
				}
			}
		}
	}

	g_free(resource_name);

	if (jbi->vcard_text != NULL) {
		purple_notify_user_info_add_section_break(user_info);
		/* Should this have some sort of label? */
		purple_notify_user_info_add_pair(user_info, NULL, jbi->vcard_text);
	}

	purple_notify_userinfo(jbi->js->gc, jbi->jid, user_info, NULL, NULL);
	purple_notify_user_info_destroy(user_info);

	while(jbi->vcard_imgids) {
		purple_imgstore_unref_by_id(GPOINTER_TO_INT(jbi->vcard_imgids->data));
		jbi->vcard_imgids = g_slist_delete_link(jbi->vcard_imgids, jbi->vcard_imgids);
	}

	jbi->js->pending_buddy_info_requests = g_slist_remove(jbi->js->pending_buddy_info_requests, jbi);

	jabber_buddy_info_destroy(jbi);
}

static void jabber_buddy_info_remove_id(JabberBuddyInfo *jbi, const char *id)
{
	GSList *l = jbi->ids;
	char *comp_id;

	if(!id)
		return;

	while(l) {
		comp_id = l->data;
		if(!strcmp(id, comp_id)) {
			jbi->ids = g_slist_remove(jbi->ids, comp_id);
			g_free(comp_id);
			return;
		}
		l = l->next;
	}
}

static void jabber_vcard_parse(JabberStream *js, xmlnode *packet, gpointer data)
{
	const char *id, *from;
	GString *info_text;
	char *bare_jid;
	char *text;
	xmlnode *vcard;
	PurpleBuddy *b;
	JabberBuddyInfo *jbi = data;

	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	if(!jbi)
		return;

	jabber_buddy_info_remove_id(jbi, id);

	if(!from)
		return;

	if(!jabber_buddy_find(js, from, FALSE))
		return;

	/* XXX: handle the error case */

	bare_jid = jabber_get_bare_jid(from);

	b = purple_find_buddy(js->gc->account, bare_jid);

	info_text = g_string_new("");

	if((vcard = xmlnode_get_child(packet, "vCard")) ||
			(vcard = xmlnode_get_child_with_namespace(packet, "query", "vcard-temp"))) {
		xmlnode *child;
		for(child = vcard->child; child; child = child->next)
		{
			xmlnode *child2;

			if(child->type != XMLNODE_TYPE_TAG)
				continue;

			text = xmlnode_get_data(child);
			if(text && !strcmp(child->name, "FN")) {
				g_string_append_printf(info_text, "<b>%s:</b> %s<br/>",
						_("Full Name"), text);
			} else if(!strcmp(child->name, "N")) {
				for(child2 = child->child; child2; child2 = child2->next)
				{
					char *text2;

					if(child2->type != XMLNODE_TYPE_TAG)
						continue;

					text2 = xmlnode_get_data(child2);
					if(text2 && !strcmp(child2->name, "FAMILY")) {
						g_string_append_printf(info_text,
								"<b>%s:</b> %s<br/>",
								_("Family Name"), text2);
					} else if(text2 && !strcmp(child2->name, "GIVEN")) {
						g_string_append_printf(info_text,
								"<b>%s:</b> %s<br/>",
								_("Given Name"), text2);
					} else if(text2 && !strcmp(child2->name, "MIDDLE")) {
						g_string_append_printf(info_text,
								"<b>%s:</b> %s<br/>",
								_("Middle Name"), text2);
					}
					g_free(text2);
				}
			} else if(text && !strcmp(child->name, "NICKNAME")) {
				serv_got_alias(js->gc, from, text);
				if(b) {
					purple_blist_node_set_string((PurpleBlistNode*)b, "servernick", text);
				}
				g_string_append_printf(info_text, "<b>%s:</b> %s<br/>",
						_("Nickname"), text);
			} else if(text && !strcmp(child->name, "BDAY")) {
				g_string_append_printf(info_text, "<b>%s:</b> %s<br/>",
						_("Birthday"), text);
			} else if(!strcmp(child->name, "ADR")) {
				gboolean address_line_added = FALSE;

				for(child2 = child->child; child2; child2 = child2->next)
				{
					char *text2;

					if(child2->type != XMLNODE_TYPE_TAG)
						continue;

					text2 = xmlnode_get_data(child2);
					if (text2 == NULL)
						continue;

					/* We do this here so that it's not added if all the child
					 * elements are empty. */
					if (!address_line_added)
					{
						g_string_append_printf(info_text, "<b>%s:</b><br/>",
								_("Address"));
						address_line_added = TRUE;
					}

					if(!strcmp(child2->name, "POBOX")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("P.O. Box"), text2);
					} else if(!strcmp(child2->name, "EXTADR")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("Extended Address"), text2);
					} else if(!strcmp(child2->name, "STREET")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("Street Address"), text2);
					} else if(!strcmp(child2->name, "LOCALITY")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("Locality"), text2);
					} else if(!strcmp(child2->name, "REGION")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("Region"), text2);
					} else if(!strcmp(child2->name, "PCODE")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("Postal Code"), text2);
					} else if(!strcmp(child2->name, "CTRY")
								|| !strcmp(child2->name, "COUNTRY")) {
						g_string_append_printf(info_text,
								"&nbsp;<b>%s:</b> %s<br/>",
								_("Country"), text2);
					}
					g_free(text2);
				}
			} else if(!strcmp(child->name, "TEL")) {
				char *number;
				if((child2 = xmlnode_get_child(child, "NUMBER"))) {
					/* show what kind of number it is */
					number = xmlnode_get_data(child2);
					if(number) {
						g_string_append_printf(info_text,
								"<b>%s:</b> %s<br/>", _("Telephone"), number);
						g_free(number);
					}
				} else if((number = xmlnode_get_data(child))) {
					/* lots of clients (including purple) do this, but it's
					 * out of spec */
					g_string_append_printf(info_text,
							"<b>%s:</b> %s<br/>", _("Telephone"), number);
					g_free(number);
				}
			} else if(!strcmp(child->name, "EMAIL")) {
				char *userid;
				if((child2 = xmlnode_get_child(child, "USERID"))) {
					/* show what kind of email it is */
					userid = xmlnode_get_data(child2);
					if(userid) {
						g_string_append_printf(info_text,
								"<b>%s:</b> <a href='mailto:%s'>%s</a><br/>",
								_("E-Mail"), userid, userid);
						g_free(userid);
					}
				} else if((userid = xmlnode_get_data(child))) {
					/* lots of clients (including purple) do this, but it's
					 * out of spec */
						g_string_append_printf(info_text,
								"<b>%s:</b> <a href='mailto:%s'>%s</a><br/>",
								_("E-Mail"), userid, userid);
					g_free(userid);
				}
			} else if(!strcmp(child->name, "ORG")) {
				for(child2 = child->child; child2; child2 = child2->next)
				{
					char *text2;

					if(child2->type != XMLNODE_TYPE_TAG)
						continue;

					text2 = xmlnode_get_data(child2);
					if(text2 && !strcmp(child2->name, "ORGNAME")) {
						g_string_append_printf(info_text,
								"<b>%s:</b> %s<br/>",
								_("Organization Name"), text2);
					} else if(text2 && !strcmp(child2->name, "ORGUNIT")) {
						g_string_append_printf(info_text,
								"<b>%s:</b> %s<br/>",
								_("Organization Unit"), text2);
					}
					g_free(text2);
				}
			} else if(text && !strcmp(child->name, "TITLE")) {
				g_string_append_printf(info_text, "<b>%s:</b> %s<br/>",
						_("Title"), text);
			} else if(text && !strcmp(child->name, "ROLE")) {
				g_string_append_printf(info_text, "<b>%s:</b> %s<br/>",
						_("Role"), text);
			} else if(text && !strcmp(child->name, "DESC")) {
				g_string_append_printf(info_text, "<b>%s:</b> %s<br/>",
						_("Description"), text);
			} else if(!strcmp(child->name, "PHOTO") ||
					!strcmp(child->name, "LOGO")) {
				char *bintext = NULL;
				xmlnode *binval;

				if( ((binval = xmlnode_get_child(child, "BINVAL")) &&
						(bintext = xmlnode_get_data(binval))) ||
						(bintext = xmlnode_get_data(child))) {
					gsize size;
					guchar *data;
					int i;
					unsigned char hashval[20];
					char *p, hash[41];
					gboolean photo = (strcmp(child->name, "PHOTO") == 0);

					data = purple_base64_decode(bintext, &size);

					jbi->vcard_imgids = g_slist_prepend(jbi->vcard_imgids, GINT_TO_POINTER(purple_imgstore_add_with_id(data, size, "logo.png")));
					g_string_append_printf(info_text,
							"<b>%s:</b> <img id='%d'><br/>",
							photo ? _("Photo") : _("Logo"),
							GPOINTER_TO_INT(jbi->vcard_imgids->data));

					purple_buddy_icons_set_for_user(js->gc->account, bare_jid,
							data, size);

					purple_cipher_digest_region("sha1", (guchar *)data, size,
							sizeof(hashval), hashval, NULL);
					p = hash;
					for(i=0; i<20; i++, p+=2)
						snprintf(p, 3, "%02x", hashval[i]);
					purple_blist_node_set_string((PurpleBlistNode*)b, "avatar_hash", hash);

					g_free(data);
					g_free(bintext);
				}
			}
			g_free(text);
		}
	}

	jbi->vcard_text = purple_strdup_withhtml(info_text->str);
	g_string_free(info_text, TRUE);
	g_free(bare_jid);

	jabber_buddy_info_show_if_ready(jbi);
}


static void jabber_buddy_info_resource_free(gpointer data)
{
	JabberBuddyInfoResource *jbri = data;
	g_free(jbri);
}

static void jabber_version_parse(JabberStream *js, xmlnode *packet, gpointer data)
{
	JabberBuddyInfo *jbi = data;
	const char *type, *id, *from;
	xmlnode *query;
	char *resource_name;

	g_return_if_fail(jbi != NULL);

	type = xmlnode_get_attrib(packet, "type");
	id = xmlnode_get_attrib(packet, "id");
	from = xmlnode_get_attrib(packet, "from");

	jabber_buddy_info_remove_id(jbi, id);

	if(!from)
		return;

	resource_name = jabber_get_resource(from);

	if(resource_name) {
		if(type && !strcmp(type, "result")) {
			if((query = xmlnode_get_child(packet, "query"))) {
				JabberBuddyResource *jbr = jabber_buddy_find_resource(jbi->jb, resource_name);
				if(jbr) {
					xmlnode *node;
					if((node = xmlnode_get_child(query, "name"))) {
						jbr->client.name = xmlnode_get_data(node);
					}
					if((node = xmlnode_get_child(query, "version"))) {
						jbr->client.version = xmlnode_get_data(node);
					}
					if((node = xmlnode_get_child(query, "os"))) {
						jbr->client.os = xmlnode_get_data(node);
					}
				}
			}
		}
		g_free(resource_name);
	}

	jabber_buddy_info_show_if_ready(jbi);
}

static void jabber_last_parse(JabberStream *js, xmlnode *packet, gpointer data)
{
	JabberBuddyInfo *jbi = data;
	xmlnode *query;
	char *resource_name;
	const char *type, *id, *from, *seconds;

	g_return_if_fail(jbi != NULL);

	type = xmlnode_get_attrib(packet, "type");
	id = xmlnode_get_attrib(packet, "id");
	from = xmlnode_get_attrib(packet, "from");

	jabber_buddy_info_remove_id(jbi, id);

	if(!from)
		return;

	resource_name = jabber_get_resource(from);

	if(resource_name) {
		if(type && !strcmp(type, "result")) {
			if((query = xmlnode_get_child(packet, "query"))) {
				seconds = xmlnode_get_attrib(query, "seconds");
				if(seconds) {
					char *end = NULL;
					long sec = strtol(seconds, &end, 10);
					if(end != seconds) {
						JabberBuddyInfoResource *jbir = g_hash_table_lookup(jbi->resources, resource_name);
						if(jbir) {
							jbir->idle_seconds = sec;
						}
					}
				}
			}
		}
		g_free(resource_name);
	}

	jabber_buddy_info_show_if_ready(jbi);
}

void jabber_buddy_remove_all_pending_buddy_info_requests(JabberStream *js)
{
	if (js->pending_buddy_info_requests)
	{
		JabberBuddyInfo *jbi;
		GSList *l = js->pending_buddy_info_requests;
		while (l) {
			jbi = l->data;

			g_slist_free(jbi->ids);
			jabber_buddy_info_destroy(jbi);

			l = l->next;
		}

		g_slist_free(js->pending_buddy_info_requests);
		js->pending_buddy_info_requests = NULL;
	}
}

static gboolean jabber_buddy_get_info_timeout(gpointer data)
{
	JabberBuddyInfo *jbi = data;

	/* remove the pending callbacks */
	while(jbi->ids) {
		char *id = jbi->ids->data;
		jabber_iq_remove_callback_by_id(jbi->js, id);
		jbi->ids = g_slist_remove(jbi->ids, id);
		g_free(id);
	}

	jbi->js->pending_buddy_info_requests = g_slist_remove(jbi->js->pending_buddy_info_requests, jbi);
	jbi->timeout_handle = 0;

	jabber_buddy_info_show_if_ready(jbi);

	return FALSE;
}

static void jabber_buddy_get_info_for_jid(JabberStream *js, const char *jid)
{
	JabberIq *iq;
	xmlnode *vcard;
	GList *resources;
	JabberBuddy *jb;
	JabberBuddyInfo *jbi;

	jb = jabber_buddy_find(js, jid, TRUE);

	/* invalid JID */
	if(!jb)
		return;

	jbi = g_new0(JabberBuddyInfo, 1);
	jbi->jid = g_strdup(jid);
	jbi->js = js;
	jbi->jb = jb;
	jbi->resources = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, jabber_buddy_info_resource_free);

	iq = jabber_iq_new(js, JABBER_IQ_GET);

	xmlnode_set_attrib(iq->node, "to", jid);
	vcard = xmlnode_new_child(iq->node, "vCard");
	xmlnode_set_namespace(vcard, "vcard-temp");

	jabber_iq_set_callback(iq, jabber_vcard_parse, jbi);
	jbi->ids = g_slist_prepend(jbi->ids, g_strdup(iq->id));

	jabber_iq_send(iq);

	for(resources = jb->resources; resources; resources = resources->next)
	{
		JabberBuddyResource *jbr = resources->data;
		JabberBuddyInfoResource *jbir;
		char *full_jid;

		if ((strchr(jid, '/') == NULL) && (jbr->name != NULL)) {
			full_jid = g_strdup_printf("%s/%s", jid, jbr->name);
		} else {
			full_jid = g_strdup(jid);
		}

		if (jbr->name != NULL)
		{
			jbir = g_new0(JabberBuddyInfoResource, 1);
			g_hash_table_insert(jbi->resources, g_strdup(jbr->name), jbir);
		}

		if(!jbr->client.name) {
			iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:version");
			xmlnode_set_attrib(iq->node, "to", full_jid);
			jabber_iq_set_callback(iq, jabber_version_parse, jbi);
			jbi->ids = g_slist_prepend(jbi->ids, g_strdup(iq->id));
			jabber_iq_send(iq);
		}

		iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:last");
		xmlnode_set_attrib(iq->node, "to", full_jid);
		jabber_iq_set_callback(iq, jabber_last_parse, jbi);
		jbi->ids = g_slist_prepend(jbi->ids, g_strdup(iq->id));
		jabber_iq_send(iq);

		g_free(full_jid);
	}

	js->pending_buddy_info_requests = g_slist_prepend(js->pending_buddy_info_requests, jbi);
	jbi->timeout_handle = purple_timeout_add(30000, jabber_buddy_get_info_timeout, jbi);
}

void jabber_buddy_get_info(PurpleConnection *gc, const char *who)
{
	JabberStream *js = gc->proto_data;
	char *bare_jid = jabber_get_bare_jid(who);

	if(bare_jid) {
		jabber_buddy_get_info_for_jid(js, bare_jid);
		g_free(bare_jid);
	}
}

void jabber_buddy_get_info_chat(PurpleConnection *gc, int id,
		const char *resource)
{
	JabberStream *js = gc->proto_data;
	JabberChat *chat = jabber_chat_find_by_id(js, id);
	char *full_jid;

	if(!chat)
		return;

	full_jid = g_strdup_printf("%s@%s/%s", chat->room, chat->server, resource);
	jabber_buddy_get_info_for_jid(js, full_jid);
	g_free(full_jid);
}


static void jabber_buddy_set_invisibility(JabberStream *js, const char *who,
		gboolean invisible)
{
	PurplePresence *gpresence;
	PurpleAccount *account;
	PurpleStatus *status;
	JabberBuddy *jb = jabber_buddy_find(js, who, TRUE);
	xmlnode *presence;
	JabberBuddyState state;
	char *msg;
	int priority;

	account   = purple_connection_get_account(js->gc);
	gpresence = purple_account_get_presence(account);
	status    = purple_presence_get_active_status(gpresence);

	purple_status_to_jabber(status, &state, &msg, &priority);
	presence = jabber_presence_create(state, msg, priority);

	g_free(msg);

	xmlnode_set_attrib(presence, "to", who);
	if(invisible) {
		xmlnode_set_attrib(presence, "type", "invisible");
		jb->invisible |= JABBER_INVIS_BUDDY;
	} else {
		jb->invisible &= ~JABBER_INVIS_BUDDY;
	}

	jabber_send(js, presence);
	xmlnode_free(presence);
}

static void jabber_buddy_make_invisible(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	JabberStream *js;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	js = gc->proto_data;

	jabber_buddy_set_invisibility(js, buddy->name, TRUE);
}

static void jabber_buddy_make_visible(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	JabberStream *js;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	js = gc->proto_data;

	jabber_buddy_set_invisibility(js, buddy->name, FALSE);
}

static void jabber_buddy_cancel_presence_notification(PurpleBlistNode *node,
		gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	JabberStream *js;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	js = gc->proto_data;

	/* I wonder if we should prompt the user before doing this */
	jabber_presence_subscription_set(js, buddy->name, "unsubscribed");
}

static void jabber_buddy_rerequest_auth(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	JabberStream *js;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	js = gc->proto_data;

	jabber_presence_subscription_set(js, buddy->name, "subscribe");
}


static void jabber_buddy_unsubscribe(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	JabberStream *js;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	js = gc->proto_data;

	jabber_presence_subscription_set(js, buddy->name, "unsubscribe");
}


static GList *jabber_buddy_menu(PurpleBuddy *buddy)
{
	PurpleConnection *gc = purple_account_get_connection(buddy->account);
	JabberStream *js = gc->proto_data;
	JabberBuddy *jb = jabber_buddy_find(js, buddy->name, TRUE);

	GList *m = NULL;
	PurpleMenuAction *act;

	if(!jb)
		return m;

	/* XXX: fix the NOT ME below */

	if(js->protocol_version == JABBER_PROTO_0_9 /* && NOT ME */) {
		if(jb->invisible & JABBER_INVIS_BUDDY) {
			act = purple_menu_action_new(_("Un-hide From"),
			                           PURPLE_CALLBACK(jabber_buddy_make_visible),
			                           NULL, NULL);
		} else {
			act = purple_menu_action_new(_("Temporarily Hide From"),
			                           PURPLE_CALLBACK(jabber_buddy_make_invisible),
			                           NULL, NULL);
		}
		m = g_list_append(m, act);
	}

	if(jb->subscription & JABBER_SUB_FROM /* && NOT ME */) {
		act = purple_menu_action_new(_("Cancel Presence Notification"),
		                           PURPLE_CALLBACK(jabber_buddy_cancel_presence_notification),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	if(!(jb->subscription & JABBER_SUB_TO)) {
		act = purple_menu_action_new(_("(Re-)Request authorization"),
		                           PURPLE_CALLBACK(jabber_buddy_rerequest_auth),
		                           NULL, NULL);
		m = g_list_append(m, act);

	} else /* if(NOT ME) */{

		/* shouldn't this just happen automatically when the buddy is
		   removed? */
		act = purple_menu_action_new(_("Unsubscribe"),
		                           PURPLE_CALLBACK(jabber_buddy_unsubscribe),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

GList *
jabber_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		return jabber_buddy_menu((PurpleBuddy *) node);
	} else {
		return NULL;
	}
}


const char *
jabber_buddy_state_get_name(JabberBuddyState state)
{
	switch(state) {
		case JABBER_BUDDY_STATE_UNKNOWN:
			return _("Unknown");
		case JABBER_BUDDY_STATE_ERROR:
			return _("Error");
		case JABBER_BUDDY_STATE_UNAVAILABLE:
			return _("Offline");
		case JABBER_BUDDY_STATE_ONLINE:
			return _("Available");
		case JABBER_BUDDY_STATE_CHAT:
			return _("Chatty");
		case JABBER_BUDDY_STATE_AWAY:
			return _("Away");
		case JABBER_BUDDY_STATE_XA:
			return _("Extended Away");
		case JABBER_BUDDY_STATE_DND:
			return _("Do Not Disturb");
	}

	return _("Unknown");
}

JabberBuddyState jabber_buddy_status_id_get_state(const char *id) {
	if(!id)
		return JABBER_BUDDY_STATE_UNKNOWN;
	if(!strcmp(id, "available"))
		return JABBER_BUDDY_STATE_ONLINE;
	if(!strcmp(id, "freeforchat"))
		return JABBER_BUDDY_STATE_CHAT;
	if(!strcmp(id, "away"))
		return JABBER_BUDDY_STATE_AWAY;
	if(!strcmp(id, "extended_away"))
		return JABBER_BUDDY_STATE_XA;
	if(!strcmp(id, "dnd"))
		return JABBER_BUDDY_STATE_DND;
	if(!strcmp(id, "offline"))
		return JABBER_BUDDY_STATE_UNAVAILABLE;
	if(!strcmp(id, "error"))
		return JABBER_BUDDY_STATE_ERROR;

	return JABBER_BUDDY_STATE_UNKNOWN;
}

JabberBuddyState jabber_buddy_show_get_state(const char *id) {
	if(!id)
		return JABBER_BUDDY_STATE_UNKNOWN;
	if(!strcmp(id, "available"))
		return JABBER_BUDDY_STATE_ONLINE;
	if(!strcmp(id, "chat"))
		return JABBER_BUDDY_STATE_CHAT;
	if(!strcmp(id, "away"))
		return JABBER_BUDDY_STATE_AWAY;
	if(!strcmp(id, "xa"))
		return JABBER_BUDDY_STATE_XA;
	if(!strcmp(id, "dnd"))
		return JABBER_BUDDY_STATE_DND;
	if(!strcmp(id, "offline"))
		return JABBER_BUDDY_STATE_UNAVAILABLE;
	if(!strcmp(id, "error"))
		return JABBER_BUDDY_STATE_ERROR;

	return JABBER_BUDDY_STATE_UNKNOWN;
}

const char *jabber_buddy_state_get_show(JabberBuddyState state) {
	switch(state) {
		case JABBER_BUDDY_STATE_CHAT:
			return "chat";
		case JABBER_BUDDY_STATE_AWAY:
			return "away";
		case JABBER_BUDDY_STATE_XA:
			return "xa";
		case JABBER_BUDDY_STATE_DND:
			return "dnd";
		case JABBER_BUDDY_STATE_ONLINE:
			return "available";
		case JABBER_BUDDY_STATE_UNKNOWN:
		case JABBER_BUDDY_STATE_ERROR:
			return NULL;
		case JABBER_BUDDY_STATE_UNAVAILABLE:
			return "offline";
	}
	return NULL;
}

const char *jabber_buddy_state_get_status_id(JabberBuddyState state) {
	switch(state) {
		case JABBER_BUDDY_STATE_CHAT:
			return "freeforchat";
		case JABBER_BUDDY_STATE_AWAY:
			return "away";
		case JABBER_BUDDY_STATE_XA:
			return "extended_away";
		case JABBER_BUDDY_STATE_DND:
			return "dnd";
		case JABBER_BUDDY_STATE_ONLINE:
			return "available";
		case JABBER_BUDDY_STATE_UNKNOWN:
			return "available";
		case JABBER_BUDDY_STATE_ERROR:
			return "error";
		case JABBER_BUDDY_STATE_UNAVAILABLE:
			return "offline";
	}
	return NULL;
}

static void user_search_result_add_buddy_cb(PurpleConnection *gc, GList *row, void *user_data)
{
	/* XXX find out the jid */
	purple_blist_request_add_buddy(purple_connection_get_account(gc),
			g_list_nth_data(row, 0), NULL, NULL);
}

static void user_search_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	PurpleNotifySearchResults *results;
	PurpleNotifySearchColumn *column;
	xmlnode *x, *query, *item, *field;

	/* XXX error checking? */
	if(!(query = xmlnode_get_child(packet, "query")))
		return;

	results = purple_notify_searchresults_new();
	if((x = xmlnode_get_child_with_namespace(query, "x", "jabber:x:data"))) {
		xmlnode *reported;
		purple_debug_info("jabber", "new-skool\n");
		if((reported = xmlnode_get_child(x, "reported"))) {
			xmlnode *field = xmlnode_get_child(reported, "field");
			while(field) {
				/* XXX keep track of this order, use it below */
				const char *var = xmlnode_get_attrib(field, "var");
				const char *label = xmlnode_get_attrib(field, "label");
				if(var) {
					column = purple_notify_searchresults_column_new(label ? label : var);
					purple_notify_searchresults_column_add(results, column);
				}
				field = xmlnode_get_next_twin(field);
			}
		}
		item = xmlnode_get_child(x, "item");
		while(item) {
			GList *row = NULL;
			field = xmlnode_get_child(item, "field");
			while(field) {
				xmlnode *valuenode = xmlnode_get_child(field, "value");
				if(valuenode) {
					char *value = xmlnode_get_data(valuenode);
					row = g_list_append(row, value);
				}
				field = xmlnode_get_next_twin(field);
			}
			purple_notify_searchresults_row_add(results, row);

			item = xmlnode_get_next_twin(item);
		}
	} else {
		/* old skool */
		purple_debug_info("jabber", "old-skool\n");

		column = purple_notify_searchresults_column_new(_("JID"));
		purple_notify_searchresults_column_add(results, column);
		column = purple_notify_searchresults_column_new(_("First Name"));
		purple_notify_searchresults_column_add(results, column);
		column = purple_notify_searchresults_column_new(_("Last Name"));
		purple_notify_searchresults_column_add(results, column);
		column = purple_notify_searchresults_column_new(_("Nickname"));
		purple_notify_searchresults_column_add(results, column);
		column = purple_notify_searchresults_column_new(_("E-Mail"));
		purple_notify_searchresults_column_add(results, column);

		for(item = xmlnode_get_child(query, "item"); item; item = xmlnode_get_next_twin(item)) {
			const char *jid;
			xmlnode *node;
			GList *row = NULL;

			if(!(jid = xmlnode_get_attrib(item, "jid")))
				continue;

			row = g_list_append(row, g_strdup(jid));
			node = xmlnode_get_child(item, "first");
			row = g_list_append(row, node ? xmlnode_get_data(node) : NULL);
			node = xmlnode_get_child(item, "last");
			row = g_list_append(row, node ? xmlnode_get_data(node) : NULL);
			node = xmlnode_get_child(item, "nick");
			row = g_list_append(row, node ? xmlnode_get_data(node) : NULL);
			node = xmlnode_get_child(item, "email");
			row = g_list_append(row, node ? xmlnode_get_data(node) : NULL);
			purple_debug_info("jabber", "row=%d\n", row);
			purple_notify_searchresults_row_add(results, row);
		}
	}

	purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_ADD,
			user_search_result_add_buddy_cb);

	purple_notify_searchresults(js->gc, NULL, NULL, _("The following are the results of your search"), results, NULL, NULL);
}

static void user_search_x_data_cb(JabberStream *js, xmlnode *result, gpointer data)
{
	xmlnode *query;
	JabberIq *iq;
	char *dir_server = data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:search");
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_insert_child(query, result);

	jabber_iq_set_callback(iq, user_search_result_cb, NULL);
	xmlnode_set_attrib(iq->node, "to", dir_server);
	jabber_iq_send(iq);
	g_free(dir_server);
}

struct user_search_info {
	JabberStream *js;
	char *directory_server;
};

static void user_search_cancel_cb(struct user_search_info *usi, PurpleRequestFields *fields)
{
	g_free(usi->directory_server);
	g_free(usi);
}

static void user_search_cb(struct user_search_info *usi, PurpleRequestFields *fields)
{
	JabberStream *js = usi->js;
	JabberIq *iq;
	xmlnode *query;
	GList *groups, *flds;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:search");
	query = xmlnode_get_child(iq->node, "query");

	for(groups = purple_request_fields_get_groups(fields); groups; groups = groups->next) {
		for(flds = purple_request_field_group_get_fields(groups->data);
				flds; flds = flds->next) {
			PurpleRequestField *field = flds->data;
			const char *id = purple_request_field_get_id(field);
			const char *value = purple_request_field_string_get_value(field);

			if(value && (!strcmp(id, "first") || !strcmp(id, "last") || !strcmp(id, "nick") || !strcmp(id, "email"))) {
				xmlnode *y = xmlnode_new_child(query, id);
				xmlnode_insert_data(y, value, -1);
			}
		}
	}

	jabber_iq_set_callback(iq, user_search_result_cb, NULL);
	xmlnode_set_attrib(iq->node, "to", usi->directory_server);
	jabber_iq_send(iq);

	g_free(usi->directory_server);
	g_free(usi);
}

#if 0
/* This is for gettext only -- it will see this even though there's an #if 0. */

/*
 * An incomplete list of server generated original language search
 * comments for Jabber User Directories
 *
 * See discussion thread "Search comment for Jabber is not translatable"
 * in purple-i18n@lists.sourceforge.net (March 2006)
 */
static const char * jabber_user_dir_comments [] = {
       /* current comment from Jabber User Directory users.jabber.org */
       N_("Find a contact by entering the search criteria in the given fields. "
          "Note: Each field supports wild card searches (%)"),
       NULL
};
#endif

static void user_search_fields_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	xmlnode *query, *x;
	const char *from, *type;

	if(!(from = xmlnode_get_attrib(packet, "from")))
		return;

	if(!(type = xmlnode_get_attrib(packet, "type")) || !strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);

		if(!msg)
			msg = g_strdup(_("Unknown error"));

		purple_notify_error(js->gc, _("Directory Query Failed"),
				  _("Could not query the directory server."), msg);
		g_free(msg);

		return;
	}


	if(!(query = xmlnode_get_child(packet, "query")))
		return;

	if((x = xmlnode_get_child_with_namespace(query, "x", "jabber:x:data"))) {
		jabber_x_data_request(js, x, user_search_x_data_cb, g_strdup(from));
		return;
	} else {
		struct user_search_info *usi;
		xmlnode *instnode;
		char *instructions = NULL;
		PurpleRequestFields *fields;
		PurpleRequestFieldGroup *group;
		PurpleRequestField *field;

		/* old skool */
		fields = purple_request_fields_new();
		group = purple_request_field_group_new(NULL);
		purple_request_fields_add_group(fields, group);

		if((instnode = xmlnode_get_child(query, "instructions")))
		{
			char *tmp = xmlnode_get_data(instnode);

			if(tmp)
			{
				/* Try to translate the message (see static message
				   list in jabber_user_dir_comments[]) */
				instructions = g_strdup_printf(_("Server Instructions: %s"), _(tmp));
				g_free(tmp);
			}
		}

		if(!instructions)
		{
			instructions = g_strdup(_("Fill in one or more fields to search "
						  "for any matching Jabber users."));
		}

		if(xmlnode_get_child(query, "first")) {
			field = purple_request_field_string_new("first", _("First Name"),
					NULL, FALSE);
			purple_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "last")) {
			field = purple_request_field_string_new("last", _("Last Name"),
					NULL, FALSE);
			purple_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "nick")) {
			field = purple_request_field_string_new("nick", _("Nickname"),
					NULL, FALSE);
			purple_request_field_group_add_field(group, field);
		}
		if(xmlnode_get_child(query, "email")) {
			field = purple_request_field_string_new("email", _("E-Mail Address"),
					NULL, FALSE);
			purple_request_field_group_add_field(group, field);
		}

		usi = g_new0(struct user_search_info, 1);
		usi->js = js;
		usi->directory_server = g_strdup(from);

		purple_request_fields(js->gc, _("Search for Jabber users"),
				_("Search for Jabber users"), instructions, fields,
				_("Search"), G_CALLBACK(user_search_cb),
				_("Cancel"), G_CALLBACK(user_search_cancel_cb), usi);

		g_free(instructions);
	}
}

static void jabber_user_search_ok(JabberStream *js, const char *directory)
{
	JabberIq *iq;

	/* XXX: should probably better validate the directory we're given */
	if(!directory || !*directory) {
		purple_notify_error(js->gc, _("Invalid Directory"), _("Invalid Directory"), NULL);
		return;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:search");
	xmlnode_set_attrib(iq->node, "to", directory);

	jabber_iq_set_callback(iq, user_search_fields_result_cb, NULL);

	jabber_iq_send(iq);
}

void jabber_user_search_begin(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	JabberStream *js = gc->proto_data;

	purple_request_input(gc, _("Enter a User Directory"), _("Enter a User Directory"),
			_("Select a user directory to search"),
			js->user_directories ? js->user_directories->data : "users.jabber.org",
			FALSE, FALSE, NULL,
			_("Search Directory"), PURPLE_CALLBACK(jabber_user_search_ok),
			_("Cancel"), NULL, js);
}



