/*=====================================================================
GeneralMemAllocator.cpp
-----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "GeneralMemAllocator.h"


#include "ConPrint.h"
#include "MemAlloc.h"
#include "Lock.h"
#include "StringUtils.h"
#include "../maths/mathstypes.h"


namespace glare
{

GeneralMemAllocator::GeneralMemAllocator(size_t arena_size_B_)
:	arena_size_B(arena_size_B_)
{
	allocNewArena();
}


GeneralMemAllocator::~GeneralMemAllocator()
{
	for(size_t i=0; i<arenas.size(); ++i)
		MemAlloc::alignedFree(arenas[i]->data);
}


void GeneralMemAllocator::allocNewArena()
{
	Arena* arena = new Arena(arena_size_B);
	arena->data = MemAlloc::alignedMalloc(arena_size_B, 64);
	arena->best_fit_allocator.name = "general mem allocator arena " + toString(arenas.size());
	arenas.push_back(arena);
}


/*

The allocation header will be placed immediately before the memory location returned to the client.
We will request max(header size, alignment) extra bytes, to give us room for the header, while
still being able to align the memory returned to the client.

Example with alignment = 32: 

                    block returned from best fit allocator
                    |             header   returned ptr
                    |==========|==========|====================|

|-------------------|---------------------|--------------------|
0                   32                   64                   96
*/

struct AllocationHeader
{
	size_t arena_index;
	BestFitAllocator::BlockInfo* block_info;
};


void* GeneralMemAllocator::tryAllocFromArena(size_t size, size_t alignment, size_t start_arena_index)
{
	for(size_t i=start_arena_index; i<arenas.size(); ++i)
	{
		Arena* arena = arenas[i];

		const size_t header_space = myMax(sizeof(AllocationHeader), alignment);
		const size_t header_space_plus_size = header_space + size;
		if(header_space_plus_size > arena_size_B)
			throw glare::Exception("GeneralMemAllocator: Allocation size too large: " + toString(size));

		BestFitAllocator::BlockInfo* block_info = arena->best_fit_allocator.alloc(header_space_plus_size, alignment);
		if(block_info)
		{
			AllocationHeader header;
			header.arena_index = i;
			header.block_info = block_info;
			void* header_ptr = (uint8*)arena->data + block_info->aligned_offset + header_space - sizeof(AllocationHeader);
			std::memcpy(header_ptr, &header, sizeof(AllocationHeader));

			return (uint8*)arena->data + block_info->aligned_offset + header_space;
		}
	}
	return NULL;
}


void* GeneralMemAllocator::alloc(size_t size, size_t alignment)
{
	Lock lock(mutex);

	void* ptr = tryAllocFromArena(size, alignment, /*start_arena_index=*/0);
	if(ptr)
		return ptr;

	// Need to alloc a new arena.
	allocNewArena();

	// Try to allocate from just the new arena.
	ptr = tryAllocFromArena(size, alignment, /*start_arena_index=*/arenas.size() - 1);
	if(!ptr)
		throw glare::Exception("GeneralMemAllocator: Allocation of " + toString(size) + " B failed.");

	return ptr;
}


void GeneralMemAllocator::free(void* ptr)
{
	if(!ptr)
		return;

	Lock lock(mutex);

	AllocationHeader* header = (AllocationHeader*)((uint8*)ptr - sizeof(AllocationHeader));

	assert(header->arena_index < arenas.size());
	arenas[header->arena_index]->best_fit_allocator.free(header->block_info);
}


std::string GeneralMemAllocator::getDiagnostics() const
{
	Lock lock(mutex);

	std::string s;
	for(size_t i=0; i<arenas.size(); ++i)
	{
		const Arena* arena = arenas[i];
		
		s += "Arena " + toString(i) + ": num allocated blocks: " + toString(arena->best_fit_allocator.getNumAllocatedBlocks()) + " (" + toString(arena->best_fit_allocator.getAllocatedSpace() / (1 << 20)) + 
			" MB), num free blocks: " + toString(arena->best_fit_allocator.getNumFreeBlocks()) + " (" + toString(arena->best_fit_allocator.getFreeSpace() / (1 << 20)) + " MB)\n";
	}

	return s;
}


} // End namespace glare


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "../maths/PCG32.h"
#include <set>


static void testAlloc(size_t arena_size, size_t size, size_t alignment)
{
	glare::GeneralMemAllocator allocator(arena_size);
	void* ptr = allocator.alloc(size, alignment);

	testAssert((uint64)ptr % alignment == 0);

	std::memset(ptr, 0, size);

	allocator.free(ptr);
}


void glare::GeneralMemAllocator::test()
{
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/16);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/32);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/8);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/4);

	testAlloc(/*arena_size=*/1024, /*size=*/0, /*alignment=*/16);

	// Test allocating a new arena
	{
		glare::GeneralMemAllocator allocator(/*arena_size=*/256);
		std::set<void*> ptrs;
		for(int i=0; i<10; ++i)
			ptrs.insert(allocator.alloc(/*size=*/128, /*alignment=*/16));
		for(auto it=ptrs.begin(); it != ptrs.end(); ++it)
			allocator.free(*it);
	}

	// Test trying to allocate an allocation with size >= the arena size, should fail, without allocating any more arenas.
	{
		glare::GeneralMemAllocator allocator(/*arena_size=*/256);
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
