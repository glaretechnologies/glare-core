/*=====================================================================
LimitedAllocator.cpp
--------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LimitedAllocator.h"


#include "ConPrint.h"
#include "MemAlloc.h"
#include "Lock.h"
#include "StringUtils.h"
#include "PlatformUtils.h"
#include "../maths/mathstypes.h"


namespace glare
{


LimitedAllocatorAllocFailed::LimitedAllocatorAllocFailed(const std::string& msg)
:	glare::Exception(msg)
{}


LimitedAllocator::LimitedAllocator(size_t max_size_B_)
:	max_size_B(max_size_B_), used_size_B(0), high_water_mark_B(0)
{
	
}


LimitedAllocator::~LimitedAllocator()
{
}


void* LimitedAllocator::alloc(size_t size, size_t alignment)
{
	const size_t extra_data_size = myMax(alignment, sizeof(uint64) * 2);
	const size_t total_alloc_size = size + extra_data_size;
	// If requested size is larger than the max size, just fail immediately without looping.
	if(total_alloc_size <= max_size_B)
	{
		for(int i=0; i<10; ++i)
		{
			{
				Lock lock(mutex);

				const size_t new_used_size_B = this->used_size_B + size;
				if(new_used_size_B <= max_size_B)
				{
					// Allocate the requested allocation size, plus room for a uint64, in which we will store the size of the allocation.
				
					void* data = MemAlloc::alignedMalloc(total_alloc_size, alignment);
					((uint64*)((const uint8*)data + extra_data_size - sizeof(uint64)*2))[0] = size; // Store requested allocation size
					((uint64*)((const uint8*)data + extra_data_size - sizeof(uint64)*2))[1] = extra_data_size;

					this->used_size_B = new_used_size_B;
					this->high_water_mark_B = myMax(high_water_mark_B, new_used_size_B);
					//conPrint("LimitedAllocator: allocated " + toString(size) + " B, new used_size_B: " + toString(used_size_B) + " B");
					assert(((uint64)data + extra_data_size) % alignment == 0);
					return (void*)((const uint8*)data + extra_data_size);
				}
			}

			conPrint("LimitedAllocator: full, waiting for mem to be freed...");
			PlatformUtils::Sleep(1);
		}
	}

	throw LimitedAllocatorAllocFailed("Failed to allocate " + toString(size) + " B. (used_size_B: " + toString(used_size_B) + ", max_size_B: " + toString(max_size_B) + ")");
}


void LimitedAllocator::free(void* ptr)
{
	if(!ptr)
		return;

	const uint64 stored_size     = *((uint64*)ptr - 2);
	const uint64 extra_data_size = *((uint64*)ptr - 1);

	void* original_ptr = (uint8*)ptr - extra_data_size;
	MemAlloc::alignedFree(original_ptr);

	Lock lock(mutex);
	
	assert(this->used_size_B >= stored_size);
	used_size_B -= stored_size;

	//conPrint("LimitedAllocator: freed " + toString(stored_size) + " B, new used_size_B: " + toString(used_size_B) + " B");
}


glare::String LimitedAllocator::getDiagnostics() const
{
	Lock lock(mutex);

	std::string s = "Used size: " + getMBSizeString(this->used_size_B) + ", high water mark: " + getMBSizeString(high_water_mark_B) + ", max size: " + getMBSizeString(max_size_B) + "\n";
	return glare::String(s.c_str());
}


MemReservation::MemReservation()
:	m_limited_allocator(nullptr),
	reserved_size(0)
{
}

MemReservation::MemReservation(size_t size_B, LimitedAllocator* limited_allocator_)
:	m_limited_allocator(nullptr),
	reserved_size(0)
{
	reserve(size_B, limited_allocator_);

	
}


MemReservation::~MemReservation()
{
	if(m_limited_allocator)
	{
		Lock lock(m_limited_allocator->mutex);
		assert(m_limited_allocator->used_size_B >= reserved_size);
		m_limited_allocator->used_size_B -= reserved_size;
	}
}


void MemReservation::reserve(size_t size_B, LimitedAllocator* limited_allocator)
{
	const size_t total_alloc_size = size_B;
	if(total_alloc_size <= limited_allocator->max_size_B)
	{
		for(int i=0; i<10; ++i)
		{
			{
				Lock lock(limited_allocator->mutex);

				const size_t new_used_size_B = limited_allocator->used_size_B + size_B;
				if(new_used_size_B <= limited_allocator->max_size_B)
				{
					limited_allocator->used_size_B = new_used_size_B;
					limited_allocator->high_water_mark_B = myMax(limited_allocator->high_water_mark_B, new_used_size_B);

					this->reserved_size = size_B;
					this->m_limited_allocator = limited_allocator;
					return;
				}
			}

			conPrint("MemReservation: full, waiting for mem to be freed...");
			PlatformUtils::Sleep(1);
		}
	}

	throw LimitedAllocatorAllocFailed("Failed to reserve " + toString(size_B) + " B. (used_size_B: " + toString(limited_allocator->used_size_B) + ", max_size_B: " + toString(limited_allocator->max_size_B) + ")");
}


} // End namespace glare


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "../maths/PCG32.h"
#include <set>


static void testAlloc(size_t arena_size, size_t size, size_t alignment)
{
	//glare::GeneralMemAllocator allocator(arena_size);
	//void* ptr = allocator.alloc(size, alignment);

	//testAssert((uint64)ptr % alignment == 0);

	//std::memset(ptr, 0, size);

	//allocator.free(ptr);
}


void glare::LimitedAllocator::test()
{
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/16);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/32);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/8);
	testAlloc(/*arena_size=*/1024, /*size=*/128, /*alignment=*/4);

	testAlloc(/*arena_size=*/1024, /*size=*/0, /*alignment=*/16);

	// Test allocating a new arena
	{
		glare::LimitedAllocator allocator(/*arena_size=*/256);
		std::set<void*> ptrs;
		for(int i=0; i<10; ++i)
			ptrs.insert(allocator.alloc(/*size=*/128, /*alignment=*/16));
		for(auto it=ptrs.begin(); it != ptrs.end(); ++it)
			allocator.free(*it);
	}

	// Test trying to allocate an allocation with size >= the arena size, should fail, without allocating any more arenas.
	{
		glare::LimitedAllocator allocator(/*arena_size=*/256);
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
