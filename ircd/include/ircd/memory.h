/*
 *   IRC - Internet Relay Chat
 *   (C) 2003 Michael Graff
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
 * These functions replace the standard malloc(), realloc(), free(),
 * and strdup() calls.  Using these allows debugging code to be inserted
 * to track memory usage or to verify memory was not overwritten to help
 * avoid buffer overflows.
 *
 * The malloc(), realloc(), and strdup() functions will verify that there
 * was sufficient memory.  If memory could not be allocated, it will
 * call outofmemory() itself, so there is no need to check for the
 * pointer being non-NULL before using it.
 */

#ifndef IRCD_MEMORY_H
#define IRCD_MEMORY_H

void *irc_malloc(size_t x);
void *irc_realloc(void *x, size_t y);
void irc_free(void *x);
char *irc_strdup(char *s);

/*
 * Called by the above when memory is not available.
 */
void outofmemory(void);

#endif /* IRCD_MEMORY_H */
