/*=====================================================================
Maybe.h
-----------
Copyright Glare Technologies Limited 2012 - 
=====================================================================*/
#pragma once


#include "Reference.h"
#include <cassert>
#include <stdlib.h> // for NULL


/*=====================================================================
Maybe
---------
Handle to a reference-counted object.
Handle may be 'null' (not reference an object; be invalid)
Referenced object will be automatically deleted when no refs to it remain.
T must be a subclass of 'RefCounted'
=====================================================================*/
template <class T>
class Maybe
{
public:
	/*=====================================================================
	Maybe
	---------
	
	=====================================================================*/
	Maybe()
	:	ob(0)
	{}

	explicit Maybe(T* ob_)
	{
		ob = ob_;

		if(ob)
			ob->incRefCount();
	}


	// Implicit copy-constructor from Reference<T>
	Maybe(const Reference<T>& other)
	{
		ob = other.getPointer();

		if(ob)
			ob->incRefCount();
	}

	template<class T2>
	Maybe(const Maybe<T2>& other)
	{
		ob = other.getPointer();

		if(ob)
			ob->incRefCount();
	}
	

	~Maybe()
	{
		if(ob)
		{
			int new_ref_count = ob->decRefCount();
			if(new_ref_count == 0)
				delete ob;
		}
	}

	Maybe(const Maybe<T>& other)
	{
		ob = other.ob;

		if(ob)
			ob->incRefCount();
	}

	Maybe& operator = (const Maybe& other)
	{
		if(&other == this)//assigning a Maybe object to itself
			return *this;

		//-----------------------------------------------------------------
		//dec old ref that this object used to contain
		//-----------------------------------------------------------------
		if(ob)
		{
			int new_ref_count = ob->decRefCount();
			if(new_ref_count == 0)
				delete ob;
		}

		ob = other.ob;

		if(ob)
			ob->incRefCount();

		return *this;
	}


	Maybe& operator = (const Reference<T>& other)
	{
		//-----------------------------------------------------------------
		//dec old ref that this object used to contain
		//-----------------------------------------------------------------
		if(ob)
		{
			int new_ref_count = ob->decRefCount();
			if(new_ref_count == 0)
				delete ob;
		}

		ob = other.getPointer();

		if(ob)
			ob->incRefCount();

		return *this;
	}



	//less than is defined as less than for the pointed to objects
	bool operator < (const Maybe& other) const
	{
		return *ob < *other.ob;
	}


	inline T& operator * ()
	{
		return *ob;
	}

	

	inline const T& operator * () const
	{
		// Disable a bogus VS 2010 Code analysis warning: 'warning C6011: Dereferencing NULL pointer 'ob': Lines: 111, 112'
		#ifdef _WIN32
		#pragma warning(push)
		#pragma warning(disable:6011)
		#endif
		
		assert(ob);
		return *ob;

		#ifdef _WIN32
		#pragma warning(pop)
		#endif
	}


	inline T* operator -> () const
	{
		return ob;
	}

	inline T* getPointer() const
	{
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

	// NOTE: These upcast functions are not needed any more.  Valid conversions will be done automatically by the compiler.
	template <class T2>
	inline const Maybe<T2> upcast() const
	{
		return Maybe<T2>(ob);
	}

	template <class T2>
	inline Maybe<T2> upcast()
	{
		return Maybe<T2>(ob);
	}


	// Downcast to a Maybe to a derived type.  NOTE: only call this if you are sure the pointer is actually to an object of the derived type.
	// Otherwise behaviour is undefined.
	template <class T2>
	inline const Maybe<T2> downcast() const
	{
		return Maybe<T2>(static_cast<T2*>(ob));
	}

	template <class T2>
	inline Maybe<T2> downcast()
	{
		return Maybe<T2>(static_cast<T2*>(ob));
	}
	
private:
	T* ob;
};
