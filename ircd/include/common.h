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

#include <time.h>
#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#include <winsock.h>
#include <process.h>
#include <io.h>
#include "struct.h"

typedef unsigned long int u_int32_t;
typedef unsigned char u_int8_t;
#endif

#ifndef _WIN32
#ifdef	PARAMH
#include <sys/param.h>
#endif
#endif

#ifndef PROTO
#if __STDC__
#	define PROTO(x)	x
#else
#	define PROTO(x)	()
#endif
#endif

#ifndef NULL
#define NULL 0
#endif

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define FALSE (0)
#define TRUE  (!FALSE)

#if 0
#ifndef	MALLOCH
char	*malloc(), *calloc();
void	free();
#else
#include MALLOCH
#endif
#endif

extern	int	match PROTO((const char *, const char *));
#define mycmp(a,b) \
 ( (toupper((a)[0])!=toupper((b)[0])) || (((a)[0]!=0) && smycmp((a)+1,(b)+1)) )
extern int     smycmp PROTO((const char *, const char *));
#if !defined(REDHAT6) && !defined(LINUX_GLIBC)
extern	int	myncmp PROTO((const char *, const char *, int));
#endif
#ifdef NEED_STRTOK
#if !defined(REDHAT5) && !defined(REDHAT6) && !defined(LINUX_GLIBC)
extern	char	*strtok PROTO((char *, char *));
#endif
#endif
#ifdef NEED_STRTOKEN
extern	char	*strtoken PROTO((char **, char *, char *));
#endif
#ifdef NEED_INET_ADDR
extern unsigned long inet_addr PROTO((char *));
#endif

#if defined(NEED_INET_NTOA) || defined(NEED_INET_NETOF) && !defined(_WIN32)
#include <netinet/in.h>
#endif

#ifdef NEED_INET_NTOA
extern char *inet_ntoa PROTO((struct in_addr));
#endif

#ifdef NEED_INET_NETOF
extern int inet_netof PROTO((struct in_addr));
#endif

extern char *myctime PROTO((time_t));
extern char *strtoken PROTO((char **, char *, char *));

#if !defined(HAVE_MINMAX)
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif
#if !defined(HAVE_MINMAX)
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define DupString(x,y) do{x=MyMalloc(strlen(y)+1);(void)strcpy(x,y);}while(0)

#ifdef USE_CASETABLES
extern int casetable;
extern u_char *tolowertab, tolowertab1[], tolowertab2[];
extern u_char *touppertab, touppertab1[], touppertab2[];
#else
extern u_char tolowertab[], touppertab[];
#endif

#undef tolower
#define tolower(c) (tolowertab[(c)])

#undef toupper
#define toupper(c) (touppertab[(c)])

#undef isalpha
#undef isdigit
#undef isxdigit
#undef isalnum
#undef isprint
#undef isascii
#undef isgraph
#undef ispunct
#undef islower
#undef isupper
#undef isspace
#undef iscntrl

extern unsigned char char_atribs[];

#define PRINT 1
#define CNTRL 2
#define ALPHA 4
#define PUNCT 8
#define DIGIT 16
#define SPACE 32

#ifndef KLINE_TEMP
#define KLINE_PERM 0
#define KLINE_TEMP 1
#define KLINE_AKILL 2
#endif

#define	iscntrl(c) (char_atribs[(u_char)(c)]&CNTRL)
#define isalpha(c) (char_atribs[(u_char)(c)]&ALPHA)
#define isspace(c) (char_atribs[(u_char)(c)]&SPACE)
#define islower(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) > 0x5f))
#define isupper(c) ((char_atribs[(u_char)(c)]&ALPHA) && ((u_char)(c) < 0x60))
#define isdigit(c) (char_atribs[(u_char)(c)]&DIGIT)
#define	isxdigit(c) (isdigit(c) || 'a' <= (c) && (c) <= 'f' || \
		     'A' <= (c) && (c) <= 'F')
#define isalnum(c) (char_atribs[(u_char)(c)]&(DIGIT|ALPHA))
#define isprint(c) (char_atribs[(u_char)(c)]&PRINT)
#define isascii(c) ((u_char)(c) >= 0 && (u_char)(c) <= 0x7f)
#define isgraph(c) ((char_atribs[(u_char)(c)]&PRINT) && ((u_char)(c) != 0x32))
#define ispunct(c) (!(char_atribs[(u_char)(c)]&(CNTRL|ALPHA|DIGIT)))

extern char *MyMalloc();
extern void flush_connections();
extern struct SLink *find_user_link(/* struct SLink *, struct Client * */);

#ifdef _WIN32
/*
 * Used to display a string to the GUI interface.
 * Windows' internal strerror() function doesn't work with socket errors.
 */
extern	int	DisplayString(HWND hWnd, char *InBuf, ...);
#undef	strerror
_inline	void	alarm(unsigned int seconds) { }
#endif

time_t NOW, tm_offset;
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

#define SERVICES_NAME   "services.sorcery.net"

extern const char *service_nick[];
#define cNickServ       service_nick[0]
#define cChanServ       service_nick[1]
#define cMemoServ       service_nick[2]
#define cOperServ       service_nick[3]
#define cInfoServ       service_nick[4]
#define cGameServ       service_nick[5]

#endif /* __common_include__ */
