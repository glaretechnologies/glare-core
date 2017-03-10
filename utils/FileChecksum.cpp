/*=====================================================================
FileChecksum.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Tue Mar 02 14:36:09 +1300 2010
=====================================================================*/
#include "FileChecksum.h"


#include "Exception.h"
#include "MemMappedFile.h"
#include "IncludeXXHash.h"


namespace FileChecksum
{


uint64 fileChecksum(const std::string& path) // throws Indigo::Exception if file not found.
{
	MemMappedFile file(path);

	if(file.fileSize() == 0)
		throw Indigo::Exception("Failed to compute checksum over file '" + path + ", file is empty.");

	return XXH64(file.fileData(), file.fileSize(), 1);
}


}
