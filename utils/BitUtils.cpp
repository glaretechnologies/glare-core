/*=====================================================================
BitUtils.cpp
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "BitUtils.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/ConPrint.h"
#include "../utils/CycleTimer.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"


namespace BitUtils
{


void test()
{
	conPrint("BitUtils::test()");

	//===================================== bitCast =====================================
	testAssert(bitCast<uint32>(1.0f) == 0x3f800000);
	testAssert(bitCast<uint32>(5) == 5u);
	testAssert(bitCast<uint32>(-1) == 0xFFFFFFFFu); // This assumes the signed integer representation is two's complement of course.

	//===================================== lowestSetBitIndex =====================================
	testAssert(lowestSetBitIndex(0u) == 0); // 0
	testAssert(lowestSetBitIndex(1u) == 0); // 1
	testAssert(lowestSetBitIndex(2u) == 1); // 10
	testAssert(lowestSetBitIndex(3u) == 0); // 11
	testAssert(lowestSetBitIndex(4u) == 2); // 100
	testAssert(lowestSetBitIndex(5u) == 0); // 101
	testAssert(lowestSetBitIndex(6u) == 1); // 110
	testAssert(lowestSetBitIndex(7u) == 0); // 111
	testAssert(lowestSetBitIndex(8u) == 3); // 1000


	testAssert(lowestSetBitIndex(0x80000000u) == 31); // MSB bit set only
	testAssert(lowestSetBitIndex(0xFFFFFFFFu) == 0); // All bits set

	//===================================== lowestZeroBitIndex =====================================
	testAssert(lowestZeroBitIndex(0u) == 0); // 0
	testAssert(lowestZeroBitIndex(1u) == 1); // 1
	testAssert(lowestZeroBitIndex(2u) == 0); // 10
	testAssert(lowestZeroBitIndex(3u) == 2); // 11
	testAssert(lowestZeroBitIndex(4u) == 0); // 100
	testAssert(lowestZeroBitIndex(5u) == 1); // 101
	testAssert(lowestZeroBitIndex(6u) == 0); // 110
	testAssert(lowestZeroBitIndex(7u) == 3); // 111
	testAssert(lowestZeroBitIndex(8u) == 0); // 1000


	testAssert(lowestZeroBitIndex(0x80000000u) == 0); // Most significant bit set only.
	testAssert(lowestZeroBitIndex(0x7FFFFFFFu) == 31); // Only most significant bit set set to zero.
	testAssert(lowestZeroBitIndex(0xFFFFFFFFu) == 0); // All bits set

	//===================================== HighestSetBitIndex =====================================
	testAssert(highestSetBitIndex(1) == 0); // 1
	testAssert(highestSetBitIndex(2) == 1); // 10
	testAssert(highestSetBitIndex(3) == 1); // 11
	testAssert(highestSetBitIndex(4) == 2); // 100
	testAssert(highestSetBitIndex(5) == 2); // 101
	testAssert(highestSetBitIndex(6) == 2); // 110
	testAssert(highestSetBitIndex(7) == 2); // 111
	testAssert(highestSetBitIndex(8) == 3); // 1000

	testAssert(highestSetBitIndex(0xFFFFFFFFull) == 31);
	testAssert(highestSetBitIndex(0xFFFFFFFFFFFFFFFFull) == 63); // All bits set

	for(uint32 z=0; z<63; ++z)
		testAssert(highestSetBitIndex((uint64)1 << (uint64)z) == z);

	//===================================== isBitSet =====================================
	{
		testAssert(!isBitSet(2, 1 << 0));
		testAssert( isBitSet(2, 1 << 1));
		testAssert(!isBitSet(2, 1 << 2));
		testAssert(!isBitSet(2, 1 << 3));
	}

	//===================================== setBit =====================================
	{
		uint32 x = 0;
		setBit(x, 1u << 7);

		testAssert(x == 1u << 7);

		setBit(x, 1u << 3);

		testAssert(x == ((1u << 7) | (1u << 3)));

		setBit(x, 1u << 31);

		testAssert(x == ((1u << 7) | (1u << 3) | (1u << 31)));
	}

	//===================================== zeroBit =====================================
	{
		uint32 x = 1u << 7;
		zeroBit(x, 1u << 7);
		testAssert(x == 0);

		x = (1u << 7) | (1u << 3);
		zeroBit(x, 1u << 3);
		testAssert(x == 1u << 7);
	}

	//===================================== setOrZeroBit =====================================
	{
		uint32 x = 1u << 7;
		setOrZeroBit(x, 1u << 7, false);
		testAssert(x == 0);

		x = (1u << 7) | (1u << 3);
		setOrZeroBit(x, 1u << 3, false);
		testAssert(x == (1u << 7));

		setOrZeroBit(x, 1u << 2, true);
		testAssert(x == ((1u << 7) | (1u << 2)));
	}



	// Do a performance test of lowestSetBitIndex():
	// On Nick's Ivy Bridge i7:
	// sum: 9999985
	// elapsed: 2.2220452 cycles
	/*{
		//Timer timer;
		CycleTimer cycle_timer;

		int sum = 0;
		const int n = 10000000;
		for(int i=0; i<n; ++i)
		{
			sum += lowestSetBitIndex(i);
		}

		int64_t cycles = cycle_timer.elapsed();
		//const double elapsed = timer.elapsed();

		printVar(sum);

		//conPrint("elapsed: " + toString(1.0e9 * elapsed / n) + " ns");
		conPrint("elapsed: " + toString((float)cycles / n) + " cycles");
	}*/

}


} // end namespace BitUtils


#endif
