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

#ifndef lint
static	char sccsid[] = "@(#)ircd.c	2.48 3/9/94 (C) 1988 University of "
"Oulu, Computing Center and Jarkko Oikarinen";
static	char rcsid[] = "$Id$";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "userload.h"
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/file.h>
#include <pwd.h>
#include <sys/time.h>
#else
#include <io.h>
#include <direct.h>
#endif
#ifdef HPUX
#define _KERNEL            /* HPUX has the world's worst headers... */
#endif
#ifndef _WIN32
#include <sys/resource.h>
#endif
#ifdef HPUX
#undef _KERNEL
#endif
#include <errno.h>
#include "h.h"

aClient me;			/* That's me */
aClient *client = &me;		/* Pointer to beginning of Client list */

void NospoofText(aClient* acptr);

time_t    NOW, tm_offset = 0;
void	server_reboot(char *);
void	restart PROTO((char *));
static	void	open_debugfile(), setup_signals();

char	**myargv;
int	portnum = -1;		    /* Server port number, listening this */
char	*configfile = CONFIGFILE;	/* Server configuration file */
int	debuglevel = -1;		/* Server debug level */
int	bootopt = 0;			/* Server boot option flags */
char	*debugmode = "";		/*  -"-    -"-   -"-  */
char	*sbrk0;				/* initial sbrk(0) */
static	int	dorehash = 0;
static	char	*dpath = DPATH;

time_t	nextconnect = 1;	/* time for next try_connections call */
time_t	nextping = 1;		/* same as above for check_pings() */
time_t	nextdnscheck = 0;	/* next time to poll dns to force timeouts */
time_t	nextexpire = 1;		/* next expire run on the dns cache */
time_t  nextsockflush = 1;
void	boot_replies(void);

#if	defined(PROFIL) && !defined(_WIN32)
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


VOIDSIG s_die()
{
#ifdef	USE_SYSLOG
	(void)syslog(LOG_CRIT, "Server Killed By SIGTERM");
#endif
	flush_connections(me.fd);
	close_logs( );
	exit(-1);
}

#ifndef _WIN32
static VOIDSIG s_rehash()
#else
VOIDSIG s_rehash()
#endif
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
# ifndef _WIN32
	(void)signal(SIGHUP, s_rehash);	/* sysV -argv */
# endif
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
	/*
	** fd 0 must be 'preserved' if either the -d or -i options have
	** been passed to us before restarting.
	*/
#ifdef USE_SYSLOG
	(void)closelog();
#endif
        close_logs( );

#ifndef _WIN32
	for (i = 3; i < MAXCONNECTIONS; i++)
		(void)close(i);
	if (!(bootopt & (BOOT_TTY|BOOT_DEBUG)))
		(void)close(2);
	(void)close(1);
	if ((bootopt & BOOT_CONSOLE) || isatty(0))
		(void)close(0);
	if (!(bootopt & (BOOT_INETD|BOOT_OPER)))
		(void)execv(MYNAME, myargv);
#else
	for (i = 0; i < highest_fd; i++)
		if ( closesocket(i) == -1 ) close(i);

	(void)execv(myargv[0], myargv);
#endif
#ifdef USE_SYSLOG
	/* Have to reopen since it has been closed above */

	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
	syslog(LOG_CRIT, "execv(%s,%s) failed: %m\n", MYNAME, myargv[0]);
	closelog();
#endif
#ifndef _WIN32
	Debug((DEBUG_FATAL,"Couldn't restart server: %s", strerror(errno)));
#else
	Debug((DEBUG_FATAL,"Couldn't restart server: %s", strerror(GetLastError())));
#endif
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
	aClient *cptr, *xcptr;
	int	connecting, confrq;
	int	con_class = 0, i = 0;
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
	if (connecting)
           for ( i = highest_fd; i > 0; i--)
               if (!( xcptr = local[i] )) continue;
               else 
                 if ( IsServer(xcptr) )
                 {
                     connecting = FALSE;
                     break;
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
				   (struct hostent *)NULL) == 0)
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
extern	time_t	check_pings(time_t currenttime, int check_kills, aConfItem *conf_target)
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
	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(cptr = local[i]) || IsMe(cptr) || IsLog(cptr))
			continue;

		/*
		** Note: No need to notify opers here. It's
		** already done when "FLAGS_DEADSOCKET" is set.
		*/
		if (ClientFlags(cptr) & FLAGS_DEADSOCKET)
		    {
			(void)exit_client(cptr, cptr, &me, "Dead socket");
			i--;  /* catch re-mapped fds */
			continue;
		    }

		if (check_kills)
			killflag = IsPerson(cptr) ? find_kill(cptr, conf_target) : 0;
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
			currenttime - LAST_TIME(cptr)));
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
		     (currenttime - LAST_TIME(cptr)) >= ping))
		    {
#ifdef ENABLE_SOCKSCHECK
                        if (cptr->socks && cptr->socks->fd >= 0)
                        {
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
                                  for (socks = flushlist; socks; socks = snext)
                                  {
                                        snext = socks->next;
                                        if (socks->fd >= 0)
                                            closesocket(socks->fd);
                                        socks->fd = -1;
                                        MyFree(socks);
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
#ifndef _WIN32
					(void)close(cptr->authfd);
#else
					(void)closesocket(cptr->authfd);
#endif
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
					MyFree(cptr->socks);
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
			i--;  /* catch re-mapped fds */
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
#ifndef _WIN32
  (void)printf(
	 "Usage: ircd %s[-h servername] [-p portnumber] [-x loglevel] [-t]\n",
#ifdef CMDLINE_CONFIG
	 "[-f config] "
#else
	 ""
#endif
	 );
  (void)printf("Server not started\n\n");
#else
  MessageBox(NULL,
         "Usage: wircd [-h servername] [-p portnumber] [-x loglevel]\n",
         "wIRCD", MB_OK);
#endif
  return (-1);
}

#ifdef BOOT_MSGS
#define loadmsg printf
#else
void null_func(void *x, ...) {return; x=NULL;}
#define loadmsg while(0) null_func
#endif

#ifndef _WIN32
int	main(argc, argv)
#else
int	InitwIRCD(argc, argv)
#endif
int	argc;
char	*argv[];
{
#ifdef _WIN32
	WORD    wVersionRequested = MAKEWORD(1, 1);
	WSADATA wsaData;
#else
	time_t	delay = 0, now;
#endif
	int	portarg = 0;
#ifdef  FORCE_CORE
	struct  rlimit corelim;
#endif

        update_time();
#ifndef _WIN32
	sbrk0 = (char *)sbrk((size_t)0);
# ifdef	PROFIL
	(void)monstartup(0, etext);
	(void)moncontrol(1);
	(void)signal(SIGUSR1, s_monitor);
# endif
#endif

#ifdef	CHROOTDIR
	if (chdir(dpath))
	    {
		perror("chdir");
		exit(-1);
	    }
	res_init();
	if (chroot(DPATH))
	  {
	    (void)fprintf(stderr,"ERROR:  Cannot chdir/chroot\n");
	    exit(5);
	  }
#endif /*CHROOTDIR*/

	myargv = argv;
#ifndef _WIN32
	(void)umask(077);                /* better safe than sorry --SRB */
#else
	WSAStartup(wVersionRequested, &wsaData);
#endif
	bzero((char *)&me, sizeof(me));
	loadmsg("Setting up signals...");
#ifndef _WIN32
	setup_signals();
#endif
	loadmsg("done\n");
	initload();

#ifdef FORCE_CORE
        loadmsg("Removing corefile size limits...");
	corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &corelim))
	  printf("unlimit core size failed; errno = %d\n", errno);
        else
        loadmsg("done\n");

#endif

#ifdef USE_CASETABLES
	/* Set up the case tables */
	loadmsg("setting up casetables...");
	setup_match();
	loadmsg("done\n");
#endif

	/*
	** All command line parameters have the syntax "-fstring"
	** or "-f string" (e.g. the space is optional). String may
	** be empty. Flag characters cannot be concatenated (like
	** "-fxyz"), it would conflict with the form "-fstring".
	*/
	loadmsg("Startup options: ");
	while (--argc > 0 && (*++argv)[0] == '-')
	    {
		char	*p = argv[0]+1;
		int	flag = *p++;

		if (flag == '\0' || *p == '\0')
			if (argc > 1 && argv[1][0] != '-')
			    {
				p = *++argv;
				argc -= 1;
			    }
			else
				p = "";

		switch (flag)
		    {
#ifndef _WIN32
                    case 'a':
			loadmsg("Autodie ");
			bootopt |= BOOT_AUTODIE;
			break;
		    case 'c':
			loadmsg("Console ");
			bootopt |= BOOT_CONSOLE;
			break;
		    case 'q':
			loadmsg("QuickBoot ");
			bootopt |= BOOT_QUICK;
			break;
#else
		    case 'd':
#endif
			dpath = p;
			break;
#ifndef _WIN32
		    case 'o': /* Per user local daemon... */
			loadmsg("Local[Oper] ");
			bootopt |= BOOT_OPER;
		        break;
#ifdef CMDLINE_CONFIG
		    case 'f':
			configfile = p;
			loadmsg("Config[%s] ", configfile);
			break;
#endif
		    case 'h':
			loadmsg("Name[%s] ", p);
			strncpyzt(me.name, p, sizeof(me.name));
			break;
		    case 'i':
			loadmsg("InetdBoot ");
			bootopt |= BOOT_INETD|BOOT_AUTODIE;
		        break;
#endif
		    case 'p':
			if ((portarg = atoi(p)) > 0 )
				portnum = portarg;
			loadmsg("Port[%d] ", portnum);
			break;
		    case 's':
			 loadmsg("\n");
		         printf("sizeof(anUser) == %lu \t\t", sizeof(anUser));	
		         printf("sizeof(aClient) == %lu \t\t", sizeof(aClient));	
		         printf("sizeof(aClient) == %lu \n", sizeof(aClient));	
		         printf("sizeof(Link) == %lu \t\t", sizeof(Link));	
		         printf("sizeof(aServer) == %lu \t\t", sizeof(aServer));	
		         printf("sizeof(dbuf) == %lu \n", sizeof(dbuf));	
		         printf("\n", sizeof(dbuf));	
		         printf("sizeof(aChannel) == %lu \t\t", sizeof(aChannel));	
		         printf("bans maxlength: %lu maxn# %lu banstruct %lu*bans\n", MAXBANLENGTH, MAXBANS, sizeof(Link));	
		         printf("invites %lu*numinvites \n", sizeof(Link));	

                            exit(0);
#ifndef _WIN32
		    case 't':
			loadmsg("tty ");
			bootopt |= BOOT_TTY;
			break;
		    case 'v':
			loadmsg("\n");
			(void)printf("ircd %s\n", version);
#else
		    case 'v':
			loadmsg("\n");
			MessageBox(NULL, version, "wIRCD version", MB_OK);
#endif
			exit(0);
		    case 'x':
#ifdef	DEBUGMODE
			debuglevel = atoi(p);
			debugmode = *p ? p : "0";
			bootopt |= BOOT_DEBUG;
			break;
#else
# ifndef _WIN32
			(void)fprintf(stderr,
				"%s: DEBUGMODE must be defined for -x y\n",
				myargv[0]);
# else
			MessageBox(NULL,
				"DEBUGMODE must be defined for -x option",
				"wIRCD", MB_OK);
# endif
			exit(0);
#endif
		    default:
			loadmsg("\n");
			bad_command();
			break;
		    }
	    }
	loadmsg("\n");

#ifndef	CHROOT
	if (chdir(dpath)) {
# ifndef _WIN32
		perror("chdir");
# else
		MessageBox(NULL, strerror(GetLastError()), "wIRCD: chdir()",
			   MB_OK);
# endif
		exit(-1);
	}
#endif

        /* read_help (0); */ /* read the helpfile and attach it into memory... */
	open_logs( );

#if !defined(_WIN32)
	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr,	"ERROR: do not run ircd setuid root.\n");
		exit(-1);
	}
#endif

#ifndef _WIN32
	/* didn't set debuglevel */
	/* but asked for debugging output to tty */
	if ((debuglevel < 0) &&  (bootopt & BOOT_TTY))
	    {
		(void)fprintf(stderr,
			"you specified -t without -x. use -x <n>\n");
		exit(-1);
	    }
#endif

	if (argc > 0)
		return bad_command(); /* This should exit out */

	loadmsg("Cleaning hash tables...");
	clear_client_hash_table();
	clear_channel_hash_table();
        boot_replies();

	loadmsg("done\n");
#ifdef HASH_MSGTAB
	(void)msgtab_buildhash();
#endif
	loadmsg("Initializing lists...");

	initlists();
	initclass();
	initwhowas();
	initstats();
	loadmsg("done\n");
	open_debugfile();
	if (portnum < 0)
		portnum = PORTNUM;
	me.port = portnum;
	loadmsg("Pre-socket startup done, going into background.\n");
	(void)init_sys();
	me.flags = FLAGS_LISTEN;
#ifndef _WIN32
	if (bootopt & BOOT_INETD)
	    {
		me.fd = 0;
		local[0] = &me;
		me.flags = FLAGS_LISTEN;
	    }
	else
#endif
		me.fd = -1;

#ifdef USE_SYSLOG
	openlog(myargv[0], LOG_PID|LOG_NDELAY, LOG_FACILITY);
#endif
	if (initconf(bootopt) == -1)
	    {
		loadmsg("error opening ircd config file: %s\n", configfile);
		Debug((DEBUG_FATAL, "Failed in reading configuration file %s",
			configfile));
#ifndef _WIN32
		(void)printf("Couldn't open configuration file %s\n",
			configfile);
#else
		MessageBox(NULL, "Couldn't open configuration file "CONFIGFILE,
			"wIRCD", MB_OK);
#endif
		exit(-1);
	    }
	if (!(bootopt & BOOT_INETD))
	    {
		static	char	star[] = "*";
		aConfItem	*aconf;

		if ((aconf = find_me()) && portarg <= 0 && aconf->port > 0)
			portnum = aconf->port;
		Debug((DEBUG_ERROR, "Port = %d", portnum));
		if (inetport(&me, aconf->passwd, portnum))
		{
			loadmsg("Error listening on port %d\n", portnum);
			close(1);
			exit(1);
		}
	    }
	else if (inetport(&me, "*", 0))
	{
		loadmsg("Error listening on port %d.%d\n", portnum, me.port);
		close(1);
		exit(1);
	}
	if ((bootopt & BOOT_OPER)) close(1);

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
	if (bootopt & BOOT_OPER)
	    {
		aClient *tmp = add_connection(&me, 0);

		if (!tmp)
			exit(1);
		SetMaster(tmp);
	    }
	else
		write_pidfile();

       open_checkport();

       Debug((DEBUG_NOTICE,"Server ready..."));
#ifdef USE_SYSLOG
       syslog(LOG_NOTICE, "Server Ready");
#endif

#ifdef _WIN32
    return 1;
}

void    SocketLoop(void *dummy)
{
	time_t	delay = 0, now;

	while (1)
#else
	for (;;)
#endif
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

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by LPATH in config.h
 * Here we just open that file and make sure it is opened to fd 2 so that
 * any fprintf's to stderr also goto the logfile.  If the debuglevel is not
 * set from the command line by -x, use /dev/null as the dummy logfile as long
 * as DEBUGMODE has been defined, else dont waste the fd.
 */
static	void	open_debugfile()
{
#ifdef	DEBUGMODE
	int	fd;
	aClient	*cptr;

	if (debuglevel >= 0)
	    {
		cptr = make_client(NULL);
		cptr->fd = 2;
		SetLog(cptr);
		cptr->port = debuglevel;
		ClientFlags(cptr) = 0;
		ClientUmode(cptr) = 0;
		cptr->acpt = cptr;
		local[2] = cptr;
		(void)strcpy(cptr->sockhost, me.sockhost);
# ifndef _WIN32
		(void)printf("isatty = %d ttyname = %#x\n",
			isatty(2), (u_int)ttyname(2));
		if (!(bootopt & BOOT_TTY)) /* leave debugging output on fd 2 */
		    {
			(void)truncate(LOGFILE, 0);
			if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0) 
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);
			if (fd != 2)
			    {
				(void)dup2(fd, 2);
				(void)close(fd); 
			    }
			strncpyzt(cptr->name, LOGFILE, sizeof(cptr->name));
		    }
		else if (isatty(2) && ttyname(2))
			strncpyzt(cptr->name, ttyname(2), sizeof(cptr->name));
		else
# endif
			(void)strcpy(cptr->name, "FD2-Pipe");
		Debug((DEBUG_FATAL, "Debug: File <%s> Level: %d at %s",
			cptr->name, cptr->port, myctime(NOW)));
	    }
	else
		local[2] = NULL;
#endif
	return;
}

#ifndef _WIN32
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
#endif /* !_Win32 */
