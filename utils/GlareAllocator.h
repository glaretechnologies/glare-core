/*=====================================================================
GlareAllocator.h
----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "MemAlloc.h"
#include "GlareString.h"


namespace glare
{


class Allocator : public ThreadSafeRefCounted
{
public:
	virtual ~Allocator() {}
	virtual void* alloc(size_t size, size_t alignment) = 0;
	virtual void free(void* ptr) = 0;

	virtual glare::String getDiagnostics() const { return glare::String(); }
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

	virtual glare::String getDiagnostics() const { return glare::String("MallocAllocator [no info]\n"); }
};


} // End namespace glare
