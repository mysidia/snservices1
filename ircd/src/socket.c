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

#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>

IRCD_RCSID("$Id$");

int	_socket_fds = 0;
sock	**fds;
sock_address	*local = NULL;
sock_address	*local6 = NULL;
#ifdef HAVE_SSL
SSL_CTX	*ctx = NULL;
#endif

#ifdef SOL20
struct sockaddr_storage
{
	char	dummy[128];
};
#endif

/*
 * conf_fds is called when the configured maximum number of filedescriptors is
 * changed.
 */

static CONF_HANDLER(conf_fds)
{
	return CONFIG_OK;
}

/*
 * conf_address is called when default source address for IPv4 is changed.
 */

static CONF_HANDLER(conf_address)
{
	char	*s = config_get_string(n, NULL);
	if (s == NULL)
	{
		return CONFIG_BAD;
	}
	if (local != NULL)
	{
		address_free(local);
	}
	local = address_make(s, 0);
	return CONFIG_OK;
}

/*
 * conf_address6 is called when default source address for IPv6 is changed.
 */

static CONF_HANDLER(conf_address6)
{
	char	*s = config_get_string(n, NULL);
	if (s == NULL)
	{
		return CONFIG_BAD;
	}
	if (local6 != NULL)
	{
		address_free(local);
	}
	local6 = address_make(s, 0);
	return CONFIG_OK;
}

#ifdef HAVE_SSL
/*
 * conf_ssl is called when the SSL configuration is changed.
 */

static CONF_HANDLER(conf_ssl)
{
	char	*cert = config_get_string(n, "certificate");
	char	*key = config_get_string(n, "key");
	if (cert == NULL || key == NULL)
	{
		return CONFIG_BAD;
	}
	if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) != 1)
	{
		return CONFIG_BAD;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0)
	{
		return CONFIG_BAD; 
	}
	return CONFIG_OK;
}
#endif

/* 
 * socket_init initializes the socket functions. It must be called before
 * using any of the other functions, and should only be called once.
 *
 * maxfds     Specifies the number of file descriptors that should be
 *            supported. In a later version, 0 will mean that the limit will
 *            be adjusted dynamically.
 *
 * returns    0 in case of success, -1 in case of failure.
 */

int socket_init(int maxfds)
{
	int	n;
	_socket_fds = maxfds;

	if (maxfds < 1)
	{
		return -1;
	}
	fds = (sock **) irc_malloc(sizeof(sock *) * maxfds);
	if (fds == NULL)
	{
		return -1;
	}
	n = _socket_init();
	if (n != 0)
	{
		return -1;
	}
	config_monitor("network/maxfds", conf_fds, CONFIG_SINGLE);
	config_monitor("network/address", conf_address, CONFIG_SINGLE);
	config_monitor("network/address6", conf_address6, CONFIG_SINGLE);
#ifdef HAVE_SSL
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();
	ctx = SSL_CTX_new (SSLv23_method());
	if (!ctx)
	{
		return 0;
	}
	config_monitor("network/ssl", conf_ssl, CONFIG_SINGLE);
#endif
	return 0;
}

/* 
 * socket_error returns a an error message if the last socket_ call returned
 * an error status. In all other cases, its result will be undefined.
 *
 * sock       A valid sock structure.
 *
 * returns    Error message.
 */

char *socket_error(sock *sock)
{
#ifdef HAVE_SSL
	if (sock->sslstate == SSL_IDLE)
	{
#endif
		if (sock->err == 0)
		{
			return "Connection closed";
		}
		else
		{
			return strerror(sock->err);
		}
#ifdef HAVE_SSL
	}
	else
	{
		return ERR_error_string(sock->err, NULL);
	}
#endif
}

/*
 * _socket_free is an internal function that cleans up any memory used by
 * a sock structure. It doesn't close the connection the structure
 * represents, this is something the caller must do before calling.
 *
 * sock       A valid sock structure.
 */

void _socket_free(sock *sock)
{
	if (sock->laddr != NULL)
	{
		address_free(sock->laddr);
	}
	if (sock->raddr != NULL)
	{
		address_free(sock->raddr);
	}
#ifdef HAVE_SSL
	if (sock->sslstate != SSL_OFF)
	{
		SSL_free(sock->ssl);
	}
#endif
	irc_free(sock);
}

#ifdef HAVE_SSL

/*
 * _socket_checkssl is an internal function that evaluates SSL return codes and
 * changes the state of the socket accordingly.
 *
 * sock       A valid sock structure that has just called a SSL function.
 * err        Returncode from function.
 *
 * returns    >= 0 is ok, < 0 is error.
 */

int _socket_checkssl(sock *sock, int err)
{
	if (err > 0)
	{
		if (sock->write_handler)
		{
			_socket_monitor(sock->fd, MONITOR_WRITE);
		}
		else
		{
			_socket_unmonitor(sock->fd, MONITOR_WRITE);
		}
		if (sock->read_handler)
		{
			_socket_monitor(sock->fd, MONITOR_READ);
		}
		else
		{
			_socket_unmonitor(sock->fd, MONITOR_READ);
		}
		sock->sslstate = SSL_IDLE;
		return err;
	}
	switch((sock->err = SSL_get_error(sock->ssl, err)))
	{
		case SSL_ERROR_WANT_READ:
			if (sock->write_handler)
			{
				_socket_unmonitor(sock->fd, MONITOR_WRITE);
			}
			if (!sock->read_handler)
			{
				_socket_monitor(sock->fd, MONITOR_READ);
			}
			return 0;
		case SSL_ERROR_WANT_WRITE:
			if (sock->read_handler)
			{
				_socket_unmonitor(sock->fd, MONITOR_READ);
			}
			if (!sock->write_handler)
			{
				_socket_monitor(sock->fd, MONITOR_WRITE);
			}
			return 0;
		default:
			return -1;
	}
}
#endif

/*
 * socket_connect tries to establish a new connection.
 *
 * src        Source address to be used. Can be NULL to indicate that any
 *            local address is ok. If src is specified, it should be an
 *            address in the same address family as dst.
 * dst        Destination address.
 * flags      Supported flags are SOCKET_DGRAM, SOCKET_STREAM, SOCKET_SSL.
 *
 * returns    A valid sock structure, or NULL if the connection failed.
 */

sock *socket_connect(sock_address *src, sock_address *dst, int flags)
{
	sock	*s = (sock *) irc_malloc(sizeof(sock));
	struct sockaddr_storage	ss;
	int	ss_len = sizeof(ss);
	sock_address	sa;

	memset(s, 0, sizeof(sock));

#ifdef HAVE_SSL
	if (flags & SOCKET_SSL)
	{
		if (ctx == NULL)
		{
			_socket_free(s);
			return NULL;
		}
		s->sslstate = SSL_CONNECT;
	}
#endif

	s->flags = flags;
	if (flags & SOCKET_DGRAM)
	{
		s->fd = socket(dst->addr->sa_family,SOCK_DGRAM,0);
	}
	else
	{
		s->fd = socket(dst->addr->sa_family,SOCK_STREAM,0);
	}
	if (s->fd == -1)
	{
		_socket_free(s);
		return NULL;
	}
	fcntl(s->fd, F_SETFL, O_NONBLOCK);
	if (src == NULL)
	{
		switch (dst->addr->sa_family)
		{
			case AF_INET:
				src = local;
				break;
#ifdef AF_INET6
			case AF_INET6:
				src = local6;
				break;
#endif
		}
	}
	if (src != NULL)
	{
		if (bind(s->fd, src->addr, src->len))
		{
			close(s->fd);
			_socket_free(s);
			return NULL;
		}
	}
	if (connect(s->fd, dst->addr, dst->len) && errno != EINPROGRESS)
	{
		close(s->fd);
		_socket_free(s);
		return NULL;
	}
	s->raddr = address_copy(dst);
	if (getsockname(s->fd, (struct sockaddr *) &ss, &ss_len))
	{
		close(s->fd);
		_socket_free(s);
		return NULL;
	}
	sa.addr = (struct sockaddr *) &ss;
	sa.len = ss_len;

	s->laddr = address_copy(&sa);
	fds[s->fd] = s;

#ifdef HAVE_SSL
	if (s->sslstate == SSL_CONNECT)
	{
		s->ssl = SSL_new(ctx);
		SSL_set_fd (s->ssl, s->fd);
		_socket_monitor(s->fd, MONITOR_WRITE);
	}
#endif

	return s;
}

/*
 * socket_listen tries to open a new listener.
 *
 * src        Address on which the listener should listen.
 * flags      Supported flags are SOCKET_DGRAM, SOCKET_STREAM, SOCKET_SSL.
 *
 * returns    A valid sock structure, or NULL if the connection failed.
 */

sock *socket_listen(sock_address *src, int flags)
{
	sock	*s = (sock *) irc_malloc(sizeof(sock));
	memset(s, 0, sizeof(sock));

#ifdef HAVE_SSL
	if (flags & SOCKET_SSL)
	{
		if (ctx == NULL)
		{
			_socket_free(s);
			return NULL;
		}
		s->sslstate = SSL_IDLE;
	}
#endif
	s->flags = flags;

	if (flags & SOCKET_DGRAM)
	{
		s->fd = socket(src->addr->sa_family,SOCK_DGRAM,0);
	}
	else
	{
		s->fd = socket(src->addr->sa_family,SOCK_STREAM,0);
	}
	if (s->fd == -1)
	{
		_socket_free(s);
		return NULL;
	}
	fcntl(s->fd, F_SETFL, O_NONBLOCK);
	if (bind(s->fd, src->addr, src->len))
	{
		close(s->fd);
		_socket_free(s);
		return NULL;
	}
	s->laddr = address_copy(src);
	if (!(flags & SOCKET_DGRAM))
	{
		if (listen(s->fd, 100))
		{
			close(s->fd);
			_socket_free(s);
			return NULL;
		}
	}
	fds[s->fd] = s;

	return s;
}

/*
 * socket_accept accepts an incoming connection on a listening socket. The
 * resulting connection will be of the same type (stream, datagram, SSL) as
 * the listener.
 *
 * listensock The listening socket to be used.
 *
 * returns    A valid sock structure, or NULL if accepting the connection
 *            failed or there wasn't any connection to accept.
 */

sock *socket_accept(sock *listensock)
{
	sock	*s;
	struct sockaddr_storage	ss;
	int     ss_len = sizeof(ss);
	sock_address	sa;
	int	fd = accept(listensock->fd, (struct sockaddr *) &ss, &ss_len);

	if (fd < 0)
	{
		return NULL;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);
	s = (sock *) irc_malloc(sizeof(sock));
	memset(s, 0, sizeof(sock));
	s->fd = fd;
	s->raddr = (sock_address *) irc_malloc(sizeof(sock_address));
#ifdef AF_INET6
	if ((ss.ss_family == AF_INET6) && IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *) &ss)->sin6_addr))
	{
		s->raddr->addr = (struct sockaddr *) irc_malloc(sizeof(struct sockaddr_in));
		s->raddr->len = sizeof(struct sockaddr_in);
		s->raddr->addr->sa_family = AF_INET;
		((struct sockaddr_in *) s->raddr->addr)->sin_port = ((struct sockaddr_in6 *) &ss)->sin6_port;
		memcpy(&((struct sockaddr_in *) s->raddr->addr)->sin_addr,
			&((struct sockaddr_in6 *) &ss)->sin6_addr.s6_addr[12],
			sizeof(struct in_addr));		
	}
	else
	{
#endif
		s->raddr->addr = (struct sockaddr *) irc_malloc(ss_len);
		s->raddr->len = ss_len;
		memcpy(s->raddr->addr, &ss, ss_len);
#ifdef AF_INET6
	}
#endif
	if (getsockname(s->fd, (struct sockaddr *) &ss, &ss_len))
	{
		s->laddr = address_copy(listensock->laddr);
	}
	else
	{
                sa.addr = (struct sockaddr *) &ss;
                sa.len = ss_len;

                s->laddr = address_copy(&sa);
	}
	fds[s->fd] = s;

#ifdef HAVE_SSL
	if (listensock->sslstate == SSL_IDLE)
	{
		s->ssl = SSL_new(ctx);
		SSL_set_fd (s->ssl, s->fd);
		s->sslstate = SSL_ACCEPT;
		_socket_checkssl(s, SSL_accept(s->ssl));
	}
#endif

	return s;	
}

/*
 * socket_monitor can be used to register a callback function for specific
 * events on a socket. Registering a handler for the same type a second time
 * will result in the previous handler being discarded.
 *
 * sock       A valid sock structure.
 * type       Any combination of the following types: MONITOR_READ,
 *            MONITOR_WRITE, MONITOR_ERROR.
 * handler    The function to be called when any of the specified events
 *            occurs.
 */

void socket_monitor(sock *sock, int type, socket_handler handler)
{
	if (type & MONITOR_READ)
	{
		sock->read_handler = handler;
	}
	if (type & MONITOR_WRITE)
	{
		sock->write_handler = handler;
	}
	if (type & MONITOR_ERROR)
	{
		sock->error_handler = handler;
	}
#ifdef HAVE_SSL
	if (sock->sslstate == SSL_OFF || sock->sslstate == SSL_IDLE)
	{
#endif
		_socket_monitor(sock->fd, type);
#ifdef HAVE_SSL
	}
#endif
}

/*
 * socket_unmonitor can be used to deregister a previously registered callback
 * function for specific events on a socket.
 *
 * sock       A valid sock structure.
 * type       Any combination of the following types: MONITOR_READ,
 *            MONITOR_WRITE, MONITOR_ERROR.
 */

void socket_unmonitor(sock *sock, int type)
{
#ifdef HAVE_SSL
	if (sock->sslstate == SSL_OFF || sock->sslstate == SSL_IDLE)
	{
#endif
		_socket_unmonitor(sock->fd, type);
#ifdef HAVE_SSL
	}
#endif
	if (type & MONITOR_READ)
	{
		sock->read_handler = NULL;
	}
	if (type & MONITOR_WRITE)
	{
		sock->write_handler = NULL;
	}
	if (type & MONITOR_ERROR)
	{
		sock->error_handler = NULL;
	}
}

/*
 * socket_poll waits for events and handles them, using the previously
 * registered event handlers.
 *
 * timeout    Timeout in milliseconds. If no events are received after this
 *            time, the function will return.
 */

void socket_poll(int timeout)
{
	_socket_poll(timeout);
}

/*
 * socket_read tries to read data from a connected socket. When the given
 * socket is using SSL, it will transparently return the decrypted data.
 *
 * sock       A valid sock structure.
 * buffer     Pointer to buffer where read data should be stored.
 * len        Maximum number of bytes to read.
 *
 * returns    Number of bytes read when positive or zero. A negative value
 *            indicates an error.
 */

int socket_read(sock *sock, void *buffer, int len)
{
#ifdef HAVE_SSL
	if (sock->sslstate == SSL_OFF)
	{
#endif
		sock->err = read(sock->fd, buffer, len);
		if (sock->err > 0)
		{
			return sock->err;
		}
		else if (sock->err == 0)
		{
			return -1;
		}
		sock->err = errno;
		if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			return 0;
		}
		else
		{
			return -1;
		}
#ifdef HAVE_SSL
	}
	if (sock->sslstate != SSL_IDLE && sock->sslstate != SSL_READ)
	{
		return 0;
	}
	sock->sslstate = SSL_READ;
	return _socket_checkssl(sock, SSL_read(sock->ssl, buffer, len));
#endif
}

/*
 * socket_write tries to write data to a connected socket.  When the given
 * socket is using SSL, it will transparently encrypt the data.
 *
 * sock       A valid sock structure.
 * buffer     Pointer to data to be written.
 * len        Number of bytes to be written.
 *
 * returns    Number of bytes written when positive or zero. A negative value
 *            indicates an error.
 */

int socket_write(sock *sock, void *buffer, int len)
{
#ifdef HAVE_SSL
	if (sock->sslstate == SSL_OFF)
	{
#endif
		sock->err = write(sock->fd, buffer, len);
		if (sock->err == -1)
		{
			sock->err = errno;
			return -1;
		}
		return sock->err;
#ifdef HAVE_SSL
	}
	if (sock->sslstate != SSL_IDLE && sock->sslstate != SSL_WRITE)
	{
		return 0;
	}
	sock->sslstate = SSL_WRITE;
	return _socket_checkssl(sock, SSL_write(sock->ssl, buffer, len));
#endif
}

/* socket_close closes the connection and deallocates all memory used by the
 * socket. When it is called while handling events of itself, the cleanup is
 * delayed.
 *
 * sock       A valid sock structure.
 */

void socket_close(sock *sock)
{
#ifdef HAVE_SSL
	if (sock->ssl != NULL)
	{
		SSL_shutdown(sock->ssl);
	}
#endif
	socket_unmonitor(sock, MONITOR_READ|MONITOR_WRITE|MONITOR_ERROR);
	fds[sock->fd] = NULL;
	close(sock->fd);
	
	if (sock->flags & SOCKET_INUSE)
	{
		sock->flags |= SOCKET_CLOSE;
	}
	else
	{
		_socket_free(sock);
	}
}

/*
 * _socket_handle is an internal function called by _socket_poll
 * implementations to dispatch events.
 *
 * fd         Filedescriptor of socket.
 * type       Any combination of the following types: MONITOR_READ,
 *            MONITOR_WRITE, MONITOR_ERROR.
 */

void _socket_handle(int fd, int type)
{
	sock *sock = fds[fd];
	if (sock == NULL)
	{
		return;
	}
	sock->flags |= SOCKET_INUSE;
#ifdef HAVE_SSL
	switch (sock->sslstate)
	{
		case SSL_OFF:
		case SSL_IDLE:
#endif
			if ((type & MONITOR_READ) && sock->read_handler)
			{
				sock->read_handler(sock);
			}
			if ((type & MONITOR_WRITE) && sock->write_handler)
			{
				sock->write_handler(sock);
			}
#ifdef HAVE_SSL
			break;
		case SSL_ACCEPT:
			if (_socket_checkssl(sock, SSL_accept(sock->ssl)) < 0)
			{
				type |= MONITOR_ERROR;
			}			
			break;
		case SSL_CONNECT:
			if (_socket_checkssl(sock, SSL_connect(sock->ssl)) < 0)
			{
				type |= MONITOR_ERROR;
			}
			break;
		case SSL_READ:
			if (sock->read_handler)
			{
				sock->read_handler(sock);
			}
			break;
		case SSL_WRITE:
			if (sock->write_handler)
			{
				sock->write_handler(sock);
			}
			break;
	}
#endif
	if ((type & MONITOR_ERROR) && sock->error_handler)
	{
		sock->error_handler(sock);
	}
	if (sock->flags & SOCKET_CLOSE)
	{
		_socket_free(sock);
	}
	else
	{
		sock->flags &= ~SOCKET_INUSE;
	}
}
