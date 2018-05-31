/*
 * purple
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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

#include <config.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <time.h>
#include <glib.h>
#include "debug.h"
#include "libc_internal.h"
#include "util.h"
#include <glib/gstdio.h>
#include "util.h"

/** This is redefined here because we can't include internal.h */
#ifdef ENABLE_NLS
#  include <locale.h>
#  include <libintl.h>
#  define _(String) ((const char *)dgettext(PACKAGE, String))
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  include <locale.h>
#  define N_(String) (String)
#  ifndef _
#    define _(String) ((const char *)String)
#  endif
#  define ngettext(Singular, Plural, Number) ((Number == 1) ? ((const char *)Singular) : ((const char *)Plural))
#  define dngettext(Domain, Singular, Plural, Number) ((Number == 1) ? ((const char *)Singular) : ((const char *)Plural))
#endif

#ifndef S_ISDIR
# define S_ISDIR(m) (((m)&S_IFDIR)==S_IFDIR)
#endif

static char errbuf[1024];

/* helpers */
static int wpurple_is_socket( int fd ) {
	int optval;
	int optlen = sizeof(int);

	if( (getsockopt(fd, SOL_SOCKET, SO_TYPE, (void*)&optval, &optlen)) == SOCKET_ERROR ) {
		int error = WSAGetLastError();
		if( error == WSAENOTSOCK )
			return FALSE;
		else {
                        purple_debug(PURPLE_DEBUG_WARNING, "wpurple", "wpurple_is_socket: getsockopt returned error: %d\n", error);
			return FALSE;
		}
	}
	return TRUE;
}

/* socket.h */
int wpurple_socket (int namespace, int style, int protocol) {
	int ret;

	ret = socket( namespace, style, protocol );

	if (ret == (int)INVALID_SOCKET) {
		errno = WSAGetLastError();
		return -1;
	}
	return ret;
}

int wpurple_connect(int socket, struct sockaddr *addr, u_long length) {
	int ret;

	ret = connect( socket, addr, length );

	if( ret == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		if( errno == WSAEWOULDBLOCK )
			errno = WSAEINPROGRESS;
		return -1;
	}
	return 0;
}

int wpurple_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlenptr) {
	if(getsockopt(socket, level, optname, optval, optlenptr) == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		return -1;
	}
	return 0;
}

int wpurple_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen) {
	if(setsockopt(socket, level, optname, optval, optlen) == SOCKET_ERROR ) {
		errno = WSAGetLastError();
		return -1;
	}
	return 0;
}

int wpurple_getsockname(int socket, struct sockaddr *addr, socklen_t *lenptr) {
        if(getsockname(socket, addr, lenptr) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

int wpurple_bind(int socket, struct sockaddr *addr, socklen_t length) {
        if(bind(socket, addr, length) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

int wpurple_listen(int socket, unsigned int n) {
        if(listen(socket, n) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

int wpurple_sendto(int socket, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
	int ret;
	if ((ret = sendto(socket, buf, len, flags, to, tolen)
			) == SOCKET_ERROR) {
		errno = WSAGetLastError();
		if(errno == WSAEWOULDBLOCK || errno == WSAEINPROGRESS)
			errno = EAGAIN;
		return -1;
	}
	return ret;
}

/* fcntl.h */
/* This is not a full implementation of fcntl. Update as needed.. */
int wpurple_fcntl(int socket, int command, ...) {

	switch( command ) {
	case F_GETFL:
		return 0;

	case F_SETFL:
	{
		va_list args;
		int val;
		int ret=0;

		va_start(args, command);
		val = va_arg(args, int);
		va_end(args);

		switch( val ) {
		case O_NONBLOCK:
		{
			u_long imode=1;
			ret = ioctlsocket(socket, FIONBIO, &imode);
			break;
		}
		case 0:
		{
			u_long imode=0;
			ret = ioctlsocket(socket, FIONBIO, &imode);
			break;
		}
		default:
			errno = EINVAL;
			return -1;
		}/*end switch*/
		if( ret == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
		return 0;
	}
	default:
                purple_debug(PURPLE_DEBUG_WARNING, "wpurple", "wpurple_fcntl: Unsupported command\n");
		return -1;
	}/*end switch*/
}

/* sys/ioctl.h */
int wpurple_ioctl(int fd, int command, void* val) {
	switch( command ) {
	case FIONBIO:
	{
		if (ioctlsocket(fd, FIONBIO, (unsigned long *)val) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
		}
		return 0;
	}
	case SIOCGIFCONF:
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(fd, SIO_GET_INTERFACE_LIST,
				0, 0, &InterfaceList,
				sizeof(InterfaceList), &nBytesReturned,
				0, 0) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
		} else {
			int i;
			struct ifconf *ifc = val;
			char *tmp = ifc->ifc_buf;
			int nNumInterfaces =
				nBytesReturned / sizeof(INTERFACE_INFO);
			for (i = 0; i < nNumInterfaces; i++) {
				INTERFACE_INFO ii = InterfaceList[i];
				struct ifreq *ifr = (struct ifreq *) tmp;
				struct sockaddr_in *sa = (struct sockaddr_in *) &ifr->ifr_addr;

				sa->sin_family = ii.iiAddress.AddressIn.sin_family;
				sa->sin_port = ii.iiAddress.AddressIn.sin_port;
				sa->sin_addr.s_addr = ii.iiAddress.AddressIn.sin_addr.s_addr;
				tmp += sizeof(struct ifreq);

				/* Make sure that we can fit in the original buffer */
				if (tmp >= (ifc->ifc_buf + ifc->ifc_len + sizeof(struct ifreq))) {
					break;
				}
			}
			/* Replace the length with the actually used length */
			ifc->ifc_len = ifc->ifc_len - (ifc->ifc_buf - tmp);
			return 0;
		}
	}
	default:
		errno = EINVAL;
		return -1;
	}/*end switch*/
}

/* arpa/inet.h */
int wpurple_inet_aton(const char *name, struct in_addr *addr) {
	if((addr->s_addr = inet_addr(name)) == INADDR_NONE)
		return 0;
	else
		return 1;
}

/* Thanks to GNU wget for this inet_ntop() implementation */
const char *
wpurple_inet_ntop (int af, const void *src, char *dst, socklen_t cnt)
{
  /* struct sockaddr can't accomodate struct sockaddr_in6. */
  union {
    struct sockaddr_in6 sin6;
    struct sockaddr_in sin;
  } sa;
  DWORD dstlen = cnt;
  size_t srcsize;

  ZeroMemory(&sa, sizeof(sa));
  switch (af)
    {
    case AF_INET:
      sa.sin.sin_family = AF_INET;
      sa.sin.sin_addr = *(struct in_addr *) src;
      srcsize = sizeof (sa.sin);
      break;
    case AF_INET6:
      sa.sin6.sin6_family = AF_INET6;
      sa.sin6.sin6_addr = *(struct in6_addr *) src;
      srcsize = sizeof (sa.sin6);
      break;
    default:
      abort ();
    }

  if (WSAAddressToString ((struct sockaddr *) &sa, srcsize, NULL, dst, &dstlen) != 0)
    {
      errno = WSAGetLastError();
      return NULL;
    }
  return (const char *) dst;
}

int
wpurple_inet_pton(int af, const char *src, void *dst)
{
	/* struct sockaddr can't accomodate struct sockaddr_in6. */
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr_in sin;
	} sa;
	int srcsize;
	
	switch(af)
	{
		case AF_INET:
			sa.sin.sin_family = AF_INET;
			srcsize = sizeof (sa.sin);
		break;
		case AF_INET6:
			sa.sin6.sin6_family = AF_INET6;
			srcsize = sizeof (sa.sin6);
		break;
		default:
			errno = WSAEPFNOSUPPORT;
			return -1;
	}
	
	if (WSAStringToAddress((LPTSTR)src, af, NULL, (struct sockaddr *) &sa, &srcsize) != 0)
	{
		errno = WSAGetLastError();
		return -1;
	}
	
	switch(af)
	{
		case AF_INET:
			memcpy(dst, &sa.sin.sin_addr, sizeof(sa.sin.sin_addr));
		break;
		case AF_INET6:
			memcpy(dst, &sa.sin6.sin6_addr, sizeof(sa.sin6.sin6_addr));
		break;
	}
	
	return 1;
}


/* netdb.h */
struct hostent* wpurple_gethostbyname(const char *name) {
	struct hostent *hp;

	if((hp = gethostbyname(name)) == NULL) {
		errno = WSAGetLastError();
		return NULL;
	}
	return hp;
}

/* string.h */
char* wpurple_strerror(int errornum) {
	if (errornum > WSABASEERR) {
		switch(errornum) {
			case WSAECONNABORTED: /* 10053 */
				g_snprintf(errbuf, sizeof(errbuf), "%s", _("Connection interrupted by other software on your computer."));
				break;
			case WSAECONNRESET: /* 10054 */
				g_snprintf(errbuf, sizeof(errbuf), "%s", _("Remote host closed connection."));
				break;
			case WSAETIMEDOUT: /* 10060 */
				g_snprintf(errbuf, sizeof(errbuf), "%s", _("Connection timed out."));
				break;
			case WSAECONNREFUSED: /* 10061 */
				g_snprintf(errbuf, sizeof(errbuf), "%s", _("Connection refused."));
				break;
			case WSAEADDRINUSE: /* 10048 */
				g_snprintf(errbuf, sizeof(errbuf), "%s", _("Address already in use."));
				break;
			default:
				g_snprintf(errbuf, sizeof(errbuf), "Windows socket error #%d", errornum);
		}
	} else {
		const char *tmp = g_strerror(errornum);
		g_snprintf(errbuf, sizeof(errbuf), "%s", tmp);
	}
	return errbuf;
}

/* unistd.h */

/*
 *  We need to figure out whether fd is a file or socket handle.
 */
int wpurple_read(int fd, void *buf, unsigned int size) {
	int ret;

	if (fd < 0) {
		errno = EBADF;
		g_return_val_if_reached(-1);
	}

	if(wpurple_is_socket(fd)) {
		if((ret = recv(fd, buf, size, 0)) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			if(errno == WSAEWOULDBLOCK || errno == WSAEINPROGRESS)
				errno = EAGAIN;
			return -1;
		}
		else {
			/* success reading socket */
			return ret;
		}
	} else {
		/* fd is not a socket handle.. pass it off to read */
		return _read(fd, buf, size);
	}
}

int wpurple_send(int fd, const void *buf, unsigned int size, int flags) {
	int ret;

	ret = send(fd, buf, size, flags);

	if (ret == SOCKET_ERROR) {
		errno = WSAGetLastError();
		if(errno == WSAEWOULDBLOCK || errno == WSAEINPROGRESS)
			errno = EAGAIN;
		return -1;
	}
	return ret;
}

int wpurple_write(int fd, const void *buf, unsigned int size) {

	if (fd < 0) {
		errno = EBADF;
		g_return_val_if_reached(-1);
	}

	if(wpurple_is_socket(fd))
		return wpurple_send(fd, buf, size, 0);
	else
		return _write(fd, buf, size);
}

int wpurple_recv(int fd, void *buf, size_t len, int flags) {
	int ret;

	if((ret = recv(fd, buf, len, flags)) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			if(errno == WSAEWOULDBLOCK || errno == WSAEINPROGRESS)
				errno = EAGAIN;
			return -1;
	} else {
		return ret;
	}
}

int wpurple_close(int fd) {
	int ret;

	if (fd < 0) {
		errno = EBADF;
		g_return_val_if_reached(-1);
	}

	if( wpurple_is_socket(fd) ) {
		if( (ret = closesocket(fd)) == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
		else
			return 0;
	}
	else
		return _close(fd);
}

int wpurple_gethostname(char *name, size_t size) {
        if(gethostname(name, size) == SOCKET_ERROR) {
                errno = WSAGetLastError();
			return -1;
        }
        return 0;
}

/* sys/time.h */

int wpurple_gettimeofday(struct timeval *p, struct timezone *z) {
	int res = 0;
	struct _timeb timebuffer;

	if (z != 0) {
		_tzset();
		z->tz_minuteswest = _timezone/60;
		z->tz_dsttime = _daylight;
	}

	if (p != 0) {
		_ftime(&timebuffer);
		p->tv_sec = timebuffer.time;			/* seconds since 1-1-1970 */
		p->tv_usec = timebuffer.millitm*1000; 	/* microseconds */
	}

	return res;
}

/* time.h */

struct tm * wpurple_localtime_r (const time_t *time, struct tm *resultp) {
	struct tm* tmptm;

	if(!time)
		return NULL;
	tmptm = localtime(time);
	if(resultp && tmptm)
		return memcpy(resultp, tmptm, sizeof(struct tm));
	else
		return NULL;
}

