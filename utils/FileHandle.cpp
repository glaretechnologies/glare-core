/*=====================================================================
FileHandle.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "FileHandle.h"


#include "StringUtils.h"
#include "PlatformUtils.h"
#include "Exception.h"
#include <assert.h>


FileHandle::FileHandle()
:	f(NULL)
{
}


FileHandle::FileHandle(const std::string& pathname, const std::string& openmode)
:	f(NULL)
{
	open(pathname, openmode);
}


FileHandle::~FileHandle()
{
	if(f)
		fclose(f);
}


void FileHandle::open(const std::string& pathname, const std::string& openmode) // throws glare::Exception
{
	assert(!f);

#if defined(_WIN32) || defined(_WIN64)
	// If we are on Windows, then, in order to use Unicode filenames, we will convert from UTF-8 to wstring and use _wfopen()
	const errno_t res = _wfopen_s(&f, StringUtils::UTF8ToWString(pathname).c_str(), StringUtils::UTF8ToWString(openmode).c_str());
	if(res != 0)
		throw glare::Exception("Failed to open file '" + pathname + "': " + PlatformUtils::getLastErrorString());
#else
	// On Linux (and on OS X?), fopen accepts UTF-8 encoded Unicode filenames natively.
	f = fopen(pathname.c_str(), openmode.c_str());
#endif

	if(!f)
		throw glare::Exception("Failed to open file '" + pathname + "'.");
}


int FileHandle::getFileDescriptor()
{
	if(f)
	{
#if defined(_WIN32)
		return _fileno(f);
#else
		return fileno(f);
#endif
	}
	else
		return -1;
}
