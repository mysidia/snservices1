/* $Id$ */

/* Copyright (c) 1998 Michael Graff <explorer@flame.org>
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
#include <string.h>
#include <unistd.h>

#include "helpfile.h"

flagmap_t if_flagmap[] = {
	{ "oper",    HELPFILE_FLAG_OPER,   HELPFILE_FLAG_OPER },
	{ "!oper",   0,                    HELPFILE_FLAG_OPER },
	{ "servop",  HELPFILE_FLAG_SERVOP, HELPFILE_FLAG_SERVOP },
	{ "!servop", 0,                    HELPFILE_FLAG_SERVOP },
	{ "sra",     HELPFILE_FLAG_SRA,    HELPFILE_FLAG_SRA },
	{ "!sra",    0,                    HELPFILE_FLAG_SRA },
	{ NULL, 0, 0 }
};

helpline_t::helpline_t(void)
{
	flags = 0;
	mask = 0;
	cmd = 0;
	text = NULL;
	next = NULL;
}

helpline_t::helpline_t(u_int32_t x_cmd, u_int32_t x_flags, u_int32_t x_mask,
		       char *x_text)
{
	flags = x_flags;
	mask = x_mask;
	cmd = x_cmd;
	next = NULL;

	if (x_text == NULL || *x_text == '\0')
		text = NULL;
	else
		text = strdup(x_text);
}

helpline_t::~helpline_t()
{
	if (text != NULL)
		delete text;
}

void
helpline_t::delchain(void)
{
	helpline_t *hl;
	helpline_t *hl2;

	hl = this;

	while (hl != NULL) {
		hl2 = hl;
		hl = hl->get_next();
		delete hl2;
	}
}

helpfile_t::helpfile_t(void)
{
	first = NULL;
	last = NULL;
	fname = NULL;
}

helpfile_t::~helpfile_t()
{
	if (first != NULL)
		first->delchain();
}

void
helpfile_t::addline(u_int32_t x_cmd, u_int32_t x_flags,
			     u_int32_t x_mask, char *x_text)
{
	helpline_t *hl;

	hl = new helpline_t(x_cmd, x_flags, x_mask, x_text);

	if (first == NULL) {
		last = hl;
		first = hl;

		return;
	}

	last->set_next(hl);
	last = hl;
}

void
helpfile_t::display(u_int32_t flags)
{
	helpline_t *hl;
	int last_blank;

	last_blank = 0;

	for (hl = first ; hl != NULL ; hl = hl->get_next()) {
		if ((hl->get_mask() & flags) != hl->get_flags())
			continue;

		switch (hl->get_cmd()) {
		case HELPFILE_CMD_PRINTF:
			if (hl->get_text() == NULL) {
				last_blank = 1;
			} else {
				if (last_blank == 1) {
					printf("\n");
					last_blank = 0;
				}
				printf("%s\n", hl->get_text());
				last_blank = 0;
			}
			break;
		case HELPFILE_CMD_EXIT:
			if (hl->get_text() != NULL) {
				if (last_blank == 1) {
					printf("\n");
					last_blank = 0;
				}
				printf("exit: %s\n", hl->get_text());
			}
			return;
			break;
		}
	}
}

void
helpfile_t::readfile(char *x_fname)
{
	int                i;
	FILE              *f;
	static char        inbuf[512];
	char              *p;
	int		   flag_change;
	u_int32_t          flags0;
	u_int32_t          mask0;
	u_int32_t          flags1;
	u_int32_t          mask1;

	f = fopen(x_fname, "r");
	if (f == NULL)
		return;

	i = 0;
	flags0 = flags1 = 0;
	mask0 = mask1 = 0;
	flag_change = 1;

	for (;;) {
		if (fgets(inbuf, sizeof(inbuf), f) == 0)
			break;

		inbuf[strlen(inbuf) - 1] = '\0';  // kill \n
		if (inbuf[0] == '\0' && flag_change == 0)
			continue;

		if (inbuf[0] == '.') {
			if (strncmp(inbuf + 1, "if", 2) == 0) {
				mask0 = mask1;
				flags0 = flags1;
				flag_change = 1;
				helpfile_parse_if(&flags1, &mask1, inbuf + 4);
			} else if (strncmp(inbuf + 1, "endif", 5) == 0) {
				mask1 = mask0;
				flags1 = flags0;
				flag_change = 1;
			} else if (strncmp(inbuf + 1, "exit", 4) == 0) {
				//
				// Strip off the "exit" part, and trailing
				// spaces.
				//
				p = inbuf + 5;

				while (*p == ' ')
					p++;

				if (*p == '\0')
					p = NULL;

				addline(HELPFILE_CMD_EXIT, flags1, mask1, p);
				i++;
			} else if (strncmp(inbuf + 1, "#", 1) == 0) {
				continue;
			} else {
				fprintf(stderr, "%s not understood\n", inbuf);
				exit(1);
			}

			continue;
		}

		addline(HELPFILE_CMD_PRINTF, flags1, mask1, inbuf);
		i++;
	}
	
	fclose(f);
	fname = strdup(x_fname);
}


void
helpfile_parse_if(u_int32_t *flags, u_int32_t *mask, char *statement)
{
	flagmap_t *cmds = NULL;
	char      *line;
	char      *cline;

	line = statement;

	cline = strsep(&line, " ");

	while (cline != NULL) {
		if (*cline == '\0')
			goto nextline;

		cmds = if_flagmap;
		while (cmds->name != NULL) {
			if (strcmp(cline, cmds->name) == 0) {
				*flags |= cmds->flags;
				*mask |= cmds->mask;
				break;
			}
			cmds++;
		}
	nextline:
		cline = strsep(&line, " ");
	}

	if (cmds->name == NULL) {
		fprintf(stderr, "if statement %s not understood\n", statement);
		exit(1);
	}
}

#ifdef TEST_HELPFILE

int
main(int argc, char **argv)
{
	helpfile_t hf;
	u_int32_t  flags;
	u_int32_t  mask;
	int        i;

	if (argc < 2) {
		fprintf(stderr, "Usage:  %s infile.help [flag [flag...]]\n",
			argv[0]);
		exit(1);
	}

	flags = 0;
	mask = 0;

	for (i = 2 ; i < argc ; i++)
		helpfile_parse_if(&flags, &mask, argv[i]);

	hf.readfile(argv[1]);

	hf.display(flags);

	return 0;
}
#endif /* TEST_HELPFILE */
