/*
 *   IRC - Internet Relay Chat, ircd/ircd.c
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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"

#include <ctype.h>

#include "ircd/send.h"
#include "ircd/string.h"


IRCD_SCCSID("@(#)ircd.c	2.48 3/9/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */

void NospoofText(aClient* acptr);
int register_user(aClient *cptr, aClient *sptr, char *nick, char *username);

time_t NOW, tm_offset = 0;

void server_reboot(char *);
void restart(char *);
static void setup_signals();

char **myargv;
int portnum = -1;		/* Server port number, listening this */
char *configfile = CONFIGFILE;	/* Server configuration file */
int debuglevel = -1;		/* Server debug level */
int bootopt = BOOT_FORK;	/* Server boot option flags */
char *debugmode = "";		/*  -"-    -"-   */
char *sbrk0;			/* initial sbrk(0) */
static int dorehash = 0;
static char *dpath = DPATH;

time_t nextconnect = 1;		/* time for next try_connections call */
time_t nextping = 1;		/* same as above for check_pings() */
time_t nextdnscheck = 0;	/* next time to poll dns to force timeouts */
time_t nextexpire = 1;		/* next expire run on the dns cache */
time_t nextsockflush = 1;
void boot_replies(void);

VOIDSIG s_die(int sig)
{
	IRCD_UNUSED(sig);

#ifdef	USE_SYSLOG
	(void)syslog(LOG_CRIT, "Server Killed By SIGTERM");
#endif
	flush_connections(me.fd);
	close_logs();
	exit(-1);
}

static VOIDSIG s_rehash()
{
	dorehash = 1;
}

void	restart(char *mesg)
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_WARNING, "Restarting Server because: %s",mesg);
#endif
	server_reboot(mesg);
}

VOIDSIG s_restart()
{
	static int restarting = 0;

#ifdef	USE_SYSLOG
	(void)syslog(LOG_WARNING, "Server Restarting on SIGHUP");
#endif
	if (restarting == 0)
	{
		sigset_t	set;

		/* Unblock this signal */
		sigemptyset(&set);
		sigaddset(&set, SIGINT);
		sigprocmask(SIG_UNBLOCK, &set, NULL);

		/* Send (or attempt to) a dying scream to oper if present */

		restarting = 1;
		server_reboot("SIGHUP");
	}
}

void	server_reboot(mesg)
char	*mesg;
{
        int	i;

	sendto_ops("Aieeeee!!!  Restarting server... %s", mesg);
	Debug((DEBUG_NOTICE,"Restarting server... %s", mesg));
	flush_connections(me.fd);

        close_logs();

	/*
	 * Close any FDs we may have open.
	 */
	for (i = 0; i < MAXCONNECTIONS; i++)
	{
		if ((bootopt & BOOT_FORK) == 0 && i == 2)
			continue;
		close(i);
	}

	execv(MYNAME, myargv);

	open_logs();
#ifdef USE_SYSLOG
	syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", MYNAME, myargv[0]);
#endif
	Debug((DEBUG_FATAL,"Couldn't restart server: %s", strerror(errno)));
	close_logs();

	exit(-1);
}


/*
** try_connections
**
**	Scan through configuration and try new connections.
**	Returns the calendar time when the next call to this
**	function should be made latest. (No harm done if this
**	is called earlier or later...)
*/
static	time_t	try_connections(currenttime)
time_t	currenttime;
{
	aConfItem *aconf, **pconf;
	aConfItem *cconf, *con_conf = NULL;
	aClient *cptr;
#ifndef HUB
	aClient *xcptr;
	int i;
#endif
	int	connecting, confrq;
	int	con_class = 0;
	time_t	next = 0;
	aClass	*cltmp;

	connecting = FALSE;
	Debug((DEBUG_NOTICE,"Connection check at   : %s",
		myctime(currenttime)));
	for (aconf = conf; aconf; aconf = aconf->next )
	    {
		/* Also when already connecting! (update holdtimes) --SRB */
		if (!(aconf->status & CONF_CONNECT_SERVER) || aconf->port <= 0)
			continue;
		cltmp = Class(aconf);
		/*
		** Skip this entry if the use of it is still on hold until
		** future. Otherwise handle this entry (and set it on hold
		** until next time). Will reset only hold times, if already
		** made one successfull connection... [this algorithm is
		** a bit fuzzy... -- msa >;) ]
		*/

		if ((aconf->hold > currenttime))
		    {
			if ((next > aconf->hold) || (next == 0))
				next = aconf->hold;
			continue;
		    }

		confrq = get_con_freq(cltmp);
		aconf->hold = currenttime + confrq;
		/*
		** Found a CONNECT config with port specified, scan clients
		** and see if this server is already connected?
		*/
		cptr = find_name(aconf->name, (aClient *)NULL);

		if (!cptr && (Links(cltmp) < MaxLinks(cltmp)) &&
		    (!connecting || (Class(cltmp) > con_class)))
		  {
		    /* Check connect rules to see if we're allowed to try */
		    for (cconf = conf; cconf; cconf = cconf->next)
		      if ((cconf->status & CONF_CRULE) &&
			  (match(cconf->host, aconf->name) == 0))
			if (crule_eval (cconf->passwd))
			  break;
		    if (!cconf)
		      {
			con_class = Class(cltmp);
			con_conf = aconf;
			/* We connect only one at time... */
			connecting = TRUE;
		      }
		  }
		if ((next > aconf->hold) || (next == 0))
			next = aconf->hold;
	    }

#ifndef HUB
	if (connecting) {
		for ( i = highest_fd; i > 0; i--)
			if (!( xcptr = local[i] ))
				continue;
			else if (IsServer(xcptr)) {
				connecting = FALSE;
				break;
			}
	}
#endif

	if (connecting)
	    {
		if (con_conf->next)  /* are we already last? */
		    {
			for (pconf = &conf; (aconf = *pconf);
			     pconf = &(aconf->next))
				/* put the current one at the end and
				 * make sure we try all connections
				 */
				if (aconf == con_conf)
					*pconf = aconf->next;
			(*pconf = con_conf)->next = 0;
		    }
		if (connect_server(con_conf, (aClient *)NULL,
				   (struct HostEnt *)NULL) == 0)
			sendto_ops("Connection to %s[%s] activated.",
				   con_conf->name, con_conf->host);
	    }
	Debug((DEBUG_NOTICE,"Next connection check : %s", myctime(next)));
	return (next);
}

/* Now find_kill is only called when a kline-related command is used:
   AKILL/RAKILL/KLINE/UNKLINE/REHASH.  Very significant CPU usage decrease.
   I made changes to every check_pings call to add new parameter.
   -- Barubary */
time_t
check_pings(time_t currenttime, int check_kills, aConfItem *conf_target)
{		
	aClient	*cptr;
	int	killflag;
	int	ping = 0, i;
	time_t	oldest = 0, timeout;

#ifdef TIMED_KLINES
	check_kills = 1;
#endif
	for (i = 0; i <= highest_fd; i++) {
		if (!(cptr = local[i]) || IsMe(cptr))
			continue;

		/*
		** Note: No need to notify opers here. It's
		** already done when "FLAGS_DEADSOCKET" is set.
		*/
		if (ClientFlags(cptr) & FLAGS_DEADSOCKET) {
			(void)exit_client(cptr, cptr, &me, "Dead socket");
			i--;  /* catch remapped descriptors */
			continue;
		}

		if (check_kills && IsPerson(cptr)
			&& (find_kill(cptr, conf_target) || find_zap(cptr, 1)))
		{
			killflag = 1;
		}
		else
		{
			killflag = 0;
		}

		ping = IsRegistered(cptr) ? get_client_ping(cptr) :
					    CONNECTTIMEOUT;
		Debug((DEBUG_DEBUG, "c(%s)=%d p %d k %d a %d",
		       cptr->name, cptr->status, ping, killflag,
		       currenttime - cptr->lasttime));

		if (killflag)
		{
			sendto_ops("Kill line active for %s",
				get_client_name(cptr, FALSE));
			exit_client(cptr, cptr, &me, "User has been banned");
			i--;  /* catch remapped descriptors */
		}
		else if (ping > currenttime - cptr->lasttime)
		{
			/*
			 * Do nothing but skip other checks.
			 */
		}
		else if (!IsRegistered(cptr) && DoingDNS(cptr))
		{
			Debug((DEBUG_NOTICE,
				"DNS timeout %s",
				get_client_name(cptr,TRUE)));
			del_queries((char *)cptr);
			ClearDNS(cptr);
			SetAccess(cptr);

			cptr->firsttime = currenttime;
			cptr->lasttime = currenttime;
			continue;
		}
		else if (!IsRegistered(cptr))
		{
#if !defined(REQ_VERSION_RESPONSE) && !defined(NO_VERSION_CHECK)
			if (IsNotSpoof(cptr)
				&& !IsUserVersionKnown(cptr)
				&& cptr->user && cptr->name[0])
			{
				SetUserVersionKnown(cptr);
				register_user(cptr, cptr, cptr->name,
					cptr->user->username);
				continue;
			}
#endif
			if (IsConnecting(cptr) || IsHandshake(cptr))
			{
				sendto_ops("Connecting to %s failed, closing link",
					get_client_name(cptr, FALSE));
			}
			else if (cptr->user)
			{
				sendto_one(cptr, ":%s NOTICE AUTH :*** Your IRC software has failed to respond properly to the client check.", me.name);
				sendto_one(cptr, ":%s NOTICE AUTH :*** Try turning off or uninstalling any software addons or scripts you may be running and try again.", me.name);
				sendto_one(cptr, ":%s NOTICE AUTH :*** If you still have trouble connecting, then please see: " NS_URL "", me.name);
			}
			exit_client(cptr, cptr, &me, "Connection setup failed");
			i--;  /* catch remapped descriptors */
			continue;
		}
		/*
		 * If the server hasnt talked to us in 2*ping seconds
		 * and it has a ping time, then close its connection.
		 * If the client is a user and a KILL line was found
		 * to be active, close this connection too.
		 */
		else if (
		    (currenttime - cptr->lasttime) >= (2 * ping) &&
		     (ClientFlags(cptr) & FLAGS_PINGSENT)) {
			if (IsServer(cptr))
			{
				sendto_ops("No response from %s, closing link",
					   get_client_name(cptr, FALSE));
				sendto_serv_butone(&me, ":%s GNOTICE :No response from %s, closing link",
					me.name, get_client_name(cptr, FALSE));
			}
                        exit_client(cptr, cptr, &me, "Ping timeout");
			i--;  /* catch remapped descriptors */
			continue;
		    }
		else if (IsRegistered(cptr) && (ClientFlags(cptr) & FLAGS_PINGSENT) == 0)
		    {
			/*
			 * if we havent PINGed the connection and we havent
			 * heard from it in a while, PING it to make sure
			 * it is still alive.
			 */
			ClientFlags(cptr) |= FLAGS_PINGSENT;
			/* not nice but does the job */
			cptr->lasttime = currenttime - ping;
			sendto_one(cptr, "PING :%s", me.name);
		    }
		timeout = cptr->lasttime + ping;
		
		while (timeout <= currenttime)
			timeout += ping;
		if (timeout < oldest || !oldest)
			oldest = timeout;
	    }
	if (!oldest || oldest < currenttime)
		oldest = currenttime + PINGFREQUENCY;
	Debug((DEBUG_NOTICE,"Next check_ping() call at: %s, %d %d %d",
		myctime(oldest), ping, oldest, currenttime));

	return (oldest);
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static void
bad_command(void)
{
#ifdef DEBUGMODE
	printf("Usage: ircd [-sFv] [-f configfile] [-x loglevel]\n");
#else
	printf("Usage: ircd [-sFv] [-f configfile]\n");
#endif
	printf("Server not started\n\n");
	exit(-1);
}

int
main(int argc, char **argv)
{
	time_t	delay = 0, now;
	int	portarg = 0;
	aConfItem *aconf;

        update_time();
	sbrk0 = (char *)sbrk((size_t)0);

	myargv = argv;
	(void)umask(077);                /* better safe than sorry --SRB */
	bzero((char *)&me, sizeof(me));

	if (chdir(dpath)) {
		perror("chdir");
		exit(-1);
	}

	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr, "ERROR: do not run ircd setuid root.\n");
		exit(-1);
	}

	open_logs();

	tolog(LOG_IRCD, "ircd %s starting up...", version);

	tolog(LOG_IRCD, "Setting up signals...");
	setup_signals();
	tolog(LOG_IRCD, "done");

#ifdef FORCE_CORE
        tolog(LOG_IRCD, "Removing corefile size limits...");
	corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &corelim))
	{
		tolog(LOG_IRCD, "unlimit core size failed; errno = %d", errno);
	}
        else
	{
		tolog(LOG_IRCD, "done");
	}

#endif

	/* Set up the case tables */
	tolog(LOG_IRCD, "setting up casetables...");
	setup_casetables();
	tolog(LOG_IRCD, "done");

	/*
	** All command line parameters have the syntax "-fstring"
	** or "-f string" (e.g. the space is optional). String may
	** be empty. Flag characters cannot be concatenated (like
	** "-fxyz"), it would conflict with the form "-fstring".
	*/
	tolog(LOG_IRCD, "Startup options: ");
	while (--argc > 0 && (*++argv)[0] == '-')
	    {
		char	*p = argv[0]+1;
		int	flag = *p++;

		if (flag == '\0' || *p == '\0') {
			if (argc > 1 && argv[1][0] != '-') {
				p = *++argv;
				argc -= 1;
			} else {
				p = "";
			}
		}

		switch (flag) {
		case 'F':
			bootopt &= ~BOOT_FORK;
			tolog(LOG_IRCD, "Will not fork()");
			break;

		case 'f':
			configfile = p;
			tolog(LOG_IRCD, "Config[%s] ", configfile);
			break;

		case 's':
			printf("sizeof(anUser) == %lu\n",
			       (unsigned long)sizeof(anUser));
			printf("sizeof(aClient) == %lu\n",
			       (unsigned long)sizeof(aClient));
			printf("sizeof(aClient) == %lu\n",
			       (unsigned long)sizeof(aClient));
			printf("sizeof(Link) == %lu\n",
			       (unsigned long)sizeof(Link));
			printf("sizeof(aServer) == %lu\n",
			       (unsigned long)sizeof(aServer));
			printf("sizeof(dbuf) == %lu\n",
			       (unsigned long)sizeof(dbuf));
			printf("sizeof(aChannel) == %lu\n",
			       (unsigned long)sizeof(aChannel));
			printf("bans maxlength: %u maxn# %u"
			       " banstruct %lu*bans\n",
			       MAXBANLENGTH, MAXBANS,
			       (unsigned long)sizeof(Link));
			printf("invites %lu*numinvites\n",
			       (unsigned long)sizeof(Link));
			exit(0);

		case 'v':
			printf("ircd %s\n", version);
			exit(0);

		case 'x':
#ifdef	DEBUGMODE
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			bootopt |= BOOT_DEBUG;
			break;
#else
			bad_command();
#endif

		default:
			bad_command();
			break;
		}
	    }

	if (argc > 0)
		bad_command();

	tolog(LOG_IRCD, "Cleaning hash tables...");
	clear_client_hash_table();
	clear_channel_hash_table();
        boot_replies();

	tolog(LOG_IRCD, "done");
	msgtab_buildhash();

	tolog(LOG_IRCD, "Initializing lists...");

	initlists();
	initclass();
	initwhowas();
	initstats();
	tolog(LOG_IRCD, "done");
	if (portnum < 0)
		portnum = PORTNUM;
	me.port = portnum;
	tolog(LOG_IRCD, "Pre-socket startup done.");
	init_sys();
	me.flags = FLAGS_LISTEN;
	me.fd = -1;

#ifdef USE_SYSLOG
	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	if (initconf(bootopt) == -1) {
		tolog(LOG_IRCD, "error opening ircd config file: %s",
			configfile);
		Debug((DEBUG_FATAL, "Failed in reading configuration file %s",
		       configfile));
		exit(-1);
	}

	if ((aconf = find_me()) && portarg <= 0 && aconf->port > 0)
		portnum = aconf->port;
	Debug((DEBUG_ERROR, "Port = %d", portnum));
	if (inetport(&me, aconf->passwd, portnum)) {
		tolog(LOG_IRCD, "Error listening on port %d", portnum);
		exit(1);
	}

	(void)get_my_name(&me, me.sockhost, sizeof(me.sockhost)-1);
	if (me.name[0] == '\0')
		strncpyzt(me.name, me.sockhost, sizeof(me.name));
	me.hopcount = 0;
	me.confs = NULL;
	me.next = NULL;
	me.user = NULL;
	me.from = &me;
	SetMe(&me);
	make_server(&me);
	(void)strcpy(me.serv->up, me.name);

	update_time();
	me.lasttime = me.since = me.firsttime = NOW;
	(void)add_to_client_hash_table(me.name, &me);

	check_class();
	write_pidfile();

       Debug((DEBUG_NOTICE,"Server ready..."));
#ifdef USE_SYSLOG
       syslog(LOG_NOTICE, "Server Ready");
#endif

	for (;;)
	    {
		update_time();
		now = NOW;
		/*
		** We only want to connect if a connection is due,
		** not every time through.  Note, if there are no
		** active C lines, this call to Tryconnections is
		** made once only; it will return 0. - avalon
		*/
		if (nextconnect && now >= nextconnect)
			nextconnect = try_connections(now);
		/*
		** DNS checks. One to timeout queries, one for cache expiries.
		*/
		if (now >= nextdnscheck)
			nextdnscheck = timeout_query_list(now);
		if (now >= nextexpire)
			nextexpire = expire_cache(now);
		/*
		** take the smaller of the two 'timed' event times as
		** the time of next event (stops us being late :) - avalon
		** WARNING - nextconnect can return 0!
		*/
		if (nextconnect)
			delay = MIN(nextping, nextconnect);
		else
			delay = nextping;
		delay = MIN(nextdnscheck, delay);
		delay = MIN(nextexpire, delay);
		delay -= now;
		/*
		** Adjust delay to something reasonable [ad hoc values]
		** (one might think something more clever here... --msa)
		** We don't really need to check that often and as long
		** as we don't delay too long, everything should be ok.
		** waiting too long can cause things to timeout...
		** i.e. PINGS -> a disconnection :(
		** - avalon
		*/
		if (delay < 1)
			delay = 1;
		else
			delay = MIN(delay, TIMESEC);
		(void)read_message(delay);
		
		Debug((DEBUG_DEBUG ,"Got message(s)"));

		update_time();
		now = NOW;
		/*
		** ...perhaps should not do these loops every time,
		** but only if there is some chance of something
		** happening (but, note that conf->hold times may
		** be changed elsewhere--so precomputed next event
		** time might be too far away... (similarly with
		** ping times) --msa
		*/
		if (now >= nextping) {
			nextping = check_pings(now, 0, NULL);
                }

		if (dorehash)
		    {
			(void)rehash(&me, &me, 1);
			dorehash = 0;
		    }
		/*
		** Flush output buffers on all connections now if they
		** have data in them (or at least try to flush)
		** -avalon
		*/
		flush_connections(me.fd);
	    }
    }

static void setup_signals()
{
	struct sigaction	act;

	/*
	 * Ignore the following signals:  PIPE, ALRM
	 */
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGALRM, &act, NULL);

	act.sa_handler = s_rehash;
	sigaction(SIGHUP, &act, NULL);

	act.sa_handler = s_restart;
	sigaction(SIGINT, &act, NULL);

	act.sa_handler = s_die;
	sigaction(SIGTERM, &act, NULL);
}
