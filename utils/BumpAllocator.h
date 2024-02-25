/*=====================================================================
BumpAllocator.h
---------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "MemAlloc.h"
#include "ConPrint.h"
#include "Vector.h"
#include <assert.h>


namespace glare
{


/*=====================================================================
BumpAllocator
-------------
A fast memory allocator for temporary allocations.
Frees must be done in reverse order from allocs, in a stack-like (LIFO) fashion.

If a requested allocation size does not fit in the remaining free memory, 
MemAlloc::alignedMalloc will be used.

Use BumpAllocation RAII wrapper defined below for making allocations.

Not thread-safe, designed to be used only by a single thread.
=====================================================================*/
class BumpAllocator
{
public:
	inline BumpAllocator(size_t size);
	inline ~BumpAllocator();

	inline void* alloc(size_t size, size_t alignment);
	inline void free(void* ptr);

	inline size_t size() const { return data.size(); }
	inline size_t highWaterMark() const { return high_water_mark; }

	static void test();
private:
	js::Vector<uint8, 16> data;
	js::Vector<size_t, 16> offsets; // offsets.back() is the offset in data at which the next allocation should be placed.
	size_t high_water_mark;
};


BumpAllocator::BumpAllocator(size_t size) : data(size), high_water_mark(0) {}
BumpAllocator::~BumpAllocator() {}


void* BumpAllocator::alloc(size_t size, size_t alignment)
{
	const size_t current_offset = offsets.empty() ? 0 : offsets.back();
	if(current_offset == std::numeric_limits<size_t>::max())
	{
		// If we already ran out of room and used malloc already:
		conPrint("BumpAllocator::alloc(): warning: Not enough room.  Falling back to malloc.");
		offsets.push_back(std::numeric_limits<size_t>::max()); // Record that we used malloc
		return MemAlloc::alignedMalloc(size, alignment);
	}

	const size_t new_start = Maths::roundUpToMultipleOfPowerOf2(current_offset, alignment);

	const size_t new_offset = new_start + size;
	if(new_offset > data.size())
	{
		// Not enough room.  Fall back to malloc
		conPrint("BumpAllocator::alloc(): warning: Not enough room.  Falling back to malloc.");

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


void BumpAllocator::free(void* ptr)
{
	if(offsets.empty())
	{
		conPrint("BumpAllocator error, called free with no allocations on offset stack remaining.");
		assert(0);
		return;
	}

	const size_t cur_offset = offsets.back();
	if(cur_offset == std::numeric_limits<size_t>::max()) // If alloc used malloc:
		MemAlloc::alignedFree(ptr);

	offsets.pop_back();
}



// RAII wrapper for a single allocation
class BumpAllocation
{
public:
	BumpAllocation(size_t size_, size_t alignment, BumpAllocator& bump_allocator_)
	:	bump_allocator(bump_allocator_),
		size(size_)
	{
		ptr = bump_allocator.alloc(size_, alignment);
	}

	~BumpAllocation()
	{
		bump_allocator.free(ptr);
	}

	BumpAllocator& bump_allocator;
	size_t size;
	void* ptr;
};


} // end namespace glare
