/*=====================================================================
Sort.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sat May 15 15:39:54 +1200 2010
=====================================================================*/
#include "Sort.h"


#include "platform.h"
#include "MTwister.h"
#include "timer.h"
#include "stringutils.h"
#include <vector>
#include <algorithm>
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"


namespace Sort
{


// From http://stereopsis.com/radix.html:

// Radix.cpp: a fast floating-point radix sort demo
//
//   Copyright (C) Herf Consulting LLC 2001.  All Rights Reserved.
//   Use for anything you want, just tell me what you do with it.
//   Code provided "as-is" with no liabilities for anything that goes wrong.
//

//#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
//#include <windows.h>	// QueryPerformanceCounter
/*

// ------------------------------------------------------------------------------------------------
// ---- Basic types

//typedef long int32;
//typedef unsigned long uint32;
typedef float real32;
typedef double real64;
typedef unsigned char uint8;
typedef const char *cpointer;


// ---- use SSE prefetch (needs compiler support), not really a problem on non-SSE machines.
//		need http://msdn.microsoft.com/vstudio/downloads/ppack/default.asp
//		or recent VC to use this


// Changed by nick: disabled this as it was causing compile errors.
#define PREFETCH 0

#if PREFETCH
#include <xmmintrin.h>	// for prefetch
#define pfval	64
#define pfval2	128
#define pf(x)	_mm_prefetch(cpointer(x + i + pfval), 0)
#define pf2(x)	_mm_prefetch(cpointer(x + i + pfval2), 0)
#else
#define pf(x)
#define pf2(x)
#endif

// ------------------------------------------------------------------------------------------------
// ---- Visual C++ eccentricities

#if _WINDOWS
#define finline __forceinline
#else
#define finline inline
#endif

// ================================================================================================
// flip a float for sorting
//  finds SIGN of fp number.
//  if it's 1 (negative float), it flips all bits
//  if it's 0 (positive float), it flips the sign only
// ================================================================================================
finline uint32 FloatFlip(uint32 f)
{
	uint32 mask = -int32(f >> 31) | 0x80000000;
	return f ^ mask;
}

finline void FloatFlipX(uint32 &f)
{
	uint32 mask = -int32(f >> 31) | 0x80000000;
	f ^= mask;
}

// ================================================================================================
// flip a float back (invert FloatFlip)
//  signed was flipped from above, so:
//  if sign is 1 (negative), it flips the sign bit back
//  if sign is 0 (positive), it flips all bits back
// ================================================================================================
finline uint32 IFloatFlip(uint32 f)
{
	uint32 mask = ((f >> 31) - 1) | 0x80000000;
	return f ^ mask;
}

// ---- utils for accessing 11-bit quantities
#define _0(x)	(x & 0x7FF)
#define _1(x)	(x >> 11 & 0x7FF)
#define _2(x)	(x >> 22 )
*/

// ================================================================================================
// Main radix sort
// ================================================================================================
void radixSortF(float *farray, float *sorted, uint32 elements)
{
	uint32 i;
	uint32 *sort = (uint32*)sorted;
	uint32 *array = (uint32*)farray;

	// 3 histograms on the stack:
	const uint32 kHist = 2048;
	uint32 b0[kHist * 3];

	uint32 *b1 = b0 + kHist;
	uint32 *b2 = b1 + kHist;

	for (i = 0; i < kHist * 3; i++) {
		b0[i] = 0;
	}
	//memset(b0, 0, kHist * 12);

	// 1.  parallel histogramming pass
	//
	for (i = 0; i < elements; i++) {

		pf(array);

		uint32 fi = FloatFlip((uint32&)array[i]);

		b0[_0(fi)] ++;
		b1[_1(fi)] ++;
		b2[_2(fi)] ++;
	}

	// 2.  Sum the histograms -- each histogram entry records the number of values preceding itself.
	{
		uint32 sum0 = 0, sum1 = 0, sum2 = 0;
		uint32 tsum;
		for (i = 0; i < kHist; i++) {

			tsum = b0[i] + sum0;
			b0[i] = sum0 - 1;
			sum0 = tsum;

			tsum = b1[i] + sum1;
			b1[i] = sum1 - 1;
			sum1 = tsum;

			tsum = b2[i] + sum2;
			b2[i] = sum2 - 1;
			sum2 = tsum;
		}
	}

	// byte 0: FloatFlip entire value, read/write histogram, write out flipped
	for (i = 0; i < elements; i++) {

		uint32 fi = array[i];
		FloatFlipX(fi);
		uint32 pos = _0(fi);

		pf2(array);
		sort[++b0[pos]] = fi;
	}

	// byte 1: read/write histogram, copy
	//   sorted -> array
	for (i = 0; i < elements; i++) {
		uint32 si = sort[i];
		uint32 pos = _1(si);
		pf2(sort);
		array[++b1[pos]] = si;
	}

	// byte 2: read/write histogram, copy & flip out
	//   array -> sorted
	for (i = 0; i < elements; i++) {
		uint32 ai = array[i];
		uint32 pos = _2(ai);

		pf2(array);
		sort[++b2[pos]] = IFloatFlip(ai);
	}

	// to write original:
	// memcpy(array, sorted, elements * 4);
}


class Item
{
public:
	int i;
	float f;
};


class ItemPredicate
{
public:
	inline bool operator()(const Item& a, const Item& b)
	{
		return a.f < b.f;
	}
};


class FloatReader
{
public:
	inline float operator() (const Item& item)
	{
		return item.f;
	}
};



void test()
{
	MTwister rng(1);

	for(uint32 N = 2; N<=16777216; N*=2)
	{
		std::vector<Item> f(N);
		for(size_t i=0; i<f.size(); ++i)
		{
			f[i].i = i;
			f[i].f = -100.0f + rng.unitRandom() * 200.0f;
		}

		std::vector<Item> sorted(N);

		std::vector<bool> seen(N, false);

		std::vector<Item> temp_f = f;

		Timer timer;

		FloatReader float_reader;
		radixSort<Item, FloatReader>(&temp_f[0], &sorted[0], float_reader, N);

		const double radix_time = timer.elapsed();

		// Check the results actually are sorted.
		for(size_t i=1; i<sorted.size(); ++i)
		{
			testAssert(sorted[i].f >= sorted[i-1].f);
		}

		// Check all original items are present.
		for(size_t i=0; i<sorted.size(); ++i)
		{
			testAssert(!seen[sorted[i].i]);
			seen[sorted[i].i] = true;
		}

		// std::sort
		timer.reset();
		ItemPredicate pred;
		std::sort(f.begin(), f.end(), pred);
		const double std_sort_time = timer.elapsed();

		conPrint("N: "  + toString(N));
		conPrint("radix_time:    " + toString(radix_time) + " s");
		conPrint("std_sort_time: " + toString(std_sort_time) + " s");
	}

	exit(0);
}


} // end namespace Sort
