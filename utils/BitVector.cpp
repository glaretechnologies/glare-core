/*=====================================================================
BitVector.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "BitVector.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/CycleTimer.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include <bitset>


void bitVectorTests()
{
	conPrint("bitVectorTests()");

	{
		BitVector bf(1);
		testAssert(bf.getBit(0) == 0);
		bf.setBit(0, 1);
		testAssert(bf.getBit(0) == 1);
		bf.setBit(0, 0);
		testAssert(bf.getBit(0) == 0);
	}

	// Start with zeroed bitfield, set a single bit, check
	for(uint32 i=0; i<100; ++i)
	{
		BitVector bf(100);
		bf.setBit(i, 1);
		for(uint32 z=0; z<100; ++z)
		{
			testAssert(bf.getBit(z) == (z == i ? 1u : 0));
			
			testAssert(bf.getBitMasked(z) == (z == i ? (1u << (i%32)) : 0));
		}
	}

	// Start with one-d bitfield, zero a single bit, check
	for(uint32 i=0; i<100; ++i)
	{
		BitVector bf(100);
		bf.setAllBits(1);

		bf.setBit(i, 0);
		for(uint32 z=0; z<100; ++z)
			testAssert(bf.getBit(z) == (z == i ? 0 : 1u));
	}


	//=================== Test setBitPair(), getBitPair() ===================

	{
		BitVector bf(100);
		for(int i=0; i<100; i += 2)
			testAssert(bf.getBitPair(i) == 0u);
	}

	{
		BitVector bf(100);
		bf.setAllBits(1);
		for(int i=0; i<100; i += 2)
			testAssert(bf.getBitPair(i) == 3u);
	}

	{
		BitVector bf(100);
		bf.setBitPair(2, 3);
		testAssert(bf.getBit(0) == 0);
		testAssert(bf.getBit(1) == 0);
		testAssert(bf.getBit(2) == 1);
		testAssert(bf.getBit(3) == 1);
		testAssert(bf.getBit(4) == 0);
		testAssert(bf.getBit(5) == 0);
	}



	// Start with zeroed bitfield, set a bit pair, check
	for(int i=0; i<100; i += 2)
	{
		for(uint32 bitpairval = 0; bitpairval < 4; ++bitpairval)
		{
			BitVector bf(100);
			bf.setBitPair(i, bitpairval);
			for(int z=0; z<100; z += 2)
				testAssert(bf.getBitPair(z) == (z == i ? bitpairval : 0));
		}
	}

	// Start with one-d bitfield, set a bit pair, check
	for(int i=0; i<100; i += 2)
	{
		for(uint32 bitpairval = 0; bitpairval < 4; ++bitpairval)
		{
			BitVector bf(100);
			bf.setAllBits(1);
			bf.setBitPair(i, bitpairval);
			for(int z=0; z<100; z += 2)
				testAssert(bf.getBitPair(z) == (z == i ? bitpairval : 3u));
		}
	}


	//=================== Test setBitToZero() ===================
	{
		BitVector bf(32);
		bf.setAllBits(1);
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
		BitVector bf(32);
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
}


#endif // BUILD_TESTS
