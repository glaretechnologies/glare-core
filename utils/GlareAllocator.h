/*=====================================================================
GlareAllocator.h
----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "MemAlloc.h"
#include <string>
#include <cstring> // for size_t


namespace glare
{


class Allocator : public ThreadSafeRefCounted
{
public:
	virtual ~Allocator() {}
	virtual void* alloc(size_t size, size_t alignment) = 0;
	virtual void free(void* ptr) = 0;

	virtual std::string getDiagnostics() const { return std::string(); }
};


class MallocAllocator : public glare::Allocator
{
	virtual void* alloc(size_t size, size_t alignment)
	{
		return MemAlloc::alignedMalloc(size, alignment);
	}

	virtual void free(void* ptr)
	{
		MemAlloc::alignedFree(ptr);
	}
};


} // End namespace glare
