/* $Id$ */

/*
 *   IRC - Internet Relay Chat, include/config.h
 *   Copyright (C) 1990 Jarkko Oikarinen
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

#ifndef	__config_include__
#define	__config_include__

#include "setup.h"
#include "options.h"

/*
 *
 *   NOTICE
 *
 * Under normal conditions, you should not have to edit this file.  Run
 * the Config script in the root directory instead!
 *
 * Windows is not a normal condition, edit this file if you use it. :-)
 *
 *
 */

/*
 * If we are using GNUC, __inline is ok.  Otherwise, remove it.
 */
#if !defined(__GNUC__)
#define __inline
#endif

/**
 * Number of seconds a client has to reply to CTCP version prior
 * to disconnection
 */
#define MAX_NOVERSION_DELAY 300

#define	MAXKILLS 25     /* maximum # of people listed per kill   */
#define	MAXHURTS 25     /* maximum # of people listed in a /hurt */
#undef	BOOT_MSGS
#define	HASH_MSGTAB
#define	ALLOW_MODEHACK	/* enable Modehack operator flag */
#define SERVICES_NAME	"services.sorcery.net"

/*
 *  URL people denied access as result of open socks server should be sent to
 *  for help
 */
#define SOCKSFOUND_URL   "http://www.sorcery.net/help/open_socks.html"

/* Timed K-line support - Timed K-lines are K-lines that only are active
   certain times of day.  This may be helpful so some, but they eat up the
   *most* CPU time in ircd.  They take more time than a WHOIS *as?f?s?d*.
   Timed K-lines are rarely used, so now they are an optional check.  Note
   that just because you disable them, they will still work partially, so
   don't disable them then have timed K-lines in ircd.conf.

   Change #undef to #define if you want them. -- Barubary */
#undef TIMED_KLINES

/* Multiple case-table support - Enable this if you need to make a transition
   from one case table to another.  It slightly increases the time needed to
   perform a match() call, so should be avoided.  Also, it can cause some
   network instability if everyone isn't using the same case table, so
   only do this on a network-wide basis.       -- Aeto. */
#undef USE_CASETABLES

/*
 * No spoof code
 *
 * This enables the spoof protection.
 */
/* #define NOSPOOF 1 */

#ifdef NOSPOOF
/*
 *
 * This controls the "nospoof" system.  These numbers are "seeds" of the
 * "random" number generating formula.  Choose any number you like in the
 * range of 0x00000000 to 0xFFFFFFFF.  Don't tell anyone these numbers, and
 * don't use the default ones.  Change both #define NOSPOOF... lines below.
 *
 * Other data is mixed in as well, but these guarantee a per-server secret.
 * Also, these values need not remain constant over compilations...  Change
 * them as often as you like.
 */
#ifndef NOSPOOF_SEED01
#define NOSPOOF_SEED01 0x12345678
#endif
#ifndef NOSPOOF_SEED02
#define NOSPOOF_SEED02 0x87654321
#endif

/* NS_ADDRESS
 *
 * This is the email address displayed to the user so that they can email
 * someone if they have problems with the no spoofing system.  It is
 * usually set up by the Config script.
 *
 * For SorceryNet servers, this should be set to nospoof@sorcery.net.  For 
 * other servers, it should be a valid email address for the users to contact.
 */
#ifndef NS_ADDRESS
#define NS_ADDRESS "clueless-admin@poorly.configured.server"
#endif
#endif

/* KLINE_ADDRESS
 *
 * This is the email address displayed to the user when they are K:lined
 * so that they can email someone in the server's administration about it.
 * It is usually set up by the Config script.
 *
 * It should be a valid email address for the users to contact.
 *
 * For SorceryNet servers, note that this message is displayed when the user
 * is affected by a local K:line or k:line.  For Services-based autokills,
 * the message is set up automatically by Services to ask to email
 * kline@sorcery.net.  It is recommended that you set this up to give a valid
 * email address for the server's admin, not kline@sorcery.net.
 */
#ifndef KLINE_ADDRESS
#define KLINE_ADDRESS "clueless-admin@poorly.configured.server"
#endif

/*
 * HOSTILENAME - Define this if you want the hostile username patch included,
 *		 it will strip characters that are not 0-9,a-z,A-Z,_,- or .
 */
#define HOSTILENAME	/* */

/*
 * NOTE: It is important to set this to the correct "domain" for your server.
 * Define this for the correct "domain" that your server is in.  This
 * is important for certain stats.  -mlv
 */
#ifndef DOMAINNAME
#define DOMAINNAME "sorcery.net"
#endif

/*
 * Define this if you wish to output a *file* to a K lined client rather
 * than the K line comment (the comment field is treated as a filename)
 */
#undef	COMMENT_IS_FILE


/*
 * read/write are restarted after signals defining this 1, gets
 * siginterrupt call compiled, which attempts to remove this
 * behaviour (apollo sr10.1/bsd4.3 needs this)
 */
#ifdef APOLLO
#define	RESTARTING_SYSTEMCALLS
#endif

/*
 * If your host supports varargs and has vsprintf(), vprintf() and vscanf()
 * C calls in its library, then you can define USE_VARARGS to use varargs
 * instead of imitation variable arg passing.
#undef	USE_VARARGS
 * NOTE: with current server code, varargs doesn't survive because it can't
 *       be used in a chain of 3 or more funtions which all have a variable
 *       number of params.  If anyone has a solution to this, please notify
 *       the maintainer.
 */

#if 0
#define	DEBUGMODE	/* define DEBUGMODE to enable debugging mode.*/
#endif

/*
 * Full pathnames and defaults of irc system's support files. Please note that
 * these are only the recommened names and paths. Change as needed.
 * You must define these to something, even if you don't really want them.
 */
#ifndef DPATH
#define	DPATH	"/usr/local/lib/ircd"	/* dir where all ircd stuff is */
#endif
#ifndef SPATH
#define	SPATH	"/usr/local/bin/ircd"	/* path to server executeable */
#endif
#define	CPATH	"ircd.conf"	/* server configuration file */
#define	MPATH	"ircd.motd"	/* server MOTD file */
#define	LPATH	"debug.log"	/* Where the debug file lives, if DEBUGMODE */
#define	PPATH	"ircd.pid"	/* file for server pid */

/*
 * this define sets the filename to maintain a list of persons who log
 * into this server. Logging will stop when the file does not exist.
 * FNAME_USERLOG just logs user connections, FNAME_OPERLOG logs oper actions
 * These are either full paths or files within DPATH.
 */
#define	IRC_LOGGING
#define FNAME_USERLOG "users.log"
#define FNAME_OPERLOG "opers.log"

/* FAILOPER_WARN
 *
 * When defined, warns users on a failed oper attempt that it was/is logged
 * Only works when FNAME_OPERLOG is defined, and a logfile exists.
 * NOTE: Failed oper attempts are logged regardless.
 */
#define FAILOPER_WARN

/* OPER_* defines
 *
 * See ./docs/example.conf for examples of how to restrict access for
 * your IRC Operators
 */
#define STRICT_LOCOPS   /* define this if you don't want locops able
                           to use remote /hurts */


/* MAXIMUM LINKS
 *
 * This define is useful for leaf nodes and gateways. It keeps you from
 * connecting to too many places. It works by keeping you from
 * connecting to more than "n" nodes which you have C:blah::blah:6667
 * lines for.
 *
 * Note that any number of nodes can still connect to you. This only
 * limits the number that you actively reach out to connect to.
 *
 * Leaf nodes are nodes which are on the edge of the tree. If you want
 * to have a backup link, then sometimes you end up connected to both
 * your primary and backup, routing traffic between them. To prevent
 * this, #define MAXIMUM_LINKS 1 and set up both primary and
 * secondary with C:blah::blah:6667 lines. THEY SHOULD NOT TRY TO
 * CONNECT TO YOU, YOU SHOULD CONNECT TO THEM.
 *
 * Gateways such as the server which connects Australia to the US can
 * do a similar thing. Put the American nodes you want to connect to
 * in with C:blah::blah:6667 lines, and the Australian nodes with
 * C:blah::blah lines. Have the Americans put you in with C:blah::blah
 * lines. Then you will only connect to one of the Americans.
 *
 * This value is only used if you don't have server classes defined, and
 * a server is in class 0 (the default class if none is set).
 *
 */
#define MAXIMUM_LINKS 1

/*
 * If your server is running as a a HUB Server then define this.
 * A HUB Server has many servers connect to it at the same as opposed
 * to a leaf which just has 1 server (typically the uplink). Define this
 * correctly for performance reasons.
 */
/* #define	HUB */

/*
 * If you wish to have the server send 'vital' messages about server
 * through syslog, define USE_SYSLOG. Only system errors and events critical
 * to the server are logged although if this is defined with FNAME_USERLOG,
 * syslog() is used instead of the above file. It is not recommended that
 * this option is used unless you tell the system administrator beforehand
 * and obtain their permission to send messages to the system log files.
 */
#undef	USE_SYSLOG

#ifdef	USE_SYSLOG
/*
 * If you use syslog above, you may want to turn some (none) of the
 * spurious log messages for KILL/SQUIT off.
 */
#undef	SYSLOG_KILL	/* log all operator kills to syslog */
#undef	SYSLOG_SQUIT	/* log all remote squits for all servers to syslog */
#undef	SYSLOG_CONNECT	/* log remote connect messages for other all servs */
#undef	SYSLOG_USERS	/* send userlog stuff to syslog */
#undef	SYSLOG_OPER	/* log all users who successfully become an Op */

/*
 * If you want to log to a different facility than DAEMON, change
 * this define.
 */
#define LOG_FACILITY LOG_DAEMON
#endif /* USE_SYSLOG */

/*
 * define this if you want to use crypted passwords for operators in your
 * ircd.conf file. See ircd/crypt/README for more details on this.
 */
/* #define	CRYPT_OPER_PASSWORD */

/*
 * If you want to store encrypted passwords in N-lines for server links,
 * define this.  For a C/N pair in your ircd.conf file, the password
 * need not be the same for both, as long as hte opposite end has the
 * right password in the opposite line.  See INSTALL doc for more details.
 */
/* #undef	CRYPT_LINK_PASSWORD */

/*
 * IDLE_FROM_MSG
 *
 * Idle-time nullified only from privmsg, if undefined idle-time
 * is nullified from everything except ping/pong.
 * Added 3.8.1992, kny@cs.hut.fi (nam)
 */
#define IDLE_FROM_MSG

/* 
 * Size of the LISTEN request.  Some machines handle this large
 * without problem, but not all.  It defaults to 5, but can be
 * raised if you know your machine handles it.
 */
#ifndef LISTEN_SIZE
#define LISTEN_SIZE 5
#endif

/*
 * Max amount of internal send buffering when socket is stuck (bytes)
 */
#ifndef MAXSENDQLENGTH
#define MAXSENDQLENGTH 3000000
#endif
/*
 *  BUFFERPOOL is the maximum size of the total of all sendq's.
 *  Recommended value is 2 * MAXSENDQLENGTH, for hubs, 5 *.
 */
#ifndef BUFFERPOOL
#define	BUFFERPOOL     (9 * MAXSENDQLENGTH)
#endif

/*
 * CLIENT_FLOOD
 *
 * this controls the number of bytes the server will allow a client to
 * send to the server without processing before disconnecting the client for
 * flooding it.  Values greater than 8000 make no difference to the server.
 */
#define	CLIENT_FLOOD	8000

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

/*
 * Port where ircd resides. NOTE: This *MUST* be greater than 1024 if you
 * plan to run ircd under any other uid than root.
 */
#define PORTNUM 9000		/* 9000 for SorceryNet */

/*
 * Maximum number of network connections your server will allow.  This should
 * never exceed max. number of open file descrpitors and wont increase this.
 * Should remain LOW as possible. Most sites will usually have under 30 or so
 * connections. A busy hub or server may need this to be as high as 50 or 60.
 * Making it over 100 decreases any performance boost gained from it being low.
 * if you have a lot of server connections, it may be worth splitting the load
 * over 2 or more servers.
 * 1 server = 1 connection, 1 user = 1 connection.
 * This should be at *least* 3: 1 listen port, 1 dns port + 1 client
 *
 * Note: this figure will be too high for most systems. If you get an 
 * fd-related error on compile, change this to 256.
 *
 * Windows users: This should be a fairly high number.  Some operations
 * will slow down because of this, but it is _required_ because of the way
 * windows NT(and possibly 95) allocate fd handles. A good number is 16384.
 */
#ifndef MAXCONNECTIONS
#define MAXCONNECTIONS	1024
#endif

/*
 * this defines the length of the nickname history.  each time a user changes
 * nickname or signs off, their old nickname is added to the top of the list.
 * The following sizes are recommended:
 * 8MB or less  core memory : 500	(at least 1/4 of max users)
 * 8MB-16MB     core memory : 500-750	(1/4 -> 1/2 of max users)
 * 16MB-32MB    core memory : 750-1000	(1/2 -> 3/4 of max users)
 * 32MB or more core memory : 1000+	(> 3/4 if max users)
 * where max users is the expected maximum number of users.
 * (100 nicks/users ~ 25k)
 * NOTE: this is directly related to the amount of memory ircd will use whilst
 *	 resident and running - it hardly ever gets swapped to disk! You can
 *	 ignore these recommendations- they only are meant to serve as a guide
 * NOTE: But the *Minimum* ammount should be 100, in order to make nick
 *       chasing possible for mode and kick.
 */
#ifndef NICKNAMEHISTORYLENGTH
#define NICKNAMEHISTORYLENGTH 2000 
#endif

/*
 * Time interval to wait and if no messages have been received, then check for
 * PINGFREQUENCY and CONNECTFREQUENCY 
 */
#define TIMESEC  60		/* Recommended value: 60 */

/*
 * If daemon doesn't receive anything from any of its links within
 * PINGFREQUENCY seconds, then the server will attempt to check for
 * an active link with a PING message. If no reply is received within
 * (PINGFREQUENCY * 2) seconds, then the connection will be closed.
 */
#define PINGFREQUENCY    120	/* Recommended value: 120 */

/*
 * If the connection to to uphost is down, then attempt to reconnect every 
 * CONNECTFREQUENCY  seconds.
 */
#define CONNECTFREQUENCY 600	/* Recommended value: 600 */

/*
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 */
#define HANGONRETRYDELAY 20	/* Recommended value: 20 seconds */
#define HANGONGOODLINK 300	/* Recommended value: 5 minutes */

/*
 * Number of seconds to wait for write to complete if stuck.
 */
#define WRITEWAITDELAY     15	/* Recommended value: 15 */

/*
 * Number of seconds to wait for a connect(2) call to complete.
 * NOTE: this must be at *LEAST* 10.  When a client connects, it has
 * CONNECTTIMEOUT - 10 seconds for its host to respond to an ident lookup
 * query and for a DNS answer to be retrieved.
 */
#define	CONNECTTIMEOUT	90	/* Recommended value: 90 */

/*
 * Max time from the nickname change that still causes KILL
 * automaticly to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90   /* Recommended value: 90 */

/*
 * Max number of channels a user is allowed to join.
 */
#define MAXCHANNELSPERUSER  10	/* Recommended value: 10 */

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MOTD MPATH
#define	MYNAME SPATH
#define	CONFIGFILE CPATH
#define	IRCD_PIDFILE PPATH

#ifdef	__osf__
#define	OSF
/* OSF defines BSD to be its version of BSD */
#undef BSD
#include <sys/param.h>
#ifndef BSD
#define BSD
#endif
#endif

#ifndef KLINE_TEMP
#define KLINE_PERM 0
#define KLINE_TEMP 1
#define KLINE_AKILL 2
#endif

#ifdef DEBUGMODE
# define Debug(x) debug x
# define LOGFILE LPATH
#else
# define Debug(x) ;
# define LOGFILE "/dev/null"
#endif

/*
 * safety margin so we can always have one spare fd, for motd/authd or
 * whatever else.  -4 allows "safety" margin of 1 and space reserved.
 */
#define	MAXCLIENTS	(MAXCONNECTIONS-4)

#if defined(CLIENT_FLOOD)
#  if	(CLIENT_FLOOD > 8000)
#    define CLIENT_FLOOD 8000
#  else
#    if (CLIENT_FLOOD < 512)
error CLIENT_FLOOD needs redefining.
#    endif
#  endif
#else
error CLIENT_FLOOD undefined
#endif
#if (NICKNAMEHISTORYLENGTH < 100)
#  define NICKNAMEHISTORYLENGTH 100
#endif

#ifndef SOCKSFOUND_URL
#error SOCKSFOUND_URL is not defined: Please define in config.h
error SOCKSFOUND_URL is not defined: Please define in config.h
#endif

#endif /* __config_include__ */
