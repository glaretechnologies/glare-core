/*=====================================================================
CheckedMaths.cpp
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "CheckedMaths.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"



void CheckedMaths::test()
{
	try
	{
		testAssert(addUnsignedInts(1u, 2u) == 3u);

		testAssert(addUnsignedInts(1u << 31, (1u << 31) - 1) == 4294967295);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Check additions that should overflow / wrap
	try
	{
		testAssert(addUnsignedInts(1u << 31, (1u << 31)) == 4294967295);
		failTest("Expected excep");
	}
	catch(glare::Exception&)
	{
	}

}


#endif // BUILD_TESTS
