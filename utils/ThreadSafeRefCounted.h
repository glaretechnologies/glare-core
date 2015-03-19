/*=====================================================================
ThreadSafeRefCounted.h
----------------------
Copyright Glare Technologies Limited 2014 - 
=====================================================================*/
#pragma once


#include <cassert>
#include "IndigoAtomic.h"


/*=====================================================================
ThreadSafeRefCounted
--------------------
derive from this to make class reference counted.
Raw pointers to this subclasses are illegal.
Use Reference<Subclass> instead.
=====================================================================*/
class ThreadSafeRefCounted
{
public:
	ThreadSafeRefCounted()
	:	refcount(0)
	{
	}

	virtual ~ThreadSafeRefCounted()
	{
		assert(refcount == 0);
	}

	// Returns resulting decremented reference count
	inline glare_atomic_int decRefCount() const
	{ 
		return refcount.decrement() - 1;
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
