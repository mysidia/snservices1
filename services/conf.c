/* $Id$ */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
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

/**
 * \file conf.c
 * Configuration file handler
 * \wd \taz \greg \mysid
 */


#include "services.h"
#include "hash.h"
#include "nickserv.h"
#include "memoserv.h"
#include "infoserv.h"
#include "db.h"
#include "log.h"

int svcOptFork = 0;

extern void makeSetterExcludedFromKlineMails(const char* nick);

/**
 * @brief Port number for services IPC server to listen on
 */
extern int ipcPort;

/* --------------------------------------------------------------------- */

/*!
 * \fn void readConf(void)
 * \brief Reads the services configuration file
 *
 * Called by services at bootup to to read the services configuration
 * file (services.conf) and initialize some of services' data such
 * as the SRA list
 *
 * \todo Redo the services.conf file format so that it looks nicer
 *       and is more tolerant about weird spacing.. allow for
 *       line splits.  More of the hard-coded options should be
 *       configurable.
 */
void readConf(void)
{
	/*
	 * read services.conf
	 */

	FILE *fp;
	char buffer[257];

	fp = fopen("services.conf", "r");

	if (fp == 0) {
		perror("Unable to open services.conf");
		sshutdown(-1);
	}

	while (sfgets(buffer, 256, fp) != NULL) {
		if (buffer[0] == '#') {
	} else if (!strncmp(buffer, "H:", 2))
		strcpy(myname, &(buffer[2]));
	  else if (!strncmp(buffer, "P:", 2))
		port = atoi(&(buffer[2]));
	  else if (!strncmp(buffer, "W:", 2))
		strcpy(mypass, &(buffer[2]));
	  else if (!strncmp(buffer, "C:", 2))
		strcpy(hostname, &(buffer[2]));
	  else if (!strncmp(buffer, "ON:", 3))
		strcpy(services[0].name, &(buffer[3]));
	  else if (!strncmp(buffer, "OU:", 3))
		strcpy(services[0].uname, &(buffer[3]));
	  else if (!strncmp(buffer, "OH:", 3))
		strcpy(services[0].host, &(buffer[3]));
	  else if (!strncmp(buffer, "OR:", 3))
		strcpy(services[0].rname, &(buffer[3]));
	  else if (!strncmp(buffer, "OM:", 3))
		strcpy(services[0].mode, &(buffer[3]));
	  else if (!strncmp(buffer, "NN:", 3))
		strcpy(services[1].name, &(buffer[3]));
	  else if (!strncmp(buffer, "NU:", 3))
		strcpy(services[1].uname, &(buffer[3]));
	  else if (!strncmp(buffer, "NH:", 3))
		strcpy(services[1].host, &(buffer[3]));
	  else if (!strncmp(buffer, "NR:", 3))
		strcpy(services[1].rname, &(buffer[3]));
	  else if (!strncmp(buffer, "NM:", 3))
		strcpy(services[1].mode, &(buffer[3]));
	  else if (!strncmp(buffer, "CN:", 3))
		strcpy(services[2].name, &(buffer[3]));
	  else if (!strncmp(buffer, "CU:", 3))
		strcpy(services[2].uname, &(buffer[3]));
	  else if (!strncmp(buffer, "CH:", 3))
		strcpy(services[2].host, &(buffer[3]));
	  else if (!strncmp(buffer, "CR:", 3))
		strcpy(services[2].rname, &(buffer[3]));
	  else if (!strncmp(buffer, "CM:", 3))
		strcpy(services[2].mode, &(buffer[3]));
	  else if (!strncmp(buffer, "MN:", 3))
		strcpy(services[3].name, &(buffer[3]));
	  else if (!strncmp(buffer, "MU:", 3))
		strcpy(services[3].uname, &(buffer[3]));
	  else if (!strncmp(buffer, "MH:", 3))
		strcpy(services[3].host, &(buffer[3]));
	  else if (!strncmp(buffer, "MR:", 3))
		strcpy(services[3].rname, &(buffer[3]));
	  else if (!strncmp(buffer, "MM:", 3))
		strcpy(services[3].mode, &(buffer[3]));
	  else if (!strncmp(buffer, "IN:", 3))
		strcpy(services[4].name, &(buffer[3]));
	  else if (!strncmp(buffer, "IU:", 3))
		strcpy(services[4].uname, &(buffer[3]));
	  else if (!strncmp(buffer, "IH:", 3))
		strcpy(services[4].host, &(buffer[3]));
	  else if (!strncmp(buffer, "IR:", 3))
		strcpy(services[4].rname, &(buffer[3]));
	  else if (!strncmp(buffer, "IM:", 3))
		strcpy(services[4].mode, &(buffer[3]));

	  else if (!strncmp(buffer, "AN:", 3))
		strcpy(services[6].name, &(buffer[3]));
	  else if (!strncmp(buffer, "AU:", 3))
		strcpy(services[6].uname, &(buffer[3]));
	  else if (!strncmp(buffer, "AH:", 3))
		strcpy(services[6].host, &(buffer[3]));
	  else if (!strncmp(buffer, "AR:", 3))
		strcpy(services[6].rname, &(buffer[3]));
	  else if (!strncmp(buffer, "AM:", 3))
		strcpy(services[6].mode, &(buffer[3]));
	else if (!strncmp(buffer, "GN:", 3))
		strcpy(services[5].name, &(buffer[3]));
	else if (!strncmp(buffer, "GU:", 3))
		strcpy(services[5].uname, &(buffer[3]));
	else if (!strncmp(buffer, "GH:", 3))
		strcpy(services[5].host, &(buffer[3]));
	else if (!strncmp(buffer, "GR:", 3))
		strcpy(services[5].rname, &(buffer[3]));
	else if (!strncmp(buffer, "GM:", 3))
		strcpy(services[5].mode, &(buffer[3]));
	else if (!strncmp(buffer, "OL:", 3))
		OpLimit = atoi(&(buffer[3]));
	else if (!strncmp(buffer, "BL:", 3))
		AkickLimit = atoi(&(buffer[3]));
	else if (!strncmp(buffer, "LL:", 3))
		AccessLimit = atoi(&(buffer[3]));
	else if (!strncmp(buffer, "CL:", 3))
		ChanLimit = atoi(&(buffer[3]));
	else if (!strncmp(buffer, "NL:", 3))
		NickLimit = atoi(&(buffer[3]));
	else if (!strncmp(buffer, "AKILLDONTMAIL:", 14)) {
		makeSetterExcludedFromKlineMails(buffer+14);
	}
	else if (!strncmp(buffer, "IPCPORT:", 8)) {
		if (ipcPort == 0)
			ipcPort = atoi(&(buffer[8]));
        }
	else if (!strncmp(buffer, "SRA:", 4)) {
		RegNickList *root;

		root = getRegNickData(&(buffer[4]));

		if (root) {
			root->flags |= NHOLD;
			root->opflags |= OROOT;
			delOpData(root);
			addOpData(root);
		}

	} else if (!strncmp(buffer, "REMSRA:", 7)) {
		RegNickList *root;
		root = getRegNickData(&(buffer[7]));
			if (root) {
				root->opflags |= OREMROOT;
				addOpData(root);
			}
		} else if (!strncmp(buffer, "-REMSRA:", 8)) {
			RegNickList *root;

			root = getRegNickData(&(buffer[8]));
			if (root) {
				root->opflags &= ~(OREMROOT);
				delOpData(root);
			}
		} else if (!strncmp(buffer, "SERVOP:", 7)) {
			RegNickList *servop;

			servop = getRegNickData(&(buffer[7]));
			if (servop) {
				servop->opflags |= OSERVOP;
				servop->opflags |=
					OPFLAG_DEFAULT | ORAKILL | OAKILL | OSETFLAG |
					ONBANDEL;
				servop->opflags |= OCBANDEL | OIGNORE | OLIST | OCLONE;
				servop->flags |= NHOLD;
				addOpData(servop);
			}
		} else if (!strncmp(buffer, "AKILL:", 6)) {
			RegNickList *kline;

			kline = getRegNickData(buffer + 6);

			if (kline) {
				kline->opflags |= OAKILL | OPFLAG_DEFAULT;
				kline->flags |= NHOLD;
				addOpData(kline);
			}
		}
	}
	fclose(fp);
}

/* --------------------------------------------------------------------- */

/* $Id$ */
