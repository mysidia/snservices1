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

IRCD_RCSID("$Id$");

/*
 * network_accept_handler is called when a listener has received a new
 * connection.
 */

SOCKET_HANDLER(network_accept_handler)
{
	aClient	*cptr = (aClient *) sock->data;
	struct sock_t	*s;

	while ((s = socket_accept(sock)) != NULL)
	{
		add_connection(cptr, s);
	}
}

/*
 * network_connect_handler is called after completing/aborting an outgoing
 * connection.
 */

SOCKET_HANDLER(network_connect_handler)
{
	aClient	*cptr = (aClient *) sock->data;

	if (completed_connection(cptr))
	{
		exit_client(cptr, cptr, &me, socket_error(sock));
	}
}

/*
 * network_read_handler is called when a connection has received new data.
 */

SOCKET_HANDLER(network_read_handler)
{
	aClient	*cptr = (aClient *) sock->data;
	int	n;

	if ((n = read_packet(cptr)) < 0 && n != FLUSH_BUFFER)
	{
		exit_client(cptr, cptr, &me, socket_error(sock));
	}
}

/*
 * network_write_handler is called when a connection is ready to send data.
 */

SOCKET_HANDLER(network_write_handler)
{
	aClient	*cptr = (aClient *) sock->data;

	if (DoList(cptr) && IsSendable(cptr))
	{
		send_list(cptr, 32);
	}
	send_queued(cptr);
}

/*
 * network_error_handler is called when an error has occurred on a connection.
 */

SOCKET_HANDLER(network_error_handler)
{
	aClient	*cptr = (aClient *) sock->data;

	exit_client(cptr, cptr, &me, socket_error(sock));
}

/*
 * network_wait waits for any network activity or until the specified timeout.
 *
 * timeout    Time to wait in milliseconds.
 */

void network_wait(int timeout)
{
	socket_poll(timeout);
}

/*
 * network_listen opens a new network listener using the parameters in the
 * specified configuration item.
 *
 * aconf      Valid P-line ConfItem.
 *
 * returns    A socket structure in case of success, NULL otherwise.
 */

sock *network_listen(aConfItem *aconf)
{
	sock	*sock;
	sock_address	*addr;

	addr = address_make(aconf->host, aconf->port);
	if (addr == NULL)
	{
		return NULL;
	}

	sock = socket_listen(addr, SOCKET_STREAM | (aconf->name == NULL || strcmp(aconf->name, "yes") ? 0 : SOCKET_SSL));
	address_free(addr);
	if (sock == NULL)
	{
		return NULL;
	}

	socket_monitor(sock, MONITOR_READ, network_accept_handler);

	return sock;
}

/*
 * network_connect opens a new connection to a remote host using the
 * parameters in the specified configuration item.
 *
 * aconf      Valid C-line ConfItem.
 *
 * returns    A socket structure in case of success, NULL otherwise.
 */

sock *network_connect(aConfItem *aconf)
{
	sock	*sock;

	if (aconf->addr == NULL)
	{
		aconf->addr = address_make(aconf->host, (aconf->port > 0 ? aconf->port : portnum));
	}

	sock = socket_connect(NULL, aconf->addr, SOCKET_STREAM | (aconf->string4 != NULL && !strcmp(aconf->string4, "yes") ? SOCKET_SSL : 0));
	if (sock == NULL)
	{
		return NULL;
	}

	socket_monitor(sock, MONITOR_ERROR, network_error_handler);
	socket_monitor(sock, MONITOR_WRITE, network_connect_handler);

	return sock;
}
