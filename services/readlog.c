/* Readlog.c */
/* Sample procedure for parsing the logs back in and sending them */
/* Out in a format that resembles (slightly) the old formatting */

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
#include <time.h>

#define LOGF_NORMAL     0x0
#define LOGF_BADPW      0x1
#define LOGF_FAIL       0x2
#define LOGF_OFF        0x4
#define LOGF_ON         0x8
#define LOGF_OPER       0x10
#define LOGF_PATTERN    0x20
#define LOGF_SCAN       0x40
#define LOGF_OK		0x80

char *log_interpret(char *inbuf)
{
  static char res[8192];
  char cmd[255], sender[255], target[255], tb[255], flbuf[255] = "";
  time_t ts;
  int fl, i, x;

  *target = '\0';
  if ( sscanf(inbuf, "%s %ld %s %d %s", cmd, &ts, sender, &fl, target) < 4 ) {
     printf("ERROR: INVALID LOG FORMAT\n");
     return "";
  }
  for(i = 0, x = 0; inbuf[i]; i++)
      if (inbuf[i] == ' ' && (++x > 4))
          break;
  sprintf(flbuf, "%s%s%s%s%s%s",
          fl & LOGF_BADPW ? " (Bad PW)" : "",
          fl & LOGF_FAIL  ? " (Failed)" : "",
          fl & LOGF_ON    ? " (Applied)" : "",
          fl & LOGF_OFF   ? " (Removed)" : "",
          fl & LOGF_OPER  ? " (Oper Cmd)" : "",
          fl & LOGF_SCAN  ? " (Listing)" : "" );

  strftime(tb, 254, "%d-%b-%y %H:%M", gmtime(&ts));
  sprintf(res, "(%s) (%s) %s %s%s %s", tb, sender, cmd, target, flbuf, inbuf + i + 1);
  return res;

//NS_SETOP 982397116 Mysidia!root@albert2.i-55.com 0 n +! (+OokKpsSNCIGrjL!ca)
}

void main()
{
   FILE *fp;
   char buf[2048];
   int i = 0;
/*   const char *whatlog[] = {
      "operserv/operserv.log",
      "chanserv/chanserv.log",
      "nickserv/nickserv.log",
      NULL
   };*/

/*   for(i = 0; whatlog[i]; i++)*/
   {
/*       fp = fopen(whatlog[i], "r");*/
fp = stdin;
/*       if (!fp) continue;*/

       while(fgets(buf, 1024, fp)) {
          if (!isalpha(buf[0]))
              continue;
          buf[strlen(buf) - 1] = '\0';
          printf("%s\n", log_interpret(buf));
       }
       fclose(fp);
   }
}
