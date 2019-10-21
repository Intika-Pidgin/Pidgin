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
 */
#include "internal.h"
#include "debug.h"
#include "presence.h"

/**************************************************************************
 * PurplePresence
 **************************************************************************/

/** Private data for a presence */
typedef struct
{
	gboolean idle;
	time_t idle_time;
	time_t login_time;

	GList *statuses;
	GHashTable *status_table;

	PurpleStatus *active_status;
} PurplePresencePrivate;

/* Presence property enums */
enum
{
	PRES_PROP_0,
	PRES_PROP_IDLE,
	PRES_PROP_IDLE_TIME,
	PRES_PROP_LOGIN_TIME,
	PRES_PROP_STATUSES,
	PRES_PROP_ACTIVE_STATUS,
	PRES_PROP_LAST
};

static GParamSpec *properties[PRES_PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE(PurplePresence, purple_presence, G_TYPE_OBJECT)

/**************************************************************************
 * PurpleAccountPresence
 **************************************************************************/

/**
 * PurpleAccountPresence:
 *
 * A presence for an account
 */
struct _PurpleAccountPresence
{
	PurplePresence parent;
};

/** Private data for an account presence */
typedef struct
{
	PurpleAccount *account;
} PurpleAccountPresencePrivate;

/* Account presence property enums */
enum
{
	ACPRES_PROP_0,
	ACPRES_PROP_ACCOUNT,
	ACPRES_PROP_LAST
};

static GParamSpec *ap_properties[ACPRES_PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE(PurpleAccountPresence, purple_account_presence,
		PURPLE_TYPE_PRESENCE)

/**************************************************************************
 * PurpleBuddyPresence
 **************************************************************************/

/**
 * PurpleBuddyPresence:
 *
 * A presence for a buddy
 */
struct _PurpleBuddyPresence
{
	PurplePresence parent;
};

/** Private data for a buddy presence */
typedef struct
{
	PurpleBuddy *buddy;
} PurpleBuddyPresencePrivate;

/* Buddy presence property enums */
enum
{
	BUDPRES_PROP_0,
	BUDPRES_PROP_BUDDY,
	BUDPRES_PROP_LAST
};

static GParamSpec *bp_properties[BUDPRES_PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE(PurpleBuddyPresence, purple_buddy_presence,
		PURPLE_TYPE_PRESENCE)

/**************************************************************************
* PurplePresence API
**************************************************************************/

void
purple_presence_set_status_active(PurplePresence *presence, const char *status_id,
		gboolean active)
{
	PurpleStatus *status;

	g_return_if_fail(PURPLE_IS_PRESENCE(presence));
	g_return_if_fail(status_id != NULL);

	status = purple_presence_get_status(presence, status_id);

	g_return_if_fail(PURPLE_IS_STATUS(status));
	/* TODO: Should we do the following? */
	/* g_return_if_fail(active == status->active); */

	if (purple_status_is_exclusive(status))
	{
		if (!active)
		{
			purple_debug_warning("presence",
					"Attempted to set a non-independent status "
					"(%s) inactive. Only independent statuses "
					"can be specifically marked inactive.",
					status_id);
			return;
		}
	}

	purple_status_set_active(status, active);
}

void
purple_presence_switch_status(PurplePresence *presence, const char *status_id)
{
	purple_presence_set_status_active(presence, status_id, TRUE);
}

void
purple_presence_set_idle(PurplePresence *presence, gboolean idle, time_t idle_time)
{
	PurplePresencePrivate *priv = NULL;
	PurplePresenceClass *klass = NULL;
	gboolean old_idle;
	GObject *obj;

	g_return_if_fail(PURPLE_IS_PRESENCE(presence));

	priv = purple_presence_get_instance_private(presence);
	klass = PURPLE_PRESENCE_GET_CLASS(presence);

	if (priv->idle == idle && priv->idle_time == idle_time)
		return;

	old_idle        = priv->idle;
	priv->idle      = idle;
	priv->idle_time = (idle ? idle_time : 0);

	obj = G_OBJECT(presence);
	g_object_freeze_notify(obj);
	g_object_notify_by_pspec(obj, properties[PRES_PROP_IDLE]);
	g_object_notify_by_pspec(obj, properties[PRES_PROP_IDLE_TIME]);
	g_object_thaw_notify(obj);

	if (klass->update_idle)
		klass->update_idle(presence, old_idle);
}

void
purple_presence_set_login_time(PurplePresence *presence, time_t login_time)
{
	PurplePresencePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_PRESENCE(presence));

	priv = purple_presence_get_instance_private(presence);

	if (priv->login_time == login_time)
		return;

	priv->login_time = login_time;

	g_object_notify_by_pspec(G_OBJECT(presence),
			properties[PRES_PROP_LOGIN_TIME]);
}

GList *
purple_presence_get_statuses(PurplePresence *presence)
{
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), NULL);

	priv = purple_presence_get_instance_private(presence);
	return priv->statuses;
}

PurpleStatus *
purple_presence_get_status(PurplePresence *presence, const char *status_id)
{
	PurplePresencePrivate *priv = NULL;
	PurpleStatus *status;
	GList *l = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	priv = purple_presence_get_instance_private(presence);

	/* What's the purpose of this hash table? */
	status = (PurpleStatus *)g_hash_table_lookup(priv->status_table,
						   status_id);

	if (status == NULL) {
		for (l = purple_presence_get_statuses(presence);
			 l != NULL && status == NULL; l = l->next)
		{
			PurpleStatus *temp_status = l->data;

			if (purple_strequal(status_id, purple_status_get_id(temp_status)))
				status = temp_status;
		}

		if (status != NULL)
			g_hash_table_insert(priv->status_table,
								g_strdup(purple_status_get_id(status)), status);
	}

	return status;
}

PurpleStatus *
purple_presence_get_active_status(PurplePresence *presence)
{
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), NULL);

	priv = purple_presence_get_instance_private(presence);
	return priv->active_status;
}

gboolean
purple_presence_is_available(PurplePresence *presence)
{
	PurpleStatus *status;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);

	status = purple_presence_get_active_status(presence);

	return ((status != NULL && purple_status_is_available(status)) &&
			!purple_presence_is_idle(presence));
}

gboolean
purple_presence_is_online(PurplePresence *presence)
{
	PurpleStatus *status;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);

	if ((status = purple_presence_get_active_status(presence)) == NULL)
		return FALSE;

	return purple_status_is_online(status);
}

gboolean
purple_presence_is_status_active(PurplePresence *presence,
		const char *status_id)
{
	PurpleStatus *status;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	status = purple_presence_get_status(presence, status_id);

	return (status != NULL && purple_status_is_active(status));
}

gboolean
purple_presence_is_status_primitive_active(PurplePresence *presence,
		PurpleStatusPrimitive primitive)
{
	GList *l;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);
	g_return_val_if_fail(primitive != PURPLE_STATUS_UNSET, FALSE);

	for (l = purple_presence_get_statuses(presence);
	     l != NULL; l = l->next)
	{
		PurpleStatus *temp_status = l->data;
		PurpleStatusType *type = purple_status_get_status_type(temp_status);

		if (purple_status_type_get_primitive(type) == primitive &&
		    purple_status_is_active(temp_status))
			return TRUE;
	}
	return FALSE;
}

gboolean
purple_presence_is_idle(PurplePresence *presence)
{
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);

	priv = purple_presence_get_instance_private(presence);
	return purple_presence_is_online(presence) && priv->idle;
}

time_t
purple_presence_get_idle_time(PurplePresence *presence)
{
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), 0);

	priv = purple_presence_get_instance_private(presence);
	return priv->idle_time;
}

time_t
purple_presence_get_login_time(PurplePresence *presence)
{
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), 0);

	priv = purple_presence_get_instance_private(presence);
	return purple_presence_is_online(presence) ? priv->login_time : 0;
}

/**************************************************************************
 * GObject code for PurplePresence
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_presence_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePresence *presence = PURPLE_PRESENCE(obj);
	PurplePresencePrivate *priv = purple_presence_get_instance_private(presence);

	switch (param_id) {
		case PRES_PROP_IDLE:
			purple_presence_set_idle(presence, g_value_get_boolean(value), 0);
			break;
		case PRES_PROP_IDLE_TIME:
#if SIZEOF_TIME_T == 4
			purple_presence_set_idle(presence, TRUE, g_value_get_int(value));
#elif SIZEOF_TIME_T == 8
			purple_presence_set_idle(presence, TRUE, g_value_get_int64(value));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_LOGIN_TIME:
#if SIZEOF_TIME_T == 4
			purple_presence_set_login_time(presence, g_value_get_int(value));
#elif SIZEOF_TIME_T == 8
			purple_presence_set_login_time(presence, g_value_get_int64(value));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_ACTIVE_STATUS:
			priv->active_status = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_presence_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurplePresence *presence = PURPLE_PRESENCE(obj);

	switch (param_id) {
		case PRES_PROP_IDLE:
			g_value_set_boolean(value, purple_presence_is_idle(presence));
			break;
		case PRES_PROP_IDLE_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_presence_get_idle_time(presence));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_presence_get_idle_time(presence));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_LOGIN_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_presence_get_login_time(presence));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_presence_get_login_time(presence));
#else
#error Unknown size of time_t
#endif
			break;
		case PRES_PROP_STATUSES:
			g_value_set_pointer(value, purple_presence_get_statuses(presence));
			break;
		case PRES_PROP_ACTIVE_STATUS:
			g_value_set_object(value, purple_presence_get_active_status(presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_presence_init(PurplePresence *presence)
{
	PurplePresencePrivate *priv = purple_presence_get_instance_private(presence);
	priv->status_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

/* GObject dispose function */
static void
purple_presence_dispose(GObject *object)
{
	PurplePresencePrivate *priv =
		purple_presence_get_instance_private(PURPLE_PRESENCE(object));

	if (priv->statuses) {
		g_list_free_full(priv->statuses, g_object_unref);
		priv->statuses = NULL;
	}

	G_OBJECT_CLASS(purple_presence_parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_presence_finalize(GObject *object)
{
	PurplePresencePrivate *priv =
		purple_presence_get_instance_private(PURPLE_PRESENCE(object));

	g_hash_table_destroy(priv->status_table);

	G_OBJECT_CLASS(purple_presence_parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_presence_class_init(PurplePresenceClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->dispose = purple_presence_dispose;
	obj_class->finalize = purple_presence_finalize;

	/* Setup properties */
	obj_class->get_property = purple_presence_get_property;
	obj_class->set_property = purple_presence_set_property;

	properties[PRES_PROP_IDLE] = g_param_spec_boolean("idle", "Idle",
				"Whether the presence is in idle state.", FALSE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PRES_PROP_IDLE_TIME] =
#if SIZEOF_TIME_T == 4
		g_param_spec_int
#elif SIZEOF_TIME_T == 8
		g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				("idle-time", "Idle time",
				"The idle time of the presence",
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PRES_PROP_LOGIN_TIME] =
#if SIZEOF_TIME_T == 4
		g_param_spec_int
#elif SIZEOF_TIME_T == 8
		g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				("login-time", "Login time",
				"The login time of the presence.",
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PRES_PROP_STATUSES] = g_param_spec_pointer("statuses",
				"Statuses",
				"The list of statuses in the presence.",
				G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	properties[PRES_PROP_ACTIVE_STATUS] = g_param_spec_object("active-status",
				"Active status",
				"The active status for the presence.", PURPLE_TYPE_STATUS,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PRES_PROP_LAST, properties);
}

/**************************************************************************
* PurpleAccountPresence API
**************************************************************************/
static void
purple_account_presence_update_idle(PurplePresence *presence, gboolean old_idle)
{
	PurpleAccount *account;
	PurpleConnection *gc = NULL;
	PurpleProtocol *protocol = NULL;
	gboolean idle = purple_presence_is_idle(presence);
	time_t idle_time = purple_presence_get_idle_time(presence);
	time_t current_time = time(NULL);

	account = purple_account_presence_get_account(PURPLE_ACCOUNT_PRESENCE(presence));

	if (purple_prefs_get_bool("/purple/logging/log_system"))
	{
		PurpleLog *log = purple_account_get_log(account, FALSE);

		if (log != NULL)
		{
			char *msg, *tmp;
			GDateTime *dt;

			if (idle) {
				tmp = g_strdup_printf(_("+++ %s became idle"), purple_account_get_username(account));
				dt = g_date_time_new_from_unix_local(idle_time);
			} else {
				tmp = g_strdup_printf(_("+++ %s became unidle"), purple_account_get_username(account));
				dt = g_date_time_new_now_utc();
			}

			msg = g_markup_escape_text(tmp, -1);
			g_free(tmp);
			purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
			                 purple_account_get_username(account),
			                 dt, msg);
			g_date_time_unref(dt);
			g_free(msg);
		}
	}

	gc = purple_account_get_connection(account);

	if(PURPLE_CONNECTION_IS_CONNECTED(gc))
		protocol = purple_connection_get_protocol(gc);

	if (protocol)
		purple_protocol_server_iface_set_idle(protocol, gc, (idle ? (current_time - idle_time) : 0));
}

PurpleAccount *
purple_account_presence_get_account(PurpleAccountPresence *presence)
{
	PurpleAccountPresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT_PRESENCE(presence), NULL);

	priv = purple_account_presence_get_instance_private(presence);
	return priv->account;
}

static int
purple_buddy_presence_compute_score(PurpleBuddyPresence *buddy_presence)
{
	GList *l;
	int score = 0;
	PurplePresence *presence = PURPLE_PRESENCE(buddy_presence);
	PurpleBuddy *b = purple_buddy_presence_get_buddy(buddy_presence);
	int *primitive_scores = _purple_statuses_get_primitive_scores();
	int offline_score = purple_prefs_get_int("/purple/status/scores/offline_msg");
	int idle_score = purple_prefs_get_int("/purple/status/scores/idle");

	for (l = purple_presence_get_statuses(presence); l != NULL; l = l->next) {
		PurpleStatus *status = (PurpleStatus *)l->data;
		PurpleStatusType *type = purple_status_get_status_type(status);

		if (purple_status_is_active(status)) {
			score += primitive_scores[purple_status_type_get_primitive(type)];
			if (!purple_status_is_online(status)) {
				if (b && purple_account_supports_offline_message(purple_buddy_get_account(b), b))
					score += offline_score;
			}
		}
	}
	score += purple_account_get_int(purple_buddy_get_account(b), "score", 0);
	if (purple_presence_is_idle(presence))
		score += idle_score;
	return score;
}

gint
purple_buddy_presence_compare(PurpleBuddyPresence *buddy_presence1,
		PurpleBuddyPresence *buddy_presence2)
{
	PurplePresence *presence1 = PURPLE_PRESENCE(buddy_presence1);
	PurplePresence *presence2 = PURPLE_PRESENCE(buddy_presence2);
	time_t idle_time_1, idle_time_2;
	int score1 = 0, score2 = 0;
	int idle_time_score = purple_prefs_get_int("/purple/status/scores/idle_time");

	if (presence1 == presence2)
		return 0;
	else if (presence1 == NULL)
		return 1;
	else if (presence2 == NULL)
		return -1;

	if (purple_presence_is_online(presence1) &&
			!purple_presence_is_online(presence2))
		return -1;
	else if (purple_presence_is_online(presence2) &&
			!purple_presence_is_online(presence1))
		return 1;

	/* Compute the score of the first set of statuses. */
	score1 = purple_buddy_presence_compute_score(buddy_presence1);

	/* Compute the score of the second set of statuses. */
	score2 = purple_buddy_presence_compute_score(buddy_presence2);

	idle_time_1 = time(NULL) - purple_presence_get_idle_time(presence1);
	idle_time_2 = time(NULL) - purple_presence_get_idle_time(presence2);

	if (idle_time_1 > idle_time_2)
		score1 += idle_time_score;
	else if (idle_time_1 < idle_time_2)
		score2 += idle_time_score;

	if (score1 < score2)
		return 1;
	else if (score1 > score2)
		return -1;

	return 0;
}

/**************************************************************************
 * GObject code for PurpleAccountPresence
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_account_presence_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleAccountPresence *account_presence = PURPLE_ACCOUNT_PRESENCE(obj);
	PurpleAccountPresencePrivate *priv =
			purple_account_presence_get_instance_private(account_presence);

	switch (param_id) {
		case ACPRES_PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_account_presence_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleAccountPresence *account_presence = PURPLE_ACCOUNT_PRESENCE(obj);

	switch (param_id) {
		case ACPRES_PROP_ACCOUNT:
			g_value_set_object(value,
					purple_account_presence_get_account(account_presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_account_presence_constructed(GObject *object)
{
	PurplePresence *presence = PURPLE_PRESENCE(object);
	PurplePresencePrivate *parent_priv = purple_presence_get_instance_private(presence);
	PurpleAccountPresencePrivate *account_priv =
		purple_account_presence_get_instance_private(PURPLE_ACCOUNT_PRESENCE(presence));

	G_OBJECT_CLASS(purple_account_presence_parent_class)->constructed(object);

	parent_priv->statuses = purple_protocol_get_statuses(account_priv->account, presence);
}

/* Class initializer function */
static void purple_account_presence_class_init(PurpleAccountPresenceClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	PURPLE_PRESENCE_CLASS(klass)->update_idle = purple_account_presence_update_idle;

	obj_class->constructed = purple_account_presence_constructed;

	/* Setup properties */
	obj_class->get_property = purple_account_presence_get_property;
	obj_class->set_property = purple_account_presence_set_property;

	ap_properties[ACPRES_PROP_ACCOUNT] = g_param_spec_object("account",
				"Account",
				"The account that this presence is of.", PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, ACPRES_PROP_LAST,
				ap_properties);
}

static void
purple_account_presence_init(PurpleAccountPresence *presence)
{
}

PurpleAccountPresence *
purple_account_presence_new(PurpleAccount *account)
{
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	return g_object_new(PURPLE_TYPE_ACCOUNT_PRESENCE,
			"account", account,
			NULL);
}

/**************************************************************************
* PurpleBuddyPresence API
**************************************************************************/
static void
purple_buddy_presence_update_idle(PurplePresence *presence, gboolean old_idle)
{
	PurpleBuddy *buddy = purple_buddy_presence_get_buddy(PURPLE_BUDDY_PRESENCE(presence));
	GDateTime *current_time = g_date_time_new_now_utc();
	PurpleAccount *account = purple_buddy_get_account(buddy);
	gboolean idle = purple_presence_is_idle(presence);

	if (!old_idle && idle)
	{
		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *tmp, *tmp2;
				tmp = g_strdup_printf(_("%s became idle"),
				purple_buddy_get_alias(buddy));
				tmp2 = g_markup_escape_text(tmp, -1);
				g_free(tmp);

				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				                 purple_buddy_get_alias(buddy),
				                 current_time, tmp2);
				g_free(tmp2);
			}
		}
	}
	else if (old_idle && !idle)
	{
		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *tmp, *tmp2;
				tmp = g_strdup_printf(_("%s became unidle"),
				purple_buddy_get_alias(buddy));
				tmp2 = g_markup_escape_text(tmp, -1);
				g_free(tmp);

				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
				                 purple_buddy_get_alias(buddy),
				                 current_time, tmp2);
				g_free(tmp2);
			}
		}
	}

	if (old_idle != idle)
		purple_signal_emit(purple_blist_get_handle(), "buddy-idle-changed", buddy,
		                 old_idle, idle);

	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));

	/* Should this be done here? It'd perhaps make more sense to
	 * connect to buddy-[un]idle signals and update from there
	 */

	purple_blist_update_node(purple_blist_get_default(),
	                         PURPLE_BLIST_NODE(buddy));

	g_date_time_unref(current_time);
}

PurpleBuddy *
purple_buddy_presence_get_buddy(PurpleBuddyPresence *presence)
{
	PurpleBuddyPresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_BUDDY_PRESENCE(presence), NULL);

	priv = purple_buddy_presence_get_instance_private(presence);
	return priv->buddy;
}

/**************************************************************************
 * GObject code for PurpleBuddyPresence
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_buddy_presence_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddyPresence *buddy_presence = PURPLE_BUDDY_PRESENCE(obj);
	PurpleBuddyPresencePrivate *priv =
			purple_buddy_presence_get_instance_private(buddy_presence);

	switch (param_id) {
		case BUDPRES_PROP_BUDDY:
			priv->buddy = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_buddy_presence_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleBuddyPresence *buddy_presence = PURPLE_BUDDY_PRESENCE(obj);

	switch (param_id) {
		case BUDPRES_PROP_BUDDY:
			g_value_set_object(value,
					purple_buddy_presence_get_buddy(buddy_presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_buddy_presence_constructed(GObject *object)
{
	PurplePresence *presence = PURPLE_PRESENCE(object);
	PurplePresencePrivate *parent_priv = purple_presence_get_instance_private(presence);
	PurpleBuddyPresencePrivate *buddy_priv =
		purple_buddy_presence_get_instance_private(PURPLE_BUDDY_PRESENCE(presence));
	PurpleAccount *account;

	G_OBJECT_CLASS(purple_buddy_presence_parent_class)->constructed(object);

	account = purple_buddy_get_account(buddy_priv->buddy);
	parent_priv->statuses = purple_protocol_get_statuses(account, presence);
}

/* Class initializer function */
static void purple_buddy_presence_class_init(PurpleBuddyPresenceClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	PURPLE_PRESENCE_CLASS(klass)->update_idle = purple_buddy_presence_update_idle;

	obj_class->constructed = purple_buddy_presence_constructed;

	/* Setup properties */
	obj_class->get_property = purple_buddy_presence_get_property;
	obj_class->set_property = purple_buddy_presence_set_property;

	bp_properties[BUDPRES_PROP_BUDDY] = g_param_spec_object("buddy", "Buddy",
				"The buddy that this presence is of.", PURPLE_TYPE_BUDDY,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, BUDPRES_PROP_LAST,
				bp_properties);
}

static void
purple_buddy_presence_init(PurpleBuddyPresence *presence)
{
}

PurpleBuddyPresence *
purple_buddy_presence_new(PurpleBuddy *buddy)
{
	g_return_val_if_fail(PURPLE_IS_BUDDY(buddy), NULL);

	return g_object_new(PURPLE_TYPE_BUDDY_PRESENCE,
			"buddy", buddy,
			NULL);
}
