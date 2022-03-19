/*=====================================================================
BestFitAllocator.h
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "RefCounted.h"
#include "Reference.h"
#include <cstring> // for size_t
#include <map>
#include <string>


namespace glare
{


class BestFitAllocator : public RefCounted
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

	BlockInfo* alloc(size_t size, size_t alignment);
	void free(BlockInfo* block);

	size_t getNumAllocatedBlocks() const;
	size_t getNumFreeBlocks() const { return size_to_free_blocks.size(); }

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
