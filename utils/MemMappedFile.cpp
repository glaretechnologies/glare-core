/*=====================================================================
MemMappedFile.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-09-05 15:48:02 +0100
=====================================================================*/
#include "MemMappedFile.h"


#include "../utils/stringutils.h"
#include "../utils/Exception.h"
#include "../utils/platformutils.h"
#include "../utils/IncludeWindows.h"
#include <assert.h>

#if !defined(_WIN32)

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>	// For open / close

#endif


#if defined(_WIN32)

MemMappedFile::MemMappedFile(const std::string& path)
:	file_handle(NULL),
	file_mapping_handle(NULL),
	file_data(NULL)
{
	this->file_handle = CreateFile(
		StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str(),
		GENERIC_READ,
		FILE_SHARE_READ, // share mode - Use FILE_SHARE_READ so that other processes can read the file as well.
		NULL, // security attributes
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if(file_handle == INVALID_HANDLE_VALUE)
		throw Indigo::Exception("CreateFile failed for path '" + path + "': " + PlatformUtils::getLastErrorString());

	// Get size of file
	LARGE_INTEGER file_size_li;
	BOOL res = GetFileSizeEx(
		file_handle,
		&file_size_li
	);
	if(!res)
		throw Indigo::Exception("GetFileSizeEx failed: " + PlatformUtils::getLastErrorString());

	// If we are compiling in 32-bit mode, and the file is more than 2^32 bytes long, then fail.
	// NOTE: boolean expression broken into 2 if statements to silence VS static analysis.
	if(sizeof(size_t) == 4)
		if(file_size_li.HighPart != 0)
			throw Indigo::Exception("File is too large.");

	this->file_size = (size_t)file_size_li.QuadPart;

	// CreateFileMapping fails if the file size == 0.
	// We don't want to throw an exception in this case however, as a file of size zero is perfectly valid.
	// So don't try and map the file if it has size zero.
	if(this->file_size > 0)
	{
		this->file_mapping_handle = CreateFileMapping(
			file_handle,
			NULL, // security attributes
			PAGE_READONLY, // flprotect
			0, // max size high
			0, // max size low
			NULL // name
		);

		if(file_mapping_handle == NULL)
		{
			const std::string error_string = PlatformUtils::getLastErrorString();

			BOOL res = CloseHandle(this->file_handle);
			assert(res);
			throw Indigo::Exception("CreateFileMapping failed: " + error_string);
		}

		this->file_data = MapViewOfFile(
			file_mapping_handle,
			FILE_MAP_READ,
			0, // file offset high
			0, // file offset low
			file_size  // num bytes to map
		);

		if(file_data == NULL)
			throw Indigo::Exception("MapViewOfFile failed: " + PlatformUtils::getLastErrorString());
	}
}


MemMappedFile::~MemMappedFile()
{
	if(this->file_data != NULL)
	{
		BOOL res = UnmapViewOfFile(this->file_data);
		assert(res);
	}

	if(this->file_mapping_handle != NULL)
	{
		BOOL res = CloseHandle(this->file_mapping_handle);
		assert(res);
	}

	if(this->file_handle != NULL)
	{
		BOOL res = CloseHandle(this->file_handle);
		assert(res);
	}
}

#else // if defined LINUX or OSX

MemMappedFile::MemMappedFile(const std::string& path)
:	file_data(NULL)
{
	this->linux_file_handle = ::open(
		StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str(),
		O_RDONLY);

	if(this->linux_file_handle <= 0)
		throw Indigo::Exception("File open failed.");

	this->file_size = lseek(this->linux_file_handle, 0, SEEK_END);
	lseek(this->linux_file_handle, 0, SEEK_SET);

	// Don't try and map the file if it has size zero, since it will fail.
	if(this->file_size > 0)
	{
		this->file_data = mmap(0, this->file_size, PROT_READ, MAP_SHARED, this->linux_file_handle, 0);
		if(this->file_data == MAP_FAILED)
			throw Indigo::Exception("File mmap failed.");
	}
}


MemMappedFile::~MemMappedFile()
{
	if(this->file_data != NULL)
	{
		munmap(this->file_data, this->file_size);
	}

	if(this->linux_file_handle > 0)
	{
		::close(this->linux_file_handle);
	}
}

#endif


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"


void MemMappedFile::test()
{
	// Mem-map a file of size zero
	{
		const std::string pathname = TestUtils::getIndigoTestReposDir() + "/testfiles/empty_file";

		try
		{
			MemMappedFile file(pathname);

			testAssert(file.fileSize() == 0);
			testAssert(file.fileData() == NULL);
		}
		catch(Indigo::Exception& e)
		{
			failTest(e.what());
		}
	}

	{
		const std::string pathname = TestUtils::getIndigoTestReposDir() + "/testfiles/bun_zipper.ply";

		try
		{
			MemMappedFile file(pathname);

			testAssert(file.fileSize() == 3033195);

			conPrint("file size: " + toString((int64)file.fileSize()));

			for(int i=0; i<5; ++i)
			{
				conPrint(std::string(1, ((char*)file.fileData())[i]));
			}

		}
		catch(Indigo::Exception& e)
		{
			failTest(e.what());
		}

		// Try opening the same file twice
		try
		{
			MemMappedFile file1(pathname);

			MemMappedFile file2(pathname);
		}
		catch(Indigo::Exception& e)
		{
			failTest(e.what());
		}
	}
}


#endif

