#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

#include <cppunit/extensions/HelperMacros.h>

#include "../timestr.h"
#include "../options.h"

class PasswdTestCase : public CppUnit::TestCase
{
	CPPUNIT_TEST_SUITE( PasswdTestCase );
	CPPUNIT_TEST( testToBase64 );
	CPPUNIT_TEST( testFromBase64 );
	CPPUNIT_TEST( validPw );
	CPPUNIT_TEST_SUITE_END();
	public:

	void testToBase64();
	void testFromBase64();
	void validPw();

	PasswdTestCase() { }
	virtual ~PasswdTestCase() { }
	virtual void setUp() { }
	virtual void tearDown() { }
	void testUsingCheckIt() { checkIt(); }

	protected:
	virtual void checkIt() { }

	private:
	PasswdTestCase(const PasswdTestCase&);
	void operator =(const PasswdTestCase&);

};
