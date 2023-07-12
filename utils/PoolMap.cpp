/*=====================================================================
PoolMap.cpp
-----------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "PoolMap.h"


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "FastIterMap.h"
#include "../maths/PCG32.h"
#include <set>
#include <map>
#include <unordered_set>


class PoolMapTestClass
{
public:
	PoolMapTestClass() : num_alive_ptr(NULL) {}
	~PoolMapTestClass()
	{
		if(num_alive_ptr)
			(*num_alive_ptr)--;
	}

	int* num_alive_ptr;
};


class PoolMapTestLargeClass
{
public:
	PoolMapTestLargeClass() {}
	~PoolMapTestLargeClass() {}

	char data[1056];
};


void glare::testPoolMap()
{
	conPrint("glare::testPoolMap()");

	{
		PoolMap<int, int, std::hash<int>> map;

		int* value_ptr = map.allocateWithKey(10);

		*value_ptr = 20;

		PoolMap<int, int, std::hash<int>>::iterator res = map.find(10);
		if(res != map.end())
		{
			int* looked_up_val_ptr = res.getValuePtr();
			testAssert(looked_up_val_ptr == value_ptr);
			testAssert(*looked_up_val_ptr == 20);
		}

		testAssert(map.find(123) == map.end()); // Test finding a key not in map.


		int* value_ptr2 = map.allocateWithKey(100);
		*value_ptr2 = 200;

		res = map.find(100);
		if(res != map.end())
		{
			int* looked_up_val_ptr = res.getValuePtr();
			testAssert(looked_up_val_ptr == value_ptr2);
			testAssert(*looked_up_val_ptr == 200);
		}

		{
			std::set<int> seen_items;
			for(auto it = map.begin(); it != map.end(); ++it)
			{
				testAssert(seen_items.count(*it.getValuePtr()) == 0); // Check we haven't seen this object already.
				seen_items.insert(*it.getValuePtr());
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
			for(auto it = map.begin(); it != map.end(); ++it)
			{
				testAssert(seen_items.count(*it.getValuePtr()) == 0); // Check we haven't seen this object already.
				seen_items.insert(*it.getValuePtr());
			}
			std::set<int> expected_items;
			expected_items.insert(200);
			testAssert(seen_items == expected_items);
		}

		testAssert(map.find(10) == map.end()); // Test finding key we just removed results in end()
	}

	{
		PoolMap<int, int, std::hash<int>> map;
		for(int i=0; i<10000; ++i)
		{
			int* value_ptr = map.allocateWithKey(i);
			*value_ptr = i;
		}

		{
			std::set<int> seen_items;
			for(auto it = map.begin(); it != map.end(); ++it)
			{
				testAssert(seen_items.count(*it.getValuePtr()) == 0); // Check we haven't seen this object already.
				seen_items.insert(*it.getValuePtr());
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
			for(auto it = map.begin(); it != map.end(); ++it)
			{
				testAssert(seen_items.count(*it.getValuePtr()) == 0); // Check we haven't seen this object already.
				seen_items.insert(*it.getValuePtr());
			}
			std::set<int> expected_items;
			testAssert(seen_items == expected_items);
		}
	}

	// Try erasing objects in reverse order from addition
	{
		PoolMap<int, int, std::hash<int>> map;
		for(int i=0; i<10000; ++i)
		{
			int* value_ptr = map.allocateWithKey(i);
			*value_ptr = i;
		}

		{
			std::set<int> seen_items;
			for(auto it = map.begin(); it != map.end(); ++it)
			{
				testAssert(seen_items.count(*it.getValuePtr()) == 0); // Check we haven't seen this object already.
				seen_items.insert(*it.getValuePtr());
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
			for(auto it = map.begin(); it != map.end(); ++it)
			{
				testAssert(seen_items.count(*it.getValuePtr()) == 0); // Check we haven't seen this object already.
				seen_items.insert(*it.getValuePtr());
			}
			std::set<int> expected_items;
			testAssert(seen_items == expected_items);
		}
	}



	// Test with a class value
	{
		PoolMap<int, PoolMapTestClass, std::hash<int>> map;
		int num_alive = 0;
		for(int i=0; i<10; ++i)
		{
			PoolMapTestClass* value_ptr = map.allocateWithKey(i);
			value_ptr->num_alive_ptr = &num_alive;
			num_alive++;
		}

		testAssert(num_alive == 10);

		for(int i=0; i<10; ++i)
		{
			map.erase(i);
		}

		testAssert(num_alive == 0);
	}

	// Test PoolMap destructor destroys values
	{
		int num_alive = 0;
		{
			PoolMap<int, PoolMapTestClass, std::hash<int>> map;

			for(int i=0; i<10; ++i)
			{
				PoolMapTestClass* value_ptr = map.allocateWithKey(i);
				value_ptr->num_alive_ptr = &num_alive;
				num_alive++;
			}

			testAssert(num_alive == 10);
		}
		testAssert(num_alive == 0);
	}



	// Test with references 
	{
		/*Reference<PoolAllocator<PoolAllocatorTestStruct>> pool = new PoolAllocator<PoolAllocatorTestStruct>(16);
		
		testAssert(pool->numAllocatedObs() == 0);

		{
			void* mem = pool->alloc(sizeof(PoolAllocatorTestStruct), 16);
			Reference<PoolAllocatorTestStruct> a = ::new (mem) PoolAllocatorTestStruct();
			a->allocator = pool;

			testAssert(pool->numAllocatedObs() == 1);
		}
		testAssert(pool->numAllocatedObs() == 0);

		testAssert(pool->getRefCount() == 1);*/
	}


	//============================================= Random stress/fuzz test. =============================================
	{
		PCG32 rng(1);
		for(int q=0; q<1; ++q)
		{
			PoolMap<int, int, std::hash<uint64>> map;
			std::unordered_set<int> keys;
			
			for(int i = 0; i<10000; ++i)
			{
				if(i % (1 << 16) == 0)
					conPrint("iter: " + toString(i) + ": allocated keys size: " + toString(keys.size()));
				const float ur = rng.unitRandom();
				if(ur < 0.001f)
				{
					// Iterate over all items
					for(auto it = map.begin(); it != map.end(); ++it)
					{
						const int val = *it.getValuePtr();
						testAssert(keys.count(val) == 1);
					}
				}
				else if(ur < 0.55f)
				{
					const int key = rng.nextUInt(100000);
					try
					{
						int* valueptr = map.allocateWithKey(key);
						*valueptr = key;

						keys.insert(key);
					}
					catch(glare::Exception&)
					{
						// key was already inserted
					}
				}
				else
				{
					if(keys.size() > 0)
					{
						// Select a random block in our allocated set to free
						const size_t index = rng.nextUInt((uint32)keys.size());

						size_t z = 0;
						for(auto it = keys.begin(); it != keys.end(); ++it)
						{
							if(z == index)
							{
								const int key = *it;
								map.erase(key);
								keys.erase(it);
								break;
							}
							z++;
						}
					}
				}
			}
		}

		// Free all allocated blocks
		//for(auto it = blocks.begin(); it != blocks.end(); ++it)
		//	allocator->free(*it);
	}






	//============================================= Do some perf tests. =============================================
	const int num_objs = 730000;
	const int num_trials = 40;

	std::vector<PoolMapTestLargeClass*> test_obs(num_objs);
	for(int i=0; i<num_objs; ++i)
	{
		test_obs[i] = new PoolMapTestLargeClass();
		test_obs[i]->data[0] = 0;
	}


	// Test speed of iteration with PoolMap
	/*{
		PoolMap<uint64, PoolMapTestLargeClass, std::hash<uint64>> map;

		for(int i=0; i<num_objs; ++i)
		{
			PoolMapTestLargeClass* value_ptr = map.allocateWithKey(i);
			value_ptr->data[0] = 0;
		}
		
		double least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = map.begin(); it != map.end(); ++it)
				sum++;
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		double per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("PoolMap: Iteration over " + toString(num_objs) + " objects took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");

		least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = map.begin(); it != map.end(); ++it)
				sum += (uint64)it.getValuePtr()->data[0];
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("PoolMap: Iteration over " + toString(num_objs) + " objects with ob access took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");
			
	}*/

	// Test speed of iteration with FastIterMap
#if 0
	{
		FastIterMap<uint64, PoolMapTestLargeClass*, std::hash<uint64>> map(std::numeric_limits<uint64>::max());

		
		for(int i=0; i<num_objs; ++i)
		{
			//PoolMapTestLargeClass* value_ptr = new PoolMapTestLargeClass();
			//PoolMapTestLargeClass* value_ptr = map.allocateWithKey(i);
			//value_ptr->data[0] = 0;
			map.insert(i, test_obs[i]);
		}

		double least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			std::set<PoolMapTestLargeClass*>& values = map.values;
			for(auto it = values.begin(); it != values.end(); ++it)
				sum++;
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		double per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("PoolMap2: Iteration over " + toString(num_objs) + " objects took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");

		//uint64 last_address = 0;

		least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			std::set<PoolMapTestLargeClass*>& values = map.values;
			for(auto it = values.begin(); it != values.end(); ++it)
			{
				//const int64 address_diff = (int64)(it.getValue()) - (int64)last_address;
				//printVar(address_diff);
				//sum += (uint64)it.getValue()->data[0];
				sum += (uint64)(*it)->data[0];

				//last_address = (int64)(it.getValue());
			}
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("PoolMap2: Iteration over " + toString(num_objs) + " objects with ob access took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");

	}
#endif
/*

	// Test speed of iteration with std::unordered_map
	{
		std::unordered_map<uint64, PoolMapTestLargeClass, std::hash<uint64>> map;

		for(int i=0; i<num_objs; ++i)
		{
			PoolMapTestLargeClass value;
			value.data[0] = 0;
			map.insert(std::make_pair(i, value));
		}

		double least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = map.begin(); it != map.end(); ++it)
				sum++;
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		double per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("std::unordered_map: Iteration over " + toString(num_objs) + " objects took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");

		least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = map.begin(); it != map.end(); ++it)
				sum += (uint64)it->second.data[0];
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("std::unordered_map: Iteration over " + toString(num_objs) + " objects with ob access took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");
	}

	// Test speed of iteration with std::map
	{
		std::map<uint64, PoolMapTestLargeClass> map;

		for(int i=0; i<num_objs; ++i)
		{
			PoolMapTestLargeClass value;
			value.data[0] = 0;
			map.insert(std::make_pair(i, value));
		}

		double least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = map.begin(); it != map.end(); ++it)
				sum++;
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		double per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("std::map: Iteration over " + toString(num_objs) + " objects took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");

		least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = map.begin(); it != map.end(); ++it)
				sum += (uint64)it->second.data[0];
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("std::map: Iteration over " + toString(num_objs) + " objects with ob access took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");
	}
*/
	// Test speed of iteration with std::map
	{
		std::set<PoolMapTestLargeClass*> set;

		for(int i=0; i<num_objs; ++i)
		{
			//PoolMapTestLargeClass* value = new PoolMapTestLargeClass();
			//value->data[0] = 0;
			set.insert(test_obs[i]);
		}

		double least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = set.begin(); it != set.end(); ++it)
				sum++;
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		double per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("std::set<Value*>: Iteration over " + toString(num_objs) + " objects took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");

		least_time = 1.0e10;
		for(int trial = 0; trial < num_trials; ++trial)
		{
			Timer timer;
			uint64 sum = 0;
			for(auto it = set.begin(); it != set.end(); ++it)
				sum += (uint64)(*it)->data[0];
			const double elapsed = timer.elapsed();
			least_time = myMin(least_time, elapsed);
			if(sum == 234534)
				conPrint("boo");
		}

		per_ob_iter_time_ns = least_time / num_objs * 1.0e9;
		conPrint("std::set<Value*>: Iteration over " + toString(num_objs) + " objects with ob access took " + doubleToStringNSigFigs(least_time, 4) + " s (" + doubleToStringNSigFigs(per_ob_iter_time_ns, 4) + " ns per object)");
	}

	conPrint("glare::testPoolMap() done.");
}


#endif // BUILD_TESTS
