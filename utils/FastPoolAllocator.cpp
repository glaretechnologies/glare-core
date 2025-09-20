/*=====================================================================
FastPoolAllocator.cpp
---------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "FastPoolAllocator.h"


#include "MemAlloc.h"
#include "Lock.h"
#include "StringUtils.h"
#include "ConPrint.h"
#include "BitUtils.h"


glare::FastPoolAllocator::FastPoolAllocator(size_t ob_alloc_size_, size_t alignment_, size_t block_capacity_)
:	ob_alloc_size(Maths::roundUpToMultipleOfPowerOf2<size_t>(ob_alloc_size_, alignment_)),
	alignment(alignment_),
	block_capacity(block_capacity_),
	block_capacity_num_bits(BitUtils::highestSetBitIndex((uint64)block_capacity_)),
	block_capacity_mask(block_capacity_ - 1)
{
	assert(Maths::isPowerOfTwo(alignment_));
	assert(Maths::isPowerOfTwo(block_capacity_));
}


glare::FastPoolAllocator::~FastPoolAllocator()
{
	const size_t num_allocated_obs = numAllocatedObs();
	if(num_allocated_obs > 0)
	{
		conPrint(toString(num_allocated_obs) + " ob(s) still allocated in FastPoolAllocator (allocator name: '" + name + "') destructor!");
		assert(0);
	}

	for(size_t i=0; i<blocks.size(); ++i)
		MemAlloc::alignedFree(blocks[i].data);
}


size_t glare::FastPoolAllocator::numAllocatedObs() const
{
	Lock lock(mutex);

	assert(blocks.size() * block_capacity >= free_indices.size());
	return blocks.size() * block_capacity - free_indices.size();
}


size_t glare::FastPoolAllocator::numAllocatedBlocks() const
{
	Lock lock(mutex);
	return blocks.size();
}


glare::FastPoolAllocator::AllocResult glare::FastPoolAllocator::alloc()
{
	Lock lock(mutex);

	if(free_indices.size() > 0)
	{
		// There was at least one free index:
		const int index = free_indices.back();
		const size_t block_i    = (size_t)index >> block_capacity_num_bits;
		const size_t in_block_i = (size_t)index & block_capacity_mask;
		assert(block_i    == (size_t)index / block_capacity);
		assert(in_block_i == (size_t)index % block_capacity);

		free_indices.pop_back();

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

		const size_t insert_index = free_indices.size();
		free_indices.reserve(insert_index + block_capacity); // Reserve block_capacity extra space so we don't realloc more space when free() is called and slot 0 is added to free_indices.
		free_indices.resize(insert_index + block_capacity - 1);

		// Add in reverse order so that the lowest slots are at the back of free_indices, where we will pop first.
		for(size_t i=1; i<block_capacity; ++i) // Don't add slot 0, we will use that one.
			free_indices[insert_index + i - 1] = (int)(new_block_index * block_capacity + (block_capacity - i));

		BlockInfo& new_block = blocks[new_block_index];
		new_block.data = (uint8*)MemAlloc::alignedMalloc(block_capacity * ob_alloc_size, alignment);

		AllocResult alloc_res;
		alloc_res.ptr = new_block.data;
		alloc_res.index = (int)(new_block_index * block_capacity);
		return alloc_res;
	}
}


void glare::FastPoolAllocator::free(int allocation_index)
{
	Lock lock(mutex);

	assert(allocation_index >= 0 && allocation_index < blocks.size() * block_capacity);

	free_indices.push_back(allocation_index);

	// TODO: issue warning if allocation_index is already in free_indices? I.e. if object is been freed twice.
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "Timer.h"
#include "Reference.h"


void glare::testFastPoolAllocator()
{
	conPrint("glare::testFastPoolAllocator()");

	{
		FastPoolAllocator pool(/*ob alloc size=*/4, /*alignment=*/4, /*block capacity=*/64);

		FastPoolAllocator::AllocResult a = pool.alloc();
		FastPoolAllocator::AllocResult b = pool.alloc();
		FastPoolAllocator::AllocResult c = pool.alloc();

		testAssert(pool.numAllocatedObs() == 3);

		pool.free(a.index);
		pool.free(b.index);
		pool.free(c.index);

		testAssert(pool.numAllocatedObs() == 0);

		const int N = 10000;
		std::vector<int> indices;
		for(int i=0; i<N; ++i)
		{
			FastPoolAllocator::AllocResult res = pool.alloc();
			indices.push_back(res.index);
		}

		testAssert(pool.numAllocatedObs() == N);

		for(int i=0; i<N; ++i)
		{
			pool.free(indices[i]);
		}

		testAssert(pool.numAllocatedObs() == 0);
	}

	// Check free_indices capacity == block capacity after initial alloc() and free().
	{
		FastPoolAllocator pool(/*ob alloc size=*/4, /*alignment=*/4, /*block capacity=*/64);

		FastPoolAllocator::AllocResult a = pool.alloc();
		pool.free(a.index);

		testAssert(pool.numAllocatedObs() == 0);
		testAssert(pool.numAllocatedBlocks() == 1);
		testAssert(pool.free_indices.capacity() == 64);
	}

	if(false)
	{
		FastPoolAllocator pool(/*ob alloc size=*/4, /*alignment=*/4, /*block capacity=*/64);

		std::vector<int> indices;

		for(int z=0; z<10; ++z)
		{
			conPrint("-----");
			const int N = 10 + z;
			for(int i=0; i<N; ++i)
			{
				FastPoolAllocator::AllocResult res = pool.alloc();
				conPrint("Allocated index " + toString(res.index));
				indices.push_back(res.index);
			}

			testAssert(pool.numAllocatedObs() == N);

			for(int i=0; i<N; ++i)
				pool.free(indices[i]);
			indices.clear();

			testAssert(pool.numAllocatedObs() == 0);
		}
	}

	conPrint("glare::testFastPoolAllocator() done.");
}


#endif // BUILD_TESTS
