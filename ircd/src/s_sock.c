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
#include <sys/types.h>
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

/* Send op notices for cached check results ? */
#undef SHOW_CACHED_SOCKS 


#ifdef ENABLE_SOCKSCHECK

#if 0
#define SOCKS_DEBUG
#endif

aSocks *socks_list;
void ApplySocksFound(aClient *);
void ApplySocksConn(aClient *);
void ApplySocks(aSocks *);
void remFromSocks(aSocks *, aSocks **);



void remFromSocks(aSocks *sock, aSocks **addto)
{
   aSocks *s, *snext, *tmp = NULL;
   for (s = socks_list; s; s = snext) 
   {
        snext = s->next;
        if (s == sock)
	{
	    if (tmp)   tmp->next = snext;
            else       socks_list = snext;
	    if (addto)
	    {
		sock->next = *addto;
		*addto = sock;
	    }
	}
        else tmp = s;
   }

}

time_t flush_socks(time_t now, int fAll)
{
   aSocks *flushlist = NULL, *socks, *snext;
   aClient *cptr;
   int i;

   update_time();
   for (socks = socks_list; socks; socks = snext)
   {
	snext = socks->next;
        if (!(socks->status & SOCK_DONE))
            continue;
	socks->status &= ~(SOCK_DESTROY);
	if ((fAll>1) || (fAll && (socks->status & SOCK_DONE|SOCK_FOUND|SOCK_DESTROY)))
	    socks->status |= SOCK_DESTROY;
        if (!(socks->status & SOCK_FOUND) && ((NOW - socks->start) > 3))
            socks->status |= SOCK_DESTROY;

        /* The 'real' timeout condition is in s_bsd.c */
        if (((NOW - socks->start) > (SOCKS_TIMEOUT*3)) &&
            ((NOW - socks->start) > 60)) {
            socks->status |= SOCK_DESTROY;
        }
	if (socks->status & SOCK_DESTROY)
	    remFromSocks(socks, &flushlist);
   }
   for (i = highest_fd; i >= 0; i--)
   {
	if (!(cptr = local[i]) || !cptr->socks)
	    continue;
	for (socks = flushlist; socks; socks = socks->next)
	     if (cptr->socks == socks)
		 cptr->socks = NULL;
    }
   for (socks = flushlist; socks; socks = snext)
   {
	snext = socks->next;
        if (socks->status & SOCK_DESTROY)
	{
	    if (socks->fd >= 0)
                closesocket(socks->fd);
	    socks->fd = -1;
	    free(socks);
	}
   }
   return (now + 5);
}

aSocks *get_socks(struct in_addr addy, int new)
{
    aSocks *tmpsocks;

    for (tmpsocks = socks_list; tmpsocks; tmpsocks = tmpsocks->next)
    {
         if (tmpsocks->in_addr.s_addr == addy.s_addr)
	{
	     tmpsocks->status &= ~(SOCK_NEW);
	     return tmpsocks;
	}
    }

    update_time();
    if (!new)
        return NULL;
    tmpsocks = (aSocks *)MyMalloc(sizeof(aSocks));
    memset(tmpsocks, 0, sizeof(aSocks));
    tmpsocks->status = SOCK_NEW;
    tmpsocks->fd = -1;
    tmpsocks->in_addr = addy;
    tmpsocks->start = NOW;
    tmpsocks->next = socks_list;
    socks_list = tmpsocks;
    return tmpsocks;
}

void ApplySock(aSocks *socks)
{
    int i = 0;
    aClient *cptr;

    for (i = highest_fd; i >= 0; i--)
    {
         if (!(cptr = local[i]) || cptr->socks != socks)
             continue;
	 if (!IS_SET(ClientFlags(cptr), FLAGS_SOCK))
	     continue;
	 if (IsRegistered(cptr) || cptr->user || cptr->serv)
	     continue;
	 if (IS_SET(socks->status, SOCK_FOUND))
	 {
	     ApplySocksFound(cptr);
	     continue;
	 }
	 if (!IS_SET(socks->status, SOCK_DONE))
	     continue;
	if (!DoingDNS(cptr))
		SetAccess(cptr);
         if ((socks->status & (SOCK_DONE|SOCK_REFUSED)) == (SOCK_DONE|SOCK_REFUSED))
	     connotice(cptr, REPORT_OK_SOCKS);
         else if ((socks->status & (SOCK_DONE|SOCK_ERROR)) == (SOCK_DONE|SOCK_ERROR))
	     connotice(cptr, REPORT_ERR_SOCKS);
         else if ((socks->status & (SOCK_DONE)) == (SOCK_DONE))
	     connotice(cptr, REPORT_FAIL_SOCKS);
    }

}

void ApplySocksFound(aClient *cptr)
{
	sendto_one(cptr, ":%s NOTICE %s :*** Open socks/proxy server detected, IRC session aborted.", me.name, "AUTH");
	sendto_one(cptr, ":%s NOTICE AUTH :*** %s contains an open socks/proxy server, you may not IRC from this host until the server is secured", me.name, get_client_name3(cptr, 0));
	sendto_one(cptr, ":%s NOTICE AUTH :*** See %s for information about why you are not allowed to connect", me.name, SOCKSFOUND_URL);
#ifndef SHOW_CACHED_SOCKS
	if (!cptr->socks || !(cptr->socks->status & SOCK_CACHED))
#endif
		sendto_umode_norep(FLAGSET_SOCKS, 3, "*** Notice -- Open socks server from %s%s", get_client_name(cptr, FALSE), cptr->socks && (cptr->socks->status & SOCK_CACHED) ? " (cached)" : "");
	SetHurt(cptr);
	cptr->hurt = 2;
	cptr->lasttime = 0;
	SET_BIT(ClientFlags(cptr), FLAGS_PINGSENT);
	REMOVE_BIT(ClientFlags(cptr), FLAGS_SOCKS);
	cptr->socks = NULL;
	exit_client(cptr, cptr, &me, "Open proxy/socks server detected");
}

void ApplySockConn(aClient *cptr)
{
	if (!cptr->socks)
	    return;
	if (!DoingDNS(cptr))
		SetAccess(cptr);

	if ((cptr->socks->status & (SOCK_GO|SOCK_FOUND)) == (SOCK_GO|SOCK_FOUND))
	{
		if (cptr != &me)
		{
			if (!(cptr->socks->status & SOCK_NEW) && cptr->socks->fd == -1)
				connotice(cptr, REPORT_FIN_SOCKS " (cached)");
			else
				connotice(cptr, REPORT_FIN_SOCKS);
		}
		ApplySocksFound(cptr);
		cptr->socks = NULL;
		return;
	}
	if ((cptr->socks->status & SOCK_GO))
	{
		ClientFlags(cptr) &= ~FLAGS_SOCK;
		if (!(cptr->socks->status & SOCK_NEW) && (cptr->socks->fd == -1))
			connotice(cptr, REPORT_FAIL_SOCKS " (cached)");
		else
			connotice(cptr, REPORT_FAIL_SOCKS);
		cptr->socks = NULL;
		return;
	}
}

/*
 * request a sockets connection 
 * if we can't create it we let the user connect...
 */
void init_socks(aClient *cptr)
{
	struct sockaddr_in sin, sock;
	int addrlen = sizeof (struct sockaddr_in);

	Debug((DEBUG_ERROR, "Starting socks check for cptr %p, cptr->fd %d\n",
	       cptr, cptr->fd));
	if (cptr->socks == NULL)
	    cptr->socks = get_socks(cptr->ip, 1);
	if (cptr->socks->fd == -1) /* !New doesn't necessarilly mean cached */
	    cptr->socks->status |= SOCK_CACHED;

	if ((cptr->socks->status & SOCK_NEW) && !(cptr->socks->status & SOCK_DONE)) 
	{
	    cptr->socks->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#if 0
		/* we can't use all the fds! */
		if (cptr->socks->fd >= (MAXCLIENTS + 3))
		{
			sendto_realops("Warning: out of client fds on socks check %s",
				       get_client_name(cptr, TRUE));
			close(cptr->socks->fd);
			cptr->socks->fd = -1;
			cptr->socks->status |= (SOCK_GO | SOCK_DESTROY);
			if (!DoingDNS(cptr))
				SetAccess(cptr);
			return;
		}
#endif

		if (cptr->socks->fd < 0)
		{
		    cptr->socks->status = (SOCK_DONE | SOCK_DESTROY);
		    if (!DoingDNS(cptr))
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
			    sizeof(struct sockaddr_in)) < 0)
		{

			if (errno != EAGAIN
			    && errno != EWOULDBLOCK
			    && errno != EINPROGRESS)
			{
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
				cptr->socks->status |= (SOCK_DONE);

				if (!DoingDNS(cptr))
				SetAccess(cptr);

				if (cptr != &me)
					connotice(cptr, REPORT_OK_SOCKS);
				return;
			}
		}
	cptr->socks->status = SOCK_WANTCON;
	}

	ClientFlags(cptr) |= FLAGS_SOCK;
	if (cptr != &me)
		connotice(cptr, REPORT_START_SOCKS);
	ApplySockConn(cptr);
}

/*
 * send_socksquery
 * send the actual request to the socks server
 */
void send_socksquery (aSocks *sItem)
{
	int rv;
	unsigned char sbuf[12];
	u_short port;
	struct sockaddr_in sin, si;
	int socklen;

	assert(sItem);

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
	rv = send(sItem->fd, sbuf, 9, 0);
	if (rv != 9) {
		/*
		 * Assume any error at this point is good.
		 */
		closesocket(sItem->fd);

		if (sItem->fd == highest_fd)
			while (!local[highest_fd])
				highest_fd--;
		sItem->status |= (SOCK_DONE);
		if ((rv < 0) && (errno == ETIMEDOUT || errno == ENETUNREACH))
		    sItem->status |= (SOCK_ERROR);
		else if (rv >= 0)
		    sItem->status |= (SOCK_REFUSED);
		sItem->fd = -1;
		ApplySock(sItem);
		return;
	}

	sItem->status |= SOCK_SENT;
	return;
}

/*
 *     read data from socks  
 *
 *     allow them in if it doesnt seem to be an _open_ socks server...
 *     allow them in if connection refused
 *     block connection if open socks found
 */
void read_socks(aSocks *sItem)
{
	unsigned char sbuf[9];
	int len, erv, i;

	assert(sItem);

	len = recv(sItem->fd, sbuf, 9, 0);
	erv = errno;

	Debug((DEBUG_ERROR,
	       "Socks: recv %d (%02x %02x %02x %02x %02x %02x %02x %02x %02x)",
	       len, sbuf[0], sbuf[1], sbuf[2], sbuf[3], sbuf[4],
	       sbuf[5], sbuf[6], sbuf[7], sbuf[8]));
	    
	(void)closesocket(sItem->fd);
	if (sItem->fd == highest_fd)
		while (!local[highest_fd])
			highest_fd--;

	sItem->fd = -1;

	if (len < 0) {
		if (erv != EAGAIN)
			sItem->status |= (SOCK_GO);

        for(i = 0; i < highest_fd; i++)
            if (local[i] && !IsLog(local[i]) && local[i]->socks == sItem)
                ApplySockConn(local[i]);
	return;

	}

	/*
	 * return code of 90 means the connection was granted.
	 */
	if ((len >= 2) && (sbuf[1] == 90)) {
		sItem->status |= (SOCK_FOUND | SOCK_GO);
                for(i = 0; i < highest_fd; i++)
		   if (local[i] && !IsLog(local[i]) && local[i]->socks == sItem)
			ApplySockConn(local[i]);
		return;
	}

	/*
	 * The request failed.  Good.
	 */
	sItem->status |= (SOCK_GO);
	/*if (!DoingDNS(cptr))
		SetAccess(cptr);
	if (cptr != &me)
		connotice(cptr, REPORT_OK_SOCKS);*/
	return;
}

#endif /* ENABLE_SOCKSCHECK */
