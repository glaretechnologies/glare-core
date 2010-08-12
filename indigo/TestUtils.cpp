/*=====================================================================
TestUtils.cpp
-------------
File created by ClassTemplate on Wed Jul 18 14:25:06 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TestUtils.h"


#include "globals.h"
#include "../utils/stringutils.h"
#include "../utils/platformutils.h"
#include <stdlib.h>


#if (BUILD_TESTS)
void doTestAssert(bool expr, const char* test, long line, const char* file)
{
	if(!expr)
	{
		conPrint("Test Assertion Failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + std::string(test));
		assert(0);
		exit(1);
	}
}


void doFailTest(const std::string& msg, long line, const char* file)
{
	conPrint("Test Failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + msg);
	assert(0);
	exit(1);
}


namespace TestUtils
{


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
		conPrint("Failed to read environment variable INDIGO_TEST_REPOS_DIR");
		assert(0);
		exit(1);
	}
}


}
#endif