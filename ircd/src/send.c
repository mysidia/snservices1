/************************************************************************
 *   IRC - Internet Relay Chat, common/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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

/* -- Jto -- 16 Jun 1990
 * Added Armin's PRIVMSG patches...
 */

#ifndef lint
static  char sccsid[] = "@(#)send.c	2.32 2/28/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#endif

#ifdef	IRCII_KLUDGE
#define	NEWLINE	"\n"
#else
#define NEWLINE	"\r\n"
#endif

static int recurse_send = 0;
static char last_pattern[2048*2]="";

static	char	sendbuf[2048];
static	int	send_message PROTO((aClient *, char *, int));

static	int	sentalong[MAXCONNECTIONS];

/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	If 'notice' is not NULL, it is assumed to be a format
**	for a message to local opers. I can contain only one
**	'%s', which will be replaced by the sockhost field of
**	the failing link.
**
**	Also, the notice is skipped for "uninteresting" cases,
**	like Persons and yet unknown connections...
*/
static	int	dead_link(to, notice)
aClient *to;
char	*notice;
{
	ClientFlags(to) |= FLAGS_DEADSOCKET;
	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
	if (!IsPerson(to) && !IsUnknown(to) && !(ClientFlags(to) & FLAGS_CLOSING))
		sendto_ops(notice, get_client_name(to, FALSE));
	Debug((DEBUG_ERROR, notice, get_client_name(to, FALSE)));
	return -1;
}

/*
** flush_connections
**	Used to empty all output buffers for all connections. Should only
**	be called once per scan of connections. There should be a select in
**	here perhaps but that means either forcing a timeout or doing a poll.
**	When flushing, all we do is empty the obuffer array for each local
**	client and try to send it. if we cant send it, it goes into the sendQ
**	-avalon
*/
void	flush_connections(fd)
int	fd;
{
#ifdef SENDQ_ALWAYS
	int	i;
	aClient *cptr;

	if (fd == me.fd)
	    {
		for (i = highest_fd; i >= 0; i--)
			if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
				(void)send_queued(cptr);
	    }
	else if (fd >= 0 && (cptr = local[fd]) && DBufLength(&cptr->sendQ) > 0)
		(void)send_queued(cptr);
#endif
}

/*
** send_message
**	Internal utility which delivers one message buffer to the
**	socket. Takes care of the error handling and buffering, if
**	needed.
*/
static	int	send_message(to, msg, len)
aClient	*to;
char	*msg;	/* if msg is a null pointer, we are flushing connection */
int	len;
#ifdef SENDQ_ALWAYS
{
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */
	if (DBufLength(&to->sendQ) > get_sendq(to))
	    {
		if (IsServer(to))
			sendto_ops("Max SendQ limit exceeded for %s: %d > %d",
			   	get_client_name(to, FALSE),
				DBufLength(&to->sendQ), get_sendq(to));
		return dead_link(to, "Max Sendq exceeded");
	    }
	else if (dbuf_put(&to->sendQ, msg, len) < 0)
		return dead_link(to, "Buffer allocation error for %s");
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt && to->acpt != &me)
		to->acpt->sendM += 1;
	/*
	** This little bit is to stop the sendQ from growing too large when
	** there is no need for it to. Thus we call send_queued() every time
	** 2k has been added to the queue since the last non-fatal write.
	** Also stops us from deliberately building a large sendQ and then
	** trying to flood that link with data (possible during the net
	** relinking done by servers with a large load).
	*/
	if (DBufLength(&to->sendQ)/1024 > to->lastsq)
		send_queued(to);
	return 0;
}
#else
{
	int	rlen = 0;

	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */

	/*
	** DeliverIt can be called only if SendQ is empty...
	*/
	if ((DBufLength(&to->sendQ) == 0) &&
	    (rlen = deliver_it(to, msg, len)) < 0)
		return dead_link(to,"Write error to %s, closing link");
	else if (rlen < len)
	    {
		/*
		** Was unable to transfer all of the requested data. Queue
		** up the remainder for some later time...
		*/
		if (DBufLength(&to->sendQ) > get_sendq(to))
		    {
			sendto_ops("Max SendQ limit exceeded for %s : %d > %d",
				   get_client_name(to, FALSE),
				   DBufLength(&to->sendQ), get_sendq(to));
			return dead_link(to, "Max Sendq exceeded");
		    }
		else if (dbuf_put(&to->sendQ,msg+rlen,len-rlen) < 0)
			return dead_link(to,"Buffer allocation error for %s");
	    }
	/*
	** Update statistics. The following is slightly incorrect
	** because it counts messages even if queued, but bytes
	** only really sent. Queued bytes get updated in SendQueued.
	*/
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt && to->acpt != &me)
		to->acpt->sendM += 1;
	return 0;
}
#endif

/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int	send_queued(to)
aClient *to;
{
	char	*msg;
	int	len, rlen;

	/*
	** Once socket is marked dead, we cannot start writing to it,
	** even if the error is removed...
	*/
	if (IsDead(to))
	    {
		/*
		** Actually, we should *NEVER* get here--something is
		** not working correct if send_queued is called for a
		** dead socket... --msa
		*/
#ifndef SENDQ_ALWAYS
		return dead_link(to, "send_queued called for a DEADSOCKET:%s");
#else
		return -1;
#endif
	    }
	while (DBufLength(&to->sendQ) > 0)
	    {
		msg = dbuf_map(&to->sendQ, &len);
					/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
			return dead_link(to,"Write error to %s, closing link");
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ)/1024;
		if (rlen < len) /* ..or should I continue until rlen==0? */
			break;
	    }

	return (IsDead(to)) ? -1 : 0;
}

/*
** send message to single client
*/
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_one(to, pattern, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)
aClient *to;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11;
{
#else
void	sendto_one(to, pattern, va_alist)
aClient	*to;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif

/*
# ifdef NPATH
        check_command((long)1, pattern, p1, p2, p3);
# endif
*/

#ifdef	USE_VARARGS
	va_start(vl);
	(void)vsprintf(sendbuf, pattern, vl);
	va_end(vl);
#else
	(void)sprintf(sendbuf, pattern, p1, p2, p3, p4, p5, p6,
		p7, p8, p9, p10, p11);
#endif
	Debug((DEBUG_SEND,"Sending [%s] to %s", sendbuf, to->name));

	if (to->from)
		to = to->from;
	if (to->fd < 0) {
		assert(to->fd >= 0);
		Debug((DEBUG_ERROR,
		      "Local socket %s with negative fd... AARGH!",
		      to->name));
	    }
	else if (IsMe(to)) {
		sendto_ops("Trying to send [%s] to myself!", sendbuf);
		return;
	    }
	(void)strcat(sendbuf, NEWLINE);
#ifndef	IRCII_KLUDGE
	sendbuf[510] = '\r';
#endif
	sendbuf[511] = '\n';
	sendbuf[512] = '\0';
	(void)send_message(to, sendbuf, strlen(sendbuf));
}

# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_channel_butone(one, from, chptr, pattern,
			      p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
aChannel *chptr;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_channel_butone(one, from, chptr, pattern, va_alist)
aClient	*one, *from;
aChannel *chptr;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	Link	*lp;
	aClient *acptr;
	int	i;

# ifdef	USE_VARARGS
	va_start(vl);
# endif
	bzero(sentalong, sizeof(sentalong));
	for (lp = chptr->members; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one || (lp->flags & CHFL_ZOMBIE))
			continue;	/* ...was the one I should skip */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
# ifdef	USE_VARARGS
			sendto_prefix_one(acptr, from, pattern, vl);
# else
			sendto_prefix_one(acptr, from, pattern, p1, p2,
					  p3, p4, p5, p6, p7, p8);
# endif
			sentalong[i] = 1;
		    }
		else
		    {
		/* Now check whether a message has been sent to this
		 * remote link already */
			if (sentalong[i] == 0)
			    {
# ifdef	USE_VARARGS
	  			sendto_prefix_one(acptr, from, pattern, vl);
# else
	  			sendto_prefix_one(acptr, from, pattern,
						  p1, p2, p3, p4,
						  p5, p6, p7, p8);
# endif
				sentalong[i] = 1;
			    }
		    }
	    }
# ifdef	USE_VARARGS
	va_end(vl);
# endif
	return;
}

/*
 * sendto_channelops_butone Added 1 Sep 1996 by Cabal95.
 *   Send a message to all OPs in channel chptr that
 *   are directly on this server and sends the message
 *   on to the next server if it has any OPs.
 *
 *   All servers must have this functional ability
 *    or one without will send back an error message. -- Cabal95
 */
# ifndef        USE_VARARGS
/*VARARGS*/
void    sendto_channelops_butone(one, from, chptr, pattern,
			      p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from; 
aChannel *chptr;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_channelops_butone(one, from, chptr, pattern, va_alist)
aClient *one, *from;
aChannel *chptr;
char	*pattern;
va_dcl
{ 
	va_list vl;
# endif
	Link	*lp;
	aClient	*acptr;
	int	i;

# ifdef USE_VARARGS
	va_start(vl);
# endif
	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one || (lp->flags & CHFL_ZOMBIE) ||
		    !(lp->flags & CHFL_CHANOP))
			continue;       /* ...was the one I should skip
                                           or user not not a channel op */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
# ifdef USE_VARARGS
			sendto_prefix_one(acptr, from, pattern, vl);
# else 
			sendto_prefix_one(acptr, from, pattern, p1, p2,
                                          p3, p4, p5, p6, p7, p8);
# endif
			sentalong[i] = 1;
		    }
		else
                    {
		/* Now check whether a message has been sent to this
		 * remote link already */
			if (sentalong[i] == 0) 
			    {
# ifdef USE_VARARGS
				sendto_prefix_one(acptr, from, pattern, vl);
# else
				sendto_prefix_one(acptr, from, pattern,
						  p1, p2, p3, p4,
						  p5, p6, p7, p8);  
# endif
				sentalong[i] = 1;
			    }
		    }
	    }
# ifdef USE_VARARGS  
	va_end(vl); 
# endif
	return;
}



# ifndef        USE_VARARGS
/*VARARGS*/
void    sendto_channelvoices_butone(one, from, chptr, pattern,
			      p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from; 
aChannel *chptr;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_channelvoices_butone(one, from, chptr, pattern, va_alist)
aClient *one, *from;
aChannel *chptr;
char	*pattern;
va_dcl
{ 
	va_list vl;
# endif
	Link	*lp;
	aClient	*acptr;
	int	i;

# ifdef USE_VARARGS
	va_start(vl);
# endif
	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next)
	    {
		acptr = lp->value.cptr;
		if (acptr->from == one || (lp->flags & CHFL_ZOMBIE) ||
		    ( !(lp->flags & CHFL_VOICE) && !(lp->flags & CHFL_CHANOP) ))
			continue;       /* ...was the one I should skip
                                           or user not not a channel op */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		    {
# ifdef USE_VARARGS
			sendto_prefix_one(acptr, from, pattern, vl);
# else 
			sendto_prefix_one(acptr, from, pattern, p1, p2,
                                          p3, p4, p5, p6, p7, p8);
# endif
			sentalong[i] = 1;
		    }
		else
                    {
		/* Now check whether a message has been sent to this
		 * remote link already */
			if (sentalong[i] == 0) 
			    {
# ifdef USE_VARARGS
				sendto_prefix_one(acptr, from, pattern, vl);
# else
				sendto_prefix_one(acptr, from, pattern,
						  p1, p2, p3, p4,
						  p5, p6, p7, p8);  
# endif
				sentalong[i] = 1;
			    }
		    }
	    }
# ifdef USE_VARARGS  
	va_end(vl); 
# endif
	return;
}


/*
 * sendto_server_butone
 *
 * Send a message to all connected servers except the client 'one'.
 */
# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_serv_butone(one, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_serv_butone(one, pattern, va_alist)
aClient	*one;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	int	i;
	aClient *cptr;

# ifdef	USE_VARARGS
	va_start(vl);
# endif

# ifdef NPATH
        check_command((long)2, pattern, p1, p2, p3);
# endif

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		if (IsServer(cptr))
# ifdef	USE_VARARGS
			sendto_one(cptr, pattern, vl);
	    }
	va_end(vl);
# else
			sendto_one(cptr, pattern, p1, p2, p3, p4,
				   p5, p6, p7, p8);
	    }
# endif
	return;
}

/*
 * sendto_common_channels()
 *
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
# ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_common_channels(user, pattern, p1, p2, p3, p4,
				p5, p6, p7, p8)
aClient *user;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
# else
void	sendto_common_channels(user, pattern, va_alist)
aClient	*user;
char	*pattern;
va_dcl
{
	va_list	vl;
# endif
	int	i;
	aClient *cptr;
	Link	*lp;

# ifdef	USE_VARARGS
	va_start(vl);
# endif
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]) || IsServer(cptr) ||
		    user == cptr || !user->user)
			continue;
		for (lp = user->user->channel; lp; lp = lp->next)
			if (IsMember(user, lp->value.chptr) &&
			    IsMember(cptr, lp->value.chptr))
			    {
# ifdef	USE_VARARGS
				sendto_prefix_one(cptr, user, pattern, vl);
# else
				sendto_prefix_one(cptr, user, pattern,
						  p1, p2, p3, p4,
						  p5, p6, p7, p8);
# endif
				break;
			    }
	    }
	if (MyConnect(user))
# ifdef	USE_VARARGS
		sendto_prefix_one(user, user, pattern, vl);
	va_end(vl);
# else
		sendto_prefix_one(user, user, pattern, p1, p2, p3, p4,
					p5, p6, p7, p8);
# endif
	return;
}

/*
 * sendto_channel_butserv
 *
 * Send a message to all members of a channel that are connected to this
 * server.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_channel_butserv(chptr, from, pattern, p1, p2, p3,
			       p4, p5, p6, p7, p8)
aChannel *chptr;
aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_channel_butserv(chptr, from, pattern, va_alist)
aChannel *chptr;
aClient *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	Link	*lp;
        aClient	*acptr;

#ifdef	USE_VARARGS
	for (va_start(vl), lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->value.cptr) &&
		    !(lp->flags & CHFL_ZOMBIE))
			sendto_prefix_one(acptr, from, pattern, vl);
	va_end(vl);
#else
	for (lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->value.cptr) &&
		    !(lp->flags & CHFL_ZOMBIE))
			sendto_prefix_one(acptr, from, pattern,
					  p1, p2, p3, p4,
					  p5, p6, p7, p8);
#endif

	return;
}


/*
 * sendto_channel_butserv_unmask
 *
 * Send a message to all members of a channel that are connected to this
 * server; show real address to ops.
 */
#ifndef USE_VARARGS
/*VARARGS*/
void    sendto_channel_butserv_unmask(chptr, from, pattern, p1, p2, p3,
                               p4, p5, p6, p7, p8)
aChannel *chptr;
aClient *from;
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void    sendto_channel_butserv_unmask(chptr, from, pattern, va_alist)
aChannel *chptr;
aClient *from;
char    *pattern;
va_dcl
{
        va_list vl;
#endif
        Link    *lp;
        aClient *acptr;
        int x = IsMasked(from);
	char *origMask = NULL;


#ifdef  USE_VARARGS
        for (va_start(vl), lp = chptr->members; lp; lp = lp->next) {
                if (MyConnect(acptr = lp->value.cptr) &&
                    !(lp->flags & CHFL_ZOMBIE)) {
                        if (IS_SET(chptr->mode.mode, MODE_SHOWHOST) &&
                            (lp->flags & CHFL_CHANOP) &&
                            !IS_SET(ClientUmode(from), U_FULLMASK) && from->user) {
                            ClearMasked(from);
			    origMask = from->user->mask;
			    from->user->mask = NULL;
			} else origMask = NULL;

                        sendto_prefix_one(acptr, from, pattern, vl);
                        if (x && from->user) {
                            SetMask(from);
			    if (origMask)
				    from->user->mask = origMask;
			}
                    }
        va_end(vl);
        }
#else
        for (lp = chptr->members; lp; lp = lp->next) {
                if (MyConnect(acptr = lp->value.cptr) &&
                    !(lp->flags & CHFL_ZOMBIE)) {
                        if (IS_SET(chptr->mode.mode, MODE_SHOWHOST) &&
                            (lp->flags & CHFL_CHANOP) &&
                            !IS_SET(ClientUmode(from), U_FULLMASK) && from->user) {
                            ClearMasked(from);
			    origMask = from->user->mask;
			    from->user->mask = NULL;
			} else origMask = NULL;
			
                        sendto_prefix_one(acptr, from, pattern,
                                          p1, p2, p3, p4,
                                          p5, p6, p7, p8);
                        if (x && from->user) {
                            SetMasked(from);
			    if (origMask) {
				    from->user->mask = origMask;
			    }
			}
                }
        }
#endif

        return;
}


/*
** send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs)
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
*/

static	int	match_it(one, mask, what)
aClient *one;
char	*mask;
int	what;
{
	switch (what)
	{
	case MATCH_HOST:
		return (match(mask, one->user->host)==0);
	case MATCH_SERVER:
	default:
		return (match(mask, one->user->server)==0);
	}
}

/*
 * sendto_match_servs
 *
 * send to all servers which match the mask at the end of a channel name
 * (if there is a mask present) or to all if no mask.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_match_servs(chptr, from, format, p1,p2,p3,p4,p5,p6,p7,p8,p9)
aChannel *chptr;
aClient	*from;
char	*format, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9;
{
#else
void	sendto_match_servs(chptr, from, format, va_alist)
aChannel *chptr;
aClient	*from;
char	*format;
va_dcl
{
	va_list	vl;
#endif
	int	i;
	aClient	*cptr;
	char	*mask;

#ifdef	USE_VARARGS
	va_start(vl);
#endif

# ifdef NPATH
        check_command((long)3, format, p1, p2, p3);
# endif
	if (chptr)
	    {
		if (*chptr->chname == '&')
			return;
		if (mask = (char *)rindex(chptr->chname, ':'))
			mask++;
	    }
	else
		mask = (char *)NULL;

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]))
			continue;
		if ((cptr == from) || !IsServer(cptr))
			continue;
		if (!BadPtr(mask) && IsServer(cptr) &&
		    match(mask, cptr->name))
			continue;
#ifdef	USE_VARARGS
		sendto_one(cptr, format, vl);
	    }
	va_end(vl);
#else
		sendto_one(cptr, format, p1, p2, p3, p4, p5, p6, p7, p8, p9);
	    }
#endif
}

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_match_butone(one, from, mask, what, pattern,
			    p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
int	what;
char	*mask, *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_match_butone(one, from, mask, what, pattern, va_alist)
aClient *one, *from;
int	what;
char	*mask, *pattern;
va_dcl
{
	va_list	vl;
#endif
	int	i;
	aClient *cptr, *acptr;
	char	cansendlocal, cansendglobal;
  
#ifdef	USE_VARARGS
	va_start(vl);
#endif
	if (MyConnect(from))
	    {
		cansendlocal = (OPCanLNotice(from)) ? 1 : 0;
		cansendglobal = (OPCanGNotice(from)) ? 1 : 0;
	    }
	else
		cansendlocal = cansendglobal = 1;

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]))
			continue;       /* that clients are not mine */
 		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr))
		    {
			if (!cansendglobal)
				continue;
			for (acptr = client; acptr; acptr = acptr->next)
				if (IsRegisteredUser(acptr)
				    && match_it(acptr, mask, what)
				    && acptr->from == cptr)
					break;
			/* a person on that server matches the mask, so we
			** send *one* msg to that server ...
			*/
			if (acptr == NULL)
				continue;
			/* ... but only if there *IS* a matching person */
		    }
		/* my client, does he match ? */
		else if (!cansendlocal || (!(IsRegisteredUser(cptr) &&
			match_it(cptr, mask, what))))
			continue;
#ifdef	USE_VARARGS
		sendto_prefix_one(cptr, from, pattern, vl);
	    }
	va_end(vl);
#else
		sendto_prefix_one(cptr, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
#endif
	return;
}

/*
 * sendto_all_butone.
 *
 * Send a message to all connections except 'one'. The basic wall type
 * message generator.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_all_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_all_butone(one, from, pattern, va_alist)
aClient *one, *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	int	i;
	aClient *cptr;

#ifdef	USE_VARARGS
	for (va_start(vl), i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			sendto_prefix_one(cptr, from, pattern, vl);
	va_end(vl);
#else
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			sendto_prefix_one(cptr, from, pattern,
					  p1, p2, p3, p4, p5, p6, p7, p8);
#endif

	return;
}

/*
 * sendto_ops
 *
 *	Send to *local* ops only.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_ops(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_ops(pattern, va_alist)
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	aClient *cptr;
	int	i;
	char	nbuf[1024];

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendServNotice(cptr))
		    {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
					me.name, cptr->name);
			(void)strncat(nbuf, pattern,
					sizeof(nbuf) - strlen(nbuf));
#ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, va_alist);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, p7, p8);
#endif
		    }
	return;
}

/*
 * sendto_failops
 *
 *      Send to *local* mode +g ops only.
 */
#ifndef USE_VARARGS
/*VARARGS*/
void    sendto_failops(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void    sendto_failops(pattern, va_alist)
char    *pattern;
va_dcl
{
        va_list vl;
#endif
        aClient *cptr;
        int     i;
        char    nbuf[1024];

#ifdef  USE_VARARGS
        va_start(vl);
#endif
        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                    SendFailops(cptr))
                    {
                        (void)sprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
                                        me.name, cptr->name);
                        (void)strncat(nbuf, pattern,
                                        sizeof(nbuf) - strlen(nbuf));
#ifdef  USE_VARARGS
                        sendto_one(cptr, nbuf, va_alist);
#else
                        sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, 
p7, p8);
#endif
                    }
        return;
}

/*
 * sendto_helpops
 *
 *	Send to mode +h people
 */
#ifndef USE_VARARGS
/*VARARGS*/
void	sendto_helpops(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_helpops(pattern, va_alist)
char    *pattern;
va_dcl
{
	va_list vl;
#endif
	aClient *cptr;
	int     i;
	char    nbuf[1024];

#ifdef  USE_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsHelpOp(cptr))
		    {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** HelpOp -- ",
				      me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			              sizeof(nbuf) - strlen(nbuf));
#ifdef  USE_VARARGS
			sendto_one(cptr, nbuf, va_alist);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, 
				   p7, p8);
#endif
		    }
	return;
}



/*
 *   send to flags very loosely, IOW it's not essential
 *   that each message makes it, but it is essential not to 
 *   send out a flood of messages if this routine is called
 *   very quickly...
 */
void sendto_flag_norep(flags, max, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
int	flags;
int     max;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
     char next_pattern[2048*2] = "";
     static time_t last_send = 0;
     static int    num_sent  = 0;

     if (last_send<1 || ((NOW - last_send) > 7 ))
                num_sent = 0;

     last_send = NOW;

     if (++num_sent > 15 && (num_sent < 100))
                 return;
     else if (num_sent >= 100) /* give another warning */
                 num_sent = 13;

     (void)sprintf(next_pattern, pattern, p1, p2, p3, p4, p5, p6,
		   p7, p8);
      if (strcmp(last_pattern, next_pattern)==0)
               recurse_send++;
      else     recurse_send = 0;
      if (recurse_send == 0) strncpy(last_pattern, next_pattern, sizeof(last_pattern));
      if (!recurse_send || recurse_send < max)
       {
              sendto_flag(flags, next_pattern);
       }
       else if ((recurse_send > max) && (((recurse_send-max)) > 10))
                 recurse_send = 0;
       else num_sent--;
}

void
sendto_umode_norep(flags, max, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
     int	flags;
     int     max;
     char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	static char   buff1[1024] = "";
	static char   buff2[1024] = "";
	static char   *outbuf = buff1;
	char *lastbuf;
	static time_t last_send = 0;
	static int    num_sent  = 0;
	int same;

	/*
	 * Rotate buffers.  This sames the last output in the buffer, but
	 * keeps us from having to strcpy it to the save area.
	 */
	if (outbuf == buff1) {
		outbuf = buff2;
		lastbuf = buff1;
	} else {
		outbuf = buff1;
		lastbuf = buff2;
	}

	/*
	 * Print the text to the output buffer.
	 */
	(void)snprintf(outbuf, sizeof(buff1), pattern,
		       p1, p2, p3, p4, p5, p6, p7, p8);

	/*
	 * Has it been at least 7 seconds since the last call?
	 */
	if (last_send < 1 || ((NOW - last_send) > 7 ))
                num_sent = 0;

	/*
	 * Was it the same as last call?
	 */
	same = 0;
	if (num_sent)
		if (strcmp(lastbuf, outbuf) == 0)
			same = 1;

	last_send = NOW;
	if (!same)
		num_sent = 0;

	/*
	 * Print no more than "max" messages in a row from this one source.
	 */
	if (++num_sent > max && (num_sent < 100))
		return;
	else if (num_sent >= 100) /* give another warning */
		num_sent = max;

	sendto_umode(flags, outbuf);
}







/*
 * sendto_flag
 *
 *  Send to specified flag
 */
#ifndef USE_VARARGS
/*VARARGS*/
void	sendto_flag(flags, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
int	flags;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_flag(flags, pattern, va_alist)
int	flags;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	aClient *cptr;
	int	i;
	char	nbuf[1024];

#ifdef	USER_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (ClientFlags(cptr) & flags)==flags)
		    {
			(void)sprintf(nbuf, ":%s NOTICE %s :",
				me.name, cptr->name);
			(void)strncat(nbuf, pattern,
				      sizeof(nbuf) - strlen(nbuf));
#ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, va_list);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6,
				p7, p8);
#endif
		    }
	return;
}


/*
 * sendto_socks
 *
 *  Send message to specified socks struct clients
 */
#ifndef USE_VARARGS
/*VARARGS*/
void	sendto_socks(socks, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aSocks  *socks;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_socks(socks, pattern, va_alist)
aSocks  *socks;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	aClient *cptr;
	int	i;
	char	nbuf[1024];

#ifdef	USER_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (cptr->socks == socks))
		    {
			(void)sprintf(nbuf, ":%s NOTICE AUTH :",
				me.name);
			(void)strncat(nbuf, pattern,
				      sizeof(nbuf) - strlen(nbuf));
#ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, va_list);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6,
				p7, p8);
#endif
		    }
	return;
}


void	sendto_umode(flags, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
int	flags;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
	aClient *cptr;
	int	i;
	char	nbuf[1024];

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (ClientUmode(cptr) & flags)==flags)
                   {
                       (void)sprintf(nbuf, ":%s NOTICE %s :",
                               me.name, cptr->name);
                       (void)strncat(nbuf, pattern,
                                     sizeof(nbuf) - strlen(nbuf));
                       sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6,
                               p7, p8);
                   }
       return;
}


void   sendto_umode_except(flags, notflags, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
int    flags, notflags;
char   *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
       aClient *cptr;
       int     i;
       char    nbuf[1024];

       for (i = 0; i <= highest_fd; i++)
               if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                   ((ClientUmode(cptr) & flags)==flags) &&
                    !((ClientUmode(cptr) & notflags) == notflags))
		    {
			(void)sprintf(nbuf, ":%s NOTICE %s :",
				me.name, cptr->name);
			(void)strncat(nbuf, pattern,
				      sizeof(nbuf) - strlen(nbuf));
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6,
				p7, p8);
		    }
	return;
}





/*
 * sendto_failops_whoare_opers
 *
 *      Send to *local* mode +g ops only who are also +o.
 */
#ifndef USE_VARARGS
/*VARARGS*/
void    sendto_failops_whoare_opers(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void    sendto_failops_whoare_opers(pattern, va_alist)
char    *pattern;
va_dcl
{
        va_list vl;
#endif
        aClient *cptr;
        int     i;
        char    nbuf[1024];

#ifdef  USE_VARARGS
        va_start(vl);
#endif
        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                    SendFailops(cptr) && OPCangmode(cptr))
                    {
                        (void)sprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
                                        me.name, cptr->name);
                        (void)strncat(nbuf, pattern,
                                        sizeof(nbuf) - strlen(nbuf));
#ifdef  USE_VARARGS
                        sendto_one(cptr, nbuf, va_alist);
#else
                        sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6,
p7, p8);
#endif
                    }

        return;
}
/*
 * sendto_locfailops
 *
 *      Send to *local* mode +g ops only who are also +o.
 */
#ifndef USE_VARARGS
/*VARARGS*/
void    sendto_locfailops(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char    *pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void    sendto_locfailops(pattern, va_alist)
char    *pattern;
va_dcl
{
        va_list vl;
#endif
        aClient *cptr;
        int     i;
        char    nbuf[1024];

#ifdef  USE_VARARGS
        va_start(vl);
#endif
        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                    SendFailops(cptr) && IsAnOper(cptr))
                    {
                        (void)sprintf(nbuf, ":%s NOTICE %s :*** LocOps -- ",
                                        me.name, cptr->name);
                        (void)strncat(nbuf, pattern,
                                        sizeof(nbuf) - strlen(nbuf));
#ifdef  USE_VARARGS
                        sendto_one(cptr, nbuf, va_alist);
#else
                        sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6,
p7, p8);
#endif
                    }

        return;
}
/*
 * sendto_opers
 *
 *	Send to *local* ops only. (all +O or +o people)
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_opers(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_opers(pattern, va_alist)
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	aClient *cptr;
	int	i;
	char	nbuf[1024];

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsAnOper(cptr))
		    {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** Oper -- ",
					me.name, cptr->name);
			(void)strncat(nbuf, pattern,
					sizeof(nbuf) - strlen(nbuf));
#ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, va_alist);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, 
p7, p8);
#endif
		    }

	return;
}

/* ** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_ops_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_ops_butone(one, from, pattern, va_alist)
aClient *one, *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	int	i;
	aClient *cptr;

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
# ifdef	USE_VARARGS
      		sendto_prefix_one(cptr->from, from, pattern, vl);
	    }
	va_end(vl);
# else
      		sendto_prefix_one(cptr->from, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
# endif
	return;
}

/*
** sendto_ops_butone
**	Send message to all operators regardless of whether they are +w or 
**	not.. 
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_opers_butone(one, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *one, *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_opers_butone(one, from, pattern, va_alist)
aClient *one, *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	int	i;
	aClient *cptr;

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (!IsAnOper(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
/* This is a HORRIBLE hack.  but it's the only way I could think of to 
 * format the notice messages properly.  --Russell
 */
# ifdef	USE_VARARGS
      		sendto_prefix_one(cptr->from, from, pattern, vl);
	    }
	va_end(vl);
# else
      		sendto_prefix_one(cptr->from, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
# endif
	return;
}
/*
** sendto_ops_butme
**	Send message to all operators except local ones
** from- client which message is from *NEVER* NULL!!
*/
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_ops_butme(from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient **from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_ops_butme(from, pattern, va_alist)
aClient *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	int	i;
	aClient *cptr;

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	    {
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (!strcmp(cptr->user->server, me.name))	/* a locop */
			continue;
		sentalong[i] = 1;
# ifdef	USE_VARARGS
      		sendto_prefix_one(cptr->from, from, pattern, vl);
	    }
	va_end(vl);
# else
      		sendto_prefix_one(cptr->from, from, pattern,
				  p1, p2, p3, p4, p5, p6, p7, p8);
	    }
# endif
	return;
}

/*
 * sendto_prefix_one()
 *
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 * -avalon
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_prefix_one(to, from, pattern, p1, p2, p3, p4, p5, p6, p7, p8)
aClient *to;
aClient *from;
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_prefix_one(to, from, pattern, va_alist)
aClient *to;
aClient *from;
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	static	char	sender[HOSTLEN+NICKLEN+USERLEN+5];
	anUser	*user;
	char	*par;
	int	flag = 0;

#ifdef	USE_VARARGS
	va_start(vl);
	par = va_arg(vl, char *);
#else
	par = p1;
#endif
	if (to && from && MyClient(to) && IsPerson(from) &&
	    !mycmp(par, from->name))
	    {
		user = from->user;
		(void)strcpy(sender, from->name);
		if (user)
		    {
			if (*user->username)
			    {
				(void)strcat(sender, "!");
				(void)strcat(sender, user->username);
			    }
			if (*user->host && !MyConnect(from))
			    {
				(void)strcat(sender, "@");
				(void)strcat(sender, UGETHOST(to, user));
				flag = 1;
			    }
		    }
		/*
		** flag is used instead of index(sender, '@') for speed and
		** also since username/nick may have had a '@' in them. -avalon
		*/
		if (!flag && MyConnect(from) && *user->host)
		    {
			(void)strcat(sender, "@");
                        if (!IsMasked(from) || !user->mask || to == from || IsOper(to))
			    (void)strcat(sender, from->sockhost);
			else
			    (void)strcat(sender, user->mask);
		    }
#ifdef	USE_VARARGS
		par = sender;
	    }
	sendto_one(to, pattern, par, vl);
	va_end(vl);
#else
		par = sender;
	    }
	sendto_one(to, pattern, par, p2, p3, p4, p5, p6, p7, p8);
#endif
}

/*
 * sendto_realops
 *
 *	Send to *local* ops only but NOT +s nonopers.
 */
#ifndef	USE_VARARGS
/*VARARGS*/
void	sendto_realops(pattern, p1, p2, p3, p4, p5, p6, p7, p8)
char	*pattern, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8;
{
#else
void	sendto_realops(pattern, va_alist)
char	*pattern;
va_dcl
{
	va_list	vl;
#endif
	aClient *cptr;
	int	i;
	char	nbuf[1024];

#ifdef	USE_VARARGS
	va_start(vl);
#endif
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsOper(cptr))
		    {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
					me.name, cptr->name);
			(void)strncat(nbuf, pattern,
					sizeof(nbuf) - strlen(nbuf));
#ifdef	USE_VARARGS
			sendto_one(cptr, nbuf, va_alist);
#else
			sendto_one(cptr, nbuf, p1, p2, p3, p4, p5, p6, p7, p8);
#endif
		    }
#ifdef	USE_VARARGS
	va_end(vl);
#endif
	return;
}

