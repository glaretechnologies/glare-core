/*=====================================================================
FastPoolAllocator.h
-------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "ThreadSafeRefCounted.h"
#include "Mutex.h"
#include "Lock.h"
#include "Vector.h"
#include <vector>


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
	FastPoolAllocator(size_t ob_alloc_size_, size_t alignment_);

	virtual ~FastPoolAllocator();

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

	virtual AllocResult alloc();

	virtual void free(int allocation_index);


	mutable Mutex mutex;
	js::Vector<BlockInfo, 16> blocks	GUARDED_BY(mutex);
	size_t ob_alloc_size, alignment;

	std::vector<int> free_indices;
};


void testFastPoolAllocator();


} // End namespace glare
