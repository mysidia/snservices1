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

#include <sys/time.h>
#include <sys/select.h>

IRCD_RCSID("$Id$");

fd_set	readfds;
fd_set	writefds;
fd_set	exceptfds;

fd_set	_readfds;
fd_set	_writefds;
fd_set	_exceptfds;

int _socket_init()
{
	if (_socket_fds > FD_SETSIZE)
	{
		return -1;
	}
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	return 0;
}

void _socket_monitor(int fd, int type)
{
	if (type & MONITOR_READ)
	{
		FD_SET(fd, &readfds);
	}
	if (type & MONITOR_WRITE)
	{
		FD_SET(fd, &writefds);
	}
	if (type & MONITOR_ERROR)
	{
		FD_SET(fd, &exceptfds);
	}
}

void _socket_unmonitor(int fd, int type)
{
	if (type & MONITOR_READ)
	{
		FD_CLR(fd, &readfds);
	}
	if (type & MONITOR_WRITE)
	{
		FD_CLR(fd, &writefds);
	}
	if (type & MONITOR_ERROR)
	{
		FD_CLR(fd, &exceptfds);
	}
}

void _socket_poll(int timeout)
{
	struct timeval	tv;
	int	i;
	int	n;
	int	type;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout - (tv.tv_sec * 1000)) * 1000;

	memcpy(&_readfds, &readfds, sizeof(fd_set));
	memcpy(&_writefds, &writefds, sizeof(fd_set));
	memcpy(&_exceptfds, &exceptfds, sizeof(fd_set));

	n = select(_socket_fds, &_readfds, &_writefds, &_exceptfds, &tv);

	for (i = 0; n > 0; i++)
	{
		type = 0;
		if (FD_ISSET(i, &_readfds))
		{
			type |= MONITOR_READ;
		}
		if (FD_ISSET(i, &_writefds))
		{
			type |= MONITOR_WRITE;
		}
		if (FD_ISSET(i, &_exceptfds))
		{
			type |= MONITOR_ERROR;
		}
		if (type > 0)
		{
			_socket_handle(i, type);
			n--;
		}
	}
}
