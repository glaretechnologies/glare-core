/*=====================================================================
MemAlloc.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <stddef.h> // For size_t


/*=====================================================================
MemAlloc
----------
Functions for aligned memory allocation and freeing.
=====================================================================*/
namespace MemAlloc
{


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


void* alignedMalloc(size_t size, size_t alignment); // throws std::bad_alloc on allocation failure.
void alignedFree(void* mem);


inline void* alignedSSEMalloc(size_t size)
{
	return alignedMalloc(size, 16);
}

template <class T>
inline void alignedSSEFree(T* t)
{
	alignedFree((void*)t);
}

template <class T>
inline void alignedSSEArrayMalloc(size_t numelems, T*& t_out)
{
	const size_t memsize = sizeof(T) * numelems;
	t_out = static_cast<T*>(alignedSSEMalloc(memsize));
}

template <class T>
inline void alignedArrayMalloc(size_t numelems, size_t alignment, T*& t_out)
{
	const size_t memsize = sizeof(T) * numelems;
	t_out = static_cast<T*>(alignedMalloc(memsize, alignment));
}

template <class T>
inline void alignedSSEArrayFree(T* t)
{
	alignedSSEFree(t);
}

template <class T>
inline void alignedArrayFree(T* t)
{
	alignedFree(t);
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
