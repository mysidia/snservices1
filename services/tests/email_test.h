#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

#include <cppunit/extensions/HelperMacros.h>

#include "../email.h"
#include "../options.h"

class EmailTestCase : public CppUnit::TestCase
{
	CPPUNIT_TEST_SUITE( EmailTestCase );
	CPPUNIT_TEST( testStringBuf );
	CPPUNIT_TEST( testAddressBuf );
	CPPUNIT_TEST( testReset );
	CPPUNIT_TEST_SUITE_END();
	public:

	void testStringBuf();
	void testAddressBuf();
	void testReset();
	void testLength();

	EmailTestCase() { }
	virtual ~EmailTestCase() { }
	virtual void setUp() { }
	virtual void tearDown() { }
	void testUsingCheckIt() { checkIt(); }

	protected:
	virtual void checkIt() { }

	private:
	EmailTestCase(const EmailTestCase&);
	void operator =(const EmailTestCase&);

};
