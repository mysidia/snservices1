/* $Id$ */

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

/**
 * \file signal.c
 * Signal and other handlers
 */


#include "services.h"
#include "hash.h"
#include "nickserv.h"
#include "memoserv.h"
#include "infoserv.h"
#include "db.h"
#include "log.h"

/* --------------------------------------------------------------------- */

#ifdef USE_SQL
/**
 * \todo XXX this, need to do SQL support but in an abstract manner
 */
PGconn *dbConn = NULL;
#endif

/**
 * \brief My server name, and my server password
 */
char myname[255],
     mypass[33];

/*
 * Their hostname, and port
 */
char hostname[255];
int port;

/*
 * the servers socket...
 */
int server;

/*
 * array of services, and their data...
 * see above for Service struct.
 */
Service services[NUMSERVS];
database db;
SLogfile *operlog, *nicklog, *chanlog;
FILE *corelog;
u_long totalusers;
u_long mostusers = 0, mostnicks = 0, mostchans = 0, mostmemos = 0;
extern unsigned long top_akill_stamp;
long startup, firstup;
char *OperServ, *NickServ, *ChanServ, *MemoServ, *InfoServ, *GameServ;
char *MassServ;
char coreBuffer[IRCBUF];
u_int AccessLimit = 3, OpLimit = 25, AkickLimit = 30, ChanLimit = 10, NickLimit = 5;
time_t CTime; /* current time, keep this in mind... */

time_t nextNsync, nextCsync;
time_t nextMsync;

/*!
 \fn void flushLogs(char *)
 \brief Flush services logs

  Performs a 'flush' on the log files resulting in any new services log messages
  being written to disk
*/
void flushLogs(char *)
{
	nicklog->flush();
	chanlog->flush();
	operlog->flush();
	fflush(corelog);

	timer(60, flushLogs, NULL);
}

/*!
 \fn void checkTusers(char *)
 \brief Check total number of users

  Checks the total number of users to see if a change to the network
  PLUS-L-CHANNEL mode is necessary to take into account a new maximum
  number of concurrent connections.
*/

void checkTusers(char *)
{
#ifdef PLUSLCHAN
	static u_int last_mostusers = 0;

	if (last_mostusers < mostusers) {
		sSend(":%s MODE %s +l %lu", OperServ, PLUSLCHAN, mostusers + 5);
		last_mostusers = mostusers;
	}

	timer(60, checkTusers, NULL);
#endif
}

/*!
 * \brief Write out services.totals
 *
 * Save services' statistical/total info
 * Also, save top serial numbers and other important info
 */
void writeServicesTotals()
{
        FILE *fp = fopen("services.totals", "w");

        if (fp) {
                fprintf(fp, "%lu\n%lu\n%lu\n%lu\n%lu\n", mostusers, mostnicks,
                                mostchans, mostmemos, top_akill_stamp);
                fflush(fp);
                fclose(fp);
        }
}

/////////////
/*!
 * \fn void sshutdown(int type)
 * \brief Shuts down services
 * \param type specifies type of shutdown (fatal error, normal exit, etc)
 *
 * \bug XXXMLG This needs to be done better...  I suggest #defines for the
 * various things we want this function to do (save data, send message,
 * etc) and then use something like:
 *      sshutdown(SHUTDOWN_SAVE | SHUTDOWN_MSG, "This is the message");
 */
void sshutdown(int type)
{
	if (type != (-1) && type != 2) {
		writeServicesTotals();
		saveakills();
		saveNickData();
		syncChanData(1);
		saveMemoData();
		saveInfoData();
	}

	timed_akill_queue(NULL);
	unlink("services.pid");

	if (1 /*type == 0 || type == -1 || type > 2 */ ) {
#ifdef USE_SQL
		if (dbConn != NULL)
			PQfinish(dbConn);
		dbConn = NULL;
#endif
	}

	if (type == 1) {	
		sSend("SQUIT %s :Services %s fall down go BOOM!", hostname,
			VERSION_NUM);
		close(server);
		abort(); 				/* dump a core file */
        }
	else if (type == 2) {				/* No connection to server established */
		fprintf(stderr,
				"Error connecting to server, check above message\n");
		exit(-1);
	}
	else if (type == 0) {
		sSend("SQUIT %s :Services %s shutdown", hostname, VERSION_NUM);
		close(server);
		exit(66);
	}
	else {
		sSend("SQUIT %s :Services %s shutdown", hostname, VERSION_NUM);
		close(server);
		exit(1);
	}
}

/*!
 * \fn void handler(int signal)
 * \brief Signal handler
 * \param signal A signal number (such as SIGTERM)
 * This is the function used by services to handle signals.
 */
void handler(int sig)
{
	switch (sig) {
	case SIGTERM:
		sSend("WALLOPS "
			":Received some signal thats telling me to shutdown");
		timed_akill_queue(NULL);
		sshutdown(0);
		break;
	case SIGSEGV:
		timed_akill_queue(NULL);
		logDump(corelog, "Core dumped ---\n\"%s\"", coreBuffer);
		sSend("WALLOPS :Segmentation Violation, shutdown NOW");
		sSend("GOPER :Buffer is -> %s", coreBuffer);
		dlogDump(corelog);
		sshutdown(1);
		break;

	case SIGPIPE:
		timed_akill_queue(NULL);
		signal(SIGPIPE, SIG_IGN);  /*! \bug XXXMLG probably not QUITE right */
/*		sshutdown(0);*/
		break;

	default:
		sSend("GNOTICE :Recieved unidentified signal %i", sig);
		return;
	}
}

/* --------------------------------------------------------------------- */

/* $Id$ */
