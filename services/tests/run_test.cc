#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include "email_test.h"
#include "stuff_test.h"

int main()
{
	CppUnit::TextTestRunner runner;

	runner.addTest( CppUnit::TestFactoryRegistry::getRegistry().makeTest() );
	runner.run();

	return 0;
}
