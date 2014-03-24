/*=====================================================================
ThreadSafeRefCounted.h
----------------------
Copyright Glare Technologies Limited 2012 - 
=====================================================================*/
#pragma once


#include <cassert>
#include "Mutex.h"
#include "Lock.h"


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
	/*=====================================================================
	RefCounted
	----------
	
	=====================================================================*/
	ThreadSafeRefCounted()
	:	refcount(0)
	{
	}

	virtual ~ThreadSafeRefCounted()
	{
		assert(refcount == 0);
	}

	// Returns resulting decremented reference count
	inline int decRefCount() const
	{ 
		Lock lock(mutex);
		refcount--;
		assert(refcount >= 0);
		return refcount;
	}

	inline void incRefCount() const
	{ 
		Lock lock(mutex);
		assert(refcount >= 0);
		refcount++; 
	}

	inline int getRefCount() const 
	{
		Lock lock(mutex);
		assert(refcount >= 0);
		return refcount; 
	}
	
private:
	INDIGO_DISABLE_COPY(ThreadSafeRefCounted)
	mutable Mutex mutex;
	mutable int refcount;
};
