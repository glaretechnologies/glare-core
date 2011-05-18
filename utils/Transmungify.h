/*=====================================================================
Transmungify.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun May 30 21:03:25 +1200 2010
=====================================================================*/
#pragma once


#include "platform.h"
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

	static bool encrypt(const std::string& src_string, std::vector<uint32>& dst_dwords);

	static bool decrypt(const std::vector<uint32>& src_dwords, std::string& dst_string);
	static bool decrypt(const uint32* src_dwords, uint32 src_dwords_count, std::string& dst_string);

	static void test();

private:
};
