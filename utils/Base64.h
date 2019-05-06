/*=====================================================================
Base64.h
-------------------
Copyright Glare Technologies Limited 2019 -
Generated at 2014-03-22 11:44:41 +0000
=====================================================================*/
#pragma once


#include <string>
#include <vector>


/*=====================================================================
Base64
-------------------
Base64 encoding and decoding.
=====================================================================*/
namespace Base64
{
	void encode(const void* data, size_t datalen, std::string& res_out);

	// Throws Indigo::Exception on failure.
	void decode(const std::string& s, std::vector<unsigned char>& data_out);

	void test();
}
