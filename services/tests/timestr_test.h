#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

#include <cppunit/extensions/HelperMacros.h>

#include "../timestr.h"
#include "../options.h"

class TimeStrTestCase : public CppUnit::TestCase
{
	CPPUNIT_TEST_SUITE( TimeStrTestCase );
	CPPUNIT_TEST( testParse );
	CPPUNIT_TEST_SUITE_END();
	public:

	void testParse();

	TimeStrTestCase() { }
	virtual ~TimeStrTestCase() { }
	virtual void setUp() { }
	virtual void tearDown() { }
	void testUsingCheckIt() { checkIt(); }

	protected:
	virtual void checkIt() { }

	private:
	TimeStrTestCase(const TimeStrTestCase&);
	void operator =(const TimeStrTestCase&);

};
