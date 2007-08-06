/*=====================================================================
refcounted.h
------------
File created by ClassTemplate on Thu Jul 17 18:16:48 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __REFCOUNTED_H_666_
#define __REFCOUNTED_H_666_



#include <assert.h>

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
	{
		refcount = 0;
	}

	virtual ~RefCounted()
	{
		//too much of hassle this assert for CountedHandle assert(refcount == 0);
	}

	void decRefCount()
	{ 
		refcount--;

		assert(refcount >= 0);

		if(refcount == 0)
			doNoRefsLeft();
	}

	void incRefCount()
	{ 
		assert(refcount >= 0);

		refcount++; 
	}
	int getRefCount() const 
	{ 
		assert(refcount >= 0);

		return refcount; 
	}
	
private:
	virtual void doNoRefsLeft(){}

	int refcount;
};



#endif //__REFCOUNTED_H_666_




