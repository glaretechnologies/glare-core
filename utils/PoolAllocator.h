/*=====================================================================
PoolAllocator.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "MemAlloc.h"
#include "GlareAllocator.h"
#include "BitField.h"
#include "Mutex.h"
#include "Lock.h"
#include <stddef.h> // For size_t
#include <vector>


/*=====================================================================
PoolAllocator
-------------
Thread-safe.
=====================================================================*/
namespace glare
{


class BlockInfo
{
public:
	BlockInfo() : used(0) {}

	BitField<uint32> used; // A bitmask where 0 = slot not used, 1 = slot used.
	uint8* data;
};


template <class T>
class PoolAllocator : public glare::Allocator
{
public:
	const size_t block_capacity = 16; // Max num objects per block.

	// When calling alloc(), the requested_alignemnt passed to alloc() must be <= alignment_.
	PoolAllocator(size_t alignment_)
	:	alignment(alignment_)
	{}

	virtual ~PoolAllocator()
	{
		for(size_t i=0; i<blocks.size(); ++i)
			MemAlloc::alignedFree(blocks[i].data);
	}

	size_t numAllocatedObs() const
	{
		Lock lock(mutex);

		size_t c = 0;
		for(size_t i=0; i<blocks.size(); ++i)
		{
			const BlockInfo& block = blocks[i];
			for(size_t z=0; z<block_capacity; ++z) // TODO: use popcount
				if(block.used.getBit((uint32)z))
					c++;
		}
		return c;
	}

	size_t numAllocatedBlocks() const
	{
		Lock lock(mutex);

		return blocks.size();
	}

	virtual void* alloc(size_t size, size_t requested_alignment)
	{
		assert(requested_alignment <= this->alignment);
		assert(size == sizeof(T));
		const size_t ob_mem_usage = Maths::roundUpToMultipleOfPowerOf2(size, alignment);

		Lock lock(mutex);

		for(size_t i=0; i<blocks.size(); ++i)
		{
			BlockInfo& block = blocks[i];

			if(block.used.v != ((1u << block_capacity) - 1u)) // If block is not entirely used:
			{
				const uint32 free_index = BitUtils::lowestZeroBitIndex(block.used.v); // Get first availabile free slot in block
				assert(block.used.getBit(free_index) == 0);

				block.used.setBitToOne(free_index); // Mark spot in block as used
				return block.data + ob_mem_usage * free_index;
			}
		}

		// No free room in any existing blocks.
		blocks.push_back(BlockInfo());
		blocks.back().data = (uint8*)MemAlloc::alignedMalloc(block_capacity * ob_mem_usage, alignment);
		blocks.back().used.setBitToOne(0); // Mark spot in block as used
		return blocks.back().data;
	}

	virtual void free(void* ptr)
	{
		const size_t ob_mem_usage = Maths::roundUpToMultipleOfPowerOf2(sizeof(T), alignment);

		Lock lock(mutex);

		for(size_t i=0; i<blocks.size(); ++i)
		{
			BlockInfo& block = blocks[i];
			if(ptr >= block.data && ptr < block.data + block_capacity * ob_mem_usage) // If ptr lies within memory range for the block:
			{
				// ptr was from this block
				const size_t index = ((uint8*)ptr - block.data) / ob_mem_usage;
				assert(ptr == block.data + index * ob_mem_usage);
				block.used.setBitToZero((uint32)index); // Mark spot as not-used.
				return;
			}
		}

		assert(0); // Memory was not found in any block!
	}


	mutable Mutex mutex;
	std::vector<BlockInfo> blocks;
	size_t alignment;
};


void testPoolAllocator();


} // End namespace glare
