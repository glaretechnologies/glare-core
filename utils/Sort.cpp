/*=====================================================================
Sort.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Sat May 15 15:39:54 +1200 2010
=====================================================================*/
#include "Sort.h"


#if BUILD_TESTS


#include "Platform.h"
#include "../maths/PCG32.h"
#include "Timer.h"
#include "StringUtils.h"
#include "ConPrint.h"
#include "Vector.h"
#include "Plotter.h"
#include "../utils/TestUtils.h"
#include <vector>
#include <algorithm>
#include <map>
//#include "D:\programming\tbb44_20150728oss_win\include\tbb\parallel_sort.h"


/*
// Includes for falseSharingTest() below
#include <chrono>
#include <thread>
#include <iostream>
#include <random>*/


namespace Sort
{


#if 0
// Source code for http://www.forwardscattering.org/post/29

void threadFunc(const int* data_, size_t num, size_t* local_counts_)
{
	const int* const data = data_;
	size_t* const local_counts = local_counts_;

	for(size_t i=0; i<num; ++i)
		local_counts[data[i]]++;
}


void testWithPadding(const std::vector<int>& data, size_t padding_elems)
{
	auto start_time = std::chrono::high_resolution_clock::now();

	static_assert(sizeof(size_t) == 8, "sizeof(size_t) == 8");

	const int num_threads = 4;

	// Allocate memory for counts, align to 128-byte boundaries.
	size_t* counts = (size_t*)_mm_malloc(num_threads * (8 + padding_elems) * sizeof(size_t), 128);
	std::memset(counts, 0, num_threads * (8 + padding_elems) * sizeof(size_t)); // zero counts
	
	std::thread threads[num_threads];
	for(int i=0; i<num_threads; ++i)
		threads[i] = std::thread(threadFunc, /*data=*/data.data() + (data.size()/num_threads)*i, /*num=*/data.size()/num_threads, /*local_counts=*/counts + (8 + padding_elems)*i);

	for(int i=0; i<num_threads; ++i)
		threads[i].join();

	auto end_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> diff = end_time - start_time;

	std::cout << "padding: " << padding_elems*sizeof(size_t) << " B, elapsed: " << diff.count() << " s" << std::endl;

	_mm_free(counts);
}


void falseSharingTest()
{
	// Generate random data to count
	std::default_random_engine generator;
	std::uniform_int_distribution<int> distribution(0, 7);
	const size_t DATA_SIZE = 1 << 26;
	std::vector<int> data(DATA_SIZE);
	for(size_t i=0; i<DATA_SIZE; ++i)
		data[i] = distribution(generator);

	// Test performance with various padding sizes
	for(int i=0; i<32; ++i)
		testWithPadding(data, i);
}
#endif








class Item
{
public:
	//int i;
	float f;

	// int padding[16];
};


class ItemPredicate
{
public:
	inline bool operator()(const Item& a, const Item& b) const
	{
		return a.f < b.f;
	}
};


class ItemKey
{
public:
	inline float operator() (const Item& item)
	{
		return item.f;
	}
};


/*
typedef float Item;

class ItemPredicate
{
public:
	inline bool operator()(float a, float b)
	{
		return a < b;
	}
};


class ItemKey
{
public:
	inline float operator() (float x)
	{
		return x;
	}
};*/


#if BUILD_TESTS


static void checkSorted(const std::vector<Item>& original, const std::vector<Item>& sorted)
{
	// Check the results actually are sorted.
	for(size_t i=1; i<sorted.size(); ++i)
		testAssert(sorted[i].f >= sorted[i-1].f);

	// Check it's a permutation
	std::map<Item, int, ItemPredicate> item_count;
	for(size_t i=0; i<original.size(); ++i)
		item_count[original[i]]++;

	for(size_t i=0; i<sorted.size(); ++i)
	{
		Item item = sorted[i];
		testAssert(item_count[item] >= 1);
		item_count[item]--;
	}

	for(std::map<Item, int, ItemPredicate>::iterator i = item_count.begin(); i != item_count.end(); ++i)
		testAssert(i->second == 0);
}


class TestPartitionPred
{
public:
	bool operator() (int x) const { return x <= val; }
	int val;
};


static void testStablePartition(size_t N, size_t num_threads)
{
	Indigo::TaskManager task_manager(num_threads);
	
	// Make some random data
	std::vector<int> data(N);
	for(size_t i=0; i<N; ++i)
		data[i] = (int)i;
	
	// Shuffle
	PCG32 rng(1);
	for(int t=(int)N-1; t>=0; --t)
	{
		int k = (int)(rng.unitRandom() * t);
		mySwap(data[t], data[k]);
	}

	// Print data
	//for(size_t i=0; i<N; ++i)
	//	conPrint("data[" + toString(i) + "]: " + toString(data[i]));
	
	// Test with a few different parition values.
	int partition_vals[] = {-1, 0, 1, 2, (int)N/2 - 1, (int)N/2, (int)N/2 + 1, (int)N-2, (int)N-1, (int)N};
	for(size_t z=0; z<sizeof(partition_vals)/sizeof(int); ++z)
	{
		const int partition_val = partition_vals[z];
		//printVar(partition_val);
		TestPartitionPred pred;
		pred.val = partition_val;

		std::vector<int> partitioned(N);
		const size_t par_num_left = parallelStablePartition(task_manager, data.data(), partitioned.data(), N, pred);

		// Print partitioned
		//for(size_t i=0; i<N; ++i)
		//	conPrint("partitioned[" + toString(i) + "]: " + toString(partitioned[i]));

		// Test against (serial) stablePartition() as well.
		std::vector<int> serial_partitioned(N);
		const size_t num_left = stablePartition(data.data(), serial_partitioned.data(), N, pred);
		testAssert(par_num_left == num_left);
		testAssert(partitioned == serial_partitioned);

		// Test against std::stable_partition
		std::vector<int> std_partitioned = data;
		std::vector<int>::iterator first_right = std::stable_partition(std_partitioned.begin(), std_partitioned.end(), pred);
		testAssert((size_t)(first_right - std_partitioned.begin()) == num_left);
		testAssert(partitioned == std_partitioned);
	}
}


static void stablePartitionPerftest(size_t N, size_t num_threads)
{
	Indigo::TaskManager task_manager(num_threads);
	
	// Make some random data
	std::vector<int> data(N);
	for(size_t i=0; i<N; ++i)
		data[i] = (int)i;
	
	// Shuffle
	PCG32 rng(1);
	for(int t=(int)N-1; t>=0; --t)
	{
		int k = (int)(rng.unitRandom() * t);
		mySwap(data[t], data[k]);
	}

	// Test with a few different parition values.
	int partition_vals[] = {(int)N/2};
	for(size_t z=0; z<sizeof(partition_vals)/sizeof(int); ++z)
	{
		const int partition_val = partition_vals[z];
		printVar(partition_val);

		std::vector<int> partitioned(N);
		TestPartitionPred pred;
		pred.val = partition_val;

		Timer timer;
		parallelStablePartition(task_manager, data.data(), partitioned.data(), N, pred);
		const double parallel_elapsed = timer.elapsed();


		// Test against (serial) stablePartition() as well.
		std::vector<int> serial_partitioned(N);
		timer.reset();
		stablePartition(data.data(), serial_partitioned.data(), N, pred);
		const double serial_elapsed = timer.elapsed();

		testAssert(partitioned == serial_partitioned);


		// test std::stable_partition
		std::vector<int> std_data = data;
		timer.reset();
		std::stable_partition(std_data.begin(), std_data.end(), pred);
		const double std_elapsed = timer.elapsed();

		testAssert(std_data == partitioned);

		conPrint("parallelStablePartition: " + doubleToStringNDecimalPlaces(parallel_elapsed, 5) + " s (" + doubleToStringNDecimalPlaces(N / parallel_elapsed, 5) + " items/sec)"); 
		conPrint("stablePartition        : " + doubleToStringNDecimalPlaces(serial_elapsed, 5)   + " s (" + doubleToStringNDecimalPlaces(N / serial_elapsed, 5)   + " items/sec)");  
		conPrint("std::stable_partition  : " + doubleToStringNDecimalPlaces(std_elapsed, 5)      + " s (" + doubleToStringNDecimalPlaces(N / std_elapsed, 5)      + " items/sec)");  
	}
}



//=====================================================================================


class TestNWayPartitionBucketChooser
{
public:
	size_t operator() (int x) const { return x % 8; }
};


static void testStableNWayPartition(size_t N, size_t num_threads)
{
	Indigo::TaskManager task_manager(num_threads);
	
	// Make some random data
	std::vector<int> data(N);
	for(size_t i=0; i<N; ++i)
		data[i] = (int)i;
	
	// Shuffle
	PCG32 rng(1);
	for(int t=(int)N-1; t>=0; --t)
	{
		int k = (int)(rng.unitRandom() * t);
		mySwap(data[t], data[k]);
	}

	// Print data
	//for(size_t i=0; i<N; ++i)
	//	conPrint("data[" + toString(i) + "]: " + toString(data[i]));
	
	// Test with a few different parition values.
	{
		TestNWayPartitionBucketChooser pred;
		const size_t num_buckets = 8;

		std::vector<int> partitioned(N);
		parallelStableNWayPartition(task_manager, data.data(), partitioned.data(), N, num_buckets, pred);

		// Print partitioned
		//for(size_t i=0; i<N; ++i)
		//	conPrint("partitioned[" + toString(i) + "]: " + toString(partitioned[i]));

		// Test against (serial) stableNWayPartition() as well.
		std::vector<int> serial_partitioned(N);
		stableNWayPartition(data.data(), serial_partitioned.data(), N, num_buckets, pred);
		testAssert(partitioned == serial_partitioned);
	}
}


static double stableNWayPartitionPerfTest(size_t N, size_t num_threads)
{
	Indigo::TaskManager task_manager(num_threads);
	
	// Make some random data
	std::vector<int> data(N);
	for(size_t i=0; i<N; ++i)
		data[i] = (int)i;
	
	// Shuffle
	PCG32 rng(1);
	for(int t=(int)N-1; t>=0; --t)
	{
		int k = (int)(rng.unitRandom() * t);
		mySwap(data[t], data[k]);
	}

	double best_nway_part_items_per_sec = 0;
	// Print data
	//for(size_t i=0; i<N; ++i)
	//	conPrint("data[" + toString(i) + "]: " + toString(data[i]));
	
	// Test with a few different parition values.
	{
		TestNWayPartitionBucketChooser pred;
		const size_t num_buckets = 8;

		std::vector<int> partitioned(N);
		Timer timer;
		parallelStableNWayPartition(task_manager, data.data(), partitioned.data(), N, num_buckets, pred);
		const double parallel_elapsed = timer.elapsed();

		// Print partitioned
		//for(size_t i=0; i<N; ++i)
		//	conPrint("partitioned[" + toString(i) + "]: " + toString(partitioned[i]));

		// Test against (serial) stableNWayPartition() as well.
		std::vector<int> serial_partitioned(N);
		timer.reset();
		stableNWayPartition(data.data(), serial_partitioned.data(), N, num_buckets, pred);
		const double serial_elapsed = timer.elapsed();
		testAssert(partitioned == serial_partitioned);
		
		conPrint("parallelStableNWayPartition: " + doubleToStringNDecimalPlaces(parallel_elapsed, 5) + " s (" + doubleToStringNDecimalPlaces(N / parallel_elapsed, 5) + " items/sec)"); 
		conPrint("stableNWayPartition        : " + doubleToStringNDecimalPlaces(serial_elapsed, 5)   + " s (" + doubleToStringNDecimalPlaces(N / serial_elapsed, 5)   + " items/sec)");  

		best_nway_part_items_per_sec = myMax(best_nway_part_items_per_sec, N / parallel_elapsed);
	}

	return best_nway_part_items_per_sec;
}



void test()
{
	PCG32 rng(1);
	Indigo::TaskManager task_manager(8);
	Timer timer;
	
	
	//const uint32 N = 5000000;

	for(int N=1; N<1024; N*=2)
	{
		std::vector<Item> original(N);
		for(size_t i=0; i<original.size(); ++i)
		{
			//f[i].i = (int)i;
			original[i].f = -100.0f + rng.unitRandom() * 200.0f;
			//original[i].f = 100.0f + rng.unitRandom();
		}

		std::vector<Item> radix_sorted = original;
		std::vector<Item> radix16_sorted = original;
		std::vector<Item> bucket_sorted = original;
		std::vector<Item> radix_par_part_sorted = original;
		std::vector<Item> std_sort_sorted = original;
		//std::vector<Item> tbb_parallel_sort_sorted = original;

		ItemKey item_key;
		ItemPredicate item_pred;

		//-------- radix sort ---------
		timer.reset();
		radixSort<Item, ItemKey>(&radix_sorted[0], N, item_key);
		const double radix_time = timer.elapsed();

		//-------- radix sort 16 ---------
		timer.reset();
		radixSort16<Item, ItemKey>(&radix16_sorted[0], N, item_key);
		const double radix16_time = timer.elapsed();
		
		//-------- bucket sort ---------
		timer.reset();
		{
		js::Vector<Item, 16> working_space(N);
		bucketSort<Item, ItemPredicate, ItemKey>(task_manager, &bucket_sorted[0], &working_space[0], N, item_pred, item_key);
		}
		const double bucket_time = timer.elapsed();

		//-------- radixSortWithParallelPartition---------
		timer.reset();
		{
		js::Vector<Item, 16> working_space(N);
		radixSortWithParallelPartition<Item, ItemKey>(task_manager, &radix_par_part_sorted[0], N, item_key, &working_space[0], false);
		}
		const double radix_par_part_time = timer.elapsed();

		//-------- std::sort ---------
		timer.reset();
		std::sort(std_sort_sorted.begin(), std_sort_sorted.end(), item_pred);
		const double std_sort_time = timer.elapsed();
		
		//-------- tbb parallel sort ---------
		//timer.reset();
		//tbb::parallel_sort(tbb_parallel_sort_sorted.begin(), tbb_parallel_sort_sorted.end(), item_pred);
		//const double tbb_parallel_sort_time = timer.elapsed();

		

		conPrint("");
		conPrint("N: "  + toString(N));
		conPrint("radix_time:            " + toString(radix_time)    + " s (" + toString(1.0e-6 * N / radix_time) + " M keys/s)");
		conPrint("radix16_time:          " + toString(radix16_time)  + " s (" + toString(1.0e-6 * N / radix16_time) + " M keys/s)");
		conPrint("bucket_time:           " + toString(bucket_time)   + " s (" + toString(1.0e-6 * N / bucket_time) + " M keys/s)");
		conPrint("radix_par_part_time:   " + toString(radix_par_part_time)   + " s (" + toString(1.0e-6 * N / radix_par_part_time) + " M keys/s)");
		conPrint("std_sort_time:         " + toString(std_sort_time) + " s (" + toString(1.0e-6 * N / std_sort_time) + " M keys/s)");
		//conPrint("tbb_parallel_sort_time: " + toString(tbb_parallel_sort_time) + " s (" + toString(1.0e-6 * N / tbb_parallel_sort_time) + " M keys/s)");

		checkSorted(original, radix_sorted);
		checkSorted(original, radix16_sorted);
		checkSorted(original, bucket_sorted);
		checkSorted(original, radix_par_part_sorted);
		checkSorted(original, std_sort_sorted);
		//checkSorted(original, tbb_parallel_sort_sorted);
	}
	

	
	//=================== Test parallelStablePartition ===========================
	testStablePartition(1, 1);
	testStablePartition(2, 1);
	testStablePartition(3, 1);
	testStablePartition(4, 1);
	testStablePartition(1000, 1);

	testStablePartition(1, 4);
	testStablePartition(2, 4);
	testStablePartition(3, 4);
	testStablePartition(4, 4);
	testStablePartition(1000, 4);
	

	stablePartitionPerftest(1000, 4);
	/*if(true)
	{
		// Do perf test, serial vs parallel
		//stablePartitionPerftest(100000, 1);
		//stablePartitionPerftest(100000, 8);
		for(int z=0; z<10; ++z)
		stablePartitionPerftest(10000000, 4);
	}*/


	//=================== Test parallelStableNWayPartition ===========================

	testStableNWayPartition(1, 1);
	testStableNWayPartition(2, 1);
	testStableNWayPartition(3, 1);
	testStableNWayPartition(4, 1);
	testStableNWayPartition(16, 1);
	testStableNWayPartition(1000, 1);

	testStableNWayPartition(1, 4);
	testStableNWayPartition(2, 4);
	testStableNWayPartition(3, 4);
	testStableNWayPartition(4, 4);
	testStableNWayPartition(16, 4);
	testStableNWayPartition(1000, 4);

	stableNWayPartitionPerfTest(1000, 4);
	//for(int z=0; z<10; ++z)
	//	stableNWayPartitionPerfTest(10000000, 4);

	
	/*Plotter::DataSet dataset;
	//dataset.key = "cycles";

	for(int padding = 0; padding < 32; ++padding)
	{
		// Do perf test, serial vs parallel
		//stablePartitionPerftest(100000, 1);
		//stablePartitionPerftest(100000, 8);
		double best_items_sec = 0;
		for(int z=0; z<10; ++z)
			best_items_sec = myMax(best_items_sec, stableNWayPartitionPerfTest(10000000, 4, padding));

		printVar(padding);
		printVar(best_items_sec);
		const size_t padding_B = padding * 8;
		printVar(padding_B);


		dataset.points.push_back(Vec2f(padding_B, best_items_sec * 1.0e-9));
	}

	std::vector<Plotter::DataSet> datasets;
	datasets.push_back(dataset);
	Plotter::PlotOptions plot_options;
	Plotter::plot(
		"count_speed.png",
		"Counting speed - items/s vs padding size.",
		"padding size (B)",
		"1*10^9 items/s",
		datasets,
		plot_options
	);*/
}


#endif
	

} // end namespace Sort


#endif
