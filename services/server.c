/* $Id$ */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 1999 Portions Copyright
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
 * \file server.c
 * Handling of server messages
 * \wd \taz \greg
 */

#include "services.h"
#include "nickserv.h"
#include "operserv.h"
#include "memoserv.h"
#include "chanserv.h"
#include "infoserv.h"
#include "gameserv.h"
#include "mass.h"
#include "log.h"

void parseLine(char *);

/**
 * \brief Used at services startup to connect to its uplink
 * \param hostname Hostname of uplink
 * \param IRC port
 * Returns a file descriptor of a connected socket if successful.
 */
int
ConnectToServer(char *hostname, int portnum)
{
	fd_set wfd, xfd;
	struct sockaddr_in sa;
	struct hostent *hp;
	int s;

	if ((hp = gethostbyname(hostname)) == NULL) {
		errno = ECONNREFUSED;
		return (-1);
	}

	bzero(&sa, sizeof(sa));
	bcopy(hp->h_addr, (char *)&sa.sin_addr, hp->h_length);	/* set address */
	sa.sin_family = hp->h_addrtype;
	sa.sin_port = htons((u_short) portnum);

	if ((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)	/* get socket */
		return (-1);
	if (connect(s, (struct sockaddr *)&sa, sizeof sa) < 0) {	/* connect */
		printf("Error:  %s\n", strerror(errno));
		close(s);
		return (-1);
	}

	/*
	 * whoo hoo, we got connected.  For some reason, if we don't pause here a
	 * little while, we loose. XXX This should be fixed...
	 */
	FD_ZERO(&wfd);
	FD_ZERO(&xfd);
	FD_SET(s, &wfd);
	FD_SET(s, &xfd);
	if (select(s + 2, NULL, &wfd, &xfd, NULL) < 0 || !FD_ISSET(s, &wfd)
		|| FD_ISSET(s, &xfd)) {
		logDump(corelog,
				"Error occured while attempting to establish connection [%d]",
				errno);
		flushLogs(NULL);
		sshutdown(2);
	}

	return (s);
}

/**
 * \brief a printf-like function for sending data to the services uplink
 * socket
 * \param format Format string
 * \param ... Variable-argument list
 */
void
sSend(char *format, ...)
{
	char sBuffer[IRCBUF];
	va_list stuff;

	/*
	 * set things up to print into the buffer.  Up to sizeof(sBuffer) - 2 is
	 * used (well, -3, since vsnprintf saves room for the \0 as well) and
	 * the \r\n is appended using strcat.
	 */
	va_start(stuff, format);
	vsnprintf(sBuffer, sizeof(sBuffer) - 3, format, stuff);
	va_end(stuff);

	/*
	 * Append a \r\n to the buffer, and send it to the server
	 */
	strcat(sBuffer, "\r\n");	/* strcat is safe */
#if 0
	printf("Writing: %s\n", sBuffer);
#endif
	if (server != -1) {		
		if (net_write(server, sBuffer, strlen(sBuffer)) == -1) {
			if (errno == EPIPE || errno == EBADF || errno == EINVAL ||
	                    errno == EFAULT) {
	                    logDump(corelog, "Terminating on write error, errno=%d", errno);
	                    sshutdown(0);
	                }
		}
	} else {
		logDump(corelog, "[ServerFd=-1] sSend : %s", sBuffer);
	}

#ifdef DEBUG
	printf("-> %s\n", sBuffer);
#endif
}

/**
 *  Add a new user, be that ChanServ or a nick in holding.
 * in the 'mode' variable you can specify +iogsw or any other ircd
 * compatible modes.
 * \par Used at services startup
 */
void
addUser(char *nick, char *user, char *host, char *name, char *mode)
{
	if (!nick || !*nick) {
		return;
	}
	sSend("NICK %s 1 0 %s %s %s :%s", nick, user, host, myname, name);
	sSend(":%s MODE %s :%s", nick, nick, mode);
}


/* Breakline code starts here */

/// Maximum length of an IRC message
#define MAX_IRC_LINE_LEN 512

/** Necessary to keep previous unterminated data
  *
  * \bug oldData: ugly global variable that ought not to be needed..
  *      should be done on a socket basis or something(?)
  */
char oldData[MAX_IRC_LINE_LEN];

/**
 * Break up a string of data into seperate pieces and parse those.
 * (parseLine())
 * \bug "I'm not sure but this function could be done better(?) if someone
 *       wants to improve it..."
 */
void
breakLine(char *tmpbuffer)
{
	int usedbuf, terminated;
	char sendBuffer[MAX_IRC_LINE_LEN + 1];

	memset(sendBuffer, 0, MAX_IRC_LINE_LEN + 1);

	usedbuf = 0;

	/* Just in case the impossible happens, and our old data is >= 512 chars,
	 * which means we won't be able to add anything from the data stream, and
	 * therefore can't get the terminating \r\n
	 */
	if (strlen(oldData) >= MAX_IRC_LINE_LEN) {
		sSend(":%s GLOBOPS :Infinate loop encounted in breakLine().  "
			  "Mail this line to coders@sorcery.net", OperServ);
		sSend(":%s GLOBOPS :Services terminating", OperServ);
		sshutdown(1);
	}

	/* If we have some leftover unterminated data from the stream... */
	if (oldData[0] != '\0') {
		strncpyzt(sendBuffer, oldData, MAX_IRC_LINE_LEN);
		bzero(oldData, MAX_IRC_LINE_LEN);
		usedbuf = strlen(sendBuffer);
	}

	while (*tmpbuffer) {
		terminated = FALSE;

		/* While each of the following are true:
		 * @ We've not found a terminator
		 * @ We're not at the end of the buffer
		 * @ We're not at the end of the buffer we can copy to
		 */
		while (*tmpbuffer != '\n' && *tmpbuffer != '\r'
			   && *tmpbuffer != '\0' && usedbuf < MAX_IRC_LINE_LEN) {
			sendBuffer[usedbuf] = *tmpbuffer;
			usedbuf++;
			tmpbuffer++;
		}

		sendBuffer[usedbuf] = 0;
		usedbuf = 0;

		/* Clear trailing terminator */
		while (*tmpbuffer == '\r' || *tmpbuffer == '\n') {
			tmpbuffer++;
			terminated = TRUE;
		}

		if (terminated == FALSE) {	/* If we didn't find the terminators */
			/*
			 * Copy our current unfinished line to a tmp buffer, we'll look
			 * at it again in the next call to breakLine(), when we recieve
			 * more data.  Had to be done due to recv() taking unteminated
			 * data.
			 */
			strncpyzt(oldData, sendBuffer, MAX_IRC_LINE_LEN);
		} else {
			/* If we've got all our data, and it's terminated, we can parse it */
			parseLine(sendBuffer);
		}
	}
}

/**
 * Takes server data and "moshes it around in here to see what to do with it.
 * :>"
 *
 * "Also, I am assuming ircd will send commands in uppercase only, if this is
 *  not the case, then it should be fixed, strcasecmp is a more expensive
 *  function and all...."
 */

/*
 * WARNING WARNING WARNING
 *
 *  This function is very ugly in terms of args handling, and it is
 *  very easy to inadvertently break something if you make small tweaks
 *  to it.
 *
 *  args and numargs do NOT always map together in this function,
 *  for example numargs = 3, means that args really has 3+3=6 elements
 *  in some points: numargs should be numargs2.
 *
 *  Basically, the bottom two elements of args[] are dropped
 * in this function when
 * it goes to args2 to call the particular handlers -Mysid
 */
void
parseLine(char *line)
{
	int i = 0, a = 0, x = 0, prefixed = 0;
	char *args2[MAX_IRC_LINE_LEN + 5];
	char *args[MAX_IRC_LINE_LEN + 5];
	char realargs[151][151];
	u_int16_t numargs = 0;
	/* 
	 * Seems Ok to me ^^^ 
	 * sizes may be off(?)
	 */

	/* Yes, your sizes were off.. -Mysid */

	strncpyzt(coreBuffer, line, MAX_IRC_LINE_LEN);
#ifdef DEBUG
	printf("Read: %s\n", coreBuffer);
#endif

	CTime = time(NULL);

	while (*line && x < 150) {
		while (*line != ' ' && *line && a < 150) {
			realargs[x][a] = *line;
			a++;
			line++;
		}
		realargs[x][a] = 0;
		args[x] = realargs[x];
		x++;
		numargs++;
		while (*line == ' ')
			line++;
		a = 0;
	}

	/* ensure the next item is null so we can check it later */
	realargs[x][0] = 0;
	args[x] = realargs[x];

	if (args[0][0] == ':') {
		prefixed = 1;
				 /** \bug old lame bugfix, what would be better is to use a 'from'
                    value and args++'ing it, if there's no prefix then
                    have from set to the uplink -Mysid */
		args[0]++;
	} else
		prefixed = 0;

	if (!strcmp(args[0], "PING") && !prefixed) {
		sSend("PONG :%s", myname);
		return;
	}

	else if (!strcmp(args[0], "ERROR") && !strcmp(args[1], ":Closing"))
		sshutdown(0);

	else if (!strcmp(args[0], "NICK") && !prefixed) {
		if (strchr(args[4], '*') || strchr(args[4], '?')
			|| strchr(args[4], '!') || strchr(args[4], '@')) {
			char nick[NICKLEN];

			strncpyzt(nick, args[1], NICKLEN);
			sSend
				(":%s KILL %s :%s!%s (Your ident reply contains either a *, !, @, or ?. Please remove this before returning.)",
				 services[1].name, nick, services[1].host,
				 services[1].name);
			addGhost(nick);
			timer(15, delTimedGhost, strdup(nick));
			return;
		}

		addNewUser(args, numargs);	/* nickserv.c, add new user. */
		return;
	}


	if (!strcmp(args[1], "PRIVMSG")) {
		UserList *tmp = getNickData(args[0]);


		if (strchr(args[2], '#') || strchr(args[2], '$'))
			return;

		if (!strcasecmp(args[0], NickServ)
			|| !strcasecmp(args[0], GameServ)
			|| !strcasecmp(args[0], OperServ)
			|| !strcasecmp(args[0], ChanServ)
			|| !strcasecmp(args[0], MemoServ)
			|| !strcasecmp(args[0], InfoServ)) return;

		if (tmp && tmp->reg && tmp->reg->flags & NBANISH) {
			sSend(":%s NOTICE %s :This nickname is banished."
			      "  You cannot use services until you change"
			      " nicknames.",
			      NickServ, args[0]);
			return;
		}

		if (!tmp) {
			nDesynch(args[0], "PRIVMSG");
			return;
		}

		if (addFlood(tmp, 1))
			return;

		if (isIgnored(tmp->nick, tmp->user, tmp->host)) {
			if (tmp->floodlevel.GetLev() < 2)
				sSend
					(":%s NOTICE %s :You are on services ignore, you may not use any Service",
					 NickServ, tmp->nick);
			if (!isOper(tmp) || tmp->caccess < 2 || !tmp->reg
				|| !(tmp->reg->opflags & OROOT))
				return;
		}

		args[3]++;
		while (*args[3] == ' ')
			args[3]++;

		for (i = 3; i < numargs; i++)
			args2[i - 3] = args[i];
		numargs -= 3;

		/* Handle pings before even going to the services */
		if (!strcasecmp(args2[0], "\001PING")) {
			if (addFlood(tmp, 3))
				return;
			if (numargs < 3)
				sSend(":%s NOTICE %s :\001PING %s", args[2], args[0],
					  args2[1]);
			else
				sSend(":%s NOTICE %s :\001PING %s %s", args[2], args[0],
					  args2[1], args2[2]);
			return;
		}

		/* NOTE: numargs maps to args2 not args at this point */
		if (!strncasecmp(args[2], OperServ, strlen(OperServ)))
			sendToOperServ(tmp, args2, numargs);
		else if (!strncasecmp(args[2], NickServ, strlen(NickServ)))
			sendToNickServ(tmp, args2, numargs);
		else if (!strncasecmp(args[2], ChanServ, strlen(ChanServ)))
			sendToChanServ(tmp, args2, numargs);
		else if (!strncasecmp(args[2], MemoServ, strlen(MemoServ)))
			sendToMemoServ(tmp, args2, numargs);
		else if (!strncasecmp(args[2], InfoServ, strlen(InfoServ)))
			sendToInfoServ(tmp, args2, numargs);
		else if (!strncasecmp(args[2], GameServ, strlen(GameServ)))
			sendToGameServ(tmp, args2, numargs);
		else if (isGhost(args[2])) {
			sSend
				(":%s NOTICE %s :This is a NickServ registered nick enforcer, and not a real user.",
				 args[2], args[0]);
		}
		/* Note, the below should be correct. */
		else if ((numargs >= 1) && adCheck(tmp, args[2], args2, numargs))
			return;
		return;
	}

	else if (!strcmp(args[1], "QUIT")) {
		remUser(args[0], 0);
		return;
	} else if (!strcmp(args[1], "NICK")) {
		UserList *tmp = getNickData(args[0]);
		if (addFlood(tmp, 5))
			return;
		changeNick(args[0], args[2], args[3]);
		return;
	} else if (!strcmp(args[1], "MODE") && !strcmp(args[0], args[2])) {
		setMode(args[0], args[3]);
		return;
	} else if (!strcmp(args[1], "MODE")) {
		setChanMode(args, numargs);
		return;
	} else if (!strcmp(args[1], "TOPIC")) {
		setChanTopic(args, numargs);
		return;
	} else if (!strcmp(args[1], "AWAY")) {
		if (numargs < 3) {
			setFlags(args[0], NISAWAY, '-');
			checkMemos(getNickData(args[0]));
		} else
			setFlags(args[0], NISAWAY, '+');
		return;
	}

	else if (!strcmp(args[1], "JOIN")) {
		addUserToChan(getNickData(args[0]), args[2]);
		return;
	} else if (!strcmp(args[1], "PART")) {
		remUserFromChan(getNickData(args[0]), args[2]);
		return;
	} else if (!strcmp(args[1], "KICK")) {
		remUserFromChan(getNickData(args[3]), args[2]);
		return;
	}

	else if (!strcmp(args[1], "KILL")) {
		int i;
		for (i = 0; i < NUMSERVS; i++) {
			if (!strcasecmp(args[2], services[i].name)) {
				addUser(services[i].name, services[i].uname,
						services[i].host, services[i].rname,
						services[i].mode);
				sSend(":%s KILL %s :%s!%s (services kill protection)",
					  services[i].name, args[0], services[i].host,
					  services[i].name);
				sSend(":%s GLOBOPS :%s just killed me!", services[i].name,
					  args[0]);
				remUser(args[0], 0);
				return;
			}
		}

		if (isGhost(args[2])) {
			delGhost(args[2]);
			return;
		}

		else
			remUser(args[2], 1);
		return;
	} else if (!strcmp(args[1], "MOTD")) {
		UserList *tmp = getNickData(args[0]);
		if (addFlood(tmp, 1))
			return;
		motd(args[0]);
		return;
	}

	else if (!strcmp(args[1], "INFO")) {
		UserList *tmp = getNickData(args[0]);
		if (!tmp || addFlood(tmp, 3))
			return;
		sendInfoReply(tmp);
		return;
	}

	else if (!strcmp(args[1], "VERSION")) {
		UserList *tmp = getNickData(args[0]);
		if (addFlood(tmp, 1))
			return;
		sSend(":%s 351 %s %s %s :%s", myname, args[0], VERSION_STRING,
			  myname, VERSION_QUOTE);
		return;
	} else if ((!strcmp(args[1], "GNOTICE") || !strcmp(args[1], "GLOBOPS"))
			   && !strcmp(args[2], ":Link") && !strcmp(args[3], "with")
			   && !strncmp(args[4], myname, strlen(myname))) {
		sSend(":%s GNOTICE :Link with %s[services@%s] established.",
			  myname, args[0], hostname);
		strncpyzt(hostname, args[0], sizeof(hostname));

		expireNicks(NULL);
		expireChans(NULL);
		sync_cfg("1");
		checkTusers(NULL);
		flushLogs(NULL);
		nextNsync = (SYNCTIME + CTime);
		nextCsync = ((SYNCTIME * 2) + CTime);
		nextMsync = ((SYNCTIME * 3) + CTime);
		loadakills();
		return;
	}
#ifdef IRCD_HURTSET
	else if (!strcmp(args[1], "HURTSET") && (numargs >= 4)) {
		UserList *hurtwho;
		if ((hurtwho = getNickData(args[2]))) {
			if (args[3] && *args[3] == '-')
				hurtwho->oflags &= ~(NISAHURT);
			else if (args[3] && atoi(args[3]) == 4) {
				hurtwho->oflags |= (NISAHURT);
			}
			else if (args[3] && isdigit(*args[3])
					 && isAHurt(hurtwho->nick, hurtwho->user,
								hurtwho->host)) hurtwho->oflags |=
					(NISAHURT);
		}
	}
#endif
	else if (!strcmp(args[1], "SQUIT")) {
		time_t jupe;
		jupe = time(NULL);
		if (strchr(args[2], '.'))
			return;
		sSend(":%s WALLOPS :%s Un-jupitered by %s at %s", myname, args[2],
			  args[0], ctime(&(jupe)));
		return;
	}

	else if (!strcmp(args[1], "STATS") && numargs > 3) {
		const char* from = args[0];

		if (args[2] && !strcasecmp(args[2], "OPTS"))
		{
			sSend(":%s NOTICE %s :Network name: %s", InfoServ, from, NETWORK);


#ifdef AKILLMAILTO
		        sSend(":%s NOTICE %s :Akill log address: %s", InfoServ, from,
		                AKILLMAILTO);
#endif

#ifdef ENABLE_GRPOPS
		        sSend(":%s NOTICE %s :GRPops enabled.",  InfoServ, from);
#endif

#ifdef MD5_AUTH
		        sSend(":%s NOTICE %s :MD5 authentication available.",  InfoServ, from);
#endif

		}
		else if (args[2] && !strcasecmp(args[2], "V$"))
		{
			sSend(":%s NOTICE %s :Based on sn services1.4.", InfoServ, from);
		}
	}

	/* "No N-line" error */
	else if (!strcmp(args[0], "ERROR") && !strcmp(args[1], ":No")) {
		fprintf(stderr, "Error connecting to server: No N-line\n");
		sshutdown(2);
	}
}

/**
 * Respond to an /INFO message
 * \param nick Pointer to online user item
 */
void
sendInfoReply(UserList * nick)
{
	char *from = nick->nick;
	extern char *services_info[];
	int i = 0;

	sSend(":%s 373 %s :Server INFO", myname, from);
	for (i = 0; services_info[i]; i++)
		sSend(":%s 371 %s :%s", myname, from, services_info[i]);
	sSend(":%s 374 %s :End of /INFO list.", myname, from);
	return;
}
