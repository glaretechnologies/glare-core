/*=====================================================================
HashMap2Iterators.h
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Vector.h"


template <typename Key, typename Value, typename HashFunc> class HashMap2;


template <typename Key, typename Value, typename HashFunc>
class HashMap2Iterator
{
public:
	HashMap2Iterator(HashMap2<Key, Value, HashFunc>* map_, std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}


	bool operator == (const HashMap2Iterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const HashMap2Iterator& other) const
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
		while((bucket < (map->buckets + map->buckets_size)) && (map->mask.getBitMasked(bucket - map->buckets) == 0));
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

	HashMap2<Key, Value, HashFunc>* map;
	std::pair<Key, Value>* bucket;
};


template <typename Key, typename Value, typename HashFunc>
class ConstHashMap2Iterator
{
public:
	ConstHashMap2Iterator(const HashMap2<Key, Value, HashFunc>* map_, const std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}

	ConstHashMap2Iterator(const HashMap2Iterator<Key, Value, HashFunc>& it)
		: map(it.map), bucket(it.bucket) {}


	bool operator == (const ConstHashMap2Iterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const ConstHashMap2Iterator& other) const
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
		while((bucket < (map->buckets + map->buckets_size)) && (map->mask.getBitMasked(bucket - map->buckets) == 0));
	}

	const std::pair<Key, Value>& operator * () const
	{
		return *bucket;
	}

	const std::pair<Key, Value>* operator -> () const
	{
		return bucket;
	}

	const HashMap2<Key, Value, HashFunc>* map;
	const std::pair<Key, Value>* bucket;
};
