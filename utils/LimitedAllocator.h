/*=====================================================================
LimitedAllocator.h
------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "GlareAllocator.h"
#include "Exception.h"
#include "Mutex.h"
#include <vector>


namespace glare
{

// LimitedAllocator will throw an exception of this type if it fails to allocate
class LimitedAllocatorAllocFailed : public Exception
{
public:
	LimitedAllocatorAllocFailed(const std::string& msg);
};


class LimitedAllocator : public glare::Allocator
{
public:
	friend class MemReservation;
	LimitedAllocator(size_t max_size_B);
	~LimitedAllocator();

	virtual void* alloc(size_t size, size_t alignment) override;
	virtual void free(void* ptr) override;

	virtual glare::String getDiagnostics() const override;

	void reserve(size_t size);
	void unreserve(size_t size);

	static void test();
private:
	size_t max_size_B;
	
	mutable Mutex mutex;
	size_t used_size_B;
	size_t high_water_mark_B;
};


// Reserves a certain amount of memory (increases the limited_allocator->used_size_B) without actually allocating the memory.
// This is for memory that uses another allocator (e.g. MallocAllocator) and can't use this one for whatever reason.
class MemReservation
{
public:
	MemReservation();
	MemReservation(size_t size_B, LimitedAllocator* limited_allocator); // Throws LimitedAllocatorAllocFailed on failure to reserve
	~MemReservation();

	void reserve(size_t size_B, LimitedAllocator* limited_allocator);

	LimitedAllocator* m_limited_allocator;
	size_t reserved_size;
};


} // End namespace glare
