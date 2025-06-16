/*=====================================================================
StackAllocator.cpp
------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "StackAllocator.h"


#include "MemAlloc.h"
#include "TestUtils.h"
#include "ConPrint.h"
#include <assert.h>


glare::StackAllocator::StackAllocator(size_t size)
:	data(size), 
	high_water_mark(0)
{}


glare::StackAllocator::~StackAllocator()
{}


void* glare::StackAllocator::alloc(size_t size, size_t alignment)
{
	const size_t current_offset = offsets.empty() ? 0 : offsets.back();
	if(current_offset == std::numeric_limits<size_t>::max())
	{
		// If we already ran out of room and used malloc already:
		//conPrint("StackAllocator::alloc(): warning: Not enough room.  Falling back to malloc.");
		offsets.push_back(std::numeric_limits<size_t>::max()); // Record that we used malloc
		return MemAlloc::alignedMalloc(size, alignment);
	}

	const size_t new_start = Maths::roundUpToMultipleOfPowerOf2(current_offset, alignment);

	const size_t new_offset = new_start + size;
	if(new_offset > data.size())
	{
		// Not enough room.  Fall back to malloc
		conPrint("StackAllocator::alloc(): warning: Not enough room: new_offset=" + toString(new_offset) + ", max size: " + toString(data.size()) + ".  Falling back to malloc.");

		offsets.push_back(std::numeric_limits<size_t>::max()); // Record that we used malloc
		return MemAlloc::alignedMalloc(size, alignment);
	}
	else
	{
		high_water_mark = myMax(high_water_mark, new_offset);
		offsets.push_back(new_offset);
		return data.data() + new_start;
	}
}


void glare::StackAllocator::free(void* ptr)
{
	if(offsets.empty())
	{
		conPrint("StackAllocator error, called free with no allocations on offset stack remaining.");
		assert(0);
		return;
	}

	const size_t cur_offset = offsets.back();
	if(cur_offset == std::numeric_limits<size_t>::max()) // If alloc used malloc:
		MemAlloc::alignedFree(ptr);

	offsets.pop_back();
}


void glare::StackAllocator::test()
{
	conPrint("glare::StackAllocator::test()");

	{
		StackAllocator allocator(1024);
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
		StackAllocator allocator(1024);
		void* ptr = allocator.alloc(/*size=*/1024, /*alignment=*/16);
		testAssert((uint64)ptr % 16 == 0);
		testAssert(allocator.offsets.size() == 1);

		allocator.free(ptr);
		testAssert(allocator.offsets.size() == 0);
	}

	// Test with an allocation too big for the stack allocator
	{
		StackAllocator allocator(1024);
		void* ptr = allocator.alloc(/*size=*/2048, /*alignment=*/16);
		testAssert((uint64)ptr % 16 == 0);
		testAssert(allocator.offsets.size() == 1);

		allocator.free(ptr);
		testAssert(allocator.offsets.size() == 0);
	}

	// Test with several allocations over the stack allocator limit
	{
		StackAllocator allocator(1024);
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

	conPrint("glare::StackAllocator::test() done");
}
