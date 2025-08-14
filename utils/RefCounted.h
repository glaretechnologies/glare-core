/*=====================================================================
RefCounted.h
------------
Copyright Glare Technologies Limited 2025 - 
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_REFCOUNTED_H
#define GLARE_REFCOUNTED_H


#include "Platform.h"
#ifndef NDEBUG
#include "PlatformUtils.h"
#endif
#include <cassert>


#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If atomic int is 64-bit:
#define RC_NOT_INDEP_HEAP_ALLOC_REFVAL 0xDADB0DAFDADB0DAull
#else
#define RC_NOT_INDEP_HEAP_ALLOC_REFVAL 0x3DADB0DAull
#endif


/*=====================================================================
RefCounted
----------
This is a 'mixin' class that adds a reference count and a few methods to increment and decrement the reference count etc..
Derive from this to make a class reference-counted.

This class is for objects that are only ever accessed from a single thread.
Use ThreadSafeRefCounted for objects accessed by multiple threads.
=====================================================================*/
class RefCounted
{
public:
	RefCounted() : refcount(0)
	{
#ifndef NDEBUG
		initial_thread_id = PlatformUtils::getCurrentThreadID(); // Store the Id of the thread this object was allocated on.  Used later in checkOnInitialThread().
#endif
	}

	// We don't want a virtual destructor in this class as we don't want to force derived classes to be polymorphic (e.g. to require a vtable).
	~RefCounted()
	{
		checkOnInitialThread();
		assert(refcount == 0 || refcount == RC_NOT_INDEP_HEAP_ALLOC_REFVAL);
	}

	/// Increment reference count
	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		checkOnInitialThread();
		refcount++; 
	}

	/// Returns previous reference count
	inline int64 decRefCount() const
	{ 
		checkOnInitialThread();
		const int64 prev_ref_count = refcount;
		refcount--;
		assert(refcount >= 0);
		return prev_ref_count;
	}

	inline int64 getRefCount() const
	{ 
		assert(refcount >= 0);
		return refcount; 
	}

	// Mark the object as not independently allocated on the heap.
	// This can be used for objects allocated on the program stack, or objects embedded inside other objects.
	// Adding this large value prevents the reference count for such objects from reaching zero and being incorrectly deleted.
	// This idea is from Jolt physics Reference.h.
	inline void setAsNotIndependentlyHeapAllocated()
	{
		refcount += RC_NOT_INDEP_HEAP_ALLOC_REFVAL;
	}
	
private:
	// Classes inheriting from RefCounted shouldn't rely on the default copy constructor or assigment operator, 
	// as these may cause memory leaks when refcount is copied directly.
	GLARE_DISABLE_COPY(RefCounted)

	inline void checkOnInitialThread() const
	{
#ifndef NDEBUG
		assert(PlatformUtils::getCurrentThreadID() == this->initial_thread_id);
#endif
	}

	mutable int64 refcount;

#ifndef NDEBUG
	uint64 initial_thread_id;
#endif
};

#endif // GLARE_REFCOUNTED_H
