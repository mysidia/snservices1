/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_bsd.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

#include "ircd.h"

#include <sys/file.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <arpa/nameser.h>
#include <resolv.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef SOL20
int gethostname(char *name, int namelen);
void setlinebuf(FILE *iop);
#endif

#include "res.h"
#include "patchlevel.h"
#include "inet.h"

#include "ircd/res.h"

IRCD_SCCSID("@(#)s_bsd.c	2.78 2/7/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET	0x7f
#endif

int	readcalls = 0;

int	completed_connection(aClient *);
static	int	check_init(aClient *, char *);
extern	int LogFd(int descriptor);

static	char	readbuf[8192];
static	char	addrbuf[40];
char zlinebuf[BUFSIZE];
extern	char *version;

/*
** add_local_domain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
*/

void	add_local_domain(hname, size)
char	*hname;
int	size;
{
#ifdef RES_INIT
	/* try to fix up unqualified names */
	if (!index(hname, '.'))
	    {
		if (!(_res.options & RES_INIT))
		    {
			Debug((DEBUG_DNS,"res_init()"));
			res_init();
		    }
		if (_res.defdname[0])
		    {
			(void)strncat(hname, ".", size-1);
			(void)strncat(hname, _res.defdname, size-2);
		    }
	    }
#endif
	return;
}

/*
** Cannot use perror() within daemon. stderr is closed in
** ircd and cannot be used. And, worse yet, it might have
** been reassigned to a normal connection...
*/

/*
 * add_listener
 *
 * Create a new client which is essentially the stub like 'me' to be used
 * for a socket that is passive (listen'ing for connections to be accepted).
 */
aClient	*add_listener(aconf)
aConfItem *aconf;
{
	aClient	*cptr;

	cptr = make_client(NULL);
	cptr->hopcount = 0;
        ClientFlags(cptr) = FLAGS_LISTEN;
	cptr->acpt = cptr;
	SetMe(cptr);
	strncpyzt(cptr->name, aconf->host, sizeof(cptr->name));

	cptr->sock = network_listen(aconf);
	if (cptr->sock == NULL)
	{
		free_client(cptr);
		return NULL;
	}
	cptr->sock->data = cptr;

        sprintf(cptr->sockhost, "%-.42s.%u", aconf->host, aconf->port);
	cptr->confs = make_link();
	cptr->confs->next = NULL;
	cptr->confs->value.aconf = aconf;
	return cptr;
}

/*
 * close_listeners
 *
 * Close and free all clients which are marked as having their socket open
 * and in a state where they can accept connections.
 */
void	close_listeners()
{
	aClient	*cptr;
	aConfItem *aconf;

	/*
	 * close all 'extra' listening ports we have
	 */
	for (cptr = &me; cptr; cptr = cptr->lnext) {
		if (!IsMe(cptr) || cptr == &me || !IsListening(cptr))
			continue;
		aconf = cptr->confs->value.aconf;

		if (IsIllegal(aconf) && aconf->clients == 0)
			close_connection(cptr);
	}
}

/*
 * init_sys
 */
void	init_sys()
{
	int	fd;
	struct rlimit limit;

	if (!getrlimit(RLIMIT_NOFILE, &limit)) {
		if (limit.rlim_max < MAXCONNECTIONS) {
			(void)fprintf(stderr, "ircd fd table too big\n");
			(void)fprintf(stderr, "Hard Limit: %d IRC max: %d\n",
				      (int)limit.rlim_max, MAXCONNECTIONS);
			(void)fprintf(stderr,"Fix MAXCONNECTIONS\n");
			exit(-1);
		}
		limit.rlim_cur = limit.rlim_max; /* make soft limit the max */
		if (setrlimit(RLIMIT_NOFILE, &limit) == -1) {
			(void)fprintf(stderr,"error setting max fd's to %d\n",
				      (int)limit.rlim_cur);
			exit(-1);
		}
	}

	(void)setlinebuf(stderr);

	for (fd = 3; fd < MAXCONNECTIONS; fd++) {
		if (LogFd(fd))
			continue;
		(void)close(fd);
	}
	(void)close(1); /* -- allow output a little more yet */

	if ((bootopt & BOOT_FORK) != 0) {
		if (fork())
			exit(0);
		fprintf(stderr, "fork() successful, now in child.\n");
	}

	return;
}

void	write_pidfile()
{
#ifdef IRCD_PIDFILE
	int fd;
	char buff[20];
	if ((fd = open(IRCD_PIDFILE, O_CREAT|O_WRONLY, 0600))>=0)
	    {
		bzero(buff, sizeof(buff));
		(void)sprintf(buff,"%5d\n", (int)getpid());
		if (write(fd, buff, strlen(buff)) == -1)
			Debug((DEBUG_NOTICE,"Error writing to pid file %s",
			      IRCD_PIDFILE));
		(void)close(fd);
		return;
	    }
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE,"Error opening pid file %s",
			IRCD_PIDFILE));
#endif
#endif
}

/*
 * Initialize the various name strings used to store hostnames. This is set
 * from either the server's sockhost (if client fd is a tty or localhost)
 * or from the ip# converted into a string. 0 = success, -1 = fail.
 */
static int check_init(aClient *cptr, char *sockn)
{
	strcpy(sockn, address_tostring(cptr->sock->raddr, 0));
	return 0;
}

/*
 * Ordinary client access check. Look for conf lines which have the same
 * status as the flags passed.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int	check_client(cptr)
aClient	*cptr;
{
	static	char	sockname[HOSTLEN+1];
	struct	HostEnt *hp = NULL;
	int	i;

	ClearAccess(cptr);
	Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]",
		cptr->name, address_tostring(cptr->sock->raddr, 0)));

	if (check_init(cptr, sockname))
		return -2;

	hp = cptr->hostp;

	/*
	 * Verify that the host to ip mapping is correct both ways and that
	 * the ip#(s) for the socket is listed for the host.
	 */
	if (hp)
	    {
		for (i = 0; hp->h_addr_list[i]; i++)
			if (!address_compare(hp->h_addr_list[i], cptr->sock->raddr))
				break;
		if (!hp->h_addr_list[i])
		    {
			strcpy((char *)&addrbuf, address_tostring(cptr->sock->raddr, 0));
			sendto_ops("IP# Mismatch: %s != %s[%s]",
				   addrbuf, hp->h_name,
				   address_tostring(hp->h_addr_list[0], 0));
			hp = NULL;
		    }
	    }

	if ((i = attach_Iline(cptr, hp, sockname)))
	    {
		Debug((DEBUG_DNS,"ch_cl: access denied: %s[%s]",
			cptr->name, sockname));
		return i;
	    }

	Debug((DEBUG_DNS, "ch_cl: access ok: %s[%s]",
		cptr->name, sockname));

	return 0;
}

#define	CFLAG	CONF_CONNECT_SERVER
#define	NFLAG	CONF_NOCONNECT_SERVER
/*
 * check_server_init(), check_server()
 *	check access for a server given its name (passed in cptr struct).
 *	Must check for all C/N lines which have a name which matches the
 *	name given and a host which matches. A host alias which is the
 *	same as the server name is also acceptable in the host field of a
 *	C/N line.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int	check_server_init(cptr)
aClient	*cptr;
{
	char	*name;
	aConfItem *c_conf = NULL, *n_conf = NULL;
	struct	HostEnt	*hp = NULL;
	Link	*lp;

	name = cptr->name;

	Debug((DEBUG_DNS, "sv_cl: check access for %s[%s]", name, cptr->sockhost));

	if (IsUnknown(cptr) && !attach_confs(cptr, name, CFLAG|NFLAG))
	{
	  Debug((DEBUG_DNS,"No C/N lines for %s", name));
	  return -1;
	}

	lp = cptr->confs;

	/*
	 * We initiated this connection so the client should have a C and N
	 * line already attached after passing through the connec_server()
	 * function earlier.
	 */
	if (IsConnecting(cptr) || IsHandshake(cptr))
	{
		c_conf = find_conf(lp, name, CFLAG);
		n_conf = find_conf(lp, name, NFLAG);
		if (!c_conf || !n_conf)
		{
			sendto_ops("Connecting Error: %s[%s]", name, cptr->sockhost);
			det_confs_butmask(cptr, 0);
			return -1;
		}
	}

	/*
	** If the servername is a hostname, either an alias (CNAME) or
	** real name, then check with it as the host. Use gethostbyname()
	** to check for servername as hostname.
	*/
	if (!cptr->hostp)
	    {
		aConfItem *aconf;

		aconf = count_cnlines(lp);
		if (aconf)
		    {
			char	*s;
			Link	lin;

			/*
			** Do a lookup for the CONF line *only* and not
			** the server connection else we get stuck in a
			** nasty state since it takes a SERVER message to
			** get us here and we cant interrupt that very
			** well.
			*/
			ClearAccess(cptr);
			lin.value.aconf = aconf;
			lin.flags = ASYNC_CONF;
			nextdnscheck = 1;
			if ((s = index(aconf->host, '@')))
				s++;
			else
				s = aconf->host;
			Debug((DEBUG_DNS,"sv_ci:cache lookup (%s)",s));
			hp = gethost_byname(s, cptr->sock->raddr->addr->sa_family, &lin);
		    }
	    }
	return check_server(cptr, hp, c_conf, n_conf, 0);
}

int	check_server(cptr, hp, c_conf, n_conf, estab)
aClient	*cptr;
aConfItem	*n_conf, *c_conf;
struct	HostEnt	*hp;
int	estab;
{
	char	*name;
	char	abuff[HOSTLEN+USERLEN+2];
	char	sockname[HOSTLEN+1], fullname[HOSTLEN+1];
	Link	*lp = cptr->confs;
	int	i;

	ClearAccess(cptr);
	if (check_init(cptr, sockname))
		return -2;

check_serverback:

	if (hp)
	    {
		for (i = 0; hp->h_addr_list[i]; i++)
			if (!address_compare(hp->h_addr_list[i], cptr->sock->raddr))
				break;
		if (!hp->h_addr_list[i])
		    {
			strcpy((char *)&addrbuf, address_tostring(cptr->sock->raddr, 0));
			sendto_ops("IP# Mismatch: %s != %s[%s]",
				   addrbuf, hp->h_name,
				   address_tostring(hp->h_addr_list[0], 0));
			hp = NULL;
		    }
	    }
	else if (cptr->hostp)
	    {
		hp = cptr->hostp;
		goto check_serverback;
	    }

	if (hp)
		/*
		 * if we are missing a C or N line from above, search for
		 * it under all known hostnames we have for this ip#.
		 */
		for (i = 0,name = hp->h_name; name ; name = hp->h_aliases[i++])
		    {
			strncpyzt(fullname, name, sizeof(fullname));
			add_local_domain(fullname, HOSTLEN-strlen(fullname));
			Debug((DEBUG_DNS, "sv_cl: gethostbyaddr: %s->%s",
				sockname, fullname));
			(void)sprintf(abuff, "%s@%s", cptr->username, fullname);
			if (!c_conf)
				c_conf = find_conf_host(lp, abuff, CFLAG);
			if (!n_conf)
				n_conf = find_conf_host(lp, abuff, NFLAG);
			if (c_conf && n_conf)
			    {
				get_sockhost(cptr, fullname);
				break;
			    }
		    }
	name = cptr->name;

	/*
	 * Check for C and N lines with the hostname portion the ip number
	 * of the host the server runs on. This also checks the case where
	 * there is a server connecting from 'localhost'.
	 */
	if (IsUnknown(cptr) && (!c_conf || !n_conf))
	    {
		(void)sprintf(abuff, "%s@%s", cptr->username, sockname);
		if (!c_conf)
			c_conf = find_conf_host(lp, abuff, CFLAG);
		if (!n_conf)
			n_conf = find_conf_host(lp, abuff, NFLAG);
	    }
	/*
	 * Attach by IP# only if all other checks have failed.
	 * It is quite possible to get here with the strange things that can
	 * happen when using DNS in the way the irc server does. -avalon
	 */
	if (!hp)
	{
	    if (!c_conf)
	      c_conf = find_conf_ip(lp, cptr->sock->raddr, cptr->username, CFLAG);
	    if (!n_conf)
	      n_conf = find_conf_ip(lp, cptr->sock->raddr, cptr->username, NFLAG);
	}
	else
	  for (i = 0; hp->h_addr_list[i]; i++)
	  {
	      if (!c_conf)
		c_conf = find_conf_ip(lp, hp->h_addr_list[i], cptr->username, CFLAG);
	      if (!n_conf)
		n_conf = find_conf_ip(lp, hp->h_addr_list[i], cptr->username, NFLAG);
	  }
	/*
	 * detach all conf lines that got attached by attach_confs()
	 */
	det_confs_butmask(cptr, 0);
	/*
	 * if no C or no N lines, then deny access
	 */
	if (!c_conf || !n_conf)
	    {
		get_sockhost(cptr, sockname);
		Debug((DEBUG_DNS, "sv_cl: access denied: %s[%s@%s] c %x n %x",
			name, cptr->username, cptr->sockhost,
			c_conf, n_conf));
		return -1;
	    }
	/*
	 * attach the C and N lines to the client structure for later use.
	 */
	(void)attach_conf(cptr, n_conf);
	(void)attach_conf(cptr, c_conf);
	(void)attach_confs(cptr, name, CONF_HUB|CONF_LEAF|CONF_UWORLD);

	if (c_conf->addr == NULL)
		c_conf->addr = address_copy(cptr->sock->raddr);

	get_sockhost(cptr, c_conf->host);

	Debug((DEBUG_DNS,"sv_cl: access ok: %s[%s]",
		name, cptr->sockhost));
	if (estab)
		return m_server_estab(cptr);
	return 0;
}
#undef	CFLAG
#undef	NFLAG

/*
** completed_connection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
int completed_connection(cptr)
aClient	*cptr;
{
	aConfItem *aconf;

	SetHandshake(cptr);

	aconf = find_conf(cptr->confs, cptr->name, CONF_CONNECT_SERVER);
	if (!aconf)
	    {
		sendto_ops("Lost C-Line for %s", get_client_name(cptr,FALSE));
		return -1;
	    }
	if (!BadPtr(aconf->passwd))
		sendto_one(cptr, "PASS :%s", aconf->passwd);

	aconf = find_conf(cptr->confs, cptr->name, CONF_NOCONNECT_SERVER);
	if (!aconf)
	    {
		sendto_ops("Lost N-Line for %s", get_client_name(cptr,FALSE));
		return -1;
	    }
	sendto_one(cptr, "SERVER %s 1 :%s",
		   my_name_for_link(me.name, aconf), me.info);

	socket_monitor(cptr->sock, MONITOR_READ, network_read_handler);

	return (IsDead(cptr)) ? -1 : 0;
}

/*
** close_connection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
void	close_connection(cptr)
aClient *cptr;
{
        aConfItem *aconf;

	if (IsServer(cptr))
	    {
		ircstp->is_sv++;
		ircstp->is_sbs += cptr->sendB;
		ircstp->is_sbr += cptr->receiveB;
		ircstp->is_sks += cptr->sendK;
		ircstp->is_skr += cptr->receiveK;
		ircstp->is_sti += NOW - cptr->firsttime;
		if (ircstp->is_sbs > 1023)
		    {
			ircstp->is_sks += (ircstp->is_sbs >> 10);
			ircstp->is_sbs &= 0x3ff;
		    }
		if (ircstp->is_sbr > 1023)
		    {
			ircstp->is_skr += (ircstp->is_sbr >> 10);
			ircstp->is_sbr &= 0x3ff;
		    }
	    }
	else if (IsClient(cptr))
	    {
		ircstp->is_cl++;
		ircstp->is_cbs += cptr->sendB;
		ircstp->is_cbr += cptr->receiveB;
		ircstp->is_cks += cptr->sendK;
		ircstp->is_ckr += cptr->receiveK;
		ircstp->is_cti += NOW - cptr->firsttime;
		if (ircstp->is_cbs > 1023)
		    {
			ircstp->is_cks += (ircstp->is_cbs >> 10);
			ircstp->is_cbs &= 0x3ff;
		    }
		if (ircstp->is_cbr > 1023)
		    {
			ircstp->is_ckr += (ircstp->is_cbr >> 10);
			ircstp->is_cbr &= 0x3ff;
		    }
	    }
	else
		ircstp->is_ni++;

	/*
	 * remove outstanding DNS queries.
	 */
	del_queries((char *)cptr);
	/*
	 * If the connection has been up for a long amount of time, schedule
	 * a 'quick' reconnect, else reset the next-connect cycle.
	 *
	 * Now just hold on a minute.  We're currently doing this when a
	 * CLIENT exits too?  I don't think so!  If its not a server, or
	 * the SQUIT flag has been set, then we don't schedule a fast
	 * reconnect.  Pisses off too many opers. :-)  -Cabal95
	 */
	if (IsServer(cptr) && !(ClientFlags(cptr) & FLAGS_SQUIT) &&
	    (aconf = find_conf_exact(cptr->name, cptr->username,
				    cptr->sockhost, CONF_CONNECT_SERVER)))
	    {
		/*
		 * Reschedule a faster reconnect, if this was a automaticly
		 * connected configuration entry. (Note that if we have had
		 * a rehash in between, the status has been changed to
		 * CONF_ILLEGAL). But only do this if it was a "good" link.
		 */
		aconf->hold = NOW;
		aconf->hold += (aconf->hold - cptr->since > HANGONGOODLINK) ?
				HANGONRETRYDELAY : ConfConFreq(aconf);
		if (nextconnect > aconf->hold)
			nextconnect = aconf->hold;
	    }

	if (cptr->sock != NULL)
	    {
		flush_connections(cptr);
		socket_close(cptr->sock);
		cptr->sock = NULL;
		DBufClear(&cptr->sendQ);
		DBufClear(&cptr->recvQ);
		bzero(cptr->passwd, sizeof(cptr->passwd));
		/*
		 * clean up extra sockets from P-lines which have been
		 * discarded.
		 */
		if (cptr->acpt != &me && cptr->acpt != cptr)
		    {
			aconf = cptr->acpt->confs->value.aconf;
			if (aconf->clients > 0)
				aconf->clients--;
			if (!aconf->clients && IsIllegal(aconf))
				close_connection(cptr->acpt);
		    }
	    }

	det_confs_butmask(cptr, 0);

	/* MyConnect() should be false as there's no connection anymore */
	cptr->hopcount = -1;

	/* and for the same reason, this client should be removed from
	 * the linked list of local clients */
	cptr->lprev->lnext = cptr->lnext;
	if (cptr->lnext)
	{
		cptr->lnext->lprev = cptr->lprev;
	}

	return;
}

/*
 * Creates a client which has just connected to us on the given fd.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yuet since it doesnt have a name.
 */
aClient	*add_connection(aClient *cptr, sock *sock)
{
	Link	lin;
	aClient *acptr;
	aConfItem *aconf = NULL;

	acptr = make_client(NULL);
	acptr->hopcount = 1;

	if (cptr != &me)
		aconf = cptr->confs->value.aconf;

	acptr->sock = sock;
	sock->data = acptr;

	if (aconf && IsIllegal(aconf))
	{
		socket_close(acptr->sock);
		acptr->sock = NULL;
		free_client(acptr);
		return NULL;
	}

	/* Copy ascii address to 'sockhost' just in case. Then we
	 * have something valid to put into error messages...
	 */
	get_sockhost(acptr, address_tostring(acptr->sock->raddr, 0));

	/* Check for zaps -- Barubary */
	if (find_zap(acptr, 0))
	{
		socket_write(acptr->sock, zlinebuf, strlen(zlinebuf));
		socket_close(acptr->sock);
		acptr->sock = NULL;
		free_client(acptr);
		return NULL;
	}

	sendto_one(acptr, ":%s NOTICE AUTH :*** Hello, you are connecting to %s, the progress of your connection follows", me.name, me.name);
#ifndef URL_CONNECTHELP
	sendto_one(acptr, ":%s NOTICE AUTH :*** If you experience problems connecting, you may wish to try reconnecting with /server irc.sorcery.net 9000", me.name);
#else
	sendto_one(acptr, ":%s NOTICE AUTH :*** If you experience problems connecting, you may want to check %s", me.name, URL_CONNECTHELP);
#endif
	sendto_one(acptr, ":%s NOTICE AUTH :" REPORT_START_DNS "", me.name);
	lin.flags = ASYNC_CLIENT;
	lin.value.cptr = acptr;
	Debug((DEBUG_DNS, "lookup %s", address_tostring(acptr->sock->raddr, 0)));
	acptr->hostp = gethost_byaddr(acptr->sock->raddr, &lin);
	if (!acptr->hostp)
	{
		SetDNS(acptr);
	}
        else
	{
		connotice(acptr, REPORT_DONEC_DNS);
		socket_monitor(acptr->sock, MONITOR_READ, network_read_handler);
	}
	socket_monitor(acptr->sock, MONITOR_ERROR, network_error_handler);
	nextdnscheck = 1;

	if (aconf)
	{
		aconf->clients++;
	}
	acptr->acpt = cptr;
	add_client_to_list(acptr);

	return acptr;
}

/*
** read_packet
**
** Read a 'packet' of data from a connection and process it.  Read in 8k
** chunks to give a better performance rating (for server connections).
** Do some tricky stuff for client connections to make sure they don't do
** any flooding >:-) -avalon
*/
int	read_packet(aClient *cptr)
{
	int	dolen = 0, length = 0, done;
	time_t	now = NOW;

	if (!(IsPerson(cptr) && DBufLength(&cptr->recvQ) > 6090))
	{
		errno = 0;

		length = socket_read(cptr->sock, readbuf, sizeof(readbuf));
		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		ClientFlags(cptr) &= ~(FLAGS_NONL|FLAGS_PINGSENT);
		if (length <= 0)
		{
			return length;
		}
	}

	/*
	** For server connections, we process as many as we can without
	** worrying about the time of day or anything :)
	*/
	if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr) || IsService(cptr))
	    {
		if (length > 0)
			if ((done = dopacket(cptr, readbuf, length)))
				return done;
	    }
	else
	    {
		/*
		** Before we even think of parsing what we just read, stick
		** it on the end of the receive queue and do it when its
		** turn comes around.
		*/
		if (dbuf_put(&cptr->recvQ, readbuf, length) < 0)
			return exit_client(cptr, cptr, cptr, "dbuf_put fail");

		if (IsPerson(cptr) &&
		    DBufLength(&cptr->recvQ) > CLIENT_FLOOD)
		    {
			sendto_umode(FLAGSET_FLOOD,
				"*** Flood -- %s!%s@%s (%d) exceeds %d recvQ",
				cptr->name[0] ? cptr->name : "*",
				cptr->user ? cptr->user->username : "*",
				cptr->user ? cptr->user->host : "*",
				DBufLength(&cptr->recvQ), CLIENT_FLOOD);
			return exit_client(cptr, cptr, cptr, "Excess Flood");
		    }

		while (DBufLength(&cptr->recvQ) && !NoNewLine(cptr) &&
		       ((cptr->status < STAT_UNKNOWN) ||
			(cptr->since - now < 10)))
		    {
			/*
			** If it has become registered as a Service or Server
			** then skip the per-message parsing below.
			*/
			if (IsService(cptr) || IsServer(cptr))
			    {
				dolen = dbuf_get(&cptr->recvQ, readbuf,
						 sizeof(readbuf));
				if (dolen <= 0)
					break;
				if ((done = dopacket(cptr, readbuf, dolen)))
					return done;
				break;
			    }
			dolen = dbuf_getmsg(&cptr->recvQ, readbuf,
					    sizeof(readbuf));
			/*
			** Devious looking...whats it do ? well..if a client
			** sends a *long* message without any CR or LF, then
			** dbuf_getmsg fails and we pull it out using this
			** loop which just gets the next 512 bytes and then
			** deletes the rest of the buffer contents.
			** -avalon
			*/
			while (dolen <= 0)
			    {
				if (dolen < 0)
					return exit_client(cptr, cptr, cptr,
							   "dbuf_getmsg fail");
				if (DBufLength(&cptr->recvQ) < 510)
				    {
					ClientFlags(cptr) |= FLAGS_NONL;
					break;
				    }
				dolen = dbuf_get(&cptr->recvQ, readbuf, 511);
				if (dolen > 0 && DBufLength(&cptr->recvQ))
					DBufClear(&cptr->recvQ);
			    }

			if (dolen > 0 &&
			    (dopacket(cptr, readbuf, dolen) == FLUSH_BUFFER))
				return FLUSH_BUFFER;
		    }
	    }
	return 1;
}


/*
 * connect_server
 */
int	connect_server(aconf, by, hp)
aConfItem *aconf;
aClient	*by;
struct	HostEnt	*hp;
{
	aClient *cptr, *c2ptr;
	char	*s;

	Debug((DEBUG_NOTICE,"Connect to %s[%s] @%s",
		aconf->name, aconf->host, address_tostring(aconf->addr, 0)));

	if ((c2ptr = find_server(aconf->name, NULL)))
	    {
		sendto_ops("Server %s already present from %s",
			   aconf->name, get_client_name(c2ptr, TRUE));
		if (by && IsPerson(by) && !MyClient(by))
		  sendto_one(by,
                             ":%s NOTICE %s :Server %s already present from %s",
                             me.name, by->name, aconf->name,
			     get_client_name(c2ptr, TRUE));
		return -1;
	    }

	/*
	 * If we dont know the IP# for this host and itis a hostname and
	 * not a ip# string, then try and find the appropriate host record.
	 */
	if (aconf->addr == NULL) {
	        Link    lin;

		lin.flags = ASYNC_CONNECT;
		lin.value.aconf = aconf;
		nextdnscheck = 1;
		s = (char *)index(aconf->host, '@');
		s++; /* should NEVER be NULL */
		aconf->addr = address_make(s, (aconf->port > 0 ? aconf->port : portnum));
		if (aconf->addr == NULL)
		{
			return 0;
		}
	    }
	cptr = make_client(NULL);
	cptr->hopcount = 1;
	cptr->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strncpyzt(cptr->name, aconf->name, sizeof(cptr->name));
	strncpyzt(cptr->sockhost, aconf->host, HOSTLEN+1);

	cptr->sock = network_connect(aconf);

	if (cptr->sock == NULL)
	{
		sendto_ops("Connect to host %s failed",cptr);
		if (by && IsPerson(by) && !MyClient(by))
		{
			sendto_one(by,
				":%s NOTICE %s :Connect to host %s failed.",
				me.name, by->name, cptr);
		}
		free_client(cptr);
		return -1;
	}

	cptr->sock->data = cptr;

        /* Attach config entries to client here rather than in
         * completed_connection. This to avoid null pointer references
         * when name returned by gethostbyaddr matches no C lines
         * (could happen in 2.6.1a when host and servername differ).
         * No need to check access and do gethostbyaddr calls.
         * There must at least be one as we got here C line...  meLazy
         */
        (void)attach_confs_host(cptr, aconf->host,
		       CONF_NOCONNECT_SERVER | CONF_CONNECT_SERVER);

	if (!find_conf_host(cptr->confs, aconf->host, CONF_NOCONNECT_SERVER) ||
	    !find_conf_host(cptr->confs, aconf->host, CONF_CONNECT_SERVER))
	    {
      		sendto_ops("Host %s is not enabled for connecting:no C/N-line",
			   aconf->host);
                if (by && IsPerson(by) && !MyClient(by))
                  sendto_one(by,
                             ":%s NOTICE %s :Connect to host %s failed.",
			     me.name, by->name, cptr);
		det_confs_butmask(cptr, 0);
		socket_close(cptr->sock);
		free_client(cptr);
                return(-1);
	    }
	/*
	** The socket has been connected or connect is in progress.
	*/
	(void)make_server(cptr);
	if (by && IsPerson(by))
	    {
		(void)strcpy(cptr->serv->by, by->name);
		if (cptr->serv->user) free_user(cptr->serv->user, NULL);
		cptr->serv->user = by->user;
		by->user->refcnt++;
	    }
	    else
	    {
		(void)strcpy(cptr->serv->by, "AutoConn.");
		if (cptr->serv->user) free_user(cptr->serv->user, NULL);
		cptr->serv->user = NULL;
	    }
	(void)strcpy(cptr->serv->up, me.name);
	cptr->acpt = &me;
	SetConnecting(cptr);

	get_sockhost(cptr, aconf->host);
	add_client_to_list(cptr);
	nextping = NOW;
	update_time();

	return 0;
}

/*
** find the real hostname for the host running the server (or one which
** matches the server's name) and its primary IP#.  Hostname is stored
** in the client structure passed as a pointer.
*/
void	get_my_name(cptr, name, len)
aClient	*cptr;
char	*name;
int	len;
{
	static	char tmp[HOSTLEN+1];
	struct	hostent	*hp;
	char	*cname = cptr->name;

	if (gethostname(name,len) == -1)
		return;
	name[len] = '\0';

	/* assume that a name containing '.' is a FQDN */
	if (!index(name,'.'))
		add_local_domain(name, len - strlen(name));

	/*
	** If hostname gives another name than cname, then check if there is
	** a CNAME record for cname pointing to hostname. If so accept
	** cname as our name.   meLazy
	*/
	if (BadPtr(cname))
		return;
	if ((hp = gethostbyname(cname)) || (hp = gethostbyname(name)))
	    {
		char	*hname;
		int	i = 0;

		for (hname = hp->h_name; hname; hname = hp->h_aliases[i++])
  		    {
			strncpyzt(tmp, hname, sizeof(tmp));
			add_local_domain(tmp, sizeof(tmp) - strlen(tmp));

			/*
			** Copy the matching name over and store the
			** 'primary' IP# as 'myip' which is used
			** later for making the right one is used
			** for connecting to other hosts.
			*/
			if (!mycmp(me.name, tmp))
				break;
 		    }
		if (mycmp(me.name, tmp))
			strncpyzt(name, hp->h_name, len);
		else
			strncpyzt(name, tmp, len);
		Debug((DEBUG_DEBUG,"local name is %s",
				get_client_name(&me,TRUE)));
	    }
	return;
}

/*
 * do_dns_async
 *
 * Called when the fd returned from init_resolver() has been selected for
 * reading.
 */
void do_dns_async()
{
	static	Link	ln;
	aClient	*cptr;
	aConfItem	*aconf;
	struct	HostEnt	*hp;

	ln.flags = -1;
	hp = get_res((char *)&ln);
	while (hp != NULL)
	{
	    Debug((DEBUG_DNS,"%#x = get_res(%d,%#x)", hp, ln.flags, ln.value.cptr));

	    switch (ln.flags)
	    {
	      case ASYNC_NONE :
		/*
		 * no reply was processed that was outstanding or had a client
		 * still waiting.
		 */
		break;

	      case ASYNC_CLIENT :
		if ((cptr = ln.value.cptr))
		{
			connotice(cptr, REPORT_DONE_DNS);
			socket_monitor(cptr->sock, MONITOR_READ, network_read_handler);
			del_queries((char *)cptr);
			ClearDNS(cptr);
			SetAccess(cptr);
			cptr->hostp = hp;
		}
		break;

	      case ASYNC_CONNECT :
		aconf = ln.value.aconf;
		if (hp && aconf)
		{
			aconf->addr = address_copy(hp->h_addr);
			connect_server(aconf, NULL, hp);
		}
		else
		{
			sendto_ops("Connect to %s failed: host lookup",
			     (aconf) ? aconf->host : "unknown");
		}
		break;

	      case ASYNC_CONF :
		aconf = ln.value.aconf;
		if (hp && aconf)
		{
			aconf->addr = address_copy(hp->h_addr);
		}
		break;

	      case ASYNC_SERVER :
		cptr = ln.value.cptr;
		del_queries((char *)cptr);
		ClearDNS(cptr);

		if (check_server(cptr, hp, NULL, NULL, 1))
		  (void)exit_client(cptr, cptr, &me, "No Authorization");

		break;

	      default :
		break;
	    }

	    ln.flags = -1;
	    hp = get_res((char *)&ln);
	} /* while (hp != NULL) */
}
