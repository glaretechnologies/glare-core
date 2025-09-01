/*=====================================================================
FastPoolAllocator.h
-------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "ThreadSafeRefCounted.h"
#include "Mutex.h"
#include "Vector.h"
#include <string>


/*=====================================================================
FastPoolAllocator
-----------------
Thread-safe.

Similar to PoolAllocator, but uses a vector for free_indices instead of a set.
This is faster, but allocated objects may be spread over the block, and not use
the lowest free indices like with a set.
=====================================================================*/
namespace glare
{


class FastPoolAllocator : public ThreadSafeRefCounted
{
public:
	// alignment must be a power of 2.
	// block_capacity must be a power of 2.
	FastPoolAllocator(size_t ob_alloc_size_, size_t alignment, size_t block_capacity);

	virtual ~FastPoolAllocator();

	size_t numAllocatedObs() const;

	size_t numAllocatedBlocks() const;

	struct AllocResult
	{
		void* ptr;
		int index;
	};

	AllocResult alloc();

	void free(int allocation_index);

private:
	struct BlockInfo
	{
		uint8* data;
	};
	mutable Mutex mutex;
	js::Vector<BlockInfo, 16> blocks	GUARDED_BY(mutex);
	size_t ob_alloc_size, alignment;
	
	size_t block_capacity; // Max num objects per block
	size_t block_capacity_num_bits;
	size_t block_capacity_mask;

	js::Vector<int, 16> free_indices;

public:
	std::string name;
};


void testFastPoolAllocator();


} // End namespace glare
