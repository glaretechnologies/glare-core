/*=====================================================================
Base64.h
--------
Copyright Glare Technologies Limited 2020 -
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

	// Throws glare::Exception on failure.
	void decode(const std::string& s, std::vector<unsigned char>& data_out);

	void test();
}
