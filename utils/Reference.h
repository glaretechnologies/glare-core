/*=====================================================================
Reference.h
-----------
Copyright Glare Technologies Limited 2015 - 
=====================================================================*/
#pragma once


#include "RefCounted.h"
#include "Platform.h"
#include <cassert>


/*=====================================================================
Reference
---------
Handle to a reference-counted object.
Referenced object will be automatically deleted when no refs to it remain.
T must be a subclass of 'RefCounted' or 'ThreadSafeRefCounted'.
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
			int64 new_ref_count = ob->decRefCount();
			if(new_ref_count == 0)
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
			int64 ref_count = old_ob->decRefCount();
			if(ref_count == 0)
				delete old_ob;
		}

		return *this;
	}


	// An assignment operator from a raw pointer.
	// We have this method so that the code 
	//
	// Reference<T> ref;
	// ref = new T();
	//
	// will work without a temporary reference object being created and then assigned to ref.
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
			int64 ref_count = old_ob->decRefCount();
			if(ref_count == 0)
				delete old_ob;
		}

		return *this;
	}


	// Compares the pointer values.
	bool operator < (const Reference& other) const
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

	// Alias for getPointer()
	inline T* ptr()
	{
		return ob;
	}

	// NOTE: These upcast functions are not needed any more.  Valid conversions will be done automatically by the compiler.
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


	template <class T2>
	inline bool isType() const
	{
		return dynamic_cast<const T2*>(ob) != 0;
	}
	
private:
	T* ob;
};
