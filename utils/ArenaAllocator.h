/*=====================================================================
ArenaAllocator.h
----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "GlareAllocator.h"
#include "Mutex.h"
#include "Exception.h"
#include "StringUtils.h"
#include "../maths/mathstypes.h"
#include <vector>


#ifndef NDEBUG
#define CHECK_ALLOCATOR_USAGE 1
#endif


#if CHECK_ALLOCATOR_USAGE
#include <map>
#include "ConPrint.h"
#endif


namespace glare
{


/*=====================================================================
ArenaAllocator
--------------
A fast memory allocator for temporary allocations.
All allocations are freed at once with the clear() method.  free() does nothing (except for removing AllocationDebugInfo for the alloc).

Not thread-safe, designed to be used only by a single thread.
=====================================================================*/
class ArenaAllocator
{
public:
	ArenaAllocator(size_t arena_size_B);
	ArenaAllocator(const ArenaAllocator& other) = delete;
	~ArenaAllocator();

	ArenaAllocator& operator = (const ArenaAllocator& other) = delete;

	inline void* alloc(size_t size, size_t alignment);

	inline void free(void* ptr);

	inline void clear(); // Reset arena, freeing all allocated memory


	size_t arenaSizeB() const { return arena_size_B; }
	size_t currentOffset() const { return current_offset; }
	size_t highWaterMark() const { return high_water_mark; } // Highest current_offset seen so far, e.g. max total amount of mem allocated.

	// Used by ArenaFrame
	inline void popFrame(size_t frame_begin_offset);

	static void test();
private:
	void doCheckUsageOnFree(void* ptr);
	void doCheckUsageOnPopFrame(size_t new_offset);

	size_t arena_size_B;
	size_t current_offset;
	size_t high_water_mark;
	void* data;

#if CHECK_ALLOCATOR_USAGE
	struct AllocationDebugInfo
	{
		size_t size, alignment;
	};
	std::map<void*, AllocationDebugInfo> allocations;
	ArenaAllocator* parent;
	mutable int child_count;
#endif
};


class ArenaFrame
{
public:
	ArenaFrame(ArenaAllocator& allocator)
	{
		m_allocator = &allocator;
		frame_begin_offset = allocator.currentOffset();
	}

	~ArenaFrame()
	{
		m_allocator->popFrame(frame_begin_offset);
	}

	ArenaAllocator* m_allocator;
	size_t frame_begin_offset;
};


void* ArenaAllocator::alloc(size_t size, size_t alignment)
{
	assert(alignment <= 64);
	const size_t new_start = Maths::roundUpToMultipleOfPowerOf2(current_offset, alignment);

	const size_t new_offset = new_start + size;
	if(new_offset > arena_size_B)
		throw glare::Exception("ArenaAllocator: Allocation of " + toString(size) + " B with alignment " + toString(alignment) + " failed.  (Arena size: " + toString(arena_size_B) + ")");

	current_offset = new_offset;
	high_water_mark = myMax(high_water_mark, current_offset);
	void* ptr = (void*)((uint8*)data + new_start);

#if CHECK_ALLOCATOR_USAGE
	AllocationDebugInfo info;
	info.size = size;
	info.alignment = alignment;
	const bool inserted = allocations.insert(std::make_pair(ptr, info)).second;
	if(!inserted)
	{
		conPrint("Error: ArenaAllocator: allocation at same address as existing allocation");
		#if defined(_WIN32)
		__debugbreak();
		#endif
	}
#endif

	return ptr;
}


void ArenaAllocator::free(void* ptr)
{
#if CHECK_ALLOCATOR_USAGE
	doCheckUsageOnFree(ptr);
#endif

	// Do nothing, we will free all allocated memory in clear().
}


// Reset arena, freeing all allocated memory
void ArenaAllocator::clear()
{ 
	current_offset = 0;
	
#if CHECK_ALLOCATOR_USAGE
	allocations.clear();
#endif
} 


void glare::ArenaAllocator::popFrame(size_t frame_begin_offset)
{
#if CHECK_ALLOCATOR_USAGE
	doCheckUsageOnPopFrame(frame_begin_offset);
#endif

	current_offset = frame_begin_offset;
}


} // End namespace glare
