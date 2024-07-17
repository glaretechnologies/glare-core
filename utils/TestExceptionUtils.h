/*=====================================================================
TestExceptionUtils.h
--------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include <functional>


// Use like:
// testExceptionExpected([&]() { doTestThatThrowsExcep(); });
inline void testExceptionExpected(std::function<void()> test_func)
{
	try
	{
		test_func(); // Execute the test code.
		failTest("Excep expected");
	}
	catch(glare::Exception& e)
	{
		conPrint("Caught expected excep: " + e.what());
	}
}


// Use like:
// testThrowsExcepContainingString([&]() { doTestThatThrowsExcep(); }, "msg that should be in exception");
inline void testThrowsExcepContainingString(std::function<void()> test_func, const std::string& str)
{
	try
	{
		test_func(); // Execute the test code.
		failTest("Excep expected");
	}
	catch(glare::Exception& e)
	{
		conPrint("Caught expected excep: " + e.what());
		if(!StringUtils::containsString(e.what(), str))
		{
			TestUtils::printMessageAndFail("Test failed: the exception message '" + e.what() + "' did not contain the string '" + str + "'.");
		}
	}
}
