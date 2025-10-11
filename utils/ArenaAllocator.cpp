/*=====================================================================
ArenaAllocator.cpp
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "ArenaAllocator.h"


#include "MemAlloc.h"
#include "StringUtils.h"
#include "ConPrint.h"


namespace glare
{


ArenaAllocator::ArenaAllocator(size_t arena_size_B_)
:	arena_size_B(arena_size_B_),
	current_offset(0),
	high_water_mark(0),
	data(NULL)
{
	data = MemAlloc::alignedMalloc(arena_size_B, 64);
}


ArenaAllocator::~ArenaAllocator()
{
#if CHECK_ALLOCATOR_USAGE
	// Upon destruction of the arena, all allocations from it should have been deallocated.  Any that remain indicate an error.
	for(auto it = allocations.begin(); it != allocations.end(); ++it)
	{
		const AllocationDebugInfo& info = it->second;
		conPrint("Error: ArenaAllocator: unfreed allocation upon ArenaAllocator destruction, size: " + toString(info.size) + " B, alignment: " + toString(info.alignment));
		#if defined(_WIN32)
		__debugbreak();
		#endif
	}
#endif // CHECK_ALLOCATOR_USAGE

	MemAlloc::alignedFree(data);
}


#if CHECK_ALLOCATOR_USAGE
void ArenaAllocator::doCheckUsageOnFree(void* ptr)
{
	auto res = allocations.find(ptr);
	if(res == allocations.end())
	{
		conPrint("Error: ArenaAllocator: tried to free memory that was not allocated");
		#if defined(_WIN32)
		__debugbreak();
		#endif
	}
	else
		allocations.erase(ptr);
}


void glare::ArenaAllocator::doCheckUsageOnPopFrame(size_t frame_begin_offset)
{
	// Upon popping of a frame, all allocations from it should have been deallocated.  Any that remain indicate an error.
	// do reverse iteration, stop once we get ptr < data + new_offset
	for(auto it = allocations.rbegin(); it != allocations.rend(); ++it)
	{
		void* ptr = it->first;
		if(ptr < (uint8*)data + frame_begin_offset)
			break;
		else
		{
			const AllocationDebugInfo& info = it->second;
		
			conPrint("Error: ArenaAllocator: unfreed allocation upon frame pop, size: " + toString(info.size) + " B, alignment: " + toString(info.alignment));
			#if defined(_WIN32)
			__debugbreak();
			#endif
		}
	}
}
#endif // CHECK_ALLOCATOR_USAGE


} // End namespace glare


#if BUILD_TESTS


#include "TestUtils.h"
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

	//---------------------- Test ArenaFrame ----------------------
	{
		glare::ArenaAllocator allocator(/*arena_size=*/256);

		void* data = allocator.alloc(100, 4);
		std::memset(data, 0, 100);

		testAssert(allocator.currentOffset() == 100);
		{
			glare::ArenaFrame frame(allocator);

			testAssert(frame.frame_begin_offset == 100);

			void* data2 = allocator.alloc(156, 4);
			std::memset(data2, /*val=*/1, 156);

			allocator.free(data2);
		}
		testAssert(allocator.currentOffset() == 100);

		// Check we didn't overwrite first allocation
		for(int i=0; i<100; ++i)
			testAssert(((const uint8*)data)[i] == 0);

		allocator.free(data);
	}

	// Test nested ArenaFrames
	{
		glare::ArenaAllocator allocator(/*arena_size=*/100);

		testAssert(allocator.currentOffset() == 0);
		{
			glare::ArenaFrame frame1(allocator);
			void* data = allocator.alloc(/*size=*/10, /*alignment=*/1);
			testAssert((uint8*)data == (uint8*)allocator.data);
			testAssert(allocator.currentOffset() == 10);
			
			{
				glare::ArenaFrame frame2(allocator);
				void* data2 = allocator.alloc(/*size=*/10, /*alignment=*/1);
				testAssert((uint8*)data2 == (uint8*)allocator.data + 10);
				testAssert(allocator.currentOffset() == 20);
				allocator.free(data2);
				testAssert(allocator.currentOffset() == 20); // offset is not reduced until frame pop
			}

			allocator.free(data);
			testAssert(allocator.currentOffset() == 10);
		}
		testAssert(allocator.currentOffset() == 0);
	}
}


#endif // BUILD_TESTS
