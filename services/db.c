/**
 * \file db.c
 * \brief Services database-related procedures 
 * 
 * Procedures for saving/loading the various services database
 * files
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
 * Copyright (c) 1999-2000 James Hess
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
#include "nickserv.h"
#include "chanserv.h"
#include "db.h"
#include "hash/md5pw.h"
#include "memoserv.h"
#include "macro.h"
#include "hash.h"
#include "queue.h"
#include "log.h"
#include "infoserv.h"


#define NS_DIR "nickserv/"
#define CS_DIR "chanserv/"
#define OS_DIR "operserv/"
#define MS_DIR "memoserv/"
#define IS_DIR "infoserv/"

#define NS_DB  NS_DIR"nickserv.db"
#define CS_DB  CS_DIR"chanserv.db"
#define MS_DB  MS_DIR"memoserv.db"
#define TRG_DB OS_DIR"trigger.db"
#define AKI_DB OS_DIR"akill.db"
#define IS_DB  IS_DIR"infoserv.db"

extern RegId top_regnick_idnum;

#ifdef REQ_EMAIL
# error	REQ_EMAIL: does not work
#endif

static char cryptstr[PASSLEN + 2];	/*!< \brief Working crypt variable. 
									 *
									 *   We'll set it up here as it
									 *   is used in nearly all the functions
									 *   \bug This shouldn't necessarily be so global.
									 */

static char dbLine[2048];		/*!< I don't know what you feel, but I think that having 2048 bytes
								 * allocated in static arrays all over is useless. */

/**
 * @brief Parse state information for reading database files
 */
parse_t state;					/* yes, a global. */

void AppendBuffer(char **p, const char *);
char *str_dup(const char*);

/**
 * @brief Handle an unexpected EOF in database file error
 * @pre   file_name points to a valid Zero-terminated character array
 * @post  None
 */
void unexpected_eof( const char* file_name )
{
	fprintf(stderr, "Unexpected EOF: %s\n", file_name);
	logDump(corelog, "Unexpected EOF: %s", file_name);
	sshutdown(-1);
}
		

/**
 * \pre  fp Points to an open infile in which a multi-line dbString has been
 *          reached
 */
static char *dbReadString(FILE *fp)
{
	char tempLine[125], *p = NULL, *y;
	int fin = 0, firstLn = 0;

	while(!fin && sfgets(tempLine, 124, db.is)) {
		y = strrchr(tempLine, '~');
		assert(y != NULL);
		*y = '\0';

		if (y[1] == '$')
			fin = 1;

		strcat(tempLine, "\n");
		if (*tempLine != '\n')
			AppendBuffer(&p, tempLine);
		firstLn = 0;
		*y = '~';
	}
	if (p && *p)
		p[strlen(p) - 1] = '\0';

	return p;
}


/**
 * \pre  fp Points to an open outfile in which a multi-line dbString is
 *       to be written.  Str points to a valid NUL-terminated character
 *       array.
 */
static void dbWriteString(FILE *fp, const char *istr)
{
	char *p, *str;
	parse_t lineSplit;

	str = str_dup(istr);
	assert(parse_init(&lineSplit, str) == 0);
	lineSplit.delim = '\n';

	while((p = parse_getarg(&lineSplit))) {
		fprintf(fp, "%s~\n", p);
	}
	fprintf(fp, "~$\n");
	parse_cleanup(&lineSplit);
	FREE(str);
}

/** 
 * \brief Saves the NickServ database to disk
 */
void saveNickData(void)
{
	RegNickList *nl;
	nAccessList *accitem;
	int x = 0, bucket;

	db.ns = fopen(NS_DB, "w");

	if (db.ns == NULL) {
		sSend(":%s GOPER :ERROR: Unable to save NickServ database fopen failed: %s", myname, strerror(errno));
		logDump(corelog, "Unable to save NickServ database: fopen: %s", strerror(errno));
		return;
	}

	fprintf(db.ns, "version 3\n");

	for (bucket = 0; bucket < NICKHASHSIZE; bucket++) {
		nl = LIST_FIRST(&RegNickHash[bucket]);
		while (nl != NULL) {
			/* New nick insertion point */
			fprintf(db.ns, "nick ");
			if (nl->user[0] == '\0')
				fprintf(db.ns, "%s . %s ", nl->nick, nl->host);
			else
				fprintf(db.ns, "%s %s %s ", nl->nick, nl->user, nl->host);

			if (!(nl->flags & NENCRYPT))
			{
				strcpy(cryptstr, (char *)nl->password);
				xorit(cryptstr);
				fprintf(db.ns, "@%s ", cryptstr);
			}
			else {
				unsigned char *p;

				// XXX Put note that PASSLEN must be at least 15
				p = toBase64(nl->password, 16);
				fprintf(db.ns, "$%s ", p);
				FREE(p);
			}

			fprintf(db.ns, "%lu ", (unsigned long)nl->timestamp);
			fprintf(db.ns, "%lu ", (unsigned long)nl->timereg);
			fprintf(db.ns, "%i %X*%X\n", nl->flags, nl->regnum.a, nl->regnum.b);

			/* Infoserv data */
			fprintf(db.ns, "is %ld\n", nl->is_readtime);

#ifdef REQ_EMAIL
			if (!(nl->flags & NACTIVE) || (nl->flags & NDEACC))
				fprintf(db.ns, "akey %lu\n", nl->email_key);
#endif
			if (nl->chpw_key)
				fprintf(db.ns, "chkey %X\n", nl->chpw_key);
			if (nl->url != NULL)
				fprintf(db.ns, "url %s %s\n", nl->nick, nl->url);
#ifdef TRACK_GECOS
			if (nl->gecos != NULL)
				fprintf(db.ns, "gecos :%s\n", nl->gecos);
#endif

			if (nl->email[0] && strcmp(nl->email, "(none)"))
				fprintf(db.ns, "email %s %s\n", nl->nick, nl->email);

			if ((nl->opflags & ~(OROOT | OSERVOP)))
				fprintf(db.ns, "oper %s %lu 0 0\n", nl->nick, nl->opflags);

			if ((nl->flags & NMARK) && nl->markby)
				fprintf(db.ns, "markby %s %s\n", nl->nick, nl->markby);

			if (nl->idtime != DEF_NDELAY)
				fprintf(db.ns, "idtime %s %i\n", nl->nick, nl->idtime);

			for (accitem = LIST_FIRST(&nl->acl); accitem;
				 accitem = LIST_NEXT(accitem, al_lst))
				fprintf(db.ns, "access %s %s\n", nl->nick, accitem->mask);
			x++;
			fflush(db.ns);
#ifdef DBDEBUG
			sSend(":%s PRIVMSG " DEBUGCHAN " :Saved nick data (%s)",
				  NickServ, nl->nick);
#endif
			nl = LIST_NEXT(nl, rn_lst);
		}
	}
	fprintf(db.ns, "done\n");
	fflush(db.ns);
	fclose(db.ns);
#ifdef GLOBOPS_TIMED_MSGS
	sSend(":%s GLOBOPS :Completed save(%i)", NickServ, x);
#else
	sSend(":%s PRIVMSG " LOGCHAN " :Completed save(%i)", NickServ, x);
#endif
}

/**
 * \brief Loads the NickServ database from disk
 */
void readNickData()
{
	RegNickList *rnl = NULL;
	char *command, *tmpp;
	unsigned char *tmpup;
	int done = 0, db_version = 1;
	int line_num = 0, do_enc = 0;

#ifdef REQ_EMAIL
	readRegData();
#endif

	db.ns = fopen(NS_DB, "r");

	if (db.ns == NULL) {
		logDump(corelog, "Unable to open " NS_DB ": %s", strerror(errno));
		return;
	}

	while (!done) {
		if (!(sfgets(dbLine, 1024, db.ns))) {
			if (!done) {
				unexpected_eof(NS_DB);
			}
			done = 1;
			fclose(db.ns);
			return;
		}

		line_num++;

		if (parse_init(&state, dbLine) != 0) {
			/*! \bug XXX make a nicer error here! */
			abort();
		}

		command = parse_getarg(&state);
		if (strcmp(command, "version") == 0) {
			tmpp = parse_getarg(&state);
			assert(tmpp);

			if (tmpp)
				db_version = atoi(tmpp);
		}
		else
		if (strcmp(command, "nick") == 0) {
			rnl = (RegNickList *) oalloc(sizeof(RegNickList));
			char *sNick, *sUser, *sHost, *sPass;

			sNick = parse_getarg(&state);
			sUser = parse_getarg(&state);
			sHost = parse_getarg(&state);
			sPass = parse_getarg(&state);

			if (strlen(sNick) >= NICKLEN) {
				fprintf(stderr,
						NS_DB ":%d: " " Nickname '%.80s' exceeds "
						" NICKLEN.\n", line_num, sNick);
				sshutdown(-1);
			} 

			strcpy(rnl->nick, sNick);
			strncpyzt(rnl->user, sUser, USERLEN);
			SetDynBuffer(&rnl->host, sHost);

			if (db_version < 2)
			{
				if (strlen(sPass) > PASSLEN) {
					fprintf(stderr,
						NS_DB ":%d: " " Password for nick '%s' "
						" exceeds PASSLEN.\n", line_num, sNick);
					sshutdown(-1);
				}

				strcpy((char *)rnl->password, xorit(sPass));
			}
			else {
				char encType = *sPass;

				if (encType == '@') {
					if (strlen(sPass+1) > PASSLEN) {
						fprintf(stderr,
							NS_DB ":%d: " " Password for nick '%s' "
							" exceeds PASSLEN.\n", line_num, sNick);
						sshutdown(-1);
					}

					strcpy((char *)rnl->password, xorit(sPass + 1));
					do_enc = 0;
				}
				else if (encType == '$') {
					int q, len;

					tmpup = fromBase64(sPass + 1, &len);
					assert(tmpup);

					for(q = 0; q < 16; q++)
						rnl->password[q] = tmpup[q];
					do_enc = 1;
					FREE(tmpup);
				}
				else
					rnl->password[0] = '\0';
			}

			rnl->timestamp = (time_t) atol(parse_getarg(&state));
			rnl->timereg = (time_t) atol(parse_getarg(&state));
			rnl->flags = atoi(parse_getarg(&state));

			if (db_version >= 3) {
				const char *idString = parse_getarg(&state);
				int av, bv;

				sscanf(idString, "%X*%X", &av, &bv);
				rnl->regnum.SetDirect(top_regnick_idnum, av, bv);				
			}
			else {
				rnl->regnum.SetNext(top_regnick_idnum);
			}

			if (do_enc)
				rnl->flags |= NENCRYPT;
			else
				rnl->flags &= ~NENCRYPT;

			rnl->opflags = 0;
			rnl->idtime = DEF_NDELAY;
			ADD_MEMO_BOX(rnl);
			addRegNick(rnl);
		} else if (strcmp(command, "is") == 0) {
			char *data = parse_getarg(&state);
			if (rnl && data)
				rnl->is_readtime = atol(data);
		} else if (strcmp(command, "oper") == 0) {
			char *opflags_s;
			if (rnl && (rnl == getRegNickData(parse_getarg(&state)))) {
				if ((opflags_s = parse_getarg(&state)))
					rnl->opflags |=
						(strtoul(opflags_s, (char **)0, 10) &
						 ~(OROOT | OSERVOP));
				if (rnl->opflags)
					addOpData(rnl);
			}
		} else if (strcmp(command, "url") == 0) {
			if (rnl && (rnl == getRegNickData(parse_getarg(&state))))
			{
				rnl->url = strdup(parse_getarg(&state));
				if (strlen(rnl->url) > (URLLEN - 1))
					rnl->url[URLLEN - 1] = '\0';
			}
		} else if (strcmp(command, "gecos") == 0) {
#ifdef TRACK_GECOS
			char *gecos = parse_getallargs(&state);
			if (gecos != NULL)
				rnl->gecos = strdup(gecos);
#endif
		} else if (strcmp(command, "akey") == 0) {
#ifdef REQ_EMAIL
			if (rnl)
				rnl->email_key = atoi(parse_getarg(&state));
#endif
		} else if (strcmp(command, "chkey") == 0) {
			char *tmpp = parse_getarg(&state);
			if (rnl && tmpp)
				rnl->chpw_key = strtoul(tmpp, (char **)0, 16);
		} else if (strcmp(command, "markby") == 0) {
			char *mby;

			rnl = getRegNickData(parse_getarg(&state));
			if (!rnl || !(mby = parse_getarg(&state)))
				continue;
			rnl->markby = strdup(mby);
		} else if (strcmp(command, "access") == 0) {
			rnl = getRegNickData(parse_getarg(&state));
			addAccItem(rnl, parse_getarg(&state));
		} else if (!strcmp(command, "email")) {
			rnl = getRegNickData(parse_getarg(&state));

			strncpyzt(rnl->email, parse_getarg(&state), EMAILLEN);

			if (!strcmp(rnl->email, "(none)"))
				strcat(rnl->email, " ");
		} else if (!strcmp(command, "idtime")) {
			rnl = getRegNickData(parse_getarg(&state));
			rnl->idtime = atoi(parse_getarg(&state));
		} else if (!strcmp(command, "done"))
			done = 1;
		else {
			fprintf(stderr, NS_DB ":%d: Error reading nick data (%s)",
					line_num, dbLine);
			sshutdown(-1);
		}
#ifdef DBDEBUG
		sSend(":%s PRIVMSG " DEBUGCHAN " :Read nick data (%s)", NICKSERV,
			  dbLine);
#endif

		parse_cleanup(&state);
	}
	fclose(db.ns);

	readMemoData();
}

/* ChanServ, all things ChanServ, and the like.... */

/** 
 * \brief Saves the URLS of channels to urls.txt
 */
void saveChanUrls(RegChanList * first)
{
return;
	RegChanList *cl = first;
	const char *tmpFounderNick;
	FILE *fpOut;
	char cstr[258];

	fpOut = fopen(CS_DIR "urls.txt.new", "w");
	if (!fpOut)
		return;
	while (cl) {
		if (!cl->name || !*cl->name || (cl->restrictlevel > 0)) {
			cl = cl->next;
			continue;
		}
		if (cl->mlock & (PM_S | PM_P | PM_I | PM_K)) {
			cl = cl->next;
			continue;
		}

		tmpFounderNick = cl->founderId.getNick();

		if (tmpFounderNick == NULL)
			tmpFounderNick = "-";

		sprintf(cstr, ":%.255s",
				md5_printable(md5_password((u_char *)cl->password)));
		fprintf(fpOut, "C %s %s %ld %ld %s\n", cl->name,
				tmpFounderNick, cl->mlock, cl->flags,
				md5_printable(md5_password((u_char *)cstr)));
		memset(cstr, 0, 255);
		if (cl->topic)
			fprintf(fpOut, "topic %s %s %lu :%s\n", cl->name,
					*cl->tsetby ? cl->tsetby : "-", cl->ttimestamp,
					cl->topic);
		if (cl->url)
			fprintf(fpOut, "url %s :%s\n", cl->name, cl->url);
		fflush(fpOut);
		cl = cl->next;
	}
	if (fclose(fpOut) < 0) {
		sSend("GLOBOPS :Fatal error in writing out urls.txt.new.");
		return;
	}
	if (rename("chanserv/urls.txt.new", "chanserv/urls.txt") < 0) {
		sSend("GLOBOPS :Error renaming urls.txt.new.");
		return;
	}
}

/** 
 * \brief Saves the ChanServ database to disk
 */
void saveChanData(RegChanList * first)
{
	RegChanList *cl = first;
	cAccessList *accitem;
	cAkickList *akitem;
	const char *tmpFounderNick;
	int x = 0;

	saveChanUrls(first);
	db.cs = NULL;
	db.cs = fopen(CS_DIR "chanserv.db", "w");
	if (db.cs == NULL) {
		sSend(":%s GOPER :ERROR: Unable to save Channel database fopen failed: %s", myname, strerror(errno));
		logDump(corelog, "Unable to save Channel database: fopen: %s", strerror(errno));
		return;
	}

	fprintf(db.cs, "version 2\n");

	while (cl) {
		tmpFounderNick = cl->founderId.getNick();

		if (tmpFounderNick == NULL)
			tmpFounderNick = "-";

		/* New chan insertion point */
		fprintf(db.cs, "channel ");
		fprintf(db.cs, "%s %s ", cl->name, tmpFounderNick);
		fprintf(db.cs, "%lu %lu ", cl->mlock, cl->flags);

		if (!(cl->flags & CENCRYPT)) {
			strcpy(cryptstr, (char *)cl->password);
			xorit(cryptstr);
			fprintf(db.cs, "@%s ", cryptstr);
		}
		else {
			u_char *tmpup = toBase64(cl->password, 16);
			if (!tmpup)
				abort();
			fprintf(db.cs, "$%s ", tmpup);
			FREE(tmpup);
		}

		fprintf(db.cs, "%lu ", (unsigned long)cl->timereg);
		fprintf(db.cs, "%lu ", (unsigned long)cl->timestamp);
		fprintf(db.cs, "%s %lu %i ", cl->key, cl->limit, cl->memolevel);
		fprintf(db.cs, "%i %i :%s\n", cl->tlocklevel, cl->restrictlevel,
				cl->desc);
		if (cl->topic)
			fprintf(db.cs, "topic %s %s %lu :%s\n", cl->name, cl->tsetby,
					cl->ttimestamp, cl->topic);
		if (cl->url)
			fprintf(db.cs, "url %s :%s\n", cl->name, cl->url);
		if (cl->autogreet != NULL)
			fprintf(db.cs, "autogreet %s :%s\n", cl->name, cl->autogreet);
		if ((cl->flags & CMARK) && cl->markby)
			fprintf(db.cs, "markby %s %s\n", cl->name, cl->markby);
		if (cl->chpw_key)
			fprintf(db.cs, "chkey %X\n", cl->chpw_key);
		if (cl->firstOp) {
			const char *tmpName;

			for (accitem = cl->firstOp; accitem; accitem = accitem->next) {
				tmpName = (accitem->nickId).getNick();

				if (tmpName == NULL)
				    continue;
				fprintf(db.cs, "op %s %s ", cl->name, tmpName);
				fprintf(db.cs, "%i\n", accitem->uflags);
			}
		}

		if (cl->firstAkick) {
			for (akitem = cl->firstAkick; akitem; akitem = akitem->next) {
				fprintf(db.cs, "akick %s %s ", cl->name, akitem->mask);
				fprintf(db.cs, "%lu :%s\n", (u_long) akitem->added,
						akitem->reason);
			}
		}
		x++;
		fflush(db.cs);
#ifdef DBDEBUG
		sSend(":%s PRIVMSG " DEBUGCHAN " :Saved chan data (%s)", ChanServ,
			  cl->name);
#endif
		cl = cl->next;
	}
	fprintf(db.cs, "done\n");
	fflush(db.cs);
	fclose(db.cs);
#ifdef GLOBOPS_TIMED_MSGS
	sSend(":%s GLOBOPS :Completed save(%i)", ChanServ, x);
#else
	sSend(":%s PRIVMSG " LOGCHAN " :Completed save(%i)", ChanServ, x);
#endif
}

/** 
 * \brief Loads the ChanServ database from disk
 */
void readChanData()
{
	RegChanList *rcl;
	RegNickList *rnl;
	char *command;
	int done = 0, db_version = 1;
	int line_num = 0;
	char *pass;
	char *topic;

	rcl = NULL;
	rnl = NULL;
	db.cs = fopen(CS_DIR "chanserv.db", "r");
	if (db.cs == NULL)
		return;

	while (!done) {
		if ((sfgets(dbLine, 2048, db.cs)) == 0) {
			if (!done) {
				unexpected_eof(CS_DB);
			}
			done = 1;
			fclose(db.cs);
			return;
		}
		line_num++;

		if (parse_init(&state, dbLine) != 0) {
			fprintf(stderr,
					CS_DIR "chanserv.db:%d: " " Fatal error during read "
					" (Null line?) \n", line_num);
			abort();
		}

		command = parse_getarg(&state);
		if (!strcmp(command, "version")) {
			db_version = atoi(parse_getarg(&state));
		}
		else if (!strcmp(command, "channel")) {
			char *sChannelName, *sFounderNick;

			rcl = (RegChanList *) oalloc(sizeof(RegChanList));
			sChannelName = parse_getarg(&state);
			sFounderNick = parse_getarg(&state);
			initRegChanData(rcl);

			if (strlen(sChannelName) >= CHANLEN) {
				fprintf(stderr,
						CS_DIR "chanserv.db:%d: "
						" Channel name '%.80s' exceeds " " CHANLEN.\n",
						line_num, sChannelName);
				sshutdown(-1);
			} else if (strlen(sFounderNick) >= NICKLEN) {
				fprintf(stderr,
						CS_DIR "chanserv.db:%d: "
						" Founder nick name '%.80s' " " (%.80s)"
						" exceeds NICKLEN.\n", line_num, sFounderNick,
						sChannelName);
				sshutdown(-1);
			}

			if (!sChannelName || !sFounderNick) {
				fprintf(stderr,
						CS_DIR "chanserv.db:%d: " " Parse error. (%p, %p)",
						line_num, sChannelName, sFounderNick);
				sshutdown(-1);
			}

			rnl = getRegNickData(sFounderNick);

			strcpy(rcl->name, sChannelName);

			if (rnl)
			{
				rcl->founderId = rnl->regnum;
				rnl->chans++;
			}
			else
				rcl->founderId = RegId(0, 0);

			rcl->mlock = atol(parse_getarg(&state));
			rcl->flags = atol(parse_getarg(&state));
			pass = parse_getarg(&state);	/*! \bug XXX verify this works */
			if (!pass) {
				fprintf(stderr,
						CS_DIR "chanserv.db:%d: " " Null password?!",
						line_num);
				sshutdown(-1);
			} 

			if ((db_version < 2) || *pass == '@') {
				if (db_version < 2)
					strcpy((char *)rcl->password, pass);
				else
					strcpy((char *)rcl->password, pass + 1);

				if (strlen(pass) > (u_int)((db_version < 2) ? PASSLEN : PASSLEN+1)) {
					fprintf(stderr,
							CS_DIR "chanserv.db:%d: " " password > PASSLEN",
							line_num);
					sshutdown(-1);
				}

				xorit((char *)rcl->password);
				rcl->flags &= ~CENCRYPT;
			}
			else {
				u_char *tmpup = fromBase64(pass+1, NULL);
				int q;

				if (!tmpup)
					abort();
				for(q = 0; q < 16; q++)
					rcl->password[q] = tmpup[q];
				FREE(tmpup);
				rcl->flags |= CENCRYPT;
			}

			rcl->timereg = (time_t) atol(parse_getarg(&state));
			rcl->timestamp = (time_t) atol(parse_getarg(&state));
			strncpyzt(rcl->key, parse_getarg(&state), KEYLEN);
			rcl->limit = atol(parse_getarg(&state));
			rcl->memolevel = atoi(parse_getarg(&state));
			rcl->tlocklevel = atoi(parse_getarg(&state));
			rcl->restrictlevel = atoi(parse_getarg(&state));
			/* The rest of the line - skipping the leading : */
			topic = parse_getallargs(&state);
			if (topic == NULL)
				rcl->desc[0] = '\0';
			else
				strncpyzt(rcl->desc, topic, CHANDESCBUF);
			addRegChan(rcl);
			mostchans++;
		} else if (!strcmp(command, "topic")) {
			rcl = getRegChanData(parse_getarg(&state));
			if (rcl == NULL)
				continue;
			strncpyzt(rcl->tsetby, parse_getarg(&state), NICKLEN);
			rcl->ttimestamp = (time_t) atol(parse_getarg(&state));
			/* The rest of the topic, skipping the : in it */
			topic = parse_getallargs(&state);
			if (topic == NULL)
				rcl->topic = NULL;
			else
				rcl->topic = strdup(topic);
		} else if (!strcmp(command, "url")) {
			rcl = getRegChanData(parse_getarg(&state));
			if (rcl == NULL)
				continue;
			topic = parse_getallargs(&state);
			if (topic == NULL)
				rcl->url = NULL;
			else
				rcl->url = strdup(topic);
		} else if (!strcmp(command, "autogreet")) {
			rcl = getRegChanData(parse_getarg(&state));
			if (rcl == NULL)
				continue;
			topic = parse_getallargs(&state);
			if (topic == NULL)
				rcl->autogreet = NULL;
			else
				rcl->autogreet = strdup(topic);
		} else if (!strcmp(command, "markby")) {
			rcl = getRegChanData(parse_getarg(&state));
			if (rcl == NULL)
				continue;
			topic = parse_getarg(&state);
			if (topic == NULL)
				rcl->markby = NULL;
			else
				rcl->markby = strdup(topic);
		} else if (strcmp(command, "chkey") == 0) {
			char *tmpp = parse_getarg(&state);
			if (rcl && tmpp)
				rcl->chpw_key = strtoul(tmpp, (char **)0, 16);
		} else if (!strcmp(command, "op")) {
			cAccessList *lame;
			char *tmpName;

			rcl = getRegChanData(parse_getarg(&state));
			if (rcl == NULL)
				continue;
			tmpName = parse_getarg(&state);
			if ((rnl = getRegNickData(tmpName)) != NULL)
			{
				lame = (cAccessList *) oalloc(sizeof(cAccessList));
				lame->nickId = rnl->regnum;
				lame->uflags = atoi(parse_getarg(&state));
				addChanOp(rcl, lame);
			}
		} else if (!strcmp(command, "akick")) {
			cAkickList *lame;
			lame = (cAkickList *) oalloc(sizeof(cAkickList));
			rcl = getRegChanData(parse_getarg(&state));
			if (rcl == NULL)
				continue;
			strncpyzt(lame->mask, parse_getarg(&state), 70);
			lame->added = (time_t) atol(parse_getarg(&state));
			/* The rest of the string... */
			topic = parse_getallargs(&state);
			if (topic == NULL)
				lame->reason[0] = '\0';
			else {
				strncpyzt(lame->reason, topic, NICKLEN + 50);
			}
			addChanAkick(rcl, lame);
		} else if (!strcmp(command, "done"))
			done = 1;
		else {
			fprintf(stderr, "GLOBOPS :Read chan data (%s)", dbLine);
			sshutdown(-1);
		}

#ifdef DBDEBUG
		sSend(":%s PRIVMSG " DEBGUGCHAN " :Read chan data (%s)", ChanServ,
			  dbLine);
#endif

		parse_cleanup(&state);
	}
	fclose(db.cs);
}


/** 
 * \brief Saves the Memo database to disk
 */
void saveMemoData(void)
{
	RegNickList *nl;
	MemoBox *tmp;
	MemoList *memo;
	MemoBlock *mbitem;
	const char *tmpNickName;
	int bucket;
	int totmemos;
	int totboxes;

	db.ms = fopen(MS_DB, "w");

	totmemos = 0;
	totboxes = 0;

	for (bucket = 0; bucket < NICKHASHSIZE; bucket++) {
		nl = LIST_FIRST(&RegNickHash[bucket]);

		while (nl != NULL) {
			tmp = nl->memos;
			if (tmp != NULL) {
				totboxes++;
				fprintf(db.ms, "data %s %i %i %i %i\n", nl->nick,
						tmp->memocnt, tmp->flags, tmp->flags, tmp->max);
				if (tmp->forward)
					fprintf(db.ms, "redirect %s %s\n", nl->nick,
							tmp->forward->nick);
				for (mbitem = tmp->firstMblock;
					 mbitem; 
					 mbitem = mbitem->next
				    )
				{
					if ((tmpNickName = mbitem->blockId.getNick()))
						fprintf(db.ms, "mblock %s\n", tmpNickName);
				}

				for (memo = LIST_FIRST(&tmp->mb_memos); memo;
					 memo = LIST_NEXT(memo, ml_lst)) {
					if (ShouldMemoExpire(memo, 0)) {
						memo = LIST_NEXT(memo, ml_lst);
						if (memo == NULL)
							break;
						continue;
					}
					fprintf(db.ms, "memo %s %i %i %lu %s %s :%s\n",
							nl->nick, 0, memo->flags, (long)memo->sent,
							memo->from, memo->to, memo->memotxt);
					totmemos++;
				}
			}
			nl = LIST_NEXT(nl, rn_lst);
		}
	}

	fprintf(db.ms, "done\n");

	fclose(db.ms);

	sSend(":%s PRIVMSG " LOGCHAN " :Completed save(%u boxes, %u memos)",
		  MemoServ, totboxes, totmemos);
}

/** 
 * \brief Loads the memo database from disk
 */
void readMemoData(void)
{
	RegNickList *nick = NULL;
	RegNickList *from, *rnlb;
	MemoList *newmemo;
	char *command;
	char *topic;
	int done = 0;
	unsigned long linenum = 0;

	db.ms = fopen(MS_DB, "r");

	if (db.ms == NULL)
		return;

	while (!done) {
		if (!(sfgets(dbLine, 2048, db.ms))) {
			if (!done) {
				unexpected_eof(MS_DB);
			}
			done = 1;
			fclose(db.ms);
			return;
		}

		linenum++;
		if (parse_init(&state, dbLine) != 0) {
			/*! \bug XXX be nicer here... */
			abort();
		}

		command = parse_getarg(&state);

		if (strcmp(command, "data") == 0) {
			nick = getRegNickData(parse_getarg(&state));
			/*! \bug Increment the arguments.. we REALLY need to fix the dbs */
			(void)parse_getarg(&state);
			(void)parse_getarg(&state);
			if (nick) {
				nick->memos->flags = atoi(parse_getarg(&state));
				nick->memos->max = atoi(parse_getarg(&state));
				if (nick->memos->max <= 0)
					nick->memos->max = MS_DEF_RCV_MAX;
			}
		} else if (strcmp(command, "mblock") == 0) {
			MemoBlock *mbitem;
			if (nick && nick->memos) {
				char *nickToBlock;

				if ((nickToBlock = parse_getarg(&state))
					&& (rnlb = getRegNickData(nickToBlock))) {

					mbitem = (MemoBlock *) oalloc(sizeof(MemoBlock));
					mbitem->blockId = rnlb->regnum;
					mbitem->next = nick->memos->firstMblock;
					nick->memos->firstMblock = mbitem;
				}
			}
		} else if (strcmp(command, "redirect") == 0) {
			nick = getRegNickData(parse_getarg(&state));
			if (nick != NULL)
				nick->memos->forward =
					getRegNickData(parse_getarg(&state));
		} else if (strcmp(command, "memo") == 0) {
			char *cn;

			cn = parse_getarg(&state);

			nick = getRegNickData(cn);
			if (nick == NULL) {
				printf("memo: %s not valid\n", cn);
				continue;
			}
			newmemo = (MemoList *) oalloc(sizeof(MemoList));
			newmemo->realto = nick;
			/*! \bug Increment the argument 1, we need to fix the db */
			(void)parse_getarg(&state);
			newmemo->flags = atoi(parse_getarg(&state));
			newmemo->sent = (time_t) atol(parse_getarg(&state));

			/* Memo expiration code */

			if ((time(NULL) - newmemo->sent) >= NICKDROPTIME) {
				FREE(newmemo);
				continue;
			}

			strncpyzt(newmemo->from, parse_getarg(&state), NICKLEN);
			from = getRegNickData(newmemo->from);
			strncpyzt(newmemo->to, parse_getarg(&state), CHANLEN);

			/* Read the memo, skipping the leading : */
			topic = parse_getallargs(&state);
			if (topic == NULL) {
				FREE(newmemo);
				continue;
			}
			newmemo->memotxt = strdup(topic);

			/* Add the memo to the users memobox */
			LIST_ENTRY_INIT(newmemo, ml_lst);
			LIST_INSERT_HEAD(&nick->memos->mb_memos, newmemo, ml_lst);
			if (from && newmemo->flags & MEMO_UNREAD)
				LIST_INSERT_HEAD(&from->memos->mb_sent, newmemo, ml_sent);
			nick->memos->memocnt++;
		} else if (strcmp(command, "done") == 0) {
			done = 1;
		} else {
			fprintf(stderr, "Error in reading memo data (%s) %lu\n",
					dbLine, linenum);
			sshutdown(-1);
		}
#ifdef DBDEBUG
		sSend(":%s PRIVMSG " DEBUGCHAN " :Read memo data (%s)", MemoServ,
			  dbLine);
#endif

		parse_cleanup(&state);
	}

	fclose(db.ms);
}


/** 
 * \brief Saves the Clone rule database to disk
 */
void saveTriggerData(void)
{
	int totrules = 0;
	CloneRule *rule;
	extern CloneRule *first_crule;

	db.trigger = fopen(TRG_DB, "w");
	if (!db.trigger) {
		logDump(corelog,
				"Unable to open trigger database for read access: %s",
				strerror(errno));
		sSend
			(":%s GLOBOPS :Unable to open trigger database for read access: %s",
			 NickServ, strerror(errno));
		return;
	}
	for (rule = first_crule; rule; rule = rule->next, ++totrules) {
		if (fprintf
			(db.trigger, "Trigger %s %d %d %ld\n", rule->mask,
			 rule->trigger, rule->utrigger, rule->flags) < 0) {
			logDump(corelog,
					"Error in writing rule to trigger database: %s",
					strerror(errno));
			break;
		}
		if (rule->kill_msg) {
			if ((fprintf(db.trigger, "Killmsg %s\n", rule->kill_msg)) < 0) {
				logDump(corelog,
						"Error in writing trigger rule (killmsg) to trigger database: %s",
						strerror(errno));
				break;
			}
		}
		if (rule->warn_msg) {
			if ((fprintf(db.trigger, "Warnmsg %s\n", rule->warn_msg)) < 0) {
				logDump(corelog,
						"Error in writing trigger rule (warnlmsg) to trigger database: %s",
						strerror(errno));
				break;
			}
		}
	}
	fprintf(db.trigger, "done\n");

	if (fclose(db.trigger) < 0) {
		logDump(corelog, "Error closing trigger database: %s",
				strerror(errno));
		return;
	}
	sSend(":%s PRIVMSG " LOGCHAN " :Completed trigger save (%u rules)",
		  OperServ, totrules);
}

/** 
 * \brief Loads the clone rule database from disk
 */
void readTriggerData(void)
{
	CloneRule *rule = NULL;
	char *command, *text;
	int done = 0;
	unsigned long linenum = 0;

	db.trigger = fopen(TRG_DB, "r");

	if (db.trigger == NULL) {
		logDump(corelog,
				"Unable to open trigger database for read access: %s",
				strerror(errno));
		return;
	}

	while (!done) {
		if (!(sfgets(dbLine, 2048, db.trigger))) {
			if (!done) {
				unexpected_eof(TRG_DB);
			}
			done = 1;
			fclose(db.trigger);
			return;
		}
		linenum++;

		if (parse_init(&state, dbLine) != 0) {
			/*! \bug XXX be nicer here... */
			abort();
		}

		command = parse_getarg(&state);

		if (strcmp(command, "Trigger") == 0) {
			rule = NewCrule();
			rule->kill_msg = NULL;
			rule->warn_msg = NULL;
			strncpyzt(rule->mask, parse_getarg(&state),
					  sizeof(rule->mask));
			rule->mask[sizeof(rule->mask) - 1] = '\0';
			rule->trigger = atoi(parse_getarg(&state));
			rule->utrigger = atoi(parse_getarg(&state));
			rule->flags = atol(parse_getarg(&state));
			AddCrule(rule, -2);	/* -2 is magic for append to end */
		} else if (strcmp(command, "Killmsg") == 0) {
			text = parse_getallargs(&state);
			if (text && rule)
				rule->kill_msg = strdup(text);
		} else if (strcmp(command, "Warnmsg") == 0) {
			text = parse_getallargs(&state);
			if (text && rule)
				rule->warn_msg = strdup(text);
		} else if (strcmp(command, "done") == 0) {
			done = 1;
		} else {
			fprintf(stderr, "Error in reading trigger data (%s) %lu\n",
					dbLine, linenum);
			parse_cleanup(&state);
			return;
		}
#ifdef DBDEBUG
		sSend(":%s PRIVMSG " DEBUGCHAN " :Read trigger data (%s)",
			  OperServ, dbLine);
#endif

		parse_cleanup(&state);
	}

	fclose(db.trigger);
}

/** 
 * \brief Saves the InfoServ database to disk
 */
void saveInfoData(void)
{
	SomeNews *news = NULL;
	int index = 0;

	db.is = fopen(IS_DB, "w");

	if (db.is == NULL) {
		sSend(":%s GOPER :ERROR: Unable to save Info Article database fopen failed: %s", myname, strerror(errno));
		logDump(corelog, "Unable to save Info Article database: fopen: %s", strerror(errno));
		return;
	}

	/*! \bug Huh? Why check for NULL -twice- ? */
	if (is_listhead != NULL) {	/* In case list is empty, thanks to Echostar for pointing this cack-handed error out...  */
		for (news = is_listhead, index = 0; news;
			 news = news->next, index++)
		{
			fprintf(db.is, "article %i %s %li %s\n", news->importance,
					news->from, news->timestamp, news->header);
			dbWriteString(db.is, news->content);
		}
	}

	fprintf(db.is, "done\n");

	fclose(db.is);

	sSend(":%s PRIVMSG " LOGCHAN " :Completed save(%i articles)", InfoServ,
		  index);
}


/** 
 * \brief Loads the InfoServ database from disk
 */
void readInfoData(void)
{
	char *command;
	int done = 0;
	SomeNews *news, *tmpnews;

	is_listhead = NULL;
	db.is = fopen(IS_DB, "r");

	if (db.is == NULL)
		return;

		/**
         * There was still a crash bug in what was here before...
         * cleaning this up .. allocate memory when it's
         * needed, don't allocate it in the beginning and
         * give it up later if superflous in a linked list
         * load.
         * -Mysidia
         */

	while (!done) {
		if (!(sfgets(dbLine, 2048, db.is))) {
			if (!done) {
				unexpected_eof(IS_DB);
			}
			done = 1;
			fclose(db.is);
			return;
		}

		if (parse_init(&state, dbLine) != 0) {
			/*! \bug XXX be nicer here... */
			abort();
		}

		command = parse_getarg(&state);

		if (!strcmp(command, "article")) {
			char *temp;

			news = (SomeNews *) oalloc(sizeof(SomeNews));
			if (!is_listhead)
				is_listhead = news;
			else {
				for (tmpnews = is_listhead; tmpnews->next;
					 tmpnews = tmpnews->next);
				tmpnews->next = news;
			}
			news->importance = atoi(parse_getarg(&state));
			strncpyzt(news->from, parse_getarg(&state), NICKLEN);
			news->timestamp = atol(parse_getarg(&state));
			temp = parse_getallargs(&state);
			if (temp)
				news->header = strdup(temp);
			news->content = dbReadString(db.is);

			if (news->timestamp > is_last_post_time)
				is_last_post_time = news->timestamp;
		}

		else if (!strcmp(command, "done"))
			done = 1;
		else
			sshutdown(-1);

		parse_cleanup(&state);
	}
	fclose(db.is);
}

/** 
 * \brief Periodic synchronizations (saves) of various databases
 */
void sync_cfg(char *type)
{
	void writeServicesTotals();

	switch (*type) {
	case '1':
		syncNickData((time_t) (CTime + (SYNCTIME * 3)));
		writeServicesTotals();
		nextCsync = (CTime + SYNCTIME);
		nextMsync = (CTime + (SYNCTIME * 2));
		nextNsync = (CTime + (SYNCTIME * 3));
		timer(SYNCTIME, sync_cfg, (void *)(const_cast<char *>("2")));
		break;
	case '2':
		syncChanData((time_t) (CTime + (SYNCTIME * 2)));
		writeServicesTotals();
		nextMsync = (CTime + SYNCTIME);
		nextNsync = (CTime + (SYNCTIME * 2));
		nextCsync = (CTime + (SYNCTIME * 2));

		timer(SYNCTIME, sync_cfg, (void *)const_cast<char *>("3"));
		break;
	case '3':
		syncMemoData((time_t) (CTime + SYNCTIME));
		nextNsync = nextCsync = nextMsync = (CTime + SYNCTIME);
		timer(SYNCTIME, sync_cfg, (void *)const_cast<char *>("4"));
		break;
	case '4':
		syncNickData((time_t) (CTime + SYNCTIME));
		syncChanData((time_t) (CTime + (SYNCTIME * 2)));
		syncMemoData((time_t) (CTime + (SYNCTIME * 3)));
		nextNsync = (CTime + SYNCTIME);
		nextCsync = (CTime + SYNCTIME * 2);
		nextMsync = (CTime + SYNCTIME * 3);
		timer(SYNCTIME, sync_cfg, (void *)const_cast<char *>("1"));
		break;
	}
}

#ifdef REQ_EMAIL
void saveRegData(nRegList *first) {
        nRegList *nl = first;
        db.nsreg = fopen("nickserv/nickregs.db", "w");
        while(nl) {
                fprintf(db.nsreg, "-\n%s\n%i\n", nl->email, nl->regs);
                nl=nl->next;
                fflush(db.nsreg);
        }
        fclose(db.nsreg);
}

void readRegData(void) {
        char tmp2[10 + HOSTLEN], tmp[10];
        int lineread = 1, i;
        db.nsreg = fopen("nickserv/nickregs.db", "r");
        if(!db.nsreg)
          return;
        while(sfgets(tmp, 10, db.nsreg)) {
                if(tmp[0] != '-') {
                        sSend("GOPER :!ERROR! NickServ databases are corrupted (line %i)", lineread);
                        sshutdown(1);
                }
                sfgets(tmp2, 10 + HOSTLEN, db.nsreg);
                addNReg(tmp2);
                sfgets(tmp, 10, db.nsreg);
                i = atoi(tmp);
                for(i--;i;i--)
                  addNReg(tmp2);
        }
        fclose(db.nsreg);
}
#endif
