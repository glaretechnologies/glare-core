/*=====================================================================
Sort.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Sat May 15 15:39:54 +1200 2010
=====================================================================*/
#include "Sort.h"


#include "Platform.h"
#include "MTwister.h"
#include "Timer.h"
#include "StringUtils.h"
#include "ConPrint.h"
#include "Vector.h"
#include "../indigo/TestUtils.h"
#include <vector>
#include <algorithm>
#include <map>
//#include "D:\programming\tbb44_20150728oss_win\include\tbb\parallel_sort.h"


namespace Sort
{


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

	for(std::map<Item, int>::iterator i = item_count.begin(); i != item_count.end(); ++i)
		testAssert(i->second == 0);
}


template <class Dest, class Src> 
inline Dest bit_cast(const Src& x)
{
	static_assert(sizeof(Src) == sizeof(Dest), "sizeof(Src) == sizeof(Dest)");
	Dest d;
	std::memcpy(&d, &x, sizeof(Dest));
	return d;
}


void test()
{
	MTwister rng(1);
	Indigo::TaskManager task_manager(8);
	Timer timer;

	
	//const uint32 N = 5000000;

	for(int N=1; N<1000; N*=2)
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
		conPrint("radix_time:    " + toString(radix_time)    + " s (" + toString(1.0e-6 * N / radix_time) + " M keys/s)");
		conPrint("radix16_time:  " + toString(radix16_time)  + " s (" + toString(1.0e-6 * N / radix16_time) + " M keys/s)");
		conPrint("bucket_time:   " + toString(bucket_time)   + " s (" + toString(1.0e-6 * N / bucket_time) + " M keys/s)");
		conPrint("std_sort_time: " + toString(std_sort_time) + " s (" + toString(1.0e-6 * N / std_sort_time) + " M keys/s)");
		//conPrint("tbb_parallel_sort_time: " + toString(tbb_parallel_sort_time) + " s (" + toString(1.0e-6 * N / tbb_parallel_sort_time) + " M keys/s)");

		checkSorted(original, radix_sorted);
		checkSorted(original, radix16_sorted);
		checkSorted(original, bucket_sorted);
		checkSorted(original, std_sort_sorted);
		//checkSorted(original, tbb_parallel_sort_sorted);
	}
}


#endif
	

} // end namespace Sort
