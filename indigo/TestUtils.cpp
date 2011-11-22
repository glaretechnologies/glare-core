/*=====================================================================
TestUtils.cpp
-------------
File created by ClassTemplate on Wed Jul 18 14:25:06 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TestUtils.h"


#if defined(BUILD_TESTS)


#include "../utils/stringutils.h"
#include "../utils/platformutils.h"
#include <stdlib.h>
#include <iostream>


namespace TestUtils
{


void doTestAssert(bool expr, const char* test, long line, const char* file)
{
	if(!expr)
	{
		std::cout << ("Test Assertion Failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + std::string(test)) << std::endl;
		assert(0);
		exit(1);
	}
}


void doFailTest(const std::string& msg, long line, const char* file)
{
	std::cout << ("Test Failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + msg) << std::endl;
	assert(0);
	exit(1);
}


// Reads from environment variable INDIGO_TEST_REPOS_DIR
// Prints error message and exits on failure.
const std::string getIndigoTestReposDir()
{
	try
	{
		return PlatformUtils::getEnvironmentVariable("INDIGO_TEST_REPOS_DIR");
	}
	catch(PlatformUtils::PlatformUtilsExcep& )
	{
		std::cout << ("Failed to read environment variable INDIGO_TEST_REPOS_DIR") << std::endl;
		assert(0);
		exit(1);
	}
}


}

#endif
