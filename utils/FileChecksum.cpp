/*=====================================================================
FileChecksum.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "FileChecksum.h"


#include "Exception.h"
#include "MemMappedFile.h"
#include "IncludeXXHash.h"


namespace FileChecksum
{


uint64 fileChecksum(const std::string& path) // throws glare::Exception if file not found.
{
	MemMappedFile file(path);

	if(file.fileSize() == 0)
		throw glare::Exception("Failed to compute checksum over file '" + path + ", file is empty.");

	return XXH64(file.fileData(), file.fileSize(), 1);
}


}
