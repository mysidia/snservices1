/*
 *   IRC - Internet Relay Chat, ircd/s_misc.c (formerly ircd/date.c)
 *   Copyright (C) 1998 Mysidia
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef lint
static char rcsid[] = "$Id$";
#endif

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
#include "numeric.h"
#include "patchlevel.h"
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef  UNIXPORT
#include <sys/un.h>
#endif
#endif
#include <fcntl.h>
#include "sock.h"
#include "h.h"

#ifdef ENABLE_SOCKSCHECK

#if 0
#define SOCKS_DEBUG
#endif

/*
 * request a sockets connection 
 * if we can't create it we let the user connect...
 */
void
init_socks(aClient *cptr)
{
	struct sockaddr_in sin, sock;
	int addrlen = sizeof (struct sockaddr_in);

	Debug((DEBUG_ERROR, "Starting socks check for cptr %p, cptr->fd %d\n",
	       cptr, cptr->fd));

	if (cptr->socks == NULL) {
		cptr->socks = (aSocks *)MyMalloc(sizeof(aSocks));
		bzero(cptr->socks, sizeof(aSocks));
		cptr->socks->fd = -1;
	}
	cptr->socks->status = 0;
	cptr->socks->start = NOW;
	cptr->socks->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#if 0
	/* we can't use all the fds! */
	if (cptr->socks->fd >= (MAXCLIENTS + 3)) {
		sendto_realops("Warning: out of client fds on socks check %s",
			       get_client_name(cptr, TRUE));
		close(cptr->socks->fd);
		cptr->socks->fd = -1;
		cptr->socks->status |= (SOCK_GO | SOCK_DESTROY);
		if (!DoingDNS(cptr) && !DoingAuth(cptr))
			SetAccess(cptr);
		return;
	}
#endif

	if (cptr->socks->fd < 0) {
		cptr->socks->status = (SOCK_DONE | SOCK_DESTROY);
		if (!DoingDNS(cptr) && !DoingAuth(cptr))
			SetAccess(cptr);
		sendto_realops("[***] error in init_socks() while making socket for %s",
			       get_client_name(cptr, TRUE));
		return;
	}

	if (cptr->socks->fd > highest_fd)
		highest_fd = cptr->socks->fd;

	set_non_blocking(cptr->socks->fd, cptr);
	set_sock_opts(cptr->fd, cptr);

	/*
	 * get the address the client used to connect, and make the connection
	 * back to the socks port using the same local address.
	 */
	getsockname(cptr->fd, (struct sockaddr *)&sock, &addrlen);
	sock.sin_port = 0;
	sock.sin_family = AF_INET;
	(void)bind(cptr->socks->fd, (struct sockaddr *)&sock, sizeof sock);

	/*
	 * copy the client's address, and do the connect call.
	 */
	bcopy(&cptr->ip, &sock.sin_addr, sizeof(struct in_addr));
	sock.sin_port = htons(SOCKSPORT);
	sock.sin_family = AF_INET;
	if (connect(cptr->socks->fd, (struct sockaddr *)&sock,
		    sizeof(struct sockaddr_in)) < 0) {

		if (errno != EAGAIN
		    && errno != EWOULDBLOCK
		    && errno != EINPROGRESS) {
			/*
			 * Assume all errors at this point are good, and let
			 * them in.
			 */
#if 0
			sendto_ops("error during socks check of %s connect(): %s",
				   get_client_name(cptr, TRUE),
				   strerror(errno));
#endif
			cptr->socks->fd = -1;
			cptr->socks->status |= (SOCK_DONE | SOCK_DESTROY);

			if (!DoingDNS(cptr) && !DoingAuth(cptr))
				SetAccess(cptr);

			if (cptr != &me)
				connotice(cptr, REPORT_OK_SOCKS);

			return;
		}
	}

	cptr->socks->status = SOCK_WANTCON;
	ClientFlags(cptr) |= FLAGS_SOCK;
	addrlen = sizeof(struct sockaddr_in);
	if (getpeername(cptr->socks->fd, (struct sockaddr *)&sin,
			&addrlen) != -1) {
		cptr->socks->status |= SOCK_CONNECTED;
		send_socksquery(cptr);
	}
	if (cptr != &me)
		connotice(cptr, REPORT_START_SOCKS);
}

/*
 * send_socksquery
 * send the actual request to the socks server
 */
void send_socksquery (cptr)
     aClient *cptr;
{
	int rv;
	unsigned char sbuf[12];
	u_short port;
	struct sockaddr_in sin, si;
	int socklen;

	assert(cptr->socks);

	/*
	 * Someone needs to sanity check this...
	 */
	port = htons(CHECKPORT);
	sbuf[0] = 4;
	sbuf[1] = 1;
	memcpy(sbuf + 2, &port, 2);
	memcpy(sbuf + 4, &me.ip.s_addr, 4);
	sbuf[8] = 0;
	sbuf[9] = 0;

	Debug((DEBUG_ERROR,
	       "Socks: sending (%02x %02x %02x %02x %02x %02x %02x %02x %02x)",
	       sbuf[0], sbuf[1], sbuf[2], sbuf[3], sbuf[4],
	       sbuf[5], sbuf[6], sbuf[7], sbuf[8]));
	    
	/*
	 * Try to send.  If we get an error here (like not connected, or
	 * whatever) detect that and assume the connect() call failed.
	 */
	rv = send(cptr->socks->fd, sbuf, 9, 0);
	if (rv != 9) {
		/*
		 * Assume any error at this point is good.
		 */
#ifdef SOCKS_DEBUG
		sendto_one(cptr, ":%s NOTICE AUTH :-- %d %s",
			   me.name, rv, strerror(errno));
#endif
		closesocket(cptr->socks->fd);

		if (cptr->socks->fd == highest_fd)
			while (!local[highest_fd])
				highest_fd--;

		cptr->socks->status |= (SOCK_ERROR | SOCK_DESTROY | SOCK_DONE);
		cptr->socks->fd = -1;
	}

	cptr->socks->status |= SOCK_SENT;
	return;
}

/*
 *     read data from socks  
 *
 *     allow them in if it doesnt seem to be an _open_ socks server...
 *     allow them in if connection refused
 *     block connection if open socks found
 */
void
read_socks(aClient *cptr)
{
	unsigned char sbuf[9];
	int len, erv;

	assert(cptr->socks);

	len = recv(cptr->socks->fd, sbuf, 9, 0);
	erv = errno;

	Debug((DEBUG_ERROR,
	       "Socks: recv %d (%02x %02x %02x %02x %02x %02x %02x %02x %02x)",
	       len, sbuf[0], sbuf[1], sbuf[2], sbuf[3], sbuf[4],
	       sbuf[5], sbuf[6], sbuf[7], sbuf[8]));
	    
	(void)closesocket(cptr->socks->fd);
	if (cptr->socks->fd == highest_fd)
		while (!local[highest_fd])
			highest_fd--;

	cptr->socks->fd = -1;

	if (len < 0) {
		if (erv != EAGAIN) {
			cptr->socks->status |= (SOCK_GO | SOCK_DESTROY);
			if (!DoingDNS(cptr) && !DoingAuth(cptr))
				SetAccess(cptr);
		}

#ifdef SOCKS_DEBUG
		if (cptr != &me)
			sendto_one(cptr,
				   ":%s NOTICE AUTH :" REPORT_FAIL_SOCKS
				   " recv(%d): %s",
				   me.name, erv, strerror (erv));
#else
		if (cptr != &me)
			connotice(cptr, REPORT_FAIL_SOCKS);
#endif
		return;
	}

	/*
	 * return code of 90 means the connection was granted.
	 */
	if ((len >= 2) && (sbuf[1] == 90)) {
		if (cptr != &me)
			connotice(cptr, REPORT_FIN_SOCKS);
		cptr->socks->status |= (SOCK_FOUND | SOCK_GO);
		if (!DoingDNS(cptr) && !DoingAuth(cptr))
			SetAccess(cptr);

		return;
	}

	/*
	 * The request failed.  Good.
	 */
	cptr->socks->status |= (SOCK_GO | SOCK_DESTROY);
	if (!DoingDNS(cptr) && !DoingAuth(cptr))
		SetAccess(cptr);
	if (cptr != &me)
		connotice(cptr, REPORT_OK_SOCKS);

	return;
}

#endif /* ENABLE_SOCKSCHECK */
