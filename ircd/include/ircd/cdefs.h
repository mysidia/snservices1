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

#ifndef IRCD_CDEFS_H
#define IRCD_CDEFS_H

#ifdef __NetBSD__
#include <sys/cdefs.h>
#define IRCD_RCSID(x)	__RCSID(x)
#define IRCD_SCCSID(x)	__SCCSID(x)

#else

/*
 * For now, just define these away.  We may want to eventually put them
 * in the binary.
 */
#define IRCD_RCSID(x)
#define IRCD_SCCSID(x)

#endif /* __NetBSD__ */

/*
 * Use this to mark a variable as defined, but unused.  This works when
 * a lot of functions take the same arguments (such as in a function
 * table) but some do not use all of them.
 */
#define IRCD_UNUSED(x)	(x) = (x);

#endif /* IRCD_CDEFS_H */
