/*=====================================================================
Sort.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sat May 15 15:39:54 +1200 2010
=====================================================================*/
#pragma once


#include "platform.h"
#include <assert.h>


/*=====================================================================
Sort
-------------------
Derived from http://www.stereopsis.com/radix.html
=====================================================================*/
namespace Sort
{
	template <class T, class Key>
	inline void radixSort(T* array, uint32 num_elements, Key key);

	void test();


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


	// ---- utils for accessing 11-bit quantities
	#define _0(x)	(x & 0x7FF)
	#define _1(x)	(x >> 11 & 0x7FF)
	#define _2(x)	(x >> 22 )



	finline uint32 floatAsUInt32(float x)
	{
		union fi_union
		{
			float f;
			uint32 i;
		};

		fi_union u;
		u.f = x;
		return u.i;
	}


	finline uint32 flippedKey(float x)
	{
		return FloatFlip(floatAsUInt32(x));
	}


	// ================================================================================================
	// Main radix sort
	// ================================================================================================
	template <class T, class Key>
	void radixSort(T* in, uint32 elements, Key key)
	{
		T* sorted = new T[elements];

		// 3 histograms on the stack:
		const uint32 kHist = 2048;
		uint32 b0[kHist * 3];

		uint32 *b1 = b0 + kHist;
		uint32 *b2 = b1 + kHist;

		for (uint32 i = 0; i < kHist * 3; i++) {
			b0[i] = 0;
		}
		//memset(b0, 0, kHist * 12);

		// 1.  parallel histogramming pass
		//
		for (uint32 i = 0; i < elements; i++) {

			pf(array);

			uint32 fi = flippedKey(key(in[i])); // FloatFlip((uint32&)array[i]);

			b0[_0(fi)] ++;
			b1[_1(fi)] ++;
			b2[_2(fi)] ++;
		}

		// 2.  Sum the histograms -- each histogram entry records the number of values preceding itself.
		{
			uint32 sum0 = 0, sum1 = 0, sum2 = 0;
			uint32 tsum;
			for (uint32 i = 0; i < kHist; i++) {

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

		// byte 0: floatflip entire value, read/write histogram, write out flipped
		for (uint32 i = 0; i < elements; i++) {

			const T fi = in[i];
			uint32 pos = _0(flippedKey(key(fi)));

			sorted[++b0[pos]] = fi;
		}

		// byte 1: read/write histogram, copy
		//   sorted -> array
		for (uint32 i = 0; i < elements; i++) {

			const T si = sorted[i];
			uint32 pos = _1(flippedKey(key(si)));
			in[++b1[pos]] = si;
		}

		// byte 2: read/write histogram, copy & flip out
		//   array -> sorted
		for (uint32 i = 0; i < elements; i++) {

			const T ai = in[i];
			uint32 pos = _2(flippedKey(key(ai)));
			sorted[++b2[pos]] = ai;
		}

		// to write original:
		// memcpy(array, sorted, elements * 4);

		for (uint32 i = 0; i < elements; i++)
			in[i] = sorted[i];

		delete[] sorted;
	}


} // end namespace Sort
