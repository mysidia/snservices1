/** @file operserv.h
 * \wd \taz \greg
 * \date 1996-1997
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
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

#ifndef __OPERSERV_H
#define __OPERSERV_H

#include "nickserv.h"

/// An akill/services ignore/autohurt list item
struct akill
{
	struct akill *next, *prev;

	int id;							/*!< Identity stamp */
	char *nick;						/*!< Nickname pattern */
	char *user;						/*!< Username pattern */
	char *host;						/*!< Hostname pattern */
	time_t set;						/*!< Set when? (UTC value) */
	time_t unset;					/*!< Expires at? (UTC value) */
	char setby[NICKLEN];			/*!< What nickname created this item? */
	char reason[AKREASON_LEN + 1];	/*!< Reason autokill was set */
	char type;						/*!< Which list is this item in? */
	int tid;						/*!< Id# of an associated timer */
};

/**
 * ===DOC=== 
 * Straightforward I hope...simple AKill structure
 * nick!user@host + reason (for akill)
 */
/* typedef struct akill_struct AKill; */

/*struct akill_struct {
  char            nick[NICKLEN];	
  char            user[USERLEN];	
  char            host[HOSTLEN];	
  char            reason[50];		// Brief reason /
  int             timer;
  int             type;
  AKill          *next;
};*/

/*
#define OAKILL   0x0001
#define OIGNORE  0x0002
#define OTRIGGER 0x0004
*/
void            sendToOperServ(UserList *, char **, int);
void            addTempHost(long, char *, char *, int);
///void            loadAKills(void);
///void            saveAKills(void);
void		readConf(void);
char* applyAkill(char* nick, char* user, char* host, struct akill* ak);

#endif /* __OPERSERV_H */
