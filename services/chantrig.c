/**
 * \file chantrig.c
 * \brief Triggers by channel name
 *
 * \mysid
 * \date 2002
 *
 * $Id$
 */
/*
 * Copyright (c) 2002 James Hess
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
#include "chanserv.h"
#include "nickserv.h"
#include "macro.h"
#include "queue.h"
#include "hash.h"
#include "db.h"
#include "chantrig.h"

/**
 * @brief Find a trigger associated with channel X, if there is one
 * @parm name Name of the channel
 * @return Null pointer if no trigger found, else pointer to found item
 */
ChanTrigger* FindChannelTrigger(const char* name)
{
	ChanTrigger* s;
	long hashEnt = getHashKey(name)%CHANTRIGHASHSIZE;

	if (!LIST_FIRST(&ChanTrigHash[hashEnt]))
		return 0;

	for(s = LIST_FIRST(&ChanTrigHash[hashEnt]); s != 0;
            s = LIST_NEXT(s, cn_lst))
		if (!strcasecmp(name, s->chan_name))
			return s;
	return 0;
}

/**
 * @brief Delete trigger ct
 * @parm ct Pointer to trigger to delete (null is ok)
 * @note Does not free anything
 */
void DelChannelTrigger(ChanTrigger* ct)
{
	if (ct) {
		LIST_REMOVE(ct, cn_lst);
	}
}

/**
 * @brief Add a new channel trigger to the list
 */
void AddChannelTrigger(ChanTrigger* ct)
{
	long hashEnt = getHashKey(ct->chan_name)%CHANTRIGHASHSIZE;

	if (ct) {
		LIST_INSERT_HEAD(&ChanTrigHash[hashEnt], ct, cn_lst);
	}
}

/**
 * @brief Free a ChanTrigger object.
 */
void FreeChannelTrigger(ChanTrigger* ct)
{
	if (ct) {
		if (ct->chan_name)
			FREE(ct->chan_name);
		FREE(ct);
	}
}

/**
 * @brief Create a new channel trigger object for target name
 */
ChanTrigger* MakeChannelTrigger(const char* cn)
{
	ChanTrigger* t_new = (ChanTrigger *) oalloc(sizeof(ChanTrigger));

	t_new->chan_name = str_dup(cn);
	t_new->op_trigger = t_new->ak_trigger = 0;
	t_new->impose_modes = 0;
	t_new->flags = 0;

	return t_new;
}

/**
 * @brief Maximum number ops for this channel
 */
unsigned int ChanMaxAkicks(RegChanList* cn)
{
	ChanTrigger* trig;
	int lim = AkickLimit;

	trig = FindChannelTrigger(cn->name);

	if (trig && trig->ak_trigger != 0)
		lim = trig->ak_trigger;
	return lim;
}

/**
 * Maximum number akicks for this channel
 */
unsigned int ChanMaxOps(RegChanList* cn)
{
	ChanTrigger* trig;
	int lim = OpLimit;

	trig = FindChannelTrigger(cn->name);

	if (trig && trig->op_trigger != 0)
		lim = trig->op_trigger;
	return lim;
}

