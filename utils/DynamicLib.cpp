/*=====================================================================
DynamicLib.cpp
--------------
Copyright Glare Technologies Limited 2011 -
Generated at Thu Jan 26 17:08:23 +0000 2012
=====================================================================*/
#include "DynamicLib.h"


#include "stringutils.h"
#include <assert.h>
#if BUILD_TESTS
#include "../indigo/TestUtils.h"
#include <iostream>
#endif


namespace Indigo
{


DynamicLib::DynamicLib()
:	lib_handle(0)
{

}


DynamicLib::DynamicLib(const std::string& lib_path)
{
	open(lib_path);
}


DynamicLib::~DynamicLib()
{
	close();
}


void DynamicLib::open(const std::string& lib_path)
{
#ifdef _WIN32
	const std::wstring path = StringUtils::UTF8ToPlatformUnicodeEncoding(lib_path);
	lib_handle = ::LoadLibrary(path.c_str());
	if(!lib_handle)
	{
		const DWORD error_code = GetLastError();
		throw Indigo::Exception("Failed to get open dynamic library '" + lib_path + "', error_code: " + ::toString((uint32)error_code));
	}
#else
	lib_handle = dlopen(lib_path.c_str(), RTLD_LAZY);
	if(!lib_handle)
		throw Indigo::Exception("Failed to get open dynamic library '" + lib_path + "'");
#endif
}


void DynamicLib::close()
{
#if defined(_WIN32)
	if(lib_handle != NULL)
	{
		if(!::FreeLibrary(lib_handle))
			throw Indigo::Exception("FreeLibrary failed");

		lib_handle = NULL;
	}
#elif defined(__linux__)
	if(lib_handle != NULL)
	{
		if(dlclose(lib_handle) != 0)
			throw Indigo::Exception("FreeLibrary failed");

		lib_handle = NULL;
	}
#endif
}


#if BUILD_TESTS


#ifdef _WIN32
typedef HANDLE (WINAPI *CreateThread_TYPE)(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);
#endif
#ifdef __linux__
typedef double (*cos_TYPE)(double x);
#endif

void DynamicLib::test()
{
	std::cout << "DynamicLib::test():" << std::endl;

#ifdef _WIN32
	try
	{
		// Open Kernel32.DLL and try to find the CreateThread function
		DynamicLib kernel_lib("kernel32.dll");
		CreateThread_TYPE create_thread_func = kernel_lib.getFuncPointer<CreateThread_TYPE>("CreateThread");

		// Make sure it worked
		assert(create_thread_func != NULL);
	}
	catch(Indigo::Exception& e)
	{
		std::cout << "DynamicLib exception: " + e.what() << std::endl;
	}
#elif defined(__linux)
	try
	{
		DynamicLib math_lib("libm.so");
		cos_TYPE cos_func = math_lib.getFuncPointer<cos_TYPE>("cos");

		// Make sure it worked
		testAssert((*cos_func)(0.0) == 1.0);
	}
	catch(Indigo::Exception& e)
	{
		std::cout << "DynamicLib exception: " + e.what() << std::endl;
	}
#endif
}


#endif


} // end namespace Indigo 
