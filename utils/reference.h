/*=====================================================================
reference.h
-----------
Copyright Glare Technologies Limited 2009 - 
=====================================================================*/
#ifndef __REFERENCE_H_666_
#define __REFERENCE_H_666_


#include "refcounted.h"
#include <cassert>
#include <stdlib.h> // for NULL

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
	Reference()
	:	ob(0)
	{}

	explicit Reference(T* ob_)
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
	/*bool operator == (const Reference& other) const
	{
		return *ob == *other.ob;
	}*/

	//less than is defined as less than for the pointed to objects
	bool operator < (const Reference& other) const
	{
		return *ob < *other.ob;
	}


	inline T& operator * ()
	{
		//assert(ob);
		return *ob;
	}

	inline T* operator -> ()
	{
		//assert(ob);
		return ob;
	}

	inline const T& operator * () const
	{
		assert(ob);
		return *ob;
	}

	inline const T* operator -> () const
	{
		assert(ob);
		return ob;
	}

	inline bool isNull() const
	{
		return ob == 0;
	}
	inline bool nonNull() const
	{
		return ob != 0;
	}

	inline T* getPointer()
	{
		return ob;
	}

	template <class T2>
	inline const Reference<T2> upcast() const
	{
		return Reference<T2>(ob);
	}

	template <class T2>
	inline Reference<T2> upcast()
	{
		return Reference<T2>(ob);
	}


	// Downcast to a reference to a derived type.  NOTE: only call this if you are sure the pointer is actually to an object of the derived type.
	// Otherwise behaviour is undefined.
	template <class T2>
	inline const Reference<T2> downcast() const
	{
		assert(dynamic_cast<const T2*>(ob) != NULL);
		return Reference<T2>(static_cast<const T2*>(ob));
	}

	template <class T2>
	inline Reference<T2> downcast()
	{
		assert(dynamic_cast<T2*>(ob) != NULL);
		return Reference<T2>(static_cast<T2*>(ob));
	}
	
	inline const T* getPointer() const
	{
		return ob;
	}

	/*inline T& getRef()
	{
		return *ob;
	}

	inline const T& getRef() const
	{
		return *ob;
	}*/

private:
	T* ob;
};


//typedef Reference<T> Ref<T>;


#endif //__REFERENCE_H_666_




