/*=====================================================================
PoolAllocator.cpp
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "PoolAllocator.h"


#include "../maths/mathstypes.h"
#include <new>


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Reference.h"


struct PoolAllocatorTestStruct : public RefCounted
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
}

void glare::testPoolAllocator()
{
	conPrint("glare::testPoolAllocator()");

	{
		PoolAllocator pool(/*ob alloc size=*/4, /*alignment=*/4);

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
