/*=====================================================================
ArenaAllocator.h
----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "Exception.h"
#include "../maths/mathstypes.h"
#include "StringUtils.h"


#ifndef NDEBUG
#define CHECK_ALLOCATOR_USAGE 1
#endif


#if CHECK_ALLOCATOR_USAGE
#include "ConPrint.h"
#include <map>
#endif


namespace glare
{


/*=====================================================================
ArenaAllocator
--------------
A fast memory allocator for temporary allocations.

The best way to use this is with ArenaFrame, which should be stack-allocated, like this:


ArenaAllocator arena_allocator(1024);

{
	glare::ArenaFrame arena_frame(arena_allocator);

	void* p = arena_allocator.alloc(...);

	// do something with p

	arena_allocator.free(p);

} // arena_frame destructor runs on end of scope here and sets the allocator current offset back to what it was upon creation of the frame, effectively freeing all memory allocated while the frame was on the stack.


All allocations can be freed at once with the clear() method.  free() does nothing (except for removing AllocationDebugInfo for the alloc).

Not thread-safe, designed to be used only by a single thread.
=====================================================================*/
class ArenaAllocator
{
public:
	ArenaAllocator(size_t arena_size_B);
	ArenaAllocator(const ArenaAllocator& other) = delete;
	~ArenaAllocator();

	ArenaAllocator& operator = (const ArenaAllocator& other) = delete;

	// alignment must be a power of 2 and <= 64.
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
	assert(Maths::isPowerOfTwo(alignment) && (alignment <= 64));

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
