/*
 *   IRC - Internet Relay Chat
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

#ifndef IRCD_STRING_H
#define IRCD_STRING_H

#include "sys.h"

#ifdef NEED_STRTOK
#if !defined(REDHAT5) && !defined(REDHAT6) && !defined(LINUX_GLIBC)
extern	char	*strtok(char *, char *);
#endif
#endif

#ifdef NEED_STRTOKEN
extern	char	*strtoken(char **, char *, char *);
#endif

extern unsigned char irc_tolowertab[];
extern unsigned char irc_touppertab[];
extern unsigned char irc_charattrib[];

#define irc_tolower(c) (irc_tolowertab[(unsigned char)c])
#define irc_toupper(c) (irc_touppertab[(unsigned char)c])

#define IRC_CHAR_PRINT 0x01
#define IRC_CHAR_CNTRL 0x02
#define IRC_CHAR_ALPHA 0x04
#define IRC_CHAR_PUNCT 0x08
#define IRC_CHAR_DIGIT 0x10
#define IRC_CHAR_SPACE 0x20

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
#define	iscntrl(c) (irc_charattrib[(u_char)(c)]&IRC_CHAR_CNTRL)
#define isalpha(c) (irc_charattrib[(u_char)(c)]&IRC_CHAR_ALPHA)
#define isspace(c) (irc_charattrib[(u_char)(c)]&IRC_CHAR_SPACE)
#define islower(c) ((irc_charattrib[(u_char)(c)]&IRC_CHAR_ALPHA) && ((u_char)(c) > 0x5f))
#define isupper(c) ((irc_charattrib[(u_char)(c)]&IRC_CHAR_ALPHA) && ((u_char)(c) < 0x60))
#define isdigit(c) (irc_charattrib[(u_char)(c)]&IRC_CHAR_DIGIT)
#define	isxdigit(c) (isdigit(c) || 'a' <= (c) && (c) <= 'f' || \
		     'A' <= (c) && (c) <= 'F')
#define isalnum(c) (irc_charattrib[(u_char)(c)]&(IRC_CHAR_DIGIT|IRC_CHAR_ALPHA))
#define isprint(c) (irc_charattrib[(u_char)(c)]&IRC_CHAR_PRINT)
#define isascii(c) ((u_char)(c) >= 0 && (u_char)(c) <= 0x7f)
#define isgraph(c) ((irc_charattrib[(u_char)(c)]&IRC_CHAR_PRINT) && ((u_char)(c) != 0x32))
#define ispunct(c) (!(irc_charattrib[(u_char)(c)]&(IRC_CHAR_CNTRL|IRC_CHAR_ALPHA|IRC_CHAR_DIGIT)))

#define mycmp(a,b) \
 ( (irc_toupper((a)[0])!=irc_toupper((b)[0])) || (((a)[0]!=0) && smycmp((a)+1,(b)+1)) )
int     smycmp(const char *, const char *);
int	myncmp(const char *, const char *, int);

#endif /* IRCD_STRING_H */
