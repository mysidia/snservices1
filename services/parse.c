/**
 * \file parse.c
 * \brief Services message parser
 *
 * Procedures in this module obsolete breakString.  This module
 * is used to parse services messages and services databases.
 *
 * \skan
 * \date 1999
 *
 * $Id$
 */

/*
 * Copyright (c) 1999 Michael Graff
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "parse.h"

/// Debug print
#if 0
#define dprintf(x) do { printf x; fflush(stdout); } while (0);
#else
#define dprintf(x)
#endif

/*
 * parse_getarg() - get the next argument in the **args list
 */
char *
parse_getarg(parse_t *state)
{
	char *c;
	char *old_current;

	/** \bug XXX if state == NULL, this is a programming error.  What should
	 * be done?
	 */

	/*
	 * state->current is set to NULL when the end of string is reached.
	 */
	if (state->current == NULL) {
		dprintf(("parse_getarg() returning NULL (1)\n"));
		return (NULL);
	}

	c = state->current;

	/*
	 * Skip over any leading whitespace
	 */
	while (*c == state->delim)
		c++;

	/*
	 * If *c == \0, no more arguments were found.
	 */
	if (*c == '\0') {
		state->current = NULL;
		dprintf(("parse_getarg() returning NULL (2)\n"));
		return (NULL);
	}

	/*
	 * We found a non-space non-NUL character.  Scan forward in the
	 * string, looking for another space or the end of the string.
	 */
	old_current = c;
	state->current = c;
	while (*c != state->delim && *c != '\0')
		c++;

	/*
	 * If a space was found, change it to NUL, update current to point to
	 * the character after the space and and return the old current
	 * value.
	 */
	if (*c == state->delim) {
		*c++ = '\0';
		state->current = c;
		dprintf(("parse_getarg() returning %s\n", old_current));
		return (old_current);
	}

	/*
	 * To get here, *c must be NUL, so we have hit the end of the
	 * string.  This means our current argument is valid, but any more
	 * calls will return NULL, so change state->current as needed here.
	 */
	state->current = NULL;
	dprintf(("parse_getarg() returning %s\n", old_current));
	return (old_current);
}

char *
parse_getallargs(parse_t *state)
{
	char *c;
	char *e;

	/*
	 * Already called once, or at end of string?
	 */
	if (state->current == NULL) {
		dprintf(("parse_getallargs() returning NULL (1)\n"));
		return (NULL);
	}

	c = state->current;
	state->current = NULL;

	/*
	 * Skip over any leading whitespace
	 */
	while (*c == state->delim)
		c++;

	if (1)
	{
		/*
		 * Skip over leading ':'.
		 *
		 * XXX should be higher level...
		 */
		if (*c == ':')
			c++;

		/*
		 * Skip over whitespace again, in case we skipped over a colon above.
		 */
		while (*c == state->delim)
			c++;
	}

	/*
	 * If the string had nothing BUT spaces, return NULL instead
	 * of an empty string.
	 */
	if (*c == '\0') {
		dprintf(("parse_getallargs() returning NULL (2)\n"));
		return (NULL);
	}

	/*
	 * strip off any trailing whitespace
	 */
	e = c + strlen(c) - 1; /* ABC0 would make e point to C, not 0 */
	while ((e != c) && (*e == state->delim))
		e--;

	/*
	 * Nothing left in the string?  This SHOULD have been checked for
	 * above, but check here too.
	 */
	if ((e == c) && (*e == state->delim)) {
		dprintf(("parse_getallargs() returning NULL (3)\n"));
		return (NULL);
	}

	/*
	 * set the character after 'e' to be NUL.
	 */
	e++;
	*e = '\0';

	dprintf(("parse_getallargs() returning %s\n", c));
	return (c);
}

int
parse_init(parse_t *state, char *pointer)
{
	if (pointer && !*pointer)
		pointer = NULL;

	state->current = pointer;
	state->base = pointer;
	state->delim = ' ';

	return (0);
}

void
parse_cleanup(parse_t *state)
{
	state->base = NULL;
	state->current = NULL;
	state->delim = ' ';
}
