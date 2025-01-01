/*=====================================================================
LRUCache.cpp
------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "LRUCache.h"


#if BUILD_TESTS


#include "Reference.h"
#include "ConPrint.h"
#include "TestUtils.h"
#include "Timer.h"
#include "StringUtils.h"


struct LRUCacheTestItem : public RefCounted
{
	LRUCacheTestItem(int key_)
	:	key(key_)
	{}

	int key;
};


void testLRUCache()
{
	conPrint("testLRUCache()");

	// Test insert, itemWasUsed, removeLRUUnusedItem
	{
		LRUCache<std::string, int> cache;
		cache.insert("a", 1);
		cache.itemWasUsed("a");

		cache.insert("b", 2);
		cache.itemWasUsed("b");

		testAssert(cache.numItems() == 2);

		cache.itemWasUsed("a");
		cache.itemWasUsed("b");

		testAssert(cache.numItems() == 2);

		std::string removed_key;
		int removed_val;
		bool removed = cache.removeLRUItem(removed_key, removed_val);
		testAssert(removed);
		testAssert(removed_key == "a");
		testEqual(removed_val, 1);
		testAssert(cache.numItems() == 1);

		removed = cache.removeLRUItem(removed_key, removed_val);
		testAssert(removed);
		testAssert(removed_key == "b");
		testEqual(removed_val, 2);
		testAssert(cache.numItems() == 0);

		removed = cache.removeLRUItem(removed_key, removed_val);
		testAssert(!removed);
		testAssert(cache.numItems() == 0);

		cache.insert("a", 1);
		cache.insert("b", 2);
		cache.insert("c", 3);
		// item_list: [c, b, a]

		cache.itemWasUsed("a"); // item_list: [a, c, b]
		cache.itemWasUsed("b"); // item_list: [b, a, c]
		cache.itemWasUsed("c"); // item_list: [c, b, a]
		cache.itemWasUsed("a"); // item_list: [a, c, b]

		testAssert(cache.removeLRUItem(removed_key, removed_val));
		testAssert(removed_key == "b");
		testEqual(removed_val, 2);
		testAssert(cache.numItems() == 2);

		 // item_list: [a, c]

		testAssert(cache.removeLRUItem(removed_key, removed_val));
		testAssert(removed_key == "c");
		testEqual(removed_val, 3);
		testAssert(cache.numItems() == 1);

		testAssert(cache.isInserted("a"));
	}

	// Test find and erase
	{
		LRUCache<std::string, int> cache;
		cache.insert("a", 1);
		cache.insert("b", 2);

		cache.itemWasUsed("a");
		cache.itemWasUsed("b");

		testAssert(cache.numItems() == 2);

		testAssert(cache.find("z") == cache.end());
		auto res = cache.find("a");
		testAssert(res != cache.end());

		cache.erase(res);
		testAssert(cache.find("a") == cache.end());

		testAssert(cache.numItems() == 1);
	}

	// Test duplicate insert (second insert has no effect, like std::map)
	{
		LRUCache<std::string, int> cache;
		cache.insert("a", 1);
		testAssert(cache.numItems() == 1);
		testAssert(cache.find("a")->second.value == 1);
		cache.insert("a", 2);
		testAssert(cache.numItems() == 1);
		testAssert(cache.find("a")->second.value == 1);
	}

	// Test calling itemWasUsed with the same key multiple times.
	{
		LRUCache<std::string, int> cache;
		cache.insert("a", 1);
		cache.itemWasUsed("a");
		cache.itemWasUsed("a");
		cache.itemWasUsed("a");
	}

	// Test with a refcounted class as a value.
	{
		LRUCache<int, Reference<LRUCacheTestItem> > cache;
		Reference<LRUCacheTestItem> item_0 = new LRUCacheTestItem(0);
		cache.insert(0, item_0);
	}

	// Test with a refcounted class as a value.
	{
		LRUCache<int, Reference<LRUCacheTestItem> > cache;

		Reference<LRUCacheTestItem> item_0 = new LRUCacheTestItem(0);

		cache.insert(0, item_0);
		
		testAssert(item_0->getRefCount() == 2); // cache should hold one ref, one is in this scope.

		item_0 = NULL; // Free reference.  Item should now be unused.

		int removed_key;
		Reference<LRUCacheTestItem> removed_item;
		testAssert(cache.removeLRUItem(removed_key, removed_item));
		testAssert(removed_key == 0);
		testAssert(removed_item->key == 0);
	}

	// Test clear() method with a refcounted class
	{
		LRUCache<int, Reference<LRUCacheTestItem> > cache;

		Reference<LRUCacheTestItem> item_0 = new LRUCacheTestItem(0);

		cache.insert(0, item_0);

		testAssert(item_0->getRefCount() == 2); // cache should hold one ref, one is in this scope.

		cache.clear();
	}

	// Test destroying LRUCache with items in it
	{
		LRUCache<int, Reference<LRUCacheTestItem> > cache;

		Reference<LRUCacheTestItem> item_0 = new LRUCacheTestItem(0);

		cache.insert(0, item_0);

		testAssert(item_0->getRefCount() == 2); // cache should hold one ref, one is in this scope.
	}

	// Test with a larger number of items
	{
		Timer timer;

		const int N = 1000; // Can't actually do that many inserts because std::list is rediculously slow in debug mode in VS2019 (O(n^2) checking?)
		LRUCache<int, int> cache;
		for(int i=0; i<N; ++i)
			cache.insert(i, i);

		for(int i=0; i<N; ++i)
			cache.itemWasUsed(i);

		for(int i=0; i<N; ++i)
		{
			int key = -1;
			int val = -1;
			const bool removed = cache.removeLRUItem(key, val);
			testAssert(removed);
			testAssert(key == i && val == i);
		}

		testAssert(cache.numItems() == 0);
		conPrint("larger number of inserts and removals took " + timer.elapsedStringNSigFigs(3));
	}

	conPrint("testLRUCache() done.");
}


#endif // BUILD_TESTS
