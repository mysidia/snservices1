/*
 *   IRC - Internet Relay Chat
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

#include <sys/types.h>

#include <ctype.h>

#include "ircd/cdefs.h"
#include "ircd/string.h"

#include "sys.h"
#include "struct.h"
#include "h.h"

IRCD_RCSID("$Id$");

#ifdef NEED_STRTOKEN
/*
** 	strtoken.c --  	walk through a string of tokens, using a set
**			of separators
**			argv 9/90
**
*/
char *
strtoken(char **save, char *str, char *fs)
{
	char *pos = *save;	/* keep last position across calls */
	char *tmp;

	if (str)
		pos = str;		/* new string scan */

	while (pos && *pos && index(fs, *pos) != NULL)
		pos++; 		 	/* skip leading separators */

	if (!pos || !*pos)
		return (pos = *save = NULL); 	/* string contains only sep's */

	tmp = pos; 			/* now, keep position of the token */

	while (*pos && index(fs, *pos) == NULL)
		pos++; 			/* skip content of the token */

	if (*pos)
		*pos++ = '\0';		/* remove first sep after the token */
	else
		pos = NULL;		/* end of string */

	*save = pos;
	return(tmp);
}
#endif /* NEED_STRTOKEN */

#if (defined(REDHAT5) || defined(REDHAT6) || defined(LINUX_GLIBC)) && defined(NEED_STRTOK) 
#undef NEED_STRTOK
#endif

#ifdef NEED_STRTOK
/*
** NOT encouraged to use!
*/

char *
strtok(char *str, char *fs)
{
	static char *pos;

	return strtoken(&pos, str, fs);
}

#endif /* NEED_STRTOK */

/*
 *  Case insensitive comparison of two NULL terminated strings.
 *
 *	returns	 0, if s1 equal to s2
 *		<0, if s1 lexicographically less than s2
 *		>0, if s1 lexicographically greater than s2
 */
int
smycmp(const char *s1, const char *s2)
{
	unsigned char        *str1;
	unsigned char        *str2;
	int            res;
  
	str1 = (unsigned char *)s1;
	str2 = (unsigned char *)s2;
  
	while ((res = irc_toupper(*str1) - irc_toupper(*str2)) == 0) {
		if (*str1 == '\0')
			return 0;
		str1++;
		str2++;
	}
	return (res);
}


int
myncmp(const char *str1, const char *str2, int n)
{
	unsigned char  *s1, *s2;
	int      res;
  
	s1 = (unsigned char *)str1;
	s2 = (unsigned char *)str2;
  
	while ((res = irc_toupper(*s1) - irc_toupper(*s2)) == 0) {
		s1++; s2++; n--;
		if (n == 0 || (*s1 == '\0' && *s2 == '\0'))
			return 0;
	}
	return (res);
}

/* Flag differences in the case tables as an error  */

/*
 * This is intended to test the ircd case table against the system
 * toupper() and tolower().
 *
 * When necessary
 */

void setup_casetables()
{
	int	i;

	for(i = 0; i < 256; i++)
	{
		if (tolower(i) != irc_tolower(i))
		{
			tolog(LOG_IRCD, "Warning: tolower(%d) is %d != irc_tolower(%d) which is %d", i,
				tolower(i), i, irc_tolower(i));
		}

		if (toupper(i) != irc_toupper(i))
		{
			tolog(LOG_IRCD, "Warning: toupper(%d) is %d != irc_toupper(%d) which is %d", i,
				toupper(i), i, irc_toupper(i));
		}
	}
}

unsigned char irc_tolowertab[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 'a',  'b',  'c',  'd',  'e',  'f',  'g',
	'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
	'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '[',  '\\',  ']', '^', 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

unsigned char irc_touppertab[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 'A',  'B',  'C',  'D',  'E',  'F',  'G',
	'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
	'X',  'Y',  'Z',  '{',  '|', '}',   '~',  0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#undef P
#define P IRC_CHAR_PRINT
#undef C
#define C IRC_CHAR_CNTRL
#undef A
#define A IRC_CHAR_ALPHA
#undef N
#define N IRC_CHAR_PUNCT
#undef D
#define D IRC_CHAR_DIGIT
#undef S
#define S IRC_CHAR_SPACE

unsigned char irc_charattrib[] = {
	C,   C,   C,   C,   C,   C,   C,   C,
	C,   C|S, C|S, C|S, C|S, C|S, C,   C,
	C,   C,   C,   C,   C,   C,   C,   C,
	C,   C,   C,   C,   C,   C,   C,   C,
	P|S, P,   P,   P,   P,   P,   P,   P,
	P,   P,   P,   P,   P,   P,   P,   P,
	P|D, P|D, P|D, P|D, P|D, P|D, P|D, P|D,
	P|D, P|D, P,   P,   P,   P,   P,   P,
	P,   P|A, P|A, P|A, P|A, P|A, P|A, P|A,
	P|A, P|A, P|A, P|A, P|A, P|A, P|A, P|A,
	P|A, P|A, P|A, P|A, P|A, P|A, P|A, P|A,
	P|A, P|A, P|A, P|A, P|A, P|A, P|A, P,
	P,   P|A, P|A, P|A, P|A, P|A, P|A, P|A,
	P|A, P|A, P|A, P|A, P|A, P|A, P|A, P|A,
	P|A, P|A, P|A, P|A, P|A, P|A, P|A, P|A,
	P|A, P|A, P|A, P|A, P|A, P|A, P|A, 0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0
};
