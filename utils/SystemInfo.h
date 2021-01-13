/*=====================================================================
SystemInfo.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Mon Mar 01 14:37:00 +1300 2010
=====================================================================*/
#pragma once


#include <vector>
#include <string>


/*=====================================================================
SystemInfo
-------------------

=====================================================================*/
namespace SystemInfo
{
	void getMACAddresses(std::vector<std::string>& addresses_out); // throws glare::Exception

	void test();
};
