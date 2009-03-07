/*=====================================================================
refcounted.h
------------
Copyright Glare Technologies Limited 2009 - 
=====================================================================*/
#ifndef __REFCOUNTED_H_666_
#define __REFCOUNTED_H_666_


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

	inline void decRefCount()
	{ 
		refcount--;
		assert(refcount >= 0);
		//if(refcount == 0)
		//	doNoRefsLeft();
	}

	inline void incRefCount()
	{ 
		assert(refcount >= 0);
		refcount++; 
	}

	inline int getRefCount() const 
	{ 
		assert(refcount >= 0);
		return refcount; 
	}
	
private:
	//virtual void doNoRefsLeft(){}

	int refcount;
};



#endif //__REFCOUNTED_H_666_




