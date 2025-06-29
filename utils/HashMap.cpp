/*=====================================================================
HashMap.cpp
------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "HashMap.h"


#if BUILD_TESTS


#include "RefCounted.h"
#include "Reference.h"
#include "ConPrint.h"
#include "TestUtils.h"
#include "Timer.h"
#include "Hasher.h"
//#include "HashMapInsertOnly2.h"
//#include "HashMap2.h"
#include "StringUtils.h"
#include "../maths/PCG32.h"
#include "PlatformUtils.h"
#include <map>
#include <unordered_map>

#if TEST_JOLT_UNORDERED_MAP
#include <Jolt/Jolt.h>
#include <Jolt/Core/UnorderedMap.h>
#endif


template <class K, class V, class H>
static void printMap(HashMap<K, V, H>& m)
{
	conPrint("-----------------------------------------");
	for(size_t i=0; i<m.buckets_size; ++i)
		conPrint("bucket " + toString(i) + ": (" + toString(m.buckets[i].first) + ", " + toString(m.buckets[i].second) + ")");
}


class HashMapTestClass : public RefCounted
{
public:
};

struct TestIdentityHashFunc
{
	size_t operator() (const int& x) const
	{
		return (size_t)x;
	}
};


static void printTimingResult(const std::string& op_name, double elapsed, int N)
{
	conPrint(rightPad(op_name + " took", ' ', /*minwidth=*/40) + ::doubleToStringNSigFigs(elapsed * 1.0e3, 4) + " ms (" + 
		doubleToStringNSigFigs(elapsed / N * 1.0e9, 4) + " ns / op, " + 
		doubleToStringNSigFigs(N / elapsed * 1.0e-6, 4) + " M ops/s)");
}


static void testHashMapPerf(const std::vector<int>& testdata, int N, int NUM_LOOKUPS, bool do_reservation)
{
	// Test std::map
	/*{
		Timer timer;
		std::map<int, int> m;

		for(int i=0; i<N; ++i)
		{
			const int x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		printTimingResult("std::map insert", elapsed, N);

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = testdata[i];
			std::map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		printTimingResult("std::map lookups", elapsed, N);
		TestUtils::silentPrint(toString(num_present));
	}*/

	// Test std::unordered_map
	{
		Timer timer;
		std::unordered_map<int, int> m;
		if(do_reservation)
			m.reserve(N);

		for(int i=0; i<N; ++i)
		{
			const int x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		printTimingResult("std::unordered_map insert", elapsed, N);

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = testdata[i];
			std::unordered_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		printTimingResult("std::unordered_map lookups", elapsed, N);
		TestUtils::silentPrint(toString(num_present));
	}
	
	// Test HashMapInsertOnly2
#if 0
	{
		Timer timer;
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max(), /*expected num items=*/do_reservation ? N : 0);

		for(int i=0; i<N; ++i)
		{
			const int x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		printTimingResult("HashMapInsertOnly2 insert", elapsed, N);

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = testdata[i];
			HashMapInsertOnly2<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		printTimingResult("HashMapInsertOnly2 lookups", elapsed, N);
		TestUtils::silentPrint(toString(num_present));
	}
#endif

	{
		Timer timer;
		HashMap<int, int> m(std::numeric_limits<int>::max(), /*expected num items=*/do_reservation ? N : 0);
		
		for(int i=0; i<N; ++i)
		{
			const int x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		printTimingResult("HashMap insert", elapsed, N);

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = testdata[i];
			HashMap<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		printTimingResult("HashMap lookups", elapsed, N);
		TestUtils::silentPrint(toString(num_present));
		printVar(m.buckets_size);
	}

	
#if 0
	// Test HashMap2 (experimental non-power-of-2 buckets)
	{
		Timer timer;
		HashMap2<int, int> m(std::numeric_limits<int>::max(), /*expected num items=*/do_reservation ? N : 0);
		
		for(int i=0; i<N; ++i)
		{
			const int x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		printTimingResult("HashMap2 insert", elapsed, N);

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = testdata[i];
			HashMap2<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		printTimingResult("HashMap2 lookups", elapsed, N);
		TestUtils::silentPrint(toString(num_present));
		printVar(m.buckets_size);
	}
#endif
	
	// Test Jolt hash table
#if TEST_JOLT_UNORDERED_MAP
	{
		Timer timer;
		JPH::UnorderedMap<int, int> m;//(std::numeric_limits<int>::max(), /*expected num items=*/do_reservation ? N : 0);
		
		if(do_reservation)
			m.reserve(N);

		for(int i=0; i<N; ++i)
		{
			const int x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		printTimingResult("JPH::UnorderedMap insert", elapsed, N);

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = testdata[i];
			JPH::UnorderedMap<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		printTimingResult("JPH::UnorderedMap lookups", elapsed, N);
		TestUtils::silentPrint(toString(num_present));
		//printVar(m.buckets_size);
	}
#endif // TEST_JOLT_UNORDERED_MAP
}


void testHashMap()
{
	conPrint("testHashMap()");


	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max(), /*expected num items=*/33);
		printVar(m.buckets_size);
	}

	// Test hash function performance
	{
		const int N = 1000000;
		{
			Timer timer;
			uint64 v = 0;
			for(int i=0; i<N; ++i)
			{
				std::hash<uint64> h;
				uint64 hashval = h(v);
				v ^= hashval;
			}
			double elapsed = timer.elapsed();
			conPrint("std::hash<uint64> took  " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s");
			conPrint(toString(v));
		}

		{
			Timer timer;
			uint64 v = 0;
			for(int i=0; i<N; ++i)
			{
				HasherOb<uint64> h;
				uint64 hashval = h(v);
				v ^= hashval;
			}
			double elapsed = timer.elapsed();
			conPrint("HasherOb<uint64> took   " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s");
			conPrint(toString(v));
		}

		{
			Timer timer;
			uint64 v = 0;
			for(int i=0; i<N; ++i)
			{
				uint64 hashval = hashBytes((const uint8*)&v, sizeof(uint64));
				v ^= hashval;
			}
			double elapsed = timer.elapsed();
			conPrint("hashBytes (uint64) took " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s");
			conPrint(toString(v));
		}

		{
			Timer timer;
			size_t v = 0;
			for(int i=0; i<N; ++i)
			{
				std::hash<uint32> h;
				size_t hashval = h((uint32)v);
				v ^= hashval;
			}
			double elapsed = timer.elapsed();
			conPrint("std::hash (uint32) took                 " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s");
			conPrint(toString(v));
		}

		{
			Timer timer;
			size_t v = 0;
			for(int i=0; i<N; ++i)
			{
				size_t hashval = hashBytes((const uint8*)&v, sizeof(uint32));
				v ^= hashval;
			}
			double elapsed = timer.elapsed();
			conPrint("hashBytes (uint32) took                 " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s");
			conPrint(toString(v));
		}
	}




	// For each bucket i, find an item that hashes to bucket i, add it, then remove it.
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		for(int b=0; b<32; ++b)
		{
			for(int x=0; ; ++x)
			{
				std::hash<int> hasher;
				const int bucket = hasher(x) % 32;
				if(bucket == b)
				{
					m.insert(std::make_pair(x, x));
					m.erase(x);
					break;
				}
			}
		}

		m.find(0); // infinite loop with naive code.
	}

	// For each bucket i, find an item that hashes to bucket 0, add it, then remove it.
	{
		HashMap<int, int, TestIdentityHashFunc> m(/*empty key=*/std::numeric_limits<int>::max());

		m.insert(std::make_pair(31, 31));
		for(int b=30; b>=0; --b)
		{
			m.insert(std::make_pair(b, b));
			m.erase(b);
		}

		m.erase(31);

		m.find(0); // infinite loop with naive code.
	}


	// Test with storing references, so we can check we create and destroy all elements properly.
	{
		Reference<HashMapTestClass> test_ob = new HashMapTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			HashMap<int, Reference<HashMapTestClass> > m(/*empty key=*/std::numeric_limits<int>::max());


			m.insert(std::make_pair(1, test_ob));
			testAssert(m.find(1) != m.end());
			testAssert(m.find(1)->second.getPointer() == test_ob.getPointer());

			// Add lots of items, to force an expand.
			for(int i=0; i<100; ++i)
				m.insert(std::make_pair(i, test_ob));

			for(int i=0; i<100; ++i)
			{
				testAssert(m.find(i) != m.end());
				testAssert(m.find(i)->second.getPointer() == test_ob.getPointer());
			}

		}

		testAssert(test_ob->getRefCount() == 1);
	}

	// Test erase()
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		m.insert(std::make_pair(1, 10));
		m.insert(std::make_pair(2, 20));
		m.insert(std::make_pair(3, 30));

		testAssert(m.size() == 3);

		testAssert(m.find(2) != m.end());
		m.erase(2);
		testAssert(m.find(2) == m.end());
		testAssert(m.size() == 2);

		testAssert(m.find(1) != m.end());
		m.erase(1);
		testAssert(m.find(1) == m.end());
		testAssert(m.size() == 1);

		testAssert(m.find(3) != m.end());
		m.erase(3);
		testAssert(m.find(3) == m.end());
		testAssert(m.size() == 0);

		// Check erasing a non-existent key is fine
		m.erase(100);
		testAssert(m.size() == 0);
	}

	// Test erase with references
	{
		Reference<HashMapTestClass> test_ob = new HashMapTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			HashMap<int, Reference<HashMapTestClass> > m(/*empty key=*/std::numeric_limits<int>::max());
			
			m.insert(std::make_pair(1, test_ob));
			testAssert(test_ob->getRefCount() == 2);
			m.erase(1);
			testAssert(test_ob->getRefCount() == 1);
		}
	}

	// Insert and erase a lot of elements a few times
	{
		
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		for(int z=0; z<10; ++z)
		{
			for(int i=0; i<100; ++i)
				m.insert(std::make_pair(i, 1000 + i));

			testAssert(m.size() == 100);

			for(int i=0; i<100; ++i)
				testAssert(m.find(i) != m.end());

			for(int i=0; i<100; ++i)
				m.erase(i);

			testAssert(m.size() == 0);

			for(int i=0; i<100; ++i)
				testAssert(m.find(i) == m.end());
		}
	}


	// Test inserting a (key, value) pair with same key.  Should not change the value in the map.
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		std::pair<HashMap<int, int>::iterator, bool> res = m.insert(std::make_pair(1, 2));
		testAssert(res.second);
		testAssert(m.find(1) != m.end());
		testAssert(m.find(1)->second == 2);

		// Insert another (key, value) pair with same key.  Should not change the value in the map.
		res = m.insert(std::make_pair(1, 3));
		testAssert(!res.second);
		testAssert(m.find(1) != m.end());
		testAssert(m.find(1)->second == 2);
	}

	// Test constructor with num elems expected
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max(), /*expected num elems=*/1000);
		testAssert(m.empty());

		m.insert(std::make_pair(1, 2));
		testAssert(m.find(1) != m.end());
		testAssert(m.find(1)->second == 2);
	}

	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		testAssert(m.empty());

		std::pair<HashMap<int, int>::iterator, bool> res = m.insert(std::make_pair(1, 2));
		testAssert(res.first == m.begin());
		testAssert(res.second);

		testAssert(!m.empty());
		testAssert(m.size() == 1);

		res = m.insert(std::make_pair(1, 2)); // Insert same key
		testAssert(res.first == m.begin());
		testAssert(!res.second);

		testAssert(m.size() == 1);

		// Test find()
		HashMap<int, int>::iterator i = m.find(1);
		testAssert(i->first == 1);
		testAssert(i->second == 2);
		i = m.find(5);
		testAssert(i == m.end());

		testAssert(m[1] == 2);
	}

	// Test clear()
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());
		m.insert(std::make_pair(1, 2));
		m.clear();
		testAssert(m.find(1) == m.end());
		testAssert(m.empty());
	}

	// Test clear() with references - make sure clear() removes the reference value.
	{
		Reference<HashMapTestClass> test_ob = new HashMapTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			HashMap<int, Reference<HashMapTestClass> > m(/*empty key=*/std::numeric_limits<int>::max());
			m.insert(std::make_pair(1, test_ob));
			testAssert(test_ob->getRefCount() == 2);
			m.clear();
			testAssert(test_ob->getRefCount() == 1);
		}
		testAssert(test_ob->getRefCount() == 1);
	}

	// Test iterators
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());
		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() == m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));

		m.insert(std::make_pair(1, 2));

		testAssert(m.begin()->first == 1);
		testAssert(m.begin()->second == 2);

		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() != m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));
	}

	// Test const iterators
	{
		HashMap<int, int> non_const_m(/*empty key=*/std::numeric_limits<int>::max());
		const HashMap<int, int>& m = non_const_m;
		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() == m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));

		non_const_m.insert(std::make_pair(1, 2));

		testAssert(m.begin()->first == 1);
		testAssert(m.begin()->second == 2);

		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() != m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));
	}


	{
		// Insert lots of values
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		const int N = 10000;
		for(int i=0; i<N; ++i)
		{
			std::pair<HashMap<int, int>::iterator, bool> res = m.insert(std::make_pair(i, i*2));
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
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		const int N = 10;
		for(int i=0; i<N; ++i)
		{
			m.insert(std::make_pair(i, i*2));
		}

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMap<int, int>::iterator i=m.begin(); i != m.end(); ++i)
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
			for(HashMap<int, int>::const_iterator i=m.begin(); i != m.end(); ++i)
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
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());

		const int N = 10000;
		for(int i=0; i<N; ++i)
			m.insert(std::make_pair(i, i*2));

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashMap<int, int>::iterator i=m.begin(); i != m.end(); ++i)
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
			for(HashMap<int, int>::const_iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[i->first]);
				seen[i->first] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}
	}


	// Do a stress test with randomly inserted integer keys
	{
		HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());
		const int N = 100000;
		std::vector<bool> ref_inserted(1000, false);
		PCG32 rng(1);
		for(int i=0; i<N; ++i)
		{
			const int x = (int)(rng.unitRandom() * 999.99f);
			assert(x >= 0 && x < 1000);

			std::pair<HashMap<int, int>::iterator, bool> res = m.insert(std::make_pair(x, x));

			testAssert(res.second == !ref_inserted[x]); // Should only have been inserted if wasn't in there already.
			ref_inserted[x] = true;
		}

		testAssert(m.size() == 1000);
	}


	// Do a stress test with randomly inserted and removed integer keys
	{
		PCG32 rng(1);
		for(int t=0; t<10; ++t)
		{
			HashMap<int, int> m(/*empty key=*/std::numeric_limits<int>::max());
			const int N = 1 << 16;
			std::vector<bool> ref_inserted(1000, false);

			for(int i=0; i<N; ++i)
			{
				const int x = (int)(rng.unitRandom() * 999.99f);
				assert(x >= 0 && x < 1000);

				if(rng.unitRandom() < 0.5f)
				{
					std::pair<HashMap<int, int>::iterator, bool> res = m.insert(std::make_pair(x, x));
					const bool was_inserted = res.second;
					testAssert(was_inserted == !ref_inserted[x]); // Should only have been inserted if wasn't in there already.
					ref_inserted[x] = true;
				}
				else
				{
					m.erase(x);
					ref_inserted[x] = false;
				}

				if(i % (1 << 16) == 0)
				{
					// Check lookups are correct
					for(int z=0; z<1000; ++z)
					{
						auto lookup_res = m.find(z);
						const bool is_inserted = lookup_res != m.end();
						testAssert(is_inserted == ref_inserted[z]);
					}

					// Check iteration is correct - set of objects while seen iterating over map is the same as ref_inserted.
					{
						std::vector<bool> seen(1000, false);
						for(auto it = m.begin(); it != m.end(); ++it)
						{
							const int val = it->first;
							testAssert(it->first == it->second);
							testAssert(seen[val] == 0);
							seen[val] = true;
						}
						testAssert(seen == ref_inserted);
					}

					m.invariant();
				}

				if(i % (1 << 20) == 0)
					conPrint("iter " + toString(i) + " / " + toString(N));
			}
		}
	}

	//======================= Test performance ===========================

	const int N = 2100000;
	//const int N = 34000000;
	const int NUM_LOOKUPS = N;

	std::vector<int> sparse_testdata(N); // Values that are mostly unique
	std::vector<int> dense_testdata(N); // Values that will mostly be duplicated
	PCG32 rng(1);
	for(int i=0; i<N; ++i)
		sparse_testdata[i] = (int)(rng.unitRandom() * 1.0e8f);
	for(int i=0; i<N; ++i)
		dense_testdata[i] = (int)(rng.unitRandom() * 1000.f);

	conPrint("\nperf for mostly unique keys");
	conPrint("-------------------------------------");
	testHashMapPerf(sparse_testdata, N, NUM_LOOKUPS, /*do reservation=*/false);

	conPrint("\nperf for mostly unique keys with size reservation");
	conPrint("-------------------------------------");
	testHashMapPerf(sparse_testdata, N, NUM_LOOKUPS, /*do reservation=*/true);
	
	conPrint("");
	conPrint("\nperf for mostly duplicate keys");
	conPrint("-------------------------------------");
	testHashMapPerf(dense_testdata, N, NUM_LOOKUPS, /*do reservation=*/false);


	conPrint("testHashMap() done.");
}


#endif // BUILD_TESTS
