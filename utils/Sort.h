/*=====================================================================
Sort.h
------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "Timer.h"
#include "Task.h"
#include "TaskManager.h"
#include "BitUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Vector.h"
#include <assert.h>
#include <algorithm>


/*=====================================================================
Sort
----

=====================================================================*/
namespace Sort
{
	// Serial radix sort derived from http://www.stereopsis.com/radix.html
	template <class T, class LessThanPredicate, class Key>
	inline void floatKeyAscendingSort(T* data, size_t num_elements, LessThanPredicate pred, Key key, T* working_space = NULL, bool put_result_in_working_space = false);


	template <class T, class Key>
	inline void radixSort(T* data, uint32 num_elements, Key key, T* working_space = NULL, bool put_result_in_working_space = false);

	template <class T, class Key>
	inline void radixSort16(T* data, uint32 num_elements, Key key);


	/*
	temp_counts should be a pointer to an array of 6144 uint32s  (6144 = 2**11 * 3).  temp_counts_size should be >= 6144.
	GetKey operator () should return a uint32 (or wider) integer.
	*/
	template<class T, class GetKey>
	inline void radixSort32BitKey(T* __restrict data, T* __restrict working_space, size_t num_items, GetKey getKey, uint32* temp_counts, size_t temp_counts_size);

	/*
	Counting sorts.  Efficient for when the total number of different buckets/keys is relatively small.
	*/
	template <class T, class BucketChooser>
	inline void serialCountingSort(const T* in, T* out, size_t num, BucketChooser bucket_chooser);

	// For when num buckets is already known.
	template <class T, class BucketChooser>
	inline void serialCountingSortWithNumBuckets(const T* in, T* out, size_t num, size_t num_buckets, BucketChooser bucket_chooser);

	template <class T, class BucketChooser>
	inline void parallelCountingSort(glare::TaskManager& task_manager, const T* in, T* out, size_t num, BucketChooser bucket_chooser);


	template <class T, class BucketChooser>
	inline void parallelStableNWayPartition(glare::TaskManager& task_manager, const T* in, T* out, size_t num, size_t num_buckets, BucketChooser bucket_chooser);


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
	GLARE_STRONG_INLINE uint32 FloatFlip(uint32 f)
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


	GLARE_STRONG_INLINE uint32 flippedKey(float x)
	{
		return FloatFlip(bitCast<uint32>(x));
	}


	// ================================================================================================
	// serial radix sort
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

	template <class T, class Key>
	class Bits0
	{
	public:
		size_t operator() (T x) { return _0(flippedKey(key(x))); }
		Key key;
	};

	template <class T, class Key>
	class Bits1
	{
	public:
		size_t operator() (T x) { return _1(flippedKey(key(x))); }
		Key key;
	};

	template <class T, class Key>
	class Bits2
	{
	public:
		size_t operator() (T x) { return _2(flippedKey(key(x))); }
		Key key;
	};


	// ================================================================================================
	// Parallel radix sort
	// ================================================================================================
	template <class T, class Key>
	void radixSortWithParallelPartition(glare::TaskManager& task_manager, T* in, uint32 elements, Key key, T* working_space, bool put_result_in_working_space)
	{
		T* sorted;
		if(working_space)
			sorted = working_space;
		else
			sorted = new T[elements];

		const int num_buckets = 2048;
		Bits0<T, Key> bits0; bits0.key = key;
		parallelStableNWayPartition<T, Bits0<T, Key> >(task_manager, in, sorted, elements, num_buckets, bits0);

		Bits1<T, Key> bits1; bits1.key = key;
		parallelStableNWayPartition<T, Bits1<T, Key> >(task_manager, sorted, in, elements, num_buckets, bits1);

		Bits2<T, Key> bits2; bits2.key = key;
		parallelStableNWayPartition<T, Bits2<T, Key> >(task_manager, in, sorted, elements, num_buckets, bits2);

		// Copy back to original input array if desired.
		if(!put_result_in_working_space)
			std::memcpy(in, sorted, elements * sizeof(T));
			//for(uint32 i = 0; i < elements; i++)
			//	in[i] = sorted[i];

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
	class SortBucketTask : public glare::Task
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
	void bucketSort(glare::TaskManager& task_manager, T* in, T* working_space, uint32 elements, LessThanPredicate pred, Key key) 
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


	//======================================================================================

	// Serial stable (binary) partition
	// NOTE: this is actually pretty slow, std::stable_partition is faster.
	template <class T, class Pred>
	size_t stablePartition(T* in, T* out, size_t num, Pred pred)
	{
		// Count number of items going to left
		size_t left_count = 0;
		for(size_t i=0; i<num; ++i)
			if(pred(in[i]))
				left_count++;

		// Do another pass, placing items in correct position
		size_t left_place_i = 0;
		size_t right_place_i = left_count;
		for(size_t i=0; i<num; ++i)
		{
			if(pred(in[i]))
				out[left_place_i++] = in[i];
			else
				out[right_place_i++] = in[i];
		}
		return left_count;
	}


	template <class T, class Pred>
	class PartitionCountTask : public glare::Task
	{
	public:
		virtual void run(size_t thread_index)
		{
			const T* const in = in_;
			const size_t num = num_;

			size_t left_count = 0;
			for(size_t i=0; i<num; ++i)
				if(pred(in[i]))
					left_count++;

			left_count_ = left_count;
		}

		T* in_;
		Pred pred;
		size_t num_, left_count_;

		char padding[128];
	};

	template <class T, class Pred>
	class PartitionPlaceTask : public glare::Task
	{
	public:
		virtual void run(size_t thread_index)
		{
			const T* const in = in_;
			T* const out = out_;
			const size_t num = num_;

			size_t left_place_i = left_write_i;
			size_t right_place_i = right_write_i;
			for(size_t i=0; i<num; ++i)
			{
				if(pred(in[i]))
					out[left_place_i++] = in[i];
				else
					out[right_place_i++] = in[i];
			}
		}

		T* in_;
		T* out_;
		Pred pred;
		size_t num_, left_write_i, right_write_i;

		char padding[128];
	};


	// Parallel stable (binary) partition
	// Returns num items put to left of parition.
	template <class T, class Pred>
	size_t parallelStablePartition(glare::TaskManager& task_manager, T* in, T* out, size_t num, Pred pred)
	{
		const size_t max_num_tasks = 32;
		const size_t num_tasks = 32;//myMin(max_num_tasks, task_manager.getNumThreads());
		const size_t num_per_task = Maths::roundedUpDivide(num, num_tasks);

		// Do a pass over the data to get the number of items going to the left and to the right. (for each task range)
		Reference<PartitionCountTask<T, Pred> > tasks[max_num_tasks];
		for(size_t i=0; i<num_tasks; ++i)
		{
			tasks[i] = new PartitionCountTask<T, Pred>();
			const size_t begin_i = num_per_task * i;
			tasks[i]->pred = pred;
			tasks[i]->in_ = in + begin_i;
			tasks[i]->num_ = begin_i >= num ? 0 : myMin<size_t>(num_per_task, num - begin_i);
		}
		task_manager.runTasks(ArrayRef<glare::TaskRef>((Reference<glare::Task>*)tasks, num_tasks));

		// Compute prefix sum for the tasks.
		size_t num_left_before[max_num_tasks];
		size_t num_right_before[max_num_tasks];
		size_t sum_left_before  = 0;
		size_t sum_right_before = 0;
		for(size_t i=0; i<num_tasks; ++i)
		{
			num_left_before[i]  = sum_left_before;
			num_right_before[i] = sum_right_before;
			sum_left_before  += tasks[i]->left_count_;
			sum_right_before += tasks[i]->num_ - tasks[i]->left_count_;
		}

		// Do a pass over the data to put it in the correct location.
		Reference<PartitionPlaceTask<T, Pred> > place_tasks[max_num_tasks];
		for(size_t i=0; i<num_tasks; ++i)
		{
			place_tasks[i] = new PartitionPlaceTask<T, Pred>();
			const size_t begin_i = num_per_task * i;
			place_tasks[i]->in_  = in + begin_i;
			place_tasks[i]->out_ = out;
			place_tasks[i]->pred = pred;
			place_tasks[i]->num_ = begin_i >= num ? 0 : myMin<size_t>(num_per_task, num - begin_i);
			place_tasks[i]->left_write_i  = num_left_before[i];
			place_tasks[i]->right_write_i = sum_left_before + num_right_before[i];
		}
		task_manager.runTasks(ArrayRef<glare::TaskRef>((Reference<glare::Task>*)place_tasks, num_tasks));

		return sum_left_before;
	}


	//==========================================================================

	// Serial
	template <class T, class BucketChooser>
	void stableNWayPartition(const T* in, T* out, size_t num, size_t num_buckets, BucketChooser bucket_chooser)
	{
		// Count num items in each bucket
		std::vector<size_t> counts(num_buckets, 0);
		for(size_t i=0; i<num; ++i)
			counts[bucket_chooser(in[i])]++;

		// Compute sum in all buckets before the given bucket. (exclusive prefix sum)
		size_t sum = 0;
		for(size_t i=0; i<num_buckets; ++i)
		{
			const size_t count_i = counts[i];
			counts[i] = sum;
			sum += count_i;
		}

		// Put items in final position
		for(size_t i=0; i<num; ++i)
		{
			const T val = in[i];
			out[counts[bucket_chooser(val)]++] = val;
		}
	}


	template <class T, class BucketChooser>
	class NWayPartitionCountTask : public glare::Task
	{
	public:
		virtual void run(size_t thread_index)
		{
			const size_t num_ = num;
			const T* const in_ = in;
			size_t* const counts_ = counts;

			for(size_t i=0; i<num_; ++i)
				counts_[bucket_chooser(in_[i])]++;

			/*const int initial_num = num / 4 * 4;
			assert(initial_num % 4 == 0 && initial_num < num);
			size_t c_0[8];
			size_t c_1[8];
			size_t c_2[8];
			size_t c_3[8];
			for(size_t i=0; i<8; ++i)
				c_0[i] = c_1[i] = c_2[i] = c_3[i] = 0;

			for(size_t i=0; i<initial_num; i += 4)
			{
				assert(bucket_chooser(in_[i]) < 8);
				c_0[bucket_chooser(in_[i+0])]++;
				c_1[bucket_chooser(in_[i+1])]++;
				c_2[bucket_chooser(in_[i+2])]++;
				c_3[bucket_chooser(in_[i+3])]++;
			}

			for(size_t i=0; i<8; ++i)
				counts[i] = c_0[i] + c_1[i] + c_2[i] + c_3[i];

			for(size_t i=initial_num; i<num_; ++i)
				counts_[bucket_chooser(in_[i])]++;*/
		}

		const T* in;
		BucketChooser bucket_chooser;
		size_t num;
		size_t* counts;
	};

	template <class T, class BucketChooser>
	class NWayPartitionPlaceTask : public glare::Task
	{
	public:
		virtual void run(size_t thread_index)
		{
			const size_t num_ = num;
			const T* const in_ = in;
			T* const out_ = out;
			size_t* const bucket_write_i_ = bucket_write_i;

			for(size_t i=0; i<num_; ++i)
			{
				const T val = in_[i];
				const size_t bucket_i = bucket_chooser(val);
				out_[bucket_write_i_[bucket_i]++] = val;
			}
		}

		const T* in;
		T* out;
		BucketChooser bucket_chooser;
		size_t num;
		size_t* bucket_write_i;
	};


	template <class T, class BucketChooser>
	inline void serialCountingSort(const T* in, T* out, size_t num, BucketChooser bucket_chooser)
	{
		// Do a pass to get number of buckets first.
		size_t max_bucket = 0;
		for(size_t i=0; i<num; ++i)
			max_bucket = myMax<size_t>(max_bucket, bucket_chooser(in[i]));

		const size_t num_buckets = max_bucket + 1;
		stableNWayPartition(in, out, num, num_buckets, bucket_chooser);
	}


	template <class T, class BucketChooser>
	inline void serialCountingSortWithNumBuckets(const T* in, T* out, size_t num, size_t num_buckets, BucketChooser bucket_chooser)
	{
#ifndef NDEBUG
		// Check passed in num_buckets is large enough
		size_t max_bucket = 0;
		for(size_t i=0; i<num; ++i)
			max_bucket = myMax<size_t>(max_bucket, bucket_chooser(in[i]));
		assert(max_bucket + 1 <= num_buckets);
#endif
		stableNWayPartition(in, out, num, num_buckets, bucket_chooser);
	}


	template <class T, class BucketChooser>
	void parallelCountingSort(glare::TaskManager& task_manager, const T* in, T* out, size_t num, BucketChooser bucket_chooser)
	{
		// Do a pass to get number of buckets first.
		// TODO: do in parallel
		size_t max_bucket = 0;
		for(size_t i=0; i<num; ++i)
			max_bucket = myMax(max_bucket, bucket_chooser(in[i]));

		// Now sort the data
		const size_t num_buckets = max_bucket + 1;
		parallelStableNWayPartition(task_manager, in, out, num, num_buckets, bucket_chooser);
	}


	template <class T, class BucketChooser>
	void parallelStableNWayPartition(glare::TaskManager& task_manager, const T* in, T* out, size_t num, size_t num_buckets, BucketChooser bucket_chooser)
	{
		const size_t max_num_tasks = 32;
		const size_t num_tasks = 32;//myMin(max_num_tasks, task_manager.getNumThreads());
		const size_t num_per_task = Maths::roundedUpDivide(num, num_tasks);

		const size_t count_padding = 128; // See http://www.forwardscattering.org/post/29 for why we need padding here.
		const size_t stride = num_buckets + count_padding;
		js::Vector<size_t, 128> counts(num_tasks * stride);
		std::memset(&counts[0], 0, num_tasks * stride * sizeof(size_t)); // zero

		//Timer timer;
		// Do a pass over the data to get the number of items going to the left and to the right. (for each task range)
		Reference<NWayPartitionCountTask<T, BucketChooser> > tasks[max_num_tasks];
		for(size_t i=0; i<num_tasks; ++i)
		{
			tasks[i] = new NWayPartitionCountTask<T, BucketChooser>();
			const size_t begin_i = num_per_task * i;
			tasks[i]->bucket_chooser = bucket_chooser;
			tasks[i]->in = in + begin_i;
			tasks[i]->num = begin_i >= num ? 0 : myMin<size_t>(num_per_task, num - begin_i);
			tasks[i]->counts = &counts[i * stride];
		}

		task_manager.runTasks(ArrayRef<glare::TaskRef>((Reference<glare::Task>*)tasks, num_tasks));

		//conPrint("count pass: " + timer.elapsedStringNSigFigs(5));
		//timer.reset();
		
		/*
		Each task (t0, t1, ..) will write to a specific place in each bucket:

		t0 b0 write                   t0 b1 write
		|     t1 b0 write              |    t1 b1 write
		|        |     t2 b0 write     |       |    t2 b1 write
		|        |        | t3 b0 write|       |        | t3 b1 write
		|        |        |      |     |       |        |   |
		t0 t0 t0 t1 t1 t1 t2 t2 t3 t3 t0 t0 t0 t1 t1 t1 t2 t3
		|----------------------------|-----------------------|--- ...
		        bucket 0                     bucket 1
		*/
		// Compute prefix sum for the tasks.
		size_t sum = 0;
		for(size_t b=0; b<num_buckets; ++b)
			for(size_t t=0; t<num_tasks; ++t)
			{
				const size_t bucket_count = tasks[t]->counts[b];
				tasks[t]->counts[b] = sum;
				sum += bucket_count;
			}

		//conPrint("Compute prefix sum: " + timer.elapsedStringNSigFigs(5));
		//timer.reset();

		// Do a pass over the data to put it in the correct location.
		Reference<NWayPartitionPlaceTask<T, BucketChooser> > place_tasks[max_num_tasks];
		for(size_t i=0; i<num_tasks; ++i)
		{
			place_tasks[i] = new NWayPartitionPlaceTask<T, BucketChooser>();
			const size_t begin_i = num_per_task * i;
			place_tasks[i]->bucket_write_i = &counts[i * stride];
			place_tasks[i]->in  = in + begin_i;
			place_tasks[i]->out = out;
			place_tasks[i]->bucket_chooser = bucket_chooser;
			place_tasks[i]->num = begin_i >= num ? 0 : myMin<size_t>(num_per_task, num - begin_i);
		}

		task_manager.runTasks(ArrayRef<glare::TaskRef>((Reference<glare::Task>*)place_tasks, num_tasks));


		//conPrint("place pass: " + timer.elapsedStringNSigFigs(5));
	}


	template<class T, class GetKey>
	inline void countingSortPass(T* __restrict in_data, T* __restrict out_data, size_t num_items, GetKey getKey, uint32* temp_counts, int shift_amount)
	{
		// Put items in final position
		for(size_t i=0; i<num_items; ++i)
		{
			const T val = in_data[i];
			out_data[temp_counts[BitUtils::getLowestNBits(getKey(val) >> shift_amount, 11)]++] = val;
		}
	}

	template<class T, class GetKey>
	inline void radixSort32BitKey(T* __restrict data, T* __restrict working_space, size_t num_items, GetKey getKey, uint32* temp_counts, size_t temp_counts_size)
	{
		assert(num_items <= std::numeric_limits<uint32>::max());

		const int num_buckets = 1 << 11;
		assert(temp_counts_size >= num_buckets * 3);

		for(size_t i=0; i<num_buckets * 3; ++i)
			temp_counts[i] = 0;

		for(size_t i=0; i<num_items; ++i)
		{
			const uint32 v = getKey(data[i]);
			const uint32 v0 = BitUtils::getLowestNBits(v  >> 0, 11);
			const uint32 v1 = BitUtils::getLowestNBits(v >> 11, 11);
			const uint32 v2 = BitUtils::getLowestNBits(v >> 22, 11);
			temp_counts[                v0]++;
			temp_counts[num_buckets   + v1]++;
			temp_counts[num_buckets*2 + v2]++;
		}

		size_t sum0 = 0;
		size_t sum1 = 0;
		size_t sum2 = 0;
		for(size_t i=0; i<num_buckets; ++i)
		{
			size_t count_i = temp_counts[i];
			temp_counts[i] = (uint32)sum0;
			sum0 += count_i;

			count_i = temp_counts[num_buckets + i];
			temp_counts[num_buckets + i] = (uint32)sum1;
			sum1 += count_i;

			count_i = temp_counts[num_buckets*2 + i];
			temp_counts[num_buckets*2 + i] = (uint32)sum2;
			sum2 += count_i;
		}

		countingSortPass(data, working_space, num_items, getKey, temp_counts                , 0);
		countingSortPass(working_space, data, num_items, getKey, temp_counts + num_buckets  , 11);
		countingSortPass(data, working_space, num_items, getKey, temp_counts + num_buckets*2, 22);

		// Copy back to data
		for(size_t i=0; i<num_items; ++i)
			data[i] = working_space[i];
	}

} // end namespace Sort
