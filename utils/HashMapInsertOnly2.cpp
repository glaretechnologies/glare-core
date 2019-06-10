/*=====================================================================
HashMapInsertOnly22.cpp
---------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Oct 18 13:13:09 +1300 2010
=====================================================================*/
#include "HashMapInsertOnly2.h"


//#define TEST_OTHER_HASH_MAPS 1


#if BUILD_TESTS


#include "HashMapInsertOnly.h"
#include "RefCounted.h"
#include "Reference.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "Timer.h"
#include "StringUtils.h"
#include "Plotter.h"
#include "../maths/PCG32.h"
#include <unordered_map>
#include <map>
#include "IncludeXXHash.h"
#if TEST_OTHER_HASH_MAPS
#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#include "D:\programming\sparsehash-2.0.2\src\sparsehash\dense_hash_map"
#include "D:\programming\sparsepp\sparsepp.h"

#endif


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


// See https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static inline uint64 FNVHash(const uint8* data, size_t len)
{
	uint64 hash = 14695981039346656037ULL;
	for(size_t i=0; i<len; ++i)
		hash = (hash ^ data[i]) * 1099511628211ULL;
	return hash;
}


static inline uint64 myHash(const uint8* data, size_t len)
{
	uint64 hash = 14695981039346656037ULL;
	const size_t rounded_len = (len / 4);
	for(size_t i=0; i<rounded_len; ++i)
	{
		const uint32 x = ((const uint32*)data)[i];
		hash = (hash ^ (x & 0xFF))       * 1099511628211ULL;
		hash = (hash ^ (x & 0xFF00))     * 1099511628211ULL;
		hash = (hash ^ (x & 0xFF0000))   * 1099511628211ULL;
		hash = (hash ^ (x & 0xFF000000)) * 1099511628211ULL;
	}

	for(size_t i=rounded_len; i<len; ++i)
		hash = (hash ^ data[i]) * 1099511628211ULL;

	return hash;
}


static inline uint32_t uint32Hash(uint32_t a)
{
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}


// Modified from std::hash: from c:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\include\xstddef, renamed from _Hash_seq
static inline size_t microsoftHash(const unsigned char *_First, size_t _Count)
{	// FNV-1a hash function for bytes in [_First, _First+_Count)

	if(sizeof(size_t) == 8)
	{
		const size_t _FNV_offset_basis = 14695981039346656037ULL;
		const size_t _FNV_prime = 1099511628211ULL;

		size_t _Val = _FNV_offset_basis;
		for(size_t _Next = 0; _Next < _Count; ++_Next)
		{	// fold in another byte
			_Val ^= (size_t)_First[_Next];
			_Val *= _FNV_prime;
		}

		_Val ^= _Val >> 32;
		return _Val;
	}
	else
	{
		const size_t _FNV_offset_basis = 2166136261U;
		const size_t _FNV_prime = 16777619U;

		size_t _Val = _FNV_offset_basis;
		for(size_t _Next = 0; _Next < _Count; ++_Next)
		{	// fold in another byte
			_Val ^= (size_t)_First[_Next];
			_Val *= _FNV_prime;
		}

		return _Val;
	}
}


struct TestKey
{
	inline bool operator == (const TestKey& other) const { return data == other.data; }
	inline bool operator != (const TestKey& other) const { return data != other.data; }
	inline bool operator < (const TestKey& other) const { return data < other.data; }
	uint64 data;
};


struct TestKeyIdentityHash
{
	size_t operator () (const TestKey& key) const
	{
		return key.data;
	}
};

struct TestKeyFNVHash
{
	size_t operator () (const TestKey& key) const
	{
		return FNVHash((const uint8*)&key.data, sizeof(key.data));
	}
};


struct TestKeyMicrosoftHash
{
	size_t operator () (const TestKey& key) const
	{
		return microsoftHash((const uint8*)&key.data, sizeof(key.data));
	}
};


struct TestKeyXXHash
{
	size_t operator () (const TestKey& key) const
	{
		return XXH64((const uint8*)&key.data, sizeof(key.data), /*seed=*/0);
	}
};


struct TestKeyMyHash
{
	size_t operator () (const TestKey& key) const
	{
		return myHash((const uint8*)&key.data, sizeof(key.data));
	}
};


struct TestKeyUInt32Hash
{
	size_t operator () (const TestKey& key) const
	{
		uint32 v = (uint32)key.data;
		return uint32Hash(v);
	}
};



// Assumes key type == value type.
template <class HashMapType, class KeyType>
static void testHashMap(const KeyType& unused_key, const std::vector<KeyType>& testdata, const std::string& hashmap_type_name)
{
	const int NUM_LOOKUPS = 1000000;
	const int N = (int)testdata.size();
	const int NUM_TRIALS = 10;

	double min_insert_time = 10000000;
	double min_lookup_time = 10000000;
	int num_present = 0;
	for(int t=0; t<NUM_TRIALS; ++t)
	{
		Timer timer;
		HashMapType m(unused_key);

		for(int i=0; i<N; ++i)
		{
			const KeyType x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		min_insert_time = myMin(min_insert_time, timer.elapsed());

		// Test lookup performance
		timer.reset();
		//int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const KeyType x = testdata[i];
			typename HashMapType::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		min_lookup_time = myMin(min_lookup_time, timer.elapsed());
	}

	//conPrint(::rightPad(hashmap_type_name, ' ', 50) + " insert took  " + ::doubleToStringNDecimalPlaces(min_insert_time, 6) + " s");
	//conPrint(::rightPad(hashmap_type_name, ' ', 50) + " lookups took " + ::doubleToStringNDecimalPlaces(min_lookup_time, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups.");
	conPrint(::rightPad(hashmap_type_name, ' ', 50) + " insert took  " + ::doubleToStringNDecimalPlaces(min_insert_time, 6) + " s, lookups took " + ::doubleToStringNDecimalPlaces(min_lookup_time, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups.");
	TestUtils::silentPrint("num_present: " + toString(num_present));
}


template <class HashMapType, class KeyType>
static void testHashMap(const std::vector<KeyType>& testdata, const std::string& hashmap_type_name)
{
	const int NUM_LOOKUPS = 1000000;
	const int N = (int)testdata.size();
	const int NUM_TRIALS = 10;

	double min_insert_time = 10000000;
	double min_lookup_time = 10000000;
	int num_present = 0;
	for(int t=0; t<NUM_TRIALS; ++t)
	{
		Timer timer;
		HashMapType m;

		for(int i=0; i<N; ++i)
		{
			const KeyType x = testdata[i];
			m.insert(std::make_pair(x, x));
		}

		min_insert_time = myMin(min_insert_time, timer.elapsed());

		// Test lookup performance
		timer.reset();
		//int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const KeyType x = testdata[i];
			typename HashMapType::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		min_lookup_time = myMin(min_lookup_time, timer.elapsed());
	}

	//conPrint(::rightPad(hashmap_type_name, ' ', 50) + " insert took  " + ::doubleToStringNDecimalPlaces(min_insert_time, 6) + " s");
	//conPrint(::rightPad(hashmap_type_name, ' ', 50) + " lookups took " + ::doubleToStringNDecimalPlaces(min_lookup_time, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups.");
	conPrint(::rightPad(hashmap_type_name, ' ', 50) + " insert took  " + ::doubleToStringNDecimalPlaces(min_insert_time, 6) + " s, lookups took " + ::doubleToStringNDecimalPlaces(min_lookup_time, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups.");
	TestUtils::silentPrint("num_present: " + toString(num_present));
}


class HashMapTestClass : public RefCounted
{
public:
};


void testHashMapInsertOnly2()
{
	// Test inserting some non-trivial structures
	{
		HashMapInsertOnly2<std::string, std::string> m("");

		m.insert(std::make_pair("a", "1"));
		testAssert(m.find("a") != m.end());
		testAssert(m.find("a")->second == "1");

		// Add lots of items, to force an expand.
		for(int i=0; i<100; ++i)
			m.insert(std::make_pair(toString(i), toString(i)));

		for(int i=0; i<100; ++i)
		{
			testAssert(m.find(toString(i)) != m.end());
			testAssert(m.find(toString(i))->second == toString(i));
		}
	}

	// Test with storing references, so we can check we create and destroy all elements properly.
	{
		Reference<HashMapTestClass> test_ob = new HashMapTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			HashMapInsertOnly2<int, Reference<HashMapTestClass> > m(std::numeric_limits<int>::max());


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


	// Test inserting a (key, value) pair with same key.  Should not change the value in the map.
	{
		std::unordered_map<int, int> m;

		std::pair<std::unordered_map<int, int>::iterator, bool> res = m.insert(std::make_pair(1, 2));
		testAssert(res.second);
		testAssert(m.find(1) != m.end());
		testAssert(m.find(1)->second == 2);

		// Insert another (key, value) pair with same key.  Should not change the value in the map.
		res = m.insert(std::make_pair(1, 3));
		testAssert(!res.second);
		testAssert(m.find(1) != m.end());
		testAssert(m.find(1)->second == 2);
	}

	{
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());

		std::pair<HashMapInsertOnly2<int, int>::iterator, bool> res = m.insert(std::make_pair(1, 2));
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
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max(), /*expected num elems=*/1000);
		testAssert(m.empty());

		m.insert(std::make_pair(1, 2));
		testAssert(m.find(1) != m.end());
		testAssert(m.find(1)->second == 2);
	}

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
		PCG32 rng(1);
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
	//		PCG32 rng(1);
	//		for(int i=0; i<N; ++i)
	//		{
	//			const int x = (int)(rng.unitRandom() * N);
	//			m.insert(std::make_pair(x, x));
	//		}

	//		conPrint("HashMapInsertOnly2 insert took " + timer.elapsedString() + " for " + toString(m.size()));
	//	}
	//}
	//return;


	//======================= Test performance ===========================
	if(false)
	{
		const int N = 1000000;

		std::vector<TestKey> increasing_testdata(N);
		std::vector<TestKey> dense_testdata(N);
		std::vector<TestKey> sparse_testdata(N);
		PCG32 rng(1);
		for(int i=0; i<N; ++i)
			increasing_testdata[i].data = i;
		for(int i=0; i<N; ++i)
			sparse_testdata[i].data = (int)(rng.unitRandom() * 1.0e8f);
		for(int i=0; i<N; ++i)
			dense_testdata[i].data = (int)(rng.unitRandom() * 1000.f);

		TestKey unused_key;
		unused_key.data = std::numeric_limits<uint64>::max();

		conPrint("\nperf for mostly unique keys");
		conPrint("-------------------------------------");
		//testHashMap<std::map<TestKey, TestKey>,                                 TestKey>(            sparse_testdata, "std::map (TestKeyIdentityHash)");
		//testHashMap<std::unordered_map<TestKey, TestKey, TestKeyIdentityHash>,  TestKey>(            sparse_testdata, "std::unordered_map (TestKeyIdentityHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyIdentityHash>,  TestKey>(unused_key, sparse_testdata, "HashMapInsertOnly2 (TestKeyIdentityHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyFNVHash>,       TestKey>(unused_key, sparse_testdata, "HashMapInsertOnly2 (TestKeyFNVHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyMicrosoftHash>, TestKey>(unused_key, sparse_testdata, "HashMapInsertOnly2 (TestKeyMicrosoftHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyXXHash>,        TestKey>(unused_key, sparse_testdata, "HashMapInsertOnly2 (TestKeyXXHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyMyHash>,        TestKey>(unused_key, sparse_testdata, "HashMapInsertOnly2 (TestKeyMyHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyUInt32Hash>,    TestKey>(unused_key, sparse_testdata, "HashMapInsertOnly2 (TestKeyUInt32Hash)");

		conPrint("\nperf for mostly duplicate keys");
		conPrint("-------------------------------------");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyIdentityHash>,  TestKey>(unused_key, dense_testdata, "HashMapInsertOnly2 (TestKeyIdentityHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyFNVHash>,       TestKey>(unused_key, dense_testdata, "HashMapInsertOnly2 (TestKeyFNVHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyMicrosoftHash>, TestKey>(unused_key, dense_testdata, "HashMapInsertOnly2 (TestKeyMicrosoftHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyXXHash>,        TestKey>(unused_key, dense_testdata, "HashMapInsertOnly2 (TestKeyXXHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyMyHash>,        TestKey>(unused_key, dense_testdata, "HashMapInsertOnly2 (MyHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyUInt32Hash>,    TestKey>(unused_key, dense_testdata, "HashMapInsertOnly2 (TestKeyUInt32Hash)");

		conPrint("\nperf for increasing keys");
		conPrint("-------------------------------------");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyIdentityHash>,  TestKey>(unused_key, increasing_testdata, "HashMapInsertOnly2 (TestKeyIdentityHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyFNVHash>,       TestKey>(unused_key, increasing_testdata, "HashMapInsertOnly2 (TestKeyFNVHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyMicrosoftHash>, TestKey>(unused_key, increasing_testdata, "HashMapInsertOnly2 (TestKeyMicrosoftHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyXXHash>,        TestKey>(unused_key, increasing_testdata, "HashMapInsertOnly2 (TestKeyXXHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyMyHash>,        TestKey>(unused_key, increasing_testdata, "HashMapInsertOnly2 (TestKeyMyHash)");
		testHashMap<HashMapInsertOnly2<TestKey, TestKey, TestKeyUInt32Hash>,    TestKey>(unused_key, increasing_testdata, "HashMapInsertOnly2 (TestKeyUInt32Hash)");

	}


	const int N = 1000000;
	const int NUM_LOOKUPS = 1000000;

	std::vector<int> sparse_testdata(N);
	std::vector<int> dense_testdata(N);
	PCG32 rng(1);
	for(int i=0; i<N; ++i)
		sparse_testdata[i] = (int)(rng.unitRandom() * 1.0e8f);
	for(int i=0; i<N; ++i)
		dense_testdata[i] = (int)(rng.unitRandom() * 1000.f);

	conPrint("\nperf for mostly unique keys");
	conPrint("-------------------------------------");
	{
		Timer timer;
		std::map<int, int> m;

		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		conPrint("std::mapinsert took                 " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = sparse_testdata[i];
			std::map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}

		elapsed = timer.elapsed();
		conPrint("std::map lookups took               " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	{
		Timer timer;
		HashMapInsertOnly<int, int> m;
		
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly insert took       " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));
		
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

		elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly lookups took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	{
		Timer timer;
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());

		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}

		double elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly2 insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

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

		elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly2 lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

#if 0 // Tests with variable load factor
	if(false)
	{
		std::vector<Plotter::DataSet> datasets;
		datasets.push_back(Plotter::DataSet("insertion time"));
		datasets.push_back(Plotter::DataSet("insertion time with reserve"));
		datasets.push_back(Plotter::DataSet("lookup time"));

		for(float max_load_factor = 0.2f; max_load_factor < 0.99f; max_load_factor += 0.01f)
		{
			Timer timer;
			HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
			m.max_load_factor = max_load_factor;
		
			for(int i=0; i<N; ++i)
			{
				const int x = sparse_testdata[i];
				m.insert(std::make_pair(x, x));
			}
			double elapsed = timer.elapsed();
			//conPrint("HashMapInsertOnly2 (insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));
			conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()) + ", expns: " + toString(m.num_expansions) + ", bkts: " + toString(m.buckets.size()));
			datasets[0].points.push_back(Vec2f(max_load_factor, elapsed));

			// Test lookup performance
			/*timer.reset();
			int num_present = 0;
			for(int i=0; i<NUM_LOOKUPS; ++i)
			{
				const int x = sparse_testdata[i];
				HashMapInsertOnly2<int, int>::const_iterator it = m.find(x);
				if(it != m.end())
					num_present++;
			}
			elapsed = timer.elapsed();*/
			//conPrint("HashMapInsertOnly2 lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
			//conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
		}

		// Measure insertion perf With reserve
		for(float max_load_factor = 0.2f; max_load_factor < 0.99f; max_load_factor += 0.01f)
		{
			Timer timer;
			HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max(), N, max_load_factor);
		
			for(int i=0; i<N; ++i)
			{
				const int x = sparse_testdata[i];
				m.insert(std::make_pair(x, x));
			}
			double elapsed = timer.elapsed();
			conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()) + ", expns: " + toString(m.num_expansions) + ", bkts: " + toString(m.buckets.size()));
			datasets[1].points.push_back(Vec2f(max_load_factor, elapsed));
		}

		// Measure lookup perf (should be same regardless of if reserve was used)
		for(float max_load_factor = 0.2f; max_load_factor < 0.99f; max_load_factor += 0.01f)
		{
			Timer timer;
			HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
			m.max_load_factor = max_load_factor;
		
			for(int i=0; i<N; ++i)
			{
				const int x = sparse_testdata[i];
				m.insert(std::make_pair(x, x));
			}

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
			double elapsed = timer.elapsed();
			//conPrint("HashMapInsertOnly2 lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
			conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present, expns: " + toString(m.num_expansions));
			datasets[2].points.push_back(Vec2f(max_load_factor, elapsed));
		}


		Plotter::plot("HashMapInsertOnly2_perf.png", "HashMapInsertOnly2 perf for various max load factors", "max load factor", "elapsed (s)", datasets);
	}
#endif


#if TEST_OTHER_HASH_MAPS
	
	{
		Timer timer;
		std::unordered_map<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("std::unordered_map insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

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
		elapsed = timer.elapsed();
		conPrint("std::unordered_map lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	
	{
		Timer timer;
		google::dense_hash_map<int, int> m;
		m.set_empty_key(std::numeric_limits<int>::max());
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("google::dense_hash_map insert took  " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

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
		elapsed = timer.elapsed();
		conPrint("google::dense_hash_map lookups took " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	
	{
		Timer timer;
		spp::sparse_hash_map<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = sparse_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("spp::sparse_hash_map insert took    " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = sparse_testdata[i];
			spp::sparse_hash_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}
		elapsed = timer.elapsed();
		conPrint("spp::sparse_hash_map lookups took   " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
#endif // TEST_OTHER_HASH_MAPS



	// Test insertion performance
	conPrint("");
	conPrint("\nperf for mostly duplicate keys");
	conPrint("-------------------------------------");
	{
		Timer timer;
		HashMapInsertOnly<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly insert took       " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));
		
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
		elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly lookups took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

	{
		Timer timer;
		HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly2 insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));
		
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
		elapsed = timer.elapsed();
		conPrint("HashMapInsertOnly2 lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

#if 0
	if(false)
	{
		std::vector<Plotter::DataSet> dense_datasets;
		dense_datasets.push_back(Plotter::DataSet("insertion time"));
		dense_datasets.push_back(Plotter::DataSet("insertion time with reserve"));
		dense_datasets.push_back(Plotter::DataSet("lookup time"));

		for(float max_load_factor = 0.2f; max_load_factor < 0.99f; max_load_factor += 0.01f)
		{
			Timer timer;
			HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
			m.max_load_factor = max_load_factor;
		
			for(int i=0; i<N; ++i)
			{
				const int x = dense_testdata[i];
				m.insert(std::make_pair(x, x));
			}
			double elapsed = timer.elapsed();
			//conPrint("HashMapInsertOnly2 (insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));
			conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()) + ", expns: " + toString(m.num_expansions) + ", bkts: " + toString(m.buckets.size()));
			dense_datasets[0].points.push_back(Vec2f(max_load_factor, elapsed));

			// Test lookup performance
			/*timer.reset();
			int num_present = 0;
			for(int i=0; i<NUM_LOOKUPS; ++i)
			{
				const int x = sparse_testdata[i];
				HashMapInsertOnly2<int, int>::const_iterator it = m.find(x);
				if(it != m.end())
					num_present++;
			}
			elapsed = timer.elapsed();*/
			//conPrint("HashMapInsertOnly2 lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
			//conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
		}

		// Measure insertion perf With reserve
		for(float max_load_factor = 0.2f; max_load_factor < 0.99f; max_load_factor += 0.01f)
		{
			Timer timer;
			HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max(), N, max_load_factor);
		
			for(int i=0; i<N; ++i)
			{
				const int x = dense_testdata[i];
				m.insert(std::make_pair(x, x));
			}
			double elapsed = timer.elapsed();
			conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()) + ", expns: " + toString(m.num_expansions) + ", bkts: " + toString(m.buckets.size()));
			dense_datasets[1].points.push_back(Vec2f(max_load_factor, elapsed));
		}

		// Measure lookup perf (should be same regardless of if reserve was used)
		for(float max_load_factor = 0.2f; max_load_factor < 0.99f; max_load_factor += 0.01f)
		{
			Timer timer;
			HashMapInsertOnly2<int, int> m(std::numeric_limits<int>::max());
			m.max_load_factor = max_load_factor;
		
			for(int i=0; i<N; ++i)
			{
				const int x = dense_testdata[i];
				m.insert(std::make_pair(x, x));
			}

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
			double elapsed = timer.elapsed();
			//conPrint("HashMapInsertOnly2 lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
			conPrint("HashMapInsertOnly2 (max_load_factor " + doubleToStringNSigFigs(max_load_factor, 4) + ") lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present, expns: " + toString(m.num_expansions));
			dense_datasets[2].points.push_back(Vec2f(max_load_factor, elapsed));
		}


		Plotter::plot("HashMapInsertOnly2_dense_perf.png", "HashMapInsertOnly2 perf for various max load factors, dense keys", "max load factor", "elapsed (s)", dense_datasets);
	}
#endif

#if TEST_OTHER_HASH_MAPS
	{
		Timer timer;
		std::unordered_map<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("std::unordered_map insert took      " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

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
		conPrint("std::unordered_map lookups took     " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}

	{
		Timer timer;
		google::dense_hash_map<int, int> m;
		m.set_empty_key(std::numeric_limits<int>::max());
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("google::dense_hash_map insert took  " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

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
		elapsed = timer.elapsed();
		conPrint("google::dense_hash_map lookups took " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
	
	{
		Timer timer;
		spp::sparse_hash_map<int, int> m;
		for(int i=0; i<N; ++i)
		{
			const int x = dense_testdata[i];
			m.insert(std::make_pair(x, x));
		}
		double elapsed = timer.elapsed();
		conPrint("spp::sparse_hash_map insert took    " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s, final size: " + toString(m.size()));

		// Test lookup performance
		timer.reset();
		int num_present = 0;
		for(int i=0; i<NUM_LOOKUPS; ++i)
		{
			const int x = dense_testdata[i];
			spp::sparse_hash_map<int, int>::const_iterator it = m.find(x);
			if(it != m.end())
				num_present++;
		}
		elapsed = timer.elapsed();
		conPrint("spp::sparse_hash_map lookups took   " + ::doubleToStringNDecimalPlaces(elapsed, 6) + " s for " + toString(NUM_LOOKUPS) + " lookups, " + toString(num_present) + " present.");
	}
#endif // TEST_OTHER_HASH_MAPS
}


#endif
