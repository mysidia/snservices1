/**
 * \file hash.c
 * \brief Services hash tables
 *
 * Procedures related to getting services hash tables indices
 * and manipulating/searching such tables.
 *
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

/*
 * hash.c, simple hash table dealings.... 
 */
#include "services.h"
#include "queue.h"
#include "nickserv.h"
#include "hash.h"
#include "chanserv.h"

UserHashEnt     UserHash[NICKHASHSIZE];		///< Hash of online users
RegNickHashEnt  RegNickHash[NICKHASHSIZE];	///< Hash of registered nicks
RegNickIdHashEnt RegNickIdHash[IDHASHSIZE];	///< Hash of regnick id nums
ChanHashEnt     ChanHash[CHANHASHSIZE];		///< Hash of channels
RegChanHashEnt  RegChanHash[CHANHASHSIZE];	///< Hash of registered chans
CloneHashEnt    CloneHash[CLONEHASHSIZE];	///< Hash of clone hosts
ChanTrigHashEnt ChanTrigHash[CHANTRIGHASHSIZE]; ///< Hash of channel trigger data


/**
 * \brief Take a string and hash it.  The hash is a 16-bit unsigned value, for
 * now.
 * \param hname Name to get the hash key of
 */
u_int16_t
getHashKey(const char *hname)
{
	const u_char    *name;
	u_int16_t  hash;
	
	hash = 0x5555;
	name = (const u_char *)hname;
	
	if (!hname) {
		dlogEntry("getHashKey: hashing NULL, returning 0");

		return 0;
	}
	
	for ( ; *name ; name++) {
		hash = (hash << 3) | (hash >> 13);
		hash ^= tolower(*name);
	}

	return (hash);
}
