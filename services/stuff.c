/* $Id$ */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
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
 * \file stuff.c
 * Implementation of utility functions
 * \wd \taz \greg \mysid
 */


#include "services.h"
#include "hash.h"
#include "nickserv.h"
#include "log.h"

/*!
 * \fn void breakString(int numargs, char *string, char **args, char delimiter)
 * \brief Splits a string into a vector across a delimeter
 * \param numargs Maximum number of args to split the string into (N of args[])
 * \param string The string to split
 * \param args An array to store the string after it is split
 * \param delimeter Where to separate the string
 * \todo Remove this function if possible (or redo it)
 *
 * ===DOC===
 * This function returns data much like an argc/argv setup.
 * send it an integer, the string of data you want broken up, an array for
 * storage, and a delimiter character to seperate sets of data by.
 * Ex: I call fixUpData like this:
 * int blah;
 * char *holdme[];
 * ...
 * breakString(blah, "WD is a programmer", holdme[], ' ');
 * now:
 * blah == 5
 * holdme[0] == "WD", holdme[2] == "a", holdme[3] == "programmer", etc.
 * (All strings end in \0)
 * the idea for this was given to me by JoelKatz (David Schwartz) of DALnet
 * 
 */

void
breakString(int numargs, char *string, char **args, char delimiter)
{
	char *tmp = string;
	int x;
	numargs = 1;
	while (*tmp) {
		if (*tmp == delimiter) {
			*tmp = 0;
			tmp++;
			numargs++;
		} else
			tmp++;
	}

	x = 0;
	tmp = string;
	while (x < numargs) {
		strcpy(args[x], tmp);
		tmp = &string[strlen(string) + 1];
		x++;
	}
}


/*!
 *  \fn int match(const char *mask, const char *string)
 *  \param mask Pattern to match against
 *  \param string String to match
 *  \brief Matches a string against the specified pattern for wildcards \
 *   as allowed done by IRC clients/servers.
 *
 *  Compare if a given string (name) matches the given
 *  mask.
 *
 *  A wildcard match can be made, '*' matches any number of characters
 *  and '?' matches any single character.
 *
 *  Additionally, wildcards can be escaped with a preceding \.
 *
 *	return	0, if match
 *		1, if no match
 *
 *  \bug match() comes from ircd, it is GPL'ed and services can't be \
 *      distributed while it remains.
 *
 */

/*
 * Written By Douglas A. Lewis <dalewis@cs.Buffalo.EDU>
 *
 * The match procedure is public domain code (from ircII's reg.c)
 */

#if 1
int
match(const char *mask, const char *string)
{
        const char  *m = mask,
                *n = string,
                *ma = NULL,
                *na = NULL,
                *mp = NULL,
                *np = NULL;
        int     just = 0,
                pcount = 0,
                acount = 0,
                count = 0;

        for (;;)
        {
                if (*m == '*')
                {
                        ma = ++m;
                        na = n;
                        just = 1;
                        mp = NULL;
                        acount = count;
                }
#if 0 
                else if (*m == '%')
                {
                        mp = ++m;
                        np = n;
                        pcount = count;
                }
#endif
                else if (*m == '?')
                {
                        m++;
                        if (!*n++)
                                return 1;
                }
                else
                {
                        if (*m == '\\')
                        {
                                m++;
                                /* Quoting "nothing" is a bad thing */
                                if (!*m)
                                        return 1;
                        }
                        if (!*m)
                        {
                                /*
                                 * If we are out of both strings or we just
                                 * saw a wildcard, then we can say we have a
                                 * match
                                 */
                                if (!*n)
                                        return 0;
                                if (just)
                                        return 0;
                                just = 0;
                                goto not_matched;
                        }
                        /*
                         * We could check for *n == NULL at this point, but
                         * since it's more common to have a character there,
                         * check to see if they match first (m and n) and
                         * then if they don't match, THEN we can check for
                         * the NULL of n
                         */
                        just = 0;
                        if (tolower(*m) == tolower(*n))
                        {
                                m++;
                                if (*n == ' ')
                                        mp = NULL;
                                count++;
                                n++;
                        }
                        else
                        {

        not_matched:

                                /*
                                 * If there are no more characters in the
                                 * string, but we still need to find another
                                 * character (*m != NULL), then it will be
                                 * impossible to match it
                                 */
                                if (!*n)
                                        return 1;
                                if (mp)
                                {
                                        m = mp;
                                        if (*np == ' ')
                                        {
                                                mp = NULL;
                                                goto check_percent;
                                        }
                                        n = ++np;
                                        count = pcount;
                                }
                                else
        check_percent:

                                if (ma)
                                {
                                        m = ma;
                                        n = ++na;
                                        count = acount;
                                }
                                else
                                        return 1;
                        }
                }
        }
}

#endif

/**
 * \brief Lowercases a NUL-terminated string in place
 */
void
strtolower(char *str)
{
	char *s;

	s = str;

	while (*s) {
		*s = tolower(*s);
		s++;
	}
}

char *
sfgets(char *str, int len, FILE * fp)
{
	if (!fgets(str, len, fp))
		return NULL;
	else {
		if (str[0])
			str[strlen(str) - 1] = 0;
		return str;
	}
}

/*!
 * \fn char *xorit(char *tocrypt)
 * \param tocrypt String to encrypt in place
 * \brief Encrypt a password string
 *
 * Code passwords before writing them to services' database - this measure
 * allows roots to examine the structure of the services database and search
 * for corruption without having passwords that they don't need or want to see
 * shown to them.
 */
char *
xorit(char *tocrypt)
{
	char uncrypted[60];
	u_int i = 0;

	bzero(uncrypted, 60);
	strncpyzt(uncrypted, tocrypt, 59);
	for (; i < strlen(uncrypted); i++) {
		uncrypted[i] ^= 0x1B;
		if (uncrypted[i] == ' ' || uncrypted[i] == '\n')
			uncrypted[i] ^= 0x1B;
	}
	strcpy(tocrypt, uncrypted);
	return tocrypt;
}

/*!
 * \fn void parse_str(char **args, int argc, int startarg, char *workingstring, size_t sz)
 * \param args Source vector
 * \param argc Number of arguments in vector
 * \param startarg First item from vector to parse
 * \param sz Size of the working buffer
 * \workingstring Buffer of size IRCBUF to fill parsed buffer into
 * \brief Merge a vector of strings into one string across delimeters
 *
 * Takes an array of strings and fills a third single string with their
 * contents, each string separated by a space.
 *
 * this function is the reverse of the original breakString
 */

void
parse_str(char **args, unsigned int argc, unsigned int startarg, char *workingstring, size_t sz)
{
	unsigned int i;
	size_t len, len2;

	len = 0;
	workingstring[0] = '\0';

	for (i = startarg; i < argc; i++) {
		if (!args[i])
			break;
		len2 = strlen(args[i]);

		if ((len + len2 + ((i < (argc - 1)) ? 1 : 0 )) > (sz - 1))
			break;

		strcat(workingstring, args[i]);
		len += len2;

		if ((i + 1) < argc) {
			strcat(workingstring, " ");
			len++;
		}
	}
}

/*!
 * \fn void mask(char *user, char *host, int type, char *where)
 * \param user Username field
 * \param host Hostname field
 * \param type Prefix username in result with a *? (1 or 0)
 *
 * The mask function is used to generate a pattern that matches
 * the supplied usermask information at the user@*.domain.com level.
 *
 * \brief This function is used by the NickServ ADDMASK command.
 */
void
mask(char *user, char *host, int type, char *where)
{
	char *blah2 = NULL;

	if (!strchr(host, (int)'.')) {
		if (!type) {
			sprintf(where, "%.*s@%.*s", USERLEN, user, HOSTLEN,
					host);
			return;
		} else {
			sprintf(where, "*%.*s@%.*s", USERLEN, user, HOSTLEN,
					host);
			return;
		}
	}

	blah2 = index(host, '.');

	/*
	 * make sure there's more than one '.' in the name 
	 */
	if (strlen(blah2) > 4) {
		if (inet_addr(host) != INADDR_NONE) {
			blah2 = host;
			blah2 = rindex(blah2, '.');
			blah2[1] = '*';
			blah2[2] = '\0';
		} else {
			host = index(host, '.');
			host -= 1;
			host[0] = '*';
		}
	}
	if (!type)
		sprintf(where, "%.*s@%.*s", USERLEN, user, HOSTLEN, host);
	else
		sprintf(where, "*%.*s@%.*s", USERLEN, user, HOSTLEN, host);
}

/*! 
 * \fn void * oalloc (size_t size)
 * \brief Clean and allocate a memory area.
 * \param size How many bytes of memory are to be allocated
 *
 * Allocate n bytes of memory, check that is was properly allocated
 * and clean it.  This function is like calloc(), but will shut
 * down services cleanly if it fails to allocate
 */
void *
oalloc(size_t size)
{
	void *alloctmp = 0;

	if (size < 1) {
		fprintf(stderr,
				"\noalloc: Error, requested size is less than 1\n");
		/* Soft shutdown, with database safe, as there is a coding error */
		sshutdown(0);
	}

	alloctmp = calloc(size, 1);

	if (alloctmp == NULL) {
		fprintf(stderr,
				"\noalloc: Error allocating memory, terminating services\n");
		/*
		 * Hard shutdown, with the databases not being resaved 
		 * (as they could be corrupted) 
		 */
		sshutdown(-1);
	}

	return alloctmp;
}


/*!
 * \fn char *flagstring(int flags, const char *bits[])
 * \brief Report the names of bits set
 * \param flags Mask of bits from which the names of those set is to be reported
 * \param bits Array of strings that represent the names of bits (in order)
 *
 * The bitmask is converted into a string indicating which flags are
 * present in the string.
 */
char *
flagstring(int flags, const char *bits[])
{
	static char buf[MAXBUF] = "";
	int i = 0;

	if (!bits)
		return NULL;

	buf[0] = 0;
	for (i = 0; i < 32 && bits[i]; i++) {
		if (flags & (1 << i)) {
			if (*buf)
				strcat(buf, " ");
			strcat(buf, bits[i]);
		}
	}
	return buf;
}

/*!
 * \fn int flagbit(char *name, const char *bits[])
 * \param Get the bit flag number from the name and array of flag names
 * \param name Name of the flag to get the bit number of
 * \param bits Array of bit names (in bit order)
 */
int
flagbit(char *name, const char *bits[])
{
	int i = 0;

	if (!name || !bits)
		return -1;
	for (i = 0; i < 32 && bits[i] && *bits[i]; i++)
		if (!strcasecmp(name, bits[i]))
			return i;
	return -1;
}

void
tzapply(char *to)
{
	int i = 0;
	if (!to || *to == ':' || *to == '\0' || strlen(to) != 3)
		return;
	for (i = 0; to[i]; i++)
		if (!isalpha(to[i]))
			return;
	setenv("TZ", to, 1);
	tzset();
}

#if HOSTLEN <= 63
#warning HOSTLEN <= 63
#endif
#define HOSTMAX (HOSTLEN + 1)

/*!
 * \fn char *tokenEncode(char *str)
 * \param str String (token) that is to be hashed
 * \brief Applies the mask (string) hashing algorithm to a token
 */
char *
tokenEncode(char *str)
{
	static char strn[HOSTMAX + 255];
	unsigned char *p;
	unsigned long m, v;

	for (p = (unsigned char *)str, m = 0, v = 0x55555; *p; p++)
		v = (31 * v) + (*p);
	sprintf(strn, "%x", (unsigned int)v);
	return strn;
}

/*!
 * \fn char *genHostMask(char *host)
 * \param host Name of the host to mask
 * \brief Returns the masked version of a host
 */
char *
genHostMask(char *host)
{
	char tok1[USERLEN + HOSTMAX + 255];
	char tok2[USERLEN + HOSTMAX + 255], *p, *q;
	static char fin[USERLEN + HOSTMAX + 255];
	char fintmp[USERLEN + HOSTMAX + 255];
	int i, fIp = TRUE;

	if (!host || !strchr(host, '.')
		|| strlen(host) >
		(USERLEN + HOSTMAX + 200)) return (host ? host :
		 const_cast<char *>(""));
	for (i = 0; host[i]; i++)
		if ((host[i] < '0' || host[i] > '9') && host[i] != '.') {
			fIp = FALSE;
			fIp = FALSE;
		}

	*tok1 = *tok2 = '\0';

	/* It's an ipv4 address in quad-octet dot notation: last two tokens are encoded */
	if (fIp && strlen(host) <= 15) {
		if ((p = strrchr(host, '.'))) {
			*p = '\0';
			strcpy(tok1, host);
			strcpy(tok2, p + 1);
			*p = '.';
		}
		if ((p = strrchr(tok1, '.'))) {
			strcpy(fintmp, tokenEncode(p + 1));
			*p = '\0';
		}
		sprintf(fin, "%s.%s.%s.imsk", tokenEncode(tok2), fintmp, tok1);
		return fin;
	}

	/* It's a resolved hostname, hash the first token */
	if ((p = strchr(host, '.'))) {
		*p = '\0';
		strcpy(tok1, host);
		strcpy(fin, p + 1);
		*p = '.';
	}

	/* Then separately hash the domain */
	if ((p = strrchr(fin, '.'))) {
		--p;
		while (p > fin && *(p - 1) != '.')
			p--;
	}
	if (p && (q = strrchr(fin, '.'))) {
		i = (unsigned char)*p;
		*p = '\0';
		*q = '\0';
		strcat(tok2, fin);
		*p = (unsigned char)i;
		if (*p == '.')
			strcat(tok2, tokenEncode(p + 1));
		else
			strcat(tok2, tokenEncode(p));
		*q = '.';
		strcat(tok2, q);
	} else
		strcpy(tok2, fin);
	strcpy(tok1, tokenEncode(tok1));
	snprintf(fin, HOSTLEN, "%s.%s.hmsk", tok1, tok2);

	return fin;
}

/**
 * \fn char *str_dup(const char *input)
 * \param input String to duplicate
 * \brief Allocates and returns a copy of the supplied string.
 */
char *
str_dup(const char *input)
{
	char *buf;
	int len;

	for (buf = (const_cast<char *>(input)); *buf; buf++);
	buf = static_cast <char *>((oalloc(1 + (len = (buf - input)))));
	return static_cast <char *>(memcpy(buf, input, len + 1));
}

/*!
 * \fn char *strn_dup(const char *input, int max)
 * \param input String to duplicate
 * \param max Maximum length of new string
 * \brief Allocates and returns a copy of part of the supplied string.
 */
char *
strn_dup(const char *input, int max)
{
	char *buf;
	int len;

	for (buf = (const_cast<char *>(input));
		 *buf && ((buf - input) < max); buf++);
	len = buf - input;
	buf = static_cast<char *>(oalloc(1 + len));
	buf[len] = '\0';
	return (char *)memcpy(buf, input, len);
}

/*!
 * \fn MaskData *make_mask()
 * \brief Allocates a mask structure
 */
MaskData *
make_mask()
{
	MaskData *mask = (MaskData *) oalloc(sizeof(MaskData));
	mask->nick = mask->user = mask->host = (char *)0;

	return mask;
}

/*!
 * \fn void free_mask(MaskData *)
 * \param mask Mask structure to free
 * \brief Frees a mask structure
 */
void
free_mask(MaskData *mask)
{
	if (mask->nick)
		FREE(mask->nick);
	if (mask->user)
		FREE(mask->user);
	if (mask->host)
		FREE(mask->host);
	FREE(mask);
}

/*!
 * \fn int split_userhost(const char *input_host, MaskData *data);
 * \param input_host String to split
 * \param data Mask structure to fill and allocate members of
 * \brief Splits a string into a supplied mask structure
 */
int
split_userhost(const char *input_host, MaskData *data)
{
	extern FILE *corelog;
	char *host, *p, *p2;
	char *tmpp, *lastp;
#define IrcStr(point) ((!*(point)) ? "*" : (point))

	if (!input_host || !data) {
		logDump(corelog, "split_userhost: Null pointer (host=%p,data=%p)",
				input_host, data);
		fflush(corelog);

		if (data)
			data->nick = data->user = data->host = NULL;
		return -1;
	}

	host = str_dup(input_host);

	for (p = host; *p && *p != '!' && *p != '@'; p++);

	if (!*p) {
		data->nick = strn_dup(IrcStr(host), NICKLEN);
		data->user = str_dup("*");
		data->host = str_dup("*");
		FREE(host);
		return 0;
	}

	if (*p == '@') {
		for (tmpp = p + 1, lastp = NULL; *tmpp; tmpp++)
			if (*tmpp == '@')
				lastp = tmpp;
		if (lastp && *lastp == *p)
			p = lastp;
		*p = '\0';
		data->nick = str_dup("*");
		data->user = strn_dup(IrcStr(host), USERLEN);
		data->host = strn_dup(IrcStr(p + 1), HOSTLEN);
		FREE(host);
		return 0;
	}

	if (*p == '!') {
		for (tmpp = p + 1, lastp = NULL; *tmpp && *tmpp != '@'; tmpp++)
			if (*tmpp == '!')
				lastp = tmpp;
		if (lastp && *lastp == *p)
			p = lastp;

		for (p2 = p + 1; *p2 && *p2 != '@'; p2++);
		if (*p2 == '@') {
			*p = *p2 = '\0';
			data->nick = strn_dup(IrcStr(host), NICKLEN);
			data->user = strn_dup(IrcStr(p + 1), USERLEN);
			data->host = strn_dup(IrcStr(p2 + 1), HOSTLEN);
			FREE(host);
			return 0;
		} else {
			*p = '\0';
			data->nick = strn_dup(IrcStr(host), NICKLEN);
			data->user = strn_dup(IrcStr(p + 1), USERLEN);
			data->host = str_dup("*");
			FREE(host);
			return 0;
		}
	}

	FREE(host);
	return -1;
}


/**
 * \pre  From points to a valid NUL-terminated character array, and
 *       add is a reference to a character array to be changed.
 *
 * \post *Buf area is reallocated to contain its present state plus
 *       the string specified as 'add'
 */
void AppendBuffer(char **buf, const char *add)
{
     char *newbuf, *x;

     /* x = newbuf = new char[ ((buf && *buf ? strlen(*buf) : 0)) + strlen(add) + 4]; */

     x = newbuf = (char *)oalloc(((*buf ? strlen(*buf) : 0)) + strlen(add) + 4);
     if (!newbuf) return;
     if (*buf)
     {
       bzero( newbuf, (*buf ? strlen( *buf ):0) + strlen(add) + 2 );
       memcpy( newbuf, *buf, strlen(*buf) );
       x = newbuf + strlen(*buf);
     } else x = newbuf;
     strcpy(x, add); /* this is ok */
     if (*buf) {
         FREE(*buf);
     }
     *buf = newbuf;
     return;
}


/**
 * \pre  From points to a valid NUL-terminated character array, and
 *       add is a reference to a character array to be changed.
 *
 * \post *Buf area is reallocated to contain the string specified as
 *       'new'
 */
void SetDynBuffer(char **buf, const char *newStr)
{
	char *newbuf, *x;

	if (newStr) {
		x = newbuf = (char *)oalloc(strlen(newStr) + 1);
		if (!newbuf)
			return;
		strcpy(x, newStr);
	}
	else
		newbuf = NULL;

	if (*buf)
		FREE(*buf);
	*buf = newbuf;
	return;
}

/* --------------------------------------------------------------------- */
/* Registration Identifiers */

void RegId::SetNext(RegId &topCounter)
{
	a = topCounter.a;
	b = topCounter.b;
		
	if (b == IDVAL_MAX) {
		if (topCounter.a == IDVAL_MAX) {
			logDump(corelog, "::Out of nick IDs at (%d, %d)::",
                                topCounter.a, topCounter.b);
			sSend("WALLOPS :Error! Out of nick IDs at (%d, %d)::",
                                topCounter.a, topCounter.b);
	                sshutdown(-1);
		}
		topCounter.a++;
		topCounter.b = 0;
	}
	else
		topCounter.b++;
}


void RegId::SetDirect(RegId &topCounter, IdVal aVal, IdVal bVal)
{
	a = aVal;
	b = bVal;

	if (topCounter.a < aVal)
		topCounter.a = aVal;
	if (topCounter.a <= aVal && topCounter.b <= bVal) {
		if (topCounter.b == IDVAL_MAX)
		{
			topCounter.a = aVal+1;
			topCounter.b = 0;
		}
		else
			topCounter.b = bVal+1;
	}
}

RegNickList *RegId::getNickItem()
{
	RegNickIdMap *ptrMap;

	ptrMap = getIdMap();

	if (!ptrMap || !ptrMap->nick)
		return NULL;
	return ptrMap->nick;
}

const char *RegId::getNick()
{
	RegNickList *ptrNick;

	ptrNick = getNickItem();

	if (ptrNick == NULL) {
		if (a == 0)
		{
			switch(b)
			{
				case 1: return NETWORK;
				case 2: return "*";

				default:;
			}
		}
		return NULL;
	}
	return ptrNick->nick;
}

const char *RegId::getChan()
{
	abort(); return NULL;
}

/*HashKeyVal RegId::getHashKey() const
{
	HashKeyVal x = (a ^ 27);

	x += (b * IDVAL_MAX);

	return x;
}*/

RegNickIdMap *RegId::getIdMap()
{
	HashKeyVal hashEnt;
	RegNickIdMap *mapPtr;

	hashEnt = this->getHashKey() % IDHASHSIZE;

	mapPtr = LIST_FIRST(&RegNickIdHash[hashEnt]);

	while( mapPtr )
	{

		if ((*mapPtr).id == (*this))
			return mapPtr;
		mapPtr = LIST_NEXT(mapPtr, id_lst);
	}

	return NULL;
}

/* Encode a URL -Mysid*/

char *urlEncode(const char *in)
{
	static char* out = 0;
	int i, x = 0, l = 0;

	if (in == 0) return 0;

	if (out)
		FREE(out);

	for(i = 0; in[i]; i++)
	{
		if (isalnum(in[i]))
			l++;
		else
			l+=3;
	}

	out = (char *) oalloc(l + 1);
	for(i = 0; in[i]; i++)
	{
		if (isalnum(in[i]))
			out[x++] = in[i];
		else {
			l+=3;
			out[x++] = '%';
			x += sprintf(out + x, "%.2X", in[i]);
		}
	}
	return out;
}

void rshift_argv(char **args, int x, int numargs)
{
        int i;

        for (i = 0; i < (numargs - x); i++)
                args[i] = args[i + x];
        args[numargs - x] = NULL;
}

/* --------------------------------------------------------------------- */

/* $Id$ */
