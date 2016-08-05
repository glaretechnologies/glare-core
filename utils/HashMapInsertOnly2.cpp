/*=====================================================================
HashMapInsertOnly22.cpp
---------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Oct 18 13:13:09 +1300 2010
=====================================================================*/
#include "HashMapInsertOnly2.h"


#if BUILD_TESTS


#include "HashMapInsertOnly.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/MTwister.h"
#include "../utils/StringUtils.h"
#include <unordered_map>
//#include "D:\programming\sparsehash-2.0.2\src\sparsehash\dense_hash_map"


template <class K, class V, class H>
static void printMap(HashMapInsertOnly2<K, V, H>& m)
{
	conPrint("-----------------------------------------");
	for(size_t i=0; i<m.buckets.size(); ++i)
	{
		conPrint("bucket " + toString(i) + ": (" + toString(m.buckets[i].first) + ", " + toString(m.buckets[i].second) + ")");
	}
}


struct IdentityHash
{
	size_t operator () (int x) const
	{
		return (size_t)x;
	}
};


void testHashMapInsertOnly2()
{
	{
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());

		testAssert(m.empty());

		std::pair<HashMapInsertOnly2<int, int>::iterator, bool> res = m.insert(std::make_pair(1, 2));
		testAssert(res.first == m.begin());
		testAssert(res.second);

		testAssert(!m.empty());
		testAssert(m.size() == 1);

		res = m.insert(std::make_pair(1, 2)); // Insert same key
		testAssert(res.first == m.begin());
		testAssert(!res.second);

		testAssert(m.size() == 1);

		// Test find()
		HashMapInsertOnly2<int, int>::iterator i = m.find(1);
		testAssert(i->first == 1);
		testAssert(i->second == 2);
		i = m.find(5);
		testAssert(i == m.end());

		testAssert(m[1] == 2);
	}

	// Test clear()
	{
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
		m.insert(std::make_pair(1, 2));
		m.clear();
		testAssert(m.find(1) == m.end());
		testAssert(m.empty());
	}

	// Test iterators
	{
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
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
		HashMapInsertOnly2<int, int, IdentityHash> m(std::numeric_limits<int>::max());
		m.insert(std::make_pair(3, 3));
		m.insert(std::make_pair(7, 7));
	}



	{
		// Insert lots of values
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());

		const int N = 10000;
		for(int i=0; i<N; ++i)
		{
			std::pair<HashMapInsertOnly2<int, int>::iterator, bool> res = m.insert(std::make_pair(i, i*2));
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
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());

		const int N = 10;
		for(int i=0; i<N; ++i)
		{
			m.insert(std::make_pair(i, i*2));
		}

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMapInsertOnly2<int, int>::iterator i=m.begin(); i != m.end(); ++i)
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
			for(HashMapInsertOnly2<int, int>::const_iterator i=m.begin(); i != m.end(); ++i)
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
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());

		const int N = 10000;
		for(int i=0; i<N; ++i)
			m.insert(std::make_pair(i, i*2));

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMapInsertOnly2<int, int>::iterator i=m.begin(); i != m.end(); ++i)
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
			for(HashMapInsertOnly2<int, int>::const_iterator i=m.begin(); i != m.end(); ++i)
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
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
		const int N = 100000;
		std::vector<bool> inserted(1000, false);
		MTwister rng(1);
		for(int i=0; i<N; ++i)
		{
			const int x = (int)(rng.unitRandom() * 999.99f);
			assert(x >= 0 && x < 1000);

			std::pair<HashMapInsertOnly2<int, int>::iterator, bool> res = m.insert(std::make_pair(x, x));

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
	//		HashMapInsertOnly2<int, int> m;
	//		MTwister rng(1);
	//		for(int i=0; i<N; ++i)
	//		{
	//			const int x = (int)(rng.unitRandom() * N);
	//			m.insert(std::make_pair(x, x));
	//		}

	//		conPrint("HashMapInsertOnly2 insert took " + timer.elapsedString() + " for " + toString(m.size()));
	//	}
	//}
	//return;


	// Test performance

	const int N = 100000;
	const int NUM_LOOKUPS = 100000;

	std::vector<int> sparse_testdata(N);
	std::vector<int> dense_testdata(N);
	MTwister rng(1);
	for(int i=0; i<N; ++i)
		sparse_testdata[i] = (int)(rng.unitRandom() * 1.0e8f);
	for(int i=0; i<N; ++i)
		dense_testdata[i] = (int)(rng.unitRandom() * 1000.f);

	conPrint("perf for mostly unique keys");
	conPrint("-------------------------------------");
	{
		Timer timer;
		HashMapInsertOnly<int, int> m;
		
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("HashMapInsertOnly insert took   " + timer.elapsedString() + ", final size: " + toString(m.size()));
		
		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = sparse_testdata[i];
			HashMapInsertOnly<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("HashMapInsertOnly lookups took  " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	{
		Timer timer;
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
		
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("HashMapInsertOnly2 insert took  " + timer.elapsedString() + ", final size: " + toString(m.size()));
		
		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = sparse_testdata[i];
			HashMapInsertOnly2<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("HashMapInsertOnly2 lookups took " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	{
		Timer timer;
		std::unordered_map<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("std::unordered_map insert took  " + timer.elapsedString() + ", final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = sparse_testdata[i];
			std::unordered_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("std::unordered_map lookups took " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

	/*{
		Timer timer;
		google::dense_hash_map<int, int> m;
		m.set_empty_key(std::numeric_limits<int>::max());
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("google::dense_hash_map insert took " + timer.elapsedString() + ", final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = sparse_testdata[i];
			google::dense_hash_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("google::dense_hash_map lookups took " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}*/


	// Test insertion performance
	conPrint("");
	conPrint("perf for mostly duplicate keys");
	conPrint("-------------------------------------");
	{
		Timer timer;
		HashMapInsertOnly<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("HashMapInsertOnly insert took   " + timer.elapsedString() + ", final size: " + toString(m.size()));
		
		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = dense_testdata[i];
			HashMapInsertOnly<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("HashMapInsertOnly lookups took  " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

	{
		Timer timer;
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("HashMapInsertOnly2 insert took  " + timer.elapsedString() + ", final size: " + toString(m.size()));
		
		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = dense_testdata[i];
			HashMapInsertOnly2<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("HashMapInsertOnly2 lookups took " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	{
		Timer timer;
		std::unordered_map<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("std::unordered_map insert took  " + ::doubleToStringNSigFigs(elapsed, 5) + " s, final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = dense_testdata[i];
			std::unordered_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}
		elapsed = timer.elapsed();
		conPrint("std::unordered_map lookups took " + ::doubleToStringNSigFigs(elapsed, 5) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

	/*{
		Timer timer;
		google::dense_hash_map<int, int> m;
		m.set_empty_key(std::numeric_limits<int>::max());
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		conPrint("google::dense_hash_map insert took " + timer.elapsedString() + ", final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = dense_testdata[i];
			google::dense_hash_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		conPrint("google::dense_hash_map lookups took " + timer.elapsedString() + " for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}*/
}


#endif
