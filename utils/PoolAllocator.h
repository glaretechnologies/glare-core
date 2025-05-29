/*=====================================================================
PoolAllocator.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "ThreadSafeRefCounted.h"
#include "Mutex.h"
#include "Lock.h"
#include "Vector.h"
#include <set>


/*=====================================================================
PoolAllocator
-------------
Thread-safe.

Uses a set so we can keep free indices sorted by index, and allocate the 
next object at the lowest index, hopefully packing objects closely together.

See also FastPoolAllocator for an allocator that uses a vector instead of 
a set for the free indices.
=====================================================================*/
namespace glare
{


class PoolAllocator : public ThreadSafeRefCounted // public glare::Allocator
{
public:
	// alignment must be a power of 2.
	// block_capacity must be a power of 2.
	PoolAllocator(size_t ob_alloc_size_, size_t alignment_, size_t block_capacity);

	virtual ~PoolAllocator();

	size_t numAllocatedObs() const;

	size_t numAllocatedBlocks() const;

	struct AllocResult
	{
		void* ptr;
		int32 index;
	};

	struct BlockInfo
	{
		uint8* data;
	};

	AllocResult alloc();

	void free(int allocation_index);


	mutable Mutex mutex;
	js::Vector<BlockInfo, 16> blocks	GUARDED_BY(mutex);
	size_t ob_alloc_size, alignment;

	std::set<int> free_indices;

	size_t block_capacity; // Max num objects per block
	size_t block_capacity_num_bits;
	size_t block_capacity_mask;
};


void testPoolAllocator();


} // End namespace glare
