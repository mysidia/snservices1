/*
 * Copyright (c) 2004, Onno Molenkamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ircd.h"

#include <netdb.h>
#ifndef HAVE_GETADDRINFO
#include "getaddrinfo.h"
#endif

IRCD_RCSID("$Id$");

/*
 * address_tostring returns an ASCII representation of an address.
 *
 * addr       Valid address structure.
 * port       0 = don't include port, 1 = include port
 *
 * returns    Pointer to static buffer containing the string.
 */

char *address_tostring(sock_address *addr, int port)
{
	static char	buf[46];
	struct sockaddr_in	*in;
#ifdef AF_INET6
	struct sockaddr_in6	*in6;
#endif

	switch (addr->addr->sa_family)
	{
		case AF_INET:
			in = (struct sockaddr_in *) addr->addr;
			sprintf(buf, "%d.%d.%d.%d",
				((u_char *) &in->sin_addr)[0],
				((u_char *) &in->sin_addr)[1],
				((u_char *) &in->sin_addr)[2],
				((u_char *) &in->sin_addr)[3]);
			return buf;
#ifdef AF_INET6
		case AF_INET6:
			in6 = (struct sockaddr_in6 *) addr->addr;
			sprintf(buf, "%x:%x:%x:%x:%x:%x:%x:%x",
				ntohs(*(short *) &in6->sin6_addr.s6_addr[0]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[2]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[4]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[6]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[8]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[10]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[12]),
				ntohs(*(short *) &in6->sin6_addr.s6_addr[14]));


			return buf;
#endif
	}
	return "unknown";
}

/*
 * address_compare compares two addresses. It ignores any differenes in the
 * port fields.
 *
 * addr1      Valid address structure.
 * addr2      Valid address structure.
 *
 * returns    0 = equal, !0 = not equal
 */

int address_compare(sock_address *addr1, sock_address *addr2)
{
	if (addr1->addr->sa_family != addr2->addr->sa_family)
	{
		return 1;
	}
	switch (addr1->addr->sa_family)
	{
		case AF_INET:
			return memcmp(&((struct sockaddr_in *)addr1->addr)->sin_addr, &((struct sockaddr_in *)addr2->addr)->sin_addr, sizeof(struct in_addr));
#ifdef AF_INET6
		case AF_INET6:
			return bcmp(&((struct sockaddr_in6 *)addr1->addr)->sin6_addr, &((struct sockaddr_in6 *)addr2->addr)->sin6_addr, sizeof(struct in6_addr));
#endif
	}
	return 1;
}

/*
 * address_copyto makes a copy of an address structure at the specified
 * memory address.
 *
 * src        Address structure to copy.
 * dst        Memory address to copy to.
 */

void address_copyto(sock_address *src, sock_address *dst)
{
	dst->addr = (struct sockaddr *) irc_malloc(src->len);
	memcpy(dst->addr, src->addr, src->len);
	dst->len = src->len;
}

/*
 * address_copy makes a copy of an address structure.
 *
 * src        Address structure to copy.
 *
 * returns    Copy of address structure.
 */

sock_address *address_copy(sock_address *src)
{
	sock_address *dst = irc_malloc(sizeof(sock_address));
	address_copyto(src, dst);
	return dst;
}

/*
 * address_free frees all memory used by a address structure.
 *
 * addr       Valid address structure.
 */

void address_free(sock_address *addr)
{
	irc_free(addr->addr);
	irc_free(addr);
}

/*
 * address_make creates an initialized address structure from an ASCII
 * representation of an address, and a portnumber.
 *
 * name       String containing the address to use.
 * port       Portnumber.
 *
 * returns    Valid sock structure in case of success, NULL otherwise.
 */

sock_address *address_make(char *name, int port)
{
	sock_address	*addr;
	struct addrinfo	*res;

	if (getaddrinfo(name, NULL, NULL, &res))
	{
		return NULL;
	}

	addr = (sock_address *) irc_malloc(sizeof(sock_address));
	addr->addr = (struct sockaddr *) irc_malloc(res->ai_addrlen);
	memcpy(addr->addr, res->ai_addr, res->ai_addrlen);
	addr->len = res->ai_addrlen;
	freeaddrinfo(res);

	switch (addr->addr->sa_family)
	{
		case AF_INET:
			((struct sockaddr_in *) addr->addr)->sin_port = htons(port);
			break;
#ifdef AF_INET6
		case AF_INET6:
			((struct sockaddr_in6 *) addr->addr)->sin6_port = htons(port);
			break;
#endif
	}

	return addr;	
}
