/* 
 *   This is a module adapted from buildhelp.c (which wrote out a help.c)
 *   to a module that writes out .hlp files
 */

/************************************************************************
 *   IRC - Internet Relay Chat, buildhf.c
 *   Copyright (C) 1998 Mysidia/SorceryNet
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

#ifndef __lint__
static char rcsid[] = "$Id$";
#endif


/*
 *   experimental help system builder
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdarg.h>
extern FILE *fout;
#define SPLIT
#undef SCRAMBLE
#ifndef SCRAMBLE
#define LF "\n"
#else
#define LF "	"
#endif
/* should built files be of type COMMAND, TOPIC, or PAGE? 
   ("PAGE" for now) */
#define HTT "PAGE" 

char Hhead[] = {
"
#
# SorceryNet Help System
# This file was automatically created by buildhf
# $Id$
# Copyright (C) 2000 The SorceryNet IRC Network - All rights reserved
#

^TOPIC:INDEX

      For detailed information on services, type:
            %B/quote help %N%B
            %B/quote help %C%B
            %B/quote help %M%B
            %B/quote help %I%B

      For general services info, type /quote help services

                 Other Help Resources

  The SorceryNet website is at %Bhttp://www.sorcery.net%B
  Type /quote help msgtab to view the list of available commands
  Type /quote help !<topic> to only search help pages for a topic
  Type /quote help ?<message> to send a message to online %BHelpOps%B
  Type /quote help <question> to make a general help request

^END

#
# The following help files are automatically loaded on startup
#
"
};

char **dirlist = NULL;
static int globV = 1;

void xprintf(int level, char *fmt, ...)
{
      static char buf[2048*3] = "";
      int i = 0;
      va_list args;

      if (!fout || level < 0 || !fmt) return;
      for ( i = 0 ; i < 2048 ; i++ )
         buf[i] = ' ';
      buf[2045] = 0;
      va_start(args, fmt);
      if ( level < 0 )  level = 0;
      if ( level > 10 ) level = 11;
#ifndef SCRAMBLE
      vsnprintf(buf+level, sizeof(buf) / 2, fmt, args);
#else
      level = 10;
      vsnprintf(buf+level, sizeof(buf) / 2, fmt, args);
#endif
      fprintf(fout, "%s", buf);
      return;
}

int getnel(char ***list)
{
    int i = 0;
     if (!*list) return 0;
     for (i = 0 ; (*list)[i]; i++);
     return i;
}

char *strlower(char *b)
{
     int i = 0;

     if (!(b)) return NULL;
      for (i = 0; b[i]; i++) b[i] = tolower(b[i]);
     return b;
}

char * stristr(char *haystack, char *needle)
{
   static char *x = NULL, *y = NULL; 
   int i = 0;
   char * r = 0;

   if (x)
       free(x);
   x = strdup(haystack);
   y = strdup(needle);
   for ( i = 0 ; x[i]; i++) x[i] = tolower(x[i]);
   for ( i = 0 ; y[i]; i++) y[i] = tolower(y[i]);
   r = strstr(x, y);
   free(y);
   return ((r));
}

void ZeroLstring(char ***list, int nel)
{
    if (nel < 1) return;
    free(*list);
    *list = NULL;
}

void npost(char ***list, char *string)
{
   int x = 0;
   x = getnel(list);

   *list = realloc(*list, sizeof(char *) * (x+2));
   ((*list)[x]) = strdup(string);
   ((*list)[x+1]) = NULL;
   x++;
   return;
}

char * SC(char *x)
{
   static char buf[512] = "";
   char *y;
   int i = 0;

   strncpy(buf, x, sizeof(buf));
   buf[sizeof(buf)-1]=0;
   for ( i = 0; buf[i]; i++) buf[i] = tolower(buf[i]);
   strlower(buf);
   y = strstr(buf, "serv");
   if (!y) return;
   *y = toupper(*y);
   buf[0] = toupper(buf[0]);
   return buf;
}

void StartDir( char *key, int tree, int cond)
{
    char buf[1024] = "";

    if (fout)
        fclose(fout);
    fout = NULL;
    if (!strstr(key, "serv"))
        return;
    sprintf(buf, "../%.256s.hlp", key);
    fout = fopen(buf, "w");
    if (!fout) {sprintf(buf, "fopen: ../%s.hlp", key); perror(buf); exit(0);}
}

void MakeHelpTopic(FILE *from, char *n, char *service)
{
  char buf[512] = "";
  char bigbuf[1024] = "";
  int x = 0, i = 0;
        {
          char *sp;
           if (!( sp = index(n, '-') ))
           {
            if (strcasecmp(n, "index"))
             xprintf(globV, LF"^"HTT":%s %s"LF, service, n);
            else
             xprintf(globV, LF"^"HTT":%s"LF, service);
           }
        }

  while (fgets(buf, 255, from))
  {
        if(!*buf || *buf=='\n' || *buf == ';' || *buf == '#' || (*buf == '/' && *(buf+1) == '/'))
              continue;
        buf[strlen(buf)-1] = 0;
        for ( x = 0, i = 0 ; buf[i]; i++)
        {
           if (!(/*buf[i] == '\"' ||*/ buf[i] == '\\'))
           {
             bigbuf[x++] = buf[i]; 
           }
           else
           {
             bigbuf[x++] = '\\'; 
             bigbuf[x++] = buf[i]; 
           }
        }
        bigbuf[x++] = 0;

        xprintf(globV, "%s"LF, bigbuf );
  }
  if (service && *service)
  {
      xprintf(globV, "^%s"LF, service);
      if (!strncasecmp(service, "Oper", 4))
          xprintf(globV, "^Oper"LF);
  }
  xprintf(globV, "^END"LF);
}


void SynthHelpTopic(char *text, char *n, int l)
{
  char buf[8192] = "";
  char *sn = NULL;

    if (n)
        xprintf(globV, LF"^"HTT":%s"LF, n);
    else
        xprintf(globV, ""LF);

  strncpy(buf, text, sizeof(buf));
  buf[sizeof(buf)-1] = 0;

  for ( sn = strtok(buf, "\n"); 
        sn; sn = strtok(NULL, "\n") )
  {
        if (!sn) break;
        if(!*sn || *sn=='\n' || *sn == ';' || *sn == '#' || (*sn == '/' && *(sn+1) == '/'))
              continue;
        xprintf(globV, "%s"LF, text );
  }
  xprintf(globV, "^DONE"LF);
}

void doit(char *b)
{
    DIR *directory;
    struct dirent *d;
    int bigindice = 0;
    int i = 0, q = 0, nservs = 0;
    if(!dirlist) exit(0);
    q = 1;
    globV = 0;

    for ( i = 0 ; dirlist[i]; i++)
    { 
          chdir(b);
          chdir(dirlist[i]);
          printf("%s\n", dirlist[i]);
          q++;
          nservs++;
          StartDir( dirlist[i], globV, q );
          bigindice = i;
          if ( (directory =  opendir ( "." ) ))
          {
              int nested = 0;
              while( ( d = readdir(directory) ))
              {
                  FILE *fn;

                  if (*d->d_name == '.' || !*d->d_name) continue;
                  if (!stristr( d->d_name, ".help")) continue;
                  fn = fopen( d->d_name, "r"); 
                  if (!fn) continue;
                  {
                      char key2[512], *v;

                      *key2 = 0;
                      strncpy(key2, d->d_name, 511);
                      key2[511] = 0;
                      v = rindex(key2, '.');
                      if (!v) continue;
                      *v = 0; v = key2;
                      for (; *v; v++) *v = tolower(*v);
                      MakeHelpTopic(fn, key2, dirlist[i]);
                      nested++;
                  }

                  fclose(fn);
              }
              /* SynthHelpTopic(NOSUCHTOPIC, NULL, 0);
              fprintf(fout, "    } "LF);*/	
          }
          //
#ifndef SCRAMBLE
          fprintf(fout, "# /* End of %s Help */"LF""LF, SC(dirlist[i]));
#endif
    }
}

void FillDirList()
{
    DIR *directory = opendir(".");
    struct dirent *d;
    struct stat stb;

     if (!directory) {exit(-1);}
     while ( ( d = readdir(directory) ) )
     {
       if ( stat( d->d_name, &stb) < 0 ) exit(-1);
       if (!S_ISDIR( stb.st_mode )) continue;
       if ( *d->d_name == '.' ) continue;
       if (!stristr( d->d_name, "serv" )) continue;

       npost(&dirlist, d->d_name);
     }
     closedir(directory);
     return;
}

FILE *fout;
main()
{
   FILE *f;
   char *present_working_directory;
/*    fout = fopen("index.hlp", "w");
    if (!fout)
    {
       perror("fopen");
       exit(-1);
    }
    fprintf( fout, "%s"LF, Hhead);
    fprintf(fout, "^INCLUDE \"nickserv.hlp\""LF);
    fprintf(fout, "^INCLUDE \"chanserv.hlp\""LF);
    fprintf(fout, "^INCLUDE \"memoserv.hlp\""LF);
    fprintf(fout, "^INCLUDE \"operserv.hlp\""LF);
    fprintf(fout, "^INCLUDE \"infoserv.hlp\""LF);
    fprintf(fout, "^INCLUDE \"gameserv.hlp\""LF);
    fprintf(fout, "^INCLUDE \"oper.hlp\""LF);
    fprintf(fout, "^INCLUDE \"irc.hlp\""LF);
    fprintf(fout, "^INCLUDE \"local.hlp\""LF);
    fprintf(fout, "^DONE" LF);
    fprintf(fout, LF "#EOF" LF);*/

    present_working_directory = getcwd(NULL, 512);
    /*
     *  here is where we build the help topics
     */
     ZeroLstring(&dirlist, getnel(&dirlist));
     FillDirList();
     doit(present_working_directory);
     xprintf(globV, ""LF);
}
