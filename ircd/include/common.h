/*
 *   IRC - Internet Relay Chat, include/common.h
 *   Copyright (C) 1990 Armin Gruner
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	__common_include__
#define __common_include__

#include <sys/param.h>

#include <time.h>

#include "ircd/cdefs.h"

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define FALSE (0)
#define TRUE  (!FALSE)

int match(const char *, const char *);

#ifdef NEED_INET_ADDR
unsigned long inet_addr(char *);
#endif

#if defined(NEED_INET_NTOA)
#include <netinet/in.h>
#endif

#ifdef NEED_INET_NTOA
char *inet_ntoa(struct in_addr);
#endif

char *myctime(time_t);
char *strtoken(char **, char *, char *);

#if !defined(MAX)
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#if !defined(MIN)
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#ifndef KLINE_TEMP
#define KLINE_PERM 0
#define KLINE_TEMP 1
#define KLINE_AKILL 2
#endif

struct SLink *find_user_link(/* struct SLink *, struct Client * */);

extern time_t NOW, tm_offset;
#define update_time() NOW=(time(NULL)+tm_offset)

#define REPORT_START_DNS "*** Looking up your hostname..."
#define REPORT_DONE_DNS "*** Found your hostname"
#define REPORT_DONEC_DNS "*** Found your hostname (cached)"
#define REPORT_FAIL_DNS "*** Couldn't resolve your hostname; using IP address instead"

#define connotice(x, y) ( sendto_one(x, ":%s NOTICE AUTH :" y "", me.name) )
#define UNSURE 2

extern const char *service_nick[];
#define cNickServ       service_nick[0]
#define cChanServ       service_nick[1]
#define cMemoServ       service_nick[2]
#define cOperServ       service_nick[3]
#define cInfoServ       service_nick[4]
#define cGameServ       service_nick[5]

#ifdef NEED_SPRINTF
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

#endif /* __common_include__ */
