/**
 * @file error.c Error functions
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
#include "msn.h"
#include "error.h"

const char *
msn_error_get_text(unsigned int type, gboolean *debug)
{
	static char msg[MSN_BUF_LEN];
	*debug = FALSE;

	switch (type) {
		case 0:
			g_snprintf(msg, sizeof(msg),
			           _("Unable to parse message"));
			*debug = TRUE;
			break;
		case 200:
			g_snprintf(msg, sizeof(msg),
			           _("Syntax Error (probably a client bug)"));
			*debug = TRUE;
			break;
		case 201:
			g_snprintf(msg, sizeof(msg),
			           _("Invalid email address"));
			break;
		case 205:
			g_snprintf(msg, sizeof(msg), _("User does not exist"));
			break;
		case 206:
			g_snprintf(msg, sizeof(msg),
			           _("Fully qualified domain name missing"));
			break;
		case 207:
			g_snprintf(msg, sizeof(msg), _("Already logged in"));
			break;
		case 208:
			g_snprintf(msg, sizeof(msg), _("Invalid username"));
			break;
		case 209:
			g_snprintf(msg, sizeof(msg), _("Invalid friendly name"));
			break;
		case 210:
			g_snprintf(msg, sizeof(msg), _("List full"));
			break;
		case 215:
			g_snprintf(msg, sizeof(msg), _("Already there"));
			*debug = TRUE;
			break;
		case 216:
			g_snprintf(msg, sizeof(msg), _("Not on list"));
			break;
		case 217:
			g_snprintf(msg, sizeof(msg), _("User is offline"));
			break;
		case 218:
			g_snprintf(msg, sizeof(msg), _("Already in the mode"));
			*debug = TRUE;
			break;
		case 219:
			g_snprintf(msg, sizeof(msg), _("Already in opposite list"));
			*debug = TRUE;
			break;
		case 223:
			g_snprintf(msg, sizeof(msg), _("Too many groups"));
			break;
		case 224:
			g_snprintf(msg, sizeof(msg), _("Invalid group"));
			break;
		case 225:
			g_snprintf(msg, sizeof(msg), _("User not in group"));
			break;
		case 229:
			g_snprintf(msg, sizeof(msg), _("Group name too long"));
			break;
		case 230:
			g_snprintf(msg, sizeof(msg), _("Cannot remove group zero"));
			*debug = TRUE;
			break;
		case 231:
			g_snprintf(msg, sizeof(msg),
			           _("Tried to add a user to a group "
			             "that doesn't exist"));
			break;
		case 280:
			g_snprintf(msg, sizeof(msg), _("Switchboard failed"));
			*debug = TRUE;
			break;
		case 281:
			g_snprintf(msg, sizeof(msg), _("Notify transfer failed"));
			*debug = TRUE;
			break;

		case 300:
			g_snprintf(msg, sizeof(msg), _("Required fields missing"));
			*debug = TRUE;
			break;
		case 301:
			g_snprintf(msg, sizeof(msg), _("Too many hits to a FND"));
			*debug = TRUE;
			break;
		case 302:
			g_snprintf(msg, sizeof(msg), _("Not logged in"));
			break;

		case 500:
			g_snprintf(msg, sizeof(msg),
			           _("Service temporarily unavailable"));
			break;
		case 501:
			g_snprintf(msg, sizeof(msg), _("Database server error"));
			*debug = TRUE;
			break;
		case 502:
			g_snprintf(msg, sizeof(msg), _("Command disabled"));
			*debug = TRUE;
			break;
		case 510:
			g_snprintf(msg, sizeof(msg), _("File operation error"));
			*debug = TRUE;
			break;
		case 520:
			g_snprintf(msg, sizeof(msg), _("Memory allocation error"));
			*debug = TRUE;
			break;
		case 540:
			g_snprintf(msg, sizeof(msg),
			           _("Wrong CHL value sent to server"));
			*debug = TRUE;
			break;

		case 600:
			g_snprintf(msg, sizeof(msg), _("Server busy"));
			break;
		case 601:
			g_snprintf(msg, sizeof(msg), _("Server unavailable"));
			break;
		case 602:
			g_snprintf(msg, sizeof(msg),
			           _("Peer notification server down"));
			*debug = TRUE;
			break;
		case 603:
			g_snprintf(msg, sizeof(msg), _("Database connect error"));
			*debug = TRUE;
			break;
		case 604:
			g_snprintf(msg, sizeof(msg),
					   _("Server is going down (abandon ship)"));
			break;
		case 605:
			g_snprintf(msg, sizeof(msg), _("Server unavailable"));
			break;

		case 707:
			g_snprintf(msg, sizeof(msg),
			           _("Error creating connection"));
			*debug = TRUE;
			break;
		case 710:
			g_snprintf(msg, sizeof(msg),
			           _("CVR parameters are either unknown "
			             "or not allowed"));
			*debug = TRUE;
			break;
		case 711:
			g_snprintf(msg, sizeof(msg), _("Unable to write"));
			break;
		case 712:
			g_snprintf(msg, sizeof(msg), _("Session overload"));
			*debug = TRUE;
			break;
		case 713:
			g_snprintf(msg, sizeof(msg), _("User is too active"));
			break;
		case 714:
			g_snprintf(msg, sizeof(msg), _("Too many sessions"));
			break;
		case 715:
			g_snprintf(msg, sizeof(msg), _("Passport not verified"));
			break;
		case 717:
			g_snprintf(msg, sizeof(msg), _("Bad friend file"));
			*debug = TRUE;
			break;
		case 731:
			g_snprintf(msg, sizeof(msg), _("Not expected"));
			*debug = TRUE;
			break;

		case 800:
			g_snprintf(msg, sizeof(msg),
			           _("Friendly name changes too rapidly"));
			break;

		case 910:
		case 912:
		case 918:
		case 919:
		case 921:
		case 922:
			g_snprintf(msg, sizeof(msg), _("Server too busy"));
			break;
		case 911:
		case 917:
			g_snprintf(msg, sizeof(msg), _("Authentication failed"));
			break;
		case 913:
			g_snprintf(msg, sizeof(msg), _("Not allowed when offline"));
			break;
		case 914:
		case 915:
		case 916:
			g_snprintf(msg, sizeof(msg), _("Server unavailable"));
			break;
		case 920:
			g_snprintf(msg, sizeof(msg), _("Not accepting new users"));
			break;
		case 923:
			g_snprintf(msg, sizeof(msg),
			           _("Kids Passport without parental consent"));
			break;
		case 924:
			g_snprintf(msg, sizeof(msg),
			           _("Passport account not yet verified"));
			break;
		case 927:
			g_snprintf(msg, sizeof(msg),
			           _("Passport account suspended"));
			break;
		case 928:
			g_snprintf(msg, sizeof(msg), _("Bad ticket"));
			*debug = TRUE;
			break;

		default:
			g_snprintf(msg, sizeof(msg),
			           _("Unknown Error Code %d"), type);
			*debug = TRUE;
			break;
	}

	return msg;
}

void
msn_error_handle(MsnSession *session, unsigned int type)
{
	char buf[MSN_BUF_LEN];
	gboolean debug;

	g_snprintf(buf, sizeof(buf), _("MSN Error: %s\n"),
	           msn_error_get_text(type, &debug));
	if (debug)
		purple_debug_warning("msn", "error %d: %s\n", type, buf);
	else
		purple_notify_error(session->account->gc, NULL, buf, NULL);
}

