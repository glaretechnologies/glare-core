/*=====================================================================
HashSet.cpp
-----------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "HashSet.h"


#if BUILD_TESTS


#include "RefCounted.h"
#include "Reference.h"
#include "ConPrint.h"
#include "TestUtils.h"
#include "Timer.h"
#include "StringUtils.h"
#include "../maths/PCG32.h"


template <class K, class H>
static void printSet(HashSet<K, H>& m)
{
	conPrint("-----------------------------------------");
	for(size_t i=0; i<m.buckets_size; ++i)
		conPrint("bucket " + toString(i) + ": " + toString(m.buckets[i]));
}


class HashSetTestClass : public RefCounted
{
public:
};


struct HashSetTestClassRefHashFunc
{
	size_t operator() (const Reference<HashSetTestClass>& ref) const
	{
		std::hash<HashSetTestClass*> hasher;
		return hasher(ref.ptr());
	}
};


struct TestIdentityHashFunc
{
	size_t operator() (const int& x) const
	{
		return (size_t)x;
	}
};


void testHashSet()
{
	conPrint("testHashSet()");

	// Some tricky patterns for hash tables that use deleted-key markers:
	// For each bucket i, find an item that hashes to bucket i, add it, then remove it.
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		for(int b=0; b<32; ++b)
		{
			for(int x=0; ; ++x)
			{
				std::hash<int> hasher;
				const int bucket = hasher(x) % 32;
				if(bucket == b)
				{
					m.insert(x);
					m.erase(x);
					break;
				}
			}
		}
		m.find(0); // infinite loop with naive implementation
	}

	// For each bucket i, find an item that hashes to bucket 0, add it, then remove it.
	{
		HashSet<int, TestIdentityHashFunc> m(/*empty key=*/std::numeric_limits<int>::max());

		m.insert(31);
		for(int b=30; b>=0; --b)
		{
			m.insert(b);
			m.erase(b);
		}

		m.erase(31);
		m.find(0); // infinite loop with naive implementation
	}


	// Test with storing references, so we can check we create and destroy all elements properly.
	{
		Reference<HashSetTestClass> test_ob = new HashSetTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			Reference<HashSetTestClass> empty_key;
			HashSet<Reference<HashSetTestClass>, HashSetTestClassRefHashFunc> m(/*empty key=*/empty_key);

			m.insert(test_ob);
			testAssert(m.find(test_ob) != m.end());
			testAssert(m.find(test_ob)->getPointer() == test_ob.getPointer());

			// Add lots of items, to force an expand.
			for(int i=0; i<100; ++i)
				m.insert(test_ob);

			for(int i=0; i<100; ++i)
			{
				testAssert(m.find(test_ob) != m.end());
				testAssert(m.find(test_ob)->getPointer() == test_ob.getPointer());
			}

		}

		testAssert(test_ob->getRefCount() == 1);
	}

	// Test erase()
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		m.insert(1);
		m.insert(2);
		m.insert(3);

		testAssert(m.size() == 3);
		//printSet(m);

		testAssert(m.find(2) != m.end());
		m.erase(2);
		testAssert(m.find(2) == m.end());
		testAssert(m.size() == 2);

		//printSet(m);

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
		Reference<HashSetTestClass> test_ob = new HashSetTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			HashSet<Reference<HashSetTestClass>, HashSetTestClassRefHashFunc> m(/*empty key=*/NULL);
			
			m.insert(test_ob);
			testAssert(test_ob->getRefCount() == 2);
			m.erase(test_ob);
			testAssert(test_ob->getRefCount() == 1);
		}
	}

	// Insert and erase a lot of elements a few times
	{
		
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		for(int z=0; z<10; ++z)
		{
			for(int i=0; i<100; ++i)
				m.insert(i);

			testAssert(m.size() == 100);

			for(int i=0; i<100; ++i)
				testAssert(m.find(i) != m.end());

			for(int i=0; i<100; ++i)
				m.erase(i);

			testAssert(m.size() == 0);
			//printSet(m);

			for(int i=0; i<100; ++i)
				testAssert(m.find(i) == m.end());
		}
	}

	// Test inserting the same key.  Should not change the set.
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		std::pair<HashSet<int>::iterator, bool> res = m.insert(1);
		testAssert(res.second);
		testAssert(m.find(1) != m.end());
		testAssert(*m.find(1) == 1);

		// Insert the same key.  Should not change the set.
		res = m.insert(1);
		testAssert(!res.second);
		testAssert(m.find(1) != m.end());
		testAssert(*m.find(1) == 1);
	}

	// Test constructor with num elems expected
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max(), /*expected num elems=*/1000);
		testAssert(m.empty());

		m.insert(1);
		testAssert(m.find(1) != m.end());
		testAssert(*m.find(1) == 1);
	}

	// Test clear()
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());
		m.insert(1);
		m.clear();
		testAssert(m.find(1) == m.end());
		testAssert(m.empty());
	}

	// Test clear() with references - make sure clear() removes the reference value.
	{
		Reference<HashSetTestClass> test_ob = new HashSetTestClass();
		testAssert(test_ob->getRefCount() == 1);

		{
			HashSet<Reference<HashSetTestClass>, HashSetTestClassRefHashFunc> m(/*empty key=*/NULL);
			m.insert(test_ob);
			testAssert(test_ob->getRefCount() == 2);
			m.clear();
			testAssert(test_ob->getRefCount() == 1);
		}
		testAssert(test_ob->getRefCount() == 1);
	}

	// Test iterators
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());
		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() == m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));

		m.insert(1);

		testAssert(*m.begin() == 1);

		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() != m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));
	}

	// Test const iterators
	{
		HashSet<int> non_const_m(/*empty key=*/std::numeric_limits<int>::max());
		const HashSet<int>& m = non_const_m;
		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() == m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));

		non_const_m.insert(1);

		testAssert(*m.begin() == 1);

		testAssert(m.begin() == m.begin());
		testAssert(!(m.begin() != m.begin()));
		testAssert(m.begin() != m.end());
		testAssert(m.end() == m.end());
		testAssert(!(m.end() != m.end()));
	}

	// Test wrap-around when looking for empty buckets
	{
		HashSet<int, TestIdentityHashFunc> m(/*empty key=*/std::numeric_limits<int>::max());
		m.insert(3);
		m.insert(7);
	}

	{
		// Insert lots of values
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		const int N = 10000;
		for(int i=0; i<N; ++i)
		{
			std::pair<HashSet<int>::iterator, bool> res = m.insert(i);
			testAssert(*(res.first) == i);
			testAssert(res.second);
		}

		testAssert(m.size() == (size_t)N);

		// Check that the keys are present.
		for(int i=0; i<N; ++i)
		{
			testAssert(m.find(i) != m.end());
			testAssert(*m.find(i) == i);
		}
	}

	// Test iteration with sparse values.
	{
		// Insert lots of values
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		const int N = 10;
		for(int i=0; i<N; ++i)
		{
			m.insert(i);
		}

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashSet<int>::iterator i=m.begin(); i != m.end(); ++i)
			{
				const int key = *i;
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
			for(HashSet<int>::const_iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[*i]);
				seen[*i] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}
	}

	// Test iteration with dense values.
	{
		// Insert lots of values
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());

		const int N = 10000;
		for(int i=0; i<N; ++i)
			m.insert(i);

		// Check iterator
		{
			std::vector<bool> seen(N, false);
			for(HashSet<int>::iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[*i]);
				seen[*i] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}

		// Check const_iterator
		{
			std::vector<bool> seen(N, false);
			for(HashSet<int>::const_iterator i=m.begin(); i != m.end(); ++i)
			{
				testAssert(!seen[*i]);
				seen[*i] = true;
			}

			// Check we have seen all items.
			for(int i=0; i<N; ++i)
				testAssert(seen[i]);
		}
	}


	// Do a stress test with randomly inserted integer keys
	{
		HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());
		const int N = 100000;
		std::vector<bool> ref_inserted(1000, false);
		PCG32 rng(1);
		for(int i=0; i<N; ++i)
		{
			const int x = (int)(rng.unitRandom() * 999.99f);
			assert(x >= 0 && x < 1000);

			std::pair<HashSet<int>::iterator, bool> res = m.insert(x);

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
			HashSet<int> m(/*empty key=*/std::numeric_limits<int>::max());
			const int N = 1 << 16;
			std::vector<bool> ref_inserted(1000, false);

			for(int i=0; i<N; ++i)
			{
				const int x = (int)(rng.unitRandom() * 999.99f);
				assert(x >= 0 && x < 1000);

				if(rng.unitRandom() < 0.5f)
				{
					std::pair<HashSet<int>::iterator, bool> res = m.insert(x);
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
							const int val = *it;
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

	conPrint("testHashSet() done.");
}


#endif // BUILD_TESTS
