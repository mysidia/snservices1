/**
 * \file tool_cmd.c
 * \brief Tools: search for commands without help
 *
 * \mysid
 * \date Sep 2002
 *
 * $Id$
 */

/*
 * Copyright (c) 2002 James Hess
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "services.h"
#include "memoserv.h"
#include "queue.h"
#include "macro.h"
#include "nickserv.h"
#include "chanserv.h"
#include "infoserv.h"
#include "email.h"
#include "hash.h"
#include "clone.h"
#include "mass.h"
#include "db.h"
#include "log.h"
#include "interp.h"
#include <unistd.h>

//extern class cmd_name_table global_cmd_table[];

extern interp::service_cmd_t nickserv_commands[];
extern interp::service_cmd_t chanserv_commands[];
extern interp::service_cmd_t gameserv_commands[];
extern interp::service_cmd_t infoserv_commands[];
extern interp::service_cmd_t memoserv_commands[];
extern interp::service_cmd_t nickserv_commands[];
extern interp::service_cmd_t operserv_commands[];

void checks(char *sv, interp::service_cmd_t tab[])
{
	int i,j;
	char* buf, p[255];

	for(i = 0; tab[i].cmd; i++) {
		strncpyzt(p,tab[i].cmd,sizeof(p));
//		printf("%s\n", tab[i].cmd);
		buf = (char *) oalloc(strlen(HELP_PATH) + strlen(p) + 3 + strlen(sv)+10);
		chdir("..");
		sprintf(buf, "%s/%s/%s.help", HELP_PATH, sv, p);
		if ( access(buf, R_OK) == -1 ) {
			printf("Warning: can't read %s\n", buf);
		}
		free(buf);
		for(j=0;interp::global_cmd_table[j].cnum!=interp::SVC_CMD_NONE;j++) {
			if (interp::global_cmd_table[j].func==tab[i].func)
				break;
		}

		if (interp::global_cmd_table[j].cnum==SVC_CMD_NONE) {
			printf("Command %s/%s not in global command table.\n", sv, tab[i].cmd);
		}
	}
}

int main()
{
	int i;

	checks("nickserv", nickserv_commands);
	checks("chanserv", chanserv_commands);
	checks("memoserv", memoserv_commands);
	checks("gameserv", gameserv_commands);
	checks("infoserv", infoserv_commands);
	checks("operserv", operserv_commands);
}

/**************************************************************************/
