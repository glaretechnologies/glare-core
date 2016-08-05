/*=====================================================================
HashMapInsertOnly.h
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "HashMapInsertOnlyIterators.h"
#include "Vector.h"
#include "BitVector.h"


/*=====================================================================
HashMapInsertOnly
-------------------
A map class using a hash table.
Only insertion and lookup is supported, not removal of individual elements.
=====================================================================*/
template <typename Key, typename Value, typename HashFunc = std::hash<Key>>
class HashMapInsertOnly
{
public:

	typedef HashMapInsertOnlyIterator<Key, Value, HashFunc> iterator;
	typedef ConstHashMapInsertOnlyIterator<Key, Value, HashFunc> const_iterator;

	HashMapInsertOnly()
	:	buckets(32), mask(32), num_items(0), hash_mask(31)
	{
	}


	HashMapInsertOnly(size_t expected_num_items)
	:	num_items(0)
	{
		const size_t num_buckets = myMax<size_t>(32ULL, Maths::roundToNextHighestPowerOf2(expected_num_items*2));
		buckets.resizeUninitialised(num_buckets);
		mask.resize(num_buckets);
		hash_mask = num_buckets - 1;
	}


	iterator begin()
	{
		// Find first bucket with a value in it.
		for(size_t i=0; i<buckets.size(); ++i)
		{
			if(mask.getBitMasked(i) != 0)
				return iterator(this, buckets.begin() + i);
		}

		return iterator(this, buckets.end());
	}

	const_iterator begin() const
	{
		// Find first bucket with a value in it.
		for(size_t i=0; i<buckets.size(); ++i)
		{
			if(mask.getBitMasked(i) != 0)
				return const_iterator(this, buckets.begin() + i);
		}

		return const_iterator(this, buckets.end());
	}

	iterator end() { return HashMapInsertOnlyIterator<Key, Value, HashFunc>(this, buckets.end()); }
	const_iterator end() const { return ConstHashMapInsertOnlyIterator<Key, Value, HashFunc>(this, buckets.end()); }


	iterator find(const Key& k)
	{
		size_t bucket_i = hashKey(k);

		// Search for bucket item is in
		while(1)
		{
			if(mask.getBitMasked(bucket_i) == 0)
				return end(); // No such key in map.

			if(buckets[bucket_i].first == k)
				return HashMapInsertOnlyIterator<Key, Value, HashFunc>(this, &buckets[bucket_i]); // Found it

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
				return ConstHashMapInsertOnlyIterator<Key, Value, HashFunc>(this, &buckets[bucket_i]); // Found it

			// Else advance to next bucket, with wrap-around
			bucket_i = (bucket_i + 1) & hash_mask; // (bucket_i + 1) % buckets.size();
		}
		return end();
	}


	// If key was already in map, returns iterator to existing item and false.
	// If key was not already in map, inserts it, then returns iterator to new item and true.
	std::pair<iterator, bool> insert(const std::pair<Key, Value>& key_val_pair)
	{
		size_t bucket_i = hashKey(key_val_pair.first);

		// Search for bucket item is in, or an empty bucket
		while(1)
		{
			if(mask.getBitMasked(bucket_i) == 0) // If bucket is empty:
			{
				// Item was not found in the map.  So we will need to insert it.
				num_items++;
				if(checkForExpand()) // If expansion/rehashing occurred:
				{
					// num buckets will have changed, so we need to rehash.
					bucket_i = hashKey(key_val_pair.first);
					while(1)
					{
						if(mask.getBitMasked(bucket_i) == 0) // If bucket is empty:
						{
							buckets[bucket_i] = key_val_pair;
							mask.setBitToOne(bucket_i);
							return std::make_pair(HashMapInsertOnlyIterator<Key, Value, HashFunc>(this, buckets.begin() + bucket_i), true);
						}
						// If bucket is full, we don't need to establish that the item in the bucket is != our key, since we already know that.
						// Else advance to next bucket, with wrap-around
						bucket_i = (bucket_i + 1) & hash_mask; // bucket_i = (bucket_i + 1) % buckets.size();
					}
				}
				
				buckets[bucket_i] = key_val_pair;
				mask.setBitToOne(bucket_i);
				return std::make_pair(HashMapInsertOnlyIterator<Key, Value, HashFunc>(this, buckets.begin() + bucket_i), true);
			}

			// Else if bucket contains an item with matching key:
			if(buckets[bucket_i].first == key_val_pair.first)
				return std::make_pair(HashMapInsertOnlyIterator<Key, Value, HashFunc>(this, buckets.begin() + bucket_i), false); // Found it, return iterator to existing item and false.

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
		//buckets.clear();
		mask.setAllBits(0);
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
		if(num_items >= buckets.size() / 2)
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
		const js::Vector<std::pair<Key, Value>, 64> old_buckets = buckets;
		const BitVector old_mask = mask;

		const size_t new_num_buckets = buckets.size()*2;
		buckets.resizeUninitialised(new_num_buckets);
		mask.resize(new_num_buckets);
		mask.setAllBits(0);

		assert(new_num_buckets == old_buckets.size()*2);
		hash_mask = new_num_buckets - 1;

		// Insert items into new buckets
		for(size_t b=0; b<old_buckets.size(); ++b) // For each old bucket
		{
			if(old_mask.getBitMasked(b) != 0) // If there is an item in the old bucket:
			{
				size_t bucket_i = hashKey(old_buckets[b].first); // Hash key of item to get new bucket index:

				// Search for an empty bucket
				while(1)
				{
					if(mask.getBitMasked(bucket_i) == 0) // If bucket is empty:
					{
						buckets[bucket_i] = old_buckets[b]; // Write item to bucket.
						mask.setBitToOne(bucket_i);
						break;
					}

					// Else advance to next bucket, with wrap-around
					bucket_i = (bucket_i + 1) & hash_mask; // bucket_i = (bucket_i + 1) % buckets.size();
				}
			}
		}
	}
				
public:
	js::Vector<std::pair<Key, Value>, 64> buckets;
	BitVector mask;
	HashFunc hash_func;
private:
	size_t num_items;
	size_t hash_mask;
};


void testHashMapInsertOnly();
