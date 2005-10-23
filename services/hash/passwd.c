/* $Id$ */

/*
 * Copyright (c) 2000 James Hess
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
 * \file passwd.c
 * Implementation of functions necessary to make calls to the hash library
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include "md5.h"
#include "md5pw.h"

#define STD_IRCBUF 512
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE !(FALSE)
#endif

#define MAX_PWD_LENGTH 16
#define str_cmp strcasecmp

/* #define CRACKLIB_DICTPATH "" */

#if defined( CRACKLIB_DICTPATH )
extern char *FascistCheck();
#endif

/**
 * \brief Verifies that a specified string is a valid argument for 'password' hash
 * \param argument Specified value
 * \param password Hash to verify against
 * \bug There should be a 'hash type' parameter
 */
bool Valid_pw(char *argument, unsigned char *password, char encType)
{
    if (!password || !argument)
        return FALSE;
    if (encType == '@' || encType == '-')
        return !str_cmp((const char*)argument, (const char*)password) ? 1 : 0;
    else if (encType == '$' || encType == '+') {
        char tmpa[MAX_PWD_LENGTH * 2];
        char tmpp[MAX_PWD_LENGTH * 2];
        unsigned char *txt2;
        int i = 0;

        for(i = 0; i < (16); i++)
            tmpp[i] = password[i];
        txt2 = md5_password((u_char*)argument);
        for(i = 0; i < (16); i++)
            tmpa[i] = txt2[i];
        for(i = 0; i < (16); i++)
            if (tmpa[i] != tmpp[i])
                return FALSE;
        return TRUE;
    }
    return FALSE;
}

/**
 * \brief Given a place to fill in the password and a user-supplied
 *        password, get the binary hash of the password and fill it in
 *        where
 *        Default hash is MD5.
 * \param what New password
 * \param where Where to fill password in
 */
void pw_enter_password(char *what, unsigned char *where, char encType)
{
     if (!where || !what || !*what) {
         if (where)
             *where = '\0';
         return;
     }

     if (encType == '-' || encType == '@') {
         strncpy((char*)where, what, MAX_PWD_LENGTH);
         where[MAX_PWD_LENGTH - 1] = '\0';
     }
     else
     {
         unsigned char *foo = md5_password((u_char*)what);
         int i = 0;
         if (!foo) {
             strncpy((char*)where, what, MAX_PWD_LENGTH);
             where[MAX_PWD_LENGTH - 1] = '\0';
         }
         else {
             for(i = 0; i < (16); i++)
                 where[i] = foo[i];
         }
     }
/* enter_password(arg, d->handle->passwd); */
}

/**
 * \brief Get the MD5 hashed version of password
 * \param pw Password to hash
 */
unsigned char *md5_password(unsigned char *pw)
{
  unsigned static char md5buffer[16 + 1];
  /* unsigned static char tresult[512]; */
  /* int cnt = 0, o = 0; */
  if (!pw) {
      static char buf[16] = "";

      buf[0] = '\0';
      return (unsigned char *)(buf);
  }
  md5_buffer((unsigned char *)pw, strlen((char *)pw), md5buffer);
  return md5buffer;
}


/**
 * \brief Given a MD5 buffer, return a printable string representing
 *        the MD5 hash
 * \param md5buffer MD5 buffer to print in plaintext
 */
char *md5_printable(unsigned char md5buffer[16])
{
  unsigned static char tresult[512];
  int cnt = 0, o = 0;

  *tresult = 0;
  for (cnt = 0; cnt < 16; ++cnt)
       o+= sprintf ((char *)(tresult + o), "%02x", md5buffer[cnt]);
  tresult[32] = '\0';

  return (char *)tresult;
}

/**
 * \brief Given the printable (hex) version of a MD5 hash, get a
 *        MD5 hash.
 * \param str The printable (hexed) MD5 buffer
 */
unsigned char *md5_unhex(unsigned char str[512])
{
  unsigned static char tresult[255];
  char smbuf[5];
  int cnt = 0, o = 0;

  if (str[0] == '-' || !str[0])
      return str;
  tresult[o++] = '+';
  for (cnt = 1; cnt < 80 && str[cnt]; ) {
      smbuf[0] = *(str + cnt);
      smbuf[1] = *(str + cnt + 1);
      smbuf[2] = '\0';
      tresult[o++] = strtol(smbuf, (char **)0, 16);
      if (((cnt + 1)<80) && str[cnt+1])
          cnt += 2;
      else break;
  }
  tresult[o++] = '\0';
   return tresult;
}

/**
 * \brief Hash the specified password, or rather.. put the password
 *        in plaintext format to be filled into a password field
 * \param pw Password
 * \par Returns a pointer to a static copy of 'pw' with a '-' affixed
 *      to the start.
 */
unsigned char *plaintext_password(char *pw)
{
  unsigned static char buffer[20];
  if (!pw) {
      static char buf[2];

      *buf = '\0';
      return ((unsigned char *)buf);
  }
  sprintf((char *)buffer, "%.16s", pw);
  return buffer;
}


void md5_buffer(unsigned char *passwd, int len, unsigned char *target)
{
     struct MD5Context ctx;

     MD5Init(&ctx);
     MD5Update(&ctx, passwd, len);
     MD5Final(target, &ctx);
}

/**
 * \brief Is a particular password field a MD5 key? (ie: for md5 auth)
 * \param Password
 * \return Non-zero if password is inside MD5(), else 0.
 *
 * \note This tests for MD5 keys in a _password_ field not in a MD5
 *       field, when using the identify-md5 command to identify this
 *      isn't needed.  This test is for handling things like:
 *
 *      -  /nickserv identify-md5
 *      -  /nickserv recover \lt nick> MD5(\lt hash>)
 */
int isMD5Key(const char* password)
{
	int l;
	
	if (!password || !*password)
		return 0;
		
	if (password[0] != 'M' || password[1] != 'D' || password[2] != '5'
	    || password[3] !=  '(')
	    return 0;
		
	l = strlen(password);
	
	if (password[l - 1] != ')')
		return 0;
	return 1;
}


int Valid_md5key( char* key, struct auth_data* auth, const char* to_object, unsigned char* passwd, char enc )
{
	char instring[STD_IRCBUF];
	char pwstring[STD_IRCBUF];
	char npwhash[STD_IRCBUF];
	char lobject[STD_IRCBUF];	
	int i;

	if (!key || !to_object || !passwd || enc == '\0')
		return 0;

	if (!auth || auth->auth_cookie == 0)
		return 0;

	/*
	 * If nick password is encrypted, then the password is already pre-hashed.
	 * otherwise we need to take the digest
	 */
	if (enc != '$')
		snprintf(npwhash, sizeof(npwhash), "%.128s",
				md5_printable(md5_password(passwd)));
	else
		snprintf(npwhash, sizeof(npwhash), "%.128s",
				md5_printable(passwd));

	/*
	 * Build the digest we expect to get
	 */

	strncpy(lobject, to_object, sizeof(lobject));
	lobject[sizeof(lobject) - 1] = '\0';

	for (i = 0; lobject[i]; i++)
		lobject[i] = tolower(lobject[i]);
	snprintf(instring, sizeof(instring), "%s:%x:%x:%s", lobject, (unsigned int)auth->auth_cookie,
			(unsigned int)auth->auth_user, npwhash);
	if (auth->format < 2) {
		snprintf(pwstring, sizeof(pwstring), "%.32s", md5_printable(md5_password((u_char *)instring)));
	} else {
	        snprintf(pwstring, sizeof(pwstring), "MD5(%.32s)", md5_printable(md5_password((u_char *)instring)));

	} 
	

	/*
	 * If that's what they sent, then they know the password!
	 */
	if (!strcmp(key, pwstring)) 
		return 1;

	/* Otherwise they don't */
	return 0;
}//yvalid
