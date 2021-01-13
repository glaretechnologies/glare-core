/*=====================================================================
DynamicLib.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "DynamicLib.h"


#include "StringUtils.h"
#include "PlatformUtils.h"
#include <assert.h>
#if BUILD_TESTS
#include "TestUtils.h"
#include "ConPrint.h"
#endif


namespace glare
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
		throw glare::Exception("Failed to open dynamic library '" + lib_path + "': " + PlatformUtils::getLastErrorString());
	}
#else
	lib_handle = dlopen(lib_path.c_str(), RTLD_LAZY);
	if(!lib_handle)
		throw glare::Exception("Failed to open dynamic library '" + lib_path + "'");
#endif
}


void DynamicLib::close()
{
#if defined(_WIN32)
	if(lib_handle != NULL)
	{
		if(!::FreeLibrary(lib_handle))
			throw glare::Exception("FreeLibrary failed");

		lib_handle = NULL;
	}
#elif defined(__linux__)
	if(lib_handle != NULL)
	{
		if(dlclose(lib_handle) != 0)
			throw glare::Exception("FreeLibrary failed");

		lib_handle = NULL;
	}
#endif
}


const std::string DynamicLib::getFullPathToLib() const
{
#if defined(_WIN32)
	TCHAR buf[2048];
	const DWORD result = GetModuleFileName(
		lib_handle, // hModule
		buf,
		2048
	);

	if(result == 0)
		throw glare::Exception("GetModuleFileName failed.");
	else
		return StringUtils::PlatformToUTF8UnicodeEncoding(buf);
#else
	throw glare::Exception("getFullPathToLib not implemented on this platform.");
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
	conPrint("DynamicLib::test():");

#ifdef _WIN32
	try
	{
		// Open Kernel32.DLL and try to find the CreateThread function
		DynamicLib kernel_lib("kernel32.dll");
		CreateThread_TYPE create_thread_func = kernel_lib.getFuncPointer<CreateThread_TYPE>("CreateThread");

		// Make sure it worked
		assertOrDeclareUsed(create_thread_func != NULL);
	}
	catch(glare::Exception& e)
	{
		conPrint("DynamicLib exception: " + e.what());
	}
#elif defined(__linux)
	try
	{
		DynamicLib math_lib("libm.so");
		cos_TYPE cos_func = math_lib.getFuncPointer<cos_TYPE>("cos");

		// Make sure it worked
		testAssert((*cos_func)(0.0) == 1.0);
	}
	catch(glare::Exception& e)
	{
		conPrint("DynamicLib exception: " + e.what());
	}
#endif
}


#endif


} // end namespace glare 
