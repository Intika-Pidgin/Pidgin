/* This file is part of the Project Athena Zephyr Notification System.
 * It contains global definitions
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987,1988,1991 by the Massachusetts Institute of
 *	Technology. For copying and distribution information, see the
 *	file "mit-copyright.h".
 */

#ifndef PURPLE_ZEPHYR_ZEPHYR_INTERNAL_H
#define PURPLE_ZEPHYR_ZEPHYR_INTERNAL_H

#include <config.h>

#include <glib.h>

#include <sys/types.h>
#include <sys/time.h>

#include <zephyr_err.h>

#ifndef IPPROTO_MAX	/* Make sure not already included */
#ifndef WIN32
#include <netinet/in.h>
#endif
#endif

#include <stdarg.h>

#ifdef WIN32
/* this really should be uint32_t */
/*typedef unsigned int in_addr_t;
struct in_addr
{
  in_addr_t s_addr;
}; */
#include <winsock2.h>
#endif

/* Service names */
#define	HM_SVCNAME		"zephyr-hm"
#define HM_SRV_SVCNAME		"zephyr-hm-srv"
#define	SERVER_SVCNAME		"zephyr-clt"
#define SERVER_SERVICE		"zephyr"
#define SERVER_INSTANCE		"zephyr"

#define ZVERSIONHDR	"ZEPH"
#define ZVERSIONMAJOR	0
#define ZVERSIONMINOR	2

#define Z_MAXPKTLEN		1024
#define Z_MAXHEADERLEN		800
#define Z_MAXOTHERFIELDS	10	/* Max unknown fields in ZNotice_t */
#define Z_NUMFIELDS		17

/* Authentication levels returned by ZCheckAuthentication */
#define ZAUTH_FAILED    	(-1)
#define ZAUTH_YES       	1
#define ZAUTH_NO        	0

typedef char ZPacket_t[Z_MAXPKTLEN];

/* Packet type */
typedef enum {
    UNSAFE, UNACKED, ACKED, HMACK, HMCTL, SERVACK, SERVNAK, CLIENTACK, STAT
} ZNotice_Kind_t;
extern const char *ZNoticeKinds[9];

/* Unique ID format */
typedef struct {
    struct	in_addr zuid_addr;
    struct	timeval	tv;
} ZUnique_Id_t;

/* Checksum */
typedef unsigned long ZChecksum_t;

/* Notice definition */
typedef struct {
    char		*z_packet;
    char		*z_version;
    ZNotice_Kind_t	z_kind;
    ZUnique_Id_t	z_uid;
#define z_sender_addr	z_uid.zuid_addr
    struct		timeval z_time;
    unsigned short	z_port;
    int			z_auth;
    int			z_checked_auth;
    int			z_authent_len;
    char		*z_ascii_authent;
    char		*z_class;
    const char		*z_class_inst;
    char		*z_opcode;
    char		*z_sender;
    const char		*z_recipient;
    char		*z_default_format;
    char		*z_multinotice;
    ZUnique_Id_t	z_multiuid;
    ZChecksum_t		z_checksum;
    int			z_num_other_fields;
    char		*z_other_fields[Z_MAXOTHERFIELDS];
    caddr_t		z_message;
    int			z_message_len;
} ZNotice_t;

/* Subscription structure */
typedef struct {
    char	*zsub_recipient;
    char	*zsub_class;
    char	*zsub_classinst;
} ZSubscription_t;

/* Function return code */
typedef int Code_t;

/* Locations structure */
typedef struct {
    char	*host;
    char	*time;
    char	*tty;
} ZLocations_t;

typedef struct {
    char		*user;
    ZUnique_Id_t	uid;
    char		*version;
} ZAsyncLocateData_t;

/* for ZSetDebug */
#ifdef Z_DEBUG
void (*__Z_debug_print)(const char *fmt, va_list args, void *closure);
void *__Z_debug_print_closure;
#endif

int ZCompareUIDPred(ZNotice_t *, void *);
int ZCompareMultiUIDPred(ZNotice_t *, void *);

/* Defines for ZFormatNotice, et al. */
typedef Code_t (*Z_AuthProc)(ZNotice_t *, char *, int, int *);
Code_t ZMakeAuthentication(ZNotice_t *, char *, int, int *);

char *ZGetSender(void);
const gchar *ZGetVariable(const gchar *);
Code_t ZSetVariable(char *var, char *value);
Code_t ZUnsetVariable(char *var);
int ZGetWGPort(void);
Code_t ZSetDestAddr(struct sockaddr_in *);
Code_t ZFormatNoticeList(ZNotice_t *, char **, int, char **, int *, Z_AuthProc);
Code_t ZParseNotice(char *, int, ZNotice_t *);
Code_t ZReadAscii(char *, int, unsigned char *, int);
Code_t ZReadAscii32(char *, int, unsigned long *);
Code_t ZReadAscii16(char *, int, unsigned short *);
Code_t ZSendPacket(char *, int, int);
Code_t ZSendList(ZNotice_t *, char *[], int, Z_AuthProc);
Code_t ZSrvSendList(ZNotice_t *, char *[], int, Z_AuthProc, Code_t (*)());
Code_t ZSendNotice(ZNotice_t *, Z_AuthProc);
Code_t ZSrvSendNotice(ZNotice_t *, Z_AuthProc, Code_t (*)());
Code_t ZFormatNotice(ZNotice_t *, char **, int *, Z_AuthProc);
Code_t ZFormatSmallNotice(ZNotice_t *, ZPacket_t, int *, Z_AuthProc);
Code_t ZFormatRawNoticeList(ZNotice_t *notice, char *list[], int nitems,
                            char **buffer, int *ret_len);
Code_t ZLocateUser(char *, int *, Z_AuthProc);
Code_t ZRequestLocations(const char *, ZAsyncLocateData_t *, ZNotice_Kind_t,
                         Z_AuthProc);
Code_t ZhmStat(struct in_addr *, ZNotice_t *);
Code_t ZInitialize(void);
Code_t ZSetServerState(int);
Code_t ZSetFD(int);
Code_t ZFormatSmallRawNotice(ZNotice_t *, ZPacket_t, int *);
int ZCompareUID(ZUnique_Id_t *, ZUnique_Id_t *);
Code_t ZMakeAscii(char *, int, unsigned char *, int);
Code_t ZMakeAscii32(char *, int, unsigned long);
Code_t ZMakeAscii16(char *, int, unsigned int);
Code_t ZReceivePacket(ZPacket_t, int *, struct sockaddr_in *);
Code_t ZCheckAuthentication(ZNotice_t *, struct sockaddr_in *);
Code_t ZSetLocation(char *exposure);
Code_t ZUnsetLocation(void);
Code_t ZFlushMyLocations(void);
Code_t ZFormatRawNotice(ZNotice_t *, char **, int *);
Code_t ZRetrieveSubscriptions(unsigned short, int *);
Code_t ZOpenPort(unsigned short *port);
Code_t ZClosePort(void);
Code_t ZFlushLocations(void);
Code_t ZFlushSubscriptions(void);
Code_t ZFreeNotice(ZNotice_t *notice);
Code_t ZParseLocations(register ZNotice_t *notice,
                       register ZAsyncLocateData_t *zald, int *nlocs,
                       char **user);
int ZCompareALDPred(ZNotice_t *notice, void *zald);
void ZFreeALD(register ZAsyncLocateData_t *zald);
Code_t ZCheckIfNotice(ZNotice_t *notice, struct sockaddr_in *from,
                      register int (*predicate)(ZNotice_t *, void *),
                      void *args);
Code_t ZPeekPacket(char **buffer, int *ret_len, struct sockaddr_in *from);
Code_t ZPeekNotice(ZNotice_t *notice, struct sockaddr_in *from);
Code_t ZIfNotice(ZNotice_t *notice, struct sockaddr_in *from,
                 int (*predicate)(ZNotice_t *, void *), void *args);
Code_t ZSubscribeTo(ZSubscription_t *sublist, int nitems, unsigned int port);
Code_t ZSubscribeToSansDefaults(ZSubscription_t *sublist, int nitems,
                                unsigned int port);
Code_t ZUnsubscribeTo(ZSubscription_t *sublist, int nitems, unsigned int port);
Code_t ZCancelSubscriptions(unsigned int port);
int ZPending(void);
Code_t ZReceiveNotice(ZNotice_t *notice, struct sockaddr_in *from);
#ifdef Z_DEBUG
void Z_debug(const char *, ...);
#endif

/* Compatibility */
#define	ZNewLocateUser ZLocateUser

/* Macros to retrieve Zephyr library values. */
extern int __Zephyr_fd;
extern int __Q_CompleteLength;
extern struct sockaddr_in __HM_addr;
extern char __Zephyr_realm[];
#define ZGetFD()	__Zephyr_fd
#define ZQLength()	__Q_CompleteLength
#define ZGetDestAddr()	__HM_addr
#define ZGetRealm()	__Zephyr_realm

#ifdef Z_DEBUG
void ZSetDebug(void (*)(const char *, va_list, void *), void *);
#define ZSetDebug(proc,closure)    (__Z_debug_print=(proc), \
				    __Z_debug_print_closure=(closure), \
				    (void) 0)
#else
#define	ZSetDebug(proc,closure)
#endif

/* Maximum queue length */
#define Z_MAXQLEN 		30

/* Successful function return */
#define ZERR_NONE		0

/* Hostmanager wait time (in secs) */
#define HM_TIMEOUT		1

/* Server wait time (in secs) */
#define	SRV_TIMEOUT		30

#define ZAUTH (ZMakeAuthentication)
#define ZNOAUTH ((Z_AuthProc)0)

/* Packet strings */
#define ZSRVACK_SENT		"SENT"	/* SERVACK codes */
#define ZSRVACK_NOTSENT		"LOST"
#define ZSRVACK_FAIL		"FAIL"

/* Server internal class */
#define ZEPHYR_ADMIN_CLASS	"ZEPHYR_ADMIN"	/* Class */

/* Control codes sent to a server */
#define ZEPHYR_CTL_CLASS	"ZEPHYR_CTL"	/* Class */

#define ZEPHYR_CTL_CLIENT	"CLIENT"	/* Inst: From client */
#define CLIENT_SUBSCRIBE	"SUBSCRIBE"	/* Opcode: Subscribe */
#define CLIENT_SUBSCRIBE_NODEFS	"SUBSCRIBE_NODEFS"	/* Opcode: Subscribe */
#define CLIENT_UNSUBSCRIBE	"UNSUBSCRIBE"	/* Opcode: Unsubsubscribe */
#define CLIENT_CANCELSUB	"CLEARSUB"	/* Opcode: Clear all subs */
#define CLIENT_GIMMESUBS	"GIMME"		/* Opcode: Give me subs */
#define	CLIENT_GIMMEDEFS	"GIMMEDEFS"	/* Opcode: Give me default
						 * subscriptions */

#define ZEPHYR_CTL_HM		"HM"		/* Inst: From HM */
#define HM_BOOT			"BOOT"		/* Opcode: Boot msg */
#define HM_FLUSH		"FLUSH"		/* Opcode: Flush me */
#define HM_DETACH		"DETACH"	/* Opcode: Detach me */
#define HM_ATTACH		"ATTACH"	/* Opcode: Attach me */

/* Control codes send to a HostManager */
#define	HM_CTL_CLASS		"HM_CTL"	/* Class */

#define HM_CTL_SERVER		"SERVER"	/* Inst: From server */
#define SERVER_SHUTDOWN		"SHUTDOWN"	/* Opcode: Server shutdown */
#define SERVER_PING		"PING"		/* Opcode: PING */

#define HM_CTL_CLIENT           "CLIENT"        /* Inst: From client */
#define CLIENT_FLUSH            "FLUSH"         /* Opcode: Send flush to srv */
#define CLIENT_NEW_SERVER       "NEWSERV"       /* Opcode: Find new server */

/* HM Statistics */
#define HM_STAT_CLASS		"HM_STAT"	/* Class */

#define HM_STAT_CLIENT		"HMST_CLIENT"	/* Inst: From client */
#define HM_GIMMESTATS		"GIMMESTATS"	/* Opcode: get stats */

/* Login class messages */
#define LOGIN_CLASS		"LOGIN"		/* Class */

/* Class Instance is principal of user who is logging in or logging out */

#define EXPOSE_NONE		"NONE"		/* Opcode: Not visible */
#define EXPOSE_OPSTAFF		"OPSTAFF"	/* Opcode: Opstaff visible */
#define EXPOSE_REALMVIS		"REALM-VISIBLE"	/* Opcode: Realm visible */
#define EXPOSE_REALMANN		"REALM-ANNOUNCED"/* Opcode: Realm announced */
#define EXPOSE_NETVIS		"NET-VISIBLE"	/* Opcode: Net visible */
#define EXPOSE_NETANN		"NET-ANNOUNCED"	/* Opcode: Net announced */
#define	LOGIN_USER_LOGIN	"USER_LOGIN"	/* Opcode: user login
						   (from server) */
#define LOGIN_USER_LOGOUT	"USER_LOGOUT"	/* Opcode: User logout */
#define	LOGIN_USER_FLUSH	"USER_FLUSH"	/* Opcode: flush all locs */

/* Locate class messages */
#define LOCATE_CLASS		"USER_LOCATE"	/* Class */

#define LOCATE_HIDE		"USER_HIDE"	/* Opcode: Hide me */
#define LOCATE_UNHIDE		"USER_UNHIDE"	/* Opcode: Unhide me */

/* Class Instance is principal of user to locate */
#define LOCATE_LOCATE		"LOCATE"	/* Opcode: Locate user */

/* WG_CTL class messages */
#define WG_CTL_CLASS		"WG_CTL"	/* Class */

#define WG_CTL_USER		"USER"		/* Inst: User request */
#define USER_REREAD		"REREAD"	/* Opcode: Reread desc file */
#define USER_SHUTDOWN		"SHUTDOWN"	/* Opcode: Go catatonic */
#define USER_STARTUP		"STARTUP"	/* Opcode: Come out of it */
#define USER_EXIT		"EXIT"		/* Opcode: Exit the client */

#endif /* PURPLE_ZEPHYR_ZEPHYR_INTERNAL_H */
