/*=====================================================================
Vec4i.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-10 21:02:10 +0000
=====================================================================*/
#include "Vec4i.h"


#include "../utils/StringUtils.h"


const std::string Vec4i::toString() const
{
	return "(" + ::toString(x[0]) + "," + ::toString(x[1]) + "," + ::toString(x[2]) + "," + ::toString(x[3]) + ")";
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/CycleTimer.h"


void Vec4i::test()
{
	conPrint("Vec4i::test()");

	// Test elem
	{
		const Vec4i a(1, 2, 3, 4);

		testAssert(elem<0>(a) == 1);
		testAssert(elem<1>(a) == 2);
		testAssert(elem<2>(a) == 3);
		testAssert(elem<3>(a) == 4);
	}
}


#endif
