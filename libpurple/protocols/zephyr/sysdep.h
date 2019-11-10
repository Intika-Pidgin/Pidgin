/* This file is part of the Project Athena Zephyr Notification System.
 * It contains system-dependent header code.
 *
 *	Created by:	Greg Hudson
 *
 *	Copyright (c) 1988,1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#ifndef PURPLE_ZEPHYR_SYSDEP_H
#define PURPLE_ZEPHYR_SYSDEP_H

#include <config.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#ifndef WIN32
#include <syslog.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>

#include <stdlib.h>

/* Strings. */
#include <string.h>

/* Exit status handling and wait(). */
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#include <stdarg.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#else
# ifdef HAVE_SYS_FILE_H
#  include <sys/file.h>
# endif
uid_t getuid(void);
char *ttyname(void);
#endif

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_SYS_FILIO_H
#  include <sys/filio.h>
# else
#  ifdef HAVE_SGTTY_H
#   include <sgtty.h>
#  endif
#  ifdef HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#  endif
# endif
#endif

/* Kerberos compatibility. */
#ifdef ZEPHYR_USES_KERBEROS
# include <krb.h>
# ifndef WIN32
#  include <krb_err.h>
# endif /* WIN32 */
# include <des.h>
#endif /* ZEPHYR_USES_KERBEROS */

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef HAVE_SYS_MSGBUF_H
#include <sys/msgbuf.h>
#endif

#endif /* PURPLE_ZEPHYR_SYSDEP_H */
