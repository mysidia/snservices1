/*
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

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"

#include "ircd/send.h"
#include "ircd/string.h"

IRCD_SCCSID("@(#)send.c	2.32 2/28/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

#define NEWLINE	"\r\n"

static int recurse_send = 0;
static char last_pattern[2048 * 2] = "";
static char sendbuf[2048];
static int sentalong[MAXCONNECTIONS];

static int send_message(aClient *, char *, int);

/*
 * An error has been detected. The link *must* be closed,
 * but *cannot* call ExitClient (m_bye) from here.
 * Instead, mark it with FLAGS_DEADSOCKET. This should
 * generate ExitClient from the main loop.
 *
 * If 'notice' is not NULL, it is assumed to be a format
 * for a message to local opers. I can contain only one
 * '%s', which will be replaced by the sockhost field of
 * the failing link.
 *
 * Also, the notice is skipped for "uninteresting" cases,
 * like Persons and yet unknown connections...
 */
static int
dead_link(aClient *to, char *notice)
{
	ClientFlags(to) |= FLAGS_DEADSOCKET;

	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
	if (!IsPerson(to)
	    && !IsUnknown(to)
	    && !(ClientFlags(to) & FLAGS_CLOSING))
		sendto_ops(notice, get_client_name(to, FALSE));

	Debug((DEBUG_ERROR, notice, get_client_name(to, FALSE)));

	return -1;
}

/*
 * Used to empty all output buffers for all connections. Should only
 * be called once per scan of connections. There should be a select in
 * here perhaps but that means either forcing a timeout or doing a poll.
 * When flushing, all we do is empty the obuffer array for each local
 * client and try to send it. if we cant send it, it goes into the sendQ
 */
void
flush_connections(int fd)
{
	int	i;
	aClient *cptr;

	if (fd == me.fd) {
		for (i = highest_fd; i >= 0; i--)
			if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
				(void)send_queued(cptr);
	} else if (fd >= 0
		   && (cptr = local[fd])
		   && DBufLength(&cptr->sendQ) > 0)
		(void)send_queued(cptr);
}

/*
 * Internal utility which delivers one message buffer to the
 * socket. Takes care of the error handling and buffering, if
 * needed.
 *
 * If msg is NULL, we are flushing the connection.
 */
static int
send_message(aClient *to, char *msg, int len)
{
	if (IsDead(to))
		return 0; /* This socket has already been marked as dead */

	if (DBufLength(&to->sendQ) > get_sendq(to)) {
		if (IsServer(to))
			sendto_ops("Max SendQ limit exceeded for %s: %d > %d",
				   get_client_name(to, FALSE),
				   DBufLength(&to->sendQ), get_sendq(to));
		return dead_link(to, "Max Sendq exceeded");
	} else if (dbuf_put(&to->sendQ, msg, len) < 0)
		return dead_link(to, "Buffer allocation error for %s");

	/*
	 * Update statistics. The following is slightly incorrect
	 * because it counts messages even if queued, but bytes
	 * only really sent. Queued bytes get updated in SendQueued.
	 */
	to->sendM += 1;
	me.sendM += 1;
	if (to->acpt && to->acpt != &me)
		to->acpt->sendM += 1;

	/*
	 * This little bit is to stop the sendQ from growing too large when
	 * there is no need for it to. Thus we call send_queued() every time
	 * 2k has been added to the queue since the last non-fatal write.
	 * Also stops us from deliberately building a large sendQ and then
	 * trying to flood that link with data (possible during the net
	 * relinking done by servers with a large load).
	 */
	if (DBufLength(&to->sendQ)/1024 > to->lastsq)
		send_queued(to);

	return 0;
}

/*
 * This function is called from the main select-loop (or whatever)
 * when there is a chance the some output would be possible. This
 * attempts to empty the send queue as far as possible...
 */
int
send_queued(aClient *to)
{
	char	*msg;
	int	len, rlen;

	/*
	 * Once socket is marked dead, we cannot start writing to it,
	 * even if the error is removed...
	 */
	if (IsDead(to)) {
		/*
		 * Actually, we should *NEVER* get here--something is
		 * not working correct if send_queued is called for a
		 * dead socket.
		 */
		return -1;
	}
	while (DBufLength(&to->sendQ) > 0) {
		msg = dbuf_map(&to->sendQ, &len);
		if ((rlen = deliver_it(to, msg, len)) < 0)
			return dead_link(to,
					 "Write error to %s, closing link");
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ)/1024;
		if (rlen < len)
			break;
	}

	return (IsDead(to)) ? -1 : 0;
}

/*
 * send message to single client
 */
void
sendto_one(aClient *to, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsendto_one(to, fmt, ap);
	va_end(ap);
}

void
vsendto_one(aClient *to, char *fmt, va_list ap)
{
	(void)vsprintf(sendbuf, fmt, ap);

	Debug((DEBUG_SEND, "Sending [%s] to %s", sendbuf, to->name));

	if (to->from)
		to = to->from;
	if (to->fd < 0) {
		assert(to->fd >= 0);
		Debug((DEBUG_ERROR,
		       "Local socket %s with negative fd... AARGH!",
		       to->name));
	} else if (IsMe(to)) {
		sendto_ops("Trying to send [%s] to myself!", sendbuf);
		return;
	}
	(void)strcat(sendbuf, NEWLINE);
	sendbuf[510] = '\r';
	sendbuf[511] = '\n';
	sendbuf[512] = '\0';
	(void)send_message(to, sendbuf, strlen(sendbuf));
}

void
vsendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
		       char *fmt, va_list ap)
{
	va_list ap2;
	Link	*lp;
	aClient *acptr;
	int	i;

	bzero(sentalong, sizeof(sentalong));
	for (lp = chptr->members ; lp ; lp = lp->next) {
		acptr = lp->value.cptr;
		if (acptr->from == one || (lp->flags & CHFL_ZOMBIE))
			continue;	/* ...was the one I should skip */
		i = acptr->from->fd;
		va_copy(ap2, ap);
		if (MyConnect(acptr) && IsRegisteredUser(acptr)) {
			vsendto_prefix_one(acptr, from, fmt, ap2);
			sentalong[i] = 1;
		} else {
			/*
			 * Now check whether a message has been sent to this
			 * remote link already
			 */
			if (sentalong[i] == 0) {
	  			vsendto_prefix_one(acptr, from, fmt, ap2);
				sentalong[i] = 1;
			}
		}
		va_end(ap2);
	}
}

void
sendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
		      char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vsendto_channel_butone(one, from, chptr, fmt, ap);
	va_end(ap);
}

/*
 * Send a message to all OPs in channel chptr that
 * are directly on this server and sends the message
 * on to the next server if it has any OPs.
 *
 * All servers must have this functional ability
 * or one without will send back an error message.
 */
void
sendto_channelops_butone(aClient *one, aClient *from, aChannel *chptr,
			 char *fmt, ...)
{ 
	va_list ap;
	va_list ap2;
	Link	*lp;
	aClient	*acptr;
	int	i;

	va_start(ap, fmt);

	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next) {
		acptr = lp->value.cptr;
		if (acptr->from == one || (lp->flags & CHFL_ZOMBIE) ||
		    !(lp->flags & CHFL_CHANOP))
			continue;       /* ...was the one I should skip
                                           or user not not a channel op */
		i = acptr->from->fd;

		if (MyConnect(acptr) && IsRegisteredUser(acptr)) {
			va_copy(ap2, ap);
			vsendto_prefix_one(acptr, from, fmt, ap2);
			va_end(ap2);

			sentalong[i] = 1;
		} else {
		/*
		 * Now check whether a message has been sent to this
		 * remote link already
		 */
			if (sentalong[i] == 0) {
				va_copy(ap2, ap);
				vsendto_prefix_one(acptr, from, fmt, ap2);
				va_end(ap2);

				sentalong[i] = 1;
			}
		}
	}

	va_end(ap);
}



void
sendto_channelvoices_butone(aClient *one, aClient *from, aChannel *chptr,
			    char *fmt, ...)
{ 
	va_list ap;
	va_list ap2;
	Link	*lp;
	aClient	*acptr;
	int	i;

	va_start(ap, fmt);

	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next) {
		acptr = lp->value.cptr;
		if (acptr->from == one
		    || (lp->flags & CHFL_ZOMBIE)
		    || (!(lp->flags & CHFL_VOICE)
			&& !(lp->flags & CHFL_CHANOP) ))
			continue;       /* ...was the one I should skip
                                           or user not not a channel op */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr)) {
			va_copy(ap2, ap);
			vsendto_prefix_one(acptr, from, fmt, ap2);
			va_end(ap2);

			sentalong[i] = 1;
		} else {
		/*
		 * Now check whether a message has been sent to this
		 * remote link already
		 */
			if (sentalong[i] == 0) {
				va_copy(ap2, ap);
				vsendto_prefix_one(acptr, from, fmt, ap2);
				va_end(ap2);

				sentalong[i] = 1;
			}
		}
	}

	va_end(ap);
}

/*
 * Send a message to all connected servers except the client 'one'.
 */
void
sendto_serv_butone(aClient *one, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient *cptr;

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++) {
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		if (IsServer(cptr)) {
			va_copy(ap2, ap);
			vsendto_one(cptr, fmt, ap2);
			va_end(ap2);
		}
	}

	va_end(ap);
}

/*
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
void
sendto_common_channels(aClient *user, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient *cptr;
	Link	*lp;

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++) {
		if (!(cptr = local[i]) || IsServer(cptr) ||
		    user == cptr || !user->user)
			continue;
		for (lp = user->user->channel; lp; lp = lp->next)
			if (IsMember(user, lp->value.chptr) &&
			    IsMember(cptr, lp->value.chptr)) {
				va_copy(ap2, ap);
				vsendto_prefix_one(cptr, user, fmt, ap2);
				va_end(ap2);
				break;
			    }
	}
	if (MyConnect(user))
		vsendto_prefix_one(user, user, fmt, ap);

	va_end(ap);
}

/*
 * Send a message to all members of a channel that are connected to this
 * server.
 */
void
sendto_channel_butserv(aChannel *chptr, aClient *from, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	Link	*lp;
        aClient	*acptr;

	va_start(ap, fmt);

	for (lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->value.cptr) &&
		    !(lp->flags & CHFL_ZOMBIE)) {
			va_copy(ap2, ap);
			vsendto_prefix_one(acptr, from, fmt, ap2);
			va_end(ap2);
		}

	va_end(ap);
}


/*
 * Send a message to all members of a channel that are connected to this
 * server; show real address to ops.
 */
void
sendto_channel_butserv_unmask(aChannel *chptr, aClient *from, char *fmt, ...)
{
        va_list ap;
	va_list ap2;
        Link    *lp;
        aClient *acptr;
        int x = IsMasked(from);
	char *origMask = NULL;

	va_start(ap, fmt);

        for (lp = chptr->members; lp; lp = lp->next) {
                if (MyConnect(acptr = lp->value.cptr) &&
                    !(lp->flags & CHFL_ZOMBIE)) {
                        if (IS_SET(chptr->mode.mode, MODE_SHOWHOST) &&
                            (lp->flags & CHFL_CHANOP) &&
                            !IS_SET(ClientUmode(from), U_FULLMASK) && from->user) {
                            ClearMasked(from);
			    origMask = from->user->mask;
			    from->user->mask = NULL;
			} else
				origMask = NULL;

			va_copy(ap2, ap);
                        vsendto_prefix_one(acptr, from, fmt, ap2);
			va_end(ap2);

                        if (x && from->user) {
                            SetMasked(from);
			    if (origMask)
				    from->user->mask = origMask;
			}
                }
        }

	va_end(ap);
}


/*
 * send a msg to all ppl on servers/hosts that match a specified mask
 * (used for enhanced PRIVMSGs)
 */

static int
match_it(aClient *one, char *mask, int what)
{
	switch (what) {
	case MATCH_HOST:
		return (match(mask, one->user->host) == 0);

	case MATCH_SERVER:
	default:
		return (match(mask, one->user->server) == 0);
	}
}

/*
 * send to all servers which match the mask at the end of a channel name
 * (if there is a mask present) or to all if no mask.
 */
void
sendto_match_servs(aChannel *chptr, aClient *from, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient	*cptr;
	char	*mask;

	va_start(ap, fmt);

	if (chptr) {
		if (*chptr->chname == '&')
			return;
		if ((mask = (char *)rindex(chptr->chname, ':')))
			mask++;
	} else
		mask = (char *)NULL;

	for (i = 0; i <= highest_fd; i++) {
		if (!(cptr = local[i]))
			continue;
		if ((cptr == from) || !IsServer(cptr))
			continue;
		if (!BadPtr(mask) && IsServer(cptr) &&
		    match(mask, cptr->name))
			continue;

		va_copy(ap2, ap);
		vsendto_one(cptr, fmt, ap2);
		va_end(ap2);
	}

	va_end(ap);
}

/*
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
void
sendto_match_butone(aClient *one, aClient *from, char *mask, int what,
		    char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient *cptr, *acptr;
	char	cansendlocal, cansendglobal;
  
	va_start(ap, fmt);

	if (MyConnect(from)) {
		cansendlocal = (OPCanLNotice(from)) ? 1 : 0;
		cansendglobal = (OPCanGNotice(from)) ? 1 : 0;
	} else
		cansendlocal = cansendglobal = 1;

	for (i = 0; i <= highest_fd; i++) {
		if (!(cptr = local[i]))
			continue;       /* that clients are not mine */
 		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr)) {
			if (!cansendglobal)
				continue;
			for (acptr = client; acptr; acptr = acptr->next)
				if (IsRegisteredUser(acptr)
				    && match_it(acptr, mask, what)
				    && acptr->from == cptr)
					break;
			/* a person on that server matches the mask, so we
			 * send *one* msg to that server ...
			*/
			if (acptr == NULL)
				continue;
			/* ... but only if there *IS* a matching person */
		} else if (!cansendlocal || (!(IsRegisteredUser(cptr) &&
					       match_it(cptr, mask, what))))
			/* my client, does he match ? */
			continue;

		va_copy(ap2, ap);
		vsendto_prefix_one(cptr, from, fmt, ap2);
		va_end(ap2);
	}

	va_end(ap);
}

/*
 * Send a message to all connections except 'one'. The basic wall type
 * message generator.
 */
void
sendto_all_butone(aClient *one, aClient *from, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient *cptr;

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr) {
			va_copy(ap2, ap);
			vsendto_prefix_one(cptr, from, fmt, ap2);
			va_end(ap2);
		}

	va_end(ap);
}

/*
 * Send to local ops only.
 */
void
sendto_ops(char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	aClient *cptr;
	int	i;
	char	nbuf[1024];

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendServNotice(cptr)) {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
				      me.name, cptr->name);
			(void)strncat(nbuf, fmt, sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
			vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		}

	va_end(ap);
}

/*
 * Send to local mode +g ops only.
 */
void
sendto_failops(char *fmt, ...)
{
        va_list ap;
	va_list ap2;
        aClient *cptr;
        int     i;
        char    nbuf[1024];


        va_start(ap, fmt);

        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                    SendFailops(cptr)) {
                        (void)sprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
				      me.name, cptr->name);
                        (void)strncat(nbuf, fmt, sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
                        vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		}

	va_end(ap);
}

/*
 * Send to mode +h people
 */
void
sendto_helpops(char *fmt, ...)
{
	va_list ap;
	va_list ap2;
	aClient *cptr;
	int     i;
	char    nbuf[1024];

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsHelpOp(cptr)) {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** HelpOp -- ",
				      me.name, cptr->name);
			(void)strncat(nbuf, fmt,
			              sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
			vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		}

	va_end(ap);
}

/*
 * send to flags very loosely, IOW it's not essential
 * that each message makes it, but it is essential not to 
 * send out a flood of messages if this routine is called
 * very quickly...
 */
void
sendto_flag_norep(int flags, int max, char *fmt, ...)
{
	va_list ap;
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

	va_start(ap, fmt);
	(void)vsprintf(next_pattern, fmt, ap);
	va_end(ap);

	if (strcmp(last_pattern, next_pattern)==0)
		recurse_send++;
	else
		recurse_send = 0;
	if (recurse_send == 0)
		strncpy(last_pattern, next_pattern, sizeof(last_pattern));
	if (!recurse_send || recurse_send < max)
		sendto_flag(flags, next_pattern);
	else if ((recurse_send > max) && (((recurse_send-max)) > 10))
		recurse_send = 0;
	else
		num_sent--;
}

void
sendto_umode_norep(int flags, int max, char *fmt, ...)
{
	va_list ap;
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
	va_start(ap, fmt);
	(void)vsnprintf(outbuf, sizeof(buff1), fmt, ap);
	va_end(ap);

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
 * Send to specified flag
 */
void
sendto_flag(int flags, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	aClient *cptr;
	int	i;
	char	nbuf[1024];

	va_start(ap, fmt);

	for (i = 0 ; i <= highest_fd ; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (ClientFlags(cptr) & flags)==flags) {
			(void)sprintf(nbuf, ":%s NOTICE %s :",
				      me.name, cptr->name);
			(void)strncat(nbuf, fmt, sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
			vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		}

	va_end(ap);
}

void
sendto_umode(int flags, char *fmt, ...)
{
	va_list ap;
	va_list ap2;
	aClient *cptr;
	int	i;
	char	nbuf[1024];

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (ClientUmode(cptr) & flags)==flags) {
                       (void)sprintf(nbuf, ":%s NOTICE %s :",
				     me.name, cptr->name);
                       (void)strncat(nbuf, fmt, sizeof(nbuf) - strlen(nbuf));

		       va_copy(ap2, ap);
                       vsendto_one(cptr, nbuf, ap2);
		       va_end(ap2);
		}

	va_end(ap);
}


void
sendto_umode_except(int flags, int notflags, char *fmt, ...)
{
	va_list ap;
	va_list ap2;
	aClient *cptr;
	int     i;
	char    nbuf[1024];

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    ((ClientUmode(cptr) & flags)==flags) &&
                    !((ClientUmode(cptr) & notflags) == notflags)) {
			(void)sprintf(nbuf, ":%s NOTICE %s :",
				      me.name, cptr->name);
			(void)strncat(nbuf, fmt,
				      sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
			vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		}

	va_end(ap);
}

/*
 * Send to local mode +g ops only who are also +o.
 */
void
sendto_locfailops(char *fmt, ...)
{
        va_list ap;
	va_list ap2;
        aClient *cptr;
        int     i;
        char    nbuf[1024];

        va_start(ap, fmt);

        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                    SendFailops(cptr) && IsAnOper(cptr)) {
                        (void)sprintf(nbuf, ":%s NOTICE %s :*** LocOps -- ",
                                        me.name, cptr->name);
                        (void)strncat(nbuf, fmt,
                                        sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
                        vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		    }

	va_end(ap);
}

/*
 * Send to *local* ops only. (all +O or +o people)
 */
void
sendto_opers(char *fmt, ...)
{
	va_list	ap;
	va_list	ap2;
	aClient *cptr;
	int	i;
	char	nbuf[1024];

	va_start(ap, fmt);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsAnOper(cptr)) {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** Oper -- ",
				      me.name, cptr->name);
			(void)strncat(nbuf, fmt, sizeof(nbuf) - strlen(nbuf));

			va_copy(ap2, ap);
			vsendto_one(cptr, nbuf, ap);
			va_end(ap2);
		}

	va_end(ap);
}

/*
 * Send message to all operators.
 *
 * one - client not to send message to
 * from- client which message is from *NEVER* NULL!!
 */
void
sendto_ops_butone(aClient *one, aClient *from, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient *cptr;

	va_start(ap, fmt);

	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next) {
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;

		va_copy(ap2, ap);
      		vsendto_prefix_one(cptr->from, from, fmt, ap2);
		va_end(ap2);
	}

	va_end(ap);
}

/*
 * Send message to all operators regardless of whether they are +w or 
 * not.. 
 *
 * one - client not to send message to
 * from- client which message is from *NEVER* NULL!!
 */
void
sendto_opers_butone(aClient *one, aClient *from, char *fmt, ...)
{
	va_list	ap;
	va_list ap2;
	int	i;
	aClient *cptr;

	va_start(ap, fmt);

	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next) {
		if (!IsAnOper(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;

		va_copy(ap2, ap);
      		vsendto_prefix_one(cptr->from, from, fmt, ap2);
		va_end(ap2);
	}

	va_end(ap);
}

/*
 * Send message to all operators except local ones
 *
 * from- client which message is from *NEVER* NULL!!
 */
void
sendto_ops_butme(aClient *from, char *fmt, ...)
{
	va_list	ap;
	va_list	ap2;
	int	i;
	aClient *cptr;

	va_start(ap, fmt);

	for (i=0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next) {
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (!strcmp(cptr->user->server, me.name))	/* a locop */
			continue;
		sentalong[i] = 1;

		va_copy(ap2, ap);
      		vsendto_prefix_one(cptr->from, from, fmt, ap2);
		va_end(ap2);
	}

	va_end(ap);
}

/*
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 */
void
sendto_prefix_one(aClient *to, aClient *from, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsendto_prefix_one(to, from, fmt, ap);
	va_end(ap);
}

void
vsendto_prefix_one(aClient *to, aClient *from, char *fmt, va_list ap)
{
	static	char	sender[HOSTLEN+NICKLEN+USERLEN+5];
	anUser	*user;
	char	*par;
	int	flag = 0;
	static char fmt2[1024];
	static char fmt2_tmp[1024];
	char *start;

	par = va_arg(ap, char *);

	if (to && from && MyClient(to) && IsPerson(from) &&
	    !mycmp(par, from->name)) {
		user = from->user;
		(void)strcpy(sender, from->name);
		if (user) {
			if (*user->username) {
				(void)strcat(sender, "!");
				(void)strcat(sender, user->username);
			}
			if (*user->host && !MyConnect(from)) {
				(void)strcat(sender, "@");
				(void)strcat(sender, UGETHOST(to, user));
				flag = 1;
			}
		}
		/*
		 * flag is used instead of index(sender, '@') for speed and
		 * also since username/nick may have had a '@' in them. -avalon
		 */
		if (!flag && MyConnect(from) && *user->host) {
			(void)strcat(sender, "@");
                        if (!IsMasked(from) || !user->mask
			    || to == from || IsOper(to))
				(void)strcat(sender, from->sockhost);
			else
				(void)strcat(sender, user->mask);
		}
		par = sender;
	}

	/*
	 * This is nasty.  Because we're doing to call vsendto_one() with
	 * both a char * and an va_list, we have to partially render the
	 * first argument into the format give to vsendto_one().
	 */
	start = index(fmt, '%');
	if (start == NULL) {
		vsendto_one(to, fmt, ap);  /* degenerate case */
	} else {
		start++;
		while ((*start == '.') || (*start == '-')
		       || ((*start >= '0') && (*start <= '9')))
			start++;
		assert(*start == 's');
		start++;
		strncpy(fmt2_tmp, fmt, start - fmt);
		sprintf(fmt2, fmt2_tmp, par);
		if (start != 0)
			strcat(fmt2, start);
		vsendto_one(to, fmt2, ap);
	}
}

/*
 * Send to local ops only but NOT +s nonopers.
 */
void
sendto_realops(char *pattern, ...)
{
	va_list	ap;
	va_list	ap2;
	aClient *cptr;
	int	i;
	char	nbuf[1024];

	va_start(ap, pattern);

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsOper(cptr)) {
			(void)sprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
				      me.name, cptr->name);
			(void)strncat(nbuf, pattern,
				      sizeof(nbuf) - strlen(nbuf));


			va_copy(ap2, ap);
			vsendto_one(cptr, nbuf, ap2);
			va_end(ap2);
		}

	va_end(ap);
}
