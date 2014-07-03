/*=====================================================================
Sort.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sat May 15 15:39:54 +1200 2010
=====================================================================*/
#include "Sort.h"


#include "Platform.h"
#include "MTwister.h"
#include "Timer.h"
#include "StringUtils.h"
#include <vector>
#include <algorithm>
#include "../utils/ConPrint.h"
#include "../indigo/TestUtils.h"


namespace Sort
{


class Item
{
public:
	int i;
	float f;

	// int padding[16];
};


class ItemPredicate
{
public:
	inline bool operator()(const Item& a, const Item& b)
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


#if (BUILD_TESTS)
void test()
{
	MTwister rng(1);

	for(uint32 N = 10; N<=100000/*16777216*/; N*=2)
	{
		std::vector<Item> f(N);
		for(size_t i=0; i<f.size(); ++i)
		{
			f[i].i = (int)i;
			f[i].f = -100.0f + rng.unitRandom() * 200.0f;
			//f[i] = -100.0f + rng.unitRandom() * 200.0f;
		}

		std::vector<Item> radix_sorted = f;
		std::vector<Item> std_sort_sorted = f;

		std::vector<bool> seen(N, false);


		Timer timer;

		ItemKey item_key;
		radixSort<Item, ItemKey>(&radix_sorted[0], N, item_key);

		const double radix_time = timer.elapsed();


		//ItemPredicate less_than;

		// Check the results actually are sorted.
		for(size_t i=1; i<radix_sorted.size(); ++i)
		{
			testAssert(radix_sorted[i].f >= radix_sorted[i-1].f);
			//testAssert(radix_sorted[i] >= radix_sorted[i-1]);
		}

		// Check all original items are present.
		for(size_t i=0; i<radix_sorted.size(); ++i)
		{
			testAssert(!seen[radix_sorted[i].i]);
			seen[radix_sorted[i].i] = true;
		}

		// std::sort
		timer.reset();
		ItemPredicate pred;
		std::sort(std_sort_sorted.begin(), std_sort_sorted.end(), pred);
		const double std_sort_time = timer.elapsed();

		conPrint("N: "  + toString(N));
		conPrint("radix_time:    " + toString(radix_time) + " s");
		conPrint("std_sort_time: " + toString(std_sort_time) + " s");
		conPrint("speedup: " + toString(std_sort_time / radix_time) + " x");

		conPrint("Sort speed: " + toString(1.0e-6 * N / radix_time) + " M Keys/sec");
	}
}
#endif
	

} // end namespace Sort
