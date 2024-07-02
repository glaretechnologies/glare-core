/*=====================================================================
ThreadSafeRefCounted.h
----------------------
Copyright Glare Technologies Limited 2020 - 
=====================================================================*/
#pragma once


#include "AtomicInt.h"
#include <cassert>


/*=====================================================================
ThreadSafeRefCounted
--------------------
This is a 'mixin' class that adds an atomic refcount and a few methods to increment and decrement the ref count etc..
Derive from this to make a class reference-counted.
=====================================================================*/
class ThreadSafeRefCounted
{
public:
	ThreadSafeRefCounted() : refcount(0) {}

	// We don't want a virtual destructor in this class as we don't want to force derived classes to be polymorphic (e.g. to require a vtable).
	~ThreadSafeRefCounted()
	{
		assert(refcount == 0 || refcount == 0xDADB0DAFDADB0DAull);
	}

	// Returns previous reference count
	inline glare::atomic_int decRefCount() const
	{ 
		return refcount.decrement();
	}

	inline void incRefCount() const
	{ 
		refcount.increment();
	}

	inline glare::atomic_int getRefCount() const 
	{
		return refcount;
	}

	// Mark the object as not independently allocated on the heap.
	// This can be used for objects allocated on the program stack, or objects embedded inside other objects.
	// Adding this large value prevents the reference count for such objects from reaching zero and being incorrectly deleted.
	// This idea is from Jolt physics Reference.h.
	inline void setAsNotIndependentlyHeapAllocated()
	{
		refcount += 0xDADB0DAFDADB0DAull;
	}
	
private:
	GLARE_DISABLE_COPY(ThreadSafeRefCounted)

	mutable glare::AtomicInt refcount;
};
