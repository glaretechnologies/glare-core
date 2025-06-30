/*=====================================================================
HashMapInsertOnlyIterators.h
----------------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Vector.h"
#include "BitVector.h"


template <typename Key, typename Value, typename HashFunc> class HashMapInsertOnly;


template <typename Key, typename Value, typename HashFunc>
class HashMapInsertOnlyIterator
{
public:
	HashMapInsertOnlyIterator(HashMapInsertOnly<Key, Value, HashFunc>* map_, std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}


	bool operator == (const HashMapInsertOnlyIterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const HashMapInsertOnlyIterator& other) const
	{
		return bucket != other.bucket;
	}

	void operator ++ ()
	{
		// Advance to next non-empty bucket
		do
		{
			bucket++;
		}
		while((bucket < map->buckets.end()) && (map->mask.getBitMasked((size_t)(bucket - map->buckets.begin())) == 0));
	}

	std::pair<Key, Value>& operator * ()
	{
		return *bucket;
	}
	const std::pair<Key, Value>& operator * () const
	{
		return *bucket;
	}

	std::pair<Key, Value>* operator -> ()
	{
		return bucket;
	}
	const std::pair<Key, Value>* operator -> () const
	{
		return bucket;
	}

	HashMapInsertOnly<Key, Value, HashFunc>* map;
	std::pair<Key, Value>* bucket;
};


template <typename Key, typename Value, typename HashFunc>
class ConstHashMapInsertOnlyIterator
{
public:
	ConstHashMapInsertOnlyIterator(const HashMapInsertOnly<Key, Value, HashFunc>* map_, const std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}

	ConstHashMapInsertOnlyIterator(const HashMapInsertOnlyIterator<Key, Value, HashFunc>& it)
		: map(it.map), bucket(it.bucket) {}


	bool operator == (const ConstHashMapInsertOnlyIterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const ConstHashMapInsertOnlyIterator& other) const
	{
		return bucket != other.bucket;
	}

	void operator ++ ()
	{
		// Advance to next non-empty bucket
		do
		{
			bucket++;
		}
		while((bucket < map->buckets.end()) && (map->mask.getBitMasked((size_t)(bucket - map->buckets.begin())) == 0));
	}

	const std::pair<Key, Value>& operator * () const
	{
		return *bucket;
	}

	const std::pair<Key, Value>* operator -> () const
	{
		return bucket;
	}

	const HashMapInsertOnly<Key, Value, HashFunc>* map;
	const std::pair<Key, Value>* bucket;
};
