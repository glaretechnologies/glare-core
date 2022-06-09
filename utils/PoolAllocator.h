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
=====================================================================*/
namespace glare
{


class PoolAllocator : public ThreadSafeRefCounted // public glare::Allocator
{
public:
	PoolAllocator(size_t ob_alloc_size_, size_t alignment_);

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

	virtual AllocResult alloc();

	virtual void free(int allocation_index);


	mutable Mutex mutex;
	js::Vector<BlockInfo, 16> blocks;
	size_t ob_alloc_size, alignment;

	std::set<int> free_indices;
};


void testPoolAllocator();


} // End namespace glare
