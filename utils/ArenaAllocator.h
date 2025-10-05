/*=====================================================================
ArenaAllocator.h
----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "GlareAllocator.h"
#include "Mutex.h"
#include <vector>


namespace glare
{


/*=====================================================================
ArenaAllocator
--------------
A fast memory allocator for temporary allocations.
All allocations are freed at once with the clear() method.  free() does nothing.

Not thread-safe, designed to be used only by a single thread.
=====================================================================*/
class ArenaAllocator// : public glare::Allocator
{
public:
	ArenaAllocator(size_t arena_size_B);
	ArenaAllocator(void* data, size_t arena_size_B); // Doesn't own data, doesn't delete it in destructor
	~ArenaAllocator();

	virtual void* alloc(size_t size, size_t alignment);
	virtual void free(void* ptr) {} // Do nothing, we will free all allocated memory in clear().

	void clear() { current_offset = 0; } // Reset arena, freeing all allocated memory

	size_t arenaSizeB() const { return arena_size_B; }
	size_t currentOffset() const { return current_offset; }
	size_t highWaterMark() const { return high_water_mark; } // Highest current_offset seen so far, e.g. max total amount of mem allocated.


	// Since this returns a ArenaAllocator value object, not a reference, will need to call
	// res_allocator.incRefCount(); on it before passing to a function that will use a reference to it, and res_allocator.decRefCount(); afterwards.
	ArenaAllocator getFreeAreaArenaAllocator() const { return ArenaAllocator((uint8*)data + current_offset, arena_size_B - current_offset); }

	static void test();
private:
	size_t arena_size_B;
	size_t current_offset;
	size_t high_water_mark;
	void* data;
	bool own_data;
};


} // End namespace glare
