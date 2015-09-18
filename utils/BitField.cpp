/*=====================================================================
BitField.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "BitField.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/CycleTimer.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include <bitset>


void bitFieldTests()
{
	conPrint("bitFieldTests()");

	{
		BitField<uint32> bf(0);
		testAssert(bf.getBit(0) == 0);
		bf.setBit(0, 1);
		testAssert(bf.getBit(0) == 1);
		bf.setBit(0, 0);
		testAssert(bf.getBit(0) == 0);
	}

	// Start with zeroed bitfield, set a single bit, check
	for(uint32 i=0; i<32; ++i)
	{
		BitField<uint32> bf(0);
		bf.setBit(i, 1);
		for(uint32 z=0; z<32; ++z)
		{
			testAssert(bf.getBit(z) == (z == i ? 1u : 0));
			
			testAssert(bf.getBitMasked(z) == (z == i ? (1u << i) : 0));
		}
	}

	// Start with one-d bitfield, zero a single bit, check
	for(uint32 i=0; i<32; ++i)
	{
		BitField<uint32> bf(0xFFFFFFFFu);
		bf.setBit(i, 0);
		for(uint32 z=0; z<32; ++z)
			testAssert(bf.getBit(z) == (z == i ? 0 : 1u));
	}


	//=================== Test setBitPair(), getBitPair() ===================

	{
		BitField<uint32> bf(0);
		for(int i=0; i<31; ++i)
			testAssert(bf.getBitPair(i) == 0u);
	}

	{
		BitField<uint32> bf(0xFFFFFFFFu);
		for(int i=0; i<31; ++i)
			testAssert(bf.getBitPair(i) == 3u);
	}

	{
		BitField<uint32> bf(0);
		bf.setBitPair(2, 3);
		testAssert(bf.getBit(0) == 0);
		testAssert(bf.getBit(1) == 0);
		testAssert(bf.getBit(2) == 1);
		testAssert(bf.getBit(3) == 1);
		testAssert(bf.getBit(4) == 0);
		testAssert(bf.getBit(5) == 0);
	}



	// Start with zeroed bitfield, set a bit pair, check
	for(int i=0; i<32; i += 2)
	{
		for(uint32 bitpairval = 0; bitpairval < 4; ++bitpairval)
		{
			BitField<uint32> bf(0);
			bf.setBitPair(i, bitpairval);
			for(int z=0; z<32; z += 2)
				testAssert(bf.getBitPair(z) == (z == i ? bitpairval : 0));
		}
	}

	// Start with one-d bitfield, set a bit pair, check
	for(int i=0; i<32; i += 2)
	{
		for(uint32 bitpairval = 0; bitpairval < 4; ++bitpairval)
		{
			BitField<uint32> bf(0xFFFFFFFFu);
			bf.setBitPair(i, bitpairval);
			for(int z=0; z<32; z += 2)
				testAssert(bf.getBitPair(z) == (z == i ? bitpairval : 3u));
		}
	}


	//=================== Test setBitToZero() ===================
	{
		BitField<uint32> bf(0xFFFFFFFFu);
		testAssert(bf.getBit(0) == 1);
		bf.setBitToZero(0);
		testAssert(bf.getBit(0) == 0);
		bf.setBitToZero(0);
		testAssert(bf.getBit(0) == 0);
		
		testAssert(bf.getBit(1) == 1);
		bf.setBitToZero(1);
		testAssert(bf.getBit(1) == 0);
		bf.setBitToZero(1);
		testAssert(bf.getBit(1) == 0);

		testAssert(bf.getBit(31) == 1);
		bf.setBitToZero(31);
		testAssert(bf.getBit(31) == 0);
		bf.setBitToZero(31);
		testAssert(bf.getBit(31) == 0);
	}

	//=================== Test setBitToOne() ===================
	{
		BitField<uint32> bf(0);
		testAssert(bf.getBit(0) == 0);
		bf.setBitToOne(0);
		testAssert(bf.getBit(0) == 1);
		bf.setBitToOne(0);
		testAssert(bf.getBit(0) == 1);
		
		testAssert(bf.getBit(1) == 0);
		bf.setBitToOne(1);
		testAssert(bf.getBit(1) == 1);
		bf.setBitToOne(1);
		testAssert(bf.getBit(1) == 1);

		testAssert(bf.getBit(31) == 0);
		bf.setBitToOne(31);
		testAssert(bf.getBit(31) == 1);
		bf.setBitToOne(31);
		testAssert(bf.getBit(31) == 1);
	}

	//=================== Perf tests ===================
	const bool do_perf_tests = false;
	if(do_perf_tests)
	{
		{
			Timer timer;

			int N = 10000000;
			BitField<uint32> bf(0);
			for(int i=0; i<N; ++i)
			{
				bf.setBit(i % 32, 1);
			}

			double elapsed = timer.elapsed() / N;
			printVar(bf.v);
			conPrint("BitField elapsed per setBit() " + toString(elapsed * 1.0e9) + " ns");
		}

		{
			Timer timer;

			int N = 10000000;
			BitField<uint32> bf(0);
			for(int i=0; i<N; ++i)
			{
				bf.setBitToOne(i % 32);
			}

			double elapsed = timer.elapsed() / N;
			printVar(bf.v);
			conPrint("BitField elapsed per setBitToOne() " + toString(elapsed * 1.0e9) + " ns");
		}

		{
			Timer timer;

			int N = 10000000;
			BitField<uint32> bf(0);
			for(int i=0; i<N; ++i)
			{
				bf.setBitToZero(i % 32);
			}

			double elapsed = timer.elapsed() / N;
			printVar(bf.v);
			conPrint("BitField elapsed per setBitToZero() " + toString(elapsed * 1.0e9) + " ns");
		}


		{
			Timer timer;

			int N = 10000000;
			std::bitset<32> bf(0);
			for(int i=0; i<N; ++i)
			{
				bf.set(i % 32, 1);
			}

			double elapsed = timer.elapsed() / N;
			conPrint(toString((uint64)bf.to_ulong()));
			conPrint("std::bitset elapsed per setBit() " + toString(elapsed * 1.0e9) + " ns");
		}


		{
			Timer timer;

			int N = 10000000;
			int sum = 0;
			BitField<uint32> bf(0);
			for(int i=0; i<N; ++i)
			{
				sum += bf.getBit(i % 32);
			}

			double elapsed = timer.elapsed() / N;
			printVar(sum);
			conPrint("BitField elapsed per getBit() " + toString(elapsed * 1.0e9) + " ns");
		}

		{
			Timer timer;

			int N = 10000000;
			int sum = 0;
			BitField<uint32> bf(0);
			for(int i=0; i<N; ++i)
			{
				sum += bf.getBitMasked(i % 32);
			}

			double elapsed = timer.elapsed() / N;
			printVar(sum);
			conPrint("BitField elapsed per getBitMasked() " + toString(elapsed * 1.0e9) + " ns");
		}


		{
			Timer timer;

			int N = 10000000;
			int sum = 0;
			std::bitset<32> bf(0);
			for(int i=0; i<N; ++i)
			{
				//sum += bf.test(i % 32) ? 1 : 0;
				sum += bf[i % 32] ? 1 : 0;
			}

			double elapsed = timer.elapsed() / N;
			conPrint(toString((uint64)bf.to_ulong()));
			conPrint("std::bitset elapsed per bf.test() " + toString(elapsed * 1.0e9) + " ns");
		}
	} // end if do_perf_tests
}


#endif // BUILD_TESTS
