/*=====================================================================
Transmungify.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun May 30 21:03:25 +1200 2010
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>
#include <vector>


/*=====================================================================
Transmungify
-------------------

=====================================================================*/
class Transmungify
{
public:
	Transmungify();
	~Transmungify();

	static void encrypt(const std::string& src_string, std::vector<uint32>& dst_dwords);
	static void encrypt(const char* src_string, uint32 src_string_size, std::vector<uint32>& dst_dwords);

	static void decrypt(const std::vector<uint32>& src_dwords, std::string& dst_string);  // throws Indigo::Exception
	static void decrypt(const uint32* src_dwords, uint32 src_dwords_count, std::string& dst_string);  // throws Indigo::Exception

	static void test();

private:
};
