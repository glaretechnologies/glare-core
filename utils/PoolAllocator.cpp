/*=====================================================================
PoolAllocator.cpp
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "PoolAllocator.h"


#include "MemAlloc.h"
#include "StringUtils.h"
#include "ConPrint.h"


glare::PoolAllocator::PoolAllocator(size_t ob_alloc_size_, size_t alignment_, size_t block_capacity_)
:	ob_alloc_size(Maths::roundUpToMultipleOfPowerOf2<size_t>(ob_alloc_size_, alignment_)),
	alignment(alignment_),
	block_capacity(block_capacity_),
	block_capacity_num_bits(BitUtils::highestSetBitIndex(block_capacity_)),
	block_capacity_mask(block_capacity_ - 1)
{
	assert(Maths::isPowerOfTwo(alignment_));
	assert(Maths::isPowerOfTwo(block_capacity_));
}


glare::PoolAllocator::~PoolAllocator()
{
	const size_t num_allocated_obs = numAllocatedObs();
	if(num_allocated_obs > 0)
	{
		conPrint(toString(num_allocated_obs) + " obs still allocated in PoolAllocator destructor!");
		assert(0);
	}

	for(size_t i=0; i<blocks.size(); ++i)
		MemAlloc::alignedFree(blocks[i].data);
}


size_t glare::PoolAllocator::numAllocatedObs() const
{
	Lock lock(mutex);

	assert(blocks.size() * block_capacity >= free_indices.size());
	return blocks.size() * block_capacity - free_indices.size();
}


size_t glare::PoolAllocator::numAllocatedBlocks() const
{
	Lock lock(mutex);
	return blocks.size();
}


glare::PoolAllocator::AllocResult glare::PoolAllocator::alloc()
{
	Lock lock(mutex);

	auto res = free_indices.begin();
	if(res != free_indices.end())
	{
		// There was at least one free index:
		const int index = *res;
		const size_t block_i    = (size_t)index >> block_capacity_num_bits;
		const size_t in_block_i = (size_t)index & block_capacity_mask;
		assert(block_i    == (size_t)index / block_capacity);
		assert(in_block_i == (size_t)index % block_capacity);

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

		for(size_t i=1; i<block_capacity; ++i) // Don't add slot 0, we will use that one.
			free_indices.insert((int)(new_block_index * block_capacity + i));

		BlockInfo& new_block = blocks[new_block_index];
		new_block.data = (uint8*)MemAlloc::alignedMalloc(block_capacity * ob_alloc_size, alignment);

		AllocResult alloc_res;
		alloc_res.ptr = new_block.data;
		alloc_res.index = (int)(new_block_index * block_capacity);
		return alloc_res;
	}
}


void glare::PoolAllocator::free(int allocation_index)
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


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Reference.h"


/*struct PoolAllocatorTestStruct : public RefCounted
{
	int a, b, c, d;

	Reference<glare::Allocator> allocator;
};


// Template specialisation of destroyAndFreeOb for WMFFrameInfo.
template <>
inline void destroyAndFreeOb<PoolAllocatorTestStruct>(PoolAllocatorTestStruct* ob)
{
	Reference<glare::Allocator> allocator = ob->allocator;

	// Destroy object
	ob->~PoolAllocatorTestStruct();

	// Free object mem
	if(allocator.nonNull())
		ob->allocator->free(ob);
	else
		delete ob;
}*/


void glare::testPoolAllocator()
{
	conPrint("glare::testPoolAllocator()");

	{
		PoolAllocator pool(/*ob alloc size=*/4, /*alignment=*/4, /*block capacity=*/16);

		PoolAllocator::AllocResult a = pool.alloc();
		PoolAllocator::AllocResult b = pool.alloc();
		PoolAllocator::AllocResult c = pool.alloc();

		testAssert(pool.numAllocatedObs() == 3);

		pool.free(a.index);
		pool.free(b.index);
		pool.free(c.index);

		testAssert(pool.numAllocatedObs() == 0);

		const int N = 10000;
		std::vector<int> indices;
		for(int i=0; i<N; ++i)
		{
			PoolAllocator::AllocResult res = pool.alloc();
			indices.push_back(res.index);
		}

		testAssert(pool.numAllocatedObs() == N);

		for(int i=0; i<N; ++i)
		{
			pool.free(indices[i]);
		}

		testAssert(pool.numAllocatedObs() == 0);
	}

	// Test with references 
	/*{
		Reference<PoolAllocator<PoolAllocatorTestStruct>> pool = new PoolAllocator<PoolAllocatorTestStruct>(16);
		
		testAssert(pool->numAllocatedObs() == 0);

		{
			void* mem = pool->alloc(sizeof(PoolAllocatorTestStruct), 16);
			Reference<PoolAllocatorTestStruct> a = ::new (mem) PoolAllocatorTestStruct();
			a->allocator = pool;

			testAssert(pool->numAllocatedObs() == 1);
		}
		testAssert(pool->numAllocatedObs() == 0);

		testAssert(pool->getRefCount() == 1);
	}*/

	conPrint("glare::testPoolAllocator() done.");
}


#endif // BUILD_TESTS
