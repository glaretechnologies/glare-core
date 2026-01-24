/*=====================================================================
WeakRefCounted.h
----------------
Copyright Glare Technologies Limited 2024 - 
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_WEAKREFCOUNTED_H
#define GLARE_WEAKREFCOUNTED_H


#include "Reference.h"
#include "ThreadSafeRefCounted.h"
#include "Mutex.h"
#include "Lock.h"
#include <cassert>


/*=====================================================================
WeakRefCounted
--------------
A reference-counted object, that may have both strong and weak references to it.

See WeakReference.h

_________________________                       ________________________________
|  WeakRefCounted       |                       |  WeakRefCountedControlBlock  |
|                       |                       |                              |
|       control_block---|---------------------->|                              |
|                       |                       |                              |
|   (strong) ref count  |                       |        ob_is_alive           |
|                       |                       |                              |
-------------------------                       --------------------------------
     ^               ^                                       ^
     |               |                                       |
     |                --------------------------------       |
     |                                                |      |
     |                                                |      |
________________________                       ________________________________
|  Reference (strong)  |                       |      WeakReference           |
|                      |                       |                              |
|                      |                       |                              |
|                      |                       |                              |
|                      |                       |                              |
------------------------                       --------------------------------


=====================================================================*/

class WeakRefCountedControlBlock : public ThreadSafeRefCounted
{
public:
	WeakRefCountedControlBlock() : ob_is_alive(1) {}
	int64 ob_is_alive			GUARDED_BY(ob_is_alive_mutex);
	Mutex ob_is_alive_mutex;
};


class WeakRefCounted
{
public:
	WeakRefCounted() : refcount(0), control_block(new WeakRefCountedControlBlock()) {}

	// We don't want a virtual destructor in this class as we don't want to force derived classes to be polymorphic (e.g. to require a vtable).
	~WeakRefCounted()
	{
		assert(refcount == 0);
	}

	/// Increment reference count
	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		refcount.increment();
	}

	/// Returns previous reference count
	inline glare::atomic_int decRefCount() const
	{
		Lock lock(control_block->ob_is_alive_mutex); // Needs to go before refcount.decrement() to prevent another thread from reading ob_is_alive=1 after refcount.decrement() but before control_block->ob_is_alive = 0;.

		const glare::atomic_int prev_ref_count = refcount.decrement();

		// If the last strong reference has been removed, mark this object as dead in the control block, so that any weak references will know it is dead.
		if(prev_ref_count == 1)
			control_block->ob_is_alive = 0;

		return prev_ref_count;
	}

	inline glare::atomic_int getRefCount() const
	{ 
		assert(refcount >= 0);
		return refcount; 
	}
	
private:
	// Classes inheriting from WeakRefCounted shouldn't rely on the default copy constructor or assignment operator, 
	// as these may cause memory leaks when refcount is copied directly.
	GLARE_DISABLE_COPY(WeakRefCounted)

	mutable glare::AtomicInt refcount; // Strong reference count
public:
	Reference<WeakRefCountedControlBlock> control_block;
};


#endif // GLARE_WEAKREFCOUNTED_
