/************************************************************************
 *   IRC - Internet Relay Chat, buildhelp.c
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


/*
 *   experimental help.c builder
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

char NOSUCHTOPIC[] = {
 "Your help request has been forwarded to the HelpOps.\n"
 "If you need help on SorceryNet services, try...\n"
 "  \2/helpop nickserv\2 - for help on registering nicknames.\n"
 "  \2/helpop chanserv\2 - for help on registering channels.\n"
 "  \2/helpop memoserv\2 - for help on sending short messages to other users.\n"
 "If you are using ircII, use \2/quote\2 instead of \2/raw\2.\n"
};


char Hhead[] = {
"/************************************************************************
 *   IRC - Internet Relay Chat, help.c
 *   Copyright (C) 1998 SorceryNet
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

#ifndef lint
static char rcsid[] = \"$Id$\";
#endif

 /*
  *    Warning! This file was generated automatically from a directory
  *             of helpfiles
  */

#include \"struct.h\"
#include \"common.h\"
#include \"sys.h\"
#include \"msg.h\"
#include \"h.h\"
#define SND(str) sendto_one(sptr, \":%s NOTICE %s :\" str \"\", me.name, name)

/*
 * This is _not_ the final help.c, we're just testing the functionality for now...
 */

int parse_help (sptr, name, helpmsg)
aClient *sptr;
char    *name;
char    *helpmsg;
{

char help[2048] = \"\";
bzero(help, sizeof(help));
strncpy(help, helpmsg, sizeof(help));
help[sizeof(help)-1] = 0;

"};

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

     xprintf(globV, "%s if (!myncmp(help, \"%s \", %d))"LF, cond>1?"} else":"", key, strlen(key) );
     xprintf(globV, "  {"LF);
     xprintf(globV, "    SND(\"***** \2%s Help\2 *****\"); "LF, SC(key));
}

void MakeHelpTopic(FILE *from, char *n, int l, int nested, int oset)
{
  char buf[512] = "";
  char bigbuf[1024] = "";
  int x = 0, i = 0;
        if (nested > 0)
         {
               xprintf(globV, "}"LF);
               xprintf(globV, "else ");
         }

#ifndef SPLIT
        xprintf(globV, "if (!myncmp(help+%d, \"%s \", %d) ) "LF"{"LF, oset, n, l);
#else
        {
          char *sp;
           if (!( sp = index(n, '-') ))
           {
            if (strcasecmp(n, "index"))
             xprintf(globV, "if (!myncmp(help+%d, \"%s \", %d) ) "LF"{"LF, oset, n, l);
            else
             xprintf(globV, "if (!myncmp(help+%d, \"%s \", %d) || !*(help+%d)) "LF"{"LF, oset, n, l,
                             oset-1);
           }
           else
           {
              char t[512] = "", *e;
              int off = 0, elen;
              strncpy(t, n, sizeof(t));
              t[500] = 0;

              off = oset;
//              xprintf(globV, "if (!myncmp(help+%d, \"%s \", %d) ) "LF"{"LF, l, n, l);
              xprintf(globV, "if (1");
              for ( e = strtok(t, "-"); e; e = strtok(NULL, "-") )
              {
                 if(!e) break;
                 elen = strlen(e);
                 fprintf(fout, " && (!myncmp(help+%d, \"%s \", %d))", off, e, elen);
                 off += (elen+1);
              }
              fprintf(fout, ") "LF"{"LF);

           }
        }
#endif

  while (fgets(buf, 255, from))
  {
        if(!*buf || *buf=='\n' || *buf == ';' || *buf == '#' || (*buf == '/' && *(buf+1) == '/'))
              continue;
        buf[strlen(buf)-1] = 0;
        for ( x = 0, i = 0 ; buf[i]; i++)
        {
           if (!(buf[i] == '\"' || buf[i] == '\\'))
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

        xprintf(globV, "   SND(\"%s\");"LF, bigbuf );
  }
  xprintf(globV, "   return 1;"LF);
}


void SynthHelpTopic(char *text, char *n, int l, int nested, int iRET, int oset)
{
  char buf[8192] = "";
  char *sn = NULL;

         {
               xprintf(globV, "}"LF);
               xprintf(globV, "else ");
         }

    if (n)
    {
        xprintf(globV, "if (!myncmp(help+%d, \"%s \", %d) ) "LF, oset, n, l);
        xprintf(globV, " {"LF);
    }
    else
        xprintf(globV, " {"LF);

  strncpy(buf, text, sizeof(buf));
  buf[sizeof(buf)-1] = 0;

  for ( sn = strtok(buf, "\n"); 
        sn; sn = strtok(NULL, "\n") )
  {
        if (!sn) break;
        if(!*sn || *sn=='\n' || *sn == ';' || *sn == '#' || (*sn == '/' && *(sn+1) == '/'))
              continue;
        xprintf(globV, "   SND(\"%s\");"LF, sn );
  }
  xprintf(globV, "   return %d;"LF, iRET);
}

void doit(char *b)
{
    DIR *directory;
    struct dirent *d;
    int bigindice = 0;
    int i = 0, q = 0, nservs = 0;
    if(!dirlist) exit(0);
    globV++;
     xprintf(globV, "if (!mycmp(help, \"msgtab\"))"LF );
     xprintf(globV, "{"LF);
     xprintf(globV, "   int z = 0;"LF);
     xprintf(globV, "   for (z = 0; msgtab[z].cmd; z++)"LF);
     xprintf(globV, "   {"LF);
     xprintf(globV, "     sendto_one(sptr, \":%%s NOTICE %%s :%%s\", me.name, sptr->name, msgtab[z].cmd);"LF);
     xprintf(globV, "   }"LF);
    q = 1;
    globV--;

    for ( i = 0 ; dirlist[i]; i++)
    { 
          chdir(b);
          chdir(dirlist[i]);
          printf("%s\n", dirlist[i]);
          q++;
          globV++;
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
                      MakeHelpTopic(fn, key2, strlen(key2), nested, strlen(dirlist[bigindice])+1);
                      nested++;
                  }

                  fclose(fn);
              }
              SynthHelpTopic(NOSUCHTOPIC, NULL, 0, nested, 0, strlen(dirlist[bigindice])+1);
              fprintf(fout, "    } "LF);
          }
          //
#ifndef SCRAMBLE
          fprintf(fout, "/* End of %s Help */"LF""LF, SC(dirlist[i]));
#endif
          globV--;
    }
    if (nservs)
    {
         fprintf(fout, LF);
         SynthHelpTopic(NOSUCHTOPIC, NULL, 0, 0, 0, 0);
         fprintf(fout, LF);
         xprintf(globV, "}"LF);
         fprintf(fout, "/* help.c */\n");
    }
    globV = 0;
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
    fout = f = fopen("help.c.build", "w");
    if (!f) exit(0);
    fprintf( fout, "%s"LF, Hhead);
    present_working_directory = getcwd(NULL, 512);
    /*
     *  here is where we build the help topics
     */
     ZeroLstring(&dirlist, getnel(&dirlist));
     FillDirList();
     doit(present_working_directory);
     xprintf(globV, "}"LF);
}
