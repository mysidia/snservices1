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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#if defined(SOL20)
#include <sys/filio.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef SOL20
int gethostname(char *name, int namelen);
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
#include "numeric.h"
#include "patchlevel.h"
#include "inet.h"
#include "nameser.h"
#include "resolv.h"
#include "h.h"

#include "ircd/res.h"
#include "ircd/send.h"
#include "ircd/string.h"

IRCD_SCCSID("@(#)s_bsd.c	2.78 2/7/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET	0x7f
#endif

aClient	*local[MAXCONNECTIONS];
int	highest_fd = 0, readcalls = 0, udpfd = -1, resfd = -1;
static	anAddress	mysk;
static	void	polludp(void);

static	anAddress *connect_inet(aConfItem *, aClient *, int *);
static	int	completed_connection(aClient *);
static	int	check_init(aClient *, char *);
static	void	do_dns_async(void);
	void	set_sock_opts(int, aClient *);
extern	int LogFd(int descriptor);

static	char	readbuf[8192];
static	char	addrbuf[40];
char zlinebuf[BUFSIZE];
extern	char *version;

/*
 * Try and find the correct name to use with getrlimit() for setting the max.
 * number of files allowed to be open by this process.
 */
#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX   RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   undef RLIMIT_FD_MAX
#  endif
# endif
#endif

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
** report_error
**	This a replacement for perror(). Record error to log and
**	also send a copy to all *LOCAL* opers online.
**
**	text	is a *format* string for outputting error. It must
**		contain only two '%s', the first will be replaced
**		by the sockhost from the cptr, and the latter will
**		be taken from sys_errlist[errno].
**
**	cptr	if not NULL, is the *LOCAL* client associated with
**		the error.
*/
void	report_error(text, cptr)
char	*text;
aClient *cptr;
{
	int	errtmp = errno; /* debug may change 'errno' */
	char	*host;
	int	err, len = sizeof(err);

	host = (cptr) ? get_client_name(cptr, FALSE) : "";

	Debug((DEBUG_ERROR, text, host, strerror(errtmp)));

	/*
	 * Get the *real* error from the socket (well try to anyway..).
	 * This may only work when SO_DEBUG is enabled but its worth the
	 * gamble anyway.
	 */
#ifdef	SO_ERROR
	if (cptr && !IsMe(cptr) && cptr->fd >= 0)
		if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR, (OPT_TYPE *)&err, &len))
			if (err)
				errtmp = err;
#endif
	sendto_ops(text, host, strerror(errtmp));
#ifdef USE_SYSLOG
	syslog(LOG_WARNING, text, host, strerror(errtmp));
#endif
	return;
}

/*
 * inetport
 *
 * Create a socket in the AF_INET domain, bind it to the port given in
 * 'port' and listen to it.  Connections are accepted to this socket
 * depending on the IP# mask given by 'name'.  Returns the fd of the
 * socket created or -1 on error.
 */
int	inetport(cptr, name, port)
aClient	*cptr;
char	*name;
int	port;
{
	static	anAddress server;
	struct	addrinfo *res;
	int	len;

	if (BadPtr(name))
		name = "*";


	if (getaddrinfo(name, NULL, NULL, &res))
	{
		name = "0.0.0.0";
		getaddrinfo(name, NULL, NULL, &res);
	}
	bcopy(res->ai_addr, &server, res->ai_addrlen);
	freeaddrinfo(res);

	if (cptr != &me)
	    {
		(void)sprintf(cptr->sockhost, "%-.42s.%u",
			name, (unsigned int)port);
		(void)strcpy(cptr->name, me.name);
	    }
	/*
	 * At first, open a new socket
	 */
	if (cptr->fd == -1)
	{
		(void)alarm(2); /* Avoid waiting for 'No more sockets' */
		cptr->fd = socket(server.addr_family, SOCK_STREAM, 0);
		(void)alarm(0);
        }
	if (cptr->fd < 0)
	    {
		report_error("opening stream socket %s:%s", cptr);
		return -1;
	    }
	else if (cptr->fd >= MAXCLIENTS)
	    {
		sendto_ops("No more connections allowed (%s)", cptr->name);
		(void)closesocket(cptr->fd);
		return -1;
	    }
	set_sock_opts(cptr->fd, cptr);
	switch(server.addr_family)
	{
		case AF_INET:
			len = sizeof(struct sockaddr_in);
			break;
		case AF_INET6:
			len = sizeof(struct sockaddr_in6);
			break;
	}
	/*
	 * Bind a port to listen for new connections if port is non-null,
	 * else assume it is already open and try get something from it.
	 */
	if (port)
	    {
		/* per-port bindings, fixes /stats l */
		switch(server.addr_family)
		{
			case AF_INET:
				server.in.sin_port = htons(port);
				break;
			case AF_INET6:
				server.in6.sin6_port = htons(port);
				break;
		}
		/*
		 * Try 10 times to bind the socket with an interval of 20
		 * seconds. Do this so we dont have to keepp trying manually
		 * to bind. Why ? Because a port that has closed often lingers
		 * around for a short time.
		 * This used to be the case.  Now it no longer is.
		 * Could cause the server to hang for too long - avalon
		 */
		if (bind(cptr->fd, (struct sockaddr *)&server,
			len) == -1)
		    {
			report_error("binding stream socket %s:%s", cptr);
			(void)closesocket(cptr->fd);
			return -1;
		    }
	    }
	if (getsockname(cptr->fd, (struct sockaddr *)&server, &len))
	    {
		report_error("getsockname failed for %s:%s",cptr);
		(void)closesocket(cptr->fd);
		return -1;
	    }

	if (cptr == &me) /* KLUDGE to get it work... */
	    {
		char	buf[1024];

		switch(server.addr_family)
		{
			case AF_INET:
				(void)sprintf(buf, rpl_str(RPL_MYPORTIS), me.name, "*",
		    			ntohs(server.in.sin_port));
				break;
			case AF_INET6:
				(void)sprintf(buf, rpl_str(RPL_MYPORTIS), me.name, "*",
		    			ntohs(server.in6.sin6_port));
				break;
		}
		(void)write(0, buf, strlen(buf));
	    }
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	bcopy(&server, &cptr->addr, sizeof(anAddress));
	switch(server.addr_family)
	{
		case AF_INET:
			cptr->port = (int)ntohs(server.in.sin_port);
			break;
		case AF_INET6:
			cptr->port = (int)ntohs(server.in6.sin6_port);
			break;
	}
	(void)listen(cptr->fd, LISTEN_SIZE);
	local[cptr->fd] = cptr;

	return 0;
}

/*
 * add_listener
 *
 * Create a new client which is essentially the stub like 'me' to be used
 * for a socket that is passive (listen'ing for connections to be accepted).
 */
int	add_listener(aconf)
aConfItem *aconf;
{
	aClient	*cptr;

	cptr = make_client(NULL);
        ClientFlags(cptr) = FLAGS_LISTEN;
	cptr->acpt = cptr;
	cptr->from = cptr;
	SetMe(cptr);
	strncpyzt(cptr->name, aconf->host, sizeof(cptr->name));

	if (inetport(cptr, aconf->host, aconf->port))
		cptr->fd = -2;

	if (cptr->fd >= 0)
	    {
		cptr->confs = make_link();
		cptr->confs->next = NULL;
		cptr->confs->value.aconf = aconf;
		set_non_blocking(cptr->fd, cptr);
	    }
	else
		free_client(cptr);
	return 0;
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
	int	i;
	aConfItem *aconf;

	/*
	 * close all 'extra' listening ports we have
	 */
	for (i = highest_fd ; i >= 0 ; i--) {
		if (!(cptr = local[i]))
			continue;
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
#ifdef RLIMIT_FD_MAX
	struct rlimit limit;

	if (!getrlimit(RLIMIT_FD_MAX, &limit))
	    {
		if (limit.rlim_max < MAXCONNECTIONS)
		    {
			(void)fprintf(stderr, "ircd fd table too big\n");
			(void)fprintf(stderr, "Hard Limit: %d IRC max: %d\n",
				      (int)limit.rlim_max, MAXCONNECTIONS);
			(void)fprintf(stderr,"Fix MAXCONNECTIONS\n");
			exit(-1);
		    }
		limit.rlim_cur = limit.rlim_max; /* make soft limit the max */
		if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
		    {
			(void)fprintf(stderr,"error setting max fd's to %d\n",
				      (int)limit.rlim_cur);
			exit(-1);
		    }
	    }
#endif
#if !defined(SOL20)
	(void)setlinebuf(stderr);
#endif
	for (fd = 3; fd < MAXCONNECTIONS; fd++)
	    {
		if (LogFd(fd)) continue;
		(void)close(fd);
		local[fd] = NULL;
	    }
	local[1] = NULL;
	(void)close(1); /* -- allow output a little more yet */

	if ((bootopt & BOOT_FORK) != 0) {
		if (fork())
			exit(0);
		fprintf(stderr, "fork() successful, now in child.\n");
	}

	resfd = init_resolver(0x1f);

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
static	int	check_init(cptr, sockn)
aClient	*cptr;
char	*sockn;
{
	anAddress	sk;
	int	len = sizeof(anAddress);

	/* If descriptor is a tty, special checking... */
	if (isatty(cptr->fd))
	    {
		strncpyzt(sockn, me.sockhost, HOSTLEN);
		bzero((char *)&sk, sizeof(anAddress));
	    }
	else if (getpeername(cptr->fd, (struct sockaddr *)&sk, &len) == -1)
	    {
		report_error("connect failure: %s %s", cptr);
		return -1;
	    }
	/* check for ipv4 in ipv6 mapped addresses */
	if ((sk.addr_family == AF_INET6) && IN6_IS_ADDR_V4MAPPED(&sk.in6.sin6_addr))
	{
		sk.addr_family = AF_INET;
		sk.in.sin_port = sk.in6.sin6_port;
		bcopy(&sk.in6.sin6_addr.s6_addr[12],&sk.in.sin_addr,sizeof(struct in_addr));
	}
	(void)strcpy(sockn, (char *)inetntoa(&sk));
	switch (sk.addr_family)
	{
		case AF_INET:
			if (inet_netof(sk.in.sin_addr) == IN_LOOPBACKNET)
			{
				cptr->hostp = NULL;
				strncpyzt(sockn, me.sockhost, HOSTLEN);
			}
			break;
		case AF_INET6:
			if (IN6_IS_ADDR_LOOPBACK(&sk.in6.sin6_addr))
			{
				cptr->hostp = NULL;
				strncpyzt(sockn, me.sockhost, HOSTLEN);
			}
			break;
	}
	bcopy((char *)&sk, (char *)&cptr->addr, sizeof(anAddress));
	switch (sk.addr_family)
	{
		case AF_INET:
			cptr->port = (int)ntohs(sk.in.sin_port);
			break;
		case AF_INET6:
			cptr->port = (int)ntohs(sk.in6.sin6_port);
			break;
	}

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
		cptr->name, inetntoa(&cptr->addr)));

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
			if (!addr_cmp(hp->h_addr_list[i], &cptr->addr))
				break;
		if (!hp->h_addr_list[i])
		    {
			strcpy((char *)&addrbuf, inetntoa(&cptr->addr));
			sendto_ops("IP# Mismatch: %s != %s[%s]",
				   addrbuf, hp->h_name,
				   inetntoa(hp->h_addr_list[0]));
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

	switch(cptr->addr.addr_family)
	{
		case AF_INET:
			if (inet_netof(cptr->addr.in.sin_addr) == IN_LOOPBACKNET || (mysk.addr_family == AF_INET
				&& inet_netof(cptr->addr.in.sin_addr) == inet_netof(mysk.in.sin_addr)))
			{
				ircstp->is_loc++;
				ClientFlags(cptr) |= FLAGS_LOCAL;
			}
			break;
		case AF_INET6:
			if (IN6_IS_ADDR_LOOPBACK(&cptr->addr.in6.sin6_addr)
				|| IN6_IS_ADDR_LINKLOCAL(&cptr->addr.in6.sin6_addr)
				|| IN6_IS_ADDR_SITELOCAL(&cptr->addr.in6.sin6_addr))
			{
				ircstp->is_loc++;
				ClientFlags(cptr) |= FLAGS_LOCAL;
			}

	    }
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
			hp = gethost_byname(s, cptr->addr.addr_family, &lin);
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
			if (!addr_cmp(hp->h_addr_list[i], &cptr->addr))
				break;
		if (!hp->h_addr_list[i])
		    {
			strcpy((char *)&addrbuf, inetntoa(&cptr->addr));
			sendto_ops("IP# Mismatch: %s != %s[%s]",
				   addrbuf, hp->h_name,
				   inetntoa(hp->h_addr_list[0]));
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
	      c_conf = find_conf_ip(lp, &cptr->addr, cptr->username, CFLAG);
	    if (!n_conf)
	      n_conf = find_conf_ip(lp, &cptr->addr, cptr->username, NFLAG);
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

	if ((!c_conf->addr.addr_family))
		bcopy((char *)&cptr->addr, (char *)&c_conf->addr, sizeof(anAddress));
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
static	int completed_connection(cptr)
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
        int	i,j;
	int	empty = cptr->fd;

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

	if (cptr->fd >= 0)
	    {
		flush_connections(cptr->fd);
		local[cptr->fd] = NULL;
		(void)closesocket(cptr->fd);
		cptr->fd = -2;
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
	for (; highest_fd > 0; highest_fd--)
		if (local[highest_fd])
			break;

	det_confs_butmask(cptr, 0);
	cptr->from = NULL; /* ...this should catch them! >:) --msa */

	/*
	 * fd remap to keep local[i] filled at the bottom.
	 */
	if (empty > 0)
		if ((j = highest_fd) > (i = empty) &&
		    (local[j]->status != STAT_LOG))
		    {
			if (dup2(j,i) == -1)
				return;
			local[i] = local[j];
			local[i]->fd = i;
			local[j] = NULL;
			(void)closesocket(j);
			while (!local[highest_fd])
				highest_fd--;
		    }
	return;
}

/*
** set_sock_opts
*/
void	set_sock_opts(fd, cptr)
int	fd;
aClient	*cptr;
{
	int	opt;
#ifdef SO_REUSEADDR
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
		report_error("setsockopt(SO_REUSEADDR) %s:%s", cptr);
#endif
#if  defined(SO_DEBUG) && defined(DEBUGMODE) && 0
/* Solaris with SO_DEBUG writes to syslog by default */
#if !defined(SOL20) || defined(USE_SYSLOG)
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_DEBUG, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
		report_error("setsockopt(SO_DEBUG) %s:%s", cptr);
#endif /* SOL20 */
#endif
#if defined(SO_USELOOPBACK)
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_USELOOPBACK, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
		report_error("setsockopt(SO_USELOOPBACK) %s:%s", cptr);
#endif
#ifdef	SO_RCVBUF
	opt = 8192;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
		report_error("setsockopt(SO_RCVBUF) %s:%s", cptr);
#endif
#ifdef	SO_SNDBUF
# ifdef	_SEQUENT_
/* seems that Sequent freezes up if the receving buffer is a different size
 * to the sending buffer (maybe a tcp window problem too).
 */
	opt = 8192;
# else
	opt = 8192;
# endif
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (OPT_TYPE *)&opt, sizeof(opt)) < 0)
		report_error("setsockopt(SO_SNDBUF) %s:%s", cptr);
#endif
#if defined(IP_OPTIONS) && defined(IPPROTO_IP)
	{
/*
	char	*s = readbuf, *t = readbuf + sizeof(readbuf) / 2;

	opt = sizeof(readbuf) / 8;
	if (getsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)t, &opt) < 0)
		report_error("getsockopt(IP_OPTIONS) %s:%s", cptr);
	else if (opt > 0 && opt != sizeof(readbuf) / 8)
	    {
		for (*readbuf = '\0'; opt > 0; opt--, s+= 3)
			(void)sprintf(s, "%02x:", *t++);
		*s = '\0';
		sendto_ops("Connection %s using IP opts: (%s)",
			   get_client_name(cptr, TRUE), readbuf);
	     }
*/
	if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)NULL, 0) < 0)
		report_error("setsockopt(IP_OPTIONS) %s:%s", cptr);
	}
#endif
}


int	get_sockerr(cptr)
aClient	*cptr;
{
	int errtmp = errno, err = 0, len = sizeof(err);
#ifdef	SO_ERROR
	if (cptr->fd >= 0)
		if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR, (OPT_TYPE *)&err, &len))
			if (err)
				errtmp = err;
#endif
	return errtmp;
}

/*
** set_non_blocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
void	set_non_blocking(fd, cptr)
int	fd;
aClient *cptr;
{
	int	res, nonb = 0;

	/*
	** NOTE: consult ALL your relevant manual pages *BEFORE* changing
	**	 these ioctl's.  There are quite a few variations on them.
	**	 They are *NOT* all the same. Heed this well. - Avalon.
	*/
#ifdef	NBLOCK_POSIX
	nonb |= O_NONBLOCK;
#endif
#ifdef	NBLOCK_BSD
	nonb |= O_NDELAY;
#endif
#ifdef	NBLOCK_SYSV
	res = 1;

	if (ioctl (fd, FIONBIO, &res) < 0)
		report_error("ioctl(fd,FIONBIO) failed for %s:%s", cptr);
#else
	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
		report_error("fcntl(fd, F_GETFL) failed for %s:%s",cptr);
	else if (fcntl(fd, F_SETFL, res | nonb) == -1)
		report_error("fcntl(fd, F_SETL, nonb) failed for %s:%s",cptr);
#endif
	return;
}

/*
 * Creates a client which has just connected to us on the given fd.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yuet since it doesnt have a name.
 */
aClient	*add_connection(cptr, fd)
aClient	*cptr;
int	fd;
{
	Link	lin;
	aClient *acptr;
	aConfItem *aconf = NULL;
	anAddress	addr;
	int	len = sizeof(anAddress), iscons = 0;

	acptr = make_client(NULL);

	if (cptr != &me)
		aconf = cptr->confs->value.aconf;
	/*
	 * Set this early so connection status messages will work.
	 */
	acptr->fd = fd;

        if (isatty(fd))
        {
              get_sockhost(acptr, cptr->sockhost);
              iscons = 1;
        }

	if ((getpeername(fd, (struct sockaddr *) &addr, &len) < 0) && !iscons) {
		report_error("Failed in connecting to %s :%s", cptr);
	add_con_refuse:
		ircstp->is_ref++;
		acptr->fd = -2;
		free_client(acptr);
		(void)closesocket(fd);
		return NULL;
	}
	/* check for ipv4 in ipv6 mapped addresses */
	if ((addr.addr_family == AF_INET6) && IN6_IS_ADDR_V4MAPPED(&addr.in6.sin6_addr))
	{
		addr.addr_family = AF_INET;
		addr.in.sin_port = addr.in6.sin6_port;
		bcopy(&addr.in6.sin6_addr.s6_addr[12],&addr.in.sin_addr,sizeof(struct in_addr));
	}
	/* don't want to add "Failed in connecting to" here.. */
	if (aconf && IsIllegal(aconf) && !iscons)
		goto add_con_refuse;
	/* Copy ascii address to 'sockhost' just in case. Then we
	 * have something valid to put into error messages...
	 */
            if (!iscons)
		get_sockhost(acptr, (char *)inetntoa(&addr));
		bcopy ((char *)&addr, (char *)&acptr->addr,
			sizeof(anAddress));
		/* Check for zaps -- Barubary */
		if (!iscons && find_zap(acptr, 0))
		{
			set_non_blocking(fd, acptr);
			set_sock_opts(fd, acptr);
			send(fd, zlinebuf, strlen(zlinebuf), 0);
			goto add_con_refuse;
		}
		switch (addr.addr_family)
		{
			case AF_INET:
				acptr->port = ntohs(addr.in.sin_port);
				break;
			case AF_INET6:
				acptr->port = ntohs(addr.in6.sin6_port);
				break;
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
		Debug((DEBUG_DNS, "lookup %s",
			inetntoa(&addr)));
		acptr->hostp = gethost_byaddr(&acptr->addr, &lin);
		if (!acptr->hostp)
                {
			SetDNS(acptr);
                }
                else connotice(acptr, REPORT_DONEC_DNS);
		nextdnscheck = 1;

	if (aconf)
		aconf->clients++;
	if (fd > highest_fd)
		highest_fd = fd;
	local[fd] = acptr;
	acptr->acpt = cptr;
	add_client_to_list(acptr);
	set_non_blocking(acptr->fd, acptr);
	set_sock_opts(acptr->fd, acptr);

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
static	int	read_packet(cptr, rfd)
aClient *cptr;
fd_set	*rfd;
{
	int	dolen = 0, length = 0, done;
	time_t	now = NOW;

	if (FD_ISSET(cptr->fd, rfd) &&
	    !(IsPerson(cptr) && DBufLength(&cptr->recvQ) > 6090))
	    {
		errno = 0;

		length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);
		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		ClientFlags(cptr) &= ~(FLAGS_NONL|FLAGS_PINGSENT);
		/*
		 * If not ready, fake it so it isnt closed
		 */
		if (length == -1 &&
			((errno == EWOULDBLOCK) || (errno == EAGAIN)))
			return 1;
		if (length <= 0)
			return length;
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
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.  delay must be > 0.
 */
int
read_message(time_t delay)
{
	aClient	*cptr;
	int	nfds;
	struct	timeval	wait;
	fd_set	read_set, write_set;
	time_t	delay2 = delay, now;
	u_long	usec = 0;
	int	res, length, fd, i;
	int	sockerr;

	now = NOW;

	for (res = 0;;) {
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		for (i = highest_fd ; i >= 0 ; i--) {
			if (!(cptr = local[i]))
				continue;

			if (IsLog(cptr))
				continue;

			if (DoingDNS(cptr))
				continue;

			if (IsMe(cptr) && IsListening(cptr)) {
				FD_SET(i, &read_set);
			} else if (!IsMe(cptr)) {
				if (DBufLength(&cptr->recvQ) && delay2 > 2)
					delay2 = 1;
				if (DBufLength(&cptr->recvQ) < 4088)
					FD_SET(i, &read_set);
			}

			if (DBufLength(&cptr->sendQ) || IsConnecting(cptr))
				FD_SET(i, &write_set);
		}

		if (udpfd >= 0)
			FD_SET(udpfd, &read_set);
		if (resfd >= 0)
			FD_SET(resfd, &read_set);

		wait.tv_sec = MIN(delay2, delay);
		wait.tv_usec = usec;
		nfds = select(FD_SETSIZE, &read_set, &write_set, 0, &wait);
		update_time();
		if (nfds == -1 && errno == EINTR)
			return -1;
		else if (nfds >= 0)
			break;
		res++;
		if (res > 5)
			restart("too many select errors");
		if (errno != 0) {
			sleep(10);
			update_time();
		}
	}

	if (udpfd >= 0 && FD_ISSET(udpfd, &read_set))
	    {
			polludp();
			update_time();
			nfds--;
			FD_CLR(udpfd, &read_set);
	    }
	if (resfd >= 0 && FD_ISSET(resfd, &read_set))
	    {
			do_dns_async();
			nfds--;
			FD_CLR(resfd, &read_set);
	    }

	for (i = highest_fd; i >= 0; i--)
		if ((cptr = local[i]) && FD_ISSET(i, &read_set) &&
		    IsListening(cptr))
		    {
			FD_CLR(i, &read_set);
			nfds--;
			cptr->lasttime = NOW;
			/*
			** There may be many reasons for error return, but
			** in otherwise correctly working environment the
			** probable cause is running out of file descriptors
			** (EMFILE, ENFILE or others?). The man pages for
			** accept don't seem to list these as possible,
			** although it's obvious that it may happen here.
			** Thus no specific errors are tested at this
			** point, just assume that connections cannot
			** be accepted until some old is closed first.
			*/
			if ((fd = accept(i, NULL, NULL)) < 0)
			    {
				report_error("Cannot accept connections %s:%s",
						cptr);
				break;
			    }
			ircstp->is_ac++;
			if (fd >= MAXCLIENTS)
			    {
				ircstp->is_ref++;
				sendto_ops("All connections in use. (%s)",
					   get_client_name(cptr, TRUE));
				(void)send(fd,
					"ERROR :All connections in use\r\n",
					32, 0);
				(void)close(fd);
				break;
			    }
			/*
			 * Use of add_connection (which never fails :) meLazy
			 */
			(void)add_connection(cptr, fd);
			nextping = NOW;
			if (!cptr->acpt)
				cptr->acpt = &me;
		    }

	for (i = highest_fd; i >= 0; i--)
	    {
		if (!(cptr = local[i]) || IsMe(cptr))
			continue;
		if (FD_ISSET(i, &write_set))
		    {
			int	write_err = 0;
			nfds--;
			/*
			** ...room for writing, empty some queue then...
			*/
			if (IsConnecting(cptr))
				  write_err = completed_connection(cptr);
			if (!write_err) {
				  if (DoList(cptr) && IsSendable(cptr))
					send_list(cptr, 32);
				  (void)send_queued(cptr);
			}
			if (IsDead(cptr) || write_err)
			    {
deadsocket:
				if (FD_ISSET(i, &read_set))
				    {
					nfds--;
					FD_CLR(i, &read_set);
				    }
				(void)exit_client(cptr, cptr, &me,
						  ((sockerr = get_sockerr(cptr))
						   ? strerror(sockerr)
						   : "Client exited"));
				continue;
			    }
		    }
		length = 1;	/* for fall through case */
		if (!NoNewLine(cptr) || FD_ISSET(i, &read_set))
			length = read_packet(cptr, &read_set);
		if (length > 0)
			flush_connections(i);
		if ((length != FLUSH_BUFFER) && IsDead(cptr))
			goto deadsocket;
		if (!FD_ISSET(i, &read_set) && length > 0)
			continue;
		nfds--;
		readcalls++;
		if (length > 0)
			continue;

		/*
		** ...hmm, with non-blocking sockets we might get
		** here from quite valid reasons, although.. why
		** would select report "data available" when there
		** wasn't... so, this must be an error anyway...  --msa
		** actually, EOF occurs when read() returns 0 and
		** in due course, select() returns that fd as ready
		** for reading even though it ends up being an EOF. -avalon
		*/
		Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d",
			i, errno, length));

		/*
		** NOTE: if length == -2 then cptr has already been freed!
		*/
		if (length != -2 && (IsServer(cptr) || IsHandshake(cptr)))
		    {
			if (length == 0) {
				 sendto_ops("Server %s closed the connection",
					    get_client_name(cptr,FALSE));
				 sendto_serv_butone(&me,
					":%s GNOTICE :Server %s closed the connection",
					me.name, get_client_name(cptr,FALSE));
			}
			else
				 report_error("Lost connection to %s:%s",
					      cptr);
		    }
		if (length != FLUSH_BUFFER)
			(void)exit_client(cptr, cptr, &me,
					  ((sockerr = get_sockerr(cptr))
					   ? strerror(sockerr)
					   : "Client exited"));
	    }
	update_time();
	return 0;
}

/*
 * connect_server
 */
int	connect_server(aconf, by, hp)
aConfItem *aconf;
aClient	*by;
struct	HostEnt	*hp;
{
	struct	sockaddr *svp;
	aClient *cptr, *c2ptr;
	char	*s;
	int	errtmp, len = sizeof(anAddress);
	struct addrinfo	*res;

	Debug((DEBUG_NOTICE,"Connect to %s[%s] @%s",
		aconf->name, aconf->host, inetntoa(&aconf->addr)));

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
	if ((!aconf->addr.addr_family)) {
	        Link    lin;

		lin.flags = ASYNC_CONNECT;
		lin.value.aconf = aconf;
		nextdnscheck = 1;
		s = (char *)index(aconf->host, '@');
		s++; /* should NEVER be NULL */
		if (getaddrinfo(s, NULL, NULL, &res))
			return 0;
		else
		{
			bcopy(res->ai_addr, &aconf->addr, sizeof(anAddress));
			freeaddrinfo(res);
		}
	    }
	cptr = make_client(NULL);
	cptr->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strncpyzt(cptr->name, aconf->name, sizeof(cptr->name));
	strncpyzt(cptr->sockhost, aconf->host, HOSTLEN+1);

	svp = (struct sockaddr*) connect_inet(aconf, cptr, &len);

	if (!svp)
	    {
		if (cptr->fd != -1)
			(void)close(cptr->fd);
		cptr->fd = -2;
		free_client(cptr);
		return -1;
	    }

	set_non_blocking(cptr->fd, cptr);
	set_sock_opts(cptr->fd, cptr);
	(void)signal(SIGALRM, dummy_sig);
	(void)alarm(4);
	if (connect(cptr->fd, svp, len) < 0 && errno != EINPROGRESS)
	    {
		errtmp = errno; /* other system calls may eat errno */
		(void)alarm(0);
		report_error("Connect to host %s failed: %s",cptr);
                if (by && IsPerson(by) && !MyClient(by))
                  sendto_one(by,
                             ":%s NOTICE %s :Connect to host %s failed.",
			     me.name, by->name, cptr);
		(void)close(cptr->fd);
		cptr->fd = -2;
		free_client(cptr);
		errno = errtmp;
		if (errno == EINTR)
			errno = ETIMEDOUT;
		return -1;
	    }
	(void)alarm(0);

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
		(void)close(cptr->fd);
		cptr->fd = -2;
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
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	local[cptr->fd] = cptr;
	cptr->acpt = &me;
	SetConnecting(cptr);

	get_sockhost(cptr, aconf->host);
	add_client_to_list(cptr);
	nextping = NOW;
	update_time();

	return 0;
}

static	anAddress *connect_inet(aconf, cptr, lenp)
aConfItem	*aconf;
aClient	*cptr;
int	*lenp;
{
	static	anAddress	server;
	/* struct	HostEnt	*hp; */
	/* char	*s; */
	struct addrinfo	*res;

	/*
	 * By this point we should know the IP# of the host listed in the
	 * conf line, whether as a result of the hostname lookup or the ip#
	 * being present instead. If we dont know it, then the connect fails.
	 */
	if (!aconf->addr.addr_family)
	{
		if (getaddrinfo(aconf->host, NULL, NULL, &res))
			return 0;
		else
		{
			bcopy(res->ai_addr, &aconf->addr, sizeof(anAddress));
			freeaddrinfo(res);
		}
	}

	/*
	 * Might as well get sockhost from here, the connection is attempted
	 * with it so if it fails its useless.
	 */
	(void)alarm(2);
	cptr->fd = socket(aconf->addr.addr_family, SOCK_STREAM, 0);
	if (cptr->fd >= MAXCLIENTS)
	    {
	    	(void)alarm(0);
		sendto_ops("No more connections allowed (%s)", cptr->name);
		return NULL;
	    }
	(void)alarm(0);
	/* This doesn't seem very useful --Onno */
	switch (mysk.addr_family)
	{
		case AF_INET:
			mysk.in.sin_port = 0;
			break;
		case AF_INET6:
			mysk.in6.sin6_port = 0;
			break;
	}
	get_sockhost(cptr, aconf->host);

	if (cptr->fd == -1)
	    {
		report_error("opening stream socket to server %s:%s", cptr);
		return NULL;
	    }
	get_sockhost(cptr, aconf->host);
	bcopy(&me.addr, &server, sizeof(anAddress));
	/*
	** Bind to a local IP# (with unknown port - let unix decide) so
	** we have some chance of knowing the IP# that gets used for a host
	** with more than one IP#.
	*/
	/* No we don't bind it, not all OS's can handle connecting with
	** an already bound socket, different ip# might occur anyway
	** leading to a freezing select() on this side for some time.
	** I had this on my Linux 1.1.88 --Run
	*/
	/* We do now.  Virtual interface stuff --ns */
	if (server.addr_family == aconf->addr.addr_family)
	{
		switch (server.addr_family)
		{
			case AF_INET:
				if (server.in.sin_addr.s_addr != INADDR_ANY)
				{
					server.in.sin_port = 0;
					if (bind(cptr->fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1)
					{
						report_error("error binding to local port for %s:%s", cptr);
						return NULL;
					}
				}
				break;
			case AF_INET6:
				if (!IN6_IS_ADDR_UNSPECIFIED(&server.in6.sin6_addr))
				{
					server.in6.sin6_port = 0;
					if (bind(cptr->fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in6)) == -1)
					{
						report_error("error binding to local port for %s:%s", cptr);
						return NULL;
					}
				}
				break;
		}
	}

	bcopy((char *)&aconf->addr, (char *)&server, sizeof(anAddress));
	bcopy((char *)&aconf->addr, (char *)&cptr->addr, sizeof(anAddress));
	switch (server.addr_family)
	{
		case AF_INET:
			server.in.sin_port = htons(((aconf->port > 0) ? aconf->port : portnum));
			*lenp = sizeof(struct sockaddr_in);
			break;
		case AF_INET6:
			server.in6.sin6_port = htons(((aconf->port > 0) ? aconf->port : portnum));
			*lenp = sizeof(struct sockaddr_in6);
			break;
		default:
			*lenp = sizeof(server);
			break;
	}
	return &server;
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

	/*
	** Setup local socket structure to use for binding to.
	*/
	bzero((char *)&mysk, sizeof(mysk));
	mysk.in.sin_family = cptr->addr.addr_family;

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
		bcopy(hp->h_addr, (char *)&mysk,
			sizeof(anAddress));
		Debug((DEBUG_DEBUG,"local name is %s",
				get_client_name(&me,TRUE)));
	    }
	return;
}

/*
** setup a UDP socket and listen for incoming packets
*/
int	setup_ping()
{
	anAddress	from;
	int	on = 1, len = 0;

	bcopy((char *)&me.addr, (char *)&from, sizeof(anAddress));
	switch (from.addr_family)
	{
		case AF_INET:
			len = sizeof(struct sockaddr_in);
			from.in.sin_port = htons(7007);
			break;
		case AF_INET6:
			len = sizeof(struct sockaddr_in6);
			from.in6.sin6_port = htons(7007);
			break;
	}

	if ((udpfd = socket(from.addr_family, SOCK_DGRAM, 0)) == -1)
	    {
		Debug((DEBUG_ERROR, "socket udp : %s", strerror(errno)));
		return -1;
	    }
	if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR,
			(OPT_TYPE *)&on, sizeof(on)) == -1)
	    {
#ifdef	USE_SYSLOG
		syslog(LOG_ERR, "setsockopt udp fd %d : %m", udpfd);
#endif
		Debug((DEBUG_ERROR, "setsockopt so_reuseaddr : %s",
			strerror(errno)));
		(void)close(udpfd);
		udpfd = -1;
		return -1;
	    }
	on = 0;
	(void) setsockopt(udpfd, SOL_SOCKET, SO_BROADCAST,
			  (char *)&on, sizeof(on));
	if (bind(udpfd, (struct sockaddr *)&from, len)==-1)
	    {
#ifdef	USE_SYSLOG
		switch (from.addr_family)
		{
			case AF_INET:
				syslog(LOG_ERR, "bind udp.%d fd %d : %m",
					from.in.sin_port, udpfd);
				break;
			case AF_INET6:
				syslog(LOG_ERR, "bind udp.%d fd %d : %m",
					from.in6.sin6_port, udpfd);
				break;
		}
#endif
		Debug((DEBUG_ERROR, "bind : %s", strerror(errno)));
		(void)close(udpfd);
		udpfd = -1;
		return -1;
	    }
	if (fcntl(udpfd, F_SETFL, FNDELAY)==-1)
	    {
		Debug((DEBUG_ERROR, "fcntl fndelay : %s", strerror(errno)));
		(void)close(udpfd);
		udpfd = -1;
		return -1;
	    }
	return udpfd;
}

/*
 * max # of pings set to 15/sec.
 */
static	void	polludp()
{
	char	*s;
	anAddress	from;
	int	n, fromlen = sizeof(from);
	static	time_t	last = 0, now;
	static	int	cnt = 0, mlen = 0;

	/*
	 * find max length of data area of packet.
	 */
	if (!mlen)
	    {
		mlen = sizeof(readbuf) - strlen(me.name) - strlen(version);
		mlen -= 6;
		if (mlen < 0)
			mlen = 0;
	    }
	Debug((DEBUG_DEBUG,"udp poll"));

	n = recvfrom(udpfd, readbuf, mlen, 0,
		(struct sockaddr *)&from, &fromlen);
	now = NOW;
	if (now == last)
		if (++cnt > 14)
			return;
	cnt = 0;
	last = now;

	if (n == -1)
	    {
		if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
			return;
		else
		    {
			report_error("udp port recvfrom (%s): %s", &me);
			return;
		    }
	    }
	ircstp->is_udp++;
	if (n  < 19)
		return;

	s = readbuf + n;
	/*
	 * attach my name and version for the reply
	 */
	*readbuf |= 1;
	(void)strcpy(s, me.name);
	s += strlen(s)+1;
	(void)strcpy(s, version);
	s += strlen(s);
	(void)sendto(udpfd, readbuf, s-readbuf, 0,
		(struct sockaddr *)&from ,sizeof(from));
	return;
}

/*
 * do_dns_async
 *
 * Called when the fd returned from init_resolver() has been selected for
 * reading.
 */
static	void	do_dns_async()
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
		    bcopy(hp->h_addr, (char *)&aconf->addr,
			  sizeof(anAddress));
		    (void)connect_server(aconf, NULL, hp);
		}
		else
		  sendto_ops("Connect to %s failed: host lookup",
			     (aconf) ? aconf->host : "unknown");
		break;

	      case ASYNC_CONF :
		aconf = ln.value.aconf;
		if (hp && aconf)
		  bcopy(hp->h_addr, (char *)&aconf->addr,
			sizeof(anAddress));
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
