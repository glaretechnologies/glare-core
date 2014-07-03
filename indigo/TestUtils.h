/*=====================================================================
TestUtils.h
-----------
File created by ClassTemplate on Wed Jul 18 14:25:06 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"
//#include "../maths/vec3.h"
#include "../utils/StringUtils.h"
#include <string>

// Wrap test assert functions in macros, so we can print out a nice error messsage with the variable names and line number.

#define testAssert(expr) (TestUtils::doTestAssert((expr), (#expr), (__LINE__), (__FILE__)))


#define testEqual(a, b) (TestUtils::doTestEqual((a), (b), (#a), (#b), (__LINE__), (__FILE__)))

#define testEpsEqual(a, b) (TestUtils::doTestEpsEqual((a), (b), (#a), (#b), (__LINE__), (__FILE__)))
#define testEpsEqualWithEps(a, b, eps) (TestUtils::doTestEpsEqualWithEps((a), (b), (eps), (#a), (#b), (#eps), (__LINE__), (__FILE__)))

#define testMathsApproxEq(a, b) (TestUtils::doTestMathsApproxEq((a), (b), (#a), (#b), (__LINE__), (__FILE__)))
#define testApproxEq(a, b) (TestUtils::doTestApproxEq((a), (b), (#a), (#b), (__LINE__), (__FILE__)))
#define testApproxEqWithEps(a, b, eps) (TestUtils::doTestApproxEqWithEps((a), (b), (eps), (#a), (#b), (#eps), (__LINE__), (__FILE__)))


#define failTest(message) (TestUtils::doFailTest((message), (__LINE__), (__FILE__)))


namespace TestUtils
{


void doTestAssert(bool expr, const char* test, long line, const char* file);


template <class T>
inline void doTestEqual(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file);

template <class T>
inline void doTestEpsEqual(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file);
template <class T>
inline void doTestEpsEqualWithEps(const T& a, const T& b, float eps, const char* a_str, const char* b_str, const char* eps_str, long line, const char* file);

template <class T>
inline void doTestApproxEq(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file);
template <class T>
inline void doTestApproxEqWithEps(const T& a, const T& b, float eps, const char* a_str, const char* b_str, const char* eps_str, long line, const char* file);


void doFailTest(const std::string& msg, long line, const char* file);

void printMessageAndFail(const std::string& msg);


// Reads from environment variable INDIGO_TEST_REPOS_DIR, which should be set to something like 'C:\programming\indigo\trunk'
// Prints error message and exits on failure.
const std::string getIndigoTestReposDir();








//======================= Inline definitions ======================

template <class T>
void doTestEqual(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file)
{
	if(a != b)
	{
		printMessageAndFail("Test equal failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + 
			a_str + "=" + toString(a) + " was not equal to " + b_str + "=" + toString(b));
	}
}


template <class T>
void doTestEpsEqual(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file)
{
	if(!epsEqual(a, b))
	{
		printMessageAndFail("Test epsEqual failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + 
			a_str + "=" + toString(a) + " was not equal to " + b_str + "=" + toString(b));
	}
}


template <class T>
void doTestEpsEqualWithEps(const T& a, const T& b, float eps, const char* a_str, const char* b_str, const char* eps_str, long line, const char* file)
{
	if(!epsEqual(a, b, eps))
	{
		printMessageAndFail("Test epsEqual failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + 
			a_str + "=" + toString(a) + " was not equal to " + b_str + "=" + toString(b) + " with eps=" + toString(eps));
	}
}


template <class T>
void doTestMathsApproxEq(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file)
{
	if(!Maths::approxEq(a, b))
	{
		printMessageAndFail("Test approxEqual failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + 
			a_str + "=" + toString(a) + " was not equal to " + b_str + "=" + toString(b));
	}
}


/*template <class T>
void doTestApproxEq(const T& a, const T& b, const char* a_str, const char* b_str, long line, const char* file)
{
	if(!::approxEq(a, b))
	{
		printMessageAndFail("Test approxEqual failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + 
			a_str + "=" + toString(a) + " was not equal to " + b_str + "=" + toString(b));
	}
}*/


template <class T>
void doTestApproxEqWithEps(const T& a, const T& b, float eps, const char* a_str, const char* b_str, const char* eps_str, long line, const char* file)
{
	if(!Maths::approxEq(a, b, eps))
	{
		printMessageAndFail("Test approxEqual failed: " + std::string(file) + ", line " + toString((int)line) + ":\n" + 
			a_str + "=" + toString(a) + " was not equal to " + b_str + "=" + toString(b) + " with eps=" + toString(eps));
	}
}


}
