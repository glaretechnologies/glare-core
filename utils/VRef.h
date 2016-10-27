/*=====================================================================
VRef.h
------
Copyright Glare Technologies Limited 2016 - 
=====================================================================*/
#pragma once


#include "RefCounted.h"
#include "Platform.h"
#include <cassert>


/*=====================================================================
VRef  (Valid Reference)
-----------------------
Handle to a reference-counted object.
Referenced object will be automatically deleted when no refs to it remain.
T must be a subclass of 'RefCounted' or 'ThreadSafeRefCounted'.
=====================================================================*/
template <class T>
class VRef
{
public:
	VRef(T* ob_)
	{
		assert(ob_);
		ob = ob_;
		if(ob)
			ob->incRefCount();
	}

	template<class T2>
	VRef(const VRef<T2>& other)
	{
		assert(other.getPointer());
		ob = other.getPointer();
		if(ob)
			ob->incRefCount();
	}

	// Convert from a reference object.  Note that 'other' must be non-null!
	template<class T2>
	explicit VRef(const Reference<T2>& other)
	{
		assert(other.getPointer());
		ob = other.getPointer();
		if(ob)
			ob->incRefCount();
	}

	VRef(const VRef<T>& other)
	{
		assert(other.ob);
		ob = other.ob;
		if(ob)
			ob->incRefCount();
	}

	~VRef()
	{
		if(ob)
		{
			int64 new_ref_count = ob->decRefCount();
			if(new_ref_count == 0)
				delete ob;
		}
	}


	// Implicit conversion to Reference<T2>
	// We will allow T2 to be a generic type here.
	// This allows e.g. conversion from VRef<T> to Reference<const T>.
	// (Note that 'T' and 'const T' are actually different types)
	template<class T2>
	operator Reference<T2> () { return Reference<T2>(ob); }

	template<class T2>
	operator Reference<T2> () const { return Reference<T2>(ob); }


	VRef& operator = (const VRef& other)
	{
		T* old_ob = ob;

		assert(other.ob);
		ob = other.ob;

		// NOTE: if a VRef is getting assigned to itself, old_ob == ob.  So make sure we increment before we decrement, to avoid deletion.
		if(ob)
			ob->incRefCount();

		// Decrement reference count for the object that this reference used to refer to.
		if(old_ob)
		{
			int64 ref_count = old_ob->decRefCount();
			if(ref_count == 0)
				delete old_ob;
		}

		return *this;
	}


	// Compares the pointer values.
	bool operator < (const VRef& other) const
	{
		return ob < other.ob;
	}


	inline T& operator * ()
	{
		assert(ob);
		return *ob;
	}

	
	inline const T& operator * () const
	{
		assert(ob);
		return *ob;
	}


	inline T* operator -> () const
	{
		return ob;
	}

	inline T* getPointer() const
	{
		return ob;
	}

	// Alias for getPointer()
	inline T* ptr() const
	{
		return ob;
	}


	inline T* getPointer()
	{
		return ob;
	}

	// Alias for getPointer()
	inline T* ptr()
	{
		return ob;
	}


	// Downcast to a reference to a derived type.  NOTE: only call this if you are sure the pointer is actually to an object of the derived type.
	// Otherwise behaviour is undefined.
	template <class T2>
	inline const VRef<T2> downcast() const
	{
#ifdef _WIN32
#ifdef _CPPRTTI 
		assert(dynamic_cast<T2*>(ob));
#endif
#else
#ifdef __GXX_RTTI
		assert(dynamic_cast<T2*>(ob));
#endif
#endif
		return VRef<T2>(static_cast<T2*>(ob));
	}

	template <class T2>
	inline VRef<T2> downcast()
	{
#ifdef _WIN32
#ifdef _CPPRTTI 
		assert(dynamic_cast<T2*>(ob));
#endif
#else
#ifdef __GXX_RTTI
		assert(dynamic_cast<T2*>(ob));
#endif
#endif
		return VRef<T2>(static_cast<T2*>(ob));
	}


	template <class T2>
	inline const T2* downcastToPtr() const
	{
#ifdef _WIN32
#ifdef _CPPRTTI 
		assert(dynamic_cast<const T2*>(ob));
#endif
#else
#ifdef __GXX_RTTI
		assert(dynamic_cast<const T2*>(ob));
#endif
#endif
		return static_cast<const T2*>(ob);
	}

	template <class T2>
	inline T2* downcastToPtr()
	{
#ifdef _WIN32
#ifdef _CPPRTTI 
		assert(dynamic_cast<T2*>(ob));
#endif
#else
#ifdef __GXX_RTTI
		assert(dynamic_cast<T2*>(ob));
#endif
#endif
		return static_cast<T2*>(ob);
	}
	
private:
	T* ob;
};
