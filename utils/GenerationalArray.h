/*=====================================================================
GenerationalArray.h
-------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Reference.h"
#include <vector>


namespace glare
{

template <class T>
class GenerationalHandle
{
public:
	GenerationalHandle() : index(18446744073709551615ull), gen(0) {}
	GenerationalHandle(size_t index_, uint64 gen_) : index(index_), gen(gen_) {}
	size_t index;
	uint64 gen;
};


template <class T>
class GenerationalArraySlot
{
public:
	Reference<T> ob;
	uint64 gen;
};


/*=====================================================================
GenerationalArray
-----------------

=====================================================================*/
template <class T>
class GenerationalArray
{
public:
	GenerationalArray() : gen(0) {}

	GenerationalHandle<T> insert(Reference<T> ref)
	{
		const size_t handle_gen = gen++;

		const size_t index = getFreeIndex();
		vec[index].ob = ref;
		vec[index].gen = handle_gen;

		return GenerationalHandle<T>(index, handle_gen);
	}

	// Handle should be valid
	void erase(GenerationalHandle<T> handle)
	{
		assert(handle.gen == vec[handle.index].gen); // If the handle is valid, the handle generation should be current.

		vec[handle.index].ob = NULL;
		vec[handle.index].gen = gen++;

		// Add slot index to free list
		free_list.push_back(handle.index);
	}

	Reference<T> getRefForHandle(GenerationalHandle<T> handle)
	{
		if(vec[handle.index].gen == handle.gen)
			return vec[handle.index].ob;
		else
			return NULL;
	}
	
private:
	const size_t getFreeIndex()
	{
		// return from free list
		if(free_list.empty())
		{
			// Resize array
			const size_t old_size = vec.size();
			const size_t new_num = myMax((size_t)16, old_size);
			vec.resize(old_size + new_num);

			free_list.resize(new_num);
			for(size_t i=0; i<new_num; ++i)
				free_list[i] = old_size + i;
		}
		const size_t free_index = free_list.back();
		free_list.pop_back();
		return free_index;
	}

	uint64 gen;
	std::vector<GenerationalArraySlot<T> > vec;
	std::vector<size_t> free_list;
};


void testGenerationalArray();


} // End namespace glare
