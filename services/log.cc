/**
 * \file log.cc
 * \brief Logging module
 *
 * This module implements the basic services command/event
 * logging system.
 *
 * \skan \mysid
 * \date 1997, 2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1997 Michael Graff.
 * Copyright (c) 2001 James Hess <mysidia@sorcery.net>
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
#include "log.h"

#ifndef NUM_LOG_ENTRIES
/// Number of log entries to keep in the debug log
#define NUM_LOG_ENTRIES		100	/* number of log entries */
#endif

#ifndef LOG_ENTRY_SIZE
/// Size of a debug log entry
#define LOG_ENTRY_SIZE		200	/* size of each entry */
#endif

/// \internal Used to suppress compiler warnings about using a null default.
const char *nullFmtHack = (const char *)0;

/// The log entries
static char *log_entry[NUM_LOG_ENTRIES];

/// Next entry goes where?
static int next_entry;

/// Number entries present?
static int num_entries;
#ifdef C_DEBUG2

/// ChanServ/Channel-related debug file
FILE *fp1;
#endif

/**
 * \brief Handle the logging of an event/command
 * \param Pointer to nick item who caused event/command or a null pointer
 * \param cmd command id number of type #interp::services_cmd_id
 * \param flags Additional logging flags (ex: #LOGF_NORMAL)
 * \return 0 on success, -1 on failure
 */
int
SLogfile::log(UserList *sender,
			  interp::services_cmd_id cmd,
			  const char *target,
			  u_int32_t flags, const char *extra, ...) {
	va_list ap;

	va_start(ap, extra);
	logx(sender, 0, cmd, target, flags, extra, ap);
	va_end(ap);

	va_start(ap, extra);
	logx(sender, 1, cmd, target, flags, extra, ap);
	va_end(ap);
}

/**
 * @brief Same as log() but only writes to 'working' (temporary) log
 */
int
SLogfile::logw(UserList *sender,
			  interp::services_cmd_id cmd,
			  const char *target,
			  u_int32_t flags, const char *extra, ...) {
	va_list ap;

	va_start(ap, extra);
	logx(sender, 1, cmd, target, flags, extra, ap);
	va_end(ap);
}

int
SLogfile::logx(UserList *sender, int zb,
			  interp::services_cmd_id cmd,
			  const char *target,
			  u_int32_t flags, const char *extra, va_list ap)
{
	static char buffer[IRCBUF * 2];
	char senderblock[IRCBUF];

	if (extra && *extra && ap != NULL) {
		vsnprintf(buffer, IRCBUF, extra, ap);
	} else
		*buffer = '\0';

	if (sender)
		sprintf(senderblock, "%s!%s@%s", sender->nick, sender->user,
				sender->host);
	else
		strcpy(senderblock, "-");
	fprintf(zb ? fpw : fp, "%s %lu %s %lu %s",
			interp::cmd_name(cmd), time(NULL),
			*senderblock ? senderblock : "<???>",
			(unsigned long int)flags, target ? target : "");
	if (*buffer)
		fprintf(zb ? fpw : fp, " %s\n", buffer);
	else
		putc('\n', zb ? fpw : fp);
	return 0;
}

// Quick reference, remove this comment later
// NS_SETFLAG //        logDump(operlog, "%s!%s@%s set flag(s): %s on %s", from, tmp->user, tmp->host, args[2], args[1]);
// NS_SETOP   //
// NS_GETREALPASS // logDump(operlog, "%s!%s@%s tries to getreal pass %s.", from, tmp->user, tmp->host, args[1]);
// NS_GETREALPASS // logDump(operlog, "%s!%s@%s GETREALPASS %s%s", from, tmp->user, tmp->host, args[1], isRoot(tmp) ? " +" : "");
// CS_DROP // logDump(chanlog, "%s!%s@%s failed DROP on %s", from, nick->user, nick->host, args[1]);
// CS_DELETE //             logDump(chanlog, "%s!%s@%s (oper cmd) FAILED DROP %s", from, tmp->user, tmp->host, args[1]);
// NS_IDENTIFY // "%s!%s@%s: Bypassed AHURT ban with registered nick %s\n", from, tmp->user, tmp->host, tonick->nick);



/**
 * \brief Initialize debug logging at bootup
 */
void
dlogInit(void)
{
	int i;

	for (i = 0; i < NUM_LOG_ENTRIES; i++) {
		log_entry[i] = (char *)oalloc(LOG_ENTRY_SIZE);
		if (log_entry[i] == NULL) {
			fprintf(stderr,
					"Could not allocate memory initializing the log\n");
			exit(1);
		}
	}

	next_entry = 0;
	num_entries = 0;
}

/**
 * \brief Store a log entry in the debug-log stack
 */
void
dlogEntry(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	(void)vsnprintf(log_entry[next_entry], LOG_ENTRY_SIZE, format, ap);
	va_end(ap);

	next_entry++;
	if (next_entry >= NUM_LOG_ENTRIES)
		next_entry = 0;

	if (num_entries < NUM_LOG_ENTRIES)
		num_entries++;
}

/**
 * \brief Services crash or request, dump debug log entries to file
 */
void
dlogDump(FILE *fp)
{
	int i;

	/*
	 * Null fp is bad
	 */
	if (fp == NULL)
		return;

	/*
	 * Nothing to dump
	 */
	if (num_entries == 0)
		return;

	/*
	 * Start the log dump
	 */
	fprintf(fp, "Dump of log [%d entries]\n", num_entries);

	/*
	 * print out the individual entries.
	 */
	if (num_entries < NUM_LOG_ENTRIES) {
		for (i = 0; i < num_entries; i++)
			fprintf(fp, "%d: %s\n", i, log_entry[i]);
	} else {
		for (i = 0; i < num_entries; i++) {
			fprintf(fp, "%d: %s\n", i, log_entry[next_entry++]);

			if (next_entry >= NUM_LOG_ENTRIES)
				next_entry = 0;
		}
	}
}

/**
 * \brief Dump a single log item directly to file
 */
void
logDump(FILE *fp, char *format, ...)
{
	char buffer[512];
	char stuff[80];
	time_t doot = time(NULL);
	va_list crud;

	va_start(crud, format);
	vsprintf(buffer, format, crud);
	va_end(crud);

	strftime(stuff, 80, "%H:%M[%d/%m/%Y]", localtime(&doot));
	if (fp != NULL) {
		fprintf(fp, "(%s) %s\n", stuff, buffer);
	} else {
		fprintf(stdout, "(%s) %s\n", stuff, buffer);
	}

	return;
}

/**
 * \brief Get a user's full host mask in a NUL-terminated string form
 * \return Pointer to _static_ buffer to a user's full hostname
 */
const char *
fullhost1(const UserList *nick)
{
	static char buf[NICKLEN + USERLEN + HOSTLEN + 5];

	if (!nick)
		return NULL;
	sprintf(buf, "%s!%s@%s", nick->nick, nick->user, nick->host);
	return buf;
}
