/*=====================================================================
StackAllocator.h
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Vector.h"


namespace glare
{


/*=====================================================================
StackAllocator
--------------
A fast memory allocator for temporary allocations.
Frees must be done in reverse order from allocs, in a stack-like (LIFO) fashion.

If a requested allocation size does not fit in the remaining free memory, 
MemAlloc::alignedMalloc will be used.

Use StackAllocation RAII wrapper defined below for making allocations.

Not thread-safe, designed to be used only by a single thread.
=====================================================================*/
class StackAllocator
{
public:
	StackAllocator(size_t size);
	~StackAllocator();

	void* alloc(size_t size, size_t alignment);
	void free(void* ptr);

	inline size_t size() const { return data.size(); }
	inline size_t highWaterMark() const { return high_water_mark; }
	inline size_t curNumAllocs() const { return offsets.size(); }

	static void test();
private:
	js::Vector<uint8, 16> data;
	js::Vector<size_t, 16> offsets; // offsets.back() is the offset in data at which the next allocation should be placed.
	size_t high_water_mark;
};


// RAII wrapper for a single allocation
class StackAllocation
{
public:
	StackAllocation(size_t size_, size_t alignment, StackAllocator& stack_allocator_)
	:	stack_allocator(stack_allocator_),
		size(size_)
	{
		ptr = stack_allocator.alloc(size_, alignment);
	}

	~StackAllocation()
	{
		stack_allocator.free(ptr);
	}

	StackAllocator& stack_allocator;
	size_t size;
	void* ptr;
};


} // end namespace glare
