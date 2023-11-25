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
class ArenaAllocator : public glare::Allocator
{
public:
	ArenaAllocator(size_t arena_size_B);
	~ArenaAllocator();

	virtual void* alloc(size_t size, size_t alignment) override;
	virtual void free(void* ptr) override; // Does nothing

	void clear() { current_offset = 0; } // Reset arena, freeing all allocated memory

	static void test();
private:
	size_t arena_size_B;
	size_t current_offset;
	void* data;
};


} // End namespace glare
