/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_misc.c (formerly ircd/date.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

#include <sys/time.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "userload.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/param.h>
#include "h.h"
#include "msg.h"

#ifdef SYSLOGH
#include <syslog.h>
#endif

#include "ircd/send.h"
#include "ircd/string.h"

IRCD_SCCSID("@(#)s_misc.c	2.42 3/1/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

#undef LOG_USER

static void exit_one_client(aClient *,aClient *,aClient *,char *);

static char *months[] = {
	"January",	"February",	"March",	"April",
	"May",	        "June",	        "July",	        "August",
	"September",	"October",	"November",	"December"
};

static char *weekdays[] = {
	"Sunday",	"Monday",	"Tuesday",	"Wednesday",
	"Thursday",	"Friday",	"Saturday"
};

/*
 * stats stuff
 */
struct	stats	ircst, *ircstp = &ircst;

char *
date(time_t clock)
{
	static	char	buf[80], plus;
	struct	tm *lt, *gm;
	struct	tm	gmbuf;
	int	minswest;

	if (!clock) 
		time(&clock);
	gm = gmtime(&clock);
	bcopy((char *)gm, (char *)&gmbuf, sizeof(gmbuf));
	gm = &gmbuf;
	lt = localtime(&clock);

	if (lt->tm_yday == gm->tm_yday)
		minswest = (gm->tm_hour - lt->tm_hour) * 60 +
			   (gm->tm_min - lt->tm_min);
	else if (lt->tm_yday > gm->tm_yday)
		minswest = (gm->tm_hour - (lt->tm_hour + 24)) * 60;
	else
		minswest = ((gm->tm_hour + 24) - lt->tm_hour) * 60;

	plus = (minswest > 0) ? '-' : '+';
	if (minswest < 0)
		minswest = -minswest;

	(void)sprintf(buf, "%s %s %d %04d -- %02d:%02d %c%02d:%02d",
		weekdays[lt->tm_wday], months[lt->tm_mon],lt->tm_mday,
		1900 + lt->tm_year, lt->tm_hour, lt->tm_min,
		plus, minswest/60, minswest%60);

	return buf;
}

/**
 ** myctime()
 **   This is like standard ctime()-function, but it zaps away
 **   the newline from the end of that string. Also, it takes
 **   the time value as parameter, instead of pointer to it.
 **   Note that it is necessary to copy the string to alternate
 **   buffer (who knows how ctime() implements it, maybe it statically
 **   has newline there and never 'refreshes' it -- zapping that
 **   might break things in other places...)
 **
 **/

char	*myctime(time_t value)
{
	static	char	buf[30];
	char	*p;

	(void)strncpy(buf, ctime(&value), 28);
        buf[27] = 0;
	if ((p = (char *)index(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}

/*
** check_registered_user is used to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
int	check_registered_user(aClient *sptr)
{
	if (!IsRegisteredUser(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	    }
	return 0;
}

/*
** check_registered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
int	check_registered(aClient *sptr)
{
	if (!IsRegistered(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	    }
	return 0;
}

/*
** get_client_name
**      Return the name of the client for various tracking and
**      admin purposes. The main purpose of this function is to
**      return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "name[user@ip#.port]" if 'showip' is true;
**	  "name[sockethost]", if name and sockhost are different and
**	  showip is false; else
**	  "name".
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char	*get_client_name(aClient *sptr, int showip)
{
  static char nbuf[HOSTLEN * 2 + USERLEN + 5];

  if (MyConnect(sptr))
  {
      if (showip)
      {
	  sprintf(nbuf, "%s[%s@%s.%u]",
		  sptr->name,
		  (ClientFlags(sptr) & FLAGS_GOTID) ? sptr->username : "",
		  inetntoa(&sptr->addr), (unsigned int)sptr->port);
      } 
      else
      {
	  if (mycmp(sptr->name, sptr->sockhost)) {
		  if (!IsServer(sptr))
		  	  (void)sprintf(nbuf, "%s[%s]", sptr->name, sptr->sockhost);
		  else
			  (void)sprintf(nbuf, "%s[ip-masked]", sptr->name);
	  }
	  else
	    return sptr->name;
      }

      return nbuf;
  }

  return sptr->name;
}

char	*get_client_name_mask(aClient *sptr, int showip, int showport, int mask)
{
  static char nbuf[HOSTLEN * 2 + USERLEN + 5];

  if (MyConnect(sptr))
  {
      if (showip)
      {
	  sprintf(nbuf, "%s[%s@%s.%u]",
		  sptr->name,
		  (ClientFlags(sptr) & FLAGS_GOTID) ? sptr->username : "",
		  mask ? genHostMask(inetntoa(&sptr->addr)) : inetntoa(&sptr->addr), showport ? (unsigned int)sptr->port : (unsigned int)0);
      }
      else
      {
	  if (mycmp(sptr->name, sptr->sockhost))
	    (void)sprintf(nbuf, "%s[%s]", sptr->name, mask ? genHostMask(sptr->sockhost) : sptr->sockhost);
	  else
	    return sptr->name;
      }

      return nbuf;
  }

  return sptr->name;
}

char	*get_client_host(aClient *cptr)
{
  static char nbuf[HOSTLEN * 2 + USERLEN + 5];

  if (!MyConnect(cptr))
    return cptr->name;

  if (!cptr->hostp)
    return get_client_name(cptr, FALSE);

  (void)sprintf(nbuf, "%s[%-.*s@%-.*s]", cptr->name, USERLEN,
		(ClientFlags(cptr) & FLAGS_GOTID) ? cptr->username : "",
		HOSTLEN, cptr->hostp->h_name);

  return nbuf;
}

/*
 * Form sockhost such that if the host is of form user@host, only the host
 * portion is copied.
 */
void	get_sockhost(aClient *cptr, char *host)
{
	char	*s;
	if ((s = (char *)index(host, '@')))
		s++;
	else
		s = host;
	strncpyzt(cptr->sockhost, s, sizeof(cptr->sockhost));
}

/*
 * Return wildcard name of my server name according to given config entry
 * --Jto
 */
char	*my_name_for_link(char *name, aConfItem *aconf)
{
	static	char	namebuf[HOSTLEN];
	int	count = aconf->port;
	char	*start = name;

	if (count <= 0 || count > 5)
		return start;

	while (count-- && name)
	    {
		name++;
		name = (char *)index(name, '.');
	    }
	if (!name)
		return start;

	namebuf[0] = '*';
	(void)strncpy(&namebuf[1], name, HOSTLEN - 1);
	namebuf[HOSTLEN - 1] = '\0';

	return namebuf;
}

/*
** exit_client
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
**
** aClient *cptr = the local clients originating the exit, or NULL if this
**                 exit is generated by this server for internal reasons.
**                 This will not get any of the generated messages.
** aClient *sptr = Client exiting
** aClient *from = Client firing off this Exit, never NULL!
** char *comment = Reason for the exit
*/
int	exit_client(aClient *cptr, aClient *sptr, aClient *from, char *comment)
{
	aClient	*acptr;
	aClient	*next;
#ifdef	FNAME_USERLOG
	time_t	on_for;
#endif
	char	comment1[HOSTLEN + HOSTLEN + 2 + 255];

	if (MyConnect(sptr))
	    {
		ClientFlags(sptr) |= FLAGS_CLOSING;
                if (IsPerson(sptr))
                 sendto_umode(FLAGSET_CLIENT,"*** Notice -- Client exiting: %s (%s@%s) [%s]", 
                  sptr->name, sptr->user->username, sptr->user->host, comment);
		current_load_data.conn_count--;
		if (IsPerson(sptr)) {
		  char mydom_mask[HOSTLEN + 1];
		  mydom_mask[0] = '*';
		  strncpy(&mydom_mask[1], DOMAINNAME, HOSTLEN - 1);
		  current_load_data.client_count--;
		  if (match(mydom_mask, sptr->sockhost) == 0)
		    current_load_data.local_count--;
		}
		update_load();
		on_for = NOW - sptr->firsttime;
		if (IsPerson(sptr)) {
                        /* poof goes their watchlist! */
                        hash_del_watch_list(sptr);

			tolog(LOG_USER,	"Client Exiting %s (%3d:%02d:%02d): %s@%s",
					myctime(sptr->firsttime),
					on_for / 3600, (on_for % 3600)/60,
					on_for % 60,
					sptr->user->username, sptr->user->host
					);
		}

		if (sptr->fd >= 0 && !IsConnecting(sptr))
		    {
		      if (cptr != NULL && sptr != cptr)
			sendto_one(sptr, "ERROR :Closing Link: %s %s (%s)",
				   get_client_name(sptr,FALSE),
				   cptr->name, comment);
		      else
			sendto_one(sptr, "ERROR :Closing Link: %s (%s)",
				   get_client_name(sptr,FALSE), comment);
		    }
		/*
		** Currently only server connections can have
		** depending remote clients here, but it does no
		** harm to check for all local clients. In
		** future some other clients than servers might
		** have remotes too...
		**
		** Close the Client connection first and mark it
		** so that no messages are attempted to send to it.
		** (The following *must* make MyConnect(sptr) == FALSE!).
		** It also makes sptr->from == NULL, thus it's unnecessary
		** to test whether "sptr != acptr" in the following loops.
		*/
		close_connection(sptr);
		/*
		** First QUIT all NON-servers which are behind this link
		**
		** Note	There is no danger of 'cptr' being exited in
		**	the following loops. 'cptr' is a *local* client,
		**	all dependants are *remote* clients.
		*/

		/* This next bit is a a bit ugly but all it does is take the
		** name of us.. me.name and tack it together with the name of
		** the server sptr->name that just broke off and puts this
		** together into exit_one_client() to provide some useful
		** information about where the net is broken.      Ian 
		*/
		(void)strcpy(comment1, me.name);
		(void)strcat(comment1," ");
		(void)strcat(comment1, sptr->name);
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (!IsServer(acptr) && acptr->from == sptr)
				exit_one_client(NULL, acptr, &me, comment1);
		    }
		/*
		** Second SQUIT all servers behind this link
		*/
		for (acptr = client; acptr; acptr = next)
		    {
			next = acptr->next;
			if (IsServer(acptr) && acptr->from == sptr)
				exit_one_client(NULL, acptr, &me, me.name);
		    }
	    }

	exit_one_client(cptr, sptr, from, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
static	void	exit_one_client(aClient *cptr, aClient *sptr, aClient *from, char *comment)
{
	aClient *acptr;
	int	i;
	Link	*lp;

	/*
	**  For a server or user quitting, propagage the information to
	**  other servers (except to the one where is came from (cptr))
	*/
	if (IsMe(sptr))
	    {
		sendto_ops("ERROR: tried to exit me! : %s", comment);
		return;	/* ...must *never* exit self!! */
	    }
	else if (IsServer(sptr)) {
	 /*
	 ** Old sendto_serv_but_one() call removed because we now
	 ** need to send different names to different servers
	 ** (domain name matching)
	 */
	 	for (i = 0; i <= highest_fd; i++)
		    {
			aConfItem *aconf;

			if (!(acptr = local[i]) || !IsServer(acptr) ||
			    acptr == cptr || IsMe(acptr))
				continue;
			if ((aconf = acptr->serv->nline) &&
			    (match(my_name_for_link(me.name, aconf),
				     sptr->name) == 0))
				continue;
			/*
			** SQUIT going "upstream". This is the remote
			** squit still hunting for the target. Use prefixed
			** form. "from" will be either the oper that issued
			** the squit or some server along the path that
			** didn't have this fix installed. --msa
			*/
			if (sptr->from == acptr)
			    {
				sendto_one(acptr, ":%s SQUIT %s :%s",
					   from->name, sptr->name, comment);
			    }
			else
			    {
				sendto_one(acptr, "SQUIT %s :%s",
					   sptr->name, comment);
			    }
	    }
	} else if (!IsPerson(sptr))
				    /* ...this test is *dubious*, would need
				    ** some thougth.. but for now it plugs a
				    ** nasty hole in the server... --msa
				    */
		; /* Nothing */
	else if (sptr->name[0]) /* ...just clean all others with QUIT... */
	    {
		/*
		** If this exit is generated from "m_kill", then there
		** is no sense in sending the QUIT--KILL's have been
		** sent instead.
		*/
		if ((ClientFlags(sptr) & FLAGS_KILLED) == 0)
		    {
			sendto_serv_butone(cptr,":%s QUIT :%s",
					   sptr->name, comment);
		    }
		/*
		** If a person is on a channel, send a QUIT notice
		** to every client (person) on the same channel (so
		** that the client can show the "**signoff" message).
		** (Note: The notice is to the local clients *only*)
		*/
		if (sptr->user)
		    {
			sendto_common_channels(sptr, ":%s QUIT :%s",
						sptr->name, comment);

			while ((lp = sptr->user->channel))
				remove_user_from_channel(sptr,lp->value.chptr);

			/* Clean up invitefield */
			while ((lp = sptr->user->invited))
				del_invite(sptr, lp->value.chptr);
				/* again, this is all that is needed */

			/* Clean up silencefield */
			while ((lp = sptr->user->silence))
				(void)del_silence(sptr, lp->value.cp);
		    }
	    }

	/* Remove sptr from the client list */
	if (del_from_client_hash_table(sptr->name, sptr) != 1)
		Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
			sptr, sptr->name,
			sptr->from ? sptr->from->sockhost : "??host",
			sptr->from, sptr->next, sptr->prev, sptr->fd,
			sptr->status, sptr->user));
        hash_check_watch(sptr, RPL_LOGOFF);
	remove_client_from_list(sptr);
	return;
}

void	initstats()
{
	bzero((char *)&ircst, sizeof(ircst));
}

void	tstats(aClient *cptr, char *name)
{
	aClient	*acptr;
	int	i;
	struct stats *sp, tmp;
	time_t	now = NOW;

	sp = &tmp;
	bcopy((char *)ircstp, (char *)sp, sizeof(*sp));
	for (i = 0; i < MAXCONNECTIONS; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsServer(acptr))
		    {
			sp->is_sbs += acptr->sendB;
			sp->is_sbr += acptr->receiveB;
			sp->is_sks += acptr->sendK;
			sp->is_skr += acptr->receiveK;
			sp->is_sti += now - acptr->firsttime;
			sp->is_sv++;
			if (sp->is_sbs > 1023)
			    {
				sp->is_sks += (sp->is_sbs >> 10);
				sp->is_sbs &= 0x3ff;
			    }
			if (sp->is_sbr > 1023)
			    {
				sp->is_skr += (sp->is_sbr >> 10);
				sp->is_sbr &= 0x3ff;
			    }
		    }
		else if (IsClient(acptr))
		    {
			sp->is_cbs += acptr->sendB;
			sp->is_cbr += acptr->receiveB;
			sp->is_cks += acptr->sendK;
			sp->is_ckr += acptr->receiveK;
			sp->is_cti += now - acptr->firsttime;
			sp->is_cl++;
			if (sp->is_cbs > 1023)
			    {
				sp->is_cks += (sp->is_cbs >> 10);
				sp->is_cbs &= 0x3ff;
			    }
			if (sp->is_cbr > 1023)
			    {
				sp->is_ckr += (sp->is_cbr >> 10);
				sp->is_cbr &= 0x3ff;
			    }
		    }
		else if (IsUnknown(acptr))
			sp->is_ni++;
	    }

	sendto_one(cptr, ":%s %d %s :accepts %u refused %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_ac, sp->is_ref);
	sendto_one(cptr, ":%s %d %s :unknown commands %u prefixes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_unco, sp->is_unpf);
	sendto_one(cptr, ":%s %d %s :nick collisions %u unknown closes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_kill, sp->is_ni);
	sendto_one(cptr, ":%s %d %s :wrong direction %u empty %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_wrdi, sp->is_empt);
	sendto_one(cptr, ":%s %d %s :numerics seen %u mode fakes %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_num, sp->is_fake);
	sendto_one(cptr, ":%s %d %s :local connections %u udp packets %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_loc, sp->is_udp);
	sendto_one(cptr, ":%s %d %s :Client Server",
		   me.name, RPL_STATSDEBUG, name);
	sendto_one(cptr, ":%s %d %s :connected %u %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_cl, sp->is_sv);
	sendto_one(cptr, ":%s %d %s :bytes sent %u.%uK %u.%uK",
		   me.name, RPL_STATSDEBUG, name,
		   sp->is_cks, sp->is_cbs, sp->is_sks, sp->is_sbs);
	sendto_one(cptr, ":%s %d %s :bytes recv %u.%uK %u.%uK",
		   me.name, RPL_STATSDEBUG, name,
		   sp->is_ckr, sp->is_cbr, sp->is_skr, sp->is_sbr);
	sendto_one(cptr, ":%s %d %s :time connected %u %u",
		   me.name, RPL_STATSDEBUG, name, sp->is_cti, sp->is_sti);
}


int set_hurt(aClient *acptr, const char *from, int ht)
{
  if (!acptr || !from)
    return -1;

#ifdef  KEEP_HURTBY
  if (acptr->user && acptr->user->hurtby)
    {
      irc_ree(acptr->user->hurtby);
      acptr->user->hurtby = NULL;
    }
  if (acptr->user)
    acptr->user->hurtby = irc_strdup(from);
#endif

  SetHurt(acptr);

  if (ht)
    acptr->hurt = ht;

  return 0;
}

int remove_hurt(aClient *acptr)
{
       if (acptr == NULL)
	       return -1;

#ifdef  KEEP_HURTBY
       if (acptr->user && acptr->user->hurtby) {
	       irc_free(acptr->user->hurtby);
	       acptr->user->hurtby = NULL;
       }
#endif

        if (IsHurt(acptr) && MyClient(acptr) && acptr->hurt > 5)
		sendto_one(acptr, ":%s NOTICE %s :You are no longer silenced and now again have full command access.",
			   me.name, acptr->name);
        ClearHurt(acptr);
        acptr->hurt = 0;
	return 0;
}

#ifdef IRC_LOGGING
static int slogfiles[LOG_HI+3];
static char *logfile_n[] = {
	FNAME_OPERLOG,
	FNAME_USERLOG,
	FNAME_IRCDLOG,
	NULL,
	NULL,
	NULL,
	NULL
};
#endif

void
open_logs(void)
{
#ifdef IRC_LOGGING
	int i;

	for (i = 0 ; i <= LOG_HI ; i++) {
		if (!logfile_n[i]) {
			slogfiles[i] = -1;
			continue;
		}
		slogfiles[i] = open(logfile_n[i],
				    O_WRONLY | O_APPEND | O_NONBLOCK);
	}
#endif
}

/* descriptor used for logging ? */
int LogFd(int descriptor)
{
#ifdef IRC_LOGGING
	int i;

	if (descriptor < 0)
	{
		return 0;
	}
	for (i = 0 ; i < LOG_HI; i++)
	{
		if (slogfiles[i] == descriptor)
		{
			return 1;
		}
	}
#endif
	return 0;
}

void
close_logs(void)
{
#ifdef IRC_LOGGING
	int i;

	for (i = 0 ; i <= LOG_HI && logfile_n[i] ; i++)
	{
		if (slogfiles[i] < 0)
		{
			continue;
		}
		close(slogfiles[i]);
		slogfiles[i] = -1;
	}
#endif
}

int tolog( int logtype, char *fmt, ... )
{
	int orig_logtype = logtype;
	static char buf[8192] = "";
	va_list args;

#ifdef IRC_LOGGING
        va_start(args, fmt);

        if (logtype == LOG_NET)
            logtype = LOG_OPER;

	if ((logtype < 0 || logtype >= LOG_HI) || (orig_logtype != LOG_NET && slogfiles[logtype] < 0))
               return -1;
        sprintf(buf, "%.25s: ", myctime(NOW));
        vsnprintf(buf + strlen(buf), 8160, fmt, args);
        va_end(args);
        if (orig_logtype == LOG_NET)
            sendto_umode(U_LOG, "Log -- from %s: %s", me.name, buf);
	strcat(buf, "\n");
        if (slogfiles[logtype] >= 0)
	{
        	write(slogfiles[logtype], buf, strlen(buf));
	}
#else
        if (logtype == LOG_NET)
        {
            va_start(args, fmt);
            vsnprintf(buf, 8190, fmt, args);
            va_end(args);
            sendto_umode(U_LOG, "Log -- from %s: %s", me.name, buf);
        }
	strcat(buf, "\n");
#endif
	if (logtype == LOG_IRCD && (bootopt & BOOT_FORK) == 0)
	{
		fprintf(stderr, buf);
	}
        return 0;
}

int
m_log(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char *message = (parc > 1) ? parv[1] : NULL;

  if (BadPtr(message))
      return 0;
  if (!IsPerson(sptr) && !IsServer(sptr))
      return 0;
  if (MyClient(cptr) && !IsAnOper(cptr))
      return 0;

  sendto_umode(U_LOG, "Log -- from %s: %s", parv[0], message);

  if (IsULine(cptr, sptr))
      tolog(LOG_OPER, "%s", message);
  else if (MyClient(sptr) || IsServer(sptr)) {
      if (IsPerson(sptr) && sptr->user)
          tolog(LOG_OPER, "<%s!%s@%s> %s", parv[0], sptr->user->username, sptr->user->host, message);
      else
          tolog(LOG_OPER, "<%s> %s", parv[0], message);
  }
  else {
      if (IsPerson(sptr) && sptr->user)
          tolog(LOG_OPER, "<%s!%s@%s/%s> %s", parv[0], sptr->user->username, sptr->user->host, sptr->user->server ? sptr->user->server : "", message);
      else
          tolog(LOG_OPER, "<%s> %s", parv[0], message);
  }

  if (MyClient(cptr))
      return 0;

  sendto_serv_butone(sptr, ":%s LOG :%s", parv[0], message);

  return 0;
}
