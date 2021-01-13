/*=====================================================================
DynamicLib.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Exception.h"
#if defined(_WIN32)
#include "IncludeWindows.h"
#include <delayimp.h>
#elif defined(__linux__) || defined(OSX)
#include <dlfcn.h>
#endif


namespace glare
{


/*=====================================================================
DynamicLib
----------

=====================================================================*/
class DynamicLib
{
public:
	DynamicLib();
	DynamicLib(const std::string& lib_path); // throws glare::Exception on failure
	~DynamicLib();

	void open(const std::string& lib_path); // throws glare::Exception on failure
	void close();

	const std::string getFullPathToLib() const;


	// Return the func pointer if it exists in the lib, or NULL otherwise.
	template <class FuncPointerType>
	FuncPointerType tryGetFuncPointer(const std::string& name)
	{
		if(!lib_handle)
			throw glare::Exception("No library open.");

#ifdef _WIN32
		return (FuncPointerType)::GetProcAddress(lib_handle, name.c_str());
#else
		return (FuncPointerType)::dlsym(lib_handle, name.c_str());
#endif
	}


	// Try and get the function pointer from the lib, throw glare::Exception on failure.
	template <class FuncPointerType>
	FuncPointerType getFuncPointer(const std::string& name)
	{
		FuncPointerType f = tryGetFuncPointer<FuncPointerType>(name);
		if(!f)
			throw glare::Exception("Failed to get pointer to function '" + name + "'");
		return f;
	}


#if BUILD_TESTS
	static void test();
#endif


private:

#if defined(_WIN32)
	HMODULE lib_handle;
#else
	void* lib_handle;
#endif
};


} // end namespace glare 
