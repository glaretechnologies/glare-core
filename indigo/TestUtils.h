/*=====================================================================
TestUtils.h
-----------
File created by ClassTemplate on Wed Jul 18 14:25:06 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TESTUTILS_H_666_
#define __TESTUTILS_H_666_

#include <string>


#define testAssert(expr) (doTestAssert((expr), (#expr), (__LINE__), (__FILE__)))

void doTestAssert(bool expr, const char* test, long line, const char* file);

#define failTest(message) (doFailTest((message), (__LINE__), (__FILE__)))

void doFailTest(const std::string& msg, long line, const char* file);

namespace TestUtils
{

	
// Reads from environment variable INDIGO_TEST_REPOS_DIR, which should be set to something like 'C:\programming\indigo\trunk'
// Prints error message and exits on failure.
const std::string getIndigoTestReposDir();


}


#endif //__TESTUTILS_H_666_
