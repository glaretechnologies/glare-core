/*=====================================================================
Reference.h
-----------
Copyright Glare Technologies Limited 2020 - 
=====================================================================*/
// Not using pragma once since we copy this file and pragma once is path based
#ifndef REFERENCE_H
#define REFERENCE_H


#include "RefCounted.h"
#include "Platform.h"
#include <cassert>


///
/// Handle to a reference-counted object.
/// Referenced object will be automatically deleted when no refs to it remain.
/// T must be a subclass of 'RefCounted' or 'ThreadSafeRefCounted'.
///
template <class T>
class Reference
{
public:
	/// Initialises to a null reference.
	Reference()
	:	ob(0)
	{}

	/// Initialise as a reference to ob.
	Reference(T* ob_)
	{
		ob = ob_;

		if(ob)
			ob->incRefCount();
	}

	template<class T2>
	Reference(const Reference<T2>& other)
	{
		ob = other.getPointer();

		if(ob)
			ob->incRefCount();
	}

	Reference(const Reference<T>& other)
	{
		ob = other.ob;

		if(ob)
			ob->incRefCount();
	}

	~Reference()
	{
		if(ob)
		{
			const int64 prev_ref_count = ob->decRefCount();
			if(prev_ref_count == 1)
				delete ob;
		}
	}

	
	Reference& operator = (const Reference& other)
	{
		T* old_ob = ob;

		ob = other.ob;
		// NOTE: if a reference is getting assigned to itself, old_ob == ob.  So make sure we increment before we decrement, to avoid deletion.
		if(ob)
			ob->incRefCount();

		// Decrement reference count for the object that this reference used to refer to.
		if(old_ob)
		{
			const int64 prev_ref_count = old_ob->decRefCount();
			if(prev_ref_count == 1)
				delete old_ob;
		}

		return *this;
	}


	/// An assignment operator from a raw pointer.
	/// We have this method so that the code 
	///
	/// Reference<T> ref;
	/// ref = new T();
	///
	/// will work without a temporary reference object being created and then assigned to ref.
	Reference& operator = (T* new_ob)
	{
		T* old_ob = ob;

		ob = new_ob;
		// NOTE: if a reference is getting assigned to itself, old_ob == ob.  So make sure we increment before we decrement, to avoid deletion.
		if(ob)
			ob->incRefCount();

		// Decrement reference count for the object that this reference used to refer to.
		if(old_ob)
		{
			const int64 prev_ref_count = old_ob->decRefCount();
			if(prev_ref_count == 1)
				delete old_ob;
		}

		return *this;
	}


	/// Compares the pointer values.
	bool operator < (const Reference& other) const
	{
		return ob < other.ob;
	}


	/// Compares the pointer values.
	bool operator == (const Reference& other) const
	{
		return ob == other.ob;
	}

	bool operator != (const Reference& other) const
	{
		return ob != other.ob;
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

	/// Return the pointer value.
	inline T* getPointer()
	{
		return ob;
	}

	/// Return the pointer value.
	inline T* getPointer() const
	{
		return ob;
	}

	/// Alias for getPointer()
	inline T* ptr() const
	{
		return ob;
	}

	/// Alias for getPointer()
	inline T* ptr()
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

	/// NOTE: These upcast functions are not needed any more.  Valid conversions will be done automatically by the compiler.
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


	/// Downcast to a reference to a derived type.  NOTE: only call this if you are sure the pointer is actually to an object of the derived type.
	/// Otherwise behaviour is undefined.
	/// Will assert the downcast is valid if compiled with RTTI.
	template <class T2>
	inline const Reference<T2> downcast() const
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
		return Reference<T2>(static_cast<T2*>(ob));
	}

	template <class T2>
	inline Reference<T2> downcast()
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
		return Reference<T2>(static_cast<T2*>(ob));
	}

	/// Downcast to a pointer instead of a Reference.
	template <class T2>
	inline T2* downcastToPtr() const
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

	/// Returns true if ob is of type T2.
	template <class T2>
	inline bool isType() const
	{
		return dynamic_cast<const T2*>(ob) != 0;
	}
	
private:
	T* ob;
};

#endif // REFERENCE_H
