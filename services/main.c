/**
 * \file main.c
 * \brief Services' startup and main loop
 *
 * Main entry point for services bootup, handles dispatching
 * startup procedures
 *
 * \wd \taz \greg \mysid
 * \date 1996-2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 2000-2001 James Hess
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

#include "services.h"
#include "hash.h"
#include "nickserv.h"
#include "memoserv.h"
#include "infoserv.h"
#include "sipc.h"
#include "db.h"
#include "mass.h"
#include "log.h"

#include <getopt.h>

#ifdef USE_SQL
extern PGconn *dbConn;
#endif

extern int ipcPort;
extern int svcOptFork;

/*
 * my server name, and my server password 
 */
extern char myname[255];
extern char mypass[33];

/*
 * Their hostname, and port 
 */
extern char hostname[255];
extern int port;

/*
 * the servers socket... 
 */
extern int server;

/*
 * array of services, and their data... 
 * see above for Service struct. 
 */
extern Service services[NUMSERVS];
extern database db;

/// Services Log handlers
extern SLogfile *operlog, *nicklog, *chanlog;

/// Logfile to record system errors/debug information
extern FILE *corelog;

/// Total users online at the moment
extern u_long totalusers;

/// Most users online at a time, num nicks, chans, memos
extern u_long mostusers, mostnicks, mostchans, mostmemos;

/// Highest akill stamp in use
extern unsigned long top_akill_stamp;

/// Time of startup, time of -first- startup of services
extern long startup, firstup;

/// Services nickname constants
extern char *OperServ, *NickServ, *ChanServ, *MemoServ, *InfoServ, *GameServ;

/// Coredump buffer
extern char coreBuffer[IRCBUF];

/// Specified limits on user data
extern u_int AccessLimit, OpLimit, AkickLimit, ChanLimit, NickLimit;

/// Current time
extern time_t CTime;					/* current time, keep this in mind... */

/// Time of Next database saves
extern time_t nextNsync, nextCsync, nextMsync;

static int lockfile(char *);
time_t updateCloneAlerts(time_t);

/// Master services IPC object
IpcType servicesIpc;

int runAsRootOverride = 0;

/// Help for services command line
void ServicesProgramHelp()
{
	printf("Usage: services [OPTION] ...\n");
	printf("SorceryNet Services server\n");
	printf("Information:\n");
	printf("  -h, --help       Show this information\n");
	printf("  -v, --version    Print version information\n");
	printf("  -f, --fork       Run as a daemon, fork into background\n");
	printf("  -F, --nofork     Stay in foreground\n");
	printf("  -i, --ipc=[PORT] Enable services IPC using PORT\n");
	printf("  -I, --noipc      Disable services IPC\n");
	printf("\nReport bugs to services-bugs@sorcery.net\n");
	return;
}

/// Startup services, begin the main loop.
int main(int argc, char *argv[])
{
	char buffer[4097];
	fd_set readme, writeme, xceptme;
	FILE *fp, *pidf;
	struct timeval tv;
	int cc;
	time_t next_alert_update = 0;
#ifdef USE_SQL
	int PQsockfd;
#endif
#ifndef NORLIMIT
	struct rlimit corelim;
#endif

#ifndef DEBUG
#ifdef FORK_EM
	int pid;
#endif
#endif

	static struct option servOpts[] =
	{
		/*{ "help", 1, 0, 0  },*/
		{ "help", 0, 0, 0  },
		{ "version", 0, 0, 1  },
		{ "fork", 0, 0, 2  },
		{ "nofork", 0, 0, 3  },
		{ "ipc", 2, 0, 4  },
		{ "noipc", 0, 0, 5  },
		{ 0, 0, 0, 0 },
	};

	int lastOpt;
	char ch;

	ipcPort = 0;

	/* Handle options */
	while ( optind < argc )
	{
		static const char *libUf = ( "\n"
			"Copyr. 1996-2001 Chip Norkus, Max Byrd, Greg Poma, Michael Graff\n"
			"                 James Hess, Dafydd James\n"
			"All Rights Reserved.\n\n"
			"This product includes software developed by Chip Norkus, Max Byrd,\n"
			"Greg Poma, Michael Graff, James Hess, Dafydd James, the University\n"
			"of California, Berkeley and its contributors.\n\n"
			"THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND\n"
			"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
			"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
			"ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE\n"
			"FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
			"DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n"
			"OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n"
			"HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n"
			"LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n"
			"OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n"
			"SUCH DAMAGE.\n"
		);

		ch = getopt_long(argc, argv, "RhvfFi:I", servOpts, &lastOpt);

		if (ch == '?' || ch == ':')
			exit(2);

		if (ch == -1) {
			printf("%s: invalid option -- %s\n", PACKAGE, argv[optind]);
			exit(2);
		}

		switch(ch)
		{
			case 'h':
			case 0: /* --help */
				ServicesProgramHelp();
				exit(0);
			break;

			case 'v':
			case 1: /* --version */
				printf("%s\n", (PACKAGE "-" VERSION));
				fprintf(stdout, "%s\n", libUf);
				exit(0);
			break;

			case 'f':
			case 2: /* --fork */
				svcOptFork = 1;
			break;

			case 'F':
			case 3: /* --nofork */
				svcOptFork = 0;
			break;

			case 'R':
				runAsRootOverride = 1;
			break;

			case 'i':
			case 4: /* --ipc */
				if (optarg == NULL)
					ipcPort = 3030;
				else if (isdigit(*optarg))
					ipcPort = atoi(optarg);

				if (ipcPort <= 1024 || ipcPort >= SHRT_MAX) {
					printf("services: Invalid port number.\n"
					       "Must be between 1025 and %d.\n",
						SHRT_MAX);
					exit(2);
				}
			break;

			case 'I':
			case 5: /* --noipc */
				ipcPort = -1;
			break;

			default:
				printf("HUH?\n");
				exit(100);
		}
	}

	/* End option handling */

	if (geteuid() == 0 && runAsRootOverride == 0) {
		fprintf(stderr, "ERROR: %s should not be run as root.\n", argv[0]);
		exit(1);
	}


	CTime = time(NULL);

	/*
	 * initialize the debugging log file
	 */
	dlogInit();

	dlogEntry("Starting...");

	/*
	 * If there's a .pid file still, then services is most likely running...
	 * scream really loudly
	 */
	pidf = fopen("services.pid", "r");
	if (pidf != NULL) {
		fprintf(stderr,
				"Error, services.pid already exists.  If services isn't\n"
				"actually running, delete the .pid file and run me again.\n"
				"If it is running, don't load me again.\n");
		fclose(pidf);
		return 1;
	}

#ifdef DEBUG
	printf("Using %s for sending mail from stdin\n", SENDMAIL);
#endif
#ifndef NORLIMIT
	corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &corelim))
		fprintf(stderr, "Cannot unlimit core! -> %i", errno);
#endif

	umask(077);
	setenv("TZ", "GMT", 1);
	tzset();
	dlogEntry("Locking files");

	if (lockfile("chanserv/chanserv.db")) {
		printf("Exiting...\n");
		sshutdown(-1);
	}

	if (lockfile("nickserv/nickserv.db")) {
		printf("Exiting...\n");
		sshutdown(-1);
	}

	if (lockfile("memoserv/memoserv.db")) {
		printf("Exiting...\n");
		sshutdown(-1);
	}

	/*
	 * logging files 
	 */
	dlogEntry("Opening logs");
	operlog = new SLogfile("operserv/operserv.log", "operserv/operserv-w");
	nicklog = new SLogfile("nickserv/nickserv.log", "nickserv/nickserv-w");
	chanlog = new SLogfile("chanserv/chanserv.log", "chanserv/chanserv-w");
	corelog = fopen("core.log", "a");

	if (!operlog || !nicklog || !chanlog || !corelog) {
		if (corelog)
			dlogEntry("Unable to open logfile(s)");
		fprintf(stderr, "Unable to open: %s%s%s%s.\n",
				operlog ? "" : " operlog",
				nicklog ? "" : " nicklog",
				chanlog ? "" : " chanlog", corelog ? "" : " corelog");
		sshutdown(0);
	}

	dlogEntry("Reading nickname data");
	readNickData();

	dlogEntry("Reading channel data");
	readChanData();

	dlogEntry("Reading article data");
	readInfoData();

	dlogEntry("Reading trigger data");
	readTriggerData();

	dlogEntry("Reading config");
	readConf();

#ifndef DEBUG
#ifdef FORK_EM
	if (svcOptFork)
	{
		dlogEntry("Forked");

		pid = fork();

		if (pid) {
			pidf = fopen("services.pid", "w");
			if (pidf != NULL) {
				fprintf(pidf, "%d", pid);
				fclose(pidf);
				return 0;
			}
			else
			{
				perror("fopen(services.pid)");
				exit(5);
			}
		}
	}
	else
	{
		pid = getpid();
		pidf = fopen("services.pid", "w");

		if (pidf != NULL) {
			fprintf(pidf, "%d", pid);
			fclose(pidf);
		}
		else {
			perror("fopen(services.pid)");
			exit(5);
		}
		return 0;
	}
#endif
#endif

	if (chdir(CFGPATH) == -1)
		fprintf(stderr, "Cannot chdir(%s)\n", CFGPATH);

#ifdef DEBUG
	printf
		("\n\n\nMyName:%s   MyPass:%s\nConnectHost:%s   ConnectPort:%i\n",
		 myname, mypass, hostname, port);
#endif

	dlogEntry("Connecting to server %s, port %d", hostname, port);

#ifdef USE_SQL
	if (dbConn != NULL)
		PQfinish(dbConn);
	dbConn = PQconnectdb(""
#ifdef SQL_HOST
						 " host=" SQL_HOST
#endif
#ifdef SQL_HOSTADDR
						 " hostaddr=" SQL_HOSTADDR
#endif
#ifdef SQL_PORT
						 " port=" SQL_PORT
#endif
						 " dbname=" SQL_DB
						 " user=" SQL_USER " password=" SQL_PASS "");
	if (!dbConn) {
		fprintf(stderr,
				"Error allocating memory for database connection.\n");
		sshutdown(-1);
	}
	if (PQstatus(dbConn) != CONNECTION_OK) {
		fprintf(stderr, "Error connecting to the database.\n");
		sshutdown(-1);
	}
	PQsetnonblocking(dbConn, TRUE);
#endif

	if (ipcPort == -1 || servicesIpc.start(ipcPort) < 0) {
		if (ipcPort > 0)
			printf("Could not startup ipc!\n");
		else
			printf("Ipc not enabled.\n");
	}

	srand48(time(NULL));
	srandom(time(NULL));

	if ((server = ConnectToServer(hostname, port)) < 0) {
		printf
			("Could not connect to server!, exiting now.\nSocket # is: %i\n",
			 server);
		exit(0);
	}

	dlogEntry("Logging in");
	sSend("PASS %s", mypass);
	sSend("SERVER %s 1 :Services Impenetrable Fortress ;)", myname);
	addUser(services[0].name, services[0].uname, services[0].host,
			services[0].rname, services[0].mode);
	addUser(services[1].name, services[1].uname, services[1].host,
			services[1].rname, services[1].mode);
	addUser(services[2].name, services[2].uname, services[2].host,
			services[2].rname, services[2].mode);
	addUser(services[3].name, services[3].uname, services[3].host,
			services[3].rname, services[3].mode);
	addUser(services[4].name, services[4].uname, services[4].host,
			services[4].rname, services[4].mode);
	addUser(services[5].name, services[5].uname, services[5].host,
			services[5].rname, services[5].mode);
	addUser(services[6].name, services[6].uname, services[6].host,
			services[6].rname, services[5].mode);

	dlogEntry("Reading services total files");
	fp = fopen("services.totals", "r");

	if (fp != NULL) {
		mostusers = mostnicks = mostchans = mostmemos = 0;
		firstup = 0;

		if (sfgets(buffer, 256, fp))
			mostusers = atoi(buffer);

		if (sfgets(buffer, 256, fp))
			mostnicks = atoi(buffer);

		if (sfgets(buffer, 256, fp))
			mostchans = atoi(buffer);

		if (sfgets(buffer, 256, fp))
			mostmemos = atoi(buffer);

		if (sfgets(buffer, 10, fp))
			firstup = atol(buffer);

		if (sfgets(buffer, 10, fp))
			top_akill_stamp = atol(buffer);

		fclose(fp);
	}

	totalusers = 0;

	startup = time(NULL);
	dlogEntry("Operational at", startup);

#if 1							/* set to 0 for reasonable GDB debugging of segv */
	signal(SIGPIPE, handler);
	signal(SIGSEGV, handler);
	signal(SIGTERM, handler);
#endif

	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	/* signal(SIGINT, SIG_IGN); */
	signal(SIGINT, handler);

	OperServ = services[0].name;
	NickServ = services[1].name;
	ChanServ = services[2].name;
	MemoServ = services[3].name;
	InfoServ = services[4].name;
	GameServ = services[5].name;
	MassServ = services[6].name;
	/*
	 * the only exit out of this loop is death.
	 */
	timer(90, timed_akill_queue, NULL);
	timer(20, timed_advert_maint, NULL);

	while (1) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&readme);
		FD_ZERO(&writeme);
		FD_ZERO(&xceptme);
		FD_SET(server, &readme);
#ifdef USE_SQL
		if (dbConn) {
			PQsockfd = PQsocket(dbConn);
			PQflush(dbConn);
			FD_SET(PQsockfd, &readme);
			FD_SET(PQsockfd, &xceptme);
		}
#endif
		memset(buffer, 0, sizeof(buffer));


		servicesIpc.fdFillSet(readme);
		servicesIpc.fdFillSet(writeme);
		servicesIpc.fdFillSet(xceptme);

		/*
		 * wait for something interesting.
		 */

#ifdef USE_SQL
		cc =
			select(MAX(1+servicesIpc.getTopfd(), MAX(server, PQsockfd) + 1), &readme, &writeme, &xceptme,
				   &tv);
#else
		cc = select(MAX(1+servicesIpc.getTopfd(), server + 1), &readme, &writeme, &xceptme, &tv);
#endif

		/*
		 * select error is fatal
		 */
		if (cc < 0) {
			dlogEntry("Error on select(): %s", strerror(errno));
			sshutdown(0);
		}

		/*
		 * if the timer expired, run the timer code
		 */
		if (cc == 0) {
			timeralarm();
			continue;
		}

		if (next_alert_update < CTime)
			next_alert_update = updateCloneAlerts(CTime);

		servicesIpc.pollAndHandle(readme, writeme, xceptme);

#ifdef USE_SQL
		if (dbConn) {
			if (FD_ISSET(PQsockfd, &xceptme)
				|| FD_ISSET(PQsockfd, &readme)) {
				switch (PQstatus(dbConn)) {
				default:
					break;
				case CONNECTION_BAD:
					logDump(corelog,
							"DB connection went bad, attempting to reset...");
					sSend
						("GLOBOPS :DB connection went bad, attempting to reset...");
					PQreset(dbConn);
					if (PQstatus(dbConn) != CONNECTION_OK) {
						logDump(corelog,
								"Unable to re-establish db connection, dying.");
						sSend
							("GLOBOPS :Unable to re-establish db connection, dying.");
						sshutdown(-1);
					}
					break;
				}
			}

			if (FD_ISSET(PQsockfd, &readme)) {
				PGresult *res;

				PQconsumeInput(dbConn);
				if (!PQisBusy(dbConn)) {
				}
				while ((res = PQgetResult(dbConn))) {
					PQclear(res);
				}
			}
		}
#endif

		/*
		 * receive one less than the size of the buffer, just in case...
		 */
		if (FD_ISSET(server, &readme)) {
			cc = net_read(server, buffer, sizeof(buffer) - 1);

			/*
			 * If we have a read error, shut down.  This isn't perfect, but it is
			 * good enough for now I guess.
			 */
			if (cc < 0 || cc == 0) {
				dlogEntry("Closing server connection due to error: %s",
						  (cc ==
						   0 ? "Connection closed" : strerror(errno)));
				close(server);
				server = -1;

				sshutdown(0);
			}
		}

		breakLine(buffer);
	}

	/* NOTREACHED */
	return 0;
}

/**
 * Get an exclusive lock on the file name given.  Note that the file is NOT
 * closed, since closing it will release the lock we just obtained, which
 * is not what we want.
 *
 * \param fname Name of file to lock
 * \return -1 on failure, 0 on success
 */
static int lockfile(char *fname)
{
	int fd;
	int i;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		printf("Cannot open %s\n", fname);
		return -1;
	}

	i = flock(fd, LOCK_EX | LOCK_NB);
	if (i != 0) {
		printf("Cannot lock file %s\n", fname);
		return -1;
	}

	return 0;
}

/**********************************************************************/

/* $Id$ */
