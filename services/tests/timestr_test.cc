// Copyright James Hess, All Rights Reserved

#include <iostream>
#include <string>
#include <strstream>
#include "timestr_test.h"
#define eq(x,y) (!strcasecmp(x,y))

CPPUNIT_TEST_SUITE_REGISTRATION( TimeStrTestCase );

void
TimeStrTestCase::testParse()
{
	TimeLengthString tls("2d23h5m42s");
	char buf[255], *l;

	CPPUNIT_ASSERT(tls.isValid());
	CPPUNIT_ASSERT(tls.getDays() == 2);

	CPPUNIT_ASSERT(tls.getHours() == 23);
	CPPUNIT_ASSERT(tls.getMinutes() == 5);
	CPPUNIT_ASSERT(tls.getSeconds() == 42);

	CPPUNIT_ASSERT(tls.getTotalSeconds() == (2*24*3600 + 23*3600 + 5*60 + 42));
	CPPUNIT_ASSERT(tls.getTotalMinutes() == (2*24*60 + 23*60 + 5));
	CPPUNIT_ASSERT(tls.getTotalHours() == (2*24 + 23));

	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"2d23h5m42s"));

	CPPUNIT_ASSERT((l=tls.asString(buf, 25, true, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"   2d23h05m42s"));

	CPPUNIT_ASSERT((l=tls.asString(buf, 10, true, true, true)) == NULL);
	CPPUNIT_ASSERT((l=tls.asString(buf, 50, true, true, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"   2 days, 23 hours,  5 minutes, and 42 seconds"));

}

