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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "setup.h"

#define	strcasecmp	mycmp
#define	strncasecmp	myncmp

#define   index   strchr
#define   rindex  strrchr

#define bcopy(x,y,z)	memcpy(y,x,z)
#define bcmp(x,y,z)	memcmp(x,y,z)
#define bzero(p,s)	memset(p,0,s)


#define VOIDSIG void	/* whether signal() returns int of void */

#ifdef SOL20
#define OPT_TYPE char	/* opt type for get/setsockopt */
#else
#define OPT_TYPE void
#endif

/*
 * Different name on NetBSD, FreeBSD, and BSDI
 */
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__bsdi__) || defined(REDHAT6) || defined(LINUX_GLIBC)
#define dn_skipname  __dn_skipname
#endif

#ifdef LINUX_GLIBC_RRES
#define res_init __res_init
#endif

extern	VOIDSIG	dummy();

#ifdef	NO_U_TYPES
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned long	u_long;
typedef	unsigned int	u_int;
#endif

#ifdef USE_DES
#include <des.h>
#endif

#define closesocket(x) close(x)
#define closefile(x) close(x)

#endif /* __sys_include__ */
