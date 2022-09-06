/*=====================================================================
IndigoAllocation.h
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <cstring> // for size_t


// Defining this in a class makes sure objects are always allocated by code on the same side of a DLL boundary.
// It's not always valid to allocate an object on one side of a DLL boundary and free on the other side, as each side may have a different heap,
// for example if one or more sides are compiled with static CRT, or one or more sides uses a debug heap.
#define USE_INDIGO_SDK_ALLOCATOR														\
	static void* operator new(size_t size) { return Indigo::indigoSDKAlloc(size); }		\
	static void* operator new[](size_t size) { return Indigo::indigoSDKAlloc(size); }	\
	static void operator delete(void* ptr) { Indigo::indigoSDKFree(ptr); }				\
	static void operator delete[](void* ptr) { Indigo::indigoSDKFree(ptr); }


namespace Indigo
{


void* indigoSDKAlloc(size_t size);
void* indigoSDKAlignedAlloc(size_t size, size_t alignment);
void indigoSDKFree(void* p);
void indigoSDKAlignedFree(void* p);


} // end namespace Indigo 
