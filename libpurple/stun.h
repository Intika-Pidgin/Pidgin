/**
 * @file stun.h STUN API
 * @ingroup core
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
#ifndef _GAIM_STUN_H_
#define _GAIM_STUN_H_

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name STUN API                                                        */
/**************************************************************************/
/*@{*/

typedef struct _GaimStunNatDiscovery GaimStunNatDiscovery;

typedef enum {
	GAIM_STUN_STATUS_UNDISCOVERED = -1,
	GAIM_STUN_STATUS_UNKNOWN, /* no STUN server reachable */
	GAIM_STUN_STATUS_DISCOVERING,
	GAIM_STUN_STATUS_DISCOVERED
} GaimStunStatus;

typedef enum {
	GAIM_STUN_NAT_TYPE_PUBLIC_IP,
	GAIM_STUN_NAT_TYPE_UNKNOWN_NAT,
	GAIM_STUN_NAT_TYPE_FULL_CONE,
	GAIM_STUN_NAT_TYPE_RESTRICTED_CONE,
	GAIM_STUN_NAT_TYPE_PORT_RESTRICTED_CONE,
	GAIM_STUN_NAT_TYPE_SYMMETRIC
} GaimStunNatType;

struct _GaimStunNatDiscovery {
	GaimStunStatus status;
	GaimStunNatType type;
	char publicip[16];
	char *servername;
	time_t lookup_time;
};

typedef void (*StunCallback) (GaimStunNatDiscovery *);

/**
 * Starts a NAT discovery. It returns a GaimStunNatDiscovery if the discovery
 * is already done. Otherwise the callback is called when the discovery is over
 * and NULL is returned.
 *
 * @param cb The callback to call when the STUN discovery is finished if the
 *           discovery would block.  If the discovery is done, this is NOT
 *           called.
 *
 * @return a GaimStunNatDiscovery which includes the public IP and the type
 *         of NAT or NULL is discovery would block
 */
GaimStunNatDiscovery *gaim_stun_discover(StunCallback cb);

void gaim_stun_init(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_STUN_H_ */
