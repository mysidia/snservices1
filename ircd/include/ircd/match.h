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

#ifndef	IRCD_MATCH_H
#define IRCD_MATCH_H

int expr_match(const char *mask, const char *text);
int match(const char *mask, const char *string);
int r_match(const char *mask, const char *name);
char *collapse(char *pattern);

#endif /* IRCD_MATCH_H */
