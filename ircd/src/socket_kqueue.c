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
#include <sys/event.h>

IRCD_RCSID("$Id$");

struct kevent	*changelist;
int	*readchange;
int	*writechange;
int	changes = 0;
int	queue;

int _socket_init()
{
	changelist = irc_malloc(sizeof(struct kevent) * _socket_fds * 2);
	readchange = irc_malloc(sizeof(int) * _socket_fds);
	writechange = irc_malloc(sizeof(int) * _socket_fds);

	if (changelist == NULL || readchange == NULL || writechange == NULL)
	{
		return -1;
	}

	memset(readchange, -1, sizeof(int) * _socket_fds);
	memset(writechange, -1, sizeof(int) * _socket_fds);

	if ((queue = kqueue()) < 0)
	{
		return -1;
	}		
	return 0;		   
}

void _socket_monitor(int fd, int type)
{
	if (type & MONITOR_READ)
	{
		if (readchange[fd] == -1)
		{
			EV_SET(&changelist[changes], fd, EVFILT_READ, EV_ADD, 0, 0, 0);
			readchange[fd] = changes++;
		}
		else
		{
			changelist[readchange[fd]].flags = EV_ADD;
		}
	}
	if (type & MONITOR_WRITE)
	{
		if (writechange[fd] == -1)
		{
			EV_SET(&changelist[changes], fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
			writechange[fd] = changes++;
		}
		else
		{
			changelist[writechange[fd]].flags = EV_ADD;
		}
	}
}

void _socket_unmonitor(int fd, int type)
{
	if (type & MONITOR_READ)
	{
		if (readchange[fd] == -1)
		{
			EV_SET(&changelist[changes], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
			readchange[fd] = changes++;
		}
		else
		{
			changelist[readchange[fd]].flags = EV_DELETE;
		}
	}
	if (type & MONITOR_WRITE)
	{
		if (writechange[fd] == -1)
		{
			EV_SET(&changelist[changes], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
			writechange[fd] = changes++;
		}
		else
		{
			changelist[writechange[fd]].flags = EV_DELETE;
		}
	}
}

void _socket_poll(int timeout)
{
	int	n;
	struct kevent	*ke = &changelist[0];
	struct timespec	ts = {timeout / 1000, (timeout % 1000) * 1000000};

	memset(readchange, -1, sizeof(int) * _socket_fds);
	memset(writechange, -1, sizeof(int) * _socket_fds);

	n = kevent(queue, &changelist[0], changes, &changelist[0], _socket_fds * 2, &ts);
	changes = 0;
	     
	while (n > 0)
	{
		if ((ke->flags & EV_ERROR) && (ke->data != 2))
		{
			_socket_handle(ke->ident, MONITOR_ERROR);
		}
		else if (ke->filter == EVFILT_READ)
		{
			_socket_handle(ke->ident, MONITOR_READ);
		}
		else if (ke->filter == EVFILT_WRITE)
		{
			_socket_handle(ke->ident, MONITOR_WRITE);
		}
		n--;
		ke++;
	}
}
