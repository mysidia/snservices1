/*
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
 * \file sipc.cc
 * \brief Services IPC Methods
 *
 * Procedures for talking with other software
 *
 * \mysid
 * \date 2001
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "services.h"
#include "options.h"
#include "sipc.h"
#include "nickserv.h"
#include "chanserv.h"
#include "hash/md5.h"
#include "hash/md5pw.h"
#include "log.h"
extern FILE *corelog;

int ipcPort = 0;

int NickGetEnc(RegNickList *);
int ChanGetEnc(RegChanList *);
const char *GetAuthChKey(const char*, const char*, time_t, u_int32_t);
const char *PrintPass(u_char pI[], char enc);
extern RegId top_regnick_idnum;

/********************************************************************/

/// An IPC system login
struct ssUInfo{
	char *userName; ///< Username of login
	char *password; ///< Password of login
	flag_t priv;    ///< Privileges of login
};

const ssUInfo *getServicesSysUser(const char *username)
{
	static ssUInfo users[] = {
		{"WWW/ahurt", "test",	PRIV_QUERY_NICK_ISREG | PRIV_LOGW},
		{"WWW/setpass", "test",	PRIV_ALTER_RNICK_GEN | PRIV_RCHAN_LOGIN | PRIV_RNICK_LOGIN },
		{"mysid/test", "test",	PRIV_QUERY_NICK_ISREG | PRIV_QUERY_NICK_PUBLIC | PRIV_QUERY_AKILL_LIST | PRIV_QUERY_NICK_PRIVATE | PRIV_QUERY_CHAN_PUBLIC | PRIV_LOGW},
		{NULL}
	};
	int i = 0;

	for(i = 0; users[i].userName; i++) {
		if (strcmp(users[i].userName, username) == 0)
			return &users[i];
	}
	return NULL;
}

/********************************************************************/
/* Handle Queries and Sets */

int queryRnickString(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRnickStringFixed(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRnickFlag(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRnickLong(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRnickUint(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRnickUchar(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRnickTime(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);

int alterRnickStringD(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickStringFixed(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickEmail(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickFlag(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickLong(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickUint(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickUchar(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRnickTime(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x);

//#define ORNL(x) offsetof(RegNickList, x)
#define ORNL(q)  (((size_t) &((RegNickList *)1)->q - 1))

struct
{
	const char *field;
	int (* func)(RegNickList *, IpcConnectType *p, parse_t *pb, int x);
	size_t off;
	flag_t priv, a_priv;
	int (* a_func)(RegNickList *, IpcConnectType *p, parse_t *pb, int x);
}
rnickQueryTable[] =
{
	{ "URL", queryRnickString,	ORNL(url),
	   0, 0, alterRnickStringD },

	{ "EMAIL",	queryRnickStringFixed,	ORNL(email),
	  PRIV_QUERY_NICK_PRIVATE, PRIV_ALTER_RNICK_2, alterRnickEmail },

	{ "LAST-HOST-UNMASK",	queryRnickString,	ORNL(host),
	  PRIV_QUERY_NICK_UNMASK, PRIV_UNDEF, queryRnickString },

	{ "FLAGS",	queryRnickFlag,		ORNL(flags),
	  PRIV_QUERY_NICK_PUBLIC, PRIV_ALTER_RNICK_3, alterRnickFlag },

	{ "OPFLAGS",	queryRnickFlag,		ORNL(opflags),
	  PRIV_QUERY_NICK_PRIVATE, PRIV_UNDEF, alterRnickFlag },

	{ "BADPWS",	queryRnickUchar,	ORNL(badpws),
	  PRIV_QUERY_NICK_PUBLIC, PRIV_ALTER_RNICK_GEN, alterRnickUchar },

	{ "TIMEREG",	queryRnickTime,		ORNL(timereg),
	  PRIV_QUERY_NICK_PUBLIC, PRIV_ALTER_RNICK_3, alterRnickTime },

	{ "NUM-MASKS", 	queryRnickUint,		ORNL(amasks),
	  PRIV_QUERY_NICK_PRIVATE, PRIV_UNDEF, alterRnickTime },

	{ "IS-READTIME", queryRnickTime,	ORNL(is_readtime),
	  PRIV_QUERY_NICK_PRIVATE, PRIV_ALTER_RNICK_GEN, alterRnickTime },

	{ NULL }
};


/* For Channels */

int queryRchanString(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRchanStringFixed(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRchanFlag(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRchanLong(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRchanUint(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRchanUchar(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int queryRchanTime(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);

int alterRchanStringD(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanStringFixed(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanEmail(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanFlag(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanLong(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanUint(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanUchar(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);
int alterRchanTime(RegChanList *rnl, IpcConnectType *p, parse_t *pb, int x);

#define ORCL(q)  (((size_t) &((RegChanList *)1)->q - 1))

struct
{
	const char *field;
	int (* func)(RegChanList *, IpcConnectType *p, parse_t *pb, int x);
	size_t off;
	flag_t priv, a_priv;
	int (* a_func)(RegChanList *, IpcConnectType *p, parse_t *pb, int x);
}
rchanQueryTable[] =
{
	{ "DESC", queryRchanStringFixed,	ORCL(desc),
	   PRIV_QUERY_CHAN_PUBLIC, PRIV_ALTER_RCHAN_2, 0 /*alterRchanStringFixed*/ },
        { "TIMEREG",    queryRchanTime,         ORCL(timereg),
          PRIV_QUERY_CHAN_PUBLIC, PRIV_ALTER_RNICK_3, 0 /*alterRchanTime*/ },
	{ NULL }
};

/********************************************************************/

char *IpcQ::qPush(char *text, char *sep, int *rlen)
{
	IpcQel *z;

	z = (IpcQel *)oalloc(sizeof(IpcQel) + (sep - text + 1));

	*sep = '\0';
	*rlen = z->len = sep - text;
	z->text = (char *)oalloc(z->len + 1);
	strcpy(z->text, text);

	if (firstEls == NULL)
		firstEls = lastEls = z;
	else
	  {
		lastEls->next = z;
		lastEls = z;
	  }

	*sep = '\n';

	return (sep + 1);
}

char *IpcQ::shove(char *textIn, size_t textLen, int *rlen)
{
	char *p, *text = textIn;

	for (p = text; p < (textIn + textLen); p++) {
		if (*p == '\n' || (*p == '\r' && p[1] == '\n')) {
			text = qPush(text, p, rlen);
		}
	}

	return text;
}

int IpcQ::pop(char cmd[IPCBUFSIZE])
{
	IpcQel *f;
	char *cp;

	if (firstEls == NULL)
	  return 0;

	f = firstEls;

	if (firstEls == lastEls)
		firstEls = lastEls = NULL;
	else
		firstEls = firstEls->next;

	strncpy(cmd, f->text, IPCBUFSIZE);
	cmd[IPCBUFSIZE - 1] = '\0';

	cp = cmd + strlen(cmd) - 1;

	if (*cp == '\n' || *cp == '\r')
 		*cp = '\0';

	if ((cp - 1) > cmd && (cp[-1] == '\r' || cp[-1] == '\n'))
		cp[-1] = '\0';

	free(f->text);
	free(f);

	return 1;
}

bool IpcQ::IsEmpty()
{
	if (firstEls == NULL)
		return 1;
	return 0;
}

void IpcQ::empty()
{
	char cmd[1025];

	while(pop(cmd))
		return;
}

/********************************************************************/

int IpcConnectType::CheckPriv(flag_t pSys, flag_t pObj)
{
	if ((pSys & PRIV_UNDEF) || (pObj & PRIV_UNDEF))
		return 1;
	if ((gpriv & pSys) != pSys)
		return 1;

	if ((opriv & pObj) != pObj)
		return 1;
	return 0;
}

int IpcConnectType::CheckOrPriv(flag_t pSys, flag_t pObj)
{
	if (!(pSys & PRIV_UNDEF) && (gpriv & pSys) == pSys)
		return 0;
	if (!(pObj & PRIV_UNDEF) && (opriv & pObj) == pSys)
		return 0;
	return 1;
}

/********************************************************************/

int queryRnickString(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	char *c;
        if (!rnl || !rnl->nick) return 0;

    	c = *(char **)((size_t)rnl + rnickQueryTable[x].off);

	if (c != NULL)
		p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %s", rnl->nick,
		            rnickQueryTable[x].field, c);
	else
		p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %s", rnl->nick,
		            rnickQueryTable[x].field, "");
	return 1;
}

int queryRnickStringFixed(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	char *c;
        if (!rnl || !rnl->nick) return 0;

    	c = (char *)((size_t)rnl + rnickQueryTable[x].off);

	if (c != NULL)
		p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %s", rnl->nick,
		            rnickQueryTable[x].field, c);
	else
		p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %s", rnl->nick,
		            rnickQueryTable[x].field, "");
	return 1;
}

int queryRnickFlag(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	flag_t *c = (flag_t *)((size_t)rnl + rnickQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int queryRnickLong(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	long *c = (long *)((size_t)rnl + rnickQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int queryRnickUint(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	u_int *c = (u_int *)((size_t)rnl + rnickQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int queryRnickUchar(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	u_char *c = (u_char *)((size_t)rnl + rnickQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int queryRnickTime(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	time_t *c = (time_t *)((size_t)rnl + rnickQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int alterRnickStringD(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	char *c = *(char **)((size_t)rnl + rnickQueryTable[x].off);
	char *d = parse_getarg(pb);

	if (c)
		free(c);
	c = (char *)0;

	if (d != NULL) {
		c = str_dup(d);
	}
	*(char **)((size_t)rnl + rnickQueryTable[x].off) = c;
		
	p->fWriteLn("OK ALTER OBJECT RNICK=%s %sx %s", rnl->nick,
	            rnickQueryTable[x].field, c);
	return 1;
}

int alterRnickEmail(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	char *e = parse_getarg(pb);

	if (e == NULL)
		return 0;
	strncpyzt(rnl->email, e, EMAILLEN);

	p->fWriteLn("OK ALTER OBJECT RNICK=%s %sx %s", rnl->nick,
	            rnickQueryTable[x].field, e);
	return 1;
}

int alterRnickFlag(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	flag_t *c = (flag_t *)((size_t)rnl + rnickQueryTable[x].off);
	char *d = parse_getarg(pb);

	if (d == NULL) return 0;

	*c = atol(d);

	p->fWriteLn("OK ALTER OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int alterRnickLong(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	long *c = (long *)((size_t)rnl + rnickQueryTable[x].off);
	char *d = parse_getarg(pb);

	if (d == NULL) return 0;

	*c = atol(d);

	p->fWriteLn("OK ALTER OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int alterRnickUint(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	u_int *c = (u_int *)((size_t)rnl + rnickQueryTable[x].off);
	char *d = parse_getarg(pb);
	int j;

	if (d == NULL) return 0;

	j = atoi(d);

	if (j < 0) j = 0;
	*c = j;

	p->fWriteLn("OK ALTER OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int alterRnickUchar(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	u_char *c = (u_char *)((size_t)rnl + rnickQueryTable[x].off);
	char *d = parse_getarg(pb);
	int X;

	if (d == 0) return 0;
	X = MIN((int)UCHAR_MAX, MAX(0, atoi(d)));
	*c = X;

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}

int alterRnickTime(RegNickList *rnl, IpcConnectType *p, parse_t *pb, int x)
{
	time_t *c = (time_t *)((size_t)rnl + rnickQueryTable[x].off);
	char *d = parse_getarg(pb);
	time_t e;

	if (d == 0) return 0;
	e = atol(d);
	*c = e;

	p->fWriteLn("OK QUERY OBJECT RNICK=%s %sx %ld", rnl->nick,
	            rnickQueryTable[x].field, *c);
	return 1;
}
/********************************************************************/

int queryRchanString(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	char *c;
        if (!rcl || !rcl->name) return 0;

    	c = *(char **)((size_t)rcl + rchanQueryTable[x].off);

	if (c != NULL)
		p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %s", rcl->name,
		            rchanQueryTable[x].field, c);
	else
		p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %s", rcl->name,
		            rchanQueryTable[x].field, "");
	return 1;
}

int queryRchanStringFixed(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	char *c;
        if (!rcl || !rcl->name) return 0;

    	c = (char *)((size_t)rcl + rchanQueryTable[x].off);

	if (c != NULL)
		p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %s", rcl->name,
		            rchanQueryTable[x].field, c);
	else
		p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %s", rcl->name,
		            rchanQueryTable[x].field, "");
	return 1;
}

int queryRchanFlag(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	flag_t *c = (flag_t *)((size_t)rcl + rchanQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int queryRchanLong(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	long *c = (long *)((size_t)rcl + rchanQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int queryRchanUint(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	u_int *c = (u_int *)((size_t)rcl + rchanQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int queryRchanUchar(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	u_char *c = (u_char *)((size_t)rcl + rchanQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int queryRchanTime(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	time_t *c = (time_t *)((size_t)rcl + rchanQueryTable[x].off);

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int alterRchanStringD(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	char *c = *(char **)((size_t)rcl + rchanQueryTable[x].off);
	char *d = parse_getarg(pb);

	if (c)
		free(c);
	c = (char *)0;

	if (d != NULL) {
		c = str_dup(d);
	}
	*(char **)((size_t)rcl + rchanQueryTable[x].off) = c;
		
	p->fWriteLn("OK ALTER OBJECT RCHAN=%s %sx %s", rcl->name,
	            rchanQueryTable[x].field, c);
	return 1;
}

int alterRchanFlag(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	flag_t *c = (flag_t *)((size_t)rcl + rchanQueryTable[x].off);
	char *d = parse_getarg(pb);

	if (d == NULL) return 0;

	*c = atol(d);

	p->fWriteLn("OK ALTER OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int alterRchanLong(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	long *c = (long *)((size_t)rcl + rchanQueryTable[x].off);
	char *d = parse_getarg(pb);

	if (d == NULL) return 0;

	*c = atol(d);

	p->fWriteLn("OK ALTER OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int alterRchanUint(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	u_int *c = (u_int *)((size_t)rcl + rchanQueryTable[x].off);
	char *d = parse_getarg(pb);
	int j;

	if (d == NULL) return 0;

	j = atoi(d);

	if (j < 0) j = 0;
	*c = j;

	p->fWriteLn("OK ALTER OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int alterRchanUchar(RegChanList *rcl, IpcConnectType *p, parse_t *pb, int x)
{
	u_char *c = (u_char *)((size_t)rcl + rchanQueryTable[x].off);
	char *d = parse_getarg(pb);
	int X;

	if (d == 0) return 0;
	X = MIN((int)UCHAR_MAX, MAX(0, atoi(d)));
	*c = X;

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

int alterRchanTime(RegChanList* rcl, IpcConnectType *p, parse_t *pb, int x)
{
	time_t *c = (time_t *)((size_t)rcl + rchanQueryTable[x].off);
	char *d = parse_getarg(pb);
	time_t e;

	if (d == 0) return 0;
	e = atol(d);
	*c = e;

	p->fWriteLn("OK QUERY OBJECT RCHAN=%s %sx %ld", rcl->name,
	            rchanQueryTable[x].field, *c);
	return 1;
}

/*$$*/

/**
 * @brief Handle ALTER OBJECT RNICK
 */
int
IpcType::alterRegNickMessage(RegNickList *rnl, const char *req, IpcConnectType *p, parse_t *pb)
{
	char *buf;
	int i = 0;

	if (req == NULL)
		req = "<unknown>";

	if (rnl == NULL)
		return 0;

	if (p->CheckPriv(0, OPRIV_OWNER) && !(p->GetPriv() & PRIV_ALTER_RNICK_GEN)) {
		p->fWriteLn("ERR-NOPRIV ALTER OBJECT RNICK=%s %s "
		            "- User %s Not authorized to alter RNICK %s.",
		            rnl->nick, req, p->user, req);
		return 1;
	}

	for(i = 0; rnickQueryTable[i].field; i++) {
		if (strcmp(req, rnickQueryTable[i].field) == 0) {
			if ((rnickQueryTable[i].a_priv && (p->GetPriv() & rnickQueryTable[i].a_priv) != rnickQueryTable[i].a_priv)
			    || (rnickQueryTable[i].a_priv & PRIV_UNDEF)) 
			{
				p->fWriteLn("ERR-NOPRIV QUERY OBJECT RNICK=%s %s"
				            "- User %s Not authorized to alter RNICK %s.",
				            rnl->nick, p->user, req, req);
				return 1;
			}
			break;
		}
	}

	if (strcmp(req, "PASS") == 0) {
		buf = parse_getarg(pb);

		if (p->CheckPriv(0, OPRIV_SETPASS))
		{
			p->fWriteLn("ERR-NOPERM ALTER OBJECT RNICK=%s PASS - Not authorized.", rnl->nick);
			return 1;
		}

		if (buf == 0 || *buf == '\0' || str_cmp(buf, rnl->nick) == 0)
		{
			p->fWriteLn("ERR-SYNTAX ALTER OBJECT RNICK=%s PASS - You need to specify a valid password", rnl->nick);
			return 1;
		}

		pw_enter_password(buf, rnl->password, NickGetEnc(rnl));
		p->fWriteLn("OK ALTER OBJECT RNICK=%s PASS %s", rnl->nick, buf);
		rnl->flags &= ~NFORCEXFER;
		rnl->chpw_key = 0;
		return 1;
	}

	if (rnickQueryTable[i].field)
		return (* rnickQueryTable[i].a_func)(rnl, p, pb, i);
	return 0;
}


/**
 * @brief Handle QUERY OBJECT RCHAN
 */
int
IpcType::queryRegChanMessage(RegChanList *rcl, const char *req, IpcConnectType *p, parse_t *pb)
{
	int i = 0;

	if (req == NULL)
		req = "<unknown>";
	if (rcl == NULL)
		return 0;

	if (!(p->GetPriv() & PRIV_QUERY_CHAN_PUBLIC)) {
		p->fWriteLn("ERR-NOPRIV QUERY OBJECT RCHAN=%s %s "
				"- User %s Not authorized to query RCHAN %s.",
				rcl->name, req, p->user, req);
		return 1;
	}

	for(i = 0; rchanQueryTable[i].field; i++) {
		if (strcmp(req, rchanQueryTable[i].field) == 0) {
			if (rchanQueryTable[i].priv && (p->GetPriv() & rchanQueryTable[i].priv) != rchanQueryTable[i].priv)
			{
				p->fWriteLn("ERR-NOPRIV QUERY OBJECT RCHAN=%s %s"
				            "- User %s Not authorized to query RCHAN %s.",
					    rcl->name, p->user, req, req);
				return 1;
			}
			break;
		}
	}

	if (rchanQueryTable[i].field) {
		return (* rchanQueryTable[i].func)(rcl, p, pb, i);
	}


	return 0;
}

/**
 * @brief Handle QUERY OBJECT RNICK
 */
int
IpcType::queryRegNickMessage(RegNickList *rnl, const char *req, IpcConnectType *p, parse_t *pb)
{
	int i = 0;

	if (req == NULL)
		req = "<unknown>";

	if (rnl == NULL)
		return 0;

	if (!(p->GetPriv() & PRIV_QUERY_NICK_PUBLIC)) {
		p->fWriteLn("ERR-NOPRIV QUERY OBJECT RNICK=%s %s "
		            "- User %s Not authorized to query RNICK %s.",
		            rnl->nick, req, p->user, req);
		return 1;
	}

	for(i = 0; rnickQueryTable[i].field; i++) {
		if (strcmp(req, rnickQueryTable[i].field) == 0) {
			if (rnickQueryTable[i].priv && (p->GetPriv() & rnickQueryTable[i].priv) != rnickQueryTable[i].priv) 
			{
				p->fWriteLn("ERR-NOPRIV QUERY OBJECT RNICK=%s %s"
				            "- User %s Not authorized to query RNICK %s.",
				            rnl->nick, p->user, req, req);
				return 1;
			}
			break;
		}
	}

	if (rnickQueryTable[i].field)
		return (* rnickQueryTable[i].func)(rnl, p, pb, i);

	if (strcmp(req, "LAST-TIME") == 0) {
		p->fWriteLn("OK QUERY OBJECT RNICK=%s LAST-TIME %ld", rnl->nick,
		            rnl->timestamp);
		return 1;
	}
	else if (strcmp(req, "REG-TIME") == 0) {
		p->fWriteLn("OK QUERY OBJECT RNICK=%s REG-TIME %ld", rnl->nick,
		            rnl->timereg);
		return 1;
	}
	else if (strcmp(req, "ACC") == 0) {
		UserList* ul = getNickData(rnl->nick);
		int acc = 0;

		if (ul) {
			acc = ul->caccess > 1 ? ul->caccess : 1;
		}

		p->fWriteLn("OK QUERY OBJECT RNICK=%s ACC %ld", rnl->nick,
		            acc);
		return 1;
	}
	else if (strcmp(req, "MARK") == 0) {
		if (!(p->GetPriv() & PRIV_QUERY_NICK_PUBLIC)) {
			p->fWriteLn("ERR-NOPRIV QUERY OBJECT RNICK=%s %s "
			            "- User %s Not authorized to query RNICK %s.",
			            rnl->nick, req, p->user, req);
			return 1;
		}
		if (rnl->flags & NMARK)
			p->fWriteLn("OK QUERY OBJECT RNICK=%s MARK YES %s", rnl->nick,
			            rnl->markby ? rnl->markby : "*");
		else
			p->fWriteLn("OK QUERY OBJECT RNICK=%s MARK NO", rnl->nick);
		return 1;
	}
	else if (strcmp(req, "LAST-HOST") == 0) {
		if (rnl->flags & NDBISMASK)
			p->fWriteLn("OK QUERY OBJECT RNICK=%s LAST-HOST %s", rnl->nick,
			            genHostMask(rnl->host));
		else
			p->fWriteLn("OK QUERY OBJECT RNICK=%s LAST-HOST %s", rnl->nick,
			            rnl->host);
		return 1;
	}
	else if (strncmp(req, "ACCESS-", 7) == 0) {
		int n = atoi(req + 7), o;
		nAccessList *acl = NULL;

		if ((isdigit(req[7]) && (n < (int)rnl->amasks+1)) || req[7] == '*')
		{
			acl = LIST_FIRST(&rnl->acl);

			if (strcmp(req, "ACCESS-*") == 0) {
				p->fWriteLn("OK QUERY OBJECT RNICK=%s ACCESS-* START",
					rnl->nick);
				for( ; acl != NULL ; acl = LIST_NEXT(acl, al_lst) )
					p->fWriteLn("DATA RNICK=%s ACCESS-* %s",
						rnl->nick, acl->mask);
				p->fWriteLn("OK QUERY OBJECT RNICK=%s ACCESS-* DONE",
					rnl->nick);
				return 1;
			}

			if (acl != NULL)
			{
				for( o = 0; acl ; acl = LIST_NEXT(acl, al_lst) )
					if (o++ == n)
						break;
			}
		}

		if (acl == NULL)
			p->fWriteLn("ERR-NOENTRY QUERY OBJECT RNICK=%s ACCESS-%d - Invalid access item",
				rnl->nick, n);
		else {

			p->fWriteLn("OK QUERY OBJECT RNICK=%s ACCESS-%d %s",
				rnl->nick, n, acl->mask);
		}
		return 1;
	}
	return 0;
}

/********************************************************************/

/**
 * @brief Make a connection endpoint non-blocking
 * @param File descriptor of endpoint
 * @return 0 on success, -1 on failure
 */
int doNonBlock(int listenDesc)
{
	int optionValue;

#ifdef SYSV
	optionValue = 1;

	if (ioctl(listenDesc, FIONBIO, &optionValue) < 0)
		return -1;
#else
	if ((optionValue = fcntl(listenDesc, F_GETFL, 0)) == -1)
		return -1;
#ifdef O_NONBLOCK
	if (fcntl(listenDesc, F_SETFL, optionValue | O_NONBLOCK) == -1)
#else
	if (fcntl(listenDesc, F_SETFL, optionValue | O_NDELAY) == -1)
#endif
		return -1;
#endif
	return 0;
}

/**
 * @brief Start up an IPC listener
 * @param portNum Number of port to listen on
 * @return -1 on failure, 0 on success
 */
int
IpcType::start(int portNum)
{
	struct sockaddr_in sa;
	int reuseAddr;

	listenDesc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (portNum == 0)
		return -1;

	if (listenDesc < 0) {
		perror("socket");
		return -1;
	}

	bzero(&sa, sizeof(sa));
	sa.sin_port = htons(portNum);
	sa.sin_addr.s_addr = 0x100007f; /*INADDR_ANY;*/
	sa.sin_family = AF_INET;

	reuseAddr = 1;
	setsockopt(listenDesc, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(int));

	doNonBlock(listenDesc);

	if (bind(listenDesc, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		return -1;
	}

	if ( listen(listenDesc, 5) < 0 ) {
		perror("listen");
		return -1;
	}

	topFd = listenDesc;

	return 0;
}

/**
 * @brief Place all descriptors that need to be polled in a fd
 *        select() set.
 * @param fset Set of descriptors to place in the set
 */
void
IpcType::fdFillSet(fd_set & fset)
{
	IpcConnectType *pConnect;

	if (listenDesc == -1)
		return;

	FD_SET(listenDesc, &fset);

	for (pConnect = links; pConnect; pConnect = pConnect->next) {
		if (pConnect->fd != -1) {
			FD_SET(pConnect->fd, &fset);
		}
	}
}


/**
 * @brief Add a logical connection item to the 'links' listing
 * @param p Pointer to connection object to add
 */
void
IpcType::addCon(IpcConnectType *p)
{
	p->next = links;
	links = p;
}

/**
 * @brief Remove a connection item from the 'links' listing
 * @param zap Item to remove
 */
void
IpcType::delCon(IpcConnectType *zap)
{
	IpcConnectType *p, *p_next, *tmp;

	tmp = NULL;

	for(p = links; p; p = p_next)
	{
		p_next = p->next;

		if (zap == p)
		{
			if (tmp)
				tmp->next = p->next;
			else
				links = p->next;
			break;
		}
		else
			tmp = p;
	}

	topFd = listenDesc;

	for(p = links; p; p = p->next)
		if (p->fd > topFd)
			topFd = p->fd;
}

/**
 * @brief Free out a connection object
 */
void
IpcType::freeCon(IpcConnectType *zap)
{
	zap->buf.empty();
	free(zap);

	if (zap->user)
		free(zap->user);
	if (zap->pass)
		free(zap->pass);
	if (zap->objName)
		free(zap->objName);
	if (zap->objPass)
		free(zap->objPass);
}

/**
 * @brief Read packets from a connection and buffer them for
 *        processing
 * @param ptrLink Pointer to the connection endpoint
 * @return -1 if the connection is closing,
 *          0 on no data yet or data read
 */
int
IpcType::ReadPackets(IpcConnectType *ptrLink)
{
	char sockbuf[8192+1], *b;
	int k, kt = 0, msgl;

	/* memset(sockbuf, 0, 8192);*/
	k = read(ptrLink->fd, sockbuf, 8191);

	/*if (k > 0)
	    sockbuf[k] = '\0';
	else sockbuf[0] = '\0';*/

	while(k > 0)
	{
		kt += k;
		b = ptrLink->buf.shove(sockbuf, k, &msgl);

		if (b && *b) {

			strncpyzt(sockbuf, b, msgl);
			/* len = strlen(b); */

			if (msgl >= IPCBUFSIZE || msgl >= 8000)
				return -1;

			k = read(ptrLink->fd, sockbuf + msgl, 8191 - msgl);
			if (k > 0)
				k += msgl;
		}
		else {
			k = read(ptrLink->fd, sockbuf, 8191);
		}
	}

	if (k == 0)
		return -1;
	if (k == -1) {
		if (errno == EINTR || errno == EWOULDBLOCK)
			return 0;
		return -1;
	}
	return 0;
}

/**
 * @brief Handle the results of a select() or poll()
 * @param readme Read descriptor set from select()
 * @param writeme Write descriptor set from select()
 * @param xcept Except descriptor set from select()
 *
 * Given the 3 descriptor sets from the select set,
 * handle the I/O as needed (process incoming data, write out data,
 * etc)
 */
void
IpcType::pollAndHandle(fd_set &readme, fd_set &writeme, fd_set &xcept)
{
	IpcConnectType *p, *p_next;
	char *c;

	if (listenDesc == -1)
		return;

	if (FD_ISSET(listenDesc, &readme))
	{
		struct sockaddr_in sai;
		socklen_t alen;
		int pFd, ipOk;

		if ((pFd = accept(listenDesc, (struct sockaddr *)&sai, &alen)) != -1)
		{
			ipOk = 1;

			//if (sai.sin_addr.s_addr == 0x1000007f)
			//	ipOk = 1;

			if (doNonBlock(pFd) != -1 && ipOk) 
			{
				if (pFd > topFd)
					topFd = pFd;
				p = (IpcConnectType *)oalloc(sizeof(IpcConnectType));
				p->objType = SIPC_UNDEF;
				addCon(p);
				p->fd = pFd;
				p->addr = sai.sin_addr;
			}
			else
				close(pFd);
		}
	}


	for(p = links; p; p = p_next)
	{
		p_next = p->next;

		if (FD_ISSET(p->fd, &xcept)) {
			FD_CLR(p->fd, &xcept);
			close(p->fd);
			delCon(p);
			freeCon(p);
			continue;
		}

		if (FD_ISSET(p->fd, &readme)) {
			char buf[IPCBUFSIZE];

			if (ReadPackets(p) < 0)
			{
				close(p->fd);
				delCon(p);
				freeCon(p);
				continue;
			}

			while (p->buf.pop(buf)) {
				parse_t pb;

				parse_init(&pb, buf);

				c = parse_getarg(&pb);
				if (c == NULL)
					continue;

				if (strcmp(c, "AUTH") == 0) {
					authMessage(p, &pb);
				}
				else if (strcmp(c, "QUIT") == 0) {
					p->sWrite("OK QUIT\n");
					close(p->fd);
					p->fd = -1;
				}
				else if (strcmp(c, "HELP") == 0) {
					p->sWrite("OK HELP\n");
					p->sWrite("REPLY HELP - Commands: \n");
					p->sWrite("REPLY HELP -  AUTH HELP QUERY MAKE QUIT \n");
				}
				else if (p->user == NULL || p->pass == NULL) {
					p->fWriteLn("ERR-NOAUTH %s - You are "
					       "not authenticated. ",
					       "(Allowed: AUTH, HELP, QUIT)", buf);
				}
				else if (strcmp(c, "QUERY") == 0) {
					queryMessage(p, &pb);
				}
				else if (strcmp(c, "ALTER") == 0) {
					alterMessage(p, &pb);
				}
				else if (strcmp(c, "LOG") == 0) {
					logMessage(p, &pb);
				}
				else if (strcmp(c, "MAKE") == 0) {
					makeMessage(p, &pb);
				}
			}

			if (p->fd == -1) {
				delCon(p);
				freeCon(p);
				continue;
			}
		}

		if (FD_ISSET(p->fd, &writeme)) {
			FD_CLR(p->fd, &writeme);

			if (p->s == 0) {
				char bufs[IPCBUFSIZE];

				snprintf(
					bufs, sizeof(bufs),
					"HELO IAM %s\n"
					"AUTH SYSTEM PID %d\n"
					"AUTH SYSTEM LOGIN irc/services\n",
					myname, getpid()
				);
				p->sWrite(bufs);
				p->s = 1;
			}
		}
	}
}

/**
 * @brief Handle an authentication message
 */
void
IpcType::authMessage(IpcConnectType *p, parse_t *pb)
{
	char *buf = parse_getarg(pb);

	if (buf == NULL)
		return;

	if (!strcmp(buf, "SYSTEM")) {
		authSysMessage(p, pb);
	}
	else if (!strcmp(buf, "OBJECT")) {
		authObjMessage(p, pb);
	}
	else {
		p->sWrite("ERR-BADTYPE AUTH - No such auth type\n");
	}
}

/**
 * @brief Handle a query message
 */
void
IpcType::queryMessage(IpcConnectType *p, parse_t *pb)
{
	char *buf = parse_getarg(pb);

	if (buf == NULL)
		return;

	if (p->user == NULL) {
		p->sWrite("ERR-AUTH QUERY - Must authenticate to system\n");
		return;
	}

	if (!strcmp(buf, "SYSTEM")) {
		querySysMessage(p, pb);
	}
	else if (!strcmp(buf, "OBJECT")) {
		queryObjMessage(p, pb);
	}
	else {
		p->sWrite("ERR-BADTYPE QUERY - No such query type\n");
	}
}


/**
 * @brief Handle a log message
 */
void
IpcType::logMessage(IpcConnectType *p, parse_t *pb)
{
	char *service = parse_getarg(pb);
	int use_globops=0;
	interp::commands::services_cmd_id logt;
	SLogfile* logf;

	/* Service */
	if (service == NULL) {
		p->sWrite("ERR-AUTH ALTER - Must at least specify SERVICE - MESSAGE\n");
		return;
        }

	if (p->CheckPriv(PRIV_LOGW, 0)) {
		p->fWriteLn("LOG %s "
		            "- User %s does not have log writing access.",
		            service, p->user);
		return;
	}

	char* buf;

	while ( 1 ) {
		buf = parse_getarg(pb);

		if (buf == NULL) {
			p->sWrite("ERR-AUTH ALTER - Must specify a service, hyphen, message\n");
			return;
	        }
		if (*buf == '-') {
			buf = parse_getallargs(pb);
			break;
		}
		if (!strcasecmp(buf, "GLOBOPS"))
			use_globops=1;
	}

	if (buf == NULL) {
		p->sWrite("ERR-AUTH ALTER - Must specify a service, hyphen, message\n");
		return;
        }

	if (!strcasecmp(service, "ChanServ")) {
		logt = CSE_IPC;
		logf = chanlog;
	}
	else if (!strcasecmp(service, "NickServ")) {
		logt = NSE_IPC;
		logf = nicklog;
	}
	else {
		service = "OperServ";
		logt = OSE_IPC;
		logf = operlog;
	}

	sSend(":%s GLOBOPS :(IPC:%s) %s", service, p->user, buf);
	logf->log(NULL, logt, NULL, 0, "(IPC:%s,%s) %s", p->user,service, buf);
}

/**
 * @brief Handle an alter message
 */
void
IpcType::alterMessage(IpcConnectType *p, parse_t *pb)
{
	char *buf = parse_getarg(pb);

	if (buf == NULL) {
		p->sWrite("ERR-AUTH ALTER - Must specify target and type\n");
		return;
        }

	if (p->user == NULL) {
		p->sWrite("ERR-AUTH ALTER - Must authenticate to system\n");
		return;
	}

	/*if (!strcmp(buf, "SYSTEM")) {
		alterSysMessage(p, pb);
	}
	else*/ if (!strcmp(buf, "OBJECT")) {
		alterObjMessage(p, pb);
	}
	else {
		p->sWrite("ERR-BADTYPE QUERY - No such query type\n");
	}
}

/**
 * @brief Handle AUTH SYSTEM
 */
void
IpcType::authSysMessage(IpcConnectType *p, parse_t *pb)
{
	char *str_dup(const char *);
	char *buf = parse_getarg(pb);
	const ssUInfo *q;
	char authBuf[IRCBUF];
	int valid = 0;

	if (buf == NULL)
		return;

	if (!strcmp(buf, "LOGIN")) {

		buf = parse_getarg(pb);

		if (buf == NULL) {
			p->sWrite("ERR-BADLOGIN AUTH SYSTEM LOGIN - Invalid login\n");
			return;
		}

		if ((q = getServicesSysUser(buf))) {
			valid = 1;
			if (p->user)
				free(p->user);
			p->user = str_dup(buf);
			p->gpriv = 0;
			p->opriv = 0;
		}
		/*if (valid == 0) {
			p->sWrite("ERR-BADLOGIN AUTH SYSTEM LOGIN - Invalid login\n");
			return;
		}*/

		p->cookie = lrand48();

		p->fWriteLn("OK AUTH SYSTEM LOGIN");
		p->fWriteLn("AUTH COOKIE %X%X", p->fd, p->cookie);
		return;
	}
	else if (!strcmp(buf, "PASS")) {
		unsigned char digest[16], *pBuf;
		MD5Context ctx;

		buf = parse_getarg(pb);

		if (buf == NULL || (p->user) == NULL || (q = getServicesSysUser(p->user)) == NULL) {
			p->sWrite("ERR-BADLOGIN AUTH SYSTEM LOGIN - Invalid login\n");
			if (p->user)
				SetDynBuffer(&p->user, NULL);
			return;
		}

		snprintf(authBuf, sizeof(authBuf), "%X%X:%s", p->fd, (u_int)p->cookie, q->password);

		MD5Init(&ctx);
		MD5Update(&ctx, (const u_char *)authBuf, strlen(authBuf));
		MD5Final(digest, &ctx);

		pBuf = (u_char *)str_dup(md5_printable(digest));

		if (pBuf == NULL || p == NULL) {
			p->sWrite("ERR-BADLOGIN AUTH SYSTEM LOGIN - Internal Failure\n");
			if (p->user)
				SetDynBuffer(&p->user, NULL);
			return;
		}

		if (strcmp((char *)pBuf, buf)) {
			p->sWrite("ERR-BADLOGIN AUTH SYSTEM LOGIN - Invalid login\n");
			if (p->user)
				SetDynBuffer(&p->user, NULL);
			return;
		}

		p->fWriteLn("OK AUTH SYSTEM PASS");
		p->fWriteLn("YOU ARE %s", p->user);
		p->pass = (char *)pBuf;
		p->gpriv = q->priv;

		if ((p->gpriv & PRIV_NOWNER_EQUIV))
			p->opriv |= OPRIV_OWNER;
		else
			p->opriv = 0;

		return;
	}

	p->fWriteLn("ERR-BADTYPE AUT SYSTEMH");
}

/**
 * @brief Handle AUTH OBJECT
 */
void
IpcType::authObjMessage(IpcConnectType *p, parse_t *pb)
{
	char *str_dup(const char *);
	char *buf = parse_getarg(pb), *buf1, *buf2, *pawd = 0, *objStr = 0;
	RegNickList *rnl;
	RegChanList *rcl;
	
	char authBuf[IRCBUF];
	int valid = 0;

	if (p->objType == SIPC_RNICK)
		objStr = "RNICK";
	else if (p->objType == SIPC_RCHAN)
		objStr = "RCHAN";
	else	objStr = "<Undef>";

	if (buf == NULL)
		return;

	if (!strcmp(buf, "LOGIN"))
	{

		buf1 = parse_getarg(pb);
		buf2 = parse_getarg(pb);

		objStr = buf1;

		if (buf1 == NULL || buf2 == NULL) {
			p->fWriteLn("ERR-BADLOGIN AUTH OBJECT LOGIN%s%s - Invalid login", objStr ? " " : "", objStr ? objStr : "");
			return;
		}

		if (!str_cmp(buf1, "RNICK") && (rnl = getRegNickData(buf2)))
		{
			p->opriv = 0;
			valid = 1;

			if (p->CheckPriv(PRIV_RNICK_LOGIN, 0)) 
				valid = 0;

			if (p->objName)
				free(p->objName);
			if (valid) {
				p->objName = str_dup(buf2);
				p->objType = SIPC_RNICK;
			}
		}
		else if (!str_cmp(buf1, "RCHAN") && (rcl = getRegChanData(buf2)))
		{
			p->opriv = 0;
			valid = 1;

			if (p->CheckPriv(PRIV_RCHAN_LOGIN, 0)) 
				valid = 0;

			if (p->objName)
				free(p->objName);
			if (valid) {
				p->objName = str_dup(buf2);
				p->objType = SIPC_RCHAN;
			}
		}


		p->cookie = lrand48();
		p->fWriteLn("OK AUTH OBJECT %s LOGIN", objStr);
		p->fWriteLn("AUTH COOKIE %X%X", p->fd, p->cookie);
		return;
	}
	else if (!strcmp(buf, "CHPASSKEY"))
	{
		buf1 = parse_getarg(pb);

		if (buf1 && p->objType == SIPC_RNICK && p->objName)
		{
			rnl = getRegNickData(p->objName);

			if (rnl && rnl->chpw_key)
			{
				const char *rnE = rnl->email;
				const char *rnP = PrintPass(rnl->password, NickGetEnc(rnl));
				const char *pwAuthChKey;

				if (rnl->flags & NFORCEXFER)
					rnE = "-forced-transfer-";
				pwAuthChKey = GetAuthChKey(rnE, rnP,
						rnl->timereg, rnl->chpw_key);
				if (strcmp(pwAuthChKey, buf1) == 0) {
					p->objPass = str_dup("");
					p->opriv = OPRIV_SETPASS;
					p->fWriteLn("OK AUTH OBJECT SETPASS");
					return;
				}
			}
		}

		if (buf1 && p->objType == SIPC_RCHAN && p->objName)
		{
			rcl = getRegChanData(p->objName);

			if (rcl && rcl->chpw_key && (rnl = rcl->founderId.getNickItem()))
			{
				const char *rcE = rnl->email;
				const char *rcP = PrintPass(rcl->password, ChanGetEnc(rcl));
				const char *pwAuthChKey;

				if (rcl->flags & CFORCEXFER)
					rcE = "-forced-transfer-";
				pwAuthChKey = GetAuthChKey(rcE, rcP,
						rcl->timereg, rcl->chpw_key);
				if (strcmp(pwAuthChKey, buf1) == 0) {
					p->objPass = str_dup("");
					p->opriv = OPRIV_SETPASS;
					p->fWriteLn("OK AUTH OBJECT SETPASS");
					return;
				}
			}
		}

		p->fWriteLn("ERR-BADLOGIN AUTH OBJECT %s LOGIN - Internal Failure",
		            objStr);
		if (p->objName)
			SetDynBuffer(&p->objName, NULL);
	}
	else if (!strcmp(buf, "PASS"))
	{
		unsigned char digest[16], *pBuf;
		MD5Context ctx;
		int valid = 0;

		buf = parse_getarg(pb);

		if (p->objType == SIPC_RNICK)
			objStr = "RNICK";
		else if (p->objType == SIPC_RCHAN)
			objStr = "RCHAN";
		else objStr = "<Undef>";

		if (objStr == 0) {
			p->fWriteLn("ERR-BADLOGIN AUTH OBJECT %s LOGIN - Invalid login", objStr);
			if (p->objName)
				SetDynBuffer(&p->objName, NULL);
			return;
		}

		if (buf && p->objType == SIPC_RNICK && p->objName &&
		    (rnl = getRegNickData(p->objName)))
		{
			valid = 1;
			if (rnl->flags & NENCRYPT)
				pawd = md5_printable(rnl->password);
			else
				pawd = md5_printable(md5_password(rnl->password));
		}
		else if (buf && p->objType == SIPC_RCHAN && p->objName &&
		    (rcl = getRegChanData(p->objName)))
		{
			valid = 1;
			if (rcl->flags & CENCRYPT)
				pawd = md5_printable(rcl->password);
			else
				pawd = md5_printable(md5_password(rcl->password));
		}
		else {
			p->fWriteLn("ERR-BADLOGIN AUTH OBJECT %s LOGIN - Invalid login", objStr);
			if (p->objName)
				SetDynBuffer(&p->objName, NULL);
			return;
		}

		snprintf(authBuf, sizeof(authBuf), "%X%X:%s", p->fd, (u_int)p->cookie, pawd);

		MD5Init(&ctx);
		MD5Update(&ctx, (const u_char *)authBuf, strlen(authBuf));
		MD5Final(digest, &ctx);

		pBuf = (u_char *)str_dup(md5_printable(digest));

		if (pBuf == NULL || p == NULL) {
			p->fWriteLn("ERR-BADLOGIN AUTH OBJECT %s LOGIN - Internal Failure",
			            objStr);
			if (p->objName)
				SetDynBuffer(&p->objName, NULL);
			return;
		}

		if (strcmp((char *)pBuf, buf)) {
			p->fWriteLn("ERR-BADLOGIN AUTH OBJECT %s LOGIN - Invalid login", objStr);
			if (p->objName)
				SetDynBuffer(&p->objName, NULL);
			return;
		}

		p->fWriteLn("OK AUTH OBJECT %s PASS", objStr);
		/* p->fWriteLn("YOU ARE %s", ); */
		p->objPass = (char *)pBuf;
		p->opriv = OPRIV_OWNER;
		return;
	}

	p->fWriteLn("ERR-BADTYPE AUTH OBJET");
}


/**
 * @brief Is nick ``qlined''?
 */
int isQlined(const char *nick)
{
	int i;

	const char *forbiddenPatterns[] =
	{
		"DALnet",		"SorceryNet",		"Status",
		"AUX",			"PRN",			"LPT?",
		"COM?",			"K9",			"W",
		"X",			"NoteOp",		"OperOp",
		"ChanOp",		"NickOp",		"*Serv",
		"HelpServ",		"*He*p*S*rv*",		"ChanServ",
		"*Ch*n*S*rv*",		"NickServ",		"*Ni*k*S*rv*",
		"MemoServ",		"*Me*o*S*rv*",		"OperServ",
		"*Op*r*S*rv*",		"Warlock",		"ServOp",
		"Sorcery*",		"IRCop",		"*IRC*op*",
		"kline",		"Services",		"admin*",
		"ms",			"ns",			"cs",
		"os",
		"ServBot",		"ChannelBot",		"*net*admin*",
		"Auth-*",
		NULL
	};

	for(i = 0; forbiddenPatterns[i]; i++)
		if (match(forbiddenPatterns[i], nick) == 0)
			return 1;
	return 0;
}

/**
 * @brief Handle MAKE OBJECT
 */
void
IpcType::makeMessage(IpcConnectType *p, parse_t *pb)
{
	char pwStuff[PASSLEN + 1];
	char *buf = parse_getarg(pb);

	if (!p->user) {
		p->sWrite("ERR-NOTAUTH MAKE - You must be authenticated to make something.\n");
		return;
	}

	if (buf != NULL)
	{
		if (strcmp(buf, "RCHAN") == 0) {
			p->sWrite("ERR-UNIMPLEMENTED MAKE RCHAN - Not yet implemented\n");
		}
		else if (strcmp(buf, "RNICK") == 0 || strcmp(buf, "FORCE-RNICK") == 0) {
			RegNickList *ptrNick;
			char *nick = parse_getarg(pb);
			char *email = parse_getarg(pb);
			char *host = parse_getarg(pb);
			char *ptrChar;
			int is_force = (strcmp(buf, "FORCE-RNICK") == 0);
			int pws;

			if (nick == NULL) {
				p->fWrite("ERR-NONICK MAKE NICK=%s - Nickname missing\n", nick);
				return;
			}

			if (email == NULL) {
				p->fWrite("ERR-NOEMAIL MAKE NICK=%s - Email missing\n", nick);
				return;
			}

			if (host == NULL) {
				p->fWrite("ERR-NOEMAIL MAKE NICK=%s - Host address missing\n", nick);
				return;
			}

			if (is_force == 0 && getNickData(nick) != NULL) {
				p->fWrite("ERR-NICKINUSE MAKE NICK=%s - Nickname is in use.\n", nick);
				return;
			}

			if (*nick == '\0' || strlen(nick) > NICKLEN || *nick == '-'
                            || isdigit(*nick)) {
				p->fWrite("ERR-NICKINVALID MAKE NICK=%s - Nickname not valid\n", nick);
				return;
			}

			for(ptrChar = nick; *ptrChar; ptrChar++)
			{
				if (isalnum(*ptrChar))
					continue;

				if (*ptrChar == '[' || *ptrChar == ']' ||
				    *ptrChar == '{' || *ptrChar == '}' ||
				    *ptrChar == '|' || *ptrChar == '_' ||
				    *ptrChar == '\\' || *ptrChar == '`' ||
				    *ptrChar == '^')
					continue;
				p->fWrite("ERR-NICKINVALID MAKE RNICK=%s - Nickname not valid\n", nick);
				return;				
			}

			if (isQlined(nick)) {
				p->fWrite("ERR-NICKQLINE MAKE RNICK=%s - Nickname reserved.\n", nick);
				return;
			}

			if (getRegNickData(nick)) {
				p->fWrite("ERR-NICKINUSE MAKE RNICK=%s - Nickname is in use.\n", nick);
				return;
			}


			ptrNick = (RegNickList *)oalloc(sizeof(RegNickList));
			ptrNick->regnum.SetNext(top_regnick_idnum);
			strcpy(ptrNick->nick, nick);
			strcpy(ptrNick->user, "www-temp");
			SetDynBuffer(&ptrNick->host, host);
			strncpyzt(ptrNick->email, email, EMAILLEN);
			ptrNick->timereg = ptrNick->timestamp = CTime;
			ptrNick->amasks = 0;
			ptrNick->url = NULL;
			ptrNick->chans = 0;
			ptrNick->flags = (NDBISMASK);
			addRegNick(ptrNick);

			pws = lrand48();
			snprintf(pwStuff, 16, "%X%X", (u_int)(pws ^ p->cookie), (u_int)(pws ^ startup));
			pw_enter_password(pwStuff, ptrNick->password, NickGetEnc(ptrNick));

//			logDump(nicklog, "%s![*ipc*:%s]@%s: REGISTER", ptrNick->nick, p->user, host);
			nicklog->log(NULL, NS_REGISTER, ptrNick->nick, 0, "%s![*ipc*:%s]@%s", ptrNick->nick, p->user, host);
			p->fWriteLn("OK MAKE RNICK=%s PASS=%s", nick, pwStuff);
			return;
		}
	}
	p->sWrite("ERR-BADTYPE MAKE - Unsupported make type (must be RNICK or RCHAN)\n");
}

/**
 * @brief Handle QUERY SYSTEM
 */
void
IpcType::querySysMessage(IpcConnectType *p, parse_t *pb)
{
	char *buf = parse_getarg(pb);

	if (buf != NULL)
	{
		if (strcmp(buf, "UPTIME") == 0) {
			p->fWriteLn("OK QUERY SYSTEM UPTIME=%ld", time(0) - startup);
			return;
		}
	}
	p->sWrite("ERR-BADTYPE QUERY SYSTEM - No such object query type (UPTIME) \n");
}
////////////////////////////

/**
 * @brief Handle QUERY OBJECT
 */
void
IpcType::queryObjMessage(IpcConnectType *p, parse_t *pb)
{
	char *buf = parse_getarg(pb);
	int is_ignore = 0, is_ahurt = 0, is_akill = 0;

	if (buf != NULL)
	{
		if (strcmp(buf, "AKILL") == 0)
			is_akill = 1;
		else if (strcmp(buf, "AHURT") == 0)
			is_ahurt = 1;
		else if (strcmp(buf, "IGNORE") == 0)
			is_ignore = 1;

		if (strcmp(buf, "RNICK") == 0)
		{
			char *req;
			RegNickList *rnl;

			if ((buf = parse_getarg(pb)) == NULL) {
				p->sWrite("ERR-SYNTAX QUERY OBJECT RNICK - nickname/attribute pair missing.\n");
				return;
			}

			req = parse_getarg(pb);

			if ((rnl = getRegNickData(buf)) == NULL) {

				if (req && strcmp(req, "ISREG"))
					p->fWriteLn("ERR-BADTARGET QUERY OBJECT RNICK=%s - NOT_REGISTERED", buf);
				else
					p->fWriteLn("OK QUERY OBJECT RNICK=%s ISREG=FALSE - Nickname is not registered.", buf);
				return;
			}

			if (req == NULL || strcmp(req, "ISREG") == 0) {
				p->fWriteLn("OK QUERY OBJECT RNICK=%s ISREG=TRUE - Nickname is registered.", buf);
				return;
			}

			if (strcmp(req, "ISBYPASS") == 0) {
				if (rnl->flags & NBYPASS)
					p->fWriteLn("OK QUERY OBJECT RNICK=%s ISBYPASS=TRUE", buf);
				else
					p->fWriteLn("OK QUERY OBJECT RNICK=%s ISBYPASS=FALSE", buf);
			}


			if (queryRegNickMessage(rnl, req, p, pb))
				return;

			p->fWriteLn("ERR-BADATT QUERY OBJECT RNICK EATTR - Known attributes (ISREG, ISBYPASS)");

			return;
		}
		else if (is_akill || is_ahurt || is_ignore) {
			const char *tar = parse_getarg(pb), *akstr;

			akstr = is_ignore ? "IGNORE" :
			        is_ahurt ?  "AHURT"  :
			        "AKILL";

			if (!(p->GetPriv() & PRIV_QUERY_AKILL_LIST)) {
				p->fWriteLn("ERR-NOPRIV QUERY OBJECT %s=LIST "
				            "- User %s Not authorized to query %s.",
				            akstr, p->user, akstr);
				return;
			}

			if (tar && strcmp(tar, "LIST") == 0)
				tar = NULL;


			{
				p->fWriteLn("START QUERY OBJECT %s=LIST",
				            akstr);
				p->sendAkills(is_ignore ? A_IGNORE : is_ahurt ? A_AHURT : A_AKILL, tar);
				p->fWriteLn("OK QUERY OBJECT %s=LIST",
				            akstr);
			}			
			return;
		}
		else if (strcmp(buf, "RCHAN") == 0)
		{
			char* req;
			RegChanList* reg_chan;

			if ((buf = parse_getarg(pb)) == NULL) {
				p->sWrite("ERR-BADSYNTAX QUERY OBJECT RCHAN - channel/attribute pair missing.\n");
				return;
			}

			req = parse_getarg(pb);

			if ((reg_chan = getRegChanData(buf)) == NULL) {
				if (req && strcmp(req, "ISREG"))
					p->fWriteLn("ERR-BADTARGET QUERY OBJECT RCHAN=%s - NOT_REGISTERED", buf);
				else
					p->fWriteLn("OK QUERY OBJECT RCHAN=%s ISREG=FALSE - Channel is not registered.", buf);
				return;
			}

			if (req == NULL || strcmp(req, "ISREG") == 0) {
				p->fWriteLn("OK QUERY OBJECT RNICK=%s ISREG=TRUE - Channel is registered.", buf);
				return;
                        }


			if (queryRegChanMessage(reg_chan, req, p, pb))
				return;			

			p->fWriteLn("ERR-BADATT QUERY OBJECT RCHAN EATTR - Unknown attribute for that channel");
			return;
		}
	}
	
	p->sWrite("ERR-BADTYPE QUERY OBJECT - No such object query type (RCHAN, RNICK, AKILL, AHURT, IGNORE) \n");
}


/**
 * @brief Handle ALTER OBJECT
 */
void
IpcType::alterObjMessage(IpcConnectType *p, parse_t *pb)
{
	char *buf = parse_getarg(pb);
//	int is_ignore = 0, is_ahurt = 0, is_akill = 0;

	if (buf != NULL)
	{
//		if (strcmp(buf, "AKILL") == 0)
//			is_akill = 1;
//		else if (strcmp(buf, "AHURT") == 0)
//			is_ahurt = 1;
//		else if (strcmp(buf, "IGNORE") == 0)
//			is_ignore = 1;

		if (strcmp(buf, "RNICK") == 0)
		{
			char *req;
			RegNickList *rnl;

			if ((buf = parse_getarg(pb)) == NULL) {
				p->sWrite("ERR-SYNTAX QUERY OBJECT RNICK - nickname/attribute pair missing.\n");
				return;
			}

			req = parse_getarg(pb);

			if ((rnl = getRegNickData(buf)) == NULL) {
				p->fWriteLn("ERR-BADTARGET ALTER OBJECT RNICK=%s - NOT_REGISTERED", buf);
				return;
			}

//			if (req == NULL || strcmp(req, "DROP") == 0) {
//				p->fWriteLn("OK QUERY OBJECT RNICK=%s ISREG=TRUE - Nickname is registered.", buf);
//				return;
//			}

			if (strcmp(req, "+BYPASS") == 0) {
				if (p->CheckPriv(PRIV_SET_BYPASS, 0)) {
					p->fWriteLn("ERR-NOPRIV ALTER OBJECT RNICK=%s %s"
 					"- Not authorized to alter RNICK %s.",
					rnl->nick, req, req);
					return;
				}
				p->fWriteLn("OK ALTER OBJECT RNICK=%s ISBYPASS=TRUE", buf);
				rnl->flags |= NBYPASS;
			}

			if (strcmp(req, "-BYPASS") == 0) {
				if (p->CheckPriv(PRIV_UNSET_BYPASS, 0)) {
					p->fWriteLn("ERR-NOPRIV ALTER OBJECT RNICK=%s %s"
 					"- Not authorized to alter RNICK %s.",
					rnl->nick, req, req);
					return;
				}
				p->fWriteLn("OK ALTER OBJECT RNICK=%s ISBYPASS=FALSE", buf);
				rnl->flags &= ~NBYPASS;
			}


			if (alterRegNickMessage(rnl, req, p, pb))
				return;

			p->fWriteLn("ERR-BADATT ALTER OBJECT RNICK EATTR - Known attributes (ISREG, ISBYPASS)");

			return;
		}
		else if (strcmp(buf, "RCHAN") == 0)
		{
			RegChanList *rcl;

			if ((buf = parse_getarg(pb)) == NULL) {
				p->sWrite("ERR-BADSYNTAX ALTER OBJECT RCHAN - channel/attribute pair missing.\n");
				return;
			}

			rcl = getRegChanData(buf);

			if (rcl == NULL) {
				p->sWrite("ERR-NOTARGET ALTER OBJECT RCHAN\n");
				return;
			}

			if ((buf = parse_getarg(pb)) == NULL) {
				p->sWrite("ERR-BADSYNTAX ALTER OBJECT RCHAN - channel/attribute pair missing.\n");
				return;
			}

			if (strcmp(buf, "PASS") == 0)
			{
				if ((buf = parse_getarg(pb)) == NULL) {
					p->sWrite("ERR-BADSYNTAX ALTER OBJECT RCHAN - attribute value missing.\n");
					return;
				}

				if (p->CheckPriv(0, OPRIV_SETPASS))
				{
					p->fWriteLn("ERR-NOPERM ALTER OBJECT RCHAN=%s PASS - Not authorized.", rcl->name);
					return;
				}
	
				if (buf == 0 || *buf == '\0' || str_cmp(buf, rcl->name) == 0)
				{
					p->fWriteLn("ERR-SYNTAX ALTER OBJECT RCHAN=%s PASS - You need to specify a valid password", rcl->name);
					return;
				}

				pw_enter_password(buf, rcl->password, ChanGetEnc(rcl));
				rcl->flags &= ~CFORCEXFER;
				rcl->chpw_key = 0;
				p->fWriteLn("OK ALTER OBJECT RCHAN=%s PASS %s", rcl->name, buf);
				return;
			}

//
			return;
		}
	}
	
	p->sWrite("ERR-BADTYPE QUERY OBJECT - No such object alter type (RNICK) \n");
}

/**
 * @brief Write a message to a connection
 */
void
IpcConnectType::sWrite(const char *buf)
{
	int len, retcode = send(fd, buf, len=strlen(buf), 0);

	if (retcode == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK &&
		    errno != EMSGSIZE) {
			if (fd != -1)
				close(fd);
			fd = -1;
		}
		return;
	}
	else if (retcode > 0 && retcode < len) {
		sWrite(buf + retcode);
	}
}

void
IpcConnectType::fWrite(const char *buf, ...)
{
	char stuff[IRCBUF];
	va_list ap;

	va_start(ap, buf);
	vsnprintf(stuff, sizeof(stuff), buf, ap);
	va_end(ap);

	sWrite(stuff);
}

void
IpcConnectType::fWriteLn(const char *buf, ...)
{
	char stuff[IRCBUF];
	va_list ap;

	va_start(ap, buf);
	vsnprintf(stuff, sizeof(stuff), buf, ap);
	va_end(ap);

	send(fd, stuff, strlen(stuff), 0);
	send(fd, "\n", 1, 0);
}

