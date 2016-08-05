/*=====================================================================
HashMapInsertOnly.cpp
---------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Oct 18 13:13:09 +1300 2010
=====================================================================*/
#include "HashMapInsertOnly.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/Timer.h"
#include "../utils/MTwister.h"
#include "../utils/StringUtils.h"


template <class K, class V, class H>
static void printMap(HashMapInsertOnly<K, V, H>& m)
{
	conPrint("-----------------------------------------");
	for(size_t i=0; i<m.buckets.size(); ++i)
	{
		conPrint("bucket " + toString(i) + ": mask = " + toString(m.mask.getBit(i)) + ", (" + toString(m.buckets[i].first) + ", " + toString(m.buckets[i].second) + ")");
	}
}


struct IdentityHash
{
	size_t operator () (int x) const
	{
		return (size_t)x;
	}
};


void testHashMapInsertOnly()
{
	{
		HashMapInsertOnly<int, int> m;

		testAssert(m.empty());

		std::pair<HashMapInsertOnly<int, int>::iterator, bool> res = m.insert(std::make_pair(1, 2));
		testAssert(res.first == m.begin());
		testAssert(res.second);

		testAssert(!m.empty());
		testAssert(m.size() == 1);

		res = m.insert(std::make_pair(1, 2)); // Insert same key
		testAssert(res.first == m.begin());
		testAssert(!res.second);

		testAssert(m.size() == 1);

		// Test find()
		HashMapInsertOnly<int, int>::iterator i = m.find(1);
		testAssert(i->first == 1);
		testAssert(i->second == 2);
		i = m.find(5);
		testAssert(i == m.end());

		testAssert(m[1] == 2);
	}

	// Test clear()
	{
		HashMapInsertOnly<int, int> m;
		m.insert(std::make_pair(1, 2));
		m.clear();
		testAssert(m.find(1) == m.end());
		testAssert(m.empty());
	}

	// Test iterators
	{
		HashMapInsertOnly<int, int> m;
		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() == m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));

		m.insert(std::make_pair(1, 2));

		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() != m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));
	}


	// Test wrap-around when looking for empty buckets
	{
		HashMapInsertOnly<int, int, IdentityHash> m;
		m.insert(std::make_pair(3, 3));
		m.insert(std::make_pair(7, 7));
	}



	{
		// Insert lots of values
		HashMapInsertOnly<int, int> m;

		const int N = 10000;
		for(int i=0; i<N; ++i)
		{
			std::pair<HashMapInsertOnly<int, int>::iterator, bool> res = m.insert(std::make_pair(i, i*2));
			testAssert(*(res.first) == std::make_pair(i, i*2));
			testAssert(res.second);
		}

		testAssert(m.size() == (size_t)N);

		// Check that the key, value pairs are present.
		for(int i=0; i<N; ++i)
		{
			testAssert(m.find(i) != m.end());
			testAssert(m.find(i)->first == i);
			testAssert(m.find(i)->second == i*2);
		}
	}

	// Test iteration with sparse values.
	{
		// Insert lots of values
		HashMapInsertOnly<int, int> m;

		const int N = 10;
		for(int i=0; i<N; ++i)
		{
			m.insert(std::make_pair(i, i*2));
		}

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMapInsertOnly<int, int>::iterator i=m.begin(); i != m.end(); ++i)
			{
				const int key = i->first;
				testAssert(!seen[key]);
				seen[key] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}

		// Check const_iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMapInsertOnly<int, int>::const_iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[i->first]);
				seen[i->first] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}
	}

	// Test iteration with dense values.
	{
		// Insert lots of values
		HashMapInsertOnly<int, int> m;

		const int N = 10000;
		for(int i=0; i<N; ++i)
			m.insert(std::make_pair(i, i*2));

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMapInsertOnly<int, int>::iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[i->first]);
				seen[i->first] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}

		// Check const_iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMapInsertOnly<int, int>::const_iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[i->first]);
				seen[i->first] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}
	}


	{
		HashMapInsertOnly<int, int> m;
		const int N = 100000;
		std::vector<bool> inserted(1000, false);
		MTwister rng(1);
		for(int i=0; i<N; ++i)
		{
			const int x = (int)(rng.unitRandom() * 999.99f);
			assert(x >= 0 && x < 1000);

			std::pair<HashMapInsertOnly<int, int>::iterator, bool> res = m.insert(std::make_pair(x, x));

			if(inserted[x])
				testAssert(res.second == false);
			else
				testAssert(res.second == true);

			inserted[x] = true;
		}

		testAssert(m.size() == 1000);
	}


	//for(int z=0; z<100; ++z)
	//{
	//	const int N = 1000000;

	//	// Test insertion performance
	//	conPrint("");
	//	conPrint("perf for mostly duplicate keys");
	//	conPrint("-------------------------------------");
	//	{
	//		Timer timer;
	//		HashMapInsertOnly<int, int> m;
	//		MTwister rng(1);
	//		for(int i=0; i<N; ++i)
	//		{
	//			const int x = (int)(rng.unitRandom() * N);
	//			m.insert(std::make_pair(x, x));
	//		}

	//		conPrint("HashMapInsertOnly insert took " + timer.elapsedString() + " for " + toString(m.size()));
	//	}
	//}
	//return;
}


#endif
