/*=====================================================================
BestFitAllocator.h
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include <cstring> // for size_t
#include <map>
#include <string>


namespace glare
{


class BestFitAllocator : public ThreadSafeRefCounted
{
public:
	struct BlockInfo
	{
		BlockInfo* prev;
		BlockInfo* next;

		// Internal data:
		size_t offset;
		size_t size;

		size_t aligned_offset; // Offset for client

		glare::BestFitAllocator* allocator;
		//Reference<glare::BestFitAllocator> allocator;

		bool allocated;
	};

	BestFitAllocator(size_t arena_size);
	~BestFitAllocator();

	void expand(size_t new_arena_size); // Increase arena size.  Keep the existing allocations, but add a new free block at the end of the old arena, or extend last free block.

	BlockInfo* alloc(size_t size, size_t alignment);
	void free(BlockInfo* block);

	size_t getNumAllocatedBlocks() const;
	size_t getNumFreeBlocks() const { return size_to_free_blocks.size(); }
	size_t getAllocatedSpace() const;
	size_t getFreeSpace() const;

	static void test();

	std::string name;
private:
	void removeBlockFromFreeMap(BlockInfo* block);
	void checkInvariants();

	BlockInfo* first;
	BlockInfo* last;

	std::multimap<size_t, BlockInfo*> size_to_free_blocks;

	size_t arena_size;
};


typedef Reference<BestFitAllocator> BestFitAllocatorRef;


} // End namespace glare
