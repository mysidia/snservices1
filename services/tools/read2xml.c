/**
 * \file read2xml.c
 * \brief Filter from old read2 stats format to XML
 *
 * \mysid
 * \date 2001, 17 November
 *
 * $Id$
 */

/*
 * Copyright (c) 2001 James Hess
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int getIter(char *s)
{
	int qq, rr;

	while(isspace(*s))
		s++;

	if ( sscanf(s, "(%2d) = %3d", &qq, &rr)  < 2 )
		abort();
	return qq;
}

int getVal(char *s)
{
	int qq, rr;

	while(isspace(*s))
		s++;

	sscanf(s, "(%2d) = %3d", &qq, &rr);
	return rr;
}

void do1Stats() {
	char buf[1024];
	time_t vD = 0;
	int nicks = 0, urls = 0, ace = 0, emails = 0;
	int qq = 0, rr = 0, ss = 0;
	int lH = 0;

	while(fgets(buf, 512, stdin))
	{
		buf[strlen(buf) - 1] = '\0';

		if (lH && *buf == '(') {
			char *p, *s;

			for(p = buf, s = strsep(&p, "*");
				s;
				s = strsep(&p, "*"))
			{
				if (lH == 1)
					printf("<nicklen i=\"%d\" val=\"%d\" />\n",
					       getIter(s), getVal(s));
				else
					printf("<explen i=\"%d\" val=\"%d\" />\n",
					       getIter(s), getVal(s));
			}
			continue;
		}

		if (lH) {
			if (lH == 1)
				printf("</nicklens>\n");
			else
				printf("</expirelens>\n");
		}


		if (strncmp(buf, "Date:", 5) == 0) {
			if (sscanf(buf, "Date: %ld", &vD) < 1)
				abort();
			continue;
		}

		if (strncmp(buf, "Nicks:", 6) == 0) {
			if (sscanf(buf, "Nicks: %d", &nicks) < 1)
				abort();
			continue;
		} else if (strncmp(buf, "Urls:", 4) == 0) {
			if (sscanf(buf, "Urls: %d", &urls) < 1)
				abort();
		} else if (strncmp(buf, "Access", 6) == 0) {
			if (sscanf(buf, "Access List Entries: %d", &ace) < 1)
				abort();
		} else if (strncmp(buf, "Emails:", 7) == 0) {
			if (sscanf(buf, "Emails: %d", &emails) < 1)
				abort();
			printf("<statsrun time=\"%d\">\n", vD);

			printf("<nickstats time=\"%ld\" total=\"%d\"\n"
			       " numurls=\"%d\" accitems=\"%d\""
			       " numemails=\"%d\">\n", 
				vD, nicks, urls, ace, emails);
		} else if (strncmp(buf, "E-mail", 6) == 0) {
			sscanf(buf, "E-mail addies           : %5d * Elected to set none    : %5d", &qq, &rr);
		}
		else if (strncmp(buf, "Negl", 4) == 0) {
			sscanf(buf, "Neglected e-mail field : %5d", &ss);
			printf(" <emails valid=\"%d\" none=\"%d\" />\n", qq, rr, qq - (rr + ss));
		}
		else if (strncmp(buf, "Largest Nick", 12) == 0) {
			sscanf(buf, "Largest Nick Length    : %5d * Average Nick Length    : %5d", &qq, &rr);
			printf(" <nicklength longest=\"%d\" average=\"%d\" />\n", qq, rr);
		}
		else if (strncmp(buf, "Largest URL", 11) == 0) {
			sscanf(buf, "Largest URL Length     : %5d * Average URL Length     : %5d", &qq, &rr);
			printf(" <urllength longest=\"%d\" average=\"%d\" />\n", qq, rr);
		}
		else if (strncmp(buf, "Largest Email", 13) == 0) {
			sscanf(buf, "Largest Email Length   : %5d * Average Email Length   : %5d", &qq, &rr);
			printf(" <emaillength longest=\"%d\" average=\"%d\" />\n", qq, rr);
		}
		else if (strncmp(buf, "Largest Realname", 16) == 0) {
			sscanf(buf, "Largest Realname Length: %5d * Average Realname Length: %5d", &qq, &rr);
			printf(" <reallen longest=\"%d\" average=\"%d\" />\n", qq, rr);
		}
		else if (strncmp(buf, "Largest Hostname", 16) == 0) {
			sscanf(buf, "Largest Hostname Length: %5d * Average Hostname Length: %5d", &qq, &rr);
			printf(" <hostlen longest=\"%d\" average=\"%d\" />\n", qq, rr);
		}
		else if (strncmp(buf, "Largest Username", 16) == 0) {
			sscanf(buf, "Largest Username Length: %5d * Average Username Length: %5d", &qq, &rr);
			printf(" <userlen longest=\"%d\" average=\"%d\" />\n", qq, rr);
		}
		else if (strncmp(buf, "Nick Lengths", 12) == 0) {
			printf("<nicklens>\n");
			lH = 1;
		}
		else if (strncmp(buf, "Days until", 10) == 0) {
			printf("<expirelens>\n");
			lH = 2;
		}

/*		printf("%s\n", buf); */
		if (strncmp(buf, "Held/", 5) == 0) {
			char *p, *s;

			for(p = buf+20, s = strsep(&p, " ");
				s;
				s = strsep(&p, " "))
			{
				printf("<heldnick>%s</heldnick>\n", s);
			}

			printf("</nickstats>\n</statsrun>\n");
			return;
		}
	}
	printf("</nickstats>\n");
}

int main()
{
	char buf[1024];

	printf("<stats time=\"%d\">\n", time(NULL));

	while(fgets(buf, 512, stdin))
	{
		buf[strlen(buf) - 1] = '\0';

		if (strcmp(buf, "===") == 0)
			do1Stats();
		else
			abort();
	}

	printf("</stats>\n", time(NULL));
}
