/*=====================================================================
IndigoAllocation.cpp
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "include/IndigoAllocation.h"


#include "../utils/MemAlloc.h"
#include <new>
#include <cstdlib> // For malloc().


namespace Indigo
{


#if !defined(INDIGO_DLL_EXPORTS)


// This code will be enabled for the sdk-lib, and for the normal Indigo executable etc..
// However it is not enabled for the Indigo SDK dll.  This means that in the case of SDK usage, allocation/frees are always done in the SDK lib, 
// not mixed and matched with the SDK lib and the dll.
void* indigoSDKAlloc(size_t size)
{
	void* p = malloc(size);
	if(!p)
		throw std::bad_alloc();
	return p;
}


void* indigoSDKAlignedAlloc(size_t size, size_t alignment)
{
	return MemAlloc::alignedMalloc(size, alignment);
}


void indigoSDKFree(void* p)
{
	if(p)
		free(p);
}


void indigoSDKAlignedFree(void* p)
{
	MemAlloc::alignedFree(p);
}


#endif


} // end namespace Indigo 
