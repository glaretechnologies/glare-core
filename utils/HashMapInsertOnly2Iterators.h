/*=====================================================================
HashMapInsertOnly2Iterator.h
----------------------------
Copyright Glare Technologies Limited 2017 -
=====================================================================*/
#pragma once


#include "Vector.h"


template <typename Key, typename Value, typename HashFunc> class HashMapInsertOnly2;


template <typename Key, typename Value, typename HashFunc>
class HashMapInsertOnly2Iterator
{
public:
	HashMapInsertOnly2Iterator(HashMapInsertOnly2<Key, Value, HashFunc>* map_, std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}


	bool operator == (const HashMapInsertOnly2Iterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const HashMapInsertOnly2Iterator& other) const
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

	HashMapInsertOnly2<Key, Value, HashFunc>* map;
	std::pair<Key, Value>* bucket;
};


template <typename Key, typename Value, typename HashFunc>
class ConstHashMapInsertOnly2Iterator
{
public:
	ConstHashMapInsertOnly2Iterator(const HashMapInsertOnly2<Key, Value, HashFunc>* map_, const std::pair<Key, Value>* bucket_)
		: map(map_), bucket(bucket_) {}

	ConstHashMapInsertOnly2Iterator(const HashMapInsertOnly2Iterator<Key, Value, HashFunc>& it)
		: map(it.map), bucket(it.bucket) {}


	bool operator == (const ConstHashMapInsertOnly2Iterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const ConstHashMapInsertOnly2Iterator& other) const
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

	const HashMapInsertOnly2<Key, Value, HashFunc>* map;
	const std::pair<Key, Value>* bucket;
};
