// Copyright James Hess, All Rights Reserved

#include <iostream>
#include <string>
#include <strstream>
#include "timestr_test.h"
#define eq(x,y) (!strcasecmp(x,y))

CPPUNIT_TEST_SUITE_REGISTRATION( TimeStrTestCase );

void 
TimeStrTestCase::testOldStyle()
{
	TimeLengthString tls("3", false);

	CPPUNIT_ASSERT(tls.isValid() == false);

	tls = TimeLengthString("3", true);
	CPPUNIT_ASSERT(tls.isValid() == true);
	CPPUNIT_ASSERT(tls.getSeconds() == 0 && tls.getMinutes() == 0);
	CPPUNIT_ASSERT(tls.getDays() == 0);
	CPPUNIT_ASSERT(tls.getHours() == 3);
	CPPUNIT_ASSERT(tls.getTotalSeconds() == 3*3600);

	CPPUNIT_ASSERT(TimeLengthString("99999999999999999999999999999").
                       isValid() == false);

	CPPUNIT_ASSERT(TimeLengthString("106144390400000").isValid() == false);

	CPPUNIT_ASSERT(TimeLengthString("4x", true).isValid() == false);
	CPPUNIT_ASSERT(TimeLengthString("4h5", true).isValid() == false);
	CPPUNIT_ASSERT(TimeLengthString("4h5d", true).isValid() == true);
	CPPUNIT_ASSERT(TimeLengthString("4h5", false).isValid() == false);
	CPPUNIT_ASSERT(TimeLengthString("4h5m", true).isValid() == true);
	CPPUNIT_ASSERT(TimeLengthString("4.5", true).isValid() == false);
	CPPUNIT_ASSERT(TimeLengthString("4h5m36x", true).isValid() == false);
	CPPUNIT_ASSERT(TimeLengthString("4h5m36m", true).isValid() == true);

	CPPUNIT_ASSERT(TimeLengthString("4h5m36m", true).getMinutes() == 36);
	CPPUNIT_ASSERT(TimeLengthString("4h5m36m15s5s", true).getTotalSeconds() 
                                   == 4*3600+36*60+5 );
}

void
TimeStrTestCase::testParse()
{
	TimeLengthString tls("2d23h5m42s");
	char buf[255];
	const char* l;

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

	tls = TimeLengthString(60);
        CPPUNIT_ASSERT((l=tls.asString(buf, 50, true, true, true)) != NULL);

        CPPUNIT_ASSERT(tls.getHours() == 0);
        CPPUNIT_ASSERT(tls.getMinutes() == 1);
        CPPUNIT_ASSERT(tls.getSeconds() == 0);
	CPPUNIT_ASSERT(eq(l, "   0 days,  0 hours,  1 minutes, and  0 seconds"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"1m"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"1m"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, true, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"   0d00h01m"));

//        const char* TimeLengthString::asString(char* buf, int len, bool pad,
//                        bool long_format, bool show_secs) const;

        tls = TimeLengthString(66);
        CPPUNIT_ASSERT(tls.getHours() == 0);
        CPPUNIT_ASSERT(tls.getMinutes() == 1);
        CPPUNIT_ASSERT(tls.getSeconds() == 6);
	CPPUNIT_ASSERT((l=tls.asString(buf, 50, true, true, true)) != NULL);
	CPPUNIT_ASSERT(eq(l, "   0 days,  0 hours,  1 minutes, and  6 seconds"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"1m"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"1m6s"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, true, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"   0d00h01m"));

	tls = TimeLengthString(15*3600*24 + 23*3600 + 45*60 + 15);
	CPPUNIT_ASSERT(tls.getDays() == 15);
	CPPUNIT_ASSERT(tls.getHours() == 23);
	CPPUNIT_ASSERT(tls.getMinutes() == 45);
	CPPUNIT_ASSERT(tls.getSeconds() == 15);
	CPPUNIT_ASSERT((l=tls.asString(buf, 50, true, true, true)) != NULL);
	CPPUNIT_ASSERT(eq(l, "  15 days, 23 hours, 45 minutes, and 15 seconds"));

	CPPUNIT_ASSERT((l=tls.asString(buf, 50, true, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"  15d23h45m"));
	
	CPPUNIT_ASSERT((l=tls.asString(buf, 50, false, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"15d23h45m"));

	CPPUNIT_ASSERT((l=tls.asString(buf, 50, true, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"  15d23h45m15s"));

	CPPUNIT_ASSERT((l=tls.asString(buf, 50, false, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"15d23h45m15s"));

	tls = TimeLengthString(15*3600*24 + 45*60 + 15);
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, true)) != NULL);
	CPPUNIT_ASSERT(eq(l,"15d45m15s"));
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"15d45m"));

	tls = TimeLengthString(15*3600*24 + 23*3600 + 15);
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"15d23h"));

	tls = TimeLengthString(23*3600 + 45*60 + 15);
	CPPUNIT_ASSERT((l=tls.asString(buf, 25, false, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"23h45m"));

	CPPUNIT_ASSERT((l=tls.asString(buf, 25, true, false, false)) != NULL);
	CPPUNIT_ASSERT(eq(l,"   0d23h45m"));
}

