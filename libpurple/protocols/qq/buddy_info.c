/**
 * @file buddy_info.c
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "internal.h"
#include "debug.h"
#include "notify.h"
#include "request.h"

#include "utils.h"
#include "packet_parse.h"
#include "buddy_info.h"
#include "char_conv.h"
#include "crypt.h"
#include "header_info.h"
#include "keep_alive.h"
#include "send_core.h"

#define QQ_PRIMARY_INFORMATION _("Primary Information")
#define QQ_ADDITIONAL_INFORMATION _("Additional Information")
#define QQ_INTRO _("Personal Introduction")
#define QQ_NUMBER _("QQ Number")
#define QQ_NICKNAME _("Nickname")
#define QQ_NAME _("Name")
#define QQ_AGE _("Age")
#define QQ_GENDER _("Gender")
#define QQ_COUNTRY _("Country/Region")
#define QQ_PROVINCE _("Province/State")
#define QQ_CITY _("City")
#define QQ_HOROSCOPE _("Horoscope Symbol")
#define QQ_OCCUPATION _("Occupation")
#define QQ_ZODIAC _("Zodiac Sign")
#define QQ_BLOOD _("Blood Type")
#define QQ_COLLEGE _("College")
#define QQ_EMAIL _("Email")
#define QQ_ADDRESS _("Address")
#define QQ_ZIPCODE _("Zipcode")
#define QQ_CELL _("Cellphone Number")
#define QQ_TELEPHONE _("Phone Number")
#define QQ_HOMEPAGE _("Homepage")

#define QQ_HOROSCOPE_SIZE 13
static const gchar *horoscope_names[] = {
	"-", N_("Aquarius"), N_("Pisces"), N_("Aries"), N_("Taurus"),
	N_("Gemini"), N_("Cancer"), N_("Leo"), N_("Virgo"), N_("Libra"),
	N_("Scorpio"), N_("Sagittarius"), N_("Capricorn")
};

#define QQ_ZODIAC_SIZE 13
static const gchar *zodiac_names[] = {
	"-", N_("Rat"), N_("Ox"), N_("Tiger"), N_("Rabbit"),
	N_("Dragon"), N_("Snake"), N_("Horse"), N_("Goat"), N_("Monkey"),
	N_("Rooster"), N_("Dog"), N_("Pig")
};

#define QQ_BLOOD_SIZE 6
static const gchar *blood_types[] = {
	"-", "A", "B", "O", "AB", N_("Other")
};

#define QQ_GENDER_SIZE 2
static const gchar *genders[] = {
	N_("Male"),
	N_("Female")
};

#define QQ_CONTACT_FIELDS                               37

/* There is no user id stored in the reply packet for information query
 * we have to manually store the query, so that we know the query source */
typedef struct _qq_info_query {
	guint32 uid;
	gboolean show_window;
	gboolean modify_info;
} qq_info_query;

/* We get an info packet on ourselves before we modify our information.
 * Even though not all of the information is modifiable, it still
 * all needs to be there when we send out the modify info packet */
typedef struct _modify_info_data {
	GaimConnection *gc;
	contact_info *info;
} modify_info_data;

/* return -1 as a sentinel */
static gint choice_index(const gchar *value, const gchar **choice, gint choice_size)
{
	gint len, i;

	len = strlen(value);
	if (len > 3 || len == 0) return -1;
	for (i = 0; i < len; i++) {
		if (!g_ascii_isdigit(value[i]))
			return -1;
	}
	i = strtol(value, NULL, 10);
	if (i >= choice_size)
		return -1;

	return i;
}

/* return should be freed */
static gchar *field_value(const gchar *field, const gchar **choice, gint choice_size)
{
	gint index, len;

	len = strlen(field);
	if (len == 0) {
		return NULL;
	} else if (choice != NULL) {
		/* some choice fields are also customizable */
		index = choice_index(field, choice, choice_size);
		if (index == -1) {
			if (strcmp(field, "-") != 0) {
				return qq_to_utf8(field, QQ_CHARSET_DEFAULT);
			} else {
				return NULL;
			}
		/* else ASCIIized index */
		} else {
			if (strcmp(choice[index], "-") != 0)
				return g_strdup(choice[index]);
			else
				return NULL;
		}
	} else {
		if (strcmp(field, "-") != 0) {
			return qq_to_utf8(field, QQ_CHARSET_DEFAULT);
		} else {
			return NULL;
		}
	}
}

static gboolean append_field_value(GaimNotifyUserInfo *user_info, const gchar *field,
		const gchar *title, const gchar **choice, gint choice_size)
{
	gchar *value = field_value(field, choice, choice_size);

	if (value != NULL) {
		gaim_notify_user_info_add_pair(user_info, title, value);
		g_free(value);
		
		return TRUE;
	}
	
	return FALSE;
}

static GaimNotifyUserInfo *
info_to_notify_user_info(const contact_info *info)
{
	GaimNotifyUserInfo *user_info = gaim_notify_user_info_new();
	const gchar *intro;
	gboolean has_extra_info = FALSE;

	gaim_notify_user_info_add_pair(user_info, QQ_NUMBER, info->uid);

	append_field_value(user_info, info->nick, QQ_NICKNAME, NULL, 0);
	append_field_value(user_info, info->name, QQ_NAME, NULL, 0);
	append_field_value(user_info, info->age, QQ_AGE, NULL, 0);
	append_field_value(user_info, info->gender, QQ_GENDER, genders, QQ_GENDER_SIZE);
	append_field_value(user_info, info->country, QQ_COUNTRY, NULL, 0);
	append_field_value(user_info, info->province, QQ_PROVINCE, NULL, 0);
	append_field_value(user_info, info->city, QQ_CITY, NULL, 0);

	gaim_notify_user_info_add_section_header(user_info, QQ_ADDITIONAL_INFORMATION);

	has_extra_info |= append_field_value(user_info, info->horoscope, QQ_HOROSCOPE, horoscope_names, QQ_HOROSCOPE_SIZE);
	has_extra_info |= append_field_value(user_info, info->occupation, QQ_OCCUPATION, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->zodiac, QQ_ZODIAC, zodiac_names, QQ_ZODIAC_SIZE);
	has_extra_info |= append_field_value(user_info, info->blood, QQ_BLOOD, blood_types, QQ_BLOOD_SIZE);
	has_extra_info |= append_field_value(user_info, info->college, QQ_COLLEGE, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->email, QQ_EMAIL, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->address, QQ_ADDRESS, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->zipcode, QQ_ZIPCODE, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->hp_num, QQ_CELL, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->tel, QQ_TELEPHONE, NULL, 0);
	has_extra_info |= append_field_value(user_info, info->homepage, QQ_HOMEPAGE, NULL, 0);

	if (!has_extra_info)
		gaim_notify_user_info_remove_last_item(user_info);

	intro = field_value(info->intro, NULL, 0);
	if (intro) {
		gaim_notify_user_info_add_pair(user_info, QQ_INTRO, intro);
	}

	/* for debugging */
	/*
	g_string_append_printf(info_text, "<br /><br /><b>%s</b><br />", "Miscellaneous");
	append_field_value(info_text, info->pager_sn, "pager_sn", NULL, 0);
	append_field_value(info_text, info->pager_num, "pager_num", NULL, 0);
	append_field_value(info_text, info->pager_sp, "pager_sp", NULL, 0);
	append_field_value(info_text, info->pager_base_num, "pager_base_num", NULL, 0);
	append_field_value(info_text, info->pager_type, "pager_type", NULL, 0);
	append_field_value(info_text, info->auth_type, "auth_type", NULL, 0);
	append_field_value(info_text, info->unknown1, "unknown1", NULL, 0);
	append_field_value(info_text, info->unknown2, "unknown2", NULL, 0);
	append_field_value(info_text, info->face, "face", NULL, 0);
	append_field_value(info_text, info->hp_type, "hp_type", NULL, 0);
	append_field_value(info_text, info->unknown3, "unknown3", NULL, 0);
	append_field_value(info_text, info->unknown4, "unknown4", NULL, 0);
	append_field_value(info_text, info->unknown5, "unknown5", NULL, 0);
	append_field_value(info_text, info->is_open_hp, "is_open_hp", NULL, 0);
	append_field_value(info_text, info->is_open_contact, "is_open_contact", NULL, 0);
	append_field_value(info_text, info->qq_show, "qq_show", NULL, 0);
	append_field_value(info_text, info->unknown6, "unknown6", NULL, 0);
	*/

	return user_info;
}

/* send a packet to get detailed information of uid */
void qq_send_packet_get_info(GaimConnection *gc, guint32 uid, gboolean show_window)
{
	qq_data *qd;
	gchar uid_str[11];
	qq_info_query *query;

	g_return_if_fail(uid != 0);

	qd = (qq_data *) gc->proto_data;
	g_snprintf(uid_str, sizeof(uid_str), "%d", uid);
	qq_send_cmd(gc, QQ_CMD_GET_USER_INFO, TRUE, 0, TRUE, (guint8 *) uid_str, strlen(uid_str));

	query = g_new0(qq_info_query, 1);
	query->uid = uid;
	query->show_window = show_window;
	query->modify_info = FALSE;
	qd->info_query = g_list_append(qd->info_query, query);
}

/* set up the fields requesting personal information and send a get_info packet
 * for myself */
void qq_prepare_modify_info(GaimConnection *gc)
{
	qq_data *qd;
	GList *ql;
	qq_info_query *query;

	qd = (qq_data *) gc->proto_data;
	qq_send_packet_get_info(gc, qd->uid, FALSE);
	/* traverse backwards so we get the most recent info_query */
	for (ql = g_list_last(qd->info_query); ql != NULL; ql = g_list_previous(ql)) {
		query = ql->data;
		if (query->uid == qd->uid)
			query->modify_info = TRUE;
	}
}

/* send packet to modify personal information */
static void qq_send_packet_modify_info(GaimConnection *gc, gchar **segments)
{
	gint i;
	guint8 *raw_data, *cursor, bar;

	g_return_if_fail(segments != NULL);

	bar = 0x1f;
	raw_data = g_newa(guint8, MAX_PACKET_SIZE - 128);
	cursor = raw_data;

	create_packet_b(raw_data, &cursor, bar);

	/* important! skip the first uid entry */
	for (i = 1; i < QQ_CONTACT_FIELDS; i++) {
		create_packet_b(raw_data, &cursor, bar);
		create_packet_data(raw_data, &cursor, (guint8 *) segments[i], strlen(segments[i]));
	}
	create_packet_b(raw_data, &cursor, bar);

	qq_send_cmd(gc, QQ_CMD_UPDATE_INFO, TRUE, 0, TRUE, raw_data, cursor - raw_data);

}

static void modify_info_cancel_cb(modify_info_data *mid)
{
	qq_data *qd;

	qd = (qq_data *) mid->gc->proto_data;
	qd->modifying_info = FALSE;

	g_strfreev((gchar **) mid->info);
	g_free(mid);
}

static gchar *parse_field(GList **list, gboolean choice)
{
	gchar *value;
	GaimRequestField *field;

	field = (GaimRequestField *) (*list)->data;
	if (choice) {
		value = g_strdup_printf("%d", gaim_request_field_choice_get_value(field));
	} else {
		value = (gchar *) gaim_request_field_string_get_value(field);
		if (value == NULL)
			value = g_strdup("-");
		else
			value = utf8_to_qq(value, QQ_CHARSET_DEFAULT);
	}
	*list = g_list_remove_link(*list, *list);

	return value;
}

/* parse fields and send info packet */
static void modify_info_ok_cb(modify_info_data *mid, GaimRequestFields *fields)
{
	GaimConnection *gc;
	qq_data *qd;
	GList *list,  *groups;
	contact_info *info;

	gc = mid->gc;
	qd = (qq_data *) gc->proto_data;
	qd->modifying_info = FALSE;

	info = mid->info;

	groups = gaim_request_fields_get_groups(fields);
	list = gaim_request_field_group_get_fields(groups->data);
	info->uid = parse_field(&list, FALSE);
	info->nick = parse_field(&list, FALSE);
	info->name = parse_field(&list, FALSE);
	info->age = parse_field(&list, FALSE);
	info->gender = parse_field(&list, TRUE);
	info->country = parse_field(&list, FALSE);
	info->province = parse_field(&list, FALSE);
	info->city = parse_field(&list, FALSE);
	groups = g_list_remove_link(groups, groups);
	list = gaim_request_field_group_get_fields(groups->data);
	info->horoscope = parse_field(&list, TRUE);
	info->occupation = parse_field(&list, FALSE);
	info->zodiac = parse_field(&list, TRUE);
	info->blood = parse_field(&list, TRUE);
	info->college = parse_field(&list, FALSE);
	info->email = parse_field(&list, FALSE);
	info->address = parse_field(&list, FALSE);
	info->zipcode = parse_field(&list, FALSE);
	info->hp_num = parse_field(&list, FALSE);
	info->tel = parse_field(&list, FALSE);
	info->homepage = parse_field(&list, FALSE);
	groups = g_list_remove_link(groups, groups);
	list = gaim_request_field_group_get_fields(groups->data);
	info->intro = parse_field(&list, FALSE);
	groups = g_list_remove_link(groups, groups);

	qq_send_packet_modify_info(gc, (gchar **) info);

	g_strfreev((gchar **) mid->info);
	g_free(mid);
}

static GaimRequestFieldGroup *setup_field_group(GaimRequestFields *fields, const gchar *title)
{
	GaimRequestFieldGroup *group;

	group = gaim_request_field_group_new(title);
	gaim_request_fields_add_group(fields, group);

	return group;
}

static void add_string_field_to_group(GaimRequestFieldGroup *group,
		const gchar *id, const gchar *title, const gchar *value)
{
	GaimRequestField *field;
	gchar *utf8_value;

	utf8_value = qq_to_utf8(value, QQ_CHARSET_DEFAULT);
	field = gaim_request_field_string_new(id, title, utf8_value, FALSE);
	gaim_request_field_group_add_field(group, field);
	g_free(utf8_value);
}

static void add_choice_field_to_group(GaimRequestFieldGroup *group,
		const gchar *id, const gchar *title, const gchar *value,
		const gchar **choice, gint choice_size)
{
	GaimRequestField *field;
	gint i, index;

	index = choice_index(value, choice, choice_size);
	field = gaim_request_field_choice_new(id, title, index);
	for (i = 0; i < choice_size; i++)
		gaim_request_field_choice_add(field, choice[i]);
	gaim_request_field_group_add_field(group, field);
}

/* take the info returned by a get_info packet for myself and set up a request form */
static void create_modify_info_dialogue(GaimConnection *gc, const contact_info *info)
{
	qq_data *qd;
	GaimRequestFieldGroup *group;
	GaimRequestFields *fields;
	GaimRequestField *field;
	modify_info_data *mid;

	/* so we only have one dialog open at a time */
	qd = (qq_data *) gc->proto_data;
	if (!qd->modifying_info) {
		qd->modifying_info = TRUE;

		fields = gaim_request_fields_new();

		group = setup_field_group(fields, QQ_PRIMARY_INFORMATION);
		field = gaim_request_field_string_new("uid", QQ_NUMBER, info->uid, FALSE);
		gaim_request_field_group_add_field(group, field);
		gaim_request_field_string_set_editable(field, FALSE);
		add_string_field_to_group(group, "nick", QQ_NICKNAME, info->nick);
		add_string_field_to_group(group, "name", QQ_NAME, info->name);
		add_string_field_to_group(group, "age", QQ_AGE, info->age);
		add_choice_field_to_group(group, "gender", QQ_GENDER, info->gender, genders, QQ_GENDER_SIZE);
		add_string_field_to_group(group, "country", QQ_COUNTRY, info->country);
		add_string_field_to_group(group, "province", QQ_PROVINCE, info->province);
		add_string_field_to_group(group, "city", QQ_CITY, info->city);
		group = setup_field_group(fields, QQ_ADDITIONAL_INFORMATION);
		add_choice_field_to_group(group, "horoscope", QQ_HOROSCOPE, info->horoscope, horoscope_names, QQ_HOROSCOPE_SIZE);
		add_string_field_to_group(group, "occupation", QQ_OCCUPATION, info->occupation);
		add_choice_field_to_group(group, "zodiac", QQ_ZODIAC, info->zodiac, zodiac_names, QQ_ZODIAC_SIZE);
		add_choice_field_to_group(group, "blood", QQ_BLOOD, info->blood, blood_types, QQ_BLOOD_SIZE);
		add_string_field_to_group(group, "college", QQ_COLLEGE, info->college);
		add_string_field_to_group(group, "email", QQ_EMAIL, info->email);
		add_string_field_to_group(group, "address", QQ_ADDRESS, info->address);
		add_string_field_to_group(group, "zipcode", QQ_ZIPCODE, info->zipcode);
		add_string_field_to_group(group, "hp_num", QQ_CELL, info->hp_num);
		add_string_field_to_group(group, "tel", QQ_TELEPHONE, info->tel);
		add_string_field_to_group(group, "homepage", QQ_HOMEPAGE, info->homepage);

		group = setup_field_group(fields, QQ_INTRO);
		field = gaim_request_field_string_new("intro", QQ_INTRO, info->intro, TRUE);
		gaim_request_field_group_add_field(group, field);

		/* prepare unmodifiable info */
		mid = g_new0(modify_info_data, 1);
		mid->gc = gc;
		/* QQ_CONTACT_FIELDS+1 so that the array is NULL-terminated and can be g_strfreev()'ed later */
		mid->info = (contact_info *) g_new0(gchar *, QQ_CONTACT_FIELDS+1);
		mid->info->pager_sn = g_strdup(info->pager_sn);
		mid->info->pager_num = g_strdup(info->pager_num);
		mid->info->pager_sp = g_strdup(info->pager_sp);
		mid->info->pager_base_num = g_strdup(info->pager_base_num);
		mid->info->pager_type = g_strdup(info->pager_type);
		mid->info->auth_type = g_strdup(info->auth_type);
		mid->info->unknown1 = g_strdup(info->unknown1);
		mid->info->unknown2 = g_strdup(info->unknown2);
		mid->info->face = g_strdup(info->face);
		mid->info->hp_type = g_strdup(info->hp_type);
		mid->info->unknown3 = g_strdup(info->unknown3);
		mid->info->unknown4 = g_strdup(info->unknown4);
		mid->info->unknown5 = g_strdup(info->unknown5);
		/* TODO stop hiding these 2 */
		mid->info->is_open_hp = g_strdup(info->is_open_hp);
		mid->info->is_open_contact = g_strdup(info->is_open_contact);
		mid->info->qq_show = g_strdup(info->qq_show);
		mid->info->unknown6 = g_strdup(info->unknown6);

		gaim_request_fields(gc, _("Modify my information"),
			_("Modify my information"), NULL, fields,
			_("Update my information"), G_CALLBACK(modify_info_ok_cb),
			_("Cancel"), G_CALLBACK(modify_info_cancel_cb),
			mid);
	}
}

/* process the reply of modify_info packet */
void qq_process_modify_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	qq_data *qd;
	gint len;
	guint8 *data;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		data[len] = '\0';
		if (qd->uid == atoi((gchar *) data)) {	/* return should be my uid */
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Update info ACK OK\n");
			gaim_notify_info(gc, NULL, _("Your information has been updated"), NULL);
		}
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt modify info reply\n");
	}
}

static void _qq_send_packet_modify_face(GaimConnection *gc, gint face_num)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimPresence *presence = gaim_account_get_presence(account);
	qq_data *qd = (qq_data *) gc->proto_data;
	gint offset;

	if(gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_INVISIBLE)) {
		offset = 2;
	} else if(gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_AWAY)
			|| gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_EXTENDED_AWAY)) {
		offset = 1;
	} else {
		offset = 0;
	}

	qd->my_icon = 3 * (face_num - 1) + offset;
	qd->modifying_face = TRUE;
	qq_send_packet_get_info(gc, qd->uid, FALSE);
}

void qq_set_buddy_icon_for_user(GaimAccount *account, const gchar *who, const gchar *iconfile)
{
	FILE *file;
	struct stat st;

	g_return_if_fail(g_stat(iconfile, &st) == 0);
	file = g_fopen(iconfile, "rb");
	if (file) {
		GaimBuddyIcon *icon;
		size_t data_len;
		gchar *data = g_new(gchar, st.st_size + 1);
		data_len = fread(data, 1, st.st_size, file);
		fclose(file);
		gaim_buddy_icons_set_for_user(account, who, data, data_len);
		icon = gaim_buddy_icons_find(account, who);
		gaim_buddy_icon_set_path(icon, iconfile);
	}
}

/* TODO: custom faces for QQ members and users with level >= 16 */
void qq_set_my_buddy_icon(GaimConnection *gc, const gchar *iconfile)
{
	gchar *icon;
	gint icon_num;
	gint icon_len;
	GaimAccount *account = gaim_connection_get_account(gc);
	const gchar *icon_path = gaim_account_get_buddy_icon_path(account);
	const gchar *buddy_icon_dir = qq_buddy_icon_dir();
	gint prefix_len = strlen(QQ_ICON_PREFIX);
	gint suffix_len = strlen(QQ_ICON_SUFFIX);
	gint dir_len = strlen(buddy_icon_dir);
	gchar *errmsg = g_strconcat(_("Setting custom faces is not currently supported. Please choose an image from "), buddy_icon_dir, ".", NULL);
	gboolean icon_global = gaim_account_get_bool(gc->account, "use-global-buddyicon", TRUE);

	if (!icon_path)
		icon_path = "";

	icon_len = strlen(icon_path) - dir_len - 1 - prefix_len - suffix_len;

	/* make sure we're using an appropriate icon */
	if (!(g_ascii_strncasecmp(icon_path, buddy_icon_dir, dir_len) == 0
		&& icon_path[dir_len] == G_DIR_SEPARATOR
			&& g_ascii_strncasecmp(icon_path + dir_len + 1, QQ_ICON_PREFIX, prefix_len) == 0
			&& g_ascii_strncasecmp(icon_path + dir_len + 1 + prefix_len + icon_len, QQ_ICON_SUFFIX, suffix_len) == 0
			&& icon_len <= 3)) {
		if (icon_global)
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "%s\n", errmsg);
		else
			gaim_notify_error(gc, _("Invalid QQ Face"), errmsg, NULL);
		g_free(errmsg);
		return;
	}
	/* strip everything but number */
	icon = g_strndup(icon_path + dir_len + 1 + prefix_len, icon_len);
	icon_num = strtol(icon, NULL, 10);
	g_free(icon);
	/* ensure face number in proper range */
	if (icon_num > QQ_FACES) {
		if (icon_global)
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "%s\n", errmsg);
		else
			gaim_notify_error(gc, _("Invalid QQ Face"), errmsg, NULL);
		g_free(errmsg);
		return;
	}
	g_free(errmsg);
	/* tell server my icon changed */
	_qq_send_packet_modify_face(gc, icon_num);
	/* display in blist */
	qq_set_buddy_icon_for_user(account, account->username, icon_path);
}


static void _qq_update_buddy_icon(GaimAccount *account, const gchar *name, gint face)
{
	gchar *icon_path;
	GaimBuddyIcon *icon = gaim_buddy_icons_find(account, name);
	gchar *icon_num_str = face_to_icon_str(face);
	const gchar *old_path = gaim_buddy_icon_get_path(icon);
	const gchar *buddy_icon_dir = qq_buddy_icon_dir();

	icon_path = g_strconcat(buddy_icon_dir, G_DIR_SEPARATOR_S, QQ_ICON_PREFIX, 
			icon_num_str, QQ_ICON_SUFFIX, NULL);
	if (icon == NULL || old_path == NULL 
		|| g_ascii_strcasecmp(icon_path, old_path) != 0)
		qq_set_buddy_icon_for_user(account, name, icon_path);
	g_free(icon_num_str);
	g_free(icon_path);
}

/* after getting info or modify myself, refresh the buddy list accordingly */
void qq_refresh_buddy_and_myself(contact_info *info, GaimConnection *gc)
{
	GaimBuddy *b;
	qq_data *qd;
	qq_buddy *q_bud;
	gchar *alias_utf8, *gaim_name;
	GaimAccount *account = gaim_connection_get_account(gc);

	qd = (qq_data *) gc->proto_data;
	gaim_name = uid_to_gaim_name(strtol(info->uid, NULL, 10));

	alias_utf8 = qq_to_utf8(info->nick, QQ_CHARSET_DEFAULT);
	if (qd->uid == strtol(info->uid, NULL, 10)) {	/* it is me */
		qd->my_icon = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			gaim_account_set_alias(account, alias_utf8);
	}
	/* update buddy list (including myself, if myself is the buddy) */
	b = gaim_find_buddy(gc->account, gaim_name);
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud != NULL) {	/* I have this buddy */
		q_bud->age = strtol(info->age, NULL, 10);
		q_bud->gender = strtol(info->gender, NULL, 10);
		q_bud->face = strtol(info->face, NULL, 10);
		if (alias_utf8 != NULL)
			q_bud->nickname = g_strdup(alias_utf8);
		qq_update_buddy_contact(gc, q_bud);
		_qq_update_buddy_icon(gc->account, gaim_name, q_bud->face);
	}
	g_free(gaim_name);
	g_free(alias_utf8);
}

/* process reply to get_info packet */
void qq_process_get_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	gint len;
	guint8 *data;
	gchar **segments;
	qq_info_query *query;
	qq_data *qd;
	contact_info *info;
	GList *list, *query_list;
	GaimNotifyUserInfo *user_info;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	list = query_list = NULL;
	len = buf_len;
	data = g_newa(guint8, len);
	info = NULL;

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		if (NULL == (segments = split_data(data, len, "\x1e", QQ_CONTACT_FIELDS)))
			return;

		info = (contact_info *) segments;
		if (qd->modifying_face && strtol(info->face, NULL, 10) != qd->my_icon) {
			gchar *icon = g_strdup_printf("%d", qd->my_icon);
			qd->modifying_face = FALSE;
			g_free(info->face);
			info->face = icon;
			qq_send_packet_modify_info(gc, segments);
		}

		qq_refresh_buddy_and_myself(info, gc);

		query_list = qd->info_query;
		/* ensure we're processing the right query */
		while (query_list) {
			query = (qq_info_query *) query_list->data;
			if (query->uid == atoi(info->uid)) {
				if (query->show_window) {
					user_info = info_to_notify_user_info(info);
					gaim_notify_userinfo(gc, info->uid, user_info, NULL, NULL);
					gaim_notify_user_info_destroy(user_info);
				} else if (query->modify_info) {
					create_modify_info_dialogue(gc, info);
				}
				qd->info_query = g_list_remove(qd->info_query, qd->info_query->data);
				g_free(query);
				break;
			}
			query_list = query_list->next;
		}

		g_strfreev(segments);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt get info reply\n");
	}
}

void qq_info_query_free(qq_data *qd)
{
	gint i;
	qq_info_query *p;

	g_return_if_fail(qd != NULL);

	i = 0;
	while (qd->info_query != NULL) {
		p = (qq_info_query *) (qd->info_query->data);
		qd->info_query = g_list_remove(qd->info_query, p);
		g_free(p);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d info queries are freed!\n", i);
}

void qq_send_packet_get_level(GaimConnection *gc, guint32 uid)
{
	guint8 buf[5];
	guint32 tmp = g_htonl(uid);
	buf[0] = 0;
	memcpy(buf+1, &tmp, 4);
	qq_send_cmd(gc, QQ_CMD_GET_LEVEL, TRUE, 0, TRUE, buf, 5);
}

/*
void qq_send_packet_get_buddies_levels(GaimConnection *gc)
{
	guint8 *buf, *tmp, size;
	qq_buddy *q_bud;
	qq_data *qd = (qq_data *) gc->proto_data;
	GList *node = qd->buddies;

	if (qd->buddies) {
*/
		/* server only sends back levels for online buddies, no point
 	 	* in asking for anyone else */
/*
		size = 4*g_list_length(qd->buddies) + 1;
		buf = g_new0(guint8, size);
		tmp = buf + 1;

		while (node != NULL) {
			guint32 tmp4;
			q_bud = (qq_buddy *) node->data;
			if (q_bud != NULL) {
				tmp4 = g_htonl(q_bud->uid);
				memcpy(tmp, &tmp4, 4);
				tmp += 4;
			}
			node = node->next;
		}
		qq_send_cmd(gc, QQ_CMD_GET_LEVEL, TRUE, 0, TRUE, buf, size);
		g_free(buf);
	}
}
*/

void qq_process_get_level_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	guint32 uid, onlineTime;
	guint16 level, timeRemainder;
	gchar *gaim_name;
	GaimBuddy *b;
	qq_buddy *q_bud;
	gint decr_len, i;
	guint8 *decr_buf, *tmp;
	GaimAccount *account = gaim_connection_get_account(gc);
	qq_data *qd = (qq_data *) gc->proto_data;
	
	decr_len = buf_len;
	decr_buf = g_new0(guint8, buf_len);
	if (!qq_crypt(DECRYPT, buf, buf_len, qd->session_key, decr_buf, &decr_len)) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Couldn't decrypt get level packet\n");
	}

	decr_len--; 
	if (decr_len % 12 != 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", 
			"Get levels list of abnormal length. Truncating last %d bytes.\n", decr_len % 12);
		decr_len -= (decr_len % 12);
	}
		
	tmp = decr_buf + 1;
	/* this byte seems random */
	/*
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Byte one of get_level packet: %d\n", buf[0]);
	*/
	for (i = 0; i < decr_len; i += 12) {
		uid = g_ntohl(*(guint32 *) tmp);
		tmp += 4;
		onlineTime = g_ntohl(*(guint32 *) tmp);
		tmp += 4;
		level = g_ntohs(*(guint16 *) tmp);
		tmp += 2;
		timeRemainder = g_ntohs(*(guint16 *) tmp);
		tmp += 2;
		/*
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Level packet entry:\nuid: %d\nonlineTime: %d\nlevel: %d\ntimeRemainder: %d\n", 
				uid, onlineTime, level, timeRemainder);
		*/
		gaim_name = uid_to_gaim_name(uid);
		b = gaim_find_buddy(account, gaim_name);
		q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;

		if (q_bud != NULL || uid == qd->uid) {
			if (q_bud) {
				q_bud->onlineTime = onlineTime;
				q_bud->level = level;
				q_bud->timeRemainder = timeRemainder;
			}
			if (uid == qd->uid) {
				qd->my_level = level;
			}
		} else {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", 
				"Got an online buddy %d, but not in my buddy list\n", uid);
		}
		g_free(gaim_name);
	}
	g_free(decr_buf);
}
