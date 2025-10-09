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
	data(NULL),
	own_data(true)
#if CHECK_ALLOCATOR_USAGE
	, parent(nullptr),
	child_count(0)
#endif
{
	data = MemAlloc::alignedMalloc(arena_size_B, 64);
}


ArenaAllocator::ArenaAllocator(void* data_, size_t arena_size_B_)
:	arena_size_B(arena_size_B_),
	current_offset(0),
	high_water_mark(0),
	data(data_),
	own_data(false)
#if CHECK_ALLOCATOR_USAGE
	, arent(nullptr),
	child_count(0)
#endif
{
}


ArenaAllocator::ArenaAllocator(const ArenaAllocator& other)
{
	arena_size_B = other.arena_size_B;
	current_offset = other.current_offset;
	high_water_mark = other.high_water_mark;
	data = other.data;
	own_data = other.own_data;

#if CHECK_ALLOCATOR_USAGE
	child_count = other.child_count;
	parent = other.parent;
	if(parent)
		parent->child_count++;
#endif
}


ArenaAllocator::~ArenaAllocator()
{
#if CHECK_ALLOCATOR_USAGE
	// There should be no alive children when this allocator is destroyed.
	if(child_count != 0)
	{
		conPrint("Error: child_count != 0");
		#if defined(_WIN32)
		__debugbreak();
		#endif
	}

	if(!own_data)
	{
		// This is the child of some other allocator
		assert(parent);
		if(parent->child_count <= 0)
		{
			conPrint("Error: parent->child_count <= 0");
			#if defined(_WIN32)
			__debugbreak();
			#endif
		}
		parent->child_count--;
	}


	// Upon destruction of the arena, all allocations from it should have been deallocated.  Any that remain indicate an error.
	for(auto it = allocations.begin(); it != allocations.end(); ++it)
	{
		const AllocationDebugInfo& info = it->second;
		conPrint("Error: ArenaAllocator: unfreed allocation upon ArenaAllocator destruction, size: " + toString(info.size) + " B, alignment: " + toString(info.alignment));
		#if defined(_WIN32)
		__debugbreak();
		#endif
	}
#endif

	if(own_data)
		MemAlloc::alignedFree(data);
}


#if CHECK_ALLOCATOR_USAGE
void ArenaAllocator::free(void* ptr)
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
#endif


} // End namespace glare


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "Reference.h"
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

	//---------------------- Test getFreeAreaArenaAllocator ----------------------
	{
		glare::ArenaAllocator allocator(/*arena_size=*/256);

		void* data = allocator.alloc(100, 8);
		std::memset(data, 0, 100);


		{
			glare::ArenaAllocator suballocator = allocator.getFreeAreaArenaAllocator();

			testAssert(suballocator.arenaSizeB() == 156);
			testAssert(suballocator.currentOffset() == 0);

			void* data2 = suballocator.alloc(156, 8);
			std::memset(data2, /*val=*/1, 156);

			suballocator.free(data2);

			// Should give an error:
			// glare::ArenaAllocator suballocator_2 = allocator.getFreeAreaArenaAllocator();
		}

		// Check we didn't overwrite first allocation
		for(int i=0; i<100; ++i)
			testAssert(((const uint8*)data)[i] == 0);


		// Test handling of references.
		//{
		//	glare::ArenaAllocator suballocator = allocator.getFreeAreaArenaAllocator();
		//
		//	suballocator.incRefCount(); // Need to manually call this when suballocator is on stack.
		//	{
		//		Reference<glare::ArenaAllocator> ref(&suballocator);
		//	}
		//	suballocator.decRefCount();
		//	testAssert(suballocator.getRefCount() == 0);
		//}
	}
}


#endif // BUILD_TESTS
