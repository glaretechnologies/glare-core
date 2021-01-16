/*=====================================================================
FileChecksum.h
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
FileChecksum
------------
Compute a checksum over a file on disk.  Uses xxhash.
=====================================================================*/
namespace FileChecksum
{


uint64 fileChecksum(const std::string& path); // throws glare::Exception


};
