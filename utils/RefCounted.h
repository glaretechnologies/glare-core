/*=====================================================================
RefCounted.h
------------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#pragma once


#include <cassert>


/*=====================================================================
RefCounted
----------
derive from this to make class reference counted.
Raw pointers to this subclasses are illegal.
Use Reference<Subclass> instead.
=====================================================================*/
class RefCounted
{
public:
	/*=====================================================================
	RefCounted
	----------
	
	=====================================================================*/
	RefCounted()
	:	refcount(0)
	{
	}

	virtual ~RefCounted()
	{
		assert(refcount == 0);
	}

	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		refcount++; 
	}

	// Returns resulting decremented reference count
	inline int decRefCount() const
	{ 
		refcount--;
		assert(refcount >= 0);
		return refcount;
	}

	inline int getRefCount() const 
	{ 
		assert(refcount >= 0);
		return refcount; 
	}
	
private:
	mutable int refcount;
};
