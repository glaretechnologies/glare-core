/*=====================================================================
FastIterMap.h
-------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"
#include "HashMap.h"
#include "Vector.h"


/*=====================================================================
FastIterMap
-----------
A map with fast iteration over values provided by a vector.

To iterate over values, use valuesBegin() and valuesEnd()
=====================================================================*/
namespace glare
{


template <class Key, class Value>
struct FastIterMapValueInfo
{
	Key key;
	Value value;
};


template <class Key, class Value, class KeyHasher>
class FastIterMapLookUpIterator
{
public:
	explicit FastIterMapLookUpIterator(js::Vector<FastIterMapValueInfo<Key, Value>, 16>* vector_, typename HashMap<Key, size_t, KeyHasher>::iterator it_) : vector(vector_), it(it_) {}

	const Value& getValue() { return (*vector)[it->second].value; }

	inline bool operator == (const FastIterMapLookUpIterator<Key, Value, KeyHasher>& other) const { return it == other.it; }
	inline bool operator != (const FastIterMapLookUpIterator<Key, Value, KeyHasher>& other) const { return it != other.it; }

	js::Vector<FastIterMapValueInfo<Key, Value>, 16>* vector;
	typename HashMap<Key, size_t, KeyHasher>::iterator it;
};


template <class Key, class Value>
class FastIterMapValueIterator
{
public:
	explicit FastIterMapValueIterator(typename js::Vector<FastIterMapValueInfo<Key, Value>, 16>::iterator it_) : it(it_) {}
	
	const Value& getValue() { return it->value; }

	inline bool operator == (const FastIterMapValueIterator<Key, Value>& other) const { return it == other.it; }
	inline bool operator != (const FastIterMapValueIterator<Key, Value>& other) const { return it != other.it; }

	inline void operator ++ () // Prefix increment operator
	{
		++it;
	}

	typename js::Vector<FastIterMapValueInfo<Key, Value>, 16>::iterator it;
};


template <class Key, class Value>
class FastIterMapValueConstIterator
{
public:
	explicit FastIterMapValueConstIterator(typename js::Vector<FastIterMapValueInfo<Key, Value>, 16>::const_iterator it_) : it(it_) {}

	const Value& getValue() const { return it->value; }

	inline bool operator == (const FastIterMapValueConstIterator<Key, Value>& other) const { return it == other.it; }
	inline bool operator != (const FastIterMapValueConstIterator<Key, Value>& other) const { return it != other.it; }

	inline void operator ++ () // Prefix increment operator
	{
		++it;
	}

	typename js::Vector<FastIterMapValueInfo<Key, Value>, 16>::const_iterator it;
};


template <class Key, class Value, class KeyHasher>
class FastIterMap
{
public:
	FastIterMap(Key empty_key) : key_info_map(empty_key) {}

	~FastIterMap()
	{
		clear();
	}

	void clear()
	{
		vector.clear();
		key_info_map.clear();
	}

	// Returns true if inserted, false otherwise.
	bool insert(const Key& key, const Value& value)
	{
		auto res = key_info_map.find(key);
		if(res == key_info_map.end())
		{
			const size_t index = vector.size();
			key_info_map.insert(std::make_pair(key, index));
			vector.push_back(FastIterMapValueInfo<Key, Value>({key, value}));
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
			const size_t index = res->second;
			const size_t back_index = vector.size() - 1;
			if(index == back_index)
			{
			}
			else
			{
				// Move vector[back_index] to vector[index]
				vector[index] = vector[back_index];

				// Update map so that map[back_elem_key] points at new index.
				const Key back_elem_key = vector[back_index].key;
				key_info_map[back_elem_key] = index;
			}

			key_info_map.erase(key);
			vector.pop_back();
		}
	}

	size_t size() const { return key_info_map.size(); }

	bool contains(const Key& key) const { return key_info_map.find(key) != key_info_map.end(); }

	FastIterMapLookUpIterator<Key, Value, KeyHasher> find(const Key& key)
	{
		return FastIterMapLookUpIterator<Key, Value, KeyHasher>(&vector, key_info_map.find(key));
	}

	// For key lookups:
	// typedef typename std::unordered_map<Key, Value, KeyHasher> iterator;

	FastIterMapLookUpIterator<Key, Value, KeyHasher> end()
	{
		return FastIterMapLookUpIterator<Key, Value, KeyHasher>(&vector, key_info_map.end());
	}


	// For iterating over values:
	FastIterMapValueIterator<Key, Value> valuesBegin()
	{
		return FastIterMapValueIterator<Key, Value>(vector.begin());
	}

	FastIterMapValueConstIterator<Key, Value> valuesBegin() const
	{
		return FastIterMapValueConstIterator<Key, Value>(vector.begin());
	}

	FastIterMapValueIterator<Key, Value> valuesEnd()
	{
		return FastIterMapValueIterator<Key, Value>(vector.end());
	}

	FastIterMapValueConstIterator<Key, Value> valuesEnd() const
	{
		return FastIterMapValueConstIterator<Key, Value>(vector.end());
	}


	HashMap<Key, size_t, KeyHasher> key_info_map;
	js::Vector<FastIterMapValueInfo<Key, Value>, 16> vector;
};


void testFastIterMap();


} // End namespace glare
