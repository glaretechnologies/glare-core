/*=====================================================================
Transmungify.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun May 30 21:03:25 +1200 2010
=====================================================================*/
#pragma once

#include <string>
#include <vector>

#include "platform.h"

// Some magic numbers...
const static int permute[4] = { 3, 1, 0, 2 };
const static int char_offsets[4] = { -79, 230, -27, 160 };
const static uint32 magic0 = 0xB77E0246; // random, nothing special
const static uint32 magic1 = 0x2E479BAA; // random, nothing special


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
