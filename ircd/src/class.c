/*
 * Copyright (c) 2004, Onno Molenkamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ircd.h"

IRCD_RCSID("$Id$");

class *classlist;

/*
 * conf_class is called when a client configuration node is added or removed. 
 */

static CONF_HANDLER(conf_class)
{
	class	*cl;
	char	*tmp;

	if (n->status != CONFIG_OK)
	{
		tmp = config_get_string(n, "name");
		if (tmp == NULL)
		{
			return CONFIG_BAD;
		}
		cl = class_get(tmp);
		if (cl == NULL)
		{
			cl = irc_malloc(sizeof(class));
			cl->name = irc_strdup(tmp);
			cl->conns = 0;
			cl->refs = 0;
		}
		else if (cl->maxconns != -1)
		{
			return CONFIG_BAD;
		}
		tmp = config_get_string(n, "ping-frequency");
		cl->pingfreq = (tmp == NULL ? PINGFREQUENCY : atoi(tmp));
		tmp = config_get_string(n, "connect-frequency");
		cl->connfreq = (tmp == NULL ? CONNECTFREQUENCY : atoi(tmp));
		tmp = config_get_string(n, "max-connections");
		cl->maxconns = (tmp == NULL ? MAXCONNECTIONS : atoi(tmp));
		tmp = config_get_string(n, "send-queue");
		cl->maxsendq = (tmp == NULL ? MAXSENDQLENGTH : atoi(tmp));
		tmp = config_get_string(n, "max-channels");
		cl->maxchannels = (tmp == NULL ? MAXCHANNELSPERUSER : atoi(tmp));
		cl->refs++;

		cl->next = classlist;
		classlist = cl;
		n->data = cl;
	}
	else
	{
		cl = (class *) n->data;
		cl->maxconns = -1;
		class_free(cl);
	}
	return CONFIG_OK;
}

/*
 * class_free decreases the reference count of a class, and frees it when zero.
 *
 * cl         Class to be free'd.
 */

void class_free(class *cl)
{
	class	**cltmp;

	cl->refs--;
	if (cl->maxconns == -1 && cl->refs == 0)
	{
		for (cltmp = &classlist; *cltmp != NULL; cltmp = &(*cltmp)->next)
		{
			if (*cltmp == cl)
			{
				*cltmp = cl->next;
				break;
			}
		}
		irc_free(cl->name);
		irc_free(cl);
	}
}

/*
 * class_get returns the class with the given name.
 *
 * name       Name of class to return.
 *
 * returns    Requested class, or NULL if not found.
 */

class *class_get(char *name)
{
	class	*cl;

	for (cl = classlist; cl != NULL; cl = cl->next)
	{
		if (!strcmp(cl->name, name))
		{
			return cl;
		}
	}
	return NULL;
}

/*
 * class_init initializes everything required by the class functions.
 */

void class_init()
{
	classlist = NULL;
	config_monitor("class", conf_class, CONFIG_LIST);
}
