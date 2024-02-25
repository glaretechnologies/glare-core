/*=====================================================================
ManagerWithCache.h
------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <unordered_map>
#include <list>
#include <assert.h>
#include <stdlib.h> // for size_t


template <typename Key, typename Value>
struct ManagerWithCacheItem
{
	Value value;

	typename std::list<Key>::iterator unused_items_it; // Effectively a pointer to a node in the unused_items list.
};


/*=====================================================================
ManagerWithCache
----------------
Something like a std::unordered_map, but also each inserted item can be 'used' or 'unused'.
Unused items are stored in a list that is sorted by the order in which they became unused,
so it can be used as a LRU cache.
The least recently used item can be removed from the cache with removeLRUUnusedItem().
=====================================================================*/
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class ManagerWithCache
{
public:
	ManagerWithCache();
	~ManagerWithCache();

	inline void insert(const Key& key, const Value& value);
	inline void insert(const std::pair<Key, Value>& key_value_pair);

	inline void set(const Key& key, const Value& value);

	// Has no effect if key is not present in map.
	inline void itemBecameUsed(const Key& key);
	
	// Has no effect if key is not present in map.
	inline void itemBecameUnused(const Key& key);

	inline void erase(typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::iterator it);

	inline bool isInserted(const Key& key) const { return items.count(key) != 0; }

	// If there was an unused item to remove, sets removed_key_out and removed_value_out, and returns true.  Returns false otherwise.
	inline bool removeLRUUnusedItem(Key& removed_key_out, Value& removed_value_out);
	
	inline size_t size() const { return items.size(); }
	inline size_t numItems() const { return items.size(); }
	inline size_t numUnusedItems() const { return unused_items.size(); } // std::list::size() is constant time in c++11 +
	inline size_t numUsedItems() const { assert(items.size() >= unused_items.size()); return items.size() - unused_items.size(); }

	inline bool isItemUsed(const ManagerWithCacheItem<Key, Value>& item) const { return item.unused_items_it == unused_items.end(); }

	inline void clear();

	inline typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::iterator       find(const Key& key)       { return items.find(key); }
	inline typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::const_iterator find(const Key& key) const { return items.find(key); }

	// Iterates over all items, both used and unused.
	inline typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::iterator       begin()       { return items.begin(); }
	inline typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::const_iterator begin() const { return items.begin(); }

	inline typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::iterator       end()       { return items.end(); }
	inline typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::const_iterator end() const { return items.end(); }

	std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash> items; // All items, both used and unused
	std::list<Key> unused_items; // A list of unused items.  Most recently used at front, least recently used at back.
private:
	bool add_to_unused_items; // Should we add items to unused_items in itemBecameUnused().
};


void testManagerWithCache();


template <typename Key, typename Value, typename Hash>
ManagerWithCache<Key, Value, Hash>::ManagerWithCache()
:	add_to_unused_items(true)
{
}


template <typename Key, typename Value, typename Hash>
ManagerWithCache<Key, Value, Hash>::~ManagerWithCache()
{
	clear();
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::insert(const Key& key, const Value& value)
{
	ManagerWithCacheItem<Key, Value> item;
	item.value = value;
	item.unused_items_it = unused_items.end();
	items.insert(std::make_pair(key, item));
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::insert(const std::pair<Key, Value>& key_value_pair)
{
	ManagerWithCacheItem<Key, Value> item;
	item.value = key_value_pair.second;
	item.unused_items_it = unused_items.end();
	items.insert(std::make_pair(key_value_pair.first, item));
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::set(const Key& key, const Value& value)
{
	auto res = items.find(key);
	if(res != items.end())
	{
		items[key].value = value;
	}
	else
	{
		items[key].value = value;
		items[key].unused_items_it = unused_items.end(); // Initialise unused_items_it
	}
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::itemBecameUsed(const Key& key)
{
	// Remove item from unused_items list, if it is in there.
	auto res = items.find(key);
	if(res != items.end())
	{
		ManagerWithCacheItem<Key, Value>& item = res->second;
		if(item.unused_items_it != unused_items.end())
		{
			unused_items.erase(item.unused_items_it);
			item.unused_items_it = unused_items.end();
		}
	}
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::itemBecameUnused(const Key& key)
{
	if(add_to_unused_items)
	{
		// Insert item at front of unused_items list
		auto res = items.find(key);
		if(res != items.end())
		{
			if(res->second.unused_items_it == unused_items.end()) // If this item is not already marked as unused:
			{
				unused_items.push_front(key); // Insert at front of unused_items list
				res->second.unused_items_it = unused_items.begin(); // Store iterator (pointer) to front unused_items list item. 
			}
		}
	}
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::erase(typename std::unordered_map<Key, ManagerWithCacheItem<Key, Value>, Hash>::iterator it)
{
	ManagerWithCacheItem<Key, Value>& item = it->second;
	if(item.unused_items_it != unused_items.end())
	{
		unused_items.erase(item.unused_items_it);
	}

	items.erase(it);
}


template <typename Key, typename Value, typename Hash>
bool ManagerWithCache<Key, Value, Hash>::removeLRUUnusedItem(Key& removed_key_out, Value& removed_value_out)
{
	if(unused_items.empty())
	{
		return false;
	}
	else
	{
		// Remove least recently used item from back of unused_items list.
		const Key key = unused_items.back();
		unused_items.pop_back();

		// Remove the corresponding key and value from the map.
		auto res = items.find(key);
		if(res == items.end())
			return false; // Key was not found in map
		else
		{
			removed_key_out = key;
			removed_value_out = res->second.value;
			items.erase(res);
			return true;
		}
	}
}


template <typename Key, typename Value, typename Hash>
void ManagerWithCache<Key, Value, Hash>::clear()
{
	// Disable adding items to unused_items during clear(), because it's invalid to add items to unused_items while it is being cleared,
	// which can happen if an item becomes unused (ref count drops to one) as it is removed from unused_items.
	add_to_unused_items = false;

	items.clear();
	unused_items.clear();

	add_to_unused_items = true;
}
