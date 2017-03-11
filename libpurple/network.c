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

#include <gio/gio.h>

#ifndef _WIN32
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif
#else
#include <nspapi.h>
#endif

/* Solaris */
#if defined (__SVR4) && defined (__sun)
#include <sys/sockio.h>
#endif

#include "debug.h"
#include "account.h"
#include "nat-pmp.h"
#include "network.h"
#include "prefs.h"
#include "stun.h"
#include "upnp.h"

#ifdef USE_IDN
#include <idna.h>
#endif

/*
 * Calling sizeof(struct ifreq) isn't always correct on
 * Mac OS X (and maybe others).
 */
#ifdef _SIZEOF_ADDR_IFREQ
#  define HX_SIZE_OF_IFREQ(a) _SIZEOF_ADDR_IFREQ(a)
#else
#  define HX_SIZE_OF_IFREQ(a) sizeof(a)
#endif

struct _PurpleNetworkListenData {
	int listenfd;
	int socket_type;
	gboolean retry;
	gboolean adding;
	PurpleNetworkListenCallback cb;
	gpointer cb_data;
	PurpleUPnPMappingAddRemove *mapping_data;
	int timer;
};

static gboolean force_online = FALSE;

/* Cached IP addresses for STUN and TURN servers (set globally in prefs) */
static gchar *stun_ip = NULL;
static gchar *turn_ip = NULL;

/* Keep track of port mappings done with UPnP and NAT-PMP */
static GHashTable *upnp_port_mappings = NULL;
static GHashTable *nat_pmp_port_mappings = NULL;

void
purple_network_set_public_ip(const char *ip)
{
	g_return_if_fail(ip != NULL);

	/* XXX - Ensure the IP address is valid */

	purple_prefs_set_string("/purple/network/public_ip", ip);
}

const char *
purple_network_get_public_ip(void)
{
	return purple_prefs_get_string("/purple/network/public_ip");
}

const char *
purple_network_get_local_system_ip(int fd)
{
	struct ifreq buffer[100];
	guchar *it, *it_end;
	static char ip[16];
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_in *sinptr;
	guint32 lhost = htonl((127 << 24) + 1); /* 127.0.0.1 */
	long unsigned int add;
	int source = fd;

	if (fd < 0)
		source = socket(PF_INET,SOCK_STREAM, 0);

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_req = buffer;
	ioctl(source, SIOCGIFCONF, &ifc);

	if (fd < 0 && source >= 0)
		close(source);

	it = (guchar*)buffer;
	it_end = it + ifc.ifc_len;
	while (it < it_end) {
		/* in this case "it" is:
		 *  a) (struct ifreq)-aligned
		 *  b) not aligned, because of OS quirks (see
		 *     _SIZEOF_ADDR_IFREQ), so the OS should deal with it.
		 */
		ifr = (struct ifreq *)(gpointer)it;
		it += HX_SIZE_OF_IFREQ(*ifr);

		if (ifr->ifr_addr.sa_family == AF_INET)
		{
			sinptr = (struct sockaddr_in *)(gpointer)&ifr->ifr_addr;
			if (sinptr->sin_addr.s_addr != lhost)
			{
				add = ntohl(sinptr->sin_addr.s_addr);
				g_snprintf(ip, 16, "%lu.%lu.%lu.%lu",
					((add >> 24) & 255),
					((add >> 16) & 255),
					((add >> 8) & 255),
					add & 255);

				return ip;
			}
		}
	}

	return "0.0.0.0";
}

GList *
purple_network_get_all_local_system_ips(void)
{
#if defined(HAVE_GETIFADDRS) && defined(HAVE_INET_NTOP)
	GList *result = NULL;
	struct ifaddrs *start, *ifa;
	int ret;

	ret = getifaddrs(&start);
	if (ret < 0) {
		purple_debug_warning("network",
				"getifaddrs() failed: %s\n", g_strerror(errno));
		return NULL;
	}

	for (ifa = start; ifa; ifa = ifa->ifa_next) {
		int family = ifa->ifa_addr ? ifa->ifa_addr->sa_family : AF_UNSPEC;
		char host[INET6_ADDRSTRLEN];
		const char *tmp = NULL;
		common_sockaddr_t *addr =
			(common_sockaddr_t *)(gpointer)ifa->ifa_addr;

		if ((family != AF_INET && family != AF_INET6) || ifa->ifa_flags & IFF_LOOPBACK)
			continue;

		if (family == AF_INET)
			tmp = inet_ntop(family, &addr->in.sin_addr, host, sizeof(host));
		else {
			/* Peer-peer link-local communication is a big TODO.  I am not sure
			 * how communicating link-local addresses is supposed to work, and
			 * it seems like it would require attempting the cartesian product
			 * of the local and remote interfaces to see if any match (eww).
			 */
			if (!IN6_IS_ADDR_LINKLOCAL(&addr->in6.sin6_addr))
				tmp = inet_ntop(family, &addr->in6.sin6_addr, host, sizeof(host));
		}
		if (tmp != NULL)
			result = g_list_prepend(result, g_strdup(tmp));
	}

	freeifaddrs(start);

	return g_list_reverse(result);
#else /* HAVE_GETIFADDRS && HAVE_INET_NTOP */
	GList *result = NULL;
	int source = socket(PF_INET,SOCK_STREAM, 0);
	struct ifreq buffer[100];
	guchar *it, *it_end;
	struct ifconf ifc;
	struct ifreq *ifr;

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_req = buffer;
	ioctl(source, SIOCGIFCONF, &ifc);
	close(source);

	it = (guchar*)buffer;
	it_end = it + ifc.ifc_len;
	while (it < it_end) {
		char dst[INET_ADDRSTRLEN];

		/* alignment: see purple_network_get_local_system_ip */
		ifr = (struct ifreq *)(gpointer)it;
		it += HX_SIZE_OF_IFREQ(*ifr);

		if (ifr->ifr_addr.sa_family == AF_INET) {
			struct sockaddr_in *sinptr =
				(struct sockaddr_in *)(gpointer)&ifr->ifr_addr;

			inet_ntop(AF_INET, &sinptr->sin_addr, dst,
				sizeof(dst));
			purple_debug_info("network",
				"found local i/f with address %s on IPv4\n", dst);
			if (!purple_strequal(dst, "127.0.0.1")) {
				result = g_list_append(result, g_strdup(dst));
			}
		}
	}

	return result;
#endif /* HAVE_GETIFADDRS && HAVE_INET_NTOP */
}

/*
 * purple_network_is_ipv4:
 * @hostname: The hostname to be verified.
 *
 * Checks, if specified hostname is valid ipv4 address.
 *
 * Returns: TRUE, if the hostname is valid.
 */
static gboolean
purple_network_is_ipv4(const gchar *hostname)
{
	g_return_val_if_fail(hostname != NULL, FALSE);

	/* We don't accept ipv6 here. */
	if (strchr(hostname, ':') != NULL)
		return FALSE;

	return g_hostname_is_ip_address(hostname);
}

const char *
purple_network_get_my_ip(int fd)
{
	const char *ip = NULL;
	PurpleStunNatDiscovery *stun;

	/* Check if the user specified an IP manually */
	if (!purple_prefs_get_bool("/purple/network/auto_ip")) {
		ip = purple_network_get_public_ip();
		/* Make sure the IP address entered by the user is valid */
		if ((ip != NULL) && (purple_network_is_ipv4(ip)))
			return ip;
	} else {
		/* Check if STUN discovery was already done */
		stun = purple_stun_discover(NULL);
		if ((stun != NULL) && (stun->status == PURPLE_STUN_STATUS_DISCOVERED))
			return stun->publicip;

		/* Attempt to get the IP from a NAT device using UPnP */
		ip = purple_upnp_get_public_ip();
		if (ip != NULL)
			return ip;

		/* Attempt to get the IP from a NAT device using NAT-PMP */
		ip = purple_pmp_get_public_ip();
		if (ip != NULL)
			return ip;
	}

	/* Just fetch the IP of the local system */
	return purple_network_get_local_system_ip(fd);
}


static void
purple_network_set_upnp_port_mapping_cb(gboolean success, gpointer data)
{
	PurpleNetworkListenData *listen_data;

	listen_data = data;
	/* TODO: Once we're keeping track of upnp requests... */
	/* listen_data->pnp_data = NULL; */

	if (!success) {
		purple_debug_warning("network", "Couldn't create UPnP mapping\n");
		if (listen_data->retry) {
			listen_data->retry = FALSE;
			listen_data->adding = FALSE;
			listen_data->mapping_data = purple_upnp_remove_port_mapping(
						purple_network_get_port_from_fd(listen_data->listenfd),
						(listen_data->socket_type == SOCK_STREAM) ? "TCP" : "UDP",
						purple_network_set_upnp_port_mapping_cb, listen_data);
			return;
		}
	} else if (!listen_data->adding) {
		/* We've tried successfully to remove the port mapping.
		 * Try to add it again */
		listen_data->adding = TRUE;
		listen_data->mapping_data = purple_upnp_set_port_mapping(
					purple_network_get_port_from_fd(listen_data->listenfd),
					(listen_data->socket_type == SOCK_STREAM) ? "TCP" : "UDP",
					purple_network_set_upnp_port_mapping_cb, listen_data);
		return;
	}

	if (success) {
		/* add port mapping to hash table */
		gint key = purple_network_get_port_from_fd(listen_data->listenfd);
		gint value = listen_data->socket_type;
		g_hash_table_insert(upnp_port_mappings, GINT_TO_POINTER(key), GINT_TO_POINTER(value));
	}

	if (listen_data->cb)
		listen_data->cb(listen_data->listenfd, listen_data->cb_data);

	/* Clear the UPnP mapping data, since it's complete and purple_network_listen_cancel() will try to cancel
	 * it otherwise. */
	listen_data->mapping_data = NULL;
	purple_network_listen_cancel(listen_data);
}

static gboolean
purple_network_finish_pmp_map_cb(gpointer data)
{
	PurpleNetworkListenData *listen_data;
	gint key;
	gint value;

	listen_data = data;
	listen_data->timer = 0;

	/* add port mapping to hash table */
	key = purple_network_get_port_from_fd(listen_data->listenfd);
	value = listen_data->socket_type;
	g_hash_table_insert(nat_pmp_port_mappings, GINT_TO_POINTER(key), GINT_TO_POINTER(value));

	if (listen_data->cb)
		listen_data->cb(listen_data->listenfd, listen_data->cb_data);

	purple_network_listen_cancel(listen_data);

	return FALSE;
}

static PurpleNetworkListenData *
purple_network_do_listen(unsigned short port, int socket_family, int socket_type, gboolean map_external,
                             PurpleNetworkListenCallback cb, gpointer cb_data)
{
	int listenfd = -1;
	const int on = 1;
	PurpleNetworkListenData *listen_data;
	unsigned short actual_port;
#ifdef HAVE_GETADDRINFO
	int errnum;
	struct addrinfo hints, *res, *next;
	char serv[6];

	/*
	 * Get a list of addresses on this machine.
	 */
	g_snprintf(serv, sizeof(serv), "%hu", port);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = socket_family;
	hints.ai_socktype = socket_type;
	errnum = getaddrinfo(NULL /* any IP */, serv, &hints, &res);
	if (errnum != 0) {
#ifndef _WIN32
		purple_debug_warning("network", "getaddrinfo: %s\n", purple_gai_strerror(errnum));
		if (errnum == EAI_SYSTEM)
			purple_debug_warning("network", "getaddrinfo: system error: %s\n", g_strerror(errno));
#else
		purple_debug_warning("network", "getaddrinfo: Error Code = %d\n", errnum);
#endif
		return NULL;
	}

	/*
	 * Go through the list of addresses and attempt to listen on
	 * one of them.
	 * XXX - Try IPv6 addresses first?
	 */
	for (next = res; next != NULL; next = next->ai_next) {
		listenfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol);
		if (listenfd < 0)
			continue;
		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
			purple_debug_warning("network", "setsockopt(SO_REUSEADDR): %s\n", g_strerror(errno));
		if (bind(listenfd, next->ai_addr, next->ai_addrlen) == 0)
			break; /* success */
		/* XXX - It is unclear to me (datallah) whether we need to be
		   using a new socket each time */
		close(listenfd);
	}

	freeaddrinfo(res);

	if (next == NULL)
		return NULL;
#else
	struct sockaddr_in sockin;

	if (socket_family != AF_INET && socket_family != AF_UNSPEC) {
		purple_debug_warning("network", "Address family %d only "
		                     "supported when built with getaddrinfo() "
		                     "support\n", socket_family);
		return NULL;
	}

	if ((listenfd = socket(AF_INET, socket_type, 0)) < 0) {
		purple_debug_warning("network", "socket: %s\n", g_strerror(errno));
		return NULL;
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
		purple_debug_warning("network", "setsockopt: %s\n", g_strerror(errno));

	memset(&sockin, 0, sizeof(struct sockaddr_in));
	sockin.sin_family = PF_INET;
	sockin.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0) {
		purple_debug_warning("network", "bind: %s\n", g_strerror(errno));
		close(listenfd);
		return NULL;
	}
#endif

	if (socket_type == SOCK_STREAM && listen(listenfd, 4) != 0) {
		purple_debug_warning("network", "listen: %s\n", g_strerror(errno));
		close(listenfd);
		return NULL;
	}
	_purple_network_set_common_socket_flags(listenfd);
	actual_port = purple_network_get_port_from_fd(listenfd);

	purple_debug_info("network", "Listening on port: %hu\n", actual_port);

	listen_data = g_new0(PurpleNetworkListenData, 1);
	listen_data->listenfd = listenfd;
	listen_data->adding = TRUE;
	listen_data->retry = TRUE;
	listen_data->cb = cb;
	listen_data->cb_data = cb_data;
	listen_data->socket_type = socket_type;

	if (!purple_socket_speaks_ipv4(listenfd) || !map_external ||
			!purple_prefs_get_bool("/purple/network/map_ports"))
	{
		purple_debug_info("network", "Skipping external port mapping.\n");
		/* The pmp_map_cb does what we want to do */
		listen_data->timer = purple_timeout_add(0, purple_network_finish_pmp_map_cb, listen_data);
	}
	/* Attempt a NAT-PMP Mapping, which will return immediately */
	else if (purple_pmp_create_map(((socket_type == SOCK_STREAM) ? PURPLE_PMP_TYPE_TCP : PURPLE_PMP_TYPE_UDP),
							  actual_port, actual_port, PURPLE_PMP_LIFETIME))
	{
		purple_debug_info("network", "Created NAT-PMP mapping on port %i\n", actual_port);
		/* We want to return listen_data now, and on the next run loop trigger the cb and destroy listen_data */
		listen_data->timer = purple_timeout_add(0, purple_network_finish_pmp_map_cb, listen_data);
	}
	else
	{
		/* Attempt a UPnP Mapping */
		listen_data->mapping_data = purple_upnp_set_port_mapping(
						 actual_port,
						 (socket_type == SOCK_STREAM) ? "TCP" : "UDP",
						 purple_network_set_upnp_port_mapping_cb, listen_data);
	}

	return listen_data;
}

PurpleNetworkListenData *
purple_network_listen(unsigned short port, int socket_family, int socket_type,
                             gboolean map_external, PurpleNetworkListenCallback cb,
                             gpointer cb_data)
{
	g_return_val_if_fail(port != 0, NULL);

	return purple_network_do_listen(port, socket_family, socket_type, map_external,
	                                cb, cb_data);
}

PurpleNetworkListenData *
purple_network_listen_range(unsigned short start, unsigned short end,
                                   int socket_family, int socket_type, gboolean map_external,
                                   PurpleNetworkListenCallback cb,
                                   gpointer cb_data)
{
	PurpleNetworkListenData *ret = NULL;

	if (purple_prefs_get_bool("/purple/network/ports_range_use")) {
		start = purple_prefs_get_int("/purple/network/ports_range_start");
		end = purple_prefs_get_int("/purple/network/ports_range_end");
	} else {
		if (end < start)
			end = start;
	}

	for (; start <= end; start++) {
		ret = purple_network_do_listen(start, AF_UNSPEC, socket_type, map_external, cb, cb_data);
		if (ret != NULL)
			break;
	}

	return ret;
}

void purple_network_listen_cancel(PurpleNetworkListenData *listen_data)
{
	if (listen_data->mapping_data != NULL)
		purple_upnp_cancel_port_mapping(listen_data->mapping_data);

	if (listen_data->timer > 0)
		purple_timeout_remove(listen_data->timer);

	g_free(listen_data);
}

unsigned short
purple_network_get_port_from_fd(int fd)
{
	struct sockaddr_in addr;
	socklen_t len;

	g_return_val_if_fail(fd >= 0, 0);

	len = sizeof(addr);
	if (getsockname(fd, (struct sockaddr *) &addr, &len) == -1) {
		purple_debug_warning("network", "getsockname: %s\n", g_strerror(errno));
		return 0;
	}

	return ntohs(addr.sin_port);
}

gboolean
purple_network_is_available(void)
{
	if(force_online) {
		return TRUE;
	}

	return g_network_monitor_get_network_available(g_network_monitor_get_default());
}

void
purple_network_force_online()
{
	force_online = TRUE;
}

static void
purple_network_ip_lookup_cb(GObject *sender, GAsyncResult *result, gpointer data) {
	GError *error = NULL;
	GList *addresses = NULL;
	GInetAddress *address = NULL;
	const gchar **ip_address = (const gchar **)data;

	addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(sender),
			result, &error);
	if(error) {
		purple_debug_info("network", "lookup of IP address failed: %s\n", error->message);

		g_error_free(error);

		return;
	}

	address = G_INET_ADDRESS(addresses->data);

	*ip_address = g_inet_address_to_string(address);

	g_resolver_free_addresses(addresses);
}

void
purple_network_set_stun_server(const gchar *stun_server)
{
	if (stun_server && stun_server[0] != '\0') {
		if (purple_network_is_available()) {
			GResolver *resolver = g_resolver_get_default();
			g_resolver_lookup_by_name_async(resolver,
			                                stun_server,
			                                NULL,
			                                purple_network_ip_lookup_cb,
			                                &stun_ip);
			g_object_unref(resolver);
		} else {
			purple_debug_info("network",
				"network is unavailable, don't try to update STUN IP");
		}
	} else {
		g_free(stun_ip);
		stun_ip = NULL;
	}
}

void
purple_network_set_turn_server(const gchar *turn_server)
{
	if (turn_server && turn_server[0] != '\0') {
		if (purple_network_is_available()) {
			GResolver *resolver = g_resolver_get_default();
			g_resolver_lookup_by_name_async(resolver,
			                                turn_server,
			                                NULL,
			                                purple_network_ip_lookup_cb,
			                                &turn_server);
			g_object_unref(resolver);
		} else {
			purple_debug_info("network",
				"network is unavailable, don't try to update TURN IP");
		}
	} else {
		g_free(turn_ip);
		turn_ip = NULL;
	}
}


const gchar *
purple_network_get_stun_ip(void)
{
	return stun_ip;
}

const gchar *
purple_network_get_turn_ip(void)
{
	return turn_ip;
}

void *
purple_network_get_handle(void)
{
	static int handle;

	return &handle;
}

static void
purple_network_upnp_mapping_remove_cb(gboolean sucess, gpointer data)
{
	purple_debug_info("network", "done removing UPnP port mapping\n");
}

/* the reason for these functions to have these signatures is to be able to
 use them for g_hash_table_foreach to clean remaining port mappings, which is
 not yet done */
static void
purple_network_upnp_mapping_remove(gpointer key, gpointer value,
	gpointer user_data)
{
	gint port = GPOINTER_TO_INT(key);
	gint protocol = GPOINTER_TO_INT(value);
	purple_debug_info("network", "removing UPnP port mapping for port %d\n",
		port);
	purple_upnp_remove_port_mapping(port,
		protocol == SOCK_STREAM ? "TCP" : "UDP",
		purple_network_upnp_mapping_remove_cb, NULL);
	g_hash_table_remove(upnp_port_mappings, GINT_TO_POINTER(port));
}

static void
purple_network_nat_pmp_mapping_remove(gpointer key, gpointer value,
	gpointer user_data)
{
	gint port = GPOINTER_TO_INT(key);
	gint protocol = GPOINTER_TO_INT(value);
	purple_debug_info("network", "removing NAT-PMP port mapping for port %d\n",
		port);
	purple_pmp_destroy_map(
		protocol == SOCK_STREAM ? PURPLE_PMP_TYPE_TCP : PURPLE_PMP_TYPE_UDP,
		port);
	g_hash_table_remove(nat_pmp_port_mappings, GINT_TO_POINTER(port));
}

void
purple_network_remove_port_mapping(gint fd)
{
	int port = purple_network_get_port_from_fd(fd);
	gint protocol = GPOINTER_TO_INT(g_hash_table_lookup(upnp_port_mappings, GINT_TO_POINTER(port)));

	if (protocol) {
		purple_network_upnp_mapping_remove(GINT_TO_POINTER(port), GINT_TO_POINTER(protocol), NULL);
	} else {
		protocol = GPOINTER_TO_INT(g_hash_table_lookup(nat_pmp_port_mappings, GINT_TO_POINTER(port)));
		if (protocol) {
			purple_network_nat_pmp_mapping_remove(GINT_TO_POINTER(port), GINT_TO_POINTER(protocol), NULL);
		}
	}
}

int purple_network_convert_idn_to_ascii(const gchar *in, gchar **out)
{
#ifdef USE_IDN
	char *tmp;
	int ret;

	g_return_val_if_fail(out != NULL, -1);

	ret = idna_to_ascii_8z(in, &tmp, IDNA_USE_STD3_ASCII_RULES);
	if (ret != IDNA_SUCCESS) {
		*out = NULL;
		return ret;
	}

	*out = g_strdup(tmp);
	/* This *MUST* be freed with free, not g_free */
	free(tmp);
	return 0;
#else
	g_return_val_if_fail(out != NULL, -1);

	*out = g_strdup(in);
	return 0;
#endif
}

gboolean
_purple_network_set_common_socket_flags(int fd)
{
	int flags;
	gboolean succ = TRUE;

	g_return_val_if_fail(fd >= 0, FALSE);

	flags = fcntl(fd, F_GETFL);

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
		purple_debug_warning("network",
			"Couldn't set O_NONBLOCK flag\n");
		succ = FALSE;
	}

#ifndef _WIN32
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
		purple_debug_warning("network",
			"Couldn't set FD_CLOEXEC flag\n");
		succ = FALSE;
	}
#endif

	return succ;
}

void
purple_network_init(void)
{
	purple_prefs_add_none  ("/purple/network");
	purple_prefs_add_string("/purple/network/stun_server", "");
	purple_prefs_add_string("/purple/network/turn_server", "");
	purple_prefs_add_int   ("/purple/network/turn_port", 3478);
	purple_prefs_add_int	 ("/purple/network/turn_port_tcp", 3478);
	purple_prefs_add_string("/purple/network/turn_username", "");
	purple_prefs_add_string("/purple/network/turn_password", "");
	purple_prefs_add_bool  ("/purple/network/auto_ip", TRUE);
	purple_prefs_add_string("/purple/network/public_ip", "");
	purple_prefs_add_bool  ("/purple/network/map_ports", TRUE);
	purple_prefs_add_bool  ("/purple/network/ports_range_use", FALSE);
	purple_prefs_add_int   ("/purple/network/ports_range_start", 1024);
	purple_prefs_add_int   ("/purple/network/ports_range_end", 2048);

	if(purple_prefs_get_bool("/purple/network/map_ports") || purple_prefs_get_bool("/purple/network/auto_ip"))
		purple_upnp_discover(NULL, NULL);

	purple_pmp_init();
	purple_upnp_init();

	purple_network_set_stun_server(
		purple_prefs_get_string("/purple/network/stun_server"));
	purple_network_set_turn_server(
		purple_prefs_get_string("/purple/network/turn_server"));

	upnp_port_mappings = g_hash_table_new(g_direct_hash, g_direct_equal);
	nat_pmp_port_mappings = g_hash_table_new(g_direct_hash, g_direct_equal);
}



void
purple_network_uninit(void)
{
	g_free(stun_ip);

	g_hash_table_destroy(upnp_port_mappings);
	g_hash_table_destroy(nat_pmp_port_mappings);

	/* TODO: clean up remaining port mappings, note calling
	 purple_upnp_remove_port_mapping from here doesn't quite work... */
}
