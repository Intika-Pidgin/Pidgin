/**
 * @file qq_proxy.h
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

#ifndef _QQ_PROXY_H
#define _QQ_PROXY_H

#include <glib.h>
#include "dnsquery.h"
#include "proxy.h"

#include "qq.h"

#define QQ_CONNECT_STEPS    2	/* steps in connection */

struct PHB {
	GaimProxyConnectFunction func;
	gpointer data;
	gchar *host;
	gint port;
	gint inpa;
	GaimProxyInfo *gpi;
	GaimAccount *account;
	gint udpsock;
	gpointer sockbuf;
};

gint qq_proxy_read(qq_data *qd, guint8 *data, gint len);
gint qq_proxy_write(qq_data *qd, guint8 *data, gint len);

gint qq_connect(GaimAccount *account, const gchar *host, guint16 port, gboolean use_tcp, gboolean is_redirect);
void qq_disconnect(GaimConnection *gc);

void _qq_show_packet(const gchar *desc, const guint8 *buf, gint len);

#endif
