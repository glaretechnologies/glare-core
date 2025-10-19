/*=====================================================================
ComObHandle.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "Exception.h"


/*=====================================================================
ComObHandle
-----------
RAII wrapper for a COM object.
=====================================================================*/
#if defined(_WIN32)
template <class T>
class ComObHandle
{
public:
	ComObHandle() : ptr(nullptr) {}
	ComObHandle(T* ptr_) : ptr(ptr_)
	{
		if(ptr)
			ptr->AddRef();
	}

	ComObHandle(const ComObHandle<T>& other)
	{
		ptr = other.ptr;
		if(ptr)
			ptr->AddRef();
	}

	~ComObHandle()
	{
		if(ptr)
			ptr->Release();
	}

	// Recommended to use explicit operator bool here: https://quuxplusone.github.io/blog/2023/04/08/most-ctors-should-be-explicit/#you-should-never-declare-convers
	explicit inline operator bool () const
	{
		return ptr != nullptr;
	}

	void operator = (const ComObHandle& other)
	{
		T* old_ptr = ptr;

		ptr = other.ptr;
		// NOTE: if a reference is getting assigned to itself, old_ob == ob.  So make sure we increment before we decrement, to avoid deletion.
		if(ptr)
			ptr->AddRef();

		// Decrement reference count for the object that this reference used to refer to.
		if(old_ptr)
			old_ptr->Release();
	}


	template <class SubClassT>
	inline bool queryInterface(ComObHandle<SubClassT>& result)
	{
		// This code needs a 'template' keyword with Clang to avoid a warning: see https://stackoverflow.com/a/3786454
		if(ptr->template QueryInterface<SubClassT>(&result.ptr) == /*S_OK=*/0)
			return true;
		else
		{
			result.ptr = NULL;
			return false;
		}
	}

	template <class SubClassT>
	inline ComObHandle<SubClassT> getInterface()
	{
		ComObHandle<SubClassT> res;
		if(!queryInterface<SubClassT>(res))
			throw glare::Exception("Failed to get interface.");
		return res;
	}


	inline void release()
	{
		if(ptr)
			ptr->Release();
		ptr = NULL;
	}

	inline unsigned int getRefCount()
	{
		if(ptr)
		{
			ptr->AddRef();
			return ptr->Release(); // returns new ref count
		}
		else
			return 0;
	}

	inline T* operator -> () const { return ptr; }

	T* ptr;
};
#endif

