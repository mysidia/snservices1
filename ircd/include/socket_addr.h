/*
 *	$Id$
 */

#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

typedef struct sock_address_t sock_address;
struct sock_address_t
{
        struct sockaddr *addr;
        int     len;
};

char *address_tostring(sock_address *addr, int port);
sock_address *address_copy(sock_address *src);
void address_copyto(sock_address *src, sock_address *dst);
sock_address *address_make(char *name, int port);
void address_free(sock_address *addr);
int address_compare(sock_address *addr1, sock_address *addr2);

#endif
