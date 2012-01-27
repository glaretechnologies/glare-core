/*=====================================================================
DynamicLib.h
------------
Copyright Glare Technologies Limited 2011 -
Generated at Thu Jan 26 17:08:23 +0000 2012
=====================================================================*/
#pragma once

#include "Exception.h"
#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <delayimp.h>
#elif defined(__linux__) || defined(OSX)
#include <dlfcn.h>
#endif


namespace Indigo
{


/*=====================================================================
DynamicLib
----------

=====================================================================*/
class DynamicLib
{
public:
	DynamicLib();
	DynamicLib(const std::string& lib_path);
	~DynamicLib();

	void open(const std::string& lib_path);
	void close();

#ifdef _WIN32

	template <class FuncPointerType>
	FuncPointerType getFuncPointer(const std::string& name)
	{
		if(!lib_handle)
			throw Indigo::Exception("No library open.");

		FuncPointerType f = (FuncPointerType)::GetProcAddress(lib_handle, name.c_str());
		if(!f)
			throw Indigo::Exception("Failed to get pointer to function '" + name + "'");

		return f;
	}

#elif defined(__linux__) || defined(OSX)

	template <class FuncPointerType>
	FuncPointerType getFuncPointer(const std::string& name)
	{
		if(!lib_handle)
			throw Indigo::Exception("No library open.");

		FuncPointerType f = (FuncPointerType)dlsym(lib_handle, name.c_str());
		if(!f)
			throw Indigo::Exception("Failed to get pointer to function '" + name + "'");

		return f;
	}

#endif

private:


#if defined(_WIN32)
	HMODULE lib_handle;
#else
	void* lib_handle;
#endif
};


} // end namespace Indigo 
