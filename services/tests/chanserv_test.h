#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/extensions/HelperMacros.h>

#ifndef _TESTMODE
#define _TESTMODE
#endif

// #include "../options.h"

class ChanServTestCase : public CppUnit::TestCase
{
	CPPUNIT_TEST_SUITE( ChanServTestCase );
	CPPUNIT_TEST( testchanlist );
	CPPUNIT_TEST( testregchanlist );
	CPPUNIT_TEST( testboplist );
	CPPUNIT_TEST( testbanlist );
	CPPUNIT_TEST_SUITE_END();
	public:

	void testchanlist();
	void testboplist();
	void testregchanlist();
	void testbanlist();

	ChanServTestCase();
	virtual ~ChanServTestCase();
	virtual void setUp() { }
	virtual void tearDown() { }
	void testUsingCheckIt() { checkIt(); }

	protected:
	virtual void checkIt() { }

	private:
	char *fDir;
	ChanServTestCase(const ChanServTestCase&);
	void operator =(const ChanServTestCase&);

};
