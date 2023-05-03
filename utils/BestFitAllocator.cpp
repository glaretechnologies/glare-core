/*=====================================================================
BestFitAllocator.cpp
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "BestFitAllocator.h"


#include "../maths/mathstypes.h"
#include "../utils/ConPrint.h"


namespace glare
{


BestFitAllocator::BestFitAllocator(size_t arena_size_)
:	arena_size(arena_size_)
{
	//conPrint("BestFitAllocator::BestFitAllocator()");
	BlockInfo* block = new BlockInfo();
	block->offset = 0;
	block->size = arena_size;
	block->prev = NULL;
	block->next = NULL;
	block->allocated = false;

	first = block;
	last = block;

	size_to_free_blocks.insert(std::make_pair(arena_size, block));

	checkInvariants();
}


BestFitAllocator::~BestFitAllocator()
{
	//conPrint("BestFitAllocator::~BestFitAllocator()");

	checkInvariants();

	BlockInfo* cur = first;
	while(cur != NULL)
	{
		BlockInfo* next = cur->next;

		if(cur->allocated)
		{
			// Come client code somewhere did not free this block, so it may still have a pointer to it.
			// Don't delete the block, or we will get a crash.
			conPrint("~BestFitAllocator(): warning: block is still allocated from '" + name + "'");
			cur->allocator = NULL;  // NULL out the pointer to this allocator, so the client code doesn't attempt to use it after it has been destroyed.
		}
		else
		{
			delete cur;
		}

		cur = next; // Walk to next block
	}
}


// Increase arena size.  Keep the existing allocations, but add a new free block at the end of the old arena, or extend last free block.
void BestFitAllocator::expand(size_t new_arena_size) 
{
	assert(new_arena_size > arena_size);

	/*
	Case A: last block was allocated:

	|-------block A-----|------------last------------------|
	----------------------------------------------------------------------------------------------------------------

	|-------block A-----|----------old last----------------|                  new block                            |
	----------------------------------------------------------------------------------------------------------------
	*/
	if(last->allocated) // If last block was allocated:
	{
		BlockInfo* old_last = last;

		// Add a single new unallocated block
		BlockInfo* block = new BlockInfo();
		block->offset = arena_size;
		block->size = new_arena_size - arena_size;
		block->prev = old_last;
		block->next = NULL;
		block->allocated = false;

		size_to_free_blocks.insert(std::make_pair(block->size, block));

		old_last->next = block;
		last = block;
	}
	/*
	Case B (1): last block was free:, last != first:

	|-------block A-----|             last                 |
	----------------------------------------------------------------------------------------------------------------

	|-------block A-----|                                  new free block                                          |
	----------------------------------------------------------------------------------------------------------------


	Case B (2): last block was free:, last == first:

	|                     first=last                      |
	----------------------------------------------------------------------------------------------------------------

	|                                                      new free block                                          |
	----------------------------------------------------------------------------------------------------------------
	*/
	else // Else if last block was free:
	{
		// Remove last block, add new merged free block:
		BlockInfo* old_last = last;

		removeBlockFromFreeMap(old_last);

		BlockInfo* block = new BlockInfo();
		block->offset = old_last->offset;
		block->size = new_arena_size - old_last->offset;
		block->prev = old_last->prev;
		block->next = NULL;
		block->allocated = false;

		size_to_free_blocks.insert(std::make_pair(block->size, block));

		if(old_last->prev)
			old_last->prev->next = block;
		else
		{
			assert(old_last == first);
			first = block;
		}

		last = block;
		delete old_last;
	}


	arena_size = new_arena_size;

	checkInvariants();
}


void BestFitAllocator::removeBlockFromFreeMap(BlockInfo* block)
{
	// Remove next block
	auto range = size_to_free_blocks.equal_range(block->size);
 
	for (auto it = range.first; it != range.second; ++it)
	{
		if(it->second == block)
		{
			size_to_free_blocks.erase(it);
			return;
		}
	}
}


size_t BestFitAllocator::getNumAllocatedBlocks() const
{
	size_t num = 0;
	BlockInfo* cur = first;
	while(cur != NULL)
	{
		if(cur->allocated)
			num++;
		cur = cur->next; // Walk to next block
	}
	return num;
}


size_t BestFitAllocator::getAllocatedSpace() const
{
	size_t space = 0;
	BlockInfo* cur = first;
	while(cur != NULL)
	{
		if(cur->allocated)
			space += cur->size;
		cur = cur->next; // Walk to next block
	}
	return space;
}


size_t BestFitAllocator::getFreeSpace() const
{
	size_t space = 0;
	BlockInfo* cur = first;
	while(cur != NULL)
	{
		if(!cur->allocated)
			space += cur->size;
		cur = cur->next; // Walk to next block
	}
	return space;
}


BestFitAllocator::BlockInfo* BestFitAllocator::alloc(size_t size_, size_t requested_alignment)
{
	assert(requested_alignment > 0);

	if(size_ == 0)
		size_ = 1;

	size_t alignment = requested_alignment;
	if(alignment == 0)
		alignment = 1;

	// Find smallest free block with size >= size

	/*
	Example where requested_alignment is 16:
	The internal block allocation returns a block with offset 24

	        block offset     aligned offset
    ------------------|-------|-------------------------------------------------------------|
	0          16     24     32          48          64

	*/

	const size_t use_size = size_ + alignment; // aligned_offset will be < offset + alignment, so allocate alignment extra bytes.
	
	const auto res = size_to_free_blocks.lower_bound(use_size); // "Returns an iterator pointing to the first element that is not less than (i.e. greater or equal to) key."
	if(res == size_to_free_blocks.end())
		return NULL;

	BlockInfo* block = res->second;
	assert(!block->allocated);

	assert(block->size >= use_size);

	// Set an aligned offset for the client code.
	block->aligned_offset = Maths::roundUpToMultiple(block->offset, alignment);
	assert(block->aligned_offset >= block->offset);
	assert(block->aligned_offset + size_ <= block->offset + block->size);

	block->allocator = this;



	const size_t remaining_size = block->size - use_size;

	/*
	Say block B is chosen to allocate in:
	
	prev-----|       block B                 |----------next----------------|       next free
	---------------------------------------------------------------------------
	
	prev-----|----block B------| remaining   |----------next----------------|       next free
	---------------------------------------------------------------------------
	
	We want to create a new free block in the remaining section of block B.
	*/

	removeBlockFromFreeMap(block);

	block->allocated = true;

	if(remaining_size > 0)
	{
		block->size = use_size;

		BlockInfo* next = block->next;

		BlockInfo* new_remaining_block = new BlockInfo();
		new_remaining_block->offset = block->offset + use_size;
		new_remaining_block->size = remaining_size;
		new_remaining_block->prev = block;
		new_remaining_block->next = next;
		new_remaining_block->allocated = false;

		// Insert new_remaining_block into our doubly-linked list of blocks
		block->next = new_remaining_block;

		if(next)
		{
			next->prev = new_remaining_block;
		}
		else // else block was last block
		{
			assert(last == block);
			last = new_remaining_block;
		}

		size_to_free_blocks.insert(std::make_pair(new_remaining_block->size, new_remaining_block));
	}

	checkInvariants();
	return block;
}


void BestFitAllocator::free(BlockInfo* block)
{
	BlockInfo* prev = block->prev;
	BlockInfo* next = block->next;

	/*
	Case A: where block B is being freed, and is adjacent to allocated blocks on both sides:
	
	prev_2      |-------prev-----|---------block B--------|----------next----------------|       next_2
	------------------------------------------------------------------------------------------------------------
	
	prev_2      |-------prev-----|         block B        |----------next----------------|       next_2
	----------------------------------------------------------------------------------------------------------------
	*/
	if((prev == NULL || prev->allocated) && (next == NULL || next->allocated)) // case A:
	{
		// Add back into free map
		size_to_free_blocks.insert(std::make_pair(block->size, block));
		//block->allocator = NULL; // Set the allocator reference to NULL, so that we don't have circular references that prevent the BestFitAllocator from being freed.
		block->allocated = false;
	}
	/*
	Case where block B is being freed, and is adjacent to a free block to the left only:

	prev_2-----|       prev     |---------block B--------|----------next----------------|       next_2
	------------------------------------------------------------------------------------------------------------
	
	prev_2-----|       prev     |         block B        |----------next----------------|       next_2
	----------------------------------------------------------------------------------------------------------------

	prev_2-----|       coalesced block                   |----------next----------------|       next_2
	----------------------------------------------------------------------------------------------------------------*/
	else if(prev != NULL && !prev->allocated && (next == NULL || next->allocated)) // case B:
	{
		BlockInfo* new_coalesced_block = new BlockInfo();
		new_coalesced_block->allocated = false;
		new_coalesced_block->offset = prev->offset;
		new_coalesced_block->size = prev->size + block->size;
		new_coalesced_block->prev = prev->prev;
		new_coalesced_block->next = next;
		new_coalesced_block->allocated = false;

		// Insert new_coalesced_block into doubly-linked list
		BlockInfo* prev_2 = prev->prev;
		if(prev_2)
			prev_2->next = new_coalesced_block;
		else // else prev was first block:
		{
			assert(first == prev);
			first = new_coalesced_block;
		}

		if(next)
		{
			next->prev = new_coalesced_block;
		}
		else // else block was last block
		{
			assert(last == block);
			last = new_coalesced_block;
		}

		size_to_free_blocks.insert(std::make_pair(new_coalesced_block->size, new_coalesced_block));

		delete block;

		// Remove prev block
		removeBlockFromFreeMap(prev);
		delete prev;
	}
	/*
	Case where block B is being freed, and is adjacent to a free block to the right only:

	prev_2-----|-------prev-----|---------block B--------|          next                |-------next_2--------
	------------------------------------------------------------------------------------------------------------
	
	prev_2-----|-------prev-----|         block B        |          next                |-------next_2--------
	----------------------------------------------------------------------------------------------------------------

	prev_2-----|-------prev-----|       coalesced block                                 |-------next_2--------
	----------------------------------------------------------------------------------------------------------------
	*/
	else if((prev == NULL || prev->allocated) && next != NULL && !next->allocated) // case C:
	{
		BlockInfo* new_coalesced_block = new BlockInfo();
		new_coalesced_block->allocated = false;
		new_coalesced_block->offset = block->offset;
		new_coalesced_block->size = block->size + next->size;
		new_coalesced_block->prev = prev;
		new_coalesced_block->next = next->next;
		new_coalesced_block->allocated = false;

		// Insert new_coalesced_block into doubly-linked list
		if(prev)
		{
			prev->next = new_coalesced_block;
		}
		else // else block B was first block:
		{
			assert(first == block);
			first = new_coalesced_block;
		}

		BlockInfo* next_2 = next->next;
		if(next_2)
		{
			next_2->prev = new_coalesced_block;
		}
		else // else next was last block
		{
			assert(last == next);
			last = new_coalesced_block;
		}

		size_to_free_blocks.insert(std::make_pair(new_coalesced_block->size, new_coalesced_block));

		delete block;

		// Remove next block
		removeBlockFromFreeMap(next);
		delete next;
	}
	/*
	Case where block B is being freed, and is adjacent to free blocks on both sides

	prev_2-----|       prev     |---------block B--------|          next                |-------next_2--------
	------------------------------------------------------------------------------------------------------------
	
	prev_2-----|       prev     |         block B        |          next                |-------next_2--------
	----------------------------------------------------------------------------------------------------------------

	prev_2-----|                        coalesced block                                 |-------next_2--------
	----------------------------------------------------------------------------------------------------------------
	*/
	else if(prev != NULL && !prev->allocated && next != NULL && !next->allocated) // case D:
	{
		BlockInfo* new_coalesced_block = new BlockInfo();
		new_coalesced_block->allocated = false;
		new_coalesced_block->offset = prev->offset;
		new_coalesced_block->size = prev->size + block->size + next->size;
		new_coalesced_block->prev = prev->prev;
		new_coalesced_block->next = next->next;
		new_coalesced_block->allocated = false;

		// Insert new_coalesced_block into doubly-linked list
		BlockInfo* prev_2 = prev->prev;
		if(prev_2)
			prev_2->next = new_coalesced_block;
		else // else prev was first block:
		{
			assert(first == prev);
			first = new_coalesced_block;
		}

		BlockInfo* next_2 = next->next;
		if(next_2)
		{
			next_2->prev = new_coalesced_block;
		}
		else // else next was last block
		{
			assert(last == next);
			last = new_coalesced_block;
		}

		size_to_free_blocks.insert(std::make_pair(new_coalesced_block->size, new_coalesced_block));

		delete block;

		// Remove prev and next blocks
		removeBlockFromFreeMap(prev);
		removeBlockFromFreeMap(next);
		delete prev;
		delete next;
	}
	else
	{
		assert(0);
	}

	checkInvariants();
}


void BestFitAllocator::checkInvariants()
{
#ifndef NDEBUG
	// Iterate over blocks
	BlockInfo* prev = NULL;
	BlockInfo* cur = first;
	size_t num_free_blocks = 0;
	while(cur != NULL)
	{
		// Check prev pointer is correct
		if(prev)
			assert(cur->prev == prev);

		// Check offset and size are valid
		size_t next_offset;
		if(cur->next)
			next_offset = cur->next->offset;
		else
			next_offset = arena_size;

		assert(cur->offset >= 0 && cur->offset <= next_offset);
		assert(cur->offset + cur->size == next_offset);

		if(cur->allocated)
		{
			// Check aligned_offset is valid
			//assert(cur->aligned_offset % MIN_ALIGNMENT == 0);
			assert((cur->aligned_offset >= cur->offset) && (cur->aligned_offset < cur->offset + cur->size));
		}
		else
		{
			// If this block is not allocated, check is in size_to_free_blocks.
			auto range = size_to_free_blocks.equal_range(cur->size);
 
			bool found = false;
			for(auto it = range.first; it != range.second; ++it)
				if(it->second == cur)
					found = true;
			assert(found);

			num_free_blocks++;
		}


		if(cur->next == NULL) // If this is the last block:
			assert(this->last == cur);

		// Walk to next block
		prev = cur;
		cur = cur->next;
	}

	// Check we don't have extra entries in size_to_free_blocks
	assert(size_to_free_blocks.size() == num_free_blocks);
#endif
}


} // End namespace glare


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "../maths/PCG32.h"
#include <set>


#if 0
// Command line:
// C:\fuzz_corpus\best_fit_allocator -max_len=10000 -seed=1

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		std::set<glare::BestFitAllocator::BlockInfo*> blocks;

		glare::BestFitAllocatorRef allocator = new glare::BestFitAllocator(1024);

		for(int i=0; i+8<size; i += 8)
		{
			int x;
			std::memcpy(&x, &data[i], 4);
			if(x >= 0)
			{
				// Do an allocation
				uint32 alloc_size = myMin<uint32>(2000, (uint32)x);
				uint32 alignment;
				std::memcpy(&alignment,  &data[i + 4], 4);

				//alignment = alignment << 2; // Must be multiple of 4.

				if(alignment == 0)
					alignment = 1;

				glare::BestFitAllocator::BlockInfo* block = allocator->alloc(alloc_size, alignment);
				if(block)
					blocks.insert(block);
			}
			else
			{
				// Do a free
				size_t index = myMin(blocks.size() - 1, (size_t)(-x));
				size_t z = 0;
				for(auto it = blocks.begin(); it != blocks.end(); ++it)
				{
					if(z == index)
					{
						glare::BestFitAllocator::BlockInfo* block = *it;
						allocator->free(block);
						blocks.erase(block);
						break;
					}
					z++;
				}
			}
		}

		// Free blocks
		for(auto it = blocks.begin(); it != blocks.end(); ++it)
			allocator->free(*it);

		testAssert(allocator->getRefCount() == 1);
	}
	catch(glare::Exception& )
	{}
	return 0;  // Non-zero return values are reserved for future use.
}
#endif


void glare::BestFitAllocator::test()
{

	//=========== Test freeing the allocator while we still have a block reference ===============
	{
		BlockInfo* block0;
		{
			BestFitAllocatorRef allocator = new BestFitAllocator(1024);
			allocator->name = "test allocator";
			block0 = allocator->alloc(1020, 4);
			testAssert(block0);
			testAssert(block0->offset == 0);
			testAssert(block0->size >= 1020);
			//block0->allocator->free(block0);
		}
		
	}


	//-------------------------- Random stress/fuzz test -------------------------------
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);


		std::set<BlockInfo*> blocks;
		PCG32 rng(1);
		for(int i = 0; i<10000; ++i)
		{
			if(i % (1 << 16) == 0)
				conPrint("allocated blocks size: " + toString(blocks.size()));
			const float ur = rng.unitRandom();
			if(ur < 0.6f)
			{
				const size_t alloc_size = rng.nextUInt(128);
				const size_t alignment = 1 + rng.nextUInt(64);// Maths::roundUpToMultipleOfPowerOf2<uint32>(rng.nextUInt(64), 4);
				BlockInfo* block = allocator->alloc(alloc_size, alignment);
				if(block)
				{
					blocks.insert(block);
				}
			}
			else
			{
				if(blocks.size() > 0)
				{
					// Select a random block in our allocated set to free
					const size_t index = rng.nextUInt((uint32)blocks.size());

					size_t z = 0;
					for(auto it = blocks.begin(); it != blocks.end(); ++it)
					{
						if(z == index)
						{
							BlockInfo* block = *it;
							allocator->free(block);
							blocks.erase(block);
							break;
						}
						z++;
					}
				}
			}
		}

		// Free all allocated blocks
		for(auto it = blocks.begin(); it != blocks.end(); ++it)
			allocator->free(*it);
	}






	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block = allocator->alloc(16, 4);
		
		testAssert(block->offset == 0);
		testAssert(block->size >= 16);

		BlockInfo* block2 = allocator->alloc(32, 4);

		//testAssert(block2->offset == 16);
		testAssert(block2->size >= 32);

		BlockInfo* block3 = allocator->alloc(8, 4);

		//testAssert(block3->offset == 16 + 32);
		testAssert(block3->size >= 8);

		// Free a block
		allocator->free(block2);

		// Allocate 32 bytes again
		BlockInfo* block4 = allocator->alloc(32, 4);

		//testAssert(block4->offset == 16);
		testAssert(block4->size >= 32);
	}


	//=========== Test some aligned allocs ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(/*size=*/64, /*alignment=*/32);
		testAssert(block0->offset == 0);
		testAssert(block0->aligned_offset == 0);
		testAssert(block0->size >= 64);
		testAssert(allocator->getNumAllocatedBlocks() == 1);
		testAssert(allocator->getNumFreeBlocks() == 1);
		//|--------block0--------|              free                  

		BlockInfo* block1 = allocator->alloc(64, 64);
		//testAssert(block1->offset == 64);
		testAssert(block1->aligned_offset % 64 == 0);
		testAssert(block1->size >= 64);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 1);
		//|--------block0--------|--------block1--------|              free                  

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->aligned_offset % 4 == 0);
		testAssert(block2->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block0);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|        block0        |--------block1--------|--------block2--------|              free                  

		BlockInfo* block3 = allocator->alloc(50, 64);
		testAssert(block3->aligned_offset % 64 == 0);
		testAssert(block3->size >= 50);
		testAssert(allocator->getNumAllocatedBlocks() == 3);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|--block3---|          |--------block1--------|--------block2--------|              free                  
	}

	//=========== Test allocation with no adjacent free blocks, and with remaining size > 0 ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);
		//|--------block0--------|              free                  

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);
		//|--------block0--------|--------block1--------|              free                  

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block0);
		//|        block0        |--------block1--------|--------block2--------|              free                  

		BlockInfo* block3 = allocator->alloc(60, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size >= 60);
		//|--block3---|          |--------block1--------|--------block2--------|              free                  
	}


	//=========== Test freeing only block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);
		//|--------block0--------|              free                  

		allocator->free(block0);
	}

	//=========== Test allocating a block the full arena size ===============
	/*{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(1024, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 1024);
		//|--------block0--------|              free                  

		allocator->free(block0);
	}*/

	//=========== Test freeing a block with a free block adjacent to the left, when the adjacent free block is the first block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);
		//|--------block0--------|              free                  

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);
		//|--------block0--------|--------block1--------|              free                  

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);
		testAssert(allocator->getNumAllocatedBlocks() == 3);
		testAssert(allocator->getNumFreeBlocks() == 1);

		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block0);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|        block0        |--------block1--------|--------block2--------|              free                  

		allocator->free(block1);
		testAssert(allocator->getNumAllocatedBlocks() == 1);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|        coalesced                            |--------block2--------|              free                  
		
		BlockInfo* block3 = allocator->alloc(200, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size >= 200);
		//|----------block3-----------------------------|--------block2--------|              free                  
	}

	//=========== Test freeing a block with a free block adjacent to the left, when the block is the last block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(208); // Note the 208
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);
		//|--------block0--------|        free          |

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);
		//|--------block0--------|--------block1--------|


		allocator->free(block0);
		//|        block0        |--------block1--------|

		allocator->free(block1);
		testAssert(allocator->getNumAllocatedBlocks() == 0);
		testAssert(allocator->getNumFreeBlocks() == 1);
		//|        coalesced                            |
		
		BlockInfo* block3 = allocator->alloc(200, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size >= 200);
		//|----------block3-----------------------------|
	}

	//=========== Test freeing a block with a free block adjacent to the left, when the adjacent free block is not the first block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset >= 300);
		testAssert(block3->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|              free                  

		allocator->free(block1);
		//|--------block0--------|        block1        |--------block2--------|--------block3--------|              free                  

		allocator->free(block2);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|--------block0--------|                  coalesced                  |--------block3--------|              free                  
		
		BlockInfo* block4 = allocator->alloc(200, 4);
		testAssert(block4->offset >= 100 && block4->offset <= 300);
		testAssert(block4->size >= 200);
		//|--------block0--------|--------------------block4-------------------|--------block3--------|              free                  
	}

	//=========== Test freeing a block with a free block adjacent to the right ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block1);
		//|--------block0--------|        block1        |--------block2--------|              free                  

		allocator->free(block0);
		testAssert(allocator->getNumAllocatedBlocks() == 1);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|        coalesced                            |--------block2--------|              free                  
		
		BlockInfo* block3 = allocator->alloc(200, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size >= 200);
		//|----------block3-----------------------------|--------block2--------|              free                  
	}

	//=========== Test freeing a block with a free block adjacent to the right, and the left block existing. ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset >= 300);
		testAssert(block3->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|              free                  

		allocator->free(block2);
		//|--------block0--------|--------block1--------|        block2        |--------block3--------|              free                  

		allocator->free(block1);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|--------block0--------|                  coalesced                  |--------block3--------|              free                  
		
		BlockInfo* block4 = allocator->alloc(200, 4);
		testAssert(block4->offset >= 100 && block4->offset <= 200);
		testAssert(block4->size >= 200);
		//|--------block0--------|--------------------block4-------------------|--------block3--------|              free                  
	}

	//=========== Test freeing a block with a free block on both sides ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset >= 300);
		testAssert(block3->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|              free                  

		allocator->free(block0);
		allocator->free(block2);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 3);
		//|        block0        |--------block1--------|        block2        |--------block3--------|              free                  

		allocator->free(block1);
		testAssert(allocator->getNumAllocatedBlocks() == 1);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|                              coalesced                             |--------block3--------|              free                  
		
		BlockInfo* block4 = allocator->alloc(300, 4);
		testAssert(block4->offset == 0);
		testAssert(block4->size >= 300);
		//|--------------------------------block4------------------------------|--------block3--------|              free                  
	}

	//=========== Test freeing a block with a free block on both sides, where the free block on the right is the last block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(312); // Note the 312 arena size here
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);

		//|--------block0--------|--------block1--------|--------block2--------|

		allocator->free(block0);
		allocator->free(block2);
		testAssert(allocator->getNumAllocatedBlocks() == 1);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|        block0        |--------block1--------|        block2        |

		allocator->free(block1);
		testAssert(allocator->getNumAllocatedBlocks() == 0);
		testAssert(allocator->getNumFreeBlocks() == 1);
		//|                              coalesced                             |
		
		BlockInfo* block4 = allocator->alloc(300, 4);
		testAssert(block4->offset == 0);
		testAssert(block4->size >= 300);
		//|--------------------------------block4------------------------------|
	}

	//=========== Test freeing a block with a free block on both sides, and the free block on the left not being the first block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset >= 200);
		testAssert(block2->size >= 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset >= 300);
		testAssert(block3->size >= 100);

		BlockInfo* block4 = allocator->alloc(100, 4);
		testAssert(block4->offset >= 400);
		testAssert(block4->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|--------block4--------|              free                  

		allocator->free(block1);
		allocator->free(block3);
		testAssert(allocator->getNumAllocatedBlocks() == 3);
		testAssert(allocator->getNumFreeBlocks() == 3);
		//|--------block0--------|        block1        |--------block2--------|        block3        |--------block4--------|              free                  

		allocator->free(block2);
		testAssert(allocator->getNumAllocatedBlocks() == 2);
		testAssert(allocator->getNumFreeBlocks() == 2);
		//|--------block0--------|                              coalesced                             |--------block4--------|              free                  
		
		BlockInfo* block5 = allocator->alloc(300, 4);
		testAssert(block5->offset >= 100 && block5->offset <= 200);
		testAssert(block5->size >= 300);
		//|--------block0--------|-------------------------------block5-------------------------------|--------block4--------|              free                  
	}

	//=========== Test an allocation that won't fit ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(2000, 4);
		testAssert(block0 == NULL);
	}


	//=========== Test expand on unallocated mem ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		allocator->expand(2048);

		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);
	}

	//=========== Test expand with a free block at end of mem ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size >= 100);

		allocator->expand(2048);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1);
		testAssert(block1->offset >= 100);
		testAssert(block1->size >= 100);
	}

	//=========== Test expand with an allocated block at end of mem ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(1020, 4);
		testAssert(block0);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 1024);

		allocator->expand(2048);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1);
		testAssert(block1->offset >= 1024);
		testAssert(block1->size >= 100);
	}
}


#endif // BUILD_TESTS
