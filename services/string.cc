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
 * @file string.cc
 * \brief Services string replies/notices procedures
 *
 * The idea is to move all handling of error/reply strings
 * into a single module so that multi-language support becomes
 * remotely possible.
 *
 * \mysid
 * \date 2001
 *
 * $Id$
 */

#define __string_cc__
#include "services.h"
#include "./string.h"


/**
 * \brief Get a reply string
 * \param reply Reply type (ex: #ERR_NEEDREGNICK)
 *        (constraint: Must use a RPL_xxx or ERR_xxx constant)
 * \return Pointer to the requested error string or a message of unknown error
 */
const char* get_reply(reply_type reply)
{
	static char buf[256];

	if (reply < 0 || reply >= MAX_REPLY_STRING_NUM) {
		sprintf(buf, "NO REPLY FOR ERROR %d -- please report this.", reply);
		return buf;
	}

	return reply_table[reply].string;
}

/**
 * \brief Send out a reply or error string to a user
 * \param cService Service to send out the message
 *        (constraint: Must be one of the services registered at startup)
 * \param nTo Target nickname record
 * \param replyNum Reply selected for transmission
 * \param ... Format string, dependent on the error/reply string
 */
void reply(const char *cService, UserList *nTo, int replyNum, ...)
{
	static char buf[(IRCBUF*2)+10];
        va_list ap;
	int sz;

	if (replyNum < 0 || replyNum >= MAX_REPLY_STRING_NUM) {
		sprintf(buf, "NO REPLY FOR ERROR %d -- please report this.\r\n", replyNum);

		net_write(server, buf, strlen(buf));
		return;
	}

	sz = sprintf(buf, ":%s NOTICE %s :", cService, nTo->nick);

	va_start(ap, replyNum);
	sz += vsnprintf(buf + sz, sizeof(buf), reply_table[replyNum].string, ap);
	va_end(ap);

	buf[sz] = '\r';
	buf[sz+1] = '\n';
	buf[sz+2] = '\0';
	sz += 2;

	net_write(server, buf, sz);
}
