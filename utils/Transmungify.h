/*=====================================================================
Transmungify.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun May 30 21:03:25 +1200 2010
=====================================================================*/
#pragma once

#include <string>
#include <vector>


// Some magic numbers...
const static int permute[4] = { 3, 1, 0, 2 };
const static int char_offsets[4] = { -79, 230, -27, 160 };
const static unsigned int magic0 = 0xDEADBEEF;
const static unsigned int magic1 = 0xC0FFEE;


/*=====================================================================
Transmungify
-------------------

=====================================================================*/
class Transmungify
{
public:
	Transmungify();
	~Transmungify();

	static bool encrypt(const std::string& src_string, std::vector<unsigned int>& dst_dwords);

	static bool decrypt(const std::vector<unsigned int>& src_dwords, std::string& dst_string);
	static bool decrypt(const unsigned int* src_dwords, std::string& dst_string, unsigned int string_len);

	static void test();

private:
};
