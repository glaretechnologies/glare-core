/*=====================================================================
FastIterMap.h
-------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"
#include <unordered_map>
#include <set>


/*=====================================================================
FastIterMap
-----------
A map with faster in-memory-order iteration over values provided by a set.
Value comparison order should correspond to memory order (e.g. Value should be a pointer or Reference type)

To iterate over values, use valuesBegin() and valuesEnd()
=====================================================================*/
namespace glare
{


template <class Key, class Value, class KeyHasher>
class FastIterMapLookUpIterator
{
public:
	explicit FastIterMapLookUpIterator(typename std::unordered_map<Key, Value, KeyHasher>::iterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	const Value& getValue() { return pool_iterator->second; }

	inline bool operator == (const FastIterMapLookUpIterator<Key, Value, KeyHasher>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const FastIterMapLookUpIterator<Key, Value, KeyHasher>& other) const { return pool_iterator != other.pool_iterator; }

	typename std::unordered_map<Key, Value, KeyHasher>::iterator pool_iterator;
};


template <class Key, class Value>
class FastIterMapValueIterator
{
public:
	explicit FastIterMapValueIterator(typename std::set<Value>::iterator pool_iterator_) : pool_iterator(pool_iterator_) {}
	
	const Value& getValue() { return *pool_iterator; }

	inline bool operator == (const FastIterMapValueIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const FastIterMapValueIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	typename std::set<Value>::iterator pool_iterator;
};


template <class Key, class Value>
class FastIterMapValueConstIterator
{
public:
	explicit FastIterMapValueConstIterator(typename std::set<Value>::const_iterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	const Value& getValue() const { return *pool_iterator; }

	inline bool operator == (const FastIterMapValueConstIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const FastIterMapValueConstIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	typename std::set<Value>::const_iterator pool_iterator;
};


template <class Key, class Value, class KeyHasher>
class FastIterMap
{
public:
	FastIterMap(){}

	~FastIterMap()
	{
		clear();
	}

	void clear()
	{
		values.clear();
		key_info_map.clear();
	}

	// Returns true if inserted, false otherwise.
	bool insert(const Key& key, const Value& value)
	{
		auto res = key_info_map.find(key);
		if(res == key_info_map.end())
		{
			key_info_map.insert(std::make_pair(key, value));
			values.insert(value);
			return true;
		}
		else
			return false;
	}

	void erase(const Key& key)
	{
		auto res = key_info_map.find(key);
		if(res != key_info_map.end())
		{
			Value value = res->second;

			key_info_map.erase(res);
			values.erase(value);
		}
	}

	FastIterMapLookUpIterator<Key, Value, KeyHasher> find(const Key& key)
	{
		return FastIterMapLookUpIterator<Key, Value, KeyHasher>(key_info_map.find(key));
	}

	size_t size() const { return key_info_map.size(); }


	// For key lookups:
	// typedef typename std::unordered_map<Key, Value, KeyHasher> iterator;

	FastIterMapLookUpIterator<Key, Value, KeyHasher> end()
	{
		return FastIterMapLookUpIterator<Key, Value, KeyHasher>(key_info_map.end());
	}


	// For iterating over values:
	FastIterMapValueIterator<Key, Value> valuesBegin()
	{
		return FastIterMapValueIterator<Key, Value>(values.begin());
	}

	FastIterMapValueConstIterator<Key, Value> valuesBegin() const
	{
		return FastIterMapValueConstIterator<Key, Value>(values.begin());
	}

	FastIterMapValueIterator<Key, Value> valuesEnd()
	{
		return FastIterMapValueIterator<Key, Value>(values.end());
	}

	FastIterMapValueConstIterator<Key, Value> valuesEnd() const
	{
		return FastIterMapValueConstIterator<Key, Value>(values.end());
	}

	

	std::unordered_map<Key, Value, KeyHasher> key_info_map;
	std::set<Value> values;
};


void testFastIterMap();


} // End namespace glare
