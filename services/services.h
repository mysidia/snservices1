/**
 * \file services.h
 * \brief Services master header file
 *
 * This header includes indirectly some system-related files 
 * and declares global constants and other entities used throughout
 * all of services.
 *
 * \wd \taz \greg \mysid
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 2001 James Hess
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __SERVICES_H
#define __SERVICES_H

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifndef NORLIMIT
#include <sys/resource.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <arpa/inet.h>

#include <netinet/in.h>

#define FREE(x) do { free(x) ; (x) = NULL; } while (0)

#include "options.h"
#include "parse.h"
#include "struct.h"

#ifdef USE_SQL
#include <libpq-fe.h>
#endif

#ifndef _TESTMODE
/// DEBUG: Terminate if x is false with a fatal error
#define assert(x) \
        (!(x) ? fatalSvsError(__FILE__, __LINE__, #x) : 0)
#endif

#ifndef MAX
/**
 * \brief Macro that returns the highest value provided
 * \param x an integer
 * \param y an integer
 */
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
/**
 * \brief Macro that returns the lower value provided
 * \param x an integer
 * \param y an integer
 */
#define MIN(x, y) ( ( (x) < (y) ) ? (x) : (y) )
#endif

/**
 * \brief Copy N bytes from source to target string but ensure \0 (NUL)
 *        termination.
 * \param s_dest Target character array
 * \param s_src Source character array
 * \param s_size Number bytes to copy
 */
#define strncpyzt(s_dest, s_src, s_size)				\
do {									\
	strncpy((s_dest), (s_src), (s_size));				\
	(s_dest)[(s_size) - 1] = '\0';					\
} while(0)

/**
 * This is for the benefit of bigbrother, so it will know when to
 * not attempt to restart services.
 * \para  What is 'bigbrother'???
 */
#define FATAL_ERROR 66

#ifndef FALSE
          /// Not true
#define FALSE 0 
#endif

#ifndef TRUE
          /// Not false
#define TRUE !(FALSE)
#endif

/**
 * \bug This is a temporary hack...  Don't leave this in place long! [XXX Skan]
 */
/// Temporary hack
#define net_read(a,b,c)  read((a), (b), (c))

/// Temporary hack
#define net_write(a,b,c) write((a), (b), (c))

/**
 * The number of services entries present -- this MUST be right or services
 * WILL have lots of problems
 */
#define NUMSERVS 7

/**
 * debugging info? 
 */
#undef DEBUG

/**
 * debugging info on database calls? 
 */
#undef DBDEBUG

/**
 * debugging info for channel tracking? 
 */
#undef CDEBUG

#undef C_DEBUG2

/**
 * maximum events running in timer system at once... 
 */
#define MAXEVENTS 4096

/**
 * Send a globops when SRAs use /os raw
 */
#define GLOBOPS_ON_RAW

/// Version number string for services
#define VERSION_NUM VERSION

/// Version quote string
#define VERSION_QUOTE ("When you wake up you're all weak, throwing your life away")

/// Complete version string
#define VERSION_STRING (PACKAGE "-" VERSION_NUM "")

/// Channel for log output
#define LOGCHAN "#services"

/// Channel for debug output
#define DEBUGCHAN "#services-debug"

/// List of network channels
#define NETWORK_CHANNELS "#sorcery:" LOGCHAN ":" DEBUGCHAN ":"

/**
 * Send copyright notices.. define or undef 
 */
#undef COPYRIGHT

          /// Autokill typeflag
#define A_AKILL            0x01

          /// Services ignore typeflag
#define A_IGNORE           0x02

          /// Autohurt typeflag
#define A_AHURT		   0x04

          /// Clone trigger kill flag
#define CLONE_KILLFLAG     0x01

          /// Clone trigger ignore flag
#define CLONE_IGNOREFLAG   0x02

          /// Clone trigger perm flag
          /*! \bug purpose of CLONE_PERMTRIGGER??? */
#define CLONE_PERMTRIGGER  0x04

          /// Clone alert has been cleared
#define CLONE_OK	   0x08

          /// Has set off a clone alert
#define CLONE_ALERT	   0x10


/*
 * Functions used throughout services... 
 */

/*
 * main.c 
 */
void            flushLogs(char *);
void            expireChans(char *);
void            expireNicks(char *);
void            sshutdown(int);
void            checkTusers(char *);
void            readConf(void);

/*
 * server.c 
 */
void            addUser(char *, char *, char *, char *, char *);
int             ConnectToServer(char *, int);
void            sSend(char *, ...)
			__attribute__ ((format (printf, 1, 2)));
void            logDump(FILE *, char *, ...)
			__attribute__ ((format (printf, 2, 3)));
void            breakLine(char *);
void            handler(int);
char 	       *xorit(char *);
void		sendInfoReply(UserList *);

/*
 * timer.c 
 */
int             cancel_timer(int);
int             timer(long, void (*func) (char *), void *);
void            timeralarm(void);
void            dumptimer(char *from);

/*
 * db.c
 */ 
void		sync_cfg(char *);

/*
 * stuff.c 
 */
void		AppendBuffer(char **, const char *);
void		SetDynBuffer(char **, const char *);
void            breakString(int, char *, char *args[256], char);
int             match(const char *, const char *);
void            strtolower(char *);
void            doTfunc(char *);
char           *sfgets(char *, int, FILE *);
int		check_match(char *);
int		exp_match(char *, char *);
void		parse_str(char **, int, int, char *, size_t);
void            mask(char *, char *, int, char *);
void	        tzapply(char *);
int		split_userhost(const char *input_host, MaskData *data);
void		free_mask(MaskData *);
MaskData	*make_mask(void);
void	       *oalloc(size_t);
char           *genHostMask(char *);
char	       *flagstring(int flags, const char *bits[]);
int	       flagbit(char *, const char *bits[]);
char*	       str_dup(const char *);

/*
 * passwd.c
 */
unsigned char *toBase64(const unsigned char *stream, size_t left);
unsigned char *fromBase64(const char *cStr, int *len);

/*
 * akill.c
 */
void listAkills(char *from, char type);
int addakill(long length, char *mask, char *by, char type, char *reason);
int removeAkill(char *from, char *mask);
int removeAkillType(char *from, char *mask, int type, int restrict);
void saveakills(void);
void loadakills(void);
int  isAKilled(char *, char *, char *);
char *checkAndSetAKill(char *nick, char *user, char *host);
int  isAHurt(char *, char *, char *);
int  isIgnored(char *, char *, char *);
void timed_akill_queue(char *);
void    autoremoveakill(char *mask);
void    queueakill(char *, char *, char *, char *, time_t, int, int, int);
const   char * aktype_str(int type, int which);



/*
 * log.c
 */
void dlogInit(void);
void dlogEntry(char *, ...);
void dlogDump(FILE *);

/*
 * help.c
 */
void motd(char *);
void help(char *, char *, char **, int);
void flush_help_cache(void);
help_cache *check_help_cache (char *);

/*****************************************************************************/

#ifdef USE_SQL
extern PGconn *dbConn;
#endif

/*
 * my server name, and my server password 
 */
          /// Name of services
extern char            myname[255];

          /// Password of services' N-line
extern char            mypass[33];

          /// Hostname of services' uplink
extern char            hostname[255];

          /// IRC Port of services' uplink
extern int             port;

          /// File descriptor of services' connection with the uplink
extern int             server;

          /// Array of services and their data... see above for Service struct
extern Service         services[NUMSERVS];

          /// Database files
extern database        db;

          /// Total online users
extern u_long          totalusers;

          /// Maximum concurrent users
extern u_long          mostusers;

          /// Number of registered nicknames
extern u_long          mostnicks;

          /// Number of registered channels
extern u_long          mostchans;

          /// Number of memos ??
extern u_long          mostmemos;

          /// Counter of number of ChanServ commands interpreted by the old hack
extern u_long	       counterOldCSFmt;

          /// Time services started (now or first time)
extern long            startup, firstup;

          /// Services nickname constants
extern char           *OperServ, *NickServ, *ChanServ, *MemoServ, *InfoServ, *GameServ;

          /// Used for printing core messages
extern char            coreBuffer[IRCBUF];

          /// Limits set in services.conf
extern u_int           AccessLimit, OpLimit, AkickLimit, ChanLimit, NickLimit;

          /// Current time (UTC), keep this in mind...
extern time_t          CTime;

          /// Times (UTC) of next syncs
extern time_t	       nextNsync, nextCsync, nextMsync;



#ifdef __string_cc__
int fatalSvsError(const char *fName, int lineNo, const char *cErr)
{
	extern FILE *corelog;

	logDump(corelog, "Fatal Error --\n\"%s\"",
		coreBuffer);
	logDump(corelog, "Assertion: %s:%d: \"%s\" was false.",
		fName, lineNo, cErr);
	sSend(":%s GOPER :Fatal error: assertion: %s:%d: \"%s\" was false.",
		myname, fName, lineNo, cErr);
	sSend("WALLOPS :Fatal assertion error, shutdown NOW");
	sSend(":%s GOPER :Buffer is -> %s", myname, coreBuffer);
	dlogDump(corelog);
	sshutdown(1);
	return 0;
}
#else
int fatalSvsError(const char *fName, int lineNo, const char *cErr);
#endif


int isPasswordAcceptable(const char* password, char* reason);

/**
 * Shortcut for case-insensitive comparison
 */
#define str_cmp         strcasecmp

#endif	/* __SERVICES_H */
