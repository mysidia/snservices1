/************************************************************************
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

#include "ircd/cdefs.h"

#include <time.h>

#ifdef	PARAMH
#include <sys/param.h>
#endif

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

#if defined(NEED_INET_NTOA) || defined(NEED_INET_NETOF)
#include <netinet/in.h>
#endif

#ifdef NEED_INET_NTOA
char *inet_ntoa(struct in_addr);
#endif

#ifdef NEED_INET_NETOF
int inet_netof(struct in_addr);
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

void flush_connections(int);
struct SLink *find_user_link(/* struct SLink *, struct Client * */);

extern time_t NOW, tm_offset;
#define update_time() NOW=(time(NULL)+tm_offset)

#define REPORT_START_DNS "*** Looking up your hostname..."
#define REPORT_DONE_DNS "*** Found your hostname"
#define REPORT_DONEC_DNS "*** Found your hostname (cached)"
#define REPORT_FAIL_DNS "*** Couldn't resolve your hostname; using IP address instead"

#define REPORT_START_AUTH "*** Checking ident..."
#define REPORT_FIN_AUTH "*** Received ident response"
#define REPORT_FAIL_AUTH "*** No ident response; username prefixed with ~"
#define REPORT_ERR_AUTH  "*** Unable to get ident; username prefixed with ~"

#define REPORT_START_SOCKS "*** Checking for open socks server..."
#define REPORT_FAIL_SOCKS "*** No socks server found (good)"
#define REPORT_FIN_SOCKS "*** Open socks server found (bad)"
#define REPORT_OK_SOCKS "*** No/Secure socks server found (ok)"
  /* remote closed inbetween our non-blocking connect() and send()  ::  tcp wrappers ? */
#define REPORT_ERR_SOCKS "*** Socks port is firewalled or connection timed out"
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
