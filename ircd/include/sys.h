/*
 *   IRC - Internet Relay Chat, include/sys.h
 *   Copyright (C) 1990 University of Oulu, Computing Center
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

#ifndef	__sys_include__
#define __sys_include__
#ifdef ISC202
#include <net/errno.h>
#else
# ifndef _WIN32
#include <sys/errno.h>
# else
#include <errno.h>
# endif
#endif

#include "setup.h"
#include <stdio.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/param.h>
#else
#include <stdarg.h>
#endif

#ifdef	UNISTDH
#include <unistd.h>
#endif
#ifdef	STDLIBH
#include <stdlib.h>
#endif

#ifdef	STRINGH
#include <string.h>
#else
# ifdef	STRINGSH
# include <strings.h>
# endif
#endif
#define	strcasecmp	mycmp
#define	strncasecmp	myncmp
#ifdef NOINDEX
#define   index   strchr
#define   rindex  strrchr
/*
extern	char	*index PROTO((char *, char));
extern	char	*rindex PROTO((char *, char));
*/
#endif
#ifdef NOBCOPY
#define bcopy(x,y,z)	memcpy(y,x,z)
#define bcmp(x,y,z)	memcmp(x,y,z)
#define bzero(p,s)	memset(p,0,s)
#endif

#ifdef AIX
#include <sys/select.h>
#endif
#if defined(HPUX )|| defined(AIX) || defined(_WIN32)
#include <time.h>
#ifdef AIX
#include <sys/time.h>
#endif
#else
#include <sys/time.h>
#endif

#if !defined(DEBUGMODE)
# ifndef _WIN32
#  define MyFree(x)	if ((x) != NULL) free(x)
# else
#  define MyFree(x)       if ((x) != NULL) GlobalFree(x)
# endif
#else
void    dumpcore(const char *msg, ...);
void    MyFree(void *x);
#define	free(x)		MyFree(x)
#endif

#ifdef NEXT
#define VOIDSIG int	/* whether signal() returns int of void */
#else
#define VOIDSIG void	/* whether signal() returns int of void */
#endif

#ifdef SOL20
#define OPT_TYPE char	/* opt type for get/setsockopt */
#else
#define OPT_TYPE void
#endif

#ifndef _WIN32
extern	VOIDSIG	dummy();
#endif

#ifdef	DYNIXPTX
#define	NO_U_TYPES
typedef unsigned short n_short;         /* short as received from the net */
typedef unsigned long   n_long;         /* long as received from the net */
typedef unsigned long   n_time;         /* ms since 00:00 GMT, byte rev */
#define _NETINET_IN_SYSTM_INCLUDED
#endif

#ifdef	NO_U_TYPES
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned long	u_long;
typedef	unsigned int	u_int;
#endif

#include <stdarg.h>

#ifdef USE_DES
#include <des.h>
#endif

#ifndef _WIN32
#define closesocket(x) close(x)
#endif
#define closefile(x) close(x)


#endif /* __sys_include__ */
