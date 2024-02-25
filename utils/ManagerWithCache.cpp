/*=====================================================================
ManagerWithCache.cpp
--------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "ManagerWithCache.h"


#if BUILD_TESTS


#include "Reference.h"
#include "ConPrint.h"
#include "TestUtils.h"
#include "Timer.h"
#include "StringUtils.h"


struct ManagerTestItem
{
	ManagerTestItem(int key_, ManagerWithCache<int, Reference<ManagerTestItem> >* manager_)
	:	key(key_), manager(manager_), refcount(0)
	{}

	/// Increment reference count
	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		refcount++;
	}

	/// Returns previous reference count
	inline int64 decRefCount() const
	{ 
		const int64 prev_ref_count = refcount;
		refcount--;
		assert(refcount >= 0);

		if(refcount == 1)
		{
			itemBecameUnused();
		}

		return prev_ref_count;
	}

	inline int64 getRefCount() const
	{ 
		assert(refcount >= 0);
		return refcount; 
	}

	void itemBecameUnused() const
	{
		manager->itemBecameUnused(key);
	}

	int key;
	ManagerWithCache<int, Reference<ManagerTestItem> >* manager;
	mutable int64 refcount;
};


void testManagerWithCache()
{
	conPrint("testManagerWithCache()");

	// Test insert, itemBecameUsed, itemBecameUnused, removeLRUUnusedItem
	{
		ManagerWithCache<std::string, int> manager;
		manager.insert("a", 1);
		manager.itemBecameUsed("a");

		manager.insert("b", 2);
		manager.itemBecameUsed("b");

		testAssert(manager.numItems() == 2);
		testAssert(manager.numUsedItems() == 2);

		manager.itemBecameUnused("a");
		manager.itemBecameUnused("b");

		testAssert(manager.numItems() == 2);
		testAssert(manager.numUsedItems() == 0);

		std::string removed_key;
		int removed_val;
		bool removed = manager.removeLRUUnusedItem(removed_key, removed_val);
		testAssert(removed);
		testAssert(removed_key == "a");
		testEqual(removed_val, 1);
		testAssert(manager.numItems() == 1);
		testAssert(manager.numUsedItems() == 0);

		removed = manager.removeLRUUnusedItem(removed_key, removed_val);
		testAssert(removed);
		testAssert(removed_key == "b");
		testEqual(removed_val, 2);
		testAssert(manager.numItems() == 0);
		testAssert(manager.numUsedItems() == 0);

		removed = manager.removeLRUUnusedItem(removed_key, removed_val);
		testAssert(!removed);
		testAssert(manager.numItems() == 0);
		testAssert(manager.numUsedItems() == 0);

		manager.insert("a", 1);
		manager.insert("b", 2);
		manager.insert("c", 3);

		manager.itemBecameUnused("a"); // unused: [a]
		manager.itemBecameUnused("b"); // unused: [b, a]
		manager.itemBecameUnused("c"); // unused: [c, b, a]
		manager.itemBecameUsed("a"); // unused: [c, b]

		testAssert(manager.removeLRUUnusedItem(removed_key, removed_val)); // unused: [c]
		testAssert(removed_key == "b");
		testEqual(removed_val, 2);
		testAssert(manager.numItems() == 2);
		testAssert(manager.numUsedItems() == 1);

		testAssert(manager.removeLRUUnusedItem(removed_key, removed_val)); // unused: []
		testAssert(removed_key == "c");
		testEqual(removed_val, 3);
		testAssert(manager.numItems() == 1);
		testAssert(manager.numUsedItems() == 1);

		// Test duplicate itemBecameUsed().
		manager.itemBecameUsed("a");
	}

	// Test find and erase
	{
		ManagerWithCache<std::string, int> manager;
		manager.insert("a", 1);
		manager.insert("b", 2);

		manager.itemBecameUnused("a");
		manager.itemBecameUnused("b");

		testAssert(manager.numItems() == 2);
		testAssert(manager.numUnusedItems() == 2);

		testAssert(manager.find("z") == manager.end());
		auto res = manager.find("a");
		testAssert(res != manager.end());

		manager.erase(res);
		testAssert(manager.find("a") == manager.end());

		testAssert(manager.numItems() == 1);
		testAssert(manager.numUnusedItems() == 1);
	}

	// Test isItemUsed
	{
		ManagerWithCache<std::string, int> manager;
		manager.insert("a", 1);
		manager.insert("b", 2);

		testAssert(manager.isItemUsed(manager.find("a")->second));
		testAssert(manager.isItemUsed(manager.find("b")->second));

		manager.itemBecameUnused("a");
		manager.itemBecameUnused("b");

		testAssert(!manager.isItemUsed(manager.find("a")->second));
		testAssert(!manager.isItemUsed(manager.find("b")->second));

		manager.itemBecameUsed("b");
		testAssert(!manager.isItemUsed(manager.find("a")->second));
		testAssert( manager.isItemUsed(manager.find("b")->second));
	}

	// Test duplicate insert (second insert has no effect, like std::map)
	{
		ManagerWithCache<std::string, int> manager;
		manager.insert("a", 1);
		testAssert(manager.numItems() == 1);
		testAssert(manager.find("a")->second.value == 1);
		manager.insert("a", 2);
		testAssert(manager.numItems() == 1);
		testAssert(manager.find("a")->second.value == 1);
	}

	// Test set()
	{
		ManagerWithCache<std::string, int> manager;
		manager.set("a", 1);
		testAssert(manager.numItems() == 1);
		testAssert(manager.find("a")->second.value == 1);
		manager.set("a", 2);
		testAssert(manager.numItems() == 1);
		testAssert(manager.find("a")->second.value == 2);
		manager.set("b", 3);
		testAssert(manager.numItems() == 2);
		testAssert(manager.find("a")->second.value == 2);
		testAssert(manager.find("b")->second.value == 3);
	}



	// Test calling itemBecameUnused with the same key multiple times.
	{
		ManagerWithCache<std::string, int> manager;
		manager.insert("a", 1);

		manager.itemBecameUnused("a");

		testAssert(manager.numItems() == 1);
		testAssert(manager.numUnusedItems() == 1);

		manager.itemBecameUnused("a");

		testAssert(manager.numItems() == 1);
		testAssert(manager.numUnusedItems() == 1);
	}

	// Test calling itemBecameUsed with the same key multiple times.
	{
		ManagerWithCache<std::string, int> manager;
		manager.insert("a", 1);
		manager.itemBecameUnused("a");
		testAssert(manager.numUnusedItems() == 1);
		manager.itemBecameUsed("a");
		testAssert(manager.numUnusedItems() == 0);
		manager.itemBecameUsed("a");
		testAssert(manager.numUnusedItems() == 0);
	}

	// Test with a refcounted class as a value.
	{
		ManagerWithCache<int, Reference<ManagerTestItem> > manager;
		Reference<ManagerTestItem> item_0 = new ManagerTestItem(0, &manager);
		manager.insert(0, item_0);
	}


	// Test with a refcounted class as a value.
	{
		ManagerWithCache<int, Reference<ManagerTestItem> > manager;

		Reference<ManagerTestItem> item_0 = new ManagerTestItem(0, &manager);

		manager.insert(0, item_0);
		
		testAssert(item_0->getRefCount() == 2); // Manager should hold one ref, one is in this scope.
		testAssert(manager.numUsedItems() == 1);

		item_0 = NULL; // Free reference.  Item should now be unused.

		testAssert(manager.numUsedItems() == 0);
		testAssert(manager.numUnusedItems() == 1);

		int removed_key;
		Reference<ManagerTestItem> removed_item;
		testAssert(manager.removeLRUUnusedItem(removed_key, removed_item));
		testAssert(removed_key == 0);
		testAssert(removed_item->key == 0);
	}

	// Test clear() method with a refcounted class
	{
		ManagerWithCache<int, Reference<ManagerTestItem> > manager;

		Reference<ManagerTestItem> item_0 = new ManagerTestItem(0, &manager);

		manager.insert(0, item_0);

		testAssert(item_0->getRefCount() == 2); // Manager should hold one ref, one is in this scope.
		testAssert(manager.numUsedItems() == 1);

		manager.clear();
	}

	// Test destroying ManagerWithCache with items in it
	{
		ManagerWithCache<int, Reference<ManagerTestItem> > manager;

		Reference<ManagerTestItem> item_0 = new ManagerTestItem(0, &manager);

		manager.insert(0, item_0);

		testAssert(item_0->getRefCount() == 2); // Manager should hold one ref, one is in this scope.
		testAssert(manager.numUsedItems() == 1);
	}


	// Test with a larger number of items
	{
		Timer timer;

		const int N = 1000; // Can't actually do that many inserts because std::list is rediculously slow in debug mode in VS2019 (O(n^2) checking?)
		ManagerWithCache<int, int> manager;
		for(int i=0; i<N; ++i)
			manager.insert(i, i);

		for(int i=0; i<N; ++i)
			manager.itemBecameUnused(i);

		for(int i=0; i<N; ++i)
		{
			int key = -1;
			int val = -1;
			const bool removed = manager.removeLRUUnusedItem(key, val);
			testAssert(removed);
			testAssert(key == i && val == i);
		}

		testAssert(manager.numItems() == 0);
		testAssert(manager.numUnusedItems() == 0);
		conPrint("larger number of inserts and removals took " + timer.elapsedStringNSigFigs(3));
	}

	conPrint("testManagerWithCache() done.");
}


#endif // BUILD_TESTS
