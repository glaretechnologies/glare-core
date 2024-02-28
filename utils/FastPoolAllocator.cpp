/*=====================================================================
FastPoolAllocator.cpp
---------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "FastPoolAllocator.h"


#include "MemAlloc.h"
#include "StringUtils.h"
#include "ConPrint.h"


static const size_t block_capacity = 1024; // Max num objects per block.


glare::FastPoolAllocator::FastPoolAllocator(size_t ob_alloc_size_, size_t alignment_)
:	ob_alloc_size(Maths::roundUpToMultipleOfPowerOf2<size_t>(ob_alloc_size_, alignment_)),
	alignment(alignment_)
{
	assert(Maths::isPowerOfTwo(alignment_));
}


glare::FastPoolAllocator::~FastPoolAllocator()
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
		const int index = free_indices[0];
		size_t block_i    = (size_t)index / block_capacity;
		size_t in_block_i = (size_t)index % block_capacity;

		// Remove slot from free indices.
		free_indices[0] = free_indices.back(); // Replace with last free index
		free_indices.pop_back(); // Remove old last free index

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

		size_t insert_index = free_indices.size();
		free_indices.resize(insert_index + block_capacity - 1);
		for(size_t i=1; i<block_capacity; ++i) // Don't add slot 0, we will use that one.
			free_indices[insert_index + i - 1] = (int)(new_block_index * block_capacity + i);

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

	free_indices.push_back(allocation_index);

	// TODO: issue warning if allocation_index is already in free_indices? I.e. if object is been freed twice.
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


void glare::testFastPoolAllocator()
{
	conPrint("glare::testFastPoolAllocator()");

	{
		FastPoolAllocator pool(/*ob alloc size=*/4, /*alignment=*/4);

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

	conPrint("glare::FastPoolAllocator() done.");
}


#endif // BUILD_TESTS
