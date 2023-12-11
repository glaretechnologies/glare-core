/*=====================================================================
LinearIterSet.h
---------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "HashMap.h"
#include "Vector.h"


/*=====================================================================
LinearIterSet
-------------
A set with fast iteration over values provided by a vector.

To iterate over values, use begin() and end() or access the vector directly.

Similar to FastIterMap.
=====================================================================*/
namespace glare
{


template <class T, class THasher>
class LinearIterSet
{
public:
	LinearIterSet(T empty_val) : map(empty_val) {}

	~LinearIterSet()
	{
		clear();
	}

	void clear()
	{
		vector.clear();
		map.clear();
	}

	// Returns true if inserted, false otherwise.
	bool insert(const T& elem)
	{
		auto res = map.find(elem);
		if(res == map.end())
		{
			const size_t index = vector.size();
			map.insert(std::make_pair(elem, index));
			vector.push_back(elem);
			invariant();
			return true;
		}
		else
			return false;
	}

	// Returns num elements removed (1 if removed, 0 otherwise)
	size_t erase(const T& elem)
	{
		auto res = map.find(elem);
		if(res != map.end())
		{
			const size_t index = res->second;
			const size_t back_index = vector.size() - 1;
			if(index == back_index)
			{
			}
			else
			{
				const T back_elem = vector[back_index];

				vector[index] = vector[back_index];
				map[back_elem] = index;
			}

			map.erase(elem);
			vector.pop_back();
			invariant();
			return 1;
		}
		else
		{
			invariant();
			return 0;
		}
	}

	typename js::Vector<T, 16>::iterator begin() { return vector.begin(); }
	typename js::Vector<T, 16>::iterator end() { return vector.end(); }

	typename js::Vector<T, 16>::const_iterator begin() const { return vector.begin(); }
	typename js::Vector<T, 16>::const_iterator end() const { return vector.end(); }

	size_t size() const { return map.size(); }

	bool empty() const { return map.empty(); }

	bool contains(const T& elem) const { return map.find(elem) != map.end(); }

	void invariant()
	{
#ifndef NDEBUG
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			const size_t index = it->second;
			assert(index < vector.size());
			assert(vector[index] == it->first);
		}
#endif
	}


	HashMap<T, size_t, THasher> map;
	js::Vector<T, 16> vector;
};


void testLinearIterSet();


} // end namespace glare
