/*=====================================================================
MemAlloc.h
----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <stddef.h> // For size_t


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





void* alignedMalloc(size_t size, size_t alignment); // throws std::bad_alloc on allocation failure.
void alignedFree(void* mem);

inline void* alignedSSEMalloc(size_t size)
{
	return alignedMalloc(size, 16);
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


#define GLARE_ALIGNED_16_NEW_DELETE \
	void* operator new  (size_t size) { return MemAlloc::alignedMalloc(size, 16); } \
	void* operator new[](size_t size) { return MemAlloc::alignedMalloc(size, 16); } \
	void operator delete  (void* ptr) { MemAlloc::alignedFree(ptr); } \
	void operator delete[](void* ptr) { MemAlloc::alignedFree(ptr); }

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
