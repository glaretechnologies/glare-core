/*=====================================================================
LRUCache.h
----------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <unordered_map>
#include <list>
#include <assert.h>
#include <stdlib.h> // for size_t


template <typename Key, typename Value>
struct LRUCacheItem
{
	Value value;
	size_t value_size_B;

	typename std::list<Key>::iterator item_list_it; // Effectively a pointer to a node in item_list.
};


/*=====================================================================
LRUCache
--------
=====================================================================*/
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LRUCache
{
public:
	LRUCache();
	~LRUCache();

	inline void insert(const Key& key, const Value& value, size_t value_size_B);
	inline void insert(const std::pair<Key, Value>& key_value_pair, size_t value_size_B);

	// Has no effect if key is not present in map.
	inline void itemWasUsed(const Key& key);
	
	inline void erase(typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::iterator it);

	inline bool isInserted(const Key& key) const { return items.count(key) != 0; }

	// If there was an unused item to remove, sets removed_key_out and removed_value_out, and returns true.  Returns false otherwise.
	inline bool removeLRUItem(Key& removed_key_out, Value& removed_value_out);
	
	// Remove the least recently used items until the total size of all the values in the cache is <= max_N.
	inline void removeLRUItemsUntilSizeLessEqualN(size_t max_N);
	
	inline size_t size() const { return items.size(); }
	inline size_t numItems() const { return items.size(); }
	inline size_t totalValueSizeB() const { return total_value_size_B; }

	inline void clear();

	inline typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::iterator       find(const Key& key)       { return items.find(key); }
	inline typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::const_iterator find(const Key& key) const { return items.find(key); }

	// Iterates over all items, both used and unused.
	inline typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::iterator       begin()       { return items.begin(); }
	inline typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::const_iterator begin() const { return items.begin(); }

	inline typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::iterator       end()       { return items.end(); }
	inline typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::const_iterator end() const { return items.end(); }

	std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash> items; // All items, both used and unused
	std::list<Key> item_list; // A list of items.  Most recently used at front, least recently used at back.
	size_t total_value_size_B;
};


void testLRUCache();


template <typename Key, typename Value, typename Hash>
LRUCache<Key, Value, Hash>::LRUCache()
:	total_value_size_B(0)
{
}


template <typename Key, typename Value, typename Hash>
LRUCache<Key, Value, Hash>::~LRUCache()
{
	clear();
}


template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::insert(const Key& key, const Value& value, size_t value_size_B)
{
	auto res = items.find(key);
	if(res == items.end()) // If key not already inserted:
	{
		LRUCacheItem<Key, Value> item;
		item.value = value;
		item.value_size_B = value_size_B;

		// Insert at front of item_list
		item_list.push_front(key);
		item.item_list_it = item_list.begin();

		items.insert(std::make_pair(key, item));

		assert(items.size() == item_list.size());

		total_value_size_B += value_size_B;
	}
}


template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::insert(const std::pair<Key, Value>& key_value_pair, size_t value_size_B)
{
	insert(key_value_pair.first, key_value_pair.second, value_size_B);
}


template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::itemWasUsed(const Key& key)
{
	// Move item to front of item_list
	auto res = items.find(key);
	if(res != items.end())
	{
		LRUCacheItem<Key, Value>& item = res->second;
		assert(item.item_list_it != item_list.end());
		
		// Move item to front of list
		item_list.splice(
			item_list.begin(), // position - item will be inserted before here.
			item_list, // the list the item comes from
			item.item_list_it // the element to transfer
		);

		assert(item.item_list_it == item_list.begin());
	}

	assert(items.size() == item_list.size());
}


template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::erase(typename std::unordered_map<Key, LRUCacheItem<Key, Value>, Hash>::iterator it)
{
	LRUCacheItem<Key, Value>& item = it->second;
	assert(item.item_list_it != item_list.end());

	if(item.item_list_it != item_list.end())
	{
		assert(total_value_size_B >= item.value_size_B);
		total_value_size_B -= item.value_size_B;
	}
	
	item_list.erase(item.item_list_it);
	items.erase(it);

	assert(items.size() == item_list.size());
}


template <typename Key, typename Value, typename Hash>
bool LRUCache<Key, Value, Hash>::removeLRUItem(Key& removed_key_out, Value& removed_value_out)
{
	if(item_list.empty())
	{
		return false;
	}
	else
	{
		// Remove least recently used item from back of item_list.
		const Key key = item_list.back();
		item_list.pop_back();

		// Remove the corresponding key and value from the map.
		auto res = items.find(key);
		assert(res != items.end());
		if(res == items.end())
			return false; // Key was not found in map
		else
		{
			removed_key_out = key;
			removed_value_out = res->second.value;

			assert(total_value_size_B >= res->second.value_size_B);
			total_value_size_B -= res->second.value_size_B;

			items.erase(res);
			return true;
		}
	}
}


template<typename Key, typename Value, typename Hash>
inline void LRUCache<Key, Value, Hash>::removeLRUItemsUntilSizeLessEqualN(size_t max_N)
{
	while(total_value_size_B > max_N)
	{
		Key removed_key;
		Value removed_value;
		const bool removed_item = removeLRUItem(removed_key, removed_value);
		if(!removed_item)
			break; // Failed to remove item, list must be empty.

		// Else an item was removed, total_value_size_B will have been decreased.
	}
}


template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::clear()
{
	items.clear();
	item_list.clear();
	total_value_size_B = 0;
}
