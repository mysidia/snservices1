// Copyright James Hess; All Rights Reserved

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "stuff_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION( StuffCTestCase );

char *sfgets(char*, int, FILE*);

void
StuffCTestCase::testMatch()
{
	int match(const char*, const char *);

	CPPUNIT_ASSERT(match("*", "") == 0);
	CPPUNIT_ASSERT(match("*", "*") == 0);
	CPPUNIT_ASSERT(match("*b", "*") != 0);
	CPPUNIT_ASSERT(match("*b*", "*") != 0);
	CPPUNIT_ASSERT(match("*b*", "b") == 0);
	CPPUNIT_ASSERT(match("*b*e*", "bde") == 0);
	CPPUNIT_ASSERT(match("*b?e*", "bde") == 0);
	CPPUNIT_ASSERT(match("*b?e*", "bdefe") == 0);
	CPPUNIT_ASSERT(match("*b?*?e*", "bdefe") == 0);
	CPPUNIT_ASSERT(match("*?*", "abc") == 0);
	CPPUNIT_ASSERT(match("*?*", "a") == 0);
	CPPUNIT_ASSERT(match("*???*", "abc") == 0);
	CPPUNIT_ASSERT(match("*??*", "abc") == 0);
	CPPUNIT_ASSERT(match("*???*", "ac") != 0);
	CPPUNIT_ASSERT(match("*!*@*", "a!b@c.com") == 0);
	CPPUNIT_ASSERT(match("*!*?*@*", "a!bbb@c.com") == 0);
	CPPUNIT_ASSERT(match("*!*?b?*@*", "a!bbb@c.com") == 0);
	CPPUNIT_ASSERT(match("*!*?b?*d?@*.??m", "a!bbbdx@c.com") == 0);
	CPPUNIT_ASSERT(match("*!*?b?*d?@*.???m", "a!bbbdx@c.com") != 0);
	CPPUNIT_ASSERT(match("*!*?b?*d?@*.??", "a!bbbdx@c.com") != 0);
	CPPUNIT_ASSERT(match("*!*?b?*d?@*.????*", "a!bbbdx@c.com") != 0);
	CPPUNIT_ASSERT(match("ab\\?", "abc") != 0);
	CPPUNIT_ASSERT(match("ab\\*", "abc") != 0);
	CPPUNIT_ASSERT(match("ab\\?", "ab?") == 0);
	CPPUNIT_ASSERT(match("", "ab") != 0);
	CPPUNIT_ASSERT(match("* *", "test") != 0);
	CPPUNIT_ASSERT(match("* *", "test blah") == 0);
	CPPUNIT_ASSERT(match("* *", "test ") == 0);
	CPPUNIT_ASSERT(match("* * ?", "test blah") != 0);
	CPPUNIT_ASSERT(match("* * ?", "test blah x") == 0);
	CPPUNIT_ASSERT(match("*x*x*y*z*", "x!bx@yedza") == 0);
	CPPUNIT_ASSERT(match("*x*x*y*z*", "x!bx@yed@a") != 0);
	CPPUNIT_ASSERT(match("?@*", "ab@bbb") != 0);
	CPPUNIT_ASSERT(match("*!*@*.com", "a!b@c.d.e.f.g.com") == 0);
	CPPUNIT_ASSERT(match("*!*@*?e?.nz", "d@e!gx@b.xed.nz") == 0);
	CPPUNIT_ASSERT(match("a!b@c", "d!e@f") != 0);
	CPPUNIT_ASSERT(match("a!b@c", "*!*@*") != 0);
	CPPUNIT_ASSERT(match("%!b@c", "joe!blah@foo") != 0);
	CPPUNIT_ASSERT(match("%", "abc") != 0);
	CPPUNIT_ASSERT(match("*!*@*.hmsk", "n@nu!uh@hm.hmsk") == 0);
}


void
StuffCTestCase::testStrToLower()
{
	void strtolower(char *);
	char ab[100];

	strcpy(ab, "tEstStringMessAge44 AND STUFF");
	strtolower(ab);
	CPPUNIT_ASSERT(strcmp(ab, "teststringmessage44 and stuff") == 0);
	CPPUNIT_ASSERT(strcmp(ab, "teststringmessage44 and stufF") != 0);
}



StuffCTestCase::StuffCTestCase()
{
	const char *p = tmpnam(NULL);

	if (!p || mkdir(p, 0700) < 0 || chdir(p) < 0) {
		CPPUNIT_ASSERT(0);
		abort();
	}

	fDir = strdup(p);
	if (!fDir) {
		CPPUNIT_ASSERT(0);
		abort();
	}
}

StuffCTestCase::~StuffCTestCase()
{
	chdir("..");
	rmdir(fDir);
	free(fDir);
}

/* Warning - this test is based on what sfgets() does, not what it ought to
   do. */
void
StuffCTestCase::testsfgets()
{
	FILE *fp = fopen("sfgets.test", "w");
	char str[256], *s;

	CPPUNIT_ASSERT(fp);
	fprintf(fp, "0 1 23456 789 abc 546\n");
	fprintf(fp, "9 8 76 456 321 54 400 58\n");
	fprintf(fp, "lalalal\n");
	fprintf(fp, "\n");
	fprintf(fp, ".\n");
	fprintf(fp, "\n");
	fclose(fp);

	fp = fopen("sfgets.test", "r");
	CPPUNIT_ASSERT(fp);

	s = sfgets(str, 24, fp);
	CPPUNIT_ASSERT(s == str);
	CPPUNIT_ASSERT(!strcmp(s, "0 1 23456 789 abc 546"));

	s = sfgets(str, 7, fp);
	CPPUNIT_ASSERT(s == str);
	CPPUNIT_ASSERT(s);
	CPPUNIT_ASSERT(!strcmp(s, "9 8 7"));

	s = sfgets(str, 5, fp);
	CPPUNIT_ASSERT(s == str);
	CPPUNIT_ASSERT(s);
	CPPUNIT_ASSERT(!strcmp(s, " 45"));
	/* CPPUNIT_ASSERT(!strcmp(s, "6 456")); */

	s = sfgets(str, 80, fp);
	CPPUNIT_ASSERT(s == str);
	CPPUNIT_ASSERT(s);
	CPPUNIT_ASSERT(!strcmp(s, " 321 54 400 58"));

	s = sfgets(str, 80, fp);
	CPPUNIT_ASSERT(s == str);
	CPPUNIT_ASSERT(s);
	CPPUNIT_ASSERT(!strcmp(s, "lalalal"));

	s = sfgets(str, 80, fp);
	CPPUNIT_ASSERT(s && s == str);
	CPPUNIT_ASSERT(!strcmp(s, ""));

	s = sfgets(str, 80, fp);
	CPPUNIT_ASSERT(s && s == str);
	CPPUNIT_ASSERT(!strcmp(s, "."));

	s = sfgets(str, 80, fp);
	CPPUNIT_ASSERT(s && s == str);
	CPPUNIT_ASSERT(!strcmp(s, ""));

	/* printf("[%s]\n", s); */

	fclose(fp);
	unlink("sfgets.test");
}


void
StuffCTestCase::testxor()
{
	char *xorit(char *);
	char buf[256], *s;
	int x;

	strcpy(buf, "xorTestBufferStringAndS;uchOk?\b");
	s = xorit(buf);

	CPPUNIT_ASSERT(s == buf);

	x = 0;
	CPPUNIT_ASSERT(buf[x++] == 'c');
	CPPUNIT_ASSERT(buf[x++] == 't');
	CPPUNIT_ASSERT(buf[x++] == 'i');
	CPPUNIT_ASSERT(buf[x++] == 'O');
	CPPUNIT_ASSERT(buf[x++] == '~');
	CPPUNIT_ASSERT(buf[x++] == 'h');
	CPPUNIT_ASSERT(buf[x++] == 'o');
	CPPUNIT_ASSERT(buf[x++] == 'Y');
	CPPUNIT_ASSERT(buf[x++] == 'n');
	CPPUNIT_ASSERT(buf[x++] == '}');
	CPPUNIT_ASSERT(buf[x++] == '}');
	CPPUNIT_ASSERT(buf[x++] == '~');
	CPPUNIT_ASSERT(buf[x++] == 'i');
	CPPUNIT_ASSERT(buf[x++] == 'H');
	CPPUNIT_ASSERT(buf[x++] == 'o');
	CPPUNIT_ASSERT(buf[x++] == 'i');
	CPPUNIT_ASSERT(buf[x++] == 'r');
	CPPUNIT_ASSERT(buf[x++] == 'u');
	CPPUNIT_ASSERT(buf[x++] == '|');
	CPPUNIT_ASSERT(buf[x++] == 'Z');
	CPPUNIT_ASSERT(buf[x++] == 'u');
	CPPUNIT_ASSERT(buf[x++] == '\177');
	CPPUNIT_ASSERT(buf[x++] == 'H');
	CPPUNIT_ASSERT(buf[x++] == ';');
	CPPUNIT_ASSERT(buf[x++] == 'n');
	CPPUNIT_ASSERT(buf[x++] == 'x');
	CPPUNIT_ASSERT(buf[x++] == 's');
	CPPUNIT_ASSERT(buf[x++] == 'T');
	CPPUNIT_ASSERT(buf[x++] == 'p');
	CPPUNIT_ASSERT(buf[x++] == '$');
}

void
StuffCTestCase::testparse_str()
{
	void parse_str(char **, int, int, char *, size_t);
	char *args[] = {
		 "junkarg-1", "bcdef-2", "ghi 3",
		 "jkl_$$^@#%@#4", "madgja65", "a^I$A#_^(",
		 "BKCLZ:JG($#AIT", "ATRK$I(#)TIA#()", NULL
	};
	char buf[256];

	buf[0] = '\0';
	parse_str(args, 3, 1, buf, 255);
	CPPUNIT_ASSERT(!strcmp(buf, "bcdef-2 ghi 3"));

	parse_str(args, 4, 1, buf, 14);
	CPPUNIT_ASSERT(!strcmp(buf, "bcdef-2 "));

	parse_str(args, 8, 1, buf, 255);
	CPPUNIT_ASSERT(!strcmp(buf, "bcdef-2 ghi 3 jkl_$$^@#%@#4 madgja65 a^I$A#_^( BKCLZ:JG($#AIT ATRK$I(#)TIA#()"));

	parse_str(args, 7, 0, buf, 255);
	CPPUNIT_ASSERT(!strcmp(buf, "junkarg-1 bcdef-2 ghi 3 jkl_$$^@#%@#4 madgja65 a^I$A#_^( BKCLZ:JG($#AIT"));
}

void
StuffCTestCase::testsnprintf()
{
	char buf[256];

	snprintf(buf, 256, "xxxxxxxxxx");
	CPPUNIT_ASSERT(!strcmp(buf, "xxxxxxxxxx"));

	snprintf(buf, 2, "%d", 666);
	CPPUNIT_ASSERT(buf[2] == 'x');

	snprintf(buf, 2, "%-3d", 666);
	CPPUNIT_ASSERT(buf[2] == 'x');

	snprintf(buf, 2, "%s", "666");
	CPPUNIT_ASSERT(buf[2] == 'x');

	snprintf(buf, 3, "%s", "666");
	CPPUNIT_ASSERT(buf[2] == '\0');
}

void
StuffCTestCase::testBase64()
{
	u_char *toBase64(const u_char *, size_t);
	u_char *fromBase64(const char *, int *);
	u_char *p;
	int z;

	p = toBase64((const u_char*)"Test Message\nTo Encode!\nzxcvlkjasdfa", 24);
	CPPUNIT_ASSERT(p && !strcmp((const char*)p,"VGVzdCBNZXNzYWdlClRvIEVuY29kZSEK"));
	free(p);

	p = fromBase64((const char *)"VGVzdCBNZXNzYWdlClRvIEVuY29kZSEK", &z);
	CPPUNIT_ASSERT(p && !strcmp((const char *)p,"Test Message\nTo Encode!\n"));
	free(p);
}
