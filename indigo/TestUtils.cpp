/*=====================================================================
TestUtils.cpp
-------------
File created by ClassTemplate on Wed Jul 18 14:25:06 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TestUtils.h"


#include "globals.h"
#include "../utils/stringutils.h"


void doTestAssert(bool expr, const char* test, long line, const char* file)
{
	if(!expr)
	{
		conPrint("Test Assertion Failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + std::string(test));
		assert(0);
		exit(0);
	}
}
