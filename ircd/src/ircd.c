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
#include "userload.h"
#include "h.h"

#include "ircd/send.h"

IRCD_SCCSID("@(#)ircd.c	2.48 3/9/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */

void NospoofText(aClient* acptr);

time_t    NOW, tm_offset = 0;
void	server_reboot(char *);
void	restart(char *);
static	void	setup_signals();

char	**myargv;
int	portnum = -1;		    /* Server port number, listening this */
char	*configfile = CONFIGFILE;	/* Server configuration file */
int	debuglevel = -1;		/* Server debug level */
int	bootopt = BOOT_FORK;		/* Server boot option flags */
char	*debugmode = "";		/*  -"-    -"-   */
char	*sbrk0;				/* initial sbrk(0) */
static	int	dorehash = 0;
static	char	*dpath = DPATH;

time_t	nextconnect = 1;	/* time for next try_connections call */
time_t	nextping = 1;		/* same as above for check_pings() */
time_t	nextdnscheck = 0;	/* next time to poll dns to force timeouts */
time_t	nextexpire = 1;		/* next expire run on the dns cache */
time_t  nextsockflush = 1;
void	boot_replies(void);

#if	defined(PROFIL)
extern	etext();

VOIDSIG	s_monitor()
{
	static	int	mon = 0;
#ifdef	POSIX_SIGNALS
	struct	sigaction act;
#endif

	(void)moncontrol(mon);
	mon = 1 - mon;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
#else
	(void)signal(SIGUSR1, s_monitor);
#endif
}
#endif


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
#ifdef	POSIX_SIGNALS
	struct	sigaction act;
#endif
	dorehash = 1;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
#else
	(void)signal(SIGHUP, s_rehash);	/* sysV -argv */
#endif
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
	(void)syslog(LOG_WARNING, "Server Restarting on SIGINT");
#endif
	if (restarting == 0)
	    {
		/* Send (or attempt to) a dying scream to oper if present */

		restarting = 1;
		server_reboot("SIGINT");
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
	for (i = 3; i < MAXCONNECTIONS; i++)
		(void)close(i);

	(void)execv(MYNAME, myargv);

#ifdef USE_SYSLOG
	syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", MYNAME, myargv[0]);
#endif
	Debug((DEBUG_FATAL,"Couldn't restart server: %s", strerror(errno)));

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
	int	ping = 0, i, rflag = 0;
	time_t	oldest = 0, timeout;

#if defined(NOSPOOF) && defined(REQ_VERSION_RESPONSE)
	#define LAST_TIME(xptr)                                          \
		(!IsRegisteredUser(xptr) ?                               \
		     ( (xptr)->lasttime )                                \
		 :                                                       \
		     ( IsUserVersionKnown((xptr))                        \
		       ?                                                 \
			((xptr)->lasttime)                               \
		       :                                                 \
                	MIN( (xptr)->firsttime + MAX_NOVERSION_DELAY,    \
				(xptr)->lasttime )                       \
		     )                                                   \
		)
#else
	#define LAST_TIME(xptr) \
		 ((const int)((xptr)->lasttime))
#endif

#ifdef TIMED_KLINES
	check_kills = 1;
#endif
	for (i = 0; i <= highest_fd; i++) {
		if (!(cptr = local[i]) || IsMe(cptr) || IsLog(cptr))
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

		if (check_kills)
			killflag = IsPerson(cptr) ? find_kill(cptr,
							      conf_target) : 0;
		else
			killflag = 0;
		if (check_kills && !killflag && IsPerson(cptr))
			if (find_zap(cptr, 1))
				killflag = 1;
#ifdef R_LINES_OFTEN
		rflag = IsPerson(cptr) ? find_restrict(cptr) : 0;
#endif
		ping = IsRegistered(cptr) ? get_client_ping(cptr) :
					    CONNECTTIMEOUT;
		Debug((DEBUG_DEBUG, "c(%s)=%d p %d k %d r %d a %d",
		       cptr->name, cptr->status, ping, killflag, rflag,
		       currenttime - cptr->lasttime));
		/*
		 * Ok, so goto's are ugly and can be avoided here but this code
		 * is already indented enough so I think its justified. -avalon
		 */
		if (!killflag && !rflag && IsRegistered(cptr) &&
		    (ping >= currenttime - LAST_TIME(cptr)))
			goto ping_timeout;
		/*
		 * If the server hasnt talked to us in 2*ping seconds
		 * and it has a ping time, then close its connection.
		 * If the client is a user and a KILL line was found
		 * to be active, close this connection too.
		 */
		if (killflag || rflag ||
		    ((currenttime - LAST_TIME(cptr)) >= (2 * ping) &&
		     (ClientFlags(cptr) & FLAGS_PINGSENT)) ||
		    (!IsRegistered(cptr) &&
		     (currenttime - cptr->firsttime) >= ping)) {
#ifdef ENABLE_SOCKSCHECK
                        if (cptr->socks && cptr->socks->fd >= 0) {
                              int q;
                              for(q = highest_fd; q >= 0; q--) {
                                  if (!local[q] || !local[q]->socks || local[q]==cptr)
					  continue;
                                  if (local[q]->socks == cptr->socks)
					  break;
                              }
                              if (q < 0) {
				      aSocks *flushlist = NULL, *socks, *snext;
				      remFromSocks(cptr->socks, &flushlist);
				      for (socks = flushlist; socks; socks = snext) {
					      snext = socks->next;
					      if (socks->fd >= 0)
						      closesocket(socks->fd);
					      socks->fd = -1;
					      irc_free(socks);
				      }
                              }			      
                              cptr->socks = NULL;
                              ClientFlags(cptr) &= ~FLAGS_SOCKS;
                        }
#endif

			if (!IsRegistered(cptr) && (DoingDNS(cptr) || DoingSocks(cptr)))
			    {
			      /*
				if (cptr->authfd >= 0)
				    {
					(void)close(cptr->authfd);
					cptr->authfd = -1;
					cptr->count = 0;
					*cptr->buffer = '\0';
				    }
			      */
				Debug((DEBUG_NOTICE,
					"DNS timeout %s",
					get_client_name(cptr,TRUE)));
				del_queries((char *)cptr);
				ClearDNS(cptr);
#if 0
				if (DoingSocks(cptr))
				{
					cptr->socks->status |= SOCK_DONE|SOCK_DESTROY;
				}
				else if (cptr->socks)
				{
					if (cptr->socks->fd >= 0) 
					{
						closesocket(cptr->socks->fd);
					}
					irc_free(cptr->socks);
					cptr->socks = NULL;
					ClientFlags(cptr) &= ~FLAGS_SOCK;
				}
#endif
				if (!DoingSocks(cptr))
				  SetAccess(cptr);

				cptr->firsttime = currenttime;
				cptr->lasttime = currenttime;
				continue;
			    }
			if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr))
			{
				sendto_ops("No response from %s, closing link",
					   get_client_name(cptr, FALSE));
				sendto_serv_butone(&me, ":%s GNOTICE :No response from %s, closing link",
					me.name, get_client_name(cptr, FALSE));
			}
			/*
			 * this is used for KILL lines with time restrictions
			 * on them - send a messgae to the user being killed
			 * first.
			 */
			if (killflag && IsPerson(cptr))
				sendto_ops("Kill line active for %s",
					   get_client_name(cptr, FALSE));

#if defined(R_LINES) && defined(R_LINES_OFTEN)
			if (IsPerson(cptr) && rflag)
				sendto_ops("Restricting %s, closing link.",
					   get_client_name(cptr,FALSE));
#endif
                         if (killflag)
                                (void)exit_client(cptr, cptr, &me,
                                  "User has been banned");
                         else {
#if defined(NOSPOOF) && defined(REQ_VERSION_RESPONSE) 
				 if (IsRegisteredUser(cptr) &&
				     !IsUserVersionKnown(cptr) &&
				     (NOW - cptr->lasttime) < ping) {
					return FailClientCheck(cptr);
				 }
				 else
#endif					 
                                (void)exit_client(cptr, cptr, &me,
                                  "Ping timeout");
			 }
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
#ifdef NOSPOOF		
/*		else if (!IsRegistered(cptr) && cptr->nospoof &&
			!DoingDNS(cptr) && !DoingSocks(cptr) &&
			!IsNotSpoof(cptr) && !SentNoSpoof(cptr) &&
			(currenttime - cptr->firsttime) >= 5)
		{
			NospoofText(cptr);
			SetSentNoSpoof(cptr);
		}*/
#endif			
ping_timeout:
		timeout = LAST_TIME(cptr) + ping;
		
		/*if (ping > 20 && !IsRegistered(cptr) &&
			!DoingDNS(cptr) && !DoingSocks(cptr) &&
			!IsNotSpoof(cptr) && !SentNoSpoof(cptr)
                    ) {
                    timeout = cptr->lasttime + (ping = 20);
                }*/

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
static	int	bad_command()
{
  (void)printf(
	 "Usage: ircd %s[-h servername] [-p portnumber] [-x loglevel] [-t]\n",
#ifdef CMDLINE_CONFIG
	 "[-f config] "
#else
	 ""
#endif
	 );
  (void)printf("Server not started\n\n");
  return (-1);
}

int	main(argc, argv)
int	argc;
char	*argv[];
{
	time_t	delay = 0, now;
	int	portarg = 0;
	aConfItem *aconf;

#ifdef  FORCE_CORE
	struct  rlimit corelim;
#endif

        update_time();
	sbrk0 = (char *)sbrk((size_t)0);

#ifdef	CHROOTDIR
	if (chdir(dpath)) {
		perror("chdir");
		exit(-1);
	}
	res_init();
	if (chroot(DPATH)) {
		(void)fprintf(stderr,"ERROR:  Cannot chdir/chroot\n");
		exit(5);
	}
#endif /*CHROOTDIR*/

	myargv = argv;
	(void)umask(077);                /* better safe than sorry --SRB */
	bzero((char *)&me, sizeof(me));
	fprintf(stderr, "Setting up signals...");
	setup_signals();
	fprintf(stderr, "done\n");
	initload();

#ifdef FORCE_CORE
        fprintf(stderr, "Removing corefile size limits...");
	corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &corelim))
	  printf("unlimit core size failed; errno = %d\n", errno);
        else
        fprintf(stderr, "done\n");

#endif

#ifdef USE_CASETABLES
	/* Set up the case tables */
	fprintf(stderr, "setting up casetables...");
	setup_match();
	fprintf(stderr, "done\n");
#endif

	/*
	** All command line parameters have the syntax "-fstring"
	** or "-f string" (e.g. the space is optional). String may
	** be empty. Flag characters cannot be concatenated (like
	** "-fxyz"), it would conflict with the form "-fstring".
	*/
	fprintf(stderr, "Startup options: ");
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
		case 'f':
			bootopt &= ~BOOT_FORK;
			fprintf(stderr, "Will not fork()");
			break;

		case 's':
			fprintf(stderr, "\n");
			printf("sizeof(anUser) == %lu\t\t",
			       (unsigned long)sizeof(anUser));
			printf("sizeof(aClient) == %lu\t\t",
			       (unsigned long)sizeof(aClient));
			printf("sizeof(aClient) == %lu\n",
			       (unsigned long)sizeof(aClient));
			printf("sizeof(Link) == %lu\t\t",
			       (unsigned long)sizeof(Link));
			printf("sizeof(aServer) == %lu\t\t",
			       (unsigned long)sizeof(aServer));
			printf("sizeof(dbuf) == %lu\n",
			       (unsigned long)sizeof(dbuf));
			printf("\n");
			printf("sizeof(aChannel) == %lu\t\t",
			       (unsigned long)sizeof(aChannel));
			printf("bans maxlength: %u maxn# %u"
			       " banstruct %lu*bans\n",
			       MAXBANLENGTH, MAXBANS,
			       (unsigned long)sizeof(Link));
			printf("invites %lu*numinvites\n",
			       (unsigned long)sizeof(Link));
			exit(0);

		case 'v':
			fprintf(stderr, "\n");
			(void)printf("ircd %s\n", version);
			exit(0);

		case 'x':
#ifdef	DEBUGMODE
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			bootopt |= BOOT_DEBUG;
			break;
#else
			(void)fprintf(stderr,
				      "%s: DEBUGMODE must be defined for -x\n",
				      myargv[0]);
			exit(0);
#endif

		default:
			fprintf(stderr, "\n");
			bad_command();
			break;
		}
	    }
	fprintf(stderr, "\n");

#ifndef	CHROOT
	if (chdir(dpath)) {
		perror("chdir");
		exit(-1);
	}
#endif

        /* read_help (0); */ /* read the helpfile and attach it into memory... */
	open_logs();

	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr,	"ERROR: do not run ircd setuid root.\n");
		exit(-1);
	}

	if (argc > 0)
		return bad_command(); /* This should exit out */

	fprintf(stderr, "Cleaning hash tables...");
	clear_client_hash_table();
	clear_channel_hash_table();
        boot_replies();

	fprintf(stderr, "done\n");
	msgtab_buildhash();

	fprintf(stderr, "Initializing lists...");

	initlists();
	initclass();
	initwhowas();
	initstats();
	fprintf(stderr, "done\n");
	if (portnum < 0)
		portnum = PORTNUM;
	me.port = portnum;
	fprintf(stderr, "Pre-socket startup done.\n");
	(void)init_sys();
	me.flags = FLAGS_LISTEN;
	me.fd = -1;

#ifdef USE_SYSLOG
	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	if (initconf(bootopt) == -1) {
		fprintf(stderr, "error opening ircd config file: %s\n",
			configfile);
		Debug((DEBUG_FATAL, "Failed in reading configuration file %s",
		       configfile));
		(void)printf("Couldn't open configuration file %s\n",
			     configfile);
		exit(-1);
	}

	if ((aconf = find_me()) && portarg <= 0 && aconf->port > 0)
		portnum = aconf->port;
	Debug((DEBUG_ERROR, "Port = %d", portnum));
	if (inetport(&me, aconf->passwd, portnum)) {
		fprintf(stderr,
			"Error listening on port %d\n", portnum);
		exit(1);
	}

	(void)setup_ping();
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

       open_checkport();

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

#ifdef ENABLE_SOCKSCHECK
		if (now >= nextsockflush)
		{
			(void)flush_socks(now, 0);
			nextsockflush = (now+3);
		}
#endif		

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

static	void	setup_signals()
{
#ifdef	POSIX_SIGNALS
	struct	sigaction act;

	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
	(void)sigaddset(&act.sa_mask, SIGALRM);
# ifdef	SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
	(void)sigaction(SIGWINCH, &act, NULL);
# endif
	(void)sigaction(SIGPIPE, &act, NULL);
	act.sa_handler = dummy;
	(void)sigaction(SIGALRM, &act, NULL);
	act.sa_handler = s_rehash;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
	act.sa_handler = s_restart;
	(void)sigaddset(&act.sa_mask, SIGINT);
	(void)sigaction(SIGINT, &act, NULL);
	act.sa_handler = s_die;
	(void)sigaddset(&act.sa_mask, SIGTERM);
	(void)sigaction(SIGTERM, &act, NULL);

#else
# ifndef	HAVE_RELIABLE_SIGNALS
	(void)signal(SIGPIPE, dummy);
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# else
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, SIG_IGN);
#  endif
	(void)signal(SIGPIPE, SIG_IGN);
# endif
	(void)signal(SIGALRM, dummy);   
	(void)signal(SIGHUP, s_rehash);
	(void)signal(SIGTERM, s_die); 
	(void)signal(SIGINT, s_restart);
#endif

#ifdef RESTARTING_SYSTEMCALLS
	/*
	** At least on Apollo sr10.1 it seems continuing system calls
	** after signal is the default. The following 'siginterrupt'
	** should change that default to interrupting calls.
	*/
	(void)siginterrupt(SIGALRM, 1);
#endif
}
