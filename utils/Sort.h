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

=====================================================================*/
namespace Sort
{
	// The original code
	void radixSortF(float* farray, float* sorted_out, uint32 num_elements);

	template <class T, class TReader>
	inline void radixSort(T* farray, T* sorted_out, TReader readFloat, uint32 num_elements);

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


	// ================================================================================================
	// Main radix sort
	// ================================================================================================
	template <class T, class TReader>
	void radixSort(T* in, T* sorted, TReader readFloat, uint32 elements)
	{
		// Create some temporary arrays so that the radix code can twiddle bits to its heart's content.
		uint32* array = new uint32[elements];
		uint32* sort = new uint32[elements];

		// Initialise sort as the float values from farray
		for(uint32 z=0; z<elements; ++z)
			array[z] = floatAsUInt32(readFloat(in[z]));

		uint32 i;
		//T *sort = sorted;
		//T *array = farray;

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

		// byte 0: floatflip entire value, read/write histogram, write out flipped
		for (i = 0; i < elements; i++) {

			uint32 fi = array[i];
			FloatFlipX(fi);
			uint32 pos = _0(fi);

			assert(b0[pos]+1 < elements);

			pf2(array);
			sort[++b0[pos]] = fi;

			sorted[b0[pos]] = in[i]; // NEW: move T object
		}

		// byte 1: read/write histogram, copy
		//   sorted -> array
		for (i = 0; i < elements; i++) {
			uint32 si = sort[i];
			uint32 pos = _1(si);
			pf2(sort);
			array[++b1[pos]] = si;

			in[b1[pos]] = sorted[i]; // NEW: move T object
		}

		// byte 2: read/write histogram, copy & flip out
		//   array -> sorted
		for (i = 0; i < elements; i++) {
			uint32 ai = array[i];
			uint32 pos = _2(ai);

			pf2(array);
			sort[++b2[pos]] = IFloatFlip(ai);

			sorted[b2[pos]] = in[i]; // NEW: move T object
		}

		delete[] array;
		delete[] sort;

		// to write original:
		// memcpy(array, sorted, elements * 4);
	}


} // end namespace Sort
