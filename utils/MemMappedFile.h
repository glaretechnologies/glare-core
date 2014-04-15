/*=====================================================================
MemMappedFile.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-09-05 15:48:02 +0100
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
MemMappedFile
-------------
Provides read-only access to a file through memory mapping.

See http://cvs.gna.org/cvsweb/radius/src/sys_map.cc?rev=1.2;content-type=text%2Fplain;cvsroot=radius for some MMap code.
=====================================================================*/
class MemMappedFile
{
public:
	MemMappedFile(const std::string& path); // Throws Indigo::Exception on failure.
	~MemMappedFile();

	size_t fileSize() const { return file_size; } // Returns file size in bytes.
	const void* fileData() const { return file_data; }

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

