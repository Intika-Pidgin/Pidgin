/*
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

#include "purpleuiinfo.h"

struct _PurpleUiInfo {
	GObject parent;

	gchar *name;
	gchar *version;
	gchar *website;
	gchar *support_website;
	gchar *client_type;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_VERSION,
	PROP_WEBSITE,
	PROP_SUPPORT_WEBSITE,
	PROP_CLIENT_TYPE,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_ui_info_set_name(PurpleUiInfo *info, const gchar *name) {
	g_free(info->name);
	info->name = g_strdup(name);
}

static void
purple_ui_info_set_version(PurpleUiInfo *info, const gchar *version) {
	g_free(info->version);
	info->version = g_strdup(version);
}

static void
purple_ui_info_set_website(PurpleUiInfo *info, const gchar *website) {
	g_free(info->website);
	info->website = g_strdup(website);
}

static void
purple_ui_info_set_support_website(PurpleUiInfo *info,
                                   const gchar *support_website)
{
	g_free(info->support_website);
	info->support_website = g_strdup(support_website);
}

static void
purple_ui_info_set_client_type(PurpleUiInfo *info, const gchar *client_type) {
	g_free(info->client_type);
	info->client_type = g_strdup(client_type);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PurpleUiInfo, purple_ui_info, G_TYPE_OBJECT);

static void
purple_ui_info_get_property(GObject *obj, guint param_id, GValue *value,
                            GParamSpec *pspec)
{
	PurpleUiInfo *info = PURPLE_UI_INFO(obj);

	switch(param_id) {
		case PROP_NAME:
			g_value_set_string(value, purple_ui_info_get_name(info));
			break;
		case PROP_VERSION:
			g_value_set_string(value, purple_ui_info_get_version(info));
			break;
		case PROP_WEBSITE:
			g_value_set_string(value, purple_ui_info_get_website(info));
			break;
		case PROP_SUPPORT_WEBSITE:
			g_value_set_string(value, purple_ui_info_get_support_website(info));
			break;
		case PROP_CLIENT_TYPE:
			g_value_set_string(value, purple_ui_info_get_client_type(info));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_ui_info_set_property(GObject *obj, guint param_id, const GValue *value,
                            GParamSpec *pspec)
{
	PurpleUiInfo *info = PURPLE_UI_INFO(obj);

	switch(param_id) {
		case PROP_NAME:
			purple_ui_info_set_name(info, g_value_get_string(value));
			break;
		case PROP_VERSION:
			purple_ui_info_set_version(info, g_value_get_string(value));
			break;
		case PROP_WEBSITE:
			purple_ui_info_set_website(info, g_value_get_string(value));
			break;
		case PROP_SUPPORT_WEBSITE:
			purple_ui_info_set_support_website(info,
			                                   g_value_get_string(value));
			break;
		case PROP_CLIENT_TYPE:
			purple_ui_info_set_client_type(info, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_ui_info_init(PurpleUiInfo *info) {
}

static void
purple_ui_info_finalize(GObject *obj) {
	PurpleUiInfo *info = PURPLE_UI_INFO(obj);

	g_clear_pointer(&info->name, g_free);
	g_clear_pointer(&info->version, g_free);
	g_clear_pointer(&info->website, g_free);
	g_clear_pointer(&info->support_website, g_free);
	g_clear_pointer(&info->client_type, g_free);

	G_OBJECT_CLASS(purple_ui_info_parent_class)->finalize(obj);
}

static void
purple_ui_info_class_init(PurpleUiInfoClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_ui_info_get_property;
	obj_class->set_property = purple_ui_info_set_property;
	obj_class->finalize = purple_ui_info_finalize;

	/**
	 * PurpleUiInfo::name:
	 *
	 * The name of the user interface.
	 */
	properties[PROP_NAME] =
		g_param_spec_string("name", "name", "The name of the user interface",
		                    NULL,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                    G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleUiInfo::version:
	 *
	 * The name of the user interface.
	 */
	properties[PROP_VERSION] =
		g_param_spec_string("version", "version",
		                    "The version of the user interface",
		                    NULL,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                    G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleUiInfo::website:
	 *
	 * The website of the user interface.
	 */
	properties[PROP_WEBSITE] =
		g_param_spec_string("website", "website",
		                    "The website of the user interface",
		                    NULL,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                    G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleUiInfo::support-website:
	 *
	 * The support website of the user interface.
	 */
	properties[PROP_SUPPORT_WEBSITE] =
		g_param_spec_string("support-website", "support-website",
		                    "The support website of the user interface",
		                    NULL,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                    G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleUiInfo::client-type:
	 *
	 * The client type of the user interface.
	 */
	properties[PROP_CLIENT_TYPE] =
		g_param_spec_string("client-type", "client-type",
		                    "The client type of the user interface",
		                    NULL,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                    G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleUiInfo *
purple_ui_info_new(const gchar *name, const gchar *version,
                   const gchar *website, const gchar *support_website,
                   const gchar *client_type)
{
	return g_object_new(PURPLE_TYPE_UI_INFO,
	                    "name", name,
	                    "version", version,
	                    "website", website,
	                    "support-website", support_website,
	                    "client-type", client_type,
	                    NULL);
}

const gchar *
purple_ui_info_get_name(PurpleUiInfo *info) {
	g_return_val_if_fail(PURPLE_IS_UI_INFO(info), NULL);

	return info->name;
}

const gchar *
purple_ui_info_get_version(PurpleUiInfo *info) {
	g_return_val_if_fail(PURPLE_IS_UI_INFO(info), NULL);

	return info->version;
}

const gchar *
purple_ui_info_get_website(PurpleUiInfo *info) {
	g_return_val_if_fail(PURPLE_IS_UI_INFO(info), NULL);

	return info->website;
}

const gchar *
purple_ui_info_get_support_website(PurpleUiInfo *info) {
	g_return_val_if_fail(PURPLE_IS_UI_INFO(info), NULL);

	return info->support_website;
}

const gchar *
purple_ui_info_get_client_type(PurpleUiInfo *info) {
	g_return_val_if_fail(PURPLE_IS_UI_INFO(info), NULL);

	return info->client_type;
}

