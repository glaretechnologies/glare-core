/*=====================================================================
ThreadSafeRefCounted.h
----------------------
Copyright Glare Technologies Limited 2018 - 
=====================================================================*/
#pragma once


#include <cassert>
#include "IndigoAtomic.h"


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
		assert(refcount == 0);
	}

	// Returns previous reference count
	inline glare_atomic_int decRefCount() const
	{ 
		return refcount.decrement();
	}

	inline void incRefCount() const
	{ 
		refcount.increment();
	}

	inline glare_atomic_int getRefCount() const 
	{
		return refcount;
	}
	
private:
	INDIGO_DISABLE_COPY(ThreadSafeRefCounted)

	mutable IndigoAtomic refcount;
};
