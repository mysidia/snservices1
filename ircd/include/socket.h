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

/*
 * This header defines the generic socket functions and structures used by
 * ircd.
 *
 * $Id$
 */

#ifndef SOCKET_H
#define SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "socket_addr.h"

#define SOCKET_HANDLER(name) void name(sock *sock)

typedef struct sock_t sock;
typedef void (*handler)(sock *);
struct sock_t
{
	int	fd;
	int	flags;
	sock_address	*laddr;
	sock_address	*raddr;
	int	err;
#ifdef HAVE_SSL
	SSL	*ssl;
	int	sslstate;
#endif
	handler	read_handler;
	handler	write_handler;
	handler	error_handler;
	void	*data;
};

#define MONITOR_READ	1
#define MONITOR_WRITE	2
#define MONITOR_ERROR	4

#define SOCKET_STREAM	0
#define SOCKET_DGRAM	1
#define SOCKET_SSL	2

#define SOCKET_CLOSE	(1 << 30)
#define SOCKET_INUSE	(1 << 31)

#define SSL_OFF		0
#define SSL_IDLE	1
#define SSL_ACCEPT	2
#define SSL_READ	3
#define SSL_WRITE	4
#define SSL_CONNECT	5
#define SSL_SHUTDOWN	6


int socket_init(int maxfds);
char *socket_error(sock *sock);

sock *socket_connect(sock_address *src, sock_address *dst, int flags);
sock *socket_listen(sock_address *src, int flags);
sock *socket_accept(sock *sock);
void socket_close(sock *sock);

void socket_monitor(sock *sock, int type, handler handler);
void socket_unmonitor(sock *sock, int type);
void socket_poll(int timeout);

int socket_read(sock *sock, void *buffer, int len);
int socket_write(sock *sock, void *buffer, int len);

int _socket_init();
void _socket_monitor(int fd, int type);
void _socket_unmonitor(int fd, int type);
void _socket_poll(int timeout);
void _socket_handle(int fd, int type);

extern int _socket_fds;
#endif
