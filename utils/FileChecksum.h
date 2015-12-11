/*=====================================================================
FileChecksum.h
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Tue Mar 02 14:36:09 +1300 2010
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
FileChecksum
-------------------

=====================================================================*/
namespace FileChecksum
{


uint64 fileChecksum(const std::string& path); // throws Indigo::Exception


};
