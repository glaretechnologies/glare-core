/*=====================================================================
FileChecksum.h
-------------------
Copyright Glare Technologies Limited 2010 -
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


uint32 fileChecksum(const std::string& p); // throws Indigo::Exception


};
