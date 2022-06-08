/*=====================================================================
PoolAllocator.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "MemAlloc.h"
#include "GlareAllocator.h"
#include "Mutex.h"
#include "Lock.h"
#include "Vector.h"
#include "ConPrint.h"
#include "../maths/mathstypes.h"
#include <stddef.h> // For size_t
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
	const static size_t block_capacity = 1024; // Max num objects per block.

	// When calling alloc(), the requested_alignment passed to alloc() must be <= alignment_.
	PoolAllocator(size_t ob_alloc_size_, size_t alignment_)
	:	ob_alloc_size(Maths::roundUpToMultipleOfPowerOf2<size_t>(ob_alloc_size_, alignment_)),
		alignment(alignment_)
	{
		assert(Maths::isPowerOfTwo(alignment_));
	}

	virtual ~PoolAllocator()
	{
		for(size_t i=0; i<blocks.size(); ++i)
			MemAlloc::alignedFree(blocks[i].data);
	}

	size_t numAllocatedObs() const
	{
		Lock lock(mutex);

		assert(blocks.size() * block_capacity >= free_indices.size());
		return blocks.size() * block_capacity - free_indices.size();
	}

	size_t numAllocatedBlocks() const
	{
		Lock lock(mutex);
		return blocks.size();
	}


	struct AllocResult
	{
		void* ptr;
		int32 index;
	};

	struct BlockInfo
	{
		uint8* data;
	};

	virtual AllocResult alloc()
	{
		Lock lock(mutex);

		auto res = free_indices.begin();
		if(res != free_indices.end())
		{
			// There was at least one free index:
			const int index = *res;
			size_t block_i    = (size_t)index / block_capacity;
			size_t in_block_i = (size_t)index % block_capacity;

			free_indices.erase(res); // Remove slot from free indices.

			AllocResult alloc_res;
			alloc_res.ptr = blocks[block_i].data + ob_alloc_size * in_block_i;
			alloc_res.index = index;
			return alloc_res;
		}
		else
		{
			// No free indices - All blocks (if any) are full.  Allocate a new block.
			const size_t new_block_index = blocks.size();
			blocks.push_back(BlockInfo());

			for(int i=1; i<block_capacity; ++i) // Don't add slot 0, we will use that one.
				free_indices.insert((int)(new_block_index * block_capacity + i));

			BlockInfo& new_block = blocks[new_block_index];
			new_block.data = (uint8*)MemAlloc::alignedMalloc(block_capacity * ob_alloc_size, alignment);

			AllocResult alloc_res;
			alloc_res.ptr = new_block.data;
			alloc_res.index = (int)(new_block_index * block_capacity);
			return alloc_res;
		}
	}

	virtual void free(int allocation_index)
	{
		Lock lock(mutex);

		const auto res = free_indices.insert(allocation_index);
		const bool was_inserted = res.second;
		if(!was_inserted) // If it was not inserted, it means it was already in free_indices
		{
			conPrint("Warning: tried to free the same memory slot more than once.");
			assert(0);
		}
	}


	mutable Mutex mutex;
	js::Vector<BlockInfo, 16> blocks;
	size_t ob_alloc_size, alignment;

	std::set<int> free_indices;
};


void testPoolAllocator();


} // End namespace glare
