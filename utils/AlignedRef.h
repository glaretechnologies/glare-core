/*=====================================================================
AlignedRef.h
------------
File created by ClassTemplate on Mon Mar 30 11:45:11 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __ALIGNEDREF_H_666_
#define __ALIGNEDREF_H_666_


#include "../maths/SSE.h"


/*=====================================================================
AlignedRef
----------
Handle to a reference-counted object.
Referenced object will be automatically deleted when no refs to it remain.
T must be a subclass of 'RefCounted'
=====================================================================*/
template <class T, unsigned int alignment>
class AlignedRef
{
public:
	/*=====================================================================
	Reference
	---------
	
	=====================================================================*/
	AlignedRef() : ob(0)
	{}

	explicit AlignedRef(T* ob_)
	{
		ob = ob_;

		if(ob)
		{
			assert(SSE::isAlignedTo(ob, alignment));
			ob->incRefCount();
		}
	}

	~AlignedRef()
	{
		if(ob)
		{
			ob->decRefCount();
			if(ob->getRefCount() == 0)
			{
				ob->~T(); // Explicit destructor call
				SSE::alignedFree(ob);
			}
		}
	}

	AlignedRef(const AlignedRef<T, alignment>& other)
	{
		ob = other.ob;

		if(ob)
		{
			assert(SSE::isAlignedTo(ob, alignment));
			ob->incRefCount();
		}
	}

	AlignedRef& operator = (const AlignedRef& other)
	{
		if(&other == this) // Assigning a reference object to itself
			return *this;

		//-----------------------------------------------------------------
		//dec old ref that this object used to contain
		//-----------------------------------------------------------------
		if(ob)
		{
			ob->decRefCount();
			if(ob->getRefCount() == 0)
			{
				ob->~T(); // Explicit destructor call
				SSE::alignedFree(ob);
			}
		}

		ob = other.ob;

		if(ob)
			ob->incRefCount();

		return *this;
	}

	//less than is defined as less than for the pointed to objects
	bool operator < (const AlignedRef& other) const
	{
		return *ob < *other.ob;
	}


	inline T& operator * ()
	{
		assert(ob);
		return *ob;
	}

	inline T* operator -> ()
	{
		assert(ob);
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



#endif //__ALIGNEDREF_H_666_




