/*=====================================================================
MemAlloc.h
----------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <stddef.h> // For size_t


#if GLARE_PLATFORM_IS_64_BIT == 1
	#define GLARE_MALLOC_ALIGNMENT 16
#elif GLARE_PLATFORM_IS_64_BIT == 0
	#define GLARE_MALLOC_ALIGNMENT 8
#else
	#error GLARE_PLATFORM_IS_64_BIT not defined
#endif


/*=====================================================================
MemAlloc
----------
Functions for memory allocation and freeing.
=====================================================================*/
namespace MemAlloc
{

// Enable this define to track the total size of all memory allocations
// #define TRACE_ALLOCATIONS 1


// If TRACE_ALLOCATIONS is 1, all malloc and free calling code should ideally call these instead, so that total memory an be tracked.
void* traceMalloc(size_t n);
void traceFree(void* ptr);


size_t getTotalAllocatedB();
size_t getNumAllocations();
size_t getNumActiveAllocations();
size_t getHighWaterMarkB();


// Throws std::bad_alloc on allocation failure.
// Returns memory with at least GLARE_MALLOC_ALIGNMENT alignement: 16 bytes on 64-bit platforms and 8 bytes on 32-bit platforms.
void* mallocWithDefaultAlignmentAndThrow(size_t size);
void freeWithDefaultAlignmentAndThrow(void* mem);

// Allocate memory with arbitrary power-of-2 alignment.
// Memory allocated with this function *must* be freed with alignedFree.
void* alignedMalloc(size_t size, size_t alignment); // throws std::bad_alloc on allocation failure.
void alignedFree(void* mem);


// Allocate memory with 16-byte alignment, suitable for SSE vectors.
// Memory allocated with this function *must* be freed with alignedSSEFree.
inline void* alignedSSEMalloc(size_t size)
{
#if GLARE_MALLOC_ALIGNMENT == 16
	return mallocWithDefaultAlignmentAndThrow(size);
#else
	return alignedMalloc(size, 16);
#endif
}

inline void alignedSSEFree(void* mem)
{
#if GLARE_MALLOC_ALIGNMENT == 16
	freeWithDefaultAlignmentAndThrow(mem);
#else
	alignedFree(mem);
#endif
}

template <class T>
inline bool isAlignedTo(T* ptr, uintptr_t alignment)
{
	return (uintptr_t(ptr) % alignment) == 0;
}

template <class T>
inline bool isSSEAligned(T* ptr)
{
	return isAlignedTo(ptr, 16);
}



void test();


}; // End namespace MemAlloc


#if GLARE_MALLOC_ALIGNMENT == 16
// On 64-bit platforms, malloc will return 16-byte aligned memory so we don't need to use MemAlloc::alignedMalloc().
#define GLARE_ALIGNED_16_NEW_DELETE \
	void* operator new  (size_t size) { return MemAlloc::mallocWithDefaultAlignmentAndThrow(size); } \
	void* operator new[](size_t size) { return MemAlloc::mallocWithDefaultAlignmentAndThrow(size); } \
	void operator delete  (void* ptr) { MemAlloc::freeWithDefaultAlignmentAndThrow(ptr); } \
	void operator delete[](void* ptr) { MemAlloc::freeWithDefaultAlignmentAndThrow(ptr); }
#else
#define GLARE_ALIGNED_16_NEW_DELETE \
	void* operator new  (size_t size) { return MemAlloc::alignedMalloc(size, 16); } \
	void* operator new[](size_t size) { return MemAlloc::alignedMalloc(size, 16); } \
	void operator delete  (void* ptr) { MemAlloc::alignedFree(ptr); } \
	void operator delete[](void* ptr) { MemAlloc::alignedFree(ptr); }
#endif

#define GLARE_ALIGNED_32_NEW_DELETE \
	void* operator new  (size_t size) { return MemAlloc::alignedMalloc(size, 32); } \
	void* operator new[](size_t size) { return MemAlloc::alignedMalloc(size, 32); } \
	void operator delete  (void* ptr) { MemAlloc::alignedFree(ptr); } \
	void operator delete[](void* ptr) { MemAlloc::alignedFree(ptr); }


// GLARE_ALIGNMENT - get alignment of type
#ifdef _MSC_VER
// alignof doesn't work in VS2012, so we have to use __alignof.
#define GLARE_ALIGNMENT __alignof
#else
#define GLARE_ALIGNMENT alignof
#endif

#define assertSSEAligned(p) (assert(MemAlloc::isSSEAligned((p))))
