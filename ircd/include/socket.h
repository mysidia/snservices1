/*
 *	$Id$
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
