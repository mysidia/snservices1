/*
 *   IRC - Internet Relay Chat, common/support.c
 *   Copyright (C) 1990, 1991 Armin Gruner
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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"

#include "ircd/memory.h"

IRCD_RCSID("$Id$");

#if defined(DEBUGMODE)

static	char	*marray[20000];
static	int	mindex = 0;

#define	SZ_EX	(sizeof(char *) + sizeof(size_t) + 4)
#define	SZ_CHST	(sizeof(char *) + sizeof(size_t))
#define	SZ_CH	(sizeof(char *))
#define	SZ_ST	(sizeof(size_t))

void *
irc_malloc(size_t x)
{
	int	i;
	char	**s;
	char	*ret;

	ret = (char *)malloc(x + (size_t)SZ_EX);

	if (ret == NULL)
		outofmemory();

	bzero(ret, (int)x + SZ_EX);
	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&x, ret + SZ_CH, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)x, 4);
	Debug((DEBUG_MALLOC, "irc_malloc(%ld) = %#x", x, ret+8));
	for(i = 0, s = marray; *s && i < mindex; i++, s++)
		;
 	if (i < 20000) {
		*s = ret;
		if (i == mindex)
			mindex++;
	}
	return (ret + SZ_CHST);
}

void *
irc_realloc(void *x, size_t y)
{
        int	l;
	char	**s;
	char	*ret, *cp;
	size_t	i;
	int	k;

	x -= SZ_CHST;
	bcopy(x, (char *)&cp, SZ_CH);
	bcopy(x + SZ_CH, (char *)&i, SZ_ST);
	bcopy(x + (int)i + SZ_CHST, (char *)&k, 4);
	if (bcmp((char *)&k, "VAVA", 4) || (x != cp))
		dumpcore("irc_realloc %#x %d %d %#x %#x", x, y, i, cp, k);
	ret = (char *)realloc(x, y + (size_t)SZ_EX);

	if (ret == NULL)
		outofmemory();

	bcopy((char *)&ret, ret, SZ_CH);
	bcopy((char *)&y, ret + SZ_CH, SZ_ST);
	bcopy("VAVA", ret + SZ_CHST + (int)y, 4);
	Debug((DEBUG_NOTICE, "irc_realloc(%#x,%ld) = %#x", x, y, ret + SZ_CHST));
	for(l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
 	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
	for(l = 0, s = marray; *s && l < mindex; l++,s++)
		;
 	if (l < 20000) {
		*s = ret;
		if (l == mindex)
			mindex++;
	}

	return (ret + SZ_CHST);
}

void
irc_free(void *x)
{
	size_t	i;
	char	*j;
	u_char	k[4];
	int	l;
	char	**s;

	if (!x)
		return;
	x -= SZ_CHST;

	bcopy(x, (char *)&j, SZ_CH);
	bcopy((char *)x + SZ_CH, (char *)&i, SZ_ST);
	bcopy((char *)x + SZ_CHST + (int)i, (char *)k, 4);

	if (bcmp((char *)k, "VAVA", 4) || (j != x))
		dumpcore("irc_free %#x %ld %#x %#x", x, i, j,
			 (k[3]<<24) | (k[2]<<16) | (k[1]<<8) | k[0],
			 NULL, NULL, NULL, NULL, NULL);

	free(x);
	Debug((DEBUG_MALLOC, "irc_free(%#x)",x + SZ_CHST));

	for (l = 0, s = marray; *s != x && l < mindex; l++, s++)
		;
	if (l < mindex)
		*s = NULL;
	else if (l == mindex)
		Debug((DEBUG_MALLOC, "%#x !found", x));
}

char *
irc_strdup(const char *s)
{
	char *ret;
	int len;

	len = strlen(s) + 1;

	ret = irc_malloc(len);
	if (ret == NULL)
		outofmemory();

	strcpy(ret, s);  /* strcpy is safe */

	return (ret);
}

#else

void *
irc_malloc(size_t x)
{
	void *ret = malloc(x);

	if (ret == NULL)
		outofmemory();

	return	(ret);
}

void *
irc_realloc(void *x, size_t y)
{
	void *ret = realloc(x, y);

	if (ret == NULL)
		outofmemory();

	return (ret);
}

void
irc_free(void *x)
{
	free(x);
}

char *
irc_strdup(char *s)
{
	char *ret = strdup(s);

	if (ret == NULL)
		outofmemory();

	return (ret);
}

#endif /* DEBUGMODE */

void
outofmemory(void)
{
	Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
	exit(1);
}

