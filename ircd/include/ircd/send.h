/*
 *   IRC - Internet Relay Chat
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	IRCD_SEND_H
#define IRCD_SEND_H

#include <stdarg.h>

void sendto_one(aClient *to, char *fmt, ...);
void vsendto_one(aClient *to, char *fmt, va_list ap);

void sendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
			   char *fmt, ...);
void vsendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
			    char *fmt, va_list ap);

void sendto_prefix_one(aClient *chptr, aClient *from, char *fmt, ...);
void vsendto_prefix_one(aClient *chptr, aClient *from, char *fmt, va_list ap);

void sendto_realops(char *fmt, ...);

void sendto_channel_butserv_unmask(aChannel *chptr, aClient *from,
				   char *fmt, ...);

void sendto_umode_except(int flags, int notflags, char *fmt, ...);

void sendto_locfailops(char *fmt, ...);

void sendto_channelops_butone(aClient *one, aClient *from, aChannel *chptr,
			      char *fmt, ...);

void sendto_channelvoices_butone(aClient *one, aClient *from, aChannel *chptr,
				 char *fmt, ...);
void sendto_serv_butone(aClient *one, char *fmt, ...);
void sendto_common_channels(aClient *user, char *fmt, ...);

void sendto_channel_butserv(aChannel *chptr, aClient *from, char *fmt, ...);

void sendto_match_servs(aChannel *chptr, aClient *from, char *fmt, ...);

void sendto_match_butone(aClient *one, aClient *from, char *mask, int what,
			 char *fmt, ...);

void sendto_ops(char *fmt, ...);

void sendto_failops(char *fmt, ...);

void sendto_umode(int flags, char *fmt, ...);

void sendto_failops_whoare_opers(char *fmt, ...);

void sendto_opers(char *fmt, ...);

void sendto_ops_butone(aClient *one, aClient *from, char *fmt, ...);

void sendto_opers_butone(aClient *one, aClient *from, char *fmt, ...);

#endif /* IRCD_SEND_H */
