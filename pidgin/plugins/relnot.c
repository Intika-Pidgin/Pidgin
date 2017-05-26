/*
 * Release Notification Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include "internal.h"

#include <string.h>

#include "connection.h"
#include "core.h"
#include "debug.h"
#include "gtkblist.h"
#include "gtkutils.h"
#include "notify.h"
#include "pidginstock.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

#include "pidgin.h"

/* 1 day */
#define MIN_CHECK_INTERVAL 60 * 60 * 24

static void
release_hide()
{
	/* No-op.  We may use this method in the future to avoid showing
	 * the popup twice */
}

static void
release_show()
{
	purple_notify_uri(NULL, PURPLE_WEBSITE);
}

static void
version_fetch_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *response, size_t len, const gchar *error_message)
{
	gchar *cur_ver;
	const char *tmp, *changelog;
	char response_code[4];
	GtkWidget *release_dialog;

	GString *message;
	int i = 0;

	if(error_message || !response || !len)
		return;

	memset(response_code, '\0', sizeof(response_code));
	/* Parse the status code - the response should be in the form of "HTTP/?.? 200 ..." */
	if ((tmp = strstr(response, " ")) != NULL) {
		tmp++;
		/* Read the 3 digit status code */
		if (len - (tmp - response) > 3) {
			memcpy(response_code, tmp, 3);
		}
	}

	if (!purple_strequal(response_code, "200")) {
		purple_debug_error("relnot", "Didn't recieve a HTTP status code of 200.\n");
		return;
	}

	/* Go to the start of the data */
	if((changelog = strstr(response, "\r\n\r\n")) == NULL) {
		purple_debug_error("relnot", "Unable to find start of HTTP response data.\n");
		return;
	}
	changelog += 4;

	while(changelog[i] && changelog[i] != '\n') i++;

	/* this basically means the version thing wasn't in the format we were
	 * looking for so sourceforge is probably having web server issues, and
	 * we should try again later */
	if(i == 0)
		return;

	cur_ver = g_strndup(changelog, i);

	message = g_string_new("");
	g_string_append_printf(message, _("You can upgrade to %s %s today."),
			PIDGIN_NAME, cur_ver);

	release_dialog = pidgin_make_mini_dialog(
		NULL, PIDGIN_STOCK_DIALOG_INFO,
		_("New Version Available"),
		message->str,
		NULL,
		_("Later"), PURPLE_CALLBACK(release_hide),
		_("Download Now"), PURPLE_CALLBACK(release_show),
		NULL);

	pidgin_blist_add_alert(release_dialog);

	g_string_free(message, TRUE);
	g_free(cur_ver);
}

static void
do_check(void)
{
	int last_check = purple_prefs_get_int("/plugins/gtk/relnot/last_check");
	if(!last_check || time(NULL) - last_check > MIN_CHECK_INTERVAL) {
		gchar *url, *request;
		const char *host = "pidgin.im";

		url = g_strdup_printf("https://%s/version.php?version=%s&build=%s",
				host,
				purple_core_get_version(),
#ifdef _WIN32
				"purple-win32"
#else
				"purple"
#endif
		);

		request = g_strdup_printf(
				"GET %s HTTP/1.0\r\n"
				"Connection: close\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				url,
				host);

		purple_util_fetch_url_request_len(url, TRUE, NULL, FALSE,
			request, TRUE, -1, version_fetch_cb, NULL);

		g_free(request);
		g_free(url);

		purple_prefs_set_int("/plugins/gtk/relnot/last_check", time(NULL));

	}
}

static void
signed_on_cb(PurpleConnection *gc, void *data) {
	do_check();
}

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						plugin, PURPLE_CALLBACK(signed_on_cb), NULL);

	/* we don't check if we're offline */
	if(purple_connections_get_all())
		do_check();

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"gtk-relnot",                                     /**< id             */
	N_("Release Notification"),                       /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Checks periodically for new releases."),
	                                                  /**  description    */
	N_("Checks periodically for new releases and notifies the user "
			"with the ChangeLog."),
	"Nathan Walp <faceprint@faceprint.com>",          /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/gtk/relnot");
	purple_prefs_add_int("/plugins/gtk/relnot/last_check", 0);
}

PURPLE_INIT_PLUGIN(relnot, init_plugin, info)
