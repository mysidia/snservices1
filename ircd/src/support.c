/************************************************************************
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

#include <stdio.h>
#include <signal.h>

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include <sys/socket.h>

#include "h.h"

#include "ircd/send.h"

IRCD_SCCSID("@(#)support.c	2.21 4/13/94 1990, 1991 Armin Gruner; 1992, 1993 Darren Reed");
IRCD_RCSID("$Id$");

int addr_cmp(const anAddress *a1, const anAddress *a2)
{
	if (a1->addr_family != a2->addr_family)
		return 1;
	switch (a1->addr_family)
	{
		case AF_INET:
			return bcmp(&a1->in.sin_addr, &a2->in.sin_addr, sizeof(struct in_addr));
#ifdef AF_INET6
		case AF_INET6:
			return bcmp(&a1->in6.sin6_addr, &a2->in6.sin6_addr, sizeof(struct in6_addr));
#endif
	}
	return 1;
}

/*
**	inetntoa  --	changed name to remove collision possibility and
**			so behaviour is gaurunteed to take a pointer arg.
**			-avalon 23/11/92
**	inet_ntoa --	returned the dotted notation of a given
**			internet number (some ULTRIX don't have this)
**			argv 11/90).
**	inet_ntoa --	its broken on some Ultrix/Dynix too. -avalon
*/

char *
inetntoa(const anAddress *addr)
{
	static	char	buf[40];
	u_char	*s;
	int	a,b,c,d;

	switch (addr->addr_family)
	{
		case AF_INET:
			s = (u_char *)&addr->in.sin_addr;
			a = (int)*s++;
			b = (int)*s++;
			c = (int)*s++;
			d = (int)*s++;
			(void) sprintf(buf, "%d.%d.%d.%d", a,b,c,d );
			break;
#ifdef AF_INET6
		case AF_INET6:
			sprintf(buf, "%x:%x:%x:%x:%x:%x:%x:%x",
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[0]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[2]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[4]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[6]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[8]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[10]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[12]),
				ntohs(*(short int *) &addr->in6.sin6_addr.s6_addr[14]));
			break;
#endif
	}

	return buf;
}

#ifdef NEED_INET_NETOF
/*
**	inet_netof --	return the net portion of an internet number
**			argv 11/90
**
*/

int
inet_netof(struct in_addr in)
{
    int addr = in.s_net;

    if (addr & 0x80 == 0)
	return ((int) in.s_net);

    if (addr & 0x40 == 0)
	return ((int) in.s_net * 256 + in.s_host);

    return ((int) in.s_net * 256 + in.s_host * 256 + in.s_lh);
}
#endif /* NEED_INET_NETOF */


#if defined(DEBUGMODE)
void
dumpcore(char *fmt, ...)
{
	va_list ap;
	va_list ap2;
	char * msg;
	static	time_t	lastd = 0;
	static	int	dumps = 0;
	char	corename[12];
	time_t	now = NOW;
	int	p;

	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (!lastd)
		lastd = now;
	else if (now - lastd < 60 && dumps > 2)
		(void)s_die(0);
	if (now - lastd > 60)
	    {
		lastd = now;
		dumps = 1;
	    }
	else
		dumps++;
	p = getpid();
	if (fork()>0) {
		kill(p, 3);
		kill(p, 9);
	}
	write_pidfile();
	(void)sprintf(corename, "core.%d", p);
	(void)rename("core", corename);
	Debug((DEBUG_FATAL, "Dumped core : core.%d", p));
	sendto_ops("Dumped core : core.%d", p);

	va_start(ap, fmt);
	va_copy(ap2, ap);
	Debug((DEBUG_FATAL, msg, ap2));
	va_end(ap2);
	sendto_ops(msg, ap);
	va_end(ap);
	(void)s_die(0);
}
#endif /* DEBUGMODE */

/*
** read a string terminated by \r or \n in from a fd
**
** Created: Sat Dec 12 06:29:58 EST 1992 by avalon
** Returns:
**	0 - EOF
**	-1 - error on read
**     >0 - number of bytes returned (<=num)
** After opening a fd, it is necessary to init dgets() by calling it as
**	dgets(x,y,0);
** to mark the buffer as being empty.
*/
int
dgets(int fd, char *buf, int num)
{
	static	char	dgbuf[8192];
	static	char	*head = dgbuf, *tail = dgbuf;
	char	*s, *t;
	int	n, nr;

	/*
	** Sanity checks.
	*/
	if (head == tail)
		*head = '\0';
	if (!num)
	    {
		head = tail = dgbuf;
		*head = '\0';
		return 0;
	    }
	if (num > sizeof(dgbuf) - 1)
		num = sizeof(dgbuf) - 1;
dgetsagain:
	if (head > dgbuf)
	    {
		for (nr = tail - head, s = head, t = dgbuf; nr > 0; nr--)
			*t++ = *s++;
		tail = t;
		head = dgbuf;
	    }
	/*
	** check input buffer for EOL and if present return string.
	*/
	if (head < tail &&
	    ((s = index(head, '\n')) || (s = index(head, '\r'))) && s < tail)
	    {
		n = MIN(s - head + 1, num);	/* at least 1 byte */
dgetsreturnbuf:
		bcopy(head, buf, n);
		head += n;
		if (head == tail)
			head = tail = dgbuf;
		return n;
	    }

	if (tail - head >= num)		/* dgets buf is big enough */
	    {
		n = num;
		goto dgetsreturnbuf;
	    }

	n = sizeof(dgbuf) - (tail - dgbuf) - 1;
	nr = read(fd, tail, n);
	if (nr == -1)
	    {
		head = tail = dgbuf;
		return -1;
	    }
	if (!nr)
	    {
		if (head < tail)
		    {
			n = MIN(tail - head, num);
			goto dgetsreturnbuf;
		    }
		head = tail = dgbuf;
		return 0;
	    }
	tail += nr;
	*tail = '\0';
	for (t = head; (s = index(t, '\n')); )
	    {
		if ((s > head) && (s > dgbuf))
		    {
			t = s-1;
			for (nr = 0; *t == '\\'; nr++)
				t--;
			if (nr & 1)
			    {
				t = s+1;
				s--;
				nr = tail - t;
				while (nr--)
					*s++ = *t++;
				tail -= 2;
				*tail = '\0';
			    }
			else
				s++;
		    }
		else
			s++;
		t = s;
	    }
	*tail = '\0';
	goto dgetsagain;
}
