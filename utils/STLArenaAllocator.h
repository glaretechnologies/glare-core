/*=====================================================================
STLArenaAllocator.h
-------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "ArenaAllocator.h"
#include "MemAlloc.h"


namespace glare
{


/*=====================================================================
STLArenaAllocator
-----------------
Adapted from Jolt's STLAllocator.
=====================================================================*/
template <typename T>
class STLArenaAllocator
{
public:
	using value_type = T;

	/// Pointer to type
	using pointer = T *;
	using const_pointer = const T *;

	/// Reference to type.
	/// Can be removed in C++20.
	using reference = T &;
	using const_reference = const T &;

	using size_type = size_t;
	using difference_type = ptrdiff_t;

	using is_always_equal = std::false_type;

	// Allocator supports moving
	using propagate_on_container_move_assignment = std::true_type;
	using propagate_on_container_copy_assignment = std::true_type;

	// Constructor
	inline STLArenaAllocator() : arena_allocator(nullptr) {}
	inline STLArenaAllocator(glare::ArenaAllocator* arena_allocator_) : arena_allocator(arena_allocator_) {}

	// Constructor from other allocator
	template <typename T2>
	inline STLArenaAllocator(const STLArenaAllocator<T2> & other) :  arena_allocator(other.arena_allocator) {}

	// If this allocator needs to fall back to aligned allocations because the type requires it
	static constexpr bool needs_aligned_allocate = alignof(T) > GLARE_MALLOC_ALIGNMENT;

	// Allocate memory
	inline pointer allocate(size_type N)
	{
		static_assert((alignof(T) & (alignof(T) - 1)) == 0); // check alignof(T) is a power of 2
		if(arena_allocator)
			return (pointer)arena_allocator->alloc(N * sizeof(value_type), alignof(T));
		else
		{
			if(needs_aligned_allocate)
				return (pointer)MemAlloc::alignedMalloc(N * sizeof(value_type), alignof(T));
			else
				return (pointer)malloc(N * sizeof(value_type));
		}
	}

	// Should we expose a reallocate function?
	static constexpr bool has_reallocate = false;

	// Free memory
	inline void deallocate(pointer ptr, size_type)
	{
		if(arena_allocator)
			arena_allocator->free(ptr);
		else
		{
			if(needs_aligned_allocate)
				MemAlloc::alignedFree(ptr);
			else
				free(ptr);
		}
	}

	inline bool operator == (const STLArenaAllocator<T> & other) const
	{
		return arena_allocator == other.arena_allocator;
	}

	inline bool	operator != (const STLArenaAllocator<T> & other) const
	{
		return arena_allocator != other.arena_allocator;
	}

	/// Converting to allocator for other type
	template <typename T2>
	struct rebind
	{
		using other = STLArenaAllocator<T2>;
	};

	glare::ArenaAllocator* arena_allocator;
};


void testSTLArenaAllocator();


} // End namespace glare
