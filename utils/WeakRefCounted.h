/*=====================================================================
WeakRefCounted.h
----------------
Copyright Glare Technologies Limited 2024 - 
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef GLARE_WEAKREFCOUNTED_H
#define GLARE_WEAKREFCOUNTED_H


#include "Reference.h"
#include <cassert>


/*=====================================================================
WeakRefCounted
--------------
A eference-counted object, that may have both strong and weak references to it.

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

class WeakRefCountedControlBlock : public RefCounted
{
public:
	WeakRefCountedControlBlock() : ob_is_alive(true) {}
	bool ob_is_alive;
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
		refcount++;
	}

	/// Returns previous reference count
	inline int64 decRefCount() const
	{
		const int64 prev_ref_count = refcount;
		refcount--;

		// If the last strong reference has been removed, mark this object as dead in the control block, so that any weak references will know it is dead.
		if(refcount == 0)
			control_block->ob_is_alive = false;

		assert(refcount >= 0);
		return prev_ref_count;
	}

	inline int64 getRefCount() const
	{ 
		assert(refcount >= 0);
		return refcount; 
	}
	
private:
	// Classes inheriting from WeakRefCounted shouldn't rely on the default copy constructor or assigment operator, 
	// as these may cause memory leaks when refcount is copied directly.
	GLARE_DISABLE_COPY(WeakRefCounted)

	mutable int64 refcount;
public:
	Reference<WeakRefCountedControlBlock> control_block;
};


#endif // GLARE_WEAKREFCOUNTED_H
