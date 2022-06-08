/*=====================================================================
PoolMap2.h
---------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "BitField.h"
#include "Vector.h"
#include "Exception.h"
#include "PoolAllocator.h"
#include "../maths/mathstypes.h"
#include <unordered_map>
#include <set>
#include <limits>


/*=====================================================================
PoolMap2
--------
A map with faster in-memory-order iteration provided by a set.
=====================================================================*/
namespace glare
{


template <class Key, class Value>
class PoolMap2Iterator
{
public:
	PoolMap2Iterator(typename std::set<Value>::iterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	//Value* getValuePtr() { return (Value*)pool_iterator.pool_vector->getPtrForIterator(pool_iterator); }
	//Value* getValuePtr() { return *pool_iterator; }
	const Value& getValue() { return *pool_iterator; }

	inline bool operator == (const PoolMap2Iterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const PoolMap2Iterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	typename std::set<Value>::iterator pool_iterator;
};


template <class Key, class Value>
class PoolMap2ConstIterator
{
public:
	PoolMap2ConstIterator(typename std::set<Value>::const_iterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	//const Value* getValuePtr() { return (const Value*)pool_iterator.pool_vector->getPtrForConstIterator(pool_iterator); }
	//const Value* getValuePtr() { return *pool_iterator; }
	const Value& getValue() const { return *pool_iterator; }

	inline bool operator == (const PoolMap2ConstIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const PoolMap2ConstIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	typename std::set<Value>::const_iterator pool_iterator;
};


template <class Key, class Value, class KeyHasher>
class PoolMap2
{
public:
	PoolMap2(){}

	~PoolMap2()
	{
		clear();
	}

	void clear()
	{
		values.clear();
		key_info_map.clear();
	}

	void insert(const Key& key, const Value& value)
	{
		auto res = key_info_map.find(key);
		if(res == key_info_map.end())
		{
			key_info_map.insert(std::make_pair(key, value));
			values.insert(value);
		}
		else
			throw glare::Exception("Something with that key already inserted.");
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


	// NOTE: this double lookup sucks.
	PoolMap2Iterator<Key, Value> find(const Key& key)
	{
		std::unordered_map<Key, Value, KeyHasher>::iterator res = key_info_map.find(key);
		if(res == key_info_map.end())
			return end();
		else
		{
			const Value& val = res->second;
			return PoolMap2Iterator<Key, Value>(values.find(val));
		}
		
	}

	typedef PoolMap2Iterator<Key, Value> iterator;
	
	//typedef typename std::set<Value>::iterator iterator;
	
	/*iterator begin()
	{
		return values.begin();
	}

	iterator end()
	{
		return values.end();
	}*/


	PoolMap2Iterator<Key, Value> begin()
	{
		return PoolMap2Iterator<Key, Value>(values.begin());
	}

	PoolMap2ConstIterator<Key, Value> begin() const
	{
		return PoolMap2ConstIterator<Key, Value>(values.begin());
	}

	PoolMap2Iterator<Key, Value> end()
	{
		return PoolMap2Iterator<Key, Value>(values.end());
	}

	PoolMap2ConstIterator<Key, Value> end() const
	{
		return PoolMap2ConstIterator<Key, Value>(values.end());
	}

	size_t size() const { return key_info_map.size(); }

	std::unordered_map<Key, Value, KeyHasher> key_info_map;
	std::set<Value> values;
};


//void testPoolMap2();


} // End namespace glare
