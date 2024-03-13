/*=====================================================================
HashMapInsertOnly2.h
--------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "HashMapInsertOnly2Iterators.h"
#include "Vector.h"
#include "GlareAllocator.h"
#include <functional>
#include <type_traits>


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4127) // Disable 'conditional expression is constant' warnings in the if(std::is_pod<Key>::value ...) code below.
#endif


// Implemented using FNV-1a hash.
// See https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// FNV is a good hash function for smaller key lengths.
// For longer key lengths, use something like xxhash.
// See http://aras-p.info/blog/2016/08/02/Hash-Functions-all-the-way-down/ for a performance comparison.
//
static inline size_t hashBytes(const uint8* data, size_t len)
{
	uint64 hash = 14695981039346656037ULL;
	for(size_t i=0; i<len; ++i)
		hash = (hash ^ data[i]) * 1099511628211ULL;
	return hash;
}


/*=====================================================================
HashMapInsertOnly2
------------------
A map class using a hash table.
Only insertion and lookup is supported, not removal of individual elements.
This class requires passing an 'empty key' to the constructor.
This is a sentinel value that is never inserted in the map, and marks empty buckets.
=====================================================================*/
template <typename Key, typename Value, typename HashFunc = std::hash<Key>>
class HashMapInsertOnly2
{
public:

	typedef HashMapInsertOnly2Iterator<Key, Value, HashFunc> iterator;
	typedef ConstHashMapInsertOnly2Iterator<Key, Value, HashFunc> const_iterator;
	typedef std::pair<Key, Value> KeyValuePair;


	HashMapInsertOnly2(Key empty_key_)
	:	buckets((std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * 32, 64)), buckets_size(32), num_items(0), hash_mask(31), empty_key(empty_key_), allocator(NULL)
	{
		// Initialise elements
		std::pair<Key, Value> empty_key_val(empty_key, Value());
		for(std::pair<Key, Value>* elem=buckets; elem<buckets + buckets_size; ++elem)
			::new (elem) std::pair<Key, Value>(empty_key_val);
	}


	HashMapInsertOnly2(Key empty_key_, size_t expected_num_items, glare::Allocator* allocator_ = NULL)
	:	num_items(0), empty_key(empty_key_), allocator(allocator_)
	{
		buckets_size = myMax<size_t>(32ULL, Maths::roundToNextHighestPowerOf2(expected_num_items*2));
		
		if(allocator)
			buckets = (std::pair<Key, Value>*)allocator->alloc       (sizeof(std::pair<Key, Value>) * buckets_size, 64);
		else
			buckets = (std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * buckets_size, 64);

		// Initialise elements
		if(std::is_pod<Key>::value && std::is_pod<Value>::value)
		{
			std::pair<Key, Value>* const buckets_ = buckets; // Having this as a local var gives better codegen in VS.
			for(size_t z=0; z<buckets_size; ++z)
				buckets_[z].first = empty_key_;
		}
		else
		{
			std::pair<Key, Value> empty_key_val(empty_key, Value());
			for(std::pair<Key, Value>* elem=buckets; elem<buckets + buckets_size; ++elem)
				::new (elem) std::pair<Key, Value>(empty_key_val);
		}

		hash_mask = buckets_size - 1;
	}

	~HashMapInsertOnly2()
	{
		// Destroy objects
		for(size_t i=0; i<buckets_size; ++i)
			(buckets + i)->~KeyValuePair();

		if(allocator)
			allocator->free(buckets);
		else
			MemAlloc::alignedFree(buckets);
	}


	iterator begin()
	{
		// Find first bucket with a value in it.
		for(size_t i=0; i<buckets_size; ++i)
			if(buckets[i].first != empty_key)
				return iterator(this, buckets + i);

		return iterator(this, buckets + buckets_size);
	}

	const_iterator begin() const
	{
		// Find first bucket with a value in it.
		for(size_t i=0; i<buckets_size; ++i)
			if(buckets[i].first != empty_key)
				return const_iterator(this, buckets + i);

		return const_iterator(this, buckets + buckets_size);
	}

	iterator end() { return HashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, buckets + buckets_size); }
	const_iterator end() const { return ConstHashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, buckets + buckets_size); }


	iterator find(const Key& k)
	{
		size_t bucket_i = hashKey(k);

		// Search for bucket item is in
		while(1)
		{
			if(buckets[bucket_i].first == k)
				return HashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, &buckets[bucket_i]); // Found it

			if(buckets[bucket_i].first == empty_key)
				return end(); // No such key in map.

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask; // (bucket_i + 1) % buckets.size();
		}
	}

	const_iterator find(const Key& k) const
	{
		size_t bucket_i = hashKey(k);

		// Search for bucket item is in
		while(1)
		{
			if(buckets[bucket_i].first == k)
				return ConstHashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, &buckets[bucket_i]); // Found it

			if(buckets[bucket_i].first == empty_key)
				return end(); // No such key in map.

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask; // (bucket_i + 1) % buckets.size();
		}
		return end();
	}


	size_t count(const Key& k) const
	{
		return (find(k) == end()) ? 0 : 1;
	}


	// If key was already in map, returns iterator to existing item and false.
	// If key was not already in map, inserts it, then returns iterator to new item and true.
	std::pair<iterator, bool> insert(const std::pair<Key, Value>& key_val_pair)
	{
		assert(key_val_pair.first != empty_key);

		size_t bucket_i = hashKey(key_val_pair.first);

		// Search for bucket item is in, or an empty bucket
		while(1)
		{
			if(buckets[bucket_i].first == empty_key) // If bucket is empty:
			{
				// Item was not found in the map.  So we will need to insert it.
				num_items++;
				if(checkForExpand()) // If expansion/rehashing occurred:
				{
					// num buckets will have changed, so we need to rehash.
					bucket_i = hashKey(key_val_pair.first);
					while(1)
					{
						if(buckets[bucket_i].first == empty_key) // If bucket is empty:
						{
							buckets[bucket_i] = key_val_pair;
							return std::make_pair(HashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, buckets/*.begin()*/ + bucket_i), true);
						}
						// If bucket is full, we don't need to establish that the item in the bucket is != our key, since we already know that.
						// Else advance to next bucket, with wrap-around
						bucket_i = (bucket_i + 1) & hash_mask; // bucket_i = (bucket_i + 1) % buckets.size();
					}
				}
				
				buckets[bucket_i] = key_val_pair;
				return std::make_pair(HashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, buckets + bucket_i), true);
			}

			// Else if bucket contains an item with matching key:
			if(buckets[bucket_i].first == key_val_pair.first)
				return std::make_pair(HashMapInsertOnly2Iterator<Key, Value, HashFunc>(this, buckets + bucket_i), false); // Found it, return iterator to existing item and false.

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask; // bucket_i = (bucket_i + 1) % buckets.size();
		}
	}

	Value& operator [] (const Key& k)
	{
		iterator i = find(k);
		if(i == end())
		{
			std::pair<iterator, bool> result = insert(std::make_pair(k, Value()));
			return result.first->second;
		}
		else
		{
			return i->second;
		}
	}


	void clear()
	{
		for(size_t i=0; i<buckets_size; ++i)
		{
			buckets[i].first = empty_key;
			
			if(!std::is_pod<Value>::value)
				buckets[i].second = Value();
		}
		num_items = 0;
	}

	bool empty() const { return num_items == 0; }

	size_t size() const { return num_items; }

private:
	size_t hashKey(const Key& k) const
	{
		//return hash_func(k) % buckets.size();
		return hash_func(k) & hash_mask;
	}

	// Returns true if expanded
	bool checkForExpand()
	{
		//size_t load_factor = num_items / buckets.size();
		//const float load_factor = (float)num_items / buckets.size();

		//if(load_factor > 0.5f)
		if(num_items >= buckets_size / 2)
		{
			expand(/*buckets.size() * 2*/);
			return true;
		}
		else
		{
			return false;
		}
	}


	void expand(/*size_t new_num_buckets*/)
	{
		// Get pointer to old buckets
		const std::pair<Key, Value>* const old_buckets = this->buckets;
		const size_t old_buckets_size = this->buckets_size;

		// Allocate new buckets
		this->buckets_size = old_buckets_size * 2;
		if(allocator)
			this->buckets = (std::pair<Key, Value>*)allocator->alloc       (sizeof(std::pair<Key, Value>) * this->buckets_size, 64);
		else
			this->buckets = (std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * this->buckets_size, 64);
		
		// Initialise elements
		if(std::is_pod<Key>::value && std::is_pod<Value>::value)
		{
			const Key empty_key_ = empty_key; // Having this as a local var gives better codegen in VS.
			std::pair<Key, Value>* const buckets_ = buckets; // Having this as a local var gives better codegen in VS.
			for(size_t z=0; z<buckets_size; ++z)
				buckets_[z].first = empty_key_;
		}
		else
		{
			// Initialise elements
			std::pair<Key, Value> empty_key_val(empty_key, Value());
			if(buckets)
				for(size_t z=0; z<buckets_size; ++z)
					::new (buckets + z) std::pair<Key, Value>(empty_key_val);
		}

		hash_mask = this->buckets_size - 1;

		// Insert items into new buckets
		
		for(size_t b=0; b<old_buckets_size; ++b) // For each old bucket
		{
			if(old_buckets[b].first != empty_key) // If there is an item in the old bucket:
			{
				size_t bucket_i = hashKey(old_buckets[b].first); // Hash key of item to get new bucket index:

				// Search for an empty bucket
				while(1)
				{
					if(buckets[bucket_i].first == empty_key) // If bucket is empty:
					{
						buckets[bucket_i] = old_buckets[b]; // Write item to bucket.
						break;
					}

					// Else advance to next bucket, with wrap-around
					bucket_i = (bucket_i + 1) & hash_mask; // bucket_i = (bucket_i + 1) % buckets.size();
				}
			}
		}

		// Destroy old bucket data
		for(size_t i=0; i<old_buckets_size; ++i)
			(old_buckets + i)->~KeyValuePair();

		if(allocator)
			allocator->free((void*)old_buckets);
		else
			MemAlloc::alignedFree((std::pair<Key, Value>*)old_buckets);
	}
				
public:
	std::pair<Key, Value>* buckets; // Elements
	size_t buckets_size;
	HashFunc hash_func;
	Key empty_key;
	glare::Allocator* allocator;
private:
	size_t num_items;
	size_t hash_mask;
};


void testHashMapInsertOnly2();


#ifdef _WIN32
#pragma warning(pop)
#endif
