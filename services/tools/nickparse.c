/* $Id$ */

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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS `AS IS'' AND
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

#include "../services.h"

void main(int, char **);
char *sfgets(char *, int, FILE *);

void main(int argc, char **argv)
{
	char line[2048];
	char *tmp = line;
	char args[15][307];
	int done = 0;
	int x,a;
	FILE *ns;
	int i = 0;
#ifndef NSDATAFILE
	if (!argv[1])
	  ns = fopen("../nickserv/nickserv.db", "r");
	else
	  ns = fopen(argv[1], "r");
#else
	ns = fopen(NSDATAFILE, "r");
#endif
#ifndef NSDATAFILE
        if(!ns) {
                if (argv[1])
                        printf("Failed in opening %s\n", argv[1]);
                else
                        printf("Failed in opening nickserv/nickserv.db\n");
                return;
        }
#else
	if(!ns) {
		printf("Failed in opening: %s\n", NSDATAFILE);
	}
#endif
	while(!done) {
		i++;
		if (!(sfgets(line, 2048, ns))) {
			done = 1;
			fclose(ns);
			return;
 		}
		tmp = line;
		x = a = 0;
		while(*tmp && x < 11) {
			if((x == 4 || x == 14) && *tmp == ':') {
				tmp++;
				while(*tmp) {
					args[x][a] = *tmp;
					a++;
					tmp++;
				}
			}
			else {
				while(*tmp != ' ' && *tmp) {
					args[x][a] = *tmp;
					a++;
					tmp++;
				}
			}
			
			args[x][a] = 0;
			x++;
			while(*tmp == ' ')
			  tmp++;
			a = 0;
		}
		
		if(!strcmp(args[0], "nick")) {
			mostnicks++;
		}
                else if(!strcmp(args[0], "url")) {
                }
                else if(!strcmp(args[0], "access")) {
                }
		else if(!strcmp(args[0], "done"))
		  done = 1;
		else
		  printf("False line (line %i): %s\n", i, line);
		
	}
	printf("Completed - Read %lu nicks\n", mostnicks);
	fclose(ns);
}

char *
sfgets(char *str, int len, FILE *fp)
{
        if(!fgets(str, len, fp))
          return NULL;
        else {
                if(str[0])
                  str[strlen(str) - 1] = 0;
                return str;
        }
}
