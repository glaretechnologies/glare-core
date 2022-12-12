/*=====================================================================
TopologicalSort.cpp
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "TopologicalSort.h"


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


namespace TopologicalSort
{


template <typename T>
static void testSort(std::vector<T>& v, const std::vector<std::pair<T, T>>& follows_pairs)
{
	const bool sorted = topologicalSort(v.begin(), v.end(), follows_pairs);
	testAssert(sorted);

	for(size_t i=0; i<v.size(); ++i)
	{
		const T val = v[i];

		// Iterate over all later values in the array
		for(size_t z=i + 1; z<v.size(); ++z) 
		{
			const T later_val = v[z];

			// Check we don't have (val, later_val) in follows_map, e.g. we don't have val follows later_val.
			for(size_t q=0; q<follows_pairs.size(); ++q)
			{
				testAssert(!(follows_pairs[q].first == val && follows_pairs[q].second == later_val));
			}
		}
	}
}


void test()
{
	conPrint("TopologicalSort::test()");

	// Test with empty follows map
	{
		std::vector<int> v = { 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = {};
		testSort(v, follows_pairs);

		testAssert(v == std::vector<int>({ 1, 2 }) || v == std::vector<int>({ 2, 1 }));
	}

	// Test with single element
	{
		std::vector<int> v = { 1 };

		std::vector<std::pair<int, int>> follows_pairs = {};
		testSort(v, follows_pairs);

		testAssert(v == std::vector<int>({1}));
	}

	// Test with no elements
	{
		std::vector<int> v = {};

		std::vector<std::pair<int, int>> follows_pairs = {};
		testSort(v, follows_pairs);

		testAssert(v == std::vector<int>({}));
	}

	{
		std::vector<int> v = { 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1} }; // 2 follows 1
		testSort(v, follows_pairs);

		std::vector<int> target = { 1, 2 };
		testAssert(v == target);
	}


	{
		std::vector<int> v = { 3, 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1}, {3, 2} }; // 2 follows 1, 3 follows 2
		testSort(v, follows_pairs);

		std::vector<int> target = { 1, 2, 3 };
		testAssert(v == target);
	}

	// Test with elements already in topological order:
	{
		std::vector<int> v = { 1, 2, 3 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1}, {3, 2} }; // 2 follows 1, 3 follows 2
		testSort(v, follows_pairs);

		testAssert(v == std::vector<int>({ 1, 2, 3 }));
	}


	{
		std::vector<int> v = { 3, 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1}, {3, 1} }; // 2 follows 1, 3 follows 1
		testSort(v, follows_pairs);

		// Acceptable results are [1, 2, 3] and [1, 3, 2].
		testAssert(v == std::vector<int>({ 1, 2, 3 }) || v == std::vector<int>({ 1, 3, 2 }));
	}

	// Test with a cycle
	{
		std::vector<int> v = { 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = { {1, 2}, {2, 1} }; // 1 follows 2, 2 follows 1
		testAssert(!topologicalSort(v.begin(), v.end(), follows_pairs)); // Should return false
	}

	// Test with a larger cycle
	{
		std::vector<int> v = { 1, 2, 3 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1}, {3, 2}, {1, 3} };
		testAssert(!topologicalSort(v.begin(), v.end(), follows_pairs)); // Should return false
	}


	// Test with more stuff
	{
		std::vector<int> v = { 7, 6, 5, 4, 3, 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1}, {3, 2}, {4, 1}, {7, 4}, {6, 3} }; 
		testSort(v, follows_pairs);
	}

	{
		std::vector<int> v = { 7, 6, 5, 4, 3, 2, 1 };

		std::vector<std::pair<int, int>> follows_pairs = { {2, 1}, {3, 2}, {4, 1}, {7, 4}, {5, 6} }; 
		testSort(v, follows_pairs);
	}

	conPrint("TopologicalSort::test() done");
}
	

} // end namespace TopologicalSort


#endif
