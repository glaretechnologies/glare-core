/*=====================================================================
PoolMap.h
---------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "BitField.h"
#include "Vector.h"
#include "Exception.h"
#include "../maths/mathstypes.h"
#include <unordered_map>
#include <limits>


/*=====================================================================
PoolMap
-------
Allocates objects with a block-based pool allocator, while supporting fast addition, removal and lookup with a hash table.

Supports the following operations:
Value* allocateWithKey(key) - allocates a new value object, stores in map, associated with key.

void erase(const Key& key) - removes an object for the key from map

iterator find(key) - looks up object for key.   Returns end() if not found.

use iterator::getValuePtr() to get a pointer to the stored value.

begin() and end() to iterate over objects stored in map.


Example:
--------
ob = pool_map.allocateWithKey(key);

it res = pool_map.find(key);
if(res != pool_map.end())
{
	ob2 = res.getValuePtr();
}


Some example iteration perf results:
------------------------------------
PoolMap: Iteration over 7300 objects took 0.00002900 s (3.973 ns per object)
PoolMap: Iteration over 7300 objects with ob access took 0.00002870 s (3.931 ns per object)
std::unordered_map: Iteration over 7300 objects took 0.00006560 s (8.986 ns per object)
std::unordered_map: Iteration over 7300 objects with ob access took 0.00007920 s (10.85 ns per object)
std::map: Iteration over 7300 objects took 0.00007820 s (10.71 ns per object)
std::map: Iteration over 7300 objects with ob access took 0.00008570 s (11.74 ns per object)


TODO
----
TODO: Combine PoolVector with PoolAllocator.h class.
TODO: Use link-list pointers for free and used lists?  Might make everything faster.
=====================================================================*/
namespace glare
{


class PoolVectorBlockInfo
{
public:
	PoolVectorBlockInfo() {}

	static const size_t num_used_bitfields = 1024 / 64; // Numerator must match block_capacity below.

	BitField<uint64> used[num_used_bitfields]; // A bitmask where 0 = slot not used, 1 = slot used.
	uint8* data;
};


class PoolVector;

class PoolVectorIterator
{
public:
	PoolVectorIterator() {}
	PoolVectorIterator(PoolVector* pool_vector_, uint64 index_, void* data_) : pool_vector(pool_vector_), index(index_), data(data_) {}

	inline bool operator == (const PoolVectorIterator& other) const { assert(other.pool_vector == pool_vector); return index == other.index; }
	inline bool operator != (const PoolVectorIterator& other) const { assert(other.pool_vector == pool_vector); return index != other.index; }

	inline void operator ++ (); // Prefix increment operator

	//uint32 block_index;
	//uint32 in_block_index;
	PoolVector* pool_vector;
	uint64 index;
	void* data;
};


class PoolVectorConstIterator
{
public:
	PoolVectorConstIterator() {}
	PoolVectorConstIterator(const PoolVector* pool_vector_, uint64 index_, void* data_) : pool_vector(pool_vector_), index(index_), data(data_) {}

	inline bool operator == (const PoolVectorConstIterator& other) const { assert(other.pool_vector == pool_vector); return index == other.index; }
	inline bool operator != (const PoolVectorConstIterator& other) const { assert(other.pool_vector == pool_vector); return index != other.index; }

	inline void operator ++ (); // Prefix increment operator

	const PoolVector* pool_vector;
	uint64 index;
	void* data;
};


// A Pool allocator which allows iteration over the used slots.  TODO: Combine with PoolAllocator.h class.
// TODO: Use link-list pointers for free and used lists?  Might make everything faster.
class PoolVector
{
public:
	static const size_t block_capacity = 1024; // Max num objects per block.

	// When calling alloc(), the requested_alignemnt passed to alloc() must be <= alignment_.
	PoolVector(size_t ob_alloc_size_, size_t alignment_)
	:	ob_alloc_size(ob_alloc_size_), alignment(alignment_)
	{}

	virtual ~PoolVector()
	{
		for(size_t i=0; i<blocks.size(); ++i)
			MemAlloc::alignedFree(blocks[i].data);
	}

	size_t numAllocatedBlocks() const
	{
		return blocks.size();
	}

	virtual PoolVectorIterator alloc()
	{
		for(size_t block_i=0; block_i<blocks.size(); ++block_i)
		{
			PoolVectorBlockInfo& block = blocks[block_i];

			for(size_t z=0; z<PoolVectorBlockInfo::num_used_bitfields; ++z)
			{
				if(block.used[z].v != std::numeric_limits<uint64>::max()) // If bitfield is not entirely used:
				{
					const uint32 free_index = BitUtils::lowestZeroBitIndex(block.used[z].v); // Get first available free slot in bitfield
					assert(block.used[z].getBit(free_index) == 0);

					block.used[z].setBitToOne(free_index); // Mark spot in bitfield as used

					return PoolVectorIterator(
						this,
						block_i * block_capacity + (z * 64 + free_index), // index
						block.data + ob_alloc_size * (z * 64 + free_index) // data ptr
					);
				}
			}
		}

		// No free room in any existing blocks.  So create a new block and allocate at the start of the new block.
		blocks.push_back(PoolVectorBlockInfo());
		PoolVectorBlockInfo& new_block = blocks.back();
		new_block.data = (uint8*)MemAlloc::alignedMalloc(block_capacity * ob_alloc_size, alignment);

		for(size_t i=0; i<PoolVectorBlockInfo::num_used_bitfields; ++i)
			new_block.used[i].v = 0;
		
		new_block.used[0].setBitToOne(0); // Mark spot in block as used

		return PoolVectorIterator(
			this,
			(blocks.size() - 1) * block_capacity, // index
			new_block.data // data
		);
	}

	virtual void free(PoolVectorIterator it)
	{
		const size_t block_index    = it.index / block_capacity;
		const size_t in_block_index = it.index % block_capacity;
		const size_t bitfield_index    = in_block_index / 64;
		const size_t in_bitfield_index = in_block_index % 64;
		blocks[block_index].used[bitfield_index].setBitToZero((uint32)in_bitfield_index);
	}


	void* getPtrForIterator(const PoolVectorIterator& it)
	{
		const size_t block_index    = it.index / block_capacity;
		const size_t in_block_index = it.index % block_capacity;
		return blocks[block_index].data + in_block_index * ob_alloc_size;
	}

	const void* getPtrForConstIterator(const PoolVectorConstIterator& it) const
	{
		const size_t block_index    = it.index / block_capacity;
		const size_t in_block_index = it.index % block_capacity;
		return blocks[block_index].data + in_block_index * ob_alloc_size;
	}

	PoolVectorIterator begin()
	{
		PoolVectorConstIterator const_begin_it = constBegin();
		return PoolVectorIterator(this,
			const_begin_it.index,
			const_begin_it.data
		);
	}

	PoolVectorConstIterator constBegin() const
	{
		// Iterate through blocks until we find the first used spot
		for(size_t block_i=0; block_i<blocks.size(); ++block_i)
		{
			const PoolVectorBlockInfo& block = blocks[block_i];
			for(size_t z=0; z<PoolVectorBlockInfo::num_used_bitfields; ++z)
			{
				if(block.used[z].v != 0) // If bitfield is not entirely empty:
				{
					const uint32 first_used_index = BitUtils::lowestSetBitIndex(block.used[z].v); // Get first used index from bitfield

					return PoolVectorConstIterator(this, 
						block_i * block_capacity + z * 64 + first_used_index, // index
						block.data + first_used_index * ob_alloc_size // data ptr
					);
				}
			}
		}
		return constEnd(); // No items in vector.
	}

	PoolVectorIterator end()
	{
		return PoolVectorIterator(this, blocks.size() * block_capacity, NULL);
	}

	PoolVectorConstIterator constEnd() const
	{
		return PoolVectorConstIterator(this, blocks.size() * block_capacity, NULL);
	}

	// returns next index with an item
	uint64 getNextIteratorIndex(uint64 index, void*& new_data_out) const
	{
		size_t block_index    = index / block_capacity;
		size_t in_block_index = index % block_capacity;

		// Look in current block for a next used spot
		{
			const PoolVectorBlockInfo& block = blocks[block_index];

			      size_t bitfield_index    = in_block_index / 64;
			const size_t in_bitfield_index = in_block_index % 64;

			// Look in this bitfield.
			if(in_bitfield_index < 63)
			{
				/*
				bit:
				63   62  61                              3   2   1   0

				bit value:
				0    0   0                               1   0   1   0
				                                             ^
				                                   in_bitfield_index

				lets say in_bitfield_index = 2.  We are only interested in bits to the left of in_bitfield_index.
				In this particular example, the next used index is 3.
				We want to ignore the rightmost 3 bits.
				We can do this by shifting right by 3 bits then shifting back left (shifting in zeroes)
				Note that for the shift to be defined, we have to shift by < 64 bits.
				*/
				const uint64 used_block_val = block.used[bitfield_index].v;
				const uint64 masked = (used_block_val >> (in_bitfield_index + 1)) << (in_bitfield_index + 1); // Zero out bits 0...in_bitfield_index
				if(masked != 0)
				{
					const uint32 next_used_index = BitUtils::lowestSetBitIndex(masked); // Get first used slot
					
					new_data_out = block.data + (bitfield_index * 64 + next_used_index) * ob_alloc_size;
					return block_index * block_capacity + bitfield_index * 64 + next_used_index;
				}
			}

			// Look in next bitfields in this block
			bitfield_index++;
			for(/*size_t z=bitfield_index + 1*/; bitfield_index<PoolVectorBlockInfo::num_used_bitfields; ++bitfield_index)
			{
				const uint64 bitfield_val = block.used[bitfield_index].v;
				if(bitfield_val != 0) // If bitfield is not entirely empty:
				{
					const uint32 next_used_index = BitUtils::lowestSetBitIndex(bitfield_val); // Get first used slot

					new_data_out = block.data + (bitfield_index * 64 + next_used_index) * ob_alloc_size;
					return block_index * block_capacity + bitfield_index * 64 + next_used_index;
				}
			}
		}

		// We haven't found a next used slot in block i.  Start looking in block i + 1.
		block_index++;

		// Iterate through next blocks until we find the next used spot
		for(; block_index<blocks.size(); ++block_index)
		{
			const PoolVectorBlockInfo& block = blocks[block_index];

			for(size_t z=0; z<PoolVectorBlockInfo::num_used_bitfields; ++z)
			{
				const uint64 bitfield_val = block.used[z].v;
				if(bitfield_val != 0) // If bitfield is not entirely empty:
				{
					const uint32 next_used_index = BitUtils::lowestSetBitIndex(bitfield_val); // Get first used slot

					new_data_out = block.data + (z * 64 + next_used_index) * ob_alloc_size;
					return block_index * block_capacity + z * 64 + next_used_index;
				}
			}
		}

		new_data_out = NULL;
		return constEnd().index; // No next used item.
	}

	js::Vector<PoolVectorBlockInfo, 16> blocks;
	size_t ob_alloc_size, alignment;
};


void PoolVectorIterator::operator ++ () // Prefix increment operator
{
	const uint64 new_index = pool_vector->getNextIteratorIndex(index, /*new data out=*/this->data);
	assert(new_index > index);
	index = new_index;
}

void PoolVectorConstIterator::operator ++ () // Prefix increment operator
{
	const uint64 new_index = pool_vector->getNextIteratorIndex(index, /*new data out=*/this->data);
	assert(new_index > index);
	index = new_index;
}



template <class Key, class Value>
class PoolMapIterator
{
public:
	PoolMapIterator(PoolVectorIterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	//Value* getValuePtr() { return (Value*)pool_iterator.pool_vector->getPtrForIterator(pool_iterator); }
	Value* getValuePtr() { return (Value*)pool_iterator.data; }

	inline bool operator == (const PoolMapIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const PoolMapIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	PoolVectorIterator pool_iterator;
};


template <class Key, class Value>
class PoolMapConstIterator
{
public:
	PoolMapConstIterator(PoolVectorConstIterator pool_iterator_) : pool_iterator(pool_iterator_) {}

	//const Value* getValuePtr() { return (const Value*)pool_iterator.pool_vector->getPtrForConstIterator(pool_iterator); }
	const Value* getValuePtr() { return (const Value*)pool_iterator.data; }

	inline bool operator == (const PoolMapConstIterator<Key, Value>& other) const { return pool_iterator == other.pool_iterator; }
	inline bool operator != (const PoolMapConstIterator<Key, Value>& other) const { return pool_iterator != other.pool_iterator; }

	inline void operator ++ () // Prefix increment operator
	{
		++pool_iterator;
	}

	PoolVectorConstIterator pool_iterator;
};


struct PoolMapKeyInfo
{
	PoolVectorIterator pool_iterator;
};


template <class Key, class Value, class KeyHasher>
class PoolMap
{
public:
	PoolMap()
	:	pool_vector(Maths::roundUpToMultipleOfPowerOf2<size_t>(sizeof(Value), 32), /*alignment=*/32)
	{}

	~PoolMap()
	{
		// Destroy objects still in pool
		for(auto it = pool_vector.begin(); it != pool_vector.end(); ++it)
		{
			Value* val = (Value*)pool_vector.getPtrForIterator(it);

			(val)->~Value(); // Call destructor
		}
	}

	Value* allocateWithKey(const Key& key)
	{
		auto res = key_info_map.find(key);
		if(res == key_info_map.end())
		{
			PoolVectorIterator it = pool_vector.alloc();

			new (it.data) Value; // Placement-new construct

			PoolMapKeyInfo keyinfo;
			keyinfo.pool_iterator = it;
			key_info_map.insert(std::make_pair(key, keyinfo));

			return (Value*)it.data;
		}
		else
			throw glare::Exception("Something with that key already inserted.");
	}


	void erase(const Key& key)
	{
		auto res = key_info_map.find(key);
		if(res != key_info_map.end())
		{
			const PoolVectorIterator iterator = res->second.pool_iterator;

			Value* ptr = (Value*)pool_vector.getPtrForIterator(iterator);

			(ptr)->~Value(); // Call destructor

			pool_vector.free(iterator);

			key_info_map.erase(res);
		}
	}


	PoolMapIterator<Key, Value> find(const Key& key)
	{
		auto res = key_info_map.find(key);
		if(res == key_info_map.end())
			return end();
		else
		{
			const PoolVectorIterator iterator = res->second.pool_iterator;
			return PoolMapIterator<Key, Value>(iterator);
		}
	}

	typedef PoolMapIterator<Key, Value> iterator;
	

	PoolMapIterator<Key, Value> begin()
	{
		return PoolMapIterator<Key, Value>(pool_vector.begin());
	}

	PoolMapConstIterator<Key, Value> begin() const
	{
		return PoolMapConstIterator<Key, Value>(pool_vector.constBegin());
	}

	PoolMapIterator<Key, Value> end()
	{
		return PoolMapIterator<Key, Value>(pool_vector.end());
	}

	PoolMapConstIterator<Key, Value> end() const
	{
		return PoolMapConstIterator<Key, Value>(pool_vector.constEnd());
	}

	size_t size() const { return key_info_map.size(); }

	PoolVector pool_vector;
	std::unordered_map<Key, PoolMapKeyInfo, KeyHasher> key_info_map;
};


void testPoolMap();


} // End namespace glare
