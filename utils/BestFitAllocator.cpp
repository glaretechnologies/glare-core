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


static const size_t MIN_ALIGNMENT = 4;


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
}


BestFitAllocator::~BestFitAllocator()
{
	//conPrint("BestFitAllocator::~BestFitAllocator()");

	checkInvariants();

	BlockInfo* cur = first;
	while(cur != NULL)
	{
		BlockInfo* next = cur->next;
		delete cur;
		cur = next; // Walk to next block
	}
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


BestFitAllocator::BlockInfo* BestFitAllocator::alloc(size_t size_, size_t requested_alignment)
{
	assert(requested_alignment % 4 == 0);

	const size_t alignment = myMax(MIN_ALIGNMENT, requested_alignment);

	if(size_ == 0)
		size_ = MIN_ALIGNMENT;
	size_ = Maths::roundUpToMultipleOfPowerOf2(size_, MIN_ALIGNMENT);

	// Find smallest free block with size >= size

	/*
	Example where requested_alignment is 16:
	The internal block allocation returns a block with offset 24

	        block offset     aligned offset
    ------------------|-------|-------------------------------------------------------------|
	0          16     24     32          48          64
	
	
	*/

	const size_t use_size = (alignment > MIN_ALIGNMENT) ? (Maths::roundUpToMultiple(size_, alignment) + alignment) : size_;
	
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
		assert(new_remaining_block->offset % MIN_ALIGNMENT == 0);
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
		block->allocator = NULL; // Set the allocator reference to NULL, so that we don't have circular references that prevent the BestFitAllocator from being freed.
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
			assert(cur->aligned_offset % MIN_ALIGNMENT == 0);
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



// Command line:
// C:\fuzz_corpus\best_fit_allocator -max_len=10000 -seed=1

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	return 0;
}

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

				alignment = alignment << 2; // Must be multiple of 4.

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


void glare::BestFitAllocator::test()
{
	//-------------------------- Random stress/fuzz test -------------------------------
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);


		std::set<BlockInfo*> blocks;
		PCG32 rng(1);
		for(int i = 0; i<10000000; ++i)
		{
			if(i % (1 << 16) == 0)
				conPrint("allocated blocks size: " + toString(blocks.size()));
			const float ur = rng.unitRandom();
			if(ur < 0.6f)
			{
				const size_t alloc_size = rng.nextUInt(128);
				const size_t alignment = Maths::roundUpToMultipleOfPowerOf2<uint32>(rng.nextUInt(64), 4);
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
	}






	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block = allocator->alloc(16, 4);
		
		testAssert(block->offset == 0);
		testAssert(block->size == 16);

		BlockInfo* block2 = allocator->alloc(32, 4);

		testAssert(block2->offset == 16);
		testAssert(block2->size == 32);

		BlockInfo* block3 = allocator->alloc(8, 4);

		testAssert(block3->offset == 16 + 32);
		testAssert(block3->size == 8);

		// Free a block
		allocator->free(block2);

		// Allocate 32 bytes again
		BlockInfo* block4 = allocator->alloc(32, 4);

		testAssert(block4->offset == 16);
		testAssert(block4->size == 32);
	}


	//=========== Test some aligned allocs ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(/*size=*/64, /*alignment=*/32);
		testAssert(block0->offset == 0);
		testAssert(block0->aligned_offset == 0);
		testAssert(block0->size >= 64);
		//|--------block0--------|              free                  

		BlockInfo* block1 = allocator->alloc(64, 64);
		//testAssert(block1->offset == 64);
		testAssert(block1->aligned_offset % 64 == 0);
		testAssert(block1->size >= 64);
		//|--------block0--------|--------block1--------|              free                  

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->aligned_offset % 4 == 0);
		testAssert(block2->size >= 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block0);
		//|        block0        |--------block1--------|--------block2--------|              free                  

		BlockInfo* block3 = allocator->alloc(50, 64);
		testAssert(block3->aligned_offset % 64 == 0);
		testAssert(block3->size >= 50);
		//|--block3---|          |--------block1--------|--------block2--------|              free                  
	}

	//=========== Test allocation with no adjacent free blocks, and with remaining size > 0 ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);
		//|--------block0--------|              free                  

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);
		//|--------block0--------|--------block1--------|              free                  

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block0);
		//|        block0        |--------block1--------|--------block2--------|              free                  

		BlockInfo* block3 = allocator->alloc(50, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size == 50);
		//|--block3---|          |--------block1--------|--------block2--------|              free                  
	}


	//=========== Test freeing only block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);
		//|--------block0--------|              free                  

		allocator->free(block0);
	}

	//=========== Test allocating a block the full arena size ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(1024, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 1024);
		//|--------block0--------|              free                  

		allocator->free(block0);
	}

	//=========== Test freeing a block with a free block adjacent to the left, when the adjacent free block is the first block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);
		//|--------block0--------|              free                  

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);
		//|--------block0--------|--------block1--------|              free                  

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block0);
		//|        block0        |--------block1--------|--------block2--------|              free                  

		allocator->free(block1);
		//|        coalesced                            |--------block2--------|              free                  
		
		BlockInfo* block3 = allocator->alloc(200, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size == 200);
		//|----------block3-----------------------------|--------block2--------|              free                  
	}

	//=========== Test freeing a block with a free block adjacent to the left, when the block is the last block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(200); // Note the 200
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);
		//|--------block0--------|        free          |

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);
		//|--------block0--------|--------block1--------|


		allocator->free(block0);
		//|        block0        |--------block1--------|

		allocator->free(block1);
		//|        coalesced                            |
		
		BlockInfo* block3 = allocator->alloc(200, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size == 200);
		//|----------block3-----------------------------|
	}

	//=========== Test freeing a block with a free block adjacent to the left, when the adjacent free block is not the first block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset == 300);
		testAssert(block3->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|              free                  

		allocator->free(block1);
		//|--------block0--------|        block1        |--------block2--------|--------block3--------|              free                  

		allocator->free(block2);
		//|--------block0--------|                  coalesced                  |--------block3--------|              free                  
		
		BlockInfo* block4 = allocator->alloc(200, 4);
		testAssert(block4->offset == 100);
		testAssert(block4->size == 200);
		//|--------block0--------|--------------------block4-------------------|--------block3--------|              free                  
	}

	//=========== Test freeing a block with a free block adjacent to the right ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|              free                  

		allocator->free(block1);
		//|--------block0--------|        block1        |--------block2--------|              free                  

		allocator->free(block0);
		//|        coalesced                            |--------block2--------|              free                  
		
		BlockInfo* block3 = allocator->alloc(200, 4);
		testAssert(block3->offset == 0);
		testAssert(block3->size == 200);
		//|----------block3-----------------------------|--------block2--------|              free                  
	}

	//=========== Test freeing a block with a free block adjacent to the right, and the left block existing. ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset == 300);
		testAssert(block3->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|              free                  

		allocator->free(block2);
		//|--------block0--------|--------block1--------|        block2        |--------block3--------|              free                  

		allocator->free(block1);
		//|--------block0--------|                  coalesced                  |--------block3--------|              free                  
		
		BlockInfo* block4 = allocator->alloc(200, 4);
		testAssert(block4->offset == 100);
		testAssert(block4->size == 200);
		//|--------block0--------|--------------------block4-------------------|--------block3--------|              free                  
	}

	//=========== Test freeing a block with a free block on both sides ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset == 300);
		testAssert(block3->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|              free                  

		allocator->free(block0);
		allocator->free(block2);
		//|        block0        |--------block1--------|        block2        |--------block3--------|              free                  

		allocator->free(block1);
		//|                              coalesced                             |--------block3--------|              free                  
		
		BlockInfo* block4 = allocator->alloc(300, 4);
		testAssert(block4->offset == 0);
		testAssert(block4->size == 300);
		//|--------------------------------block4------------------------------|--------block3--------|              free                  
	}

	//=========== Test freeing a block with a free block on both sides, where the free block on the right is the last block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(300); // Note the 300 arena size here
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);

		//|--------block0--------|--------block1--------|--------block2--------|

		allocator->free(block0);
		allocator->free(block2);
		//|        block0        |--------block1--------|        block2        |

		allocator->free(block1);
		//|                              coalesced                             |
		
		BlockInfo* block4 = allocator->alloc(300, 4);
		testAssert(block4->offset == 0);
		testAssert(block4->size == 300);
		//|--------------------------------block4------------------------------|
	}

	//=========== Test freeing a block with a free block on both sides, and the free block on the left not being the first block ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(100, 4);
		testAssert(block0->offset == 0);
		testAssert(block0->size == 100);

		BlockInfo* block1 = allocator->alloc(100, 4);
		testAssert(block1->offset == 100);
		testAssert(block1->size == 100);

		BlockInfo* block2 = allocator->alloc(100, 4);
		testAssert(block2->offset == 200);
		testAssert(block2->size == 100);

		BlockInfo* block3 = allocator->alloc(100, 4);
		testAssert(block3->offset == 300);
		testAssert(block3->size == 100);

		BlockInfo* block4 = allocator->alloc(100, 4);
		testAssert(block4->offset == 400);
		testAssert(block4->size == 100);
		//|--------block0--------|--------block1--------|--------block2--------|--------block3--------|--------block4--------|              free                  

		allocator->free(block1);
		allocator->free(block3);
		//|--------block0--------|        block1        |--------block2--------|        block3        |--------block4--------|              free                  

		allocator->free(block2);
		//|--------block0--------|                              coalesced                             |--------block4--------|              free                  
		
		BlockInfo* block5 = allocator->alloc(300, 4);
		testAssert(block5->offset == 100);
		testAssert(block5->size == 300);
		//|--------block0--------|-------------------------------block5-------------------------------|--------block4--------|              free                  
	}

	//=========== Test an allocation that won't fit ===============
	{
		BestFitAllocatorRef allocator = new BestFitAllocator(1024);
		BlockInfo* block0 = allocator->alloc(2000, 4);
		testAssert(block0 == NULL);
	}
}


#endif // BUILD_TESTS
