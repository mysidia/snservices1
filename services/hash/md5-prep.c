/*
 * Copyright (c) 2000 James Hess
 * All rights reserved.
 *
 *  This is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "md5.h"
#include "md5pw.h"
#define VERBOSE

char *NickServ = "NickServ";

int main(int argc, char **argv)
{
     char nick[256];
     char cookie[256];
     char ipasswd[256];
     char hpasswd[256];
     char ifin[8192];
     char hfin[256];
     int i = 0;

#ifdef VERBOSE
     if (isatty(fileno(stdin)))
     printf("Nick: ");
#endif
     fgets(nick, 255, stdin);
     if (*nick=='\n'||!*nick) {
         puts("error;;");
         perror("md5-prep: gets");
         exit(0);
     }
     nick[strlen(nick) - 1]='\0';
     for(i = 0;nick[i]; i++)
         nick[i] = tolower(nick[i]);

#ifdef VERBOSE
    if (isatty(fileno(stdin)))
     printf("Cookie: ");
#endif
     fgets(cookie, 255, stdin);
     if (*cookie=='\n'||!*cookie) {
         puts("error;;");
         perror("md5-prep: gets");
         exit(0);
     }
     cookie[strlen(cookie) - 1]='\0';
#ifdef VERBOSE
 //    if (!isatty(fileno(stdin)))
 //    if (isatty(fileno(stdin)))
 //        printf("Password: ");
#endif
     if (!isatty(fileno(stdin))) {
         fgets(ipasswd, 255, stdin);
         if (!*ipasswd || *ipasswd == '\0') {
             puts("error;;");
             perror("md5-prep: fgets");
             exit(0);
         }
         ipasswd[strlen(ipasswd) - 1]='\0';
     }
     else {
#ifdef VERBOSE
         char *p = getpass("Password: ");
#else
         char *p = getpass("");
#endif
         strncpy(ipasswd, p, 256);
         if (p)
             memset(p, 0, strlen(p));
         ipasswd[255] = '\0';
     }
     if (!*ipasswd) {
         puts("error;;");
         perror("md5-prep: getpass");
         exit(0);
     }
     strcpy(hpasswd, md5_printable(md5_password((u_char *)ipasswd))); 
     memset(ipasswd, 0, 255);
     sprintf(ifin, "%s:%s:%s", nick, cookie, hpasswd);
     memset(cookie, 0, 255);
     memset(hpasswd, 0, 255);
     strcpy(hfin, md5_printable(md5_password((u_char *)ifin))); 
     memset(ifin, 0, 1024);
     printf("%s\n", hfin);
     memset(hfin, 0, 255);
     return 0;
}
