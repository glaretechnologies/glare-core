/*=====================================================================
HashSetIterators.h
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Vector.h"


template <typename Key, typename HashFunc> class HashSet;


template <typename Key, typename HashFunc>
class HashSetIterator
{
public:
	HashSetIterator(HashSet<Key, HashFunc>* map_, Key* bucket_)
		: map(map_), bucket(bucket_) {}


	bool operator == (const HashSetIterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const HashSetIterator& other) const
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
		while((bucket < (map->buckets + map->buckets_size)) && (*bucket == map->empty_key));
	}

	Key& operator * ()
	{
		return *bucket;
	}
	const Key& operator * () const
	{
		return *bucket;
	}

	Key* operator -> ()
	{
		return bucket;
	}
	const Key* operator -> () const
	{
		return bucket;
	}

	HashSet<Key, HashFunc>* map;
	Key* bucket;
};


template <typename Key, typename HashFunc>
class ConstHashSetIterator
{
public:
	ConstHashSetIterator(const HashSet<Key, HashFunc>* map_, const Key* bucket_)
		: map(map_), bucket(bucket_) {}

	ConstHashSetIterator(const HashSetIterator<Key, HashFunc>& it)
		: map(it.map), bucket(it.bucket) {}


	bool operator == (const ConstHashSetIterator& other) const
	{
		return bucket == other.bucket;
	}

	bool operator != (const ConstHashSetIterator& other) const
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
		while((bucket < (map->buckets + map->buckets_size)) && (*bucket == map->empty_key));
	}

	const Key& operator * () const
	{
		return *bucket;
	}

	const Key* operator -> () const
	{
		return bucket;
	}

	const HashSet<Key, HashFunc>* map;
	const Key* bucket;
};
