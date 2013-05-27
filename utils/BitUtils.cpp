/*=====================================================================
BitUtils.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-05-27 18:54:17 +0100
=====================================================================*/
#include "BitUtils.h"




namespace BitUtils
{








} // end namespace BitUtils


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


namespace BitUtils
{


void test()
{
	//===================================== lowestSetBitIndex =====================================
	testAssert(lowestSetBitIndex(0) == 0); // 0
	testAssert(lowestSetBitIndex(1) == 0); // 1
	testAssert(lowestSetBitIndex(2) == 1); // 10
	testAssert(lowestSetBitIndex(3) == 0); // 11
	testAssert(lowestSetBitIndex(4) == 2); // 100
	testAssert(lowestSetBitIndex(5) == 0); // 101
	testAssert(lowestSetBitIndex(6) == 1); // 110
	testAssert(lowestSetBitIndex(7) == 0); // 111
	testAssert(lowestSetBitIndex(8) == 3); // 1000


	testAssert(lowestSetBitIndex(0x80000000u) == 31); // MSB bit set only
	testAssert(lowestSetBitIndex(0xFFFFFFFFu) == 0); // All bits set

	//===================================== lowestZeroBitIndex =====================================
	testAssert(lowestZeroBitIndex(0) == 0); // 0
	testAssert(lowestZeroBitIndex(1) == 1); // 1
	testAssert(lowestZeroBitIndex(2) == 0); // 10
	testAssert(lowestZeroBitIndex(3) == 2); // 11
	testAssert(lowestZeroBitIndex(4) == 0); // 100
	testAssert(lowestZeroBitIndex(5) == 1); // 101
	testAssert(lowestZeroBitIndex(6) == 0); // 110
	testAssert(lowestZeroBitIndex(7) == 3); // 111
	testAssert(lowestZeroBitIndex(8) == 0); // 1000


	testAssert(lowestZeroBitIndex(0x80000000u) == 0); // Most significant bit set only.
	testAssert(lowestZeroBitIndex(0x7FFFFFFFu) == 31); // Only most significant bit set set to zero.
	testAssert(lowestZeroBitIndex(0xFFFFFFFFu) == 0); // All bits set
}







} // end namespace BitUtils


#endif
