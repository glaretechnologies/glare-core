/*=====================================================================
MemMappedFile.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-09-05 15:48:02 +0100
=====================================================================*/
#include "MemMappedFile.h"


#include "../utils/stringutils.h"
#include "../utils/Exception.h"


#if defined(WIN32) || defined(WIN64)

// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else


#endif


MemMappedFile::MemMappedFile(const std::string& path)
{
	this->file_handle = CreateFile(
		StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str(),
		GENERIC_READ,
		0, // share mode
		NULL, // security attributes
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if(file_handle == INVALID_HANDLE_VALUE)
		throw Indigo::Exception("CreateFile failed.");

	this->file_mapping_handle = CreateFileMapping(
		file_handle,
		NULL, // security attributes
		PAGE_READONLY, // flprotect
		0, // max size high
		0, // max size low
		NULL // name
	);

	if(file_mapping_handle == NULL)
		throw Indigo::Exception("CreateFileMapping failed.");

	// Get size of file
	LARGE_INTEGER file_size_li;
	BOOL res = GetFileSizeEx(
		file_handle,
		&file_size_li
	);
	if(!res)
		throw Indigo::Exception("GetFileSizeEx failed.");

	this->file_size = file_size_li.QuadPart;


	this->file_data = MapViewOfFile(
		file_mapping_handle,
		FILE_MAP_READ,
		0, // file offset high
		0, // file offset low
		file_size  // num bytes to map
	);

	if(file_data == NULL)
		throw Indigo::Exception("MapViewOfFile failed.");
}


MemMappedFile::~MemMappedFile()
{
	BOOL res = UnmapViewOfFile(this->file_data);
	assert(res);

	res = CloseHandle(this->file_mapping_handle);
	assert(res);

	res = CloseHandle(this->file_handle);
	assert(res);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"


void MemMappedFile::test()
{
	const std::string pathname = TestUtils::getIndigoTestReposDir() + "/testfiles/bun_zipper.ply";

	try
	{
		MemMappedFile file(pathname);

		conPrint("file size: " + toString(file.fileSize()));

		for(int i=0; i<5; ++i)
		{
			conPrint(std::string(1, ((char*)file.fileData())[i]));
		}

	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif
