// Copyright James Hess, All Rights Reserved

#include <iostream>
#include <string>
#include <strstream>
#include "passwd_test.h"
#define eq(x,y) (!strcasecmp(x,y))

#include "../services.h"
#include "hash/md5pw.h"

unsigned char *toBase64(const unsigned char *stream, size_t left);

CPPUNIT_TEST_SUITE_REGISTRATION( PasswdTestCase );
void
PasswdTestCase::testToBase64()
{
        char
        s[] = "\177ELF\1\1\6\0\0\0\4\0\0\0/lib/ld-linux.so.2\0\0\4\0\0\0\020"
              "\0\0\0\1\0\0\0GNU\n";

        char
        r[] = "f0VMRgEBBgAAAAQAAAAvbGliL2xkLWxpbnV4LnNvLjIAAAQAAAAQAAAAAQAAA"
              "EdOVQo=";
        char* t;

	t = (char *) toBase64((unsigned char *)s, (sizeof s) - 1);
        CPPUNIT_ASSERT_EQUAL(strcmp(t, r), 0);
        free (t);

	t = (char *) toBase64((unsigned char *)s, (sizeof s) - 1);
        CPPUNIT_ASSERT_EQUAL(strcmp(t, r), 0);
}

void
PasswdTestCase::testFromBase64()
{
	char s[] = "f0VMRgEBBgAAAAQAAABsaWIvbGQtbGludXguc28uMgAABAAAABAAAAABAAA"
                   "AR05VCg==";

        char r[] = "\177ELF\1\1\6\0\0\0\4\0\0\0lib/ld-linux.so.2\0\0\4\0\0\0\020"
                   "\0\0\0\1\0\0\0GNU\n";

        char *t;
        int len;

        t = (char *) fromBase64(s, &len);

        CPPUNIT_ASSERT(strcmp(t, r) == 0);
        CPPUNIT_ASSERT(len >= (int)((sizeof r) -1));
}

void PasswdTestCase::validPw()
{
	unsigned char pass[PASSLEN + 1] = {};
	char *passText = "bogus_pw666";
        char table[3] = { '-', '@', '$' };
	int i, k;

	for(i = 0; i < (sizeof table) ; i++)
	{
                k = table[i];

		pw_enter_password(passText, pass, k);
		if (i < 2) {
	                CPPUNIT_ASSERT(strcmp(passText, (char *) pass) == 0);
		} else {
			CPPUNIT_ASSERT(strcmp(passText, (char *) pass) != 0);
		}
		CPPUNIT_ASSERT(Valid_pw(passText, pass, k));
		CPPUNIT_ASSERT(Valid_pw("", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw("a", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw("b", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw("bo", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw("bogus", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw("bogus_pw", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw("bogus_pw6666", pass, k) == 0);
		CPPUNIT_ASSERT(Valid_pw(passText, pass, k) == 1);
		CPPUNIT_ASSERT(Valid_pw("bogus_pw777", pass, k) == 0);
	}
}
