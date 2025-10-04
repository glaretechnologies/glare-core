/*=====================================================================
FileChecksum.h
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "string_view.h"


/*=====================================================================
FileChecksum
------------
Compute a checksum over a file on disk.  Uses xxhash.
=====================================================================*/
namespace FileChecksum
{


uint64 fileChecksum(const string_view path); // throws glare::Exception


};
