/************************************************************************
 *   IRC - Internet Relay Chat, common/dbuf.c
 *   Copyright (C) 1990 Markku Savela
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

/*
** For documentation of the *global* functions implemented here,
** see the header file (dbuf.h).
**
*/

#include <stdio.h>
#include "struct.h"
#include "common.h"
#include "h.h"
#include "sys.h"

IRCD_SCCSID("@(#)dbuf.c	2.17 1/30/94 (C) 1990 Markku Savela");
IRCD_RCSID("$Id$");

int	dbufalloc = 0, dbufblocks = 0;
static	dbufbuf	*freelist = NULL;

/* This is a dangerous define because a broken compiler will set DBUFSIZ
** to 4, which will work but will be very inefficient. However, there
** are other places where the code breaks badly if this is screwed
** up, so... -- Wumpus
*/

#define DBUFSIZ sizeof(((dbufbuf *)0)->data)

/*
** dbuf_alloc - allocates a dbufbuf structure either from freelist or
** creates a new one.
*/
static dbufbuf *
dbuf_alloc(void)
{
	dbufbuf *dbptr;

	dbufalloc++;
	if ((dbptr = freelist)) {
		freelist = freelist->next;
		return dbptr;
	}
	if (dbufalloc * DBUFSIZ > BUFFERPOOL) {
		dbufalloc--;
		return NULL;
	}

	dbufblocks++;
	return (irc_malloc(sizeof(dbufbuf)));
}

/*
** dbuf_free - return a dbufbuf structure to the freelist
*/
static	void
dbuf_free(dbufbuf *ptr)
{
	dbufalloc--;
	ptr->next = freelist;
	freelist = ptr;
}

/*
** This is called when malloc fails. Scrap the whole content
** of dynamic buffer and return -1. (malloc errors are FATAL,
** there is no reason to continue this buffer...). After this
** the "dbuf" has consistent EMPTY status... ;)
*/
static int
dbuf_malloc_error(dbuf *dyn)
{
	dbufbuf *p;

	dyn->length = 0;
	dyn->offset = 0;
	while ((p = dyn->head) != NULL) {
		dyn->head = p->next;
		dbuf_free(p);
	}

	return -1;
}


int
dbuf_put(dbuf *dyn, char *buf, int length)
{
	dbufbuf	**h, *d;
	int	nbr, off;
	int	chunk;

	off = (dyn->offset + dyn->length) % DBUFSIZ;
	nbr = (dyn->offset + dyn->length) / DBUFSIZ;
	/*
	** Locate the last non-empty buffer. If the last buffer is
	** full, the loop will terminate with 'd==NULL'. This loop
	** assumes that the 'dyn->length' field is correctly
	** maintained, as it should--no other check really needed.
	*/
	for (h = &(dyn->head); (d = *h) && --nbr >= 0; h = &(d->next));
	/*
	** Append users data to buffer, allocating buffers as needed
	*/
	chunk = DBUFSIZ - off;
	dyn->length += length;
	for ( ;length > 0; h = &(d->next)) {
		if ((d = *h) == NULL) {
			if ((d = (dbufbuf *)dbuf_alloc()) == NULL)
				return dbuf_malloc_error(dyn);
			*h = d;
			d->next = NULL;
		}
		if (chunk > length)
			chunk = length;
		bcopy(buf, d->data + off, chunk);
		length -= chunk;
		buf += chunk;
		off = 0;
		chunk = DBUFSIZ;
	}

	return 1;
}


char	*
dbuf_map(dbuf *dyn, int *length)
{
	if (dyn->head == NULL) {
		*length = 0;
		return NULL;
	}
	*length = DBUFSIZ - dyn->offset;
	if (*length > dyn->length)
		*length = dyn->length;
	return (dyn->head->data + dyn->offset);
}

int
dbuf_delete(dbuf *dyn, int length)
{
	dbufbuf *d;
	int chunk;

	if (length > dyn->length)
		length = dyn->length;
	chunk = DBUFSIZ - dyn->offset;
	while (length > 0) {
		if (chunk > length)
			chunk = length;
		length -= chunk;
		dyn->offset += chunk;
		dyn->length -= chunk;
		if (dyn->offset == DBUFSIZ || dyn->length == 0) {
			d = dyn->head;
			dyn->head = d->next;
			dyn->offset = 0;
			dbuf_free(d);
		}
		chunk = DBUFSIZ;
	}
	if (dyn->head == (dbufbuf *)NULL)
		dyn->length = 0;
	return 0;
}

int
dbuf_get(dbuf *dyn, char *buf, int length)
{
	int	moved = 0;
	int	chunk;
	char	*b;

	while (length > 0 && (b = dbuf_map(dyn, &chunk)) != NULL) {
		if (chunk > length)
			chunk = length;
		bcopy(b, buf, (int)chunk);
		(void)dbuf_delete(dyn, chunk);
		buf += chunk;
		length -= chunk;
		moved += chunk;
	}

	return moved;
}

/*
** dbuf_getmsg
**
** Check the buffers to see if there is a string which is terminted with
** either a \r or \n prsent.  If so, copy as much as possible (determined by
** length) into buf and return the amount copied - else return 0.
*/
int
dbuf_getmsg(dbuf *dyn, char *buf, int length)
{
	dbufbuf	*d;
	char	*s;
	int	dlen;
	int	i;
	int	copy;

getmsg_init:
	d = dyn->head;
	dlen = dyn->length;
	i = DBUFSIZ - dyn->offset;
	if (i <= 0)
		return -1;
	copy = 0;
	if (d && dlen)
		s = dyn->offset + d->data;
	else
		return 0;

	if (i > dlen)
		i = dlen;
	while (length > 0 && dlen > 0) {
		dlen--;
		if (*s == '\n' || *s == '\r') {
			copy = dyn->length - dlen;
			/*
			** Shortcut this case here to save time elsewhere.
			** -avalon
			*/
			if (copy == 1) {
				(void)dbuf_delete(dyn, 1);
				goto getmsg_init;
			}
			break;
		}
		length--;
		if (!--i) {
			if ((d = d->next)) {
				s = d->data;
				i = MIN(DBUFSIZ, dlen);
			}
		} else
			s++;
	}

	if (copy <= 0)
		return 0;

	/*
	** copy as much of the message as wanted into parse buffer
	*/
	i = dbuf_get(dyn, buf, MIN(copy, length));
	/*
	** and delete the rest of it!
	*/
	if (copy - i > 0)
		(void)dbuf_delete(dyn, copy - i);
	if (i >= 0)
		*(buf+i) = '\0';	/* mark end of messsage */

	return i;
}
