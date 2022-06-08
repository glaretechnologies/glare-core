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
A map with faster in-memory-order iteration provided by a set.
Value comparison order should correspond to memory order (e.g. Value should be a pointer or Reference type)
=====================================================================*/
namespace glare
{


template <class Key, class Value>
class FastIterMapIterator
{
public:
	FastIterMapIterator(typename std::set<Value>::iterator pool_iterator_) : pool_iterator(pool_iterator_) {}
	
	const Value& getValue() { return *pool_iterator; }

	inline bool operator == (const FastIterMapIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const FastIterMapIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	typename std::set<Value>::iterator pool_iterator;
};


template <class Key, class Value>
class FastIterMapConstIterator
{
public:
	FastIterMapConstIterator(typename std::set<Value>::const_iterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	const Value& getValue() const { return *pool_iterator; }

	inline bool operator == (const FastIterMapConstIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const FastIterMapConstIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

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


	// NOTE: this double lookup sucks.
	FastIterMapIterator<Key, Value> find(const Key& key)
	{
		std::unordered_map<Key, Value, KeyHasher>::iterator res = key_info_map.find(key);
		if(res == key_info_map.end())
			return end();
		else
		{
			const Value& val = res->second;
			return FastIterMapIterator<Key, Value>(values.find(val));
		}
		
	}

	// For iterating over values:
	typedef FastIterMapIterator<Key, Value> iterator;
	
	//typedef typename std::set<Value>::iterator iterator;
	
	/*iterator begin()
	{
		return values.begin();
	}

	iterator end()
	{
		return values.end();
	}*/

	
	FastIterMapIterator<Key, Value> begin()
	{
		return FastIterMapIterator<Key, Value>(values.begin());
	}

	FastIterMapConstIterator<Key, Value> begin() const
	{
		return FastIterMapConstIterator<Key, Value>(values.begin());
	}

	FastIterMapIterator<Key, Value> end()
	{
		return FastIterMapIterator<Key, Value>(values.end());
	}

	FastIterMapConstIterator<Key, Value> end() const
	{
		return FastIterMapConstIterator<Key, Value>(values.end());
	}

	size_t size() const { return key_info_map.size(); }

	std::unordered_map<Key, Value, KeyHasher> key_info_map;
	std::set<Value> values;
};


void testFastIterMap();


} // End namespace glare
