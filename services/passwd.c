/**
 * \file passwd.c
 * \author James Hess <mysidia-snn@flame.org>
 * \date 2000, 2001
 *
 * $Id$
 *
 * Services routines for managing and encoding passwords
 *
 * Copyright 2000, 2001 James Hess All Rights Reserved.
 *
 * See the end of this file for licensing information.
 */

#include "services.h"
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "hash/md5.h"
#include "hash/md5pw.h"

#ifndef __cplusplus
/// 1 or 0
typedef unsigned char bool;
#endif

/*************************************************************************/

/**
 * Encode in base64 - finally got this working, I hope. -Mysid
 *
 * \param stream Memory area to read character data from
 * \param left   Size of the memory area in bytes
 * \pre Stream points to a valid (data-octet-bearing) character array of
 *      size 'left' or higher. 
 * \return A base64-encoded version of binary data read from `stream' up
 *         to `left' octets.  The version returned will have been
 *         allocated from the heap.  Freeing it is the caller's
 *         responsibility.
 */
unsigned char *toBase64(const unsigned char *stream, size_t left)
{
	#define X_OUTPUT_MAP_SZ 64
	const unsigned char mutations[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	unsigned int j, k, s_sz = 0, acc;
	unsigned char temp[3] = {}, out[4];
	char *s = NULL, *p;

	j = acc = 0;

	// Calculate the length increase from the conversion from
	// 8 bit to 6 bit encoding. We will even provide a null terminator.

	s_sz = (((left * 8) / 24) * 4) +
		((((left * 8) % 24)) ? 3 : 0) +
		2;


	p = s = (char *)oalloc(s_sz + 1);

	for ( ; left ; )
	{
		while ( left > 0 && j < 3 ) {
			temp[j++] = *stream;
			left--;
			stream++;
		}

		//
		// Alphabetic approximation of what our
		// bitwise operations need to accomplish
		//
		// ABCDEFGH IJKLMNOP QRSTUVWX --------
		// ABCDEFGH IJKLMNOP QRSTUVWX STUVWX--
		// --ABCDEF ABCDEFGH --MNOP-- STUVWX--
		// 00111111 00000011 ------QR STUVWX--
		// --ABCDEF ------GH --MNOPQR STUVWX--
		// --ABCDEF ----GH-- --MNOPQR STUVWX--
		// --ABCDEF ----IJKL --MNOPQR STUVWX--
		// --ABCDEF --GHIJKL --MNOPQR STUVWX--
		// --ABCDEF --GHIJKL --MNOPQR STUVWX--
		//

		// Stuff the first 6 bits into the 1st 6-bit counter
		// Shift it over so that the 2 high bits are
		// zero
		out[0] = (temp[0] >> 2);

		// Stuff the last two bits of the first byte into
		// the second counter -- shift it into the high
		// bits.  Carry in the next 4 bits
		{
			if (j < 3) {
				temp[2] = 0;
				if (j < 2)
					temp[1] = 0;
			}

			out[1] = ((temp[0] & 0x03) << 4) |
			         (temp[1] >> 4);

			// Lather, rinse, repeat

			// 00001111 -> 0x0f
			out[2] = ( (temp[1] & 0x0f) << 2 ) |
	 		         ( temp[2] >> 6 );

			// 00111111 ->  0x3f
			out[3] = (temp[2] & 0x3f);
		}

		for(k = 0; k < 4; k++) {
			if ( k <= j ) {
				assert(out[k] < X_OUTPUT_MAP_SZ);
				*p++ = mutations[out[k]];
			}
			else	*p++ = '=';
			if (--s_sz < 1)
				assert(s_sz >= 1);
		}

		j = 0;
	}

	*p++ = '\0';

	//#define X_OUTPUT_MAP_SZ 64
	return (unsigned char *)s;
}

/**
 * Decode from base64 - whole lot easier to decode than to encode <G>
 *
 * \param cStr Memory area to read character data from
 * \pre Stream points to a valid NUL-terminated character array of the
 *      ASCII character set that is a valid base-64 encoding of binary
 *      data
 * \return A number of octets binary data as decoded from the input.
 *         Allocated from the heap, caller must free.
 */
unsigned char *fromBase64(const char *cStr, int *len)
{
	#define X_OUTPUT_MAP_SZ 64
	const unsigned char mutations[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	static unsigned char decode_table[255] = {};
	static int pFlag = 0;
	int i, j, k, s_sz = 0, hitz = 0;
	unsigned char temp[4] = {}, out[4] = {}, c;
	unsigned char *s = NULL, *p;
	size_t left = strlen(cStr);


	if (pFlag == 0)
	{
		pFlag = 1;

		for(j = 0; j < 127; j++)
			decode_table[j] = '\0';

		for(j = 0; j < 64; j++) {
			for(i = 0; i < 127; i++) {
				if (i == mutations[j]) {
					decode_table[i] = j;
					break;
				}
			}
		}
		pFlag = 1;
	}

	// For every 4 characters we have, we have 24 bits of output.
        // 24 / 8 ==> 3 bytes

	s_sz = 3 + (int) ((left / 4.0) * 3);

	p = s = (unsigned char *)oalloc(s_sz + 1 + 10);

	for ( ; left > 0 ; )
	{
		j = 0;
		hitz = 0;

		while(j < 4 && left > 0) {
			c = *(cStr++);
			if ((unsigned char)temp[j] < sizeof(decode_table))
				c = decode_table[(unsigned char)c];
			else { c = '\0'; hitz = 1; }

			if (hitz == 0) {
				temp[j] = c;
				j++;
			}
			left--;
		}
		for(k = j; k < 4; k++)
			temp[k] = 0;

		// --ABCDEF --GHIJKL --MNOPQR STUVWX--
		// ABCDEFGH IJKL---- QR------
		// ------GH ----MNOP --STUVWX

		out[0] = (temp[0] << 2) | ((temp[1] >> 4) & 0x03);
		out[1] = (temp[1] << 4) | ((temp[2] >> 2));
		out[2] = (temp[2] << 6) | temp[3];

		if (s_sz > 0) {
			*(p++) = out[0];
			s_sz--;
		}

		if (s_sz > 0) {
			*(p++) = out[1];
			s_sz--;
		}

		if (j >= 4) {
			(*(p++)) = out[2];
			s_sz--;
		}

		if (s_sz <= 0) {
			assert(s_sz > 0);
		}
	}

	if (len)
		(*len) = (size_t)(p - s);

	*p++ = '\0';

	return (unsigned char *)s;
	#undef X_OUTPUT_MAP_SZ
}

/*
 * -> Base on 100million in the most gruesome way imaginable and map it
 * into 65 characters, in a peculiar way, the heck? *snick* 
 *
 * -Mysid 
 */

static char *ExpressAsHighIntBase(u_int32_t value)
{
   static char result[256];
   char *x_output_map[] = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
        "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "-", "_", /*"0",*/ "1", "2", "3", "4", "5", "6", "7", "8", "9", "/",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "$", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",

        "*", "+", "=", "\\", "&", "^", "#", "@", "!", "{",
        "(", "[" "?", "'", "\"", "]", ")", ")", ",", ".",
        "<", ">", "%", "`", /*after this point not used*/"|", "`", ""
   };
   #define X_OUTPUT_MAP_SZ 88
   #define X_BASE_ENCODING ULONG_MAX
   int i = 0, x = 99;
   u_int32_t rem;

   for(i = 0; i < 100; i++)
       result[i] = '\0';

   while(value > 0) {
        rem = (value % X_BASE_ENCODING);
        value = (int) value / X_BASE_ENCODING;
        if ((rem >= X_BASE_ENCODING))
            break;
        result[x--] = *x_output_map[((unsigned char)rem) % X_OUTPUT_MAP_SZ];
        if (x < 0 || value < 1)
            break;
   }

   if (x < 99)
       for(i = 0; !result[i]; i++) ;
   else i = 0;

   return (result + i);
   #undef X_OUTPUT_MAP_SZ
   #undef X_BASE_ENCODING
}

/**
 * \pre Email and Password point to valid NUL-terminated character
 *      arrays.  Timereg is a valid UTC calendar time, and code_arg
 *      is a code generated at random.
 * \return An authentication key for password changes based
 *         on a hash of all data
 * \param email The user e-mail address
 * \param password The value of the nick or channel password structure
 * \param code_arg The selected change code
 */
const char *GetAuthChKey(const char *email, const char *password,
                         time_t timereg, u_int32_t code_arg)
{
	u_char digest[16];
	static char tresult[256];
	struct MD5Context ctx;
	uint32 buf[PASSLEN+8];
	uint32 md5buf[16];

	int cnt = 0, o = 0;

	int i, j, k, l;

	i = l = 0;
	memset(buf, 0, sizeof(buf));

	buf[l++] = (timereg & 65280UL) | (timereg & 4278190080UL); /* 1 */
	buf[l++] = (timereg & 255UL) | (timereg & 16711680UL);
	buf[l++] = (code_arg & 255UL) | (code_arg & 65280UL);
	buf[l++] = 0x7f0a528;
	buf[l++] = 0x3011f3f; /* 5 */

	for(i = 0; password[i] && i <= PASSLEN; i++) /* PASSLEN */
		buf[l++] = password[i];
	buf[l++] = 0x0; /* 6 */

	for(i = 0; i < l; i++) {
		buf[i] ^= (code_arg & 4278190080UL);
		buf[i] ^= (code_arg & 16711680UL);
	}

	buf[l++] = 0x162a34b; /* 7 */

	MD5Init(&ctx);

	for(j = 0 ;; j += 16 ) {
		if (j >= l)
			break;
		k = 0;

		for(i = j; i < j+16; i++) {
			if (i >= l) {
				do {
					md5buf[j+i] = k ^ ((code_arg & 255) << 1);
				} while(++i < 16);
				break;
			}
			md5buf[j+i] = buf[i];
			buf[i] = 0;
		}
		MD5Transform(ctx.buf, md5buf);
	}

	MD5Update(&ctx, (unsigned const char *)email, strlen(email));
	MD5Final(digest, &ctx);
	MD5Init(&ctx);
	memset(buf, 0, sizeof(buf));

	*tresult = '\0';
        memset(tresult, 0, 255);
	for(cnt = 0; cnt < 16; ++cnt)
		o += sprintf(tresult+o, "%s", ExpressAsHighIntBase(digest[cnt]));
	return tresult;
}

/*************************************************************************/

/* Verify the relative security of a password - Not actually used */

static int pwd_too_simple(const char *newp)
{
	int	count_digit = 0,	count_upper = 0;
	int	count_lower = 0,	count_misc = 0;
        int	req_len;
        int	i;

        for (i = 0;newp[i];i++) {
                if (isupper (newp[i]))
                        count_upper++;
                else if (isdigit (newp[i]))
                        count_digit++;
                else if (islower (newp[i]))
                        count_lower++;
                else
                        count_misc++;
        }

        req_len = 7;
        if (count_digit > 0) --req_len;
        if (count_upper > 0) --req_len;
        if (count_lower) --req_len;
        if (count_misc) --req_len;

        if (req_len <= i)
                return 0;

        return 1;
}

/*************************************************************************/

int mpwd_too_simple(char *epwd, RegNickList *nick)
{
     char pwd[MAXBUF];

     if (!epwd || !nick)
         return 1;
     strncpy(pwd, epwd, MIN(MAXBUF, 80));
     pwd[MAXBUF - 1] = '\0';

#ifdef CRACKLIB_DICTPATH
     if (nick)
     {
          char *msg;
          if ((msg = (char *) FascistCheck(pwd, CRACKLIB_DICTPATH))) {
              sSend(":%s NOTICE %s :That password you specified not safe because %s.", NickServ, nick->nick);
              return 1;
          }
     }
     /* if (nick && (pwd_too_simple(pwd))) {
         sSend(":%s NOTICE %s :That password is too short.", NickServ, nick->nick);
         return 1;
     } */
#else
     if (nick && (pwd_too_simple(pwd))) {
         sSend(":%s NOTICE %s :That password is too short, try again.", NickServ, nick->nick);
         return 1;
     }

#endif /* CRACKLIB_DICTPATH */
     return 0;
}

/**
 * @brief Print a password appropriately encoded according to its encryption
 *        type (if applicable).
 * \param p Password field
 * \param enc Encryption status
 * \pre pI is a NUL-terminated character array.
 * \return A pointer to a static NUL-terminated character array containing
 *         the printable version of a password
 */
const char *PrintPass(u_char pI[], char enc)
{
	static char buf[256];

	if (enc == '$' || enc == '+') {
		u_char *p = toBase64(pI, 16);
		if (!p)
			abort();
		strncpy(buf, (char *)p, sizeof(buf));
		buf[sizeof(buf) - 1] = '\0';
		FREE(p);
		return buf;
	}
	else
		return (char *)pI;
}

/**
 * \brief Is a password acceptable?
 *
 * This procedure tests whether a password is acceptable or not
 * in terms of technical devices that _need_ to have the password
 * fit into certain ranges.
 */
int isPasswordAcceptable(const char* password, char* reason)
{
	if (!password || !*password) {
		strcpy(reason, "Blank passwords are not allowed.");
		return 0;
	}
	
	if  (isMD5Key(password)) {
		strcpy(reason, "Try using a password that doesn't start with MD5(.");
		return 0;
	}
	
	if (password[0] == '$' && password[1] == '_') {
		strcpy(reason, "Try using a password that doesn't start with $_.");
		return 0;	    
	}
	return 1;
}


/*
 * Copyright (c) 2000, 2001 James Hess
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

