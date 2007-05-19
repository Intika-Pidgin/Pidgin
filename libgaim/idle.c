/*
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
 *
 */
#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "idle.h"
#include "log.h"
#include "prefs.h"
#include "savedstatuses.h"
#include "signals.h"

#define IDLEMARK 600 /* 10 minutes! */
#define IDLE_CHECK_INTERVAL 5 /* 5 seconds */

typedef enum
{
	GAIM_IDLE_NOT_AWAY = 0,
	GAIM_IDLE_AUTO_AWAY,
	GAIM_IDLE_AWAY_BUT_NOT_AUTO_AWAY

} GaimAutoAwayState;

static GaimIdleUiOps *idle_ui_ops = NULL;

/**
 * This is needed for the I'dle Mak'er plugin to work correctly.  We
 * use it to determine if we're the ones who set our accounts idle
 * or if someone else did it (the I'dle Mak'er plugin, for example).
 * Basically we just keep track of which accounts were set idle by us,
 * and then we'll only set these specific accounts unidle when the
 * user returns.
 */
static GList *idled_accts = NULL;

static guint idle_timer = 0;

static time_t last_active_time = 0;

static void
set_account_idle(GaimAccount *account, int time_idle)
{
	GaimPresence *presence;

	presence = gaim_account_get_presence(account);

	if (gaim_presence_is_idle(presence))
		/* This account is already idle! */
		return;

	gaim_debug_info("idle", "Setting %s idle %d seconds\n",
			   gaim_account_get_username(account), time_idle);
	gaim_presence_set_idle(presence, TRUE, time(NULL) - time_idle);
	idled_accts = g_list_prepend(idled_accts, account);
}

static void
set_account_unidle(GaimAccount *account)
{
	GaimPresence *presence;

	presence = gaim_account_get_presence(account);

	idled_accts = g_list_remove(idled_accts, account);

	if (!gaim_presence_is_idle(presence))
		/* This account is already unidle! */
		return;

	gaim_debug_info("idle", "Setting %s unidle\n",
			   gaim_account_get_username(account));
	gaim_presence_set_idle(presence, FALSE, 0);
}

/*
 * This function should be called when you think your idle state
 * may have changed.  Maybe you're over the 10-minute mark and
 * Gaim should start reporting idle time to the server.  Maybe
 * you've returned from being idle.  Maybe your auto-away message
 * should be set.
 *
 * There is no harm to calling this many many times, other than
 * it will be kinda slow.  This is called every 5 seconds by a
 * timer set when Gaim starts.  It is also called when
 * you send an IM, a chat, etc.
 *
 * This function has 3 sections.
 * 1. Get your idle time.  It will query XScreenSaver or Windows
 *    or use the Gaim idle time.  Whatever.
 * 2. Set or unset your auto-away message.
 * 3. Report your current idle time to the IM server.
 */
static gint
check_idleness()
{
	time_t time_idle;
	gboolean auto_away;
	const gchar *idle_reporting;
	gboolean report_idle;
	GList *l;

	gaim_signal_emit(gaim_blist_get_handle(), "update-idle");

	idle_reporting = gaim_prefs_get_string("/core/away/idle_reporting");
	report_idle = TRUE;
	if (!strcmp(idle_reporting, "system") &&
		(idle_ui_ops != NULL) && (idle_ui_ops->get_time_idle != NULL))
	{
		/* Use system idle time (mouse or keyboard movement, etc.) */
		time_idle = idle_ui_ops->get_time_idle();
	}
	else if (!strcmp(idle_reporting, "gaim"))
	{
		/* Use 'Gaim idle' */
		time_idle = time(NULL) - last_active_time;
	}
	else
	{
		/* Don't report idle time */
		time_idle = 0;
		report_idle = FALSE;
	}

	/* Auto-away stuff */
	auto_away = gaim_prefs_get_bool("/core/away/away_when_idle");

	/* If we're not reporting idle, we can still do auto-away.
	 * First try "system" and if that isn't possible, use "gaim" */
	if (!report_idle && auto_away) {
		if ((idle_ui_ops != NULL) && (idle_ui_ops->get_time_idle != NULL))
			time_idle = idle_ui_ops->get_time_idle();
		else
			time_idle = time(NULL) - last_active_time;
	}

	if (auto_away &&
		(time_idle > (60 * gaim_prefs_get_int("/core/away/mins_before_away"))))
	{
		gaim_savedstatus_set_idleaway(TRUE);
	}
	else if (time_idle < 60 * gaim_prefs_get_int("/core/away/mins_before_away"))
	{
		gaim_savedstatus_set_idleaway(FALSE);
	}

	/* Idle reporting stuff */
	if (report_idle && (time_idle >= IDLEMARK))
	{
		for (l = gaim_connections_get_all(); l != NULL; l = l->next)
		{
			GaimConnection *gc = l->data;
			set_account_idle(gaim_connection_get_account(gc), time_idle);
		}
	}
	else if (!report_idle || (time_idle < IDLEMARK))
	{
		while (idled_accts != NULL)
			set_account_unidle(idled_accts->data);
	}

	return TRUE;
}

static void
im_msg_sent_cb(GaimAccount *account, const char *receiver,
			   const char *message, void *data)
{
	/* Check our idle time after an IM is sent */
	check_idleness();
}

static void
signing_on_cb(GaimConnection *gc, void *data)
{
	/* When signing on a new account, check if the account should be idle */
	check_idleness();
}

static void
signing_off_cb(GaimConnection *gc, void *data)
{
	GaimAccount *account;

	account = gaim_connection_get_account(gc);
	set_account_unidle(account);
}

void
gaim_idle_touch()
{
	time(&last_active_time);
}

void
gaim_idle_set(time_t time)
{
	last_active_time = time;
}

void
gaim_idle_set_ui_ops(GaimIdleUiOps *ops)
{
	idle_ui_ops = ops;
}

GaimIdleUiOps *
gaim_idle_get_ui_ops(void)
{
	return idle_ui_ops;
}

static void *
gaim_idle_get_handle()
{
	static int handle;

	return &handle;
}

void
gaim_idle_init()
{
	/* Add the timer to check if we're idle */
	idle_timer = gaim_timeout_add_seconds(IDLE_CHECK_INTERVAL , check_idleness_timer, NULL);

	gaim_signal_connect(gaim_conversations_get_handle(), "sent-im-msg",
						gaim_idle_get_handle(),
						GAIM_CALLBACK(im_msg_sent_cb), NULL);
	gaim_signal_connect(gaim_connections_get_handle(), "signing-on",
						gaim_idle_get_handle(),
						GAIM_CALLBACK(signing_on_cb), NULL);
	gaim_signal_connect(gaim_connections_get_handle(), "signing-off",
						gaim_idle_get_handle(),
						GAIM_CALLBACK(signing_off_cb), NULL);

	gaim_idle_touch();
}

void
gaim_idle_uninit()
{
	gaim_signals_disconnect_by_handle(gaim_idle_get_handle());

	/* Remove the idle timer */
	if (idle_timer > 0)
		gaim_timeout_remove(idle_timer);
	idle_timer = 0;
}
