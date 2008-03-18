/**
 * @file dnsquery.c DNS query API
 * @ingroup core
 */

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
 *
 */

#include "internal.h"
#include "debug.h"
#include "dnsquery.h"
#include "notify.h"
#include "prefs.h"
#include "util.h"

/**************************************************************************
 * DNS query API
 **************************************************************************/

static PurpleDnsQueryUiOps *dns_query_ui_ops = NULL;

typedef struct _PurpleDnsQueryResolverProcess PurpleDnsQueryResolverProcess;

struct _PurpleDnsQueryData {
	char *hostname;
	int port;
	PurpleDnsQueryConnectFunction callback;
	gpointer data;
	guint timeout;

#if defined(__unix__) || defined(__APPLE__)
	PurpleDnsQueryResolverProcess *resolver;
#elif defined _WIN32 /* end __unix__ || __APPLE__ */
	GThread *resolver;
	GSList *hosts;
	gchar *error_message;
#endif
};

#if defined(__unix__) || defined(__APPLE__)

#define MAX_DNS_CHILDREN 4

/*
 * This structure keeps a reference to a child resolver process.
 */
struct _PurpleDnsQueryResolverProcess {
	guint inpa;
	int fd_in, fd_out;
	pid_t dns_pid;
};

static GSList *free_dns_children = NULL;
static GSList *queued_requests = NULL;

static int number_of_dns_children = 0;

/*
 * This is a convenience struct used to pass data to
 * the child resolver process.
 */
typedef struct {
	char hostname[512];
	int port;
} dns_params_t;
#endif

static void
purple_dnsquery_resolved(PurpleDnsQueryData *query_data, GSList *hosts)
{
	purple_debug_info("dnsquery", "IP resolved for %s\n", query_data->hostname);
	if (query_data->callback != NULL)
		query_data->callback(hosts, query_data->data, NULL);
	else
	{
		/*
		 * Callback is a required parameter, but it can get set to
		 * NULL if we cancel a thread-based DNS lookup.  So we need
		 * to free hosts.
		 */
		while (hosts != NULL)
		{
			hosts = g_slist_remove(hosts, hosts->data);
			g_free(hosts->data);
			hosts = g_slist_remove(hosts, hosts->data);
		}
	}

	purple_dnsquery_destroy(query_data);
}

static void
purple_dnsquery_failed(PurpleDnsQueryData *query_data, const gchar *error_message)
{
	purple_debug_info("dnsquery", "%s\n", error_message);
	if (query_data->callback != NULL)
		query_data->callback(NULL, query_data->data, error_message);
	purple_dnsquery_destroy(query_data);
}

static gboolean
purple_dnsquery_ui_resolve(PurpleDnsQueryData *query_data)
{
	PurpleDnsQueryUiOps *ops = purple_dnsquery_get_ui_ops();

	if (ops && ops->resolve_host)
	{
		if (ops->resolve_host(query_data, purple_dnsquery_resolved, purple_dnsquery_failed))
			return TRUE;
	}

	return FALSE;
}

#if defined(__unix__) || defined(__APPLE__)

/*
 * Unix!
 */

/*
 * Begin the DNS resolver child process functions.
 */
#ifdef HAVE_SIGNAL_H
G_GNUC_NORETURN static void
trap_gdb_bug(int sig)
{
	const char *message =
		"Purple's DNS child got a SIGTRAP signal.\n"
		"This can be caused by trying to run purple inside gdb.\n"
		"There is a known gdb bug which prevents this.  Supposedly purple\n"
		"should have detected you were using gdb and used an ugly hack,\n"
		"check cope_with_gdb_brokenness() in dnsquery.c.\n\n"
		"For more info about this bug, see http://sources.redhat.com/ml/gdb/2001-07/msg00349.html\n";
	fputs("\n* * *\n",stderr);
	fputs(message,stderr);
	fputs("* * *\n\n",stderr);
	execlp("xmessage","xmessage","-center", message, NULL);
	_exit(1);
}
#endif

static void
write_to_parent(int fd, const void *buf, size_t count)
{
	ssize_t written;

	written = write(fd, buf, count);
	if (written != count) {
		if (written < 0)
			fprintf(stderr, "dns[%d]: Error writing data to "
					"parent: %s\n", getpid(), strerror(errno));
		else
			fprintf(stderr, "dns[%d]: Error: Tried to write %"
					G_GSIZE_FORMAT " bytes to parent but instead "
					"wrote %" G_GSIZE_FORMAT " bytes\n",
					getpid(), count, written);
	}
}

G_GNUC_NORETURN static void
purple_dnsquery_resolver_run(int child_out, int child_in, gboolean show_debug)
{
	dns_params_t dns_params;
	const size_t zero = 0;
	int rc;
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res, *tmp;
	char servname[20];
#else
	struct sockaddr_in sin;
	const size_t addrlen = sizeof(sin);
#endif

#ifdef HAVE_SIGNAL_H
	purple_restore_default_signal_handlers();
	signal(SIGTRAP, trap_gdb_bug);
#endif

	/*
	 * We resolve 1 host name for each iteration of this
	 * while loop.
	 *
	 * The top half of this reads in the hostname and port
	 * number from the socket with our parent.  The bottom
	 * half of this resolves the IP (blocking) and sends
	 * the result back to our parent, when finished.
	 */
	while (1) {
		const char ch = 'Y';
		fd_set fds;
		struct timeval tv = { .tv_sec = 40 , .tv_usec = 0 };
		FD_ZERO(&fds);
		FD_SET(child_in, &fds);
		rc = select(child_in + 1, &fds, NULL, NULL, &tv);
		if (!rc) {
			if (show_debug)
				printf("dns[%d]: nobody needs me... =(\n", getpid());
			break;
		}
		rc = read(child_in, &dns_params, sizeof(dns_params_t));
		if (rc < 0) {
			fprintf(stderr, "dns[%d]: Error: Could not read dns_params: "
					"%s\n", getpid(), strerror(errno));
			break;
		}
		if (rc == 0) {
			if (show_debug)
				printf("dns[%d]: Oops, father has gone, wait for me, wait...!\n", getpid());
			_exit(0);
		}
		if (dns_params.hostname[0] == '\0') {
			fprintf(stderr, "dns[%d]: Error: Parent requested resolution "
					"of an empty hostname (port = %d)!!!\n", getpid(),
					dns_params.port);
			_exit(1);
		}
		/* Tell our parent that we read the data successfully */
		write_to_parent(child_out, &ch, sizeof(ch));

		/* We have the hostname and port, now resolve the IP */

#ifdef HAVE_GETADDRINFO
		g_snprintf(servname, sizeof(servname), "%d", dns_params.port);
		memset(&hints, 0, sizeof(hints));

		/* This is only used to convert a service
		 * name to a port number. As we know we are
		 * passing a number already, we know this
		 * value will not be really used by the C
		 * library.
		 */
		hints.ai_socktype = SOCK_STREAM;
		rc = getaddrinfo(dns_params.hostname, servname, &hints, &res);
		write_to_parent(child_out, &rc, sizeof(rc));
		if (rc != 0) {
			close(child_out);
			if (show_debug)
				printf("dns[%d] Error: getaddrinfo returned %d\n",
					getpid(), rc);
			dns_params.hostname[0] = '\0';
			continue;
		}
		tmp = res;
		while (res) {
			size_t ai_addrlen = res->ai_addrlen;
			write_to_parent(child_out, &ai_addrlen, sizeof(ai_addrlen));
			write_to_parent(child_out, res->ai_addr, res->ai_addrlen);
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
		write_to_parent(child_out, &zero, sizeof(zero));
#else
		if (!inet_aton(dns_params.hostname, &sin.sin_addr)) {
			struct hostent *hp;
			if (!(hp = gethostbyname(dns_params.hostname))) {
				write_to_parent(child_out, &h_errno, sizeof(int));
				close(child_out);
				if (show_debug)
					printf("DNS Error: %d\n", h_errno);
				_exit(0);
			}
			memset(&sin, 0, sizeof(struct sockaddr_in));
			memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
			sin.sin_family = hp->h_addrtype;
		} else
			sin.sin_family = AF_INET;

		sin.sin_port = htons(dns_params.port);
		write_to_parent(child_out, &zero, sizeof(zero));
		write_to_parent(child_out, &addrlen, sizeof(addrlen));
		write_to_parent(child_out, &sin, addrlen);
		write_to_parent(child_out, &zero, sizeof(zero));
#endif
		dns_params.hostname[0] = '\0';
	}

	close(child_out);
	close(child_in);

	_exit(0);
}
/*
 * End the DNS resolver child process functions.
 */

/*
 * Begin the functions for dealing with the DNS child processes.
 */
static void
cope_with_gdb_brokenness(void)
{
#ifdef __linux__
	static gboolean already_done = FALSE;
	char s[256], e[512];
	int n;
	pid_t ppid;

	if(already_done)
		return;
	already_done = TRUE;
	ppid = getppid();
	snprintf(s, sizeof(s), "/proc/%d/exe", ppid);
	n = readlink(s, e, sizeof(e));
	if(n < 0)
		return;

	e[MIN(n,sizeof(e)-1)] = '\0';

	if(strstr(e,"gdb")) {
		purple_debug_info("dns",
				   "Debugger detected, performing useless query...\n");
		gethostbyname("x.x.x.x.x");
	}
#endif
}

static void
purple_dnsquery_resolver_destroy(PurpleDnsQueryResolverProcess *resolver)
{
	g_return_if_fail(resolver != NULL);

	/*
	 * We might as well attempt to kill our child process.  It really
	 * doesn't matter if this fails, because children will expire on
	 * their own after a few seconds.
	 */
	if (resolver->dns_pid > 0)
		kill(resolver->dns_pid, SIGKILL);

	if (resolver->inpa != 0)
		purple_input_remove(resolver->inpa);

	close(resolver->fd_in);
	close(resolver->fd_out);

	g_free(resolver);

	number_of_dns_children--;
}

static PurpleDnsQueryResolverProcess *
purple_dnsquery_resolver_new(gboolean show_debug)
{
	PurpleDnsQueryResolverProcess *resolver;
	int child_out[2], child_in[2];

	/* Create pipes for communicating with the child process */
	if (pipe(child_out) || pipe(child_in)) {
		purple_debug_error("dns",
				   "Could not create pipes: %s\n", g_strerror(errno));
		return NULL;
	}

	resolver = g_new(PurpleDnsQueryResolverProcess, 1);
	resolver->inpa = 0;

	cope_with_gdb_brokenness();

	/* "Go fork and multiply." --Tommy Caldwell (Emily's dad, not the climber) */
	resolver->dns_pid = fork();

	/* If we are the child process... */
	if (resolver->dns_pid == 0) {
		/* We should not access the parent's side of the pipes, so close them */
		close(child_out[0]);
		close(child_in[1]);

		purple_dnsquery_resolver_run(child_out[1], child_in[0], show_debug);
		/* The thread calls _exit() rather than returning, so we never get here */
	}

	/* We should not access the child's side of the pipes, so close them */
	close(child_out[1]);
	close(child_in[0]);
	if (resolver->dns_pid == -1) {
		purple_debug_error("dns",
				   "Could not create child process for DNS: %s\n",
				   g_strerror(errno));
		purple_dnsquery_resolver_destroy(resolver);
		return NULL;
	}

	resolver->fd_out = child_out[0];
	resolver->fd_in = child_in[1];
	number_of_dns_children++;
	purple_debug_info("dns",
			   "Created new DNS child %d, there are now %d children.\n",
			   resolver->dns_pid, number_of_dns_children);

	return resolver;
}

/**
 * @return TRUE if the request was sent succesfully.  FALSE
 * 		if the request could not be sent.  This isn't
 * 		necessarily an error.  If the child has expired,
 * 		for example, we won't be able to send the message.
 */
static gboolean
send_dns_request_to_child(PurpleDnsQueryData *query_data,
		PurpleDnsQueryResolverProcess *resolver)
{
	pid_t pid;
	dns_params_t dns_params;
	int rc;
	char ch;

	/* This waitpid might return the child's PID if it has recently
	 * exited, or it might return an error if it exited "long
	 * enough" ago that it has already been reaped; in either
	 * instance, we can't use it. */
	pid = waitpid(resolver->dns_pid, NULL, WNOHANG);
	if (pid > 0) {
		purple_debug_warning("dns", "DNS child %d no longer exists\n",
				resolver->dns_pid);
		purple_dnsquery_resolver_destroy(resolver);
		return FALSE;
	} else if (pid < 0) {
		purple_debug_warning("dns", "Wait for DNS child %d failed: %s\n",
				resolver->dns_pid, g_strerror(errno));
		purple_dnsquery_resolver_destroy(resolver);
		return FALSE;
	}

	/* Copy the hostname and port into a single data structure */
	strncpy(dns_params.hostname, query_data->hostname, sizeof(dns_params.hostname) - 1);
	dns_params.hostname[sizeof(dns_params.hostname) - 1] = '\0';
	dns_params.port = query_data->port;

	/* Send the data structure to the child */
	rc = write(resolver->fd_in, &dns_params, sizeof(dns_params));
	if (rc < 0) {
		purple_debug_error("dns", "Unable to write to DNS child %d: %s\n",
				resolver->dns_pid, g_strerror(errno));
		purple_dnsquery_resolver_destroy(resolver);
		return FALSE;
	}

	g_return_val_if_fail(rc == sizeof(dns_params), -1);

	/* Did you hear me? (This avoids some race conditions) */
	rc = read(resolver->fd_out, &ch, sizeof(ch));
	if (rc != 1 || ch != 'Y')
	{
		purple_debug_warning("dns",
				"DNS child %d not responding. Killing it!\n",
				resolver->dns_pid);
		purple_dnsquery_resolver_destroy(resolver);
		return FALSE;
	}

	purple_debug_info("dns",
			"Successfully sent DNS request to child %d\n",
			resolver->dns_pid);

	query_data->resolver = resolver;

	return TRUE;
}

static void host_resolved(gpointer data, gint source, PurpleInputCondition cond);

static void
handle_next_queued_request(void)
{
	PurpleDnsQueryData *query_data;
	PurpleDnsQueryResolverProcess *resolver;

	if (queued_requests == NULL)
		/* No more DNS queries, yay! */
		return;

	query_data = queued_requests->data;
	queued_requests = g_slist_delete_link(queued_requests, queued_requests);

	if (purple_dnsquery_ui_resolve(query_data))
	{
		/* The UI is handling the resolve; we're done */
		handle_next_queued_request();
		return;
	}

	/*
	 * If we have any children, attempt to have them perform the DNS
	 * query.  If we're able to send the query then resolver will be
	 * set to the PurpleDnsQueryResolverProcess.  Otherwise, resolver
	 * will be NULL and we'll need to create a new DNS request child.
	 */
	while (free_dns_children != NULL)
	{
		resolver = free_dns_children->data;
		free_dns_children = g_slist_remove(free_dns_children, resolver);

		if (send_dns_request_to_child(query_data, resolver))
			/* We found an acceptable child, yay */
			break;
	}

	/* We need to create a new DNS request child */
	if (query_data->resolver == NULL)
	{
		if (number_of_dns_children >= MAX_DNS_CHILDREN)
		{
			/* Apparently all our children are busy */
			queued_requests = g_slist_prepend(queued_requests, query_data);
			return;
		}

		resolver = purple_dnsquery_resolver_new(purple_debug_is_enabled());
		if (resolver == NULL)
		{
			purple_dnsquery_failed(query_data, _("Unable to create new resolver process\n"));
			return;
		}
		if (!send_dns_request_to_child(query_data, resolver))
		{
			purple_dnsquery_failed(query_data, _("Unable to send request to resolver process\n"));
			return;
		}
	}

	query_data->resolver->inpa = purple_input_add(query_data->resolver->fd_out,
			PURPLE_INPUT_READ, host_resolved, query_data);
}

/*
 * End the functions for dealing with the DNS child processes.
 */

static void
host_resolved(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleDnsQueryData *query_data;
	int rc, err;
	GSList *hosts = NULL;
	struct sockaddr *addr = NULL;
	size_t addrlen;
	char message[1024];

	query_data = data;

	purple_debug_info("dns", "Got response for '%s'\n", query_data->hostname);
	purple_input_remove(query_data->resolver->inpa);
	query_data->resolver->inpa = 0;

	rc = read(query_data->resolver->fd_out, &err, sizeof(err));
	if ((rc == 4) && (err != 0))
	{
#ifdef HAVE_GETADDRINFO
		g_snprintf(message, sizeof(message), _("Error resolving %s:\n%s"),
				query_data->hostname, purple_gai_strerror(err));
#else
		g_snprintf(message, sizeof(message), _("Error resolving %s: %d"),
				query_data->hostname, err);
#endif
		purple_dnsquery_failed(query_data, message);

	} else if (rc > 0) {
		/* Success! */
		while (rc > 0) {
			rc = read(query_data->resolver->fd_out, &addrlen, sizeof(addrlen));
			if (rc > 0 && addrlen > 0) {
				addr = g_malloc(addrlen);
				rc = read(query_data->resolver->fd_out, addr, addrlen);
				hosts = g_slist_append(hosts, GINT_TO_POINTER(addrlen));
				hosts = g_slist_append(hosts, addr);
			} else {
				break;
			}
		}
		/*	wait4(resolver->dns_pid, NULL, WNOHANG, NULL); */
		purple_dnsquery_resolved(query_data, hosts);

	} else if (rc == -1) {
		g_snprintf(message, sizeof(message), _("Error reading from resolver process:\n%s"), g_strerror(errno));
		purple_dnsquery_failed(query_data, message);

	} else if (rc == 0) {
		g_snprintf(message, sizeof(message), _("EOF while reading from resolver process"));
		purple_dnsquery_failed(query_data, message);
	}

	handle_next_queued_request();
}

static gboolean
resolve_host(gpointer data)
{
	PurpleDnsQueryData *query_data;

	query_data = data;
	query_data->timeout = 0;

	handle_next_queued_request();

	return FALSE;
}

PurpleDnsQueryData *
purple_dnsquery_a(const char *hostname, int port,
				PurpleDnsQueryConnectFunction callback, gpointer data)
{
	PurpleDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port	  != 0, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	query_data = g_new(PurpleDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;
	query_data->resolver = NULL;

	if (strlen(query_data->hostname) == 0)
	{
		purple_dnsquery_destroy(query_data);
		g_return_val_if_reached(NULL);
	}

	queued_requests = g_slist_append(queued_requests, query_data);

	purple_debug_info("dns", "DNS query for '%s' queued\n", query_data->hostname);

	query_data->timeout = purple_timeout_add(0, resolve_host, query_data);

	return query_data;
}

#elif defined _WIN32 /* end __unix__ || __APPLE__ */

/*
 * Windows!
 */

static gboolean
dns_main_thread_cb(gpointer data)
{
	PurpleDnsQueryData *query_data = data;

	/* We're done, so purple_dnsquery_destroy() shouldn't think it is canceling an in-progress lookup */
	query_data->resolver = NULL;

	if (query_data->error_message != NULL)
		purple_dnsquery_failed(query_data, query_data->error_message);
	else
	{
		GSList *hosts;

		/* We don't want purple_dns_query_resolved() to free(hosts) */
		hosts = query_data->hosts;
		query_data->hosts = NULL;
		purple_dnsquery_resolved(query_data, hosts);
	}

	return FALSE;
}

static gpointer
dns_thread(gpointer data)
{
	PurpleDnsQueryData *query_data;
#ifdef HAVE_GETADDRINFO
	int rc;
	struct addrinfo hints, *res, *tmp;
	char servname[20];
#else
	struct sockaddr_in sin;
	struct hostent *hp;
#endif

	query_data = data;

#ifdef HAVE_GETADDRINFO
	g_snprintf(servname, sizeof(servname), "%d", query_data->port);
	memset(&hints,0,sizeof(hints));

	/*
	 * This is only used to convert a service
	 * name to a port number. As we know we are
	 * passing a number already, we know this
	 * value will not be really used by the C
	 * library.
	 */
	hints.ai_socktype = SOCK_STREAM;
	if ((rc = getaddrinfo(query_data->hostname, servname, &hints, &res)) == 0) {
		tmp = res;
		while(res) {
			query_data->hosts = g_slist_append(query_data->hosts,
				GSIZE_TO_POINTER(res->ai_addrlen));
			query_data->hosts = g_slist_append(query_data->hosts,
				g_memdup(res->ai_addr, res->ai_addrlen));
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
	} else {
		query_data->error_message = g_strdup_printf(_("Error resolving %s:\n%s"), query_data->hostname, purple_gai_strerror(rc));
	}
#else
	if ((hp = gethostbyname(query_data->hostname))) {
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = htons(query_data->port);

		query_data->hosts = g_slist_append(query_data->hosts,
				GSIZE_TO_POINTER(sizeof(sin)));
		query_data->hosts = g_slist_append(query_data->hosts,
				g_memdup(&sin, sizeof(sin)));
	} else {
		query_data->error_message = g_strdup_printf(_("Error resolving %s: %d"), query_data->hostname, h_errno);
	}
#endif

	/* back to main thread */
	purple_timeout_add(0, dns_main_thread_cb, query_data);

	return 0;
}

static gboolean
resolve_host(gpointer data)
{
	PurpleDnsQueryData *query_data;
	struct sockaddr_in sin;
	GError *err = NULL;

	query_data = data;
	query_data->timeout = 0;

	if (purple_dnsquery_ui_resolve(query_data))
	{
		/* The UI is handling the resolve; we're done */
		return FALSE;
	}

	if (inet_aton(query_data->hostname, &sin.sin_addr))
	{
		/*
		 * The given "hostname" is actually an IP address, so we
		 * don't need to do anything.
		 */
		GSList *hosts = NULL;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(query_data->port);
		hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(sin)));
		hosts = g_slist_append(hosts, g_memdup(&sin, sizeof(sin)));
		purple_dnsquery_resolved(query_data, hosts);
	}
	else
	{
		/*
		 * Spin off a separate thread to perform the DNS lookup so
		 * that we don't block the UI.
		 */
		query_data->resolver = g_thread_create(dns_thread,
				query_data, FALSE, &err);
		if (query_data->resolver == NULL)
		{
			char message[1024];
			g_snprintf(message, sizeof(message), _("Thread creation failure: %s"),
					(err && err->message) ? err->message : _("Unknown reason"));
			g_error_free(err);
			purple_dnsquery_failed(query_data, message);
		}
	}

	return FALSE;
}

PurpleDnsQueryData *
purple_dnsquery_a(const char *hostname, int port,
				PurpleDnsQueryConnectFunction callback, gpointer data)
{
	PurpleDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port	  != 0, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	purple_debug_info("dnsquery", "Performing DNS lookup for %s\n", hostname);

	query_data = g_new0(PurpleDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;

	if (strlen(query_data->hostname) == 0)
	{
		purple_dnsquery_destroy(query_data);
		g_return_val_if_reached(NULL);
	}

	/* Don't call the callback before returning */
	query_data->timeout = purple_timeout_add(0, resolve_host, query_data);

	return query_data;
}

#else /* not __unix__ or __APPLE__ or _WIN32 */

/*
 * We weren't able to do anything fancier above, so use the
 * fail-safe name resolution code, which is blocking.
 */

static gboolean
resolve_host(gpointer data)
{
	PurpleDnsQueryData *query_data;
	struct sockaddr_in sin;
	GSList *hosts = NULL;

	query_data = data;
	query_data->timeout = 0;

	if (purple_dnsquery_ui_resolve(query_data))
	{
		/* The UI is handling the resolve; we're done */
		return FALSE;
	}

	if (!inet_aton(query_data->hostname, &sin.sin_addr)) {
		struct hostent *hp;
		if(!(hp = gethostbyname(query_data->hostname))) {
			char message[1024];
			g_snprintf(message, sizeof(message), _("Error resolving %s: %d"),
					query_data->hostname, h_errno);
			purple_dnsquery_failed(query_data, message);
			return FALSE;
		}
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
	} else
		sin.sin_family = AF_INET;
	sin.sin_port = htons(query_data->port);

	hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(sin)));
	hosts = g_slist_append(hosts, g_memdup(&sin, sizeof(sin)));

	purple_dnsquery_resolved(query_data, hosts);

	return FALSE;
}

PurpleDnsQueryData *
purple_dnsquery_a(const char *hostname, int port,
				PurpleDnsQueryConnectFunction callback, gpointer data)
{
	PurpleDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port	  != 0, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	query_data = g_new(PurpleDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;

	if (strlen(query_data->hostname) == 0)
	{
		purple_dnsquery_destroy(query_data);
		g_return_val_if_reached(NULL);
	}

	/* Don't call the callback before returning */
	query_data->timeout = purple_timeout_add(0, resolve_host, query_data);

	return query_data;
}

#endif /* not __unix__ or __APPLE__ or _WIN32 */

void
purple_dnsquery_destroy(PurpleDnsQueryData *query_data)
{
	PurpleDnsQueryUiOps *ops = purple_dnsquery_get_ui_ops();

	if (ops && ops->destroy)
		ops->destroy(query_data);

#if defined(__unix__) || defined(__APPLE__)
	queued_requests = g_slist_remove(queued_requests, query_data);

	if (query_data->resolver != NULL)
		/*
		 * Ideally we would tell our resolver child to stop resolving
		 * shit and then we would add it back to the free_dns_children
		 * linked list.  However, it's hard to tell children stuff,
		 * they just don't listen.
		 */
		purple_dnsquery_resolver_destroy(query_data->resolver);
#elif defined _WIN32 /* end __unix__ || __APPLE__ */
	if (query_data->resolver != NULL)
	{
		/*
		 * It's not really possible to kill a thread.  So instead we
		 * just set the callback to NULL and let the DNS lookup
		 * finish.
		 */
		query_data->callback = NULL;
		return;
	}

	while (query_data->hosts != NULL)
	{
		/* Discard the length... */
		query_data->hosts = g_slist_remove(query_data->hosts, query_data->hosts->data);
		/* Free the address... */
		g_free(query_data->hosts->data);
		query_data->hosts = g_slist_remove(query_data->hosts, query_data->hosts->data);
	}
	g_free(query_data->error_message);
#endif

	if (query_data->timeout > 0)
		purple_timeout_remove(query_data->timeout);

	g_free(query_data->hostname);
	g_free(query_data);
}

char *
purple_dnsquery_get_host(PurpleDnsQueryData *query_data)
{
	g_return_val_if_fail(query_data != NULL, NULL);

	return query_data->hostname;
}

unsigned short
purple_dnsquery_get_port(PurpleDnsQueryData *query_data)
{
	g_return_val_if_fail(query_data != NULL, 0);

	return query_data->port;
}

void
purple_dnsquery_set_ui_ops(PurpleDnsQueryUiOps *ops)
{
	dns_query_ui_ops = ops;
}

PurpleDnsQueryUiOps *
purple_dnsquery_get_ui_ops(void)
{
	/* It is perfectly acceptable for dns_query_ui_ops to be NULL; this just
	 * means that the default platform-specific implementation will be used.
	 */
	return dns_query_ui_ops;
}

void
purple_dnsquery_init(void)
{
}

void
purple_dnsquery_uninit(void)
{
#if defined(__unix__) || defined(__APPLE__)
	while (free_dns_children != NULL)
	{
		purple_dnsquery_resolver_destroy(free_dns_children->data);
		free_dns_children = g_slist_remove(free_dns_children, free_dns_children->data);
	}
#endif
}
