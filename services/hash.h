/* $Id$ */

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

/**
 * \file hash.h
 * Hashtable-related headers
 */

#ifndef __HASH_H
#define __HASH_H
#include "chanserv.h"
#include "nickserv.h"
#include "clone.h"
#include "queue.h"

/*
 * in this I use hash tables for nicks and channels, speed up searching
 * and the like...
 */

/*
 * nickname hash entry 
 */
LIST_HEAD(_nickhashent, _regnicklist);

/**
 * Hash type for registered nicknames
 */
typedef struct _nickhashent RegNickHashEnt;

/*
 * nickname id number hash
 */
LIST_HEAD(RegNickIdHashEnt, RegNickIdMap);

/*
 * Channel trigger hash
 */
LIST_HEAD(ChanTrigHashEnt, _ChanTrigInfo);

/*
 * user hash entry
 */
LIST_HEAD(_userhashent, _userlist);

/**
 * Hash type for online users
 */
typedef struct _userhashent UserHashEnt;

/**
 * Channel hash entry 
 */
typedef struct chanhashent {
  ChanList       *chan;
  ChanList       *lastchan;
} ChanHashEnt;

/**
 * Registered channel hash entry
 */
typedef struct regchanhashent {
  RegChanList    *chan;
  RegChanList    *lastchan;
} RegChanHashEnt;

/**
 * Clone hash entry
 */
typedef struct clonehashent {
  HostClone      *clone;
  HostClone      *lastclone;
} CloneHashEnt;

u_int16_t       getHashKey(const char *);

extern UserHashEnt     UserHash[NICKHASHSIZE];
extern RegNickHashEnt  RegNickHash[NICKHASHSIZE];
extern RegNickIdHashEnt RegNickIdHash[IDHASHSIZE];
extern ChanHashEnt     ChanHash[CHANHASHSIZE];
extern RegChanHashEnt  RegChanHash[CHANHASHSIZE];
extern CloneHashEnt    CloneHash[CLONEHASHSIZE];
extern ChanTrigHashEnt ChanTrigHash[CHANTRIGHASHSIZE];

#endif /* __HASH_H */
