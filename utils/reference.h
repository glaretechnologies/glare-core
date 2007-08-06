/*=====================================================================
reference.h
-----------
File created by ClassTemplate on Thu Jul 17 18:17:42 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __REFERENCE_H_666_
#define __REFERENCE_H_666_


#include "refcounted.h"
#include <assert.h>


/*=====================================================================
Reference
---------
Handle to a reference-counted object.
Referenced object will be automatically deleted when no refs to it remain.
T must be a subclass of 'RefCounted'
=====================================================================*/
template <class T>
class Reference
{
public:
	/*=====================================================================
	Reference
	---------
	
	=====================================================================*/
	Reference(T* ob_)
	{
		ob = ob_;

		if(ob)
			ob->incRefCount();
	}

	~Reference()
	{
		if(ob)
		{
			ob->decRefCount();
			if(ob->getRefCount() == 0)
				delete ob;
		}
	}

	Reference(const Reference<T>& other)
	{
		ob = other.ob;

		if(ob)
			ob->incRefCount();
	}

	Reference& operator = (const Reference& other)
	{
		if(&other == this)//assigning a reference object to itself
			return *this;

		//-----------------------------------------------------------------
		//dec old ref that this object used to contain
		//-----------------------------------------------------------------
		if(ob)
		{
			ob->decRefCount();
			if(ob->getRefCount() == 0)
				delete ob;
		}

		ob = other.ob;

		if(ob)
			ob->incRefCount();

		return *this;
	}

	//equality is defined as equality of the pointed to objects
	bool operator == (const Reference& other) const
	{
		return *ob == *other.ob;
	}

	//less than is defined as less than for the pointed to objects
	bool operator < (const Reference& other) const
	{
		return *ob < *other.ob;
	}


	T& operator * ()
	{
		assert(ob);
		return *ob;
	}

	T* operator -> ()
	{
		assert(ob);
		return ob;
	}

	const T& operator * () const
	{
		assert(ob);
		return *ob;
	}

	const T* operator -> () const
	{
		assert(ob);
		return ob;
	}

	//operator bool is a BAD IDEA. cause mystream thinks refs are bools.
	/*operator bool() const
	{
		return ob != NULL;
	}*/

	bool isNull() const
	{
		return ob == NULL;
	}

	//NOTE: this method is dangerous
	T* getPointer()
	{
		return ob;
	}
	
	const T* getPointer() const
	{
		return ob;
	}

private:
	T* ob;
};



#endif //__REFERENCE_H_666_




