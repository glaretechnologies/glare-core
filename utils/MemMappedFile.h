/*=====================================================================
MemMappedFile.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include "string_view.h"
#include <string>


/*=====================================================================
MemMappedFile
-------------
Provides read-only access to a file through memory mapping.
=====================================================================*/
class MemMappedFile
{
public:
	MemMappedFile(const string_view path); // Throws glare::Exception on failure.
	~MemMappedFile();

	size_t fileSize() const { return file_size; } // Returns file size in bytes.
	const void* fileData() const { return file_data; } // Returns pointer to file data.  NOTE: This pointer will be the null pointer if the file size is zero.

	static void test();

private:
#if defined(_WIN32)
	void* file_handle;
	void* file_mapping_handle;
#else
	int linux_file_handle;
#endif
	void* file_data;
	size_t file_size;
};


// A reference-counted MemMappedFile
class SharedMemMappedFile : public ThreadSafeRefCounted
{
public:
	SharedMemMappedFile(const std::string& path); // Throws glare::Exception on failure.
	~SharedMemMappedFile();

	size_t fileSize() const { return file.fileSize(); } // Returns file size in bytes.
	const void* fileData() const { return file.fileData(); } // Returns pointer to file data.  NOTE: This pointer will be the null pointer if the file size is zero.

private:
	MemMappedFile file;
};

typedef Reference<SharedMemMappedFile> SharedMemMappedFileRef;
