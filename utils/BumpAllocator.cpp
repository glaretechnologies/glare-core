/*=====================================================================
BumpAllocator.cpp
-----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "BumpAllocator.h"


#include "TestUtils.h"
#include "ConPrint.h"


void glare::BumpAllocator::test()
{
	conPrint("glare::BumpAllocator::test()");

	{
		BumpAllocator allocator(1024);
		void* ptr = allocator.alloc(/*size=*/1, /*alignment=*/16);
		testAssert((uint64)ptr % 16 == 0);
		testAssert(allocator.offsets.size() == 1);

		allocator.free(ptr);
		testAssert(allocator.offsets.size() == 0);

		void* ptr_1 = allocator.alloc(/*size=*/100, /*alignment=*/16);
		testAssert((uint64)ptr_1 % 16 == 0);
		void* ptr_2 = allocator.alloc(/*size=*/200, /*alignment=*/16);
		testAssert((uint64)ptr_2 % 16 == 0);
		void* ptr_3 = allocator.alloc(/*size=*/300, /*alignment=*/16);
		testAssert((uint64)ptr_3 % 16 == 0);

		testAssert(allocator.offsets.size() == 3);

		allocator.free(ptr_3);
		allocator.free(ptr_2);
		allocator.free(ptr_1);
		testAssert(allocator.offsets.size() == 0);
	}
	
	// Test an allocation exactly the size of the allocator
	{
		BumpAllocator allocator(1024);
		void* ptr = allocator.alloc(/*size=*/1024, /*alignment=*/16);
		testAssert((uint64)ptr % 16 == 0);
		testAssert(allocator.offsets.size() == 1);

		allocator.free(ptr);
		testAssert(allocator.offsets.size() == 0);
	}

	// Test with an allocation too big for the bump allocator
	{
		BumpAllocator allocator(1024);
		void* ptr = allocator.alloc(/*size=*/2048, /*alignment=*/16);
		testAssert((uint64)ptr % 16 == 0);
		testAssert(allocator.offsets.size() == 1);

		allocator.free(ptr);
		testAssert(allocator.offsets.size() == 0);
	}

	// Test with several allocations over the bump allocator limit
	{
		BumpAllocator allocator(1024);
		std::vector<void*> ptrs;
		for(int i=0; i<16; ++i)
			ptrs.push_back(allocator.alloc(/*size=*/100, /*alignment=*/16));
		testAssert(allocator.offsets.size() == 16);

		while(ptrs.size() > 0)
		{
			allocator.free(ptrs.back());
			ptrs.pop_back();
		}
		testAssert(allocator.offsets.size() == 0);
	}

	conPrint("glare::BumpAllocator::test() done");
}
