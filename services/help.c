/**
 * \file help.c
 * \brief Implementation of services help
 *
 * Procedures related to services help system
 *
 * \wd \taz \greg
 * \date 1996-1997
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "services.h"
#include "macro.h"

	 /// First item in the help cache
help_cache *firsthelpcache;

	 /// Last item in the help cache
help_cache *lasthelpcache;

/**
 * \brief Print the services message of the day
 * \param to Nick to send the MOTD to
 * \pre To points to a valid NUL-terminated character array containing
 *      the nickname of a user for IRC messages containing the services
 *      motd to be sent to.
 */
void
motd(char *to)
{
	FILE *fp;
	char buffer[81];
	fp = fopen("services.motd", "r");
	if (fp == NULL) {
		sSend(":%s 422 %s :No MOTD File found", myname, to);
		return;
	}

	sSend(":%s 375 %s :SorceryNet Services Data:", myname, to);

	while (fgets(buffer, 81, fp)) {
		sSend(":%s 372 %s :%s", myname, to, buffer);
	}
	sSend
		(":%s 372 %s :# Most users online at one time: %lu     Registered nicks: %lu",
		 myname, to, mostusers + 5, mostnicks);
	/* Liam: why the +5 ? nickserv, memo, chan, oper, game, info = 6;
	 * this count also shouldn't include enforcers really.
	 * Not that it matters much.
	 */
	sSend(":%s 372 %s :# Registered channels: %lu", myname, to, mostchans);
	sSend
		(":%s 372 %s :#####################################################################",
		 myname, to);
	sSend(":%s 376 %s :End of services MOTD", myname, to);

	fclose(fp);

}

/**
 * \brief Handle a user's help request
 * \param to User requesting help
 * \param service Which service is help being requested of
 * \param args args[] array specifying help data to retrieve
 * \param numargs Number of arguments in array
 */
void
help(char *to, char *service, char **args, int numargs)
{
	int i = 1;
	off_t topoff;
	size_t totallen = 0;
	char buffer[sizeof(HELP_PATH) + HELPTOPICBUF + NICKLEN + 255], *p;
	FILE *fp;
	help_line *helpline;
	help_cache *helpcache;

#if (HELPTOPICBUF > 100)
	/* compile-time sanity check -- Liam*/
	
	you need to run configure and recompile;
#endif

	/* If you configured services with a really long help path, 
	 * we could overflow buffer, since MAX_PATH is often 4096 these
	 * days, and IRCBUF is likely 512 or 513 - Liam
	 */
	if (sizeof(HELP_PATH) + strlen(service) + HELPTOPICBUF >= IRCBUF) {
		sSend(":%s NOTICE %s :HELP_PATH is too long!", service, to);
		return;
	}

	sprintf(buffer, "%s/%s/", HELP_PATH, service);
	topoff = strlen(buffer);

	if (numargs > 5) {
		sSend(":%s NOTICE %s :Too Many Arguments", service, to);
		return;
	}

	if (numargs <= 1)
		strcat(buffer, "index");
	else {
		totallen = 0;

		for (; i < numargs; i++) {
			if ((strlen(args[i]) + totallen) > HELPTOPICBUF) {
				sSend
					(":%s NOTICE %s :Sorry, but the maximum length of a help command line is %d characters.",
					 service, to, HELPTOPICBUF);
				if (*(buffer + topoff))
					sSend(":%s NOTICE %s :No help was available on \"%s\"",
						  service, to, buffer + topoff);
				return;
			}
			if (i > 1)
				strcat(buffer, "-");
			strcat(buffer, args[i]);
			totallen += (strlen(args[i]) + 1);
		}
	}

	for (p = buffer + topoff; *p; p++)
		if (*p == '.' || *p == '/' || iscntrl(*p) || !isprint(*p))
			*p = '_';
	for (p = buffer; *p; p++)
		(*p) = (tolower(*p));

	helpcache = check_help_cache(buffer);

	if (helpcache != NULL) {	/* Woohoo! It's cached */
		for (helpline = helpcache->first->next; helpline;
			 helpline =
			 helpline->next) sSend(":%s NOTICE %s :%s", service, to,
								   helpline->line);
		return;
	}

	{
		char *fopenbuf = (char *)oalloc(strlen(buffer) + sizeof ".help" + 1);

		strcpy(fopenbuf, buffer);
		strcat(fopenbuf, ".help");

		fp = fopen(fopenbuf, "r");
		FREE(fopenbuf);
	}

	if (fp == NULL) {
		if (numargs != 0) {
			buffer[0] = 0;
			parse_str(args, numargs, 1, buffer, IRCBUF);
			sSend(":%s NOTICE %s :No help is available about \"%s\"",
				  service, to, buffer);
		} else {
			sSend(":%s NOTICE %s :Helpfiles for %s are still "
			      "in development", service, to, service);
		}
		return;
	}

	/* Okay -- the file isn't cached.. we'll do it here */
	helpcache = (help_cache *) oalloc(sizeof(help_cache));
	if (firsthelpcache == NULL)
		firsthelpcache = helpcache;
	if (lasthelpcache != NULL)
		lasthelpcache->next = helpcache;
	lasthelpcache = helpcache;
	helpcache->name = (char *)oalloc(strlen(buffer) + 1);
	strcpy(helpcache->name, buffer);

	helpcache->first = helpline = (help_line *) oalloc(sizeof(help_line));

	while (fgets(buffer, 80, fp)) {
		if (!buffer[0])
			continue;
		buffer[strlen(buffer) - 1] = '\0';

		if (!buffer[0]) {
			buffer[0] = ' ';
			buffer[1] = '\0';
		}

		helpline->next = (help_line *) oalloc(sizeof(help_line));
		helpline = helpline->next;
		sSend(":%s NOTICE %s :%s", service, to, buffer);
		strcpy(helpline->line, buffer);	/* This is safe -- limited to 80 already */
		/* safe but a little ineffient of memory;
		 * better to allocate the strings as needed, and not
		 * to allocate one more than we need each time. - Liam
		 * */
	}

	fclose(fp);
}

/**
 * \brief Check if a help item is in the cache if so return it, else
 *        return NULL.
 * \param filename What help topic filename to look for, already force
 * to lower case.
 */
help_cache *
check_help_cache(char *filename)
{
	help_cache *t_cache;

	for (t_cache = firsthelpcache; t_cache; t_cache = t_cache->next) {
		if (strcmp(t_cache->name, filename) == 0) {
			return t_cache;
		}
	}

	return NULL;
}

/**
 * \brief Flush out the help cache
 */
void
flush_help_cache(void)
{
	help_cache *t_cache;
	help_cache *t_cache2;
	help_line *t_line;
	help_line *t_line2;

	for (t_cache = firsthelpcache; t_cache;) {
		if (t_cache == NULL)
			return;
		t_cache2 = t_cache->next;
		for (t_line = t_cache->first; t_line;) {
			t_line2 = t_line->next;
			FREE(t_line);
			t_line = t_line2;
		}

		FREE(t_cache->name);
		FREE(t_cache);
		t_cache = t_cache2;
	}
	firsthelpcache = NULL;
	lasthelpcache = NULL;
}
