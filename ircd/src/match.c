/*
 *   IRC - Internet Relay Chat, common/match.c
 *   Copyright (C) 1990 Jarkko Oikarinen
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

#include "struct.h"
#include "common.h"
#include "sys.h"

#include <regex.h> 

#include "ircd/match.h"
#include "ircd/string.h"

IRCD_SCCSID("%W% %G% (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

/* 
 * Uses 'Regular Expression' matching.  The expression is recompiled every
 * time this function is called.  It would be much, much faster to keep
 * the compiled item around.
 */
int
expr_match(const char *mask, const char *text)
{
	regex_t preg;
	int ret;

	if (regcomp(&preg, mask, REG_ICASE|REG_EXTENDED|REG_NOSUB) != 0)
		return 0;

	ret = 0;
	if ((regexec(&preg, text, 0 , 0, 0) == 0))
		ret = 1;

	regfree(&preg);

	return (ret);
}

/*
 *  Compare if a given string (name) matches the given
 *  mask (which can contain wild cards: '*' - match any
 *  number of chars, '?' - match any single character.
 *
 *	return	0, if match
 *		1, if no match
 */

/*
 * match()
   Concept by JoelKatz (David Schwartz) <djls@gate.net>
   Thanks to Barubary (for fucking things up -- Skan)
 */

int
match(const char *mask, const char *string)
{
	const char *rmask = mask;
	const char *rstring = string;
	unsigned int l, p;

	while ((l = (u_char)(*(rmask++))) != 0) {
		if ((l == '*') || (l == '?'))
			continue;
		if (l == '\\') {
			l = (u_char)(*(rmask++));
			if (l == 0)
				return (r_match(mask, string));
		}
		p = (u_char)(*(rstring++));
		while ((p != 0) && (irc_tolower(p) != irc_tolower(l)))
			p = (u_char)(*(rstring++));
		/* If we're out of string, no match */
		if (p == 0)
			return (1);
	}
	return (r_match(mask, string));
}
 
/*
 * r_match()
 * Iterative matching function, rather than recursive.
 * Written by Douglas A Lewis (dalewis@acsu.buffalo.edu)
 */

int
r_match(const char *mask, const char *name)
{
	const u_char *m, *n;
	const char *ma, *na;
	int	  wild, q;

	m = (const u_char *)mask;
	n = (const u_char *)name;
	ma = mask;
	na = name;
	wild = 0;
	q = 0;
  
	while (1) {
		if (*m == '*') {
			while (*m == '*')
				m++;
			wild = 1;
			ma = (char *)m;
			na = (char *)n;
		}
    
		if (!*m) {
			if (!*n)
				return 0;
			for (m--; (m > (const u_char *)mask) && (*m == '?'); m--)
				;
			if ((*m == '*') && (m > (const u_char *)mask) &&
			    (m[-1] != '\\'))
				return 0;
			if (!wild) 
				return 1;
			m = (const u_char *)ma;
			n = (const u_char *)++na;
		} else if (!*n) {
			while(*m == '*')
				m++;
			return (*m != 0);
		}
		if ((*m == '\\') && ((m[1] == '*') || (m[1] == '?'))) {
			m++;
			q = 1;
		} else
			q = 0;
    
		if ((irc_tolower(*m) != irc_tolower(*n))
		    && ((*m != '?') || q)) {
			if (!wild)
				return 1;
			m = (const u_char *)ma;
			n = (const u_char *)++na;
		} else {
			if (*m)
				m++;
			if (*n)
				n++;
		}
	}
}

/*
 * collapse a pattern string into minimal components.
 * This particular version is "in place", so that it changes the pattern
 * which is to be reduced to a "minimal" size.
 */
char *
collapse(char *pattern)
{
	char *s;
	char *s1;
	char *t;
  
	s = pattern;
  
	if (BadPtr(pattern))
		return pattern;
	/*
	 * Collapse all \** into \*, \*[?]+\** into \*[?]+
	 */
	for (; *s; s++)
		if (*s == '\\')
			if (!*(s + 1))
				break;
			else
				s++;
		else if (*s == '*') {
			if (*(t = s1 = s + 1) == '*')
				while (*t == '*')
					t++;
			else if (*t == '?')
				for (t++, s1++; *t == '*' || *t == '?'; t++)
					if (*t == '?')
						*s1++ = *t;
			while ((*s1++ = *t++))
				;
		}

	return pattern;
}
