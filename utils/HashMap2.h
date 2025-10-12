/*=====================================================================
HashMap2.h
----------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "HashMap2Iterators.h"
#include "GlareAllocator.h"
#include "BitVector.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <functional>
#include <type_traits>


/*=====================================================================
HashMap2
--------
A map class using a hash table.
Instead of an 'empty' sentinel value to mark empty buckets, this class
uses a bitvector to store if buckets are occupied.
=====================================================================*/
template <typename Key, typename Value, typename HashFunc = std::hash<Key>>
class HashMap2
{
public:

	typedef HashMap2Iterator<Key, Value, HashFunc> iterator;
	typedef ConstHashMap2Iterator<Key, Value, HashFunc> const_iterator;
	typedef std::pair<Key, Value> KeyValuePair;


	// Load factor = 3/4
	static size_t multiplyByMaxLoadFactor(size_t x)
	{
		return x * 3 / 4; // The divide should be optimised into a mul by the compiler.
	}

	static size_t divideByMaxLoadFactor(size_t x)
	{
		return x * 4 / 3; // The divide should be optimised into a mul by the compiler.
	}


	HashMap2()
	:	buckets((std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * 32, 64)), 
		mask(32), // Bits are initialised to zero in BitVector constructor.
		buckets_size(32), num_items(0), hash_mask(31), allocator(nullptr)
	{
	}

	HashMap2(size_t expected_num_items, glare::Allocator* allocator_ = nullptr)
	:	num_items(0), allocator(allocator_)
	{
		buckets_size = myMax<size_t>(32ULL, Maths::roundToNextHighestPowerOf2(divideByMaxLoadFactor(expected_num_items)));
		
		if(allocator)
			buckets = (std::pair<Key, Value>*)allocator->alloc       (sizeof(std::pair<Key, Value>) * buckets_size, 64);
		else
			buckets = (std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * buckets_size, 64);

		mask.resizeNoCopy(buckets_size);
		mask.setAllBits(0);

		hash_mask = buckets_size - 1;
	}

	HashMap2(const HashMap2& other)
	{
		buckets_size = other.buckets_size;
		hash_func = other.hash_func;
		num_items = other.num_items;
		hash_mask = other.hash_mask;
		mask = other.mask;
		allocator = nullptr;

		buckets = (std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * buckets_size, 64);

		for(size_t i=0; i<buckets_size; ++i)
			if(mask.getBitMasked(i) != 0) // If bucket is not empty:
				::new (&buckets[i]) std::pair<Key, Value>(other.buckets[i]);
	}

	~HashMap2()
	{
		// Destroy objects
		for(size_t i=0; i<buckets_size; ++i)
			if(mask.getBitMasked(i) != 0) // If bucket is not empty:
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
		{
			if(mask.getBitMasked(i) != 0)
				return iterator(this, buckets + i);
		}

		return iterator(this, buckets + buckets_size);
	}

	const_iterator begin() const
	{
		// Find first bucket with a value in it.
		for(size_t i=0; i<buckets_size; ++i)
		{
			if(mask.getBitMasked(i) != 0)
				return const_iterator(this, buckets + i);
		}

		return const_iterator(this, buckets + buckets_size);
	}

	iterator end() { return HashMap2Iterator<Key, Value, HashFunc>(this, buckets + buckets_size); }
	const_iterator end() const { return ConstHashMap2Iterator<Key, Value, HashFunc>(this, buckets + buckets_size); }

	template <class Key2>
	iterator find(const Key2& k)
	{
		size_t bucket_i = hashKey(k);

		// Search for bucket item is in
		while(1)
		{
			if(mask.getBitMasked(bucket_i) == 0)
				return end(); // No such key in map.

			if(buckets[bucket_i].first == k)
				return HashMap2Iterator<Key, Value, HashFunc>(this, &buckets[bucket_i]); // Found it

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
			if(mask.getBitMasked(bucket_i) == 0)
				return end(); // No such key in map.

			if(buckets[bucket_i].first == k)
				return ConstHashMap2Iterator<Key, Value, HashFunc>(this, &buckets[bucket_i]); // Found it

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask; // (bucket_i + 1) % buckets.size();
		}
		return end();
	}


	// The basic idea here is instead of marking bucket i empty immediately, we will scan right, looking for objects that can be moved left to fill the empty slot.
	// See https://en.wikipedia.org/wiki/Open_addressing and https://en.wikipedia.org/w/index.php?title=Hash_table&oldid=95275577
	// This is also pretty much the same algorithm as 'Algorithm R (Deletion with linear probing)' in Section 6.4 of The Art of Computer Programming, Volume 3.
	void erase(const Key& key)
	{
		// Search for bucket item is in, or until we get to an empty bucket, which indicates the key is not in the map.
		// Bucket i is the bucket we will finally mark as empty.
		size_t i = hashKey(key);
		while(1)
		{
			if(mask.getBitMasked(i) == 0) // If bucket i is empty:
				return; // No such key in map.

			if(buckets[i].first == key)
				break;

			// Else advance to next bucket, with wrap-around
			i = (i + 1) & hash_mask;
		}

		assert((mask.getBitMasked(i) != 0) && (buckets[i].first == key));

		size_t j = i; // j = current probe index to right of i, i = the current slot we will make empty
		while(1)
		{
			j = (j + 1) & hash_mask;
			if(mask.getBitMasked(j) == 0)
				break;
			/*
			We are considering whether the item at location j can be moved to location i.
			This is allowed if the natural hash location k of the item is <= i, before modulo.

			Two cases to handle here: case where j does not wrap relative to i (j > i):
			Then acceptable ranges for k (natural hash location of j), such that j can be moved to location i:
			basically k has to be <= i before the modulo.
			
			-------------------------------------
			|   |   |   | a | b | c |   |   |   |
			-------------------------------------
			              i       j
			----------------|       |-----------
			  k <= i                     k > j

			Case where j does wrap relative to i (j < i):
			Then acceptable ranges for k (natural hash location of j), such that j can be moved to location i:
			basically k has to be <= i before the modulo.

			-------------------------------------
			| c | d |   |   |   |   |   | a | b |
			-------------------------------------
			      j                       i
			        |-----------------------|
			          k > j && k <= i

			Note that the natural hash location of an item at j is always <= j (before modulo)
			*/
			const size_t k = hashKey(buckets[j].first); // k = natural hash location of item in bucket j.
			if((j > i) ? (k <= i || k > j) : (k <= i && k > j))
			{
				// Move item in bucket j to bucket i, set i := j.
				buckets[i] = buckets[j];
				i = j;
			}
		}

		mask.setBitToZero(i);
		buckets[i].~KeyValuePair(); // Destroy key and value in bucket i.

		num_items--;
	}

	size_t count(const Key& k) const
	{
		return (find(k) == end()) ? 0 : 1;
	}


	// If key was already in map, returns iterator to existing item and false.
	// If key was not already in map, inserts it, then returns iterator to new item and true.
	template <class Key2>
	std::pair<iterator, bool> insert(const std::pair<Key2, Value>& key_val_pair)
	{
#if 1
		checkForExpand(); // Check for expand with the current size.  This does a slightly inaccurate load factor comparison, but it shouldn't matter.

		// Find
		size_t bucket_i = hashKey(key_val_pair.first);

		// Search for bucket item is in
		while(1)
		{
			if(mask.getBitMasked(bucket_i) == 0) // If bucket is empty:
			{
				::new (&buckets[bucket_i]) std::pair<Key, Value>(key_val_pair); // Copy construct from the passed-in value.
				mask.setBitToOne(bucket_i);
				num_items++;
				return std::make_pair(HashMap2Iterator<Key, Value, HashFunc>(this, buckets + bucket_i), /*inserted=*/true);
			}

			if(buckets[bucket_i].first == key_val_pair.first)
				return std::make_pair(HashMap2Iterator<Key, Value, HashFunc>(this, buckets + bucket_i), /*inserted=*/false); // Already inserted

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask;
		}
#else

	doInsert:
		// Find
		size_t bucket_i = hashKey(key_val_pair.first);

		// Search for bucket item is in
		while(1)
		{
			if(buckets[bucket_i].first == key_val_pair.first)
				return std::make_pair(HashMapIterator<Key, Value, HashFunc>(this, buckets/*.begin()*/ + bucket_i), /*inserted=*/false); // Already inserted

			if(buckets[bucket_i].first == empty_key)
			{
				const bool expanded = checkForExpand();
				if(expanded)
					goto doInsert;
				buckets[bucket_i] = key_val_pair;
				num_items++;
				return std::make_pair(HashMapIterator<Key, Value, HashFunc>(this, buckets/*.begin()*/ + bucket_i), /*inserted=*/true);
			}

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask;
		}
#endif
	}

	template <class Key2>
	Value& operator [] (const Key2& k)
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
		// Destroy objects
		for(size_t i=0; i<buckets_size; ++i)
			if(mask.getBitMasked(i) != 0) // If bucket is not empty:
				(buckets + i)->~KeyValuePair();

		mask.setAllBits(0);
		num_items = 0;
	}

	void invariant()
	{
		for(size_t i=0; i<buckets_size; ++i)
		{
			const Key key = buckets[i].first;
			if(mask.getBitMasked(i) != 0)
			{
				const size_t k = hashKey(key);

				for(size_t z=k; z != i; z = (z + 1) & hash_mask)
				{
					//assert(buckets[z].first != empty_key);
				}
			}
		}
	}

	bool empty() const { return num_items == 0; }

	size_t size() const { return num_items; }

private:
	template <class Key2>
	size_t hashKey(const Key2& k) const
	{
		//return hash_func(k) % buckets.size();
		return hash_func(k) & hash_mask;
	}

	// Returns true if expanded
	bool checkForExpand()
	{
		if(num_items >= multiplyByMaxLoadFactor(buckets_size))
		{
			expand();
			return true;
		}
		else
		{
			return false;
		}
	}


	void expand()
	{
		// Get pointer to old buckets
		std::pair<Key, Value>* const old_buckets = this->buckets;
		const size_t old_buckets_size = this->buckets_size;

		// Allocate new buckets
		this->buckets_size = old_buckets_size * 2;

		if(allocator)
			this->buckets = (std::pair<Key, Value>*)allocator->alloc       (sizeof(std::pair<Key, Value>) * this->buckets_size, 64);
		else
			this->buckets = (std::pair<Key, Value>*)MemAlloc::alignedMalloc(sizeof(std::pair<Key, Value>) * this->buckets_size, 64);
		
		// Initialise elements
		BitVector new_mask(this->buckets_size);

		hash_mask = this->buckets_size - 1;

		// Insert items into new buckets
		
		for(size_t b=0; b<old_buckets_size; ++b) // For each old bucket
		{
			if(mask.getBitMasked(b) != 0) // If there is an item in the old bucket:
			{
				size_t bucket_i = hashKey(old_buckets[b].first); // Hash key of item to get new bucket index:

				// Search for an empty bucket
				while(1)
				{
					if(new_mask.getBitMasked(bucket_i) == 0) // If bucket is empty:
					{
						::new (&buckets[bucket_i]) std::pair<Key, Value>(std::move(old_buckets[b])); // Copy construct from the old bucket.  TODO: use move?
						new_mask.setBitToOne(bucket_i); // Mark bucket as occupied.
						break;
					}

					// Else advance to next bucket, with wrap-around
					bucket_i = (bucket_i + 1) & hash_mask;
				}
			}
		}

		// Destroy old bucket data
		for(size_t i=0; i<old_buckets_size; ++i)
		{
			if(mask.getBitMasked(i) != 0) // If bucket is not empty:
				(old_buckets + i)->~KeyValuePair();
		}

		mask.swapWith(new_mask);

		if(allocator)
			allocator->free((void*)old_buckets);
		else
			MemAlloc::alignedFree((std::pair<Key, Value>*)old_buckets);
	}

public:
	std::pair<Key, Value>* buckets; // Elements
	BitVector mask;
	size_t buckets_size;
	HashFunc hash_func;
	glare::Allocator* allocator;
private:
	size_t num_items;
	size_t hash_mask;
};


void testHashMap2();
