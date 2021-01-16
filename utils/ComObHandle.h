/*=====================================================================
ComObHandle.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"


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
	ComObHandle() : ptr(0) {}
	ComObHandle(T* handle_) : ptr(ptr_) {}
	~ComObHandle() { if(ptr) ptr->Release(); }

	inline void release()
	{
		if(ptr) 
			ptr->Release();
		ptr = NULL;
	}

	inline T* operator -> () const { return ptr; }

	T* ptr;
private:
	GLARE_DISABLE_COPY(ComObHandle);
};
#endif

