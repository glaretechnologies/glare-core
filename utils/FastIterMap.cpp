/*=====================================================================
FastIterMap.cpp
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "FastIterMap.h"


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "RefCounted.h"
#include "Reference.h"
#include "../maths/PCG32.h"
#include <set>


class FastIterMapTestClass : public RefCounted
{
public:
};


class FastIterMapTestLargeClass
{
public:
	char data[1056];
};


void glare::testFastIterMap()
{
	conPrint("glare::testFastIterMap()");

	{
		FastIterMap<int, int, std::hash<int>> map;

		testAssert(map.insert(10, 20));

		auto res = map.find(10);
		testAssert(res != map.end());
		testAssert(res.getValue() == 20);


		testAssert(map.find(123) == map.end()); // Test finding a key not in map.


		testAssert(map.insert(100, 200));
		res = map.find(100);
		testAssert(res != map.end());
		testAssert(res.getValue() == 200);

		// Test inserting the same key again returns false.
		testAssert(!map.insert(10, 20));
		testAssert(!map.insert(10, 21));

		// Test iteration over values gives expected results
		{
			std::set<int> seen_items;
			for(auto it = map.valuesBegin(); it != map.valuesEnd(); ++it)
			{
				const int val = it.getValue();
				testAssert(seen_items.count(val) == 0); // Check we haven't seen this object already.
				seen_items.insert(val);
			}
			std::set<int> expected_items;
			expected_items.insert(20);
			expected_items.insert(200);
			testAssert(seen_items == expected_items);
		}

		// Test erasing an element
		map.erase(10);

		{
			std::set<int> seen_items;
			for(auto it = map.valuesBegin(); it != map.valuesEnd(); ++it)
			{
				const int val = it.getValue();
				testAssert(seen_items.count(val) == 0); // Check we haven't seen this object already.
				seen_items.insert(val);
			}
			std::set<int> expected_items;
			expected_items.insert(200);
			testAssert(seen_items == expected_items);
		}

		testAssert(map.find(10) == map.end()); // Test finding key we just removed results in end()
	}

	{
		FastIterMap<int, int, std::hash<int>> map;
		for(int i=0; i<10000; ++i)
			map.insert(i, i);

		{
			std::set<int> seen_items;
			for(auto it = map.valuesBegin(); it != map.valuesEnd(); ++it)
			{
				const int val = it.getValue();
				testAssert(seen_items.count(val) == 0); // Check we haven't seen this object already.
				seen_items.insert(val);
			}
			std::set<int> expected_items;
			for(int i=0; i<10000; ++i)
				expected_items.insert(i);
			testAssert(seen_items == expected_items);
		}

		for(int i=0; i<10000; ++i)
			map.erase(i);

		{
			std::set<int> seen_items;
			for(auto it = map.valuesBegin(); it != map.valuesEnd(); ++it)
			{
				const int val = it.getValue();
				testAssert(seen_items.count(val) == 0); // Check we haven't seen this object already.
				seen_items.insert(val);
			}
			std::set<int> expected_items;
			testAssert(seen_items == expected_items);
		}
	}

	// Try erasing objects in reverse order from addition
	{
		FastIterMap<int, int, std::hash<int>> map;
		for(int i=0; i<10000; ++i)
			map.insert(i, i);

		{
			std::set<int> seen_items;
			for(auto it = map.valuesBegin(); it != map.valuesEnd(); ++it)
			{
				const int val = it.getValue();
				testAssert(seen_items.count(val) == 0); // Check we haven't seen this object already.
				seen_items.insert(val);
			}
			std::set<int> expected_items;
			for(int i=0; i<10000; ++i)
				expected_items.insert(i);
			testAssert(seen_items == expected_items);
		}

		for(int i=10000 - 1; i >= 0; --i)
			map.erase(i);

		{
			std::set<int> seen_items;
			for(auto it = map.valuesBegin(); it != map.valuesEnd(); ++it)
			{
				const int val = it.getValue();
				testAssert(seen_items.count(val) == 0); // Check we haven't seen this object already.
				seen_items.insert(val);
			}
			std::set<int> expected_items;
			testAssert(seen_items == expected_items);
		}
	}



	// Test with a Reference class value
	{
		Reference<FastIterMapTestClass> ref = new FastIterMapTestClass();

		FastIterMap<int, Reference<FastIterMapTestClass>, std::hash<int>> map;
		for(int i=0; i<10; ++i)
			map.insert(i, ref);

		testAssert(ref->getRefCount() >= 10); // == 1 + 10 + 1); // Will be 2 copies of each ref per insertion

		for(int i=0; i<10; ++i)
			map.erase(i);

		testAssert(ref->getRefCount() == 1);
	}

	// Test FastIterMap destructor destroys values
	{
		Reference<FastIterMapTestClass> ref = new FastIterMapTestClass();
		{
			FastIterMap<int, Reference<FastIterMapTestClass>, std::hash<int>> map;
			for(int i=0; i<10; ++i)
				map.insert(i, ref);

			testAssert(ref->getRefCount() >= 10); // testAssert(ref->getRefCount() == 21); // Will be 2 copies of each ref per insertion
		}
		testAssert(ref->getRefCount() == 1);
	}



	conPrint("glare::testFastIterMap() done.");
}


#endif // BUILD_TESTS
