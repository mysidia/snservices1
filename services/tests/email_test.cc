// Copyright James Hess, All Rights Reserved

#include <iostream>
#include <string>
#include <strstream>
#include "email_test.h"
#define eq(x,y) (!strcasecmp(x,y))

CPPUNIT_TEST_SUITE_REGISTRATION( EmailTestCase );

void
EmailTestCase::testStringBuf()
{
	EmailString s;

	CPPUNIT_ASSERT(eq(s.get_string(),""));

	s = "foo";
	CPPUNIT_ASSERT(eq(s.get_string(), "foo"));

	s += "TeSt";
	CPPUNIT_ASSERT(eq(s.get_string(), "fooTeSt"));

	s[0] = 'x';
	CPPUNIT_ASSERT(eq( s.get_string(), "xooTeSt"));
	CPPUNIT_ASSERT( s[0] == 'x' );

	s = "a";
	CPPUNIT_ASSERT(eq(s.get_string(), "a"));
	CPPUNIT_ASSERT( s[1] == '\0' );

	s.add("bcd");
	CPPUNIT_ASSERT(eq(s.get_string(), "abcd"));

	s.set_string("cde");
	CPPUNIT_ASSERT(eq(s.get_string(), "cde"));
	CPPUNIT_ASSERT(eq(s.get_string(), s.get_string()));
	CPPUNIT_ASSERT(!eq(s.get_string(), "ghi"));
	CPPUNIT_ASSERT(!eq(s.get_string(), ""));
}

void
EmailTestCase::testAddressBuf()
{
	CPPUNIT_ASSERT_EQUAL( 0, 0 );
}

void
EmailTestCase::testLength()
{
	EmailString q;

	q = NULL;
	CPPUNIT_ASSERT_EQUAL( q.length(), 0 );

	q="";
	CPPUNIT_ASSERT_EQUAL( q.length(), 0 );

	q="ab";
	CPPUNIT_ASSERT_EQUAL( q.length(), 2 );

	q+="czzz";
	CPPUNIT_ASSERT_EQUAL( q.length(), 6 );

	q.add("hmm");
	CPPUNIT_ASSERT_EQUAL( q.length(), 9 );

	q.set_string((char *)0);
	CPPUNIT_ASSERT_EQUAL( q.length(), 0 );

	char* y = static_cast<char *>(malloc(10));
	strcpy(y, "abcDEUAFIOW85290");
	q.set_string_ptr(y);
	CPPUNIT_ASSERT_EQUAL( q.length(), (int)strlen(y) );

}

void
EmailTestCase::testReset()
{
	EmailMessage message;

	message.from = "abc@def";
	message.to = "fgh@jkl";
	message.subject = "hi";
	message.body = "blah\n";
	message.body += "blah\n";
	CPPUNIT_ASSERT(eq(message.body.get_string(), "blah\nblah\n"));
	CPPUNIT_ASSERT(eq(message.subject.get_string(), "hi"));
	CPPUNIT_ASSERT(eq(message.to.get_string(), "fgh@jkl"));
	CPPUNIT_ASSERT(eq(message.from.get_string(), "abc@def"));

	CPPUNIT_ASSERT(!eq(message.from.get_string(), ""));
	CPPUNIT_ASSERT(!eq(message.to.get_string(), ""));
	CPPUNIT_ASSERT(!eq(message.subject.get_string(), ""));
	CPPUNIT_ASSERT(!eq(message.body.get_string(), ""));

	message.reset();
	CPPUNIT_ASSERT(eq(message.from.get_string(), ""));
	CPPUNIT_ASSERT(eq(message.to.get_string(), ""));
	CPPUNIT_ASSERT(eq(message.subject.get_string(), ""));
	CPPUNIT_ASSERT(eq(message.body.get_string(), ""));
}

