/*=====================================================================
RefCounted.h
------------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#pragma once


#include "Platform.h"
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

	// Returns previous reference count
	inline int decRefCount() const
	{ 
		const int prev_ref_count = refcount;
		refcount--;
		assert(refcount >= 0);
		return prev_ref_count;
	}

	inline int getRefCount() const 
	{ 
		assert(refcount >= 0);
		return refcount; 
	}
	
private:
	// Classes inheriting from RefCounted shouldn't rely on the default copy constructor or assigment operator, 
	// as these may cause memory leaks when refcount is copied directly.
	INDIGO_DISABLE_COPY(RefCounted)

	mutable int refcount;
};
