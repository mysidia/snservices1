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
#include <sys/epoll.h>
#include <string.h>

IRCD_RCSID("$Id$");

struct epoll_event	*resultfds;
int	*events;
int	epollfd;

int _socket_init()
{
	resultfds = irc_malloc(sizeof(struct epoll_event) * _socket_fds);
	events = irc_malloc(sizeof(int) * _socket_fds);
	if (resultfds == NULL || events == NULL)
	{
		return -1;
	}
	memset(events, 0, sizeof(int) * _socket_fds);
	if ((epollfd = epoll_create(_socket_fds)) < 0)
	{
		return -1;
	}
	return 0;		   
}

void _socket_monitor(int fd, int type)
{
	struct epoll_event change;
	change.data.fd = fd;
	change.events = events[fd];
	if (type & MONITOR_READ)
	{
		change.events |= EPOLLIN;
	}
	if (type & MONITOR_WRITE)
	{
		change.events |= EPOLLOUT;
	}
	if (events[fd] == 0)
	{
		epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &change);
	}
	else if (events[fd] != change.events)
	{
		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &change);
	}
	events[fd] = change.events;
}

void _socket_unmonitor(int fd, int type)
{
	struct epoll_event change;
	change.data.fd = fd;
	change.events = events[fd];
	if (type & MONITOR_READ)
	{
		change.events &= ~EPOLLIN;
	}
	if (type & MONITOR_WRITE)
	{
		change.events &= ~EPOLLOUT;
	}
	if (change.events == 0)
	{
		epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &change);
	}
	else if (events[fd] != change.events)
	{
		epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &change);
	}
	events[fd] = change.events;
}

void _socket_poll(int timeout)
{
	int	n;
	int	type;
	struct epoll_event	*p = &resultfds[0];

	n = epoll_wait(epollfd, p, _socket_fds, timeout);

	while (n > 0)
	{
		type = 0;
		if (p->events & EPOLLIN)
		{
			type |= MONITOR_READ;
		}
		if (p->events & EPOLLOUT)
		{
			type |= MONITOR_WRITE;
		}
		if (p->events & ~(EPOLLIN|EPOLLOUT))
		{
			type |= MONITOR_ERROR;
		}
		_socket_handle(p->data.fd, type);
		n--;
		p++;
	}
}
