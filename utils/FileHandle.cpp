/*=====================================================================
FileHandle.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2010-10-26 22:57:11 +1300
=====================================================================*/
#include "FileHandle.h"


#include "stringutils.h"
#include "Exception.h"


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


void FileHandle::open(const std::string& pathname, const std::string& openmode) // throws Indigo::Exception
{
	assert(!f);

#if defined(WIN32) || defined(WIN64)
	// If we are on Windows, then, in order to use Unicode filenames, we will convert from UTF-8 to wstring and use _wfopen()
	f = _wfopen(StringUtils::UTF8ToWString(pathname).c_str(), StringUtils::UTF8ToWString(openmode).c_str());
#else
	// On Linux (and on OS X?), fopen accepts UTF-8 encoded Unicode filenames natively.
	f = fopen(pathname.c_str(), openmode.c_str());
#endif

	if(!f)
		throw Indigo::Exception("Failed to open file '" + pathname + "'.");
}
