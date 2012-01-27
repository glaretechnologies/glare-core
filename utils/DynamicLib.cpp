/*=====================================================================
DynamicLib.cpp
--------------
Copyright Glare Technologies Limited 2011 -
Generated at Thu Jan 26 17:08:23 +0000 2012
=====================================================================*/
#include "DynamicLib.h"

#include "stringutils.h"


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
	if(lib_handle != NULL && !::FreeLibrary(lib_handle))
		throw Indigo::Exception("FreeLibrary failed");
#elif defined(__linux__)
	dlclose(opencl_handle);
#endif
}


} // end namespace Indigo 
