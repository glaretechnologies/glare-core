/*=====================================================================
Sort.h
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Sat May 15 15:39:54 +1200 2010
=====================================================================*/
#pragma once


#include "Platform.h"
#include <assert.h>
#include <algorithm>
#include "Timer.h"
#include "Task.h"
#include "TaskManager.h"
#include "BitUtils.h"


/*=====================================================================
Sort
-------------------
Radix sort derived from http://www.stereopsis.com/radix.html
=====================================================================*/
namespace Sort
{

	template <class T, class LessThanPredicate, class Key>
	inline void floatKeyAscendingSort(T* data, size_t num_elements, LessThanPredicate pred, Key key, T* working_space = NULL, bool put_result_in_working_space = false);


	template <class T, class Key>
	inline void radixSort(T* data, uint32 num_elements, Key key, T* working_space = NULL, bool put_result_in_working_space = false);

	template <class T, class Key>
	inline void radixSort16(T* data, uint32 num_elements, Key key);


	void test();


	template <class T, class LessThanPredicate, class Key>
	inline void floatKeyAscendingSort(T* data, size_t num_elements, LessThanPredicate pred, Key key, T* working_space, bool put_result_in_working_space)
	{
		// Use std sort for arrays of less than 320 items, because std sort is faster for smaller arrays.
		if(num_elements < 320)
		{
			std::sort(data, data + num_elements, pred);

			if(put_result_in_working_space)
				for(size_t i=0; i<num_elements; ++i)
					working_space[i] = data[i];
		}
		else
		{
			radixSort(data, (uint32)num_elements, key, working_space, put_result_in_working_space);
		}
	}

	// ================================================================================================
	// flip a float for sorting
	//  finds SIGN of fp number.
	//  if it's 1 (negative float), it flips all bits
	//  if it's 0 (positive float), it flips the sign only
	// ================================================================================================
	INDIGO_STRONG_INLINE uint32 FloatFlip(uint32 f)
	{
		uint32 mask = -int32(f >> 31) | 0x80000000;
		return f ^ mask;
	}


	// ---- utils for accessing 11-bit quantities
	#define _0(x)	(x & 0x7FF)
	#define _1(x)	(x >> 11 & 0x7FF)
	#define _2(x)	(x >> 22 )

	#define _16_0(x)	(x & 0xFFFF)
	#define _16_1(x)	(x >> 16)


	INDIGO_STRONG_INLINE uint32 flippedKey(float x)
	{
		return FloatFlip(bitCast<uint32>(x));
	}


	// ================================================================================================
	// Main radix sort
	// ================================================================================================
	template <class T, class Key>
	void radixSort(T* in, uint32 elements, Key key, T* working_space, bool put_result_in_working_space)
	{
		T* sorted;
		if(working_space)
			sorted = working_space;
		else
			sorted = new T[elements];

		// 3 histograms on the stack:
		const uint32 kHist = 2048;
		uint32 b0[kHist * 3];

		uint32 *b1 = b0 + kHist;
		uint32 *b2 = b1 + kHist;

		for(uint32 i = 0; i < kHist * 3; i++)
			b0[i] = 0;
		//memset(b0, 0, kHist * 12);
		

		//Timer timer;

		// 1.  parallel histogramming pass
		// Trivially parallelisable
		for(uint32 i = 0; i < elements; i++) {
			
			const uint32 fi = flippedKey(key(in[i]));

			b0[_0(fi)]++;
			b1[_1(fi)]++;
			b2[_2(fi)]++;
		}

		//conPrint("\ncounting pass:      " + timer.elapsedString());
		//timer.reset();

		// 2.  Sum the histograms -- each histogram entry records the number of values preceding itself.
		{
			uint32 sum0 = 0, sum1 = 0, sum2 = 0;
			uint32 tsum;
			for(uint32 i = 0; i < kHist; i++) {

				tsum = b0[i] + sum0;
				b0[i] = sum0;
				sum0 = tsum;

				tsum = b1[i] + sum1;
				b1[i] = sum1;
				sum1 = tsum;

				tsum = b2[i] + sum2;
				b2[i] = sum2;
				sum2 = tsum;
			}
		}

		//conPrint("summing histograms: " + timer.elapsedString());
		//timer.reset();

		// byte 0: floatflip entire value, read/write histogram, write out flipped
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _0(flippedKey(key(in[i])));
			sorted[b0[pos]++] = in[i];
		}

		// byte 1: read/write histogram, copy
		//   sorted -> array
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _1(flippedKey(key(sorted[i])));
			in[b1[pos]++] = sorted[i];
		}

		// byte 2: read/write histogram, copy & flip out
		//   array -> sorted
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _2(flippedKey(key(in[i])));
			sorted[b2[pos]++] = in[i];
		}

		// Copy back to original input array if desired.
		if(!put_result_in_working_space)
			for(uint32 i = 0; i < elements; i++)
				in[i] = sorted[i];

		//conPrint("reading/writing:    " + timer.elapsedString());

		if(!working_space)
			delete[] sorted;
	}


	// ================================================================================================
	// Radix sort with 16-bit chunks.  This seem slower than the the 11-bit chunk radix sort.
	// ================================================================================================
	template <class T, class Key>
	void radixSort16(T* in, uint32 elements, Key key)
	{
		T* sorted = new T[elements];

		const uint32 kHist = 65536; // 2^16
		uint32 *b0 = new uint32[kHist * 2];
		uint32 *b1 = b0 + kHist;
		for(uint32 i = 0; i < kHist * 2; i++) {
			b0[i] = 0;
		}

		// 1.  parallel histogramming pass
		// Trivially parallelisable
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 fi = flippedKey(key(in[i]));

			b0[_16_0(fi)]++;
			b1[_16_1(fi)]++;
		}

		// 2.  Sum the histograms -- each histogram entry records the number of values preceding itself.
		{
			uint32 sum0 = 0, sum1 = 0;
			uint32 tsum;
			for(uint32 i = 0; i < kHist; i++) {

				tsum = b0[i] + sum0;
				b0[i] = sum0;
				sum0 = tsum;

				tsum = b1[i] + sum1;
				b1[i] = sum1;
				sum1 = tsum;
			}
		}

		// byte 0: floatflip entire value, read/write histogram, write out flipped
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _16_0(flippedKey(key(in[i])));
			sorted[b0[pos]++] = in[i];
		}

		// byte 1: read/write histogram, copy
		//   sorted -> array
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _16_1(flippedKey(key(sorted[i])));
			in[b1[pos]++] = sorted[i];
		}


		delete[] b0;
		delete[] sorted;
	}


	// ================================================================================================
	// Assumes all keys in this bucket share the same most significant 11 bits
	// Writes the output to working_space
	// ================================================================================================
	template <class T, class Key>
	void radixSortBucket(T* in, uint32 elements, Key key, T* working_space)
	{
		// 2 histograms on the stack:
		const uint32 kHist = 2048;
		uint32 b0[kHist * 2];

		uint32 *b1 = b0 + kHist;

		for(uint32 i = 0; i < kHist * 2; i++)
			b0[i] = 0;

		//Timer timer;

		// 1.  parallel histogramming pass
		// Trivially parallelisable
		for(uint32 i = 0; i < elements; i++)
		{
			//const uint32 fi = flippedKey(key(in[i]));
			T val = in[i];
			float keyval = key(val);
			uint32 fi = flippedKey(keyval);
			uint32 bits_0 = _0(fi);
			uint32 bits_1 = _1(fi);
			b0[bits_0]++;
			b1[bits_1]++;
		}

		//conPrint("\ncounting pass:      " + timer.elapsedString());
		//timer.reset();

		// 2.  Sum the histograms -- each histogram entry records the number of values preceding itself.
		{
			uint32 sum0 = 0, sum1 = 0;
			uint32 tsum;
			for(uint32 i = 0; i < kHist; i++) {

				tsum = b0[i] + sum0;
				b0[i] = sum0;
				sum0 = tsum;

				tsum = b1[i] + sum1;
				b1[i] = sum1;
				sum1 = tsum;
			}
		}

		//conPrint("summing histograms: " + timer.elapsedString());
		//timer.reset();

		// byte 0: floatflip entire value, read/write histogram, write out flipped
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _0(flippedKey(key(in[i])));
			working_space[b0[pos]++] = in[i];
		}

		// byte 1: read/write histogram, copy
		//   sorted -> array
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _1(flippedKey(key(working_space[i])));
			in[b1[pos]++] = working_space[i];
		}

		// Copy to sorted.
		//for(uint32 i = 0; i < elements; i++)
		//	sorted[i] = in[i];
		std::memcpy(working_space, in, elements * sizeof(T));

		//conPrint("reading/writing:    " + timer.elapsedString());
	}


	template <class T, class LessThanPredicate, class Key>
	class SortBucketTask : public Indigo::Task
	{
	public:
		SortBucketTask(uint32* b0_, T* a_, T* b_, LessThanPredicate pred_, Key key_) : b0(b0_), a(a_), b(b_), pred(pred_), key(key_) {}

		// Just-bucketed data is in 'b'.
		// Sort the data in each bucket and write it to 'a'.
		virtual void run(size_t thread_index)
		{
			for(int z=0; z<2048; ++z) // For each bucket:
			{
				uint32 bucket_begin = b0[z];
				uint32 bucket_end = z == 2047 ? 2048 : b0[z + 1];
				if(bucket_begin >= task_begin && bucket_begin < task_end)
				{
					T* bucket_data_begin = b + bucket_begin;
					T* bucket_working_space = a + bucket_begin;
					const uint32 bucket_num_elems = bucket_end - bucket_begin;

					if(bucket_num_elems < 320)
					{
						std::sort(bucket_data_begin, bucket_data_begin + bucket_num_elems, pred);

						// Copy back to 'a'.
						std::memcpy(bucket_working_space, bucket_data_begin, bucket_num_elems * sizeof(T));
					}
					else
					{
						radixSortBucket(bucket_data_begin, (uint32)bucket_num_elems, key, bucket_working_space); // Writes the output to 'a'.
					}
				}
			}
		}
		uint32* b0;
		T* a;
		T* b;
		uint32 task_begin;
		uint32 task_end;
		LessThanPredicate pred;
		Key key;
	};


	template <class T, class LessThanPredicate, class Key>
	void bucketSort(Indigo::TaskManager& task_manager, T* in, T* working_space, uint32 elements, LessThanPredicate pred, Key key) 
	{
		// bucket histograms on the stack:
		const uint32 kHist = 2048;
		uint32 b0[kHist];

		for(uint32 i = 0; i < kHist; i++)
			b0[i] = 0;

		//Timer timer;

		// 1.  parallel histogramming pass
		// Trivially parallelisable
		for(uint32 i = 0; i < elements; i++) {

			const uint32 fi = flippedKey(key(in[i]));

			b0[_2(fi)]++;
		}

		//conPrint("\ncounting pass:      " + timer.elapsedString());
		//timer.reset();

		/*for(uint32 i = 0; i < kHist; i++) {
			conPrint("bucket " + toString(i) + " count: " + toString(b0[i]));
		}*/

		// 2.  Sum the histograms -- each histogram entry records the number of values preceding itself.
		{
			uint32 sum0 = 0;
			uint32 tsum;
			for(uint32 i = 0; i < kHist; i++) {

				tsum = b0[i] + sum0;
				b0[i] = sum0;
				sum0 = tsum;
			}
		}

		//conPrint("summing histograms: " + timer.elapsedString());
		//timer.reset();

		// byte 0: floatflip entire value, read/write histogram, write out flipped
		for(uint32 i = 0; i < elements; i++)
		{
			const uint32 pos = _2(flippedKey(key(in[i])));
			working_space[b0[pos]++] = in[i];
		}



		/*
		Buckets occupying range [begin, end) will be assigned to the task
		such that begin is >= task_begin and < task_end.

		|----------b0--------|----b1---|-------b2------| ...  |---b_{m-1}--|

		|--------t0-------|--------t1---------| ..   |----------t_{n-1}----|
		*/

		const int num_tasks = (int)task_manager.getNumThreads();
		for(int i=0; i<num_tasks; ++i)
		{
			Reference<SortBucketTask<T, LessThanPredicate, Key>> task = new SortBucketTask<T, LessThanPredicate, Key>(b0, in, working_space, pred, key);
			task->task_begin = (elements / num_tasks) * (i);
			task->task_end   = (elements / num_tasks) * (i + 1);
			if(i == num_tasks - 1)
				task->task_end = elements;
			task_manager.addTask(task);
		}

		task_manager.waitForTasksToComplete();
	}

} // end namespace Sort
