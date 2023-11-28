/*=====================================================================
ArenaAllocator.cpp
------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "ArenaAllocator.h"


#include "MemAlloc.h"
#include "StringUtils.h"
#include "../maths/mathstypes.h"


namespace glare
{


ArenaAllocator::ArenaAllocator(size_t arena_size_B_)
:	arena_size_B(arena_size_B_),
	current_offset(0)
{
	data = MemAlloc::alignedMalloc(arena_size_B, 64);
}


ArenaAllocator::~ArenaAllocator()
{
	MemAlloc::alignedFree(data);
}



void* ArenaAllocator::alloc(size_t size, size_t alignment)
{
	const size_t new_start = Maths::roundUpToMultipleOfPowerOf2(current_offset, alignment);

	const size_t new_offset = new_start + size;
	if(new_offset > arena_size_B)
		throw glare::Exception("ArenaAllocator: Allocation of " + toString(size) + " B with alignment " + toString(alignment) + " failed.  (Arena size: " + toString(arena_size_B) + ")");

	current_offset = new_offset;
	return (void*)((uint8*)data + new_start);
}


void ArenaAllocator::free(void* ptr)
{
	// Do nothing, we will free all allocated memory in clear().
}


} // End namespace glare


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "../maths/PCG32.h"
#include <set>


static void testAlloc(size_t arena_size, size_t size, size_t alignment)
{
	glare::ArenaAllocator allocator(arena_size);
	void* ptr = allocator.alloc(size, alignment);

	testAssert((uint64)ptr % alignment == 0);

	std::memset(ptr, 0, size);

	allocator.free(ptr);
}


void glare::ArenaAllocator::test()
{
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/16);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/32);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/8);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/4);

	testAlloc(/*arena_size=*/1024, /*size=*/0, /*alignment=*/16);


	// Test clear(), making sure it frees space properly
	{
		glare::ArenaAllocator allocator(/*arena_size=*/32);
		for(int i=0; i<1000; ++i)
		{
			for(int z=0; z<4; ++z)
				allocator.alloc(/*size=*/4, /*alignment=*/8);

			allocator.clear();
		}
	}

	{
		glare::ArenaAllocator allocator(/*arena_size=*/256);
		std::set<void*> ptrs;
		for(int i=0; i<10; ++i)
			ptrs.insert(allocator.alloc(/*size=*/8, /*alignment=*/8));
		for(auto it=ptrs.begin(); it != ptrs.end(); ++it)
			allocator.free(*it);
	}

	// Test trying to allocate an allocation with size >= the arena size, should fail
	{
		glare::ArenaAllocator allocator(/*arena_size=*/256);
		try
		{
			allocator.alloc(/*size=*/1024, /*alignment=*/16);
			failTest("Expected excep");
		}
		catch(glare::Exception&)
		{}
	}
}


#endif // BUILD_TESTS
