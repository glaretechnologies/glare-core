/*=====================================================================
HashMapIterators.h
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Vector.h"


template <typename Key, typename Value, typename HashFunc> class HashMap;


template <typename Key, typename Value, typename HashFunc>
class HashMapIterator
{
public:
	HashMapIterator(HashMap<Key, Value, HashFunc>* map_, std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}


	bool operator == (const HashMapIterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const HashMapIterator& other) const
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
		while((bucket < (map->buckets + map->buckets_size)) && (bucket->first == map->empty_key));
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

	HashMap<Key, Value, HashFunc>* map;
	std::pair<Key, Value>* bucket;
};


template <typename Key, typename Value, typename HashFunc>
class ConstHashMapIterator
{
public:
	ConstHashMapIterator(const HashMap<Key, Value, HashFunc>* map_, const std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}

	ConstHashMapIterator(const HashMapIterator<Key, Value, HashFunc>& it)
		: map(it.map), bucket(it.bucket) {}


	bool operator == (const ConstHashMapIterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const ConstHashMapIterator& other) const
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
		while((bucket < (map->buckets + map->buckets_size)) && (bucket->first == map->empty_key));
	}

	const std::pair<Key, Value>& operator * () const
	{
		return *bucket;
	}

	const std::pair<Key, Value>* operator -> () const
	{
		return bucket;
	}

	const HashMap<Key, Value, HashFunc>* map;
	const std::pair<Key, Value>* bucket;
};
