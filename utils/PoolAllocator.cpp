/*=====================================================================
PoolAllocator.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "PoolAllocator.h"


#include "../maths/mathstypes.h"
#include <new>


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "CycleTimer.h"
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
	{
		PoolAllocator<int> pool(/*alignment=*/4);

		void* a = pool.alloc(sizeof(int), 4);
		void* b = pool.alloc(sizeof(int), 4);
		void* c = pool.alloc(sizeof(int), 4);

		testAssert(pool.numAllocatedObs() == 3);

		pool.free(a);
		pool.free(b);
		pool.free(c);

		testAssert(pool.numAllocatedObs() == 0);

		const int N = 100;
		std::vector<void*> ptrs;
		for(int i=0; i<N; ++i)
		{
			ptrs.push_back(pool.alloc(sizeof(int), 4));
		}

		testAssert(pool.numAllocatedObs() == N);

		for(int i=0; i<N; ++i)
		{
			pool.free(ptrs[i]);
		}

		testAssert(pool.numAllocatedObs() == 0);
	}

	// Test with references 
	{
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
	}
}


#endif // BUILD_TESTS
