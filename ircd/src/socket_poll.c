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

#include <sys/poll.h>

IRCD_RCSID("$Id$");

struct pollfd *pollfds;

int _socket_init()
{
	int i;
	pollfds = irc_malloc(sizeof(struct pollfd) * _socket_fds);
	if (pollfds == NULL)
	{
		return -1;
	}
	for (i = 0; i<_socket_fds; i++)
	{
		pollfds[i].fd = -1;
		pollfds[i].events = 0;
		pollfds[i].revents = 0;
	}
	return 0;
}

void _socket_monitor(int fd, int type)
{
	pollfds[fd].fd = fd;
	if (type & MONITOR_READ)
	{
		pollfds[fd].events |= POLLIN;
	}
	if (type & MONITOR_WRITE)
	{
		pollfds[fd].events |= POLLOUT;
	}
}

void _socket_unmonitor(int fd, int type)
{
	if (type & MONITOR_READ)
	{
		pollfds[fd].events &= ~POLLIN;
	}
	if (type & MONITOR_WRITE)
	{
		pollfds[fd].events &= ~POLLOUT;
	}
	if (pollfds[fd].events == 0)
	{
		pollfds[fd].fd = -1;
	}
}

void _socket_poll(int timeout)
{
	int n = poll(&pollfds[0], _socket_fds, timeout);
	struct pollfd *p = &pollfds[0];
	int type;

	while (n > 0)
	{
		if (p->revents)
		{
			type = 0;
			if (p->revents & POLLIN)
			{
				type |= MONITOR_READ;
			}
			if (p->revents & POLLOUT)
			{
				type |= MONITOR_WRITE;
			}
			if (p->revents & ~(POLLIN|POLLOUT))
			{
				type |= MONITOR_ERROR;
			}
			_socket_handle(p->fd, type);
			n--;
		}
		p++;
	}
}
