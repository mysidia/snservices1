#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cppunit/extensions/HelperMacros.h>

// #include "../options.h"

class StuffCTestCase : public CppUnit::TestCase
{
	CPPUNIT_TEST_SUITE( StuffCTestCase );
	CPPUNIT_TEST( testMatch );
	CPPUNIT_TEST( testStrToLower );
	CPPUNIT_TEST( testsfgets );
	CPPUNIT_TEST( testxor );
	CPPUNIT_TEST( testparse_str );
	CPPUNIT_TEST( testsnprintf );
	CPPUNIT_TEST( testBase64 );
	CPPUNIT_TEST_SUITE_END();
	public:

	void testMatch();
	void testStrToLower();
	void testsfgets();
	void testxor();
	void testparse_str();
	void testsnprintf();
	void testBase64();

	StuffCTestCase();
	virtual ~StuffCTestCase();
	virtual void setUp() { }
	virtual void tearDown() { }
	void testUsingCheckIt() { checkIt(); }

	protected:
	virtual void checkIt() { }

	private:
	char *fDir;
	StuffCTestCase(const StuffCTestCase&);
	void operator =(const StuffCTestCase&);

};
