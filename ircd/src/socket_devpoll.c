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

#include <sys/fcntl.h>
#include <sys/devpoll.h>

IRCD_RCSID("$Id$");

struct pollfd	*pollfds;
struct pollfd	*resultfds;
struct dvpoll	pollopts;

int	devpoll;

int _socket_init()
{
	int	i;
	pollfds = irc_malloc(sizeof(struct pollfd) * _socket_fds);
	resultfds = irc_malloc(sizeof(struct pollfd) * _socket_fds);
	if (pollfds == NULL || resultfds == NULL)
	{
		return -1;
	}
	pollopts.dp_fds = resultfds;
	pollopts.dp_nfds = _socket_fds;

	for (i = 0; i<_socket_fds; i++)
	{
		pollfds[i].fd = i;
		pollfds[i].events = 0;
		pollfds[i].revents = 0;
	}
	if ((devpoll = open("/dev/poll", O_RDWR)) < 0)
	{
		return -1;
	}
	return 0;		   
}

void _socket_monitor(int fd, int type)
{
	int	events = pollfds[fd].events;
	if (type & MONITOR_READ)
	{
		pollfds[fd].events |= POLLIN;
	}
	if (type & MONITOR_WRITE)
	{
		pollfds[fd].events |= POLLOUT;
	}
	if (events != pollfds[fd].events)
	{
		write(devpoll, &pollfds[fd], sizeof(struct pollfd));
	}
}

void _socket_unmonitor(int fd, int type)
{
	int	events = pollfds[fd].events;
	if (type & MONITOR_READ)
	{
		pollfds[fd].events &= ~POLLIN;
	}
	if (type & MONITOR_WRITE)
	{
		pollfds[fd].events &= ~POLLOUT;
	}
	if (events != pollfds[fd].events)
	{
		if (pollfds[fd].events == 0)
		{
			pollfds[fd].events = POLLREMOVE;
		}
		write(devpoll, &pollfds[fd], sizeof(struct pollfd));
	}
}

void _socket_poll(int timeout)
{
	int	n;
	int	type;
	struct pollfd	*p = resultfds;
	
	pollopts.dp_timeout = timeout;
	n = ioctl(devpoll, DP_POLL, &pollopts);

	while (n > 0)
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
		p++;
	}
}
