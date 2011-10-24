/*=====================================================================
Checksum.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-16 17:30:01 +0100
=====================================================================*/
#include "Checksum.h"


#include "../maths/mathstypes.h"
#include <zlib.h>
#include <limits>


uint32 Checksum::checksum(void* data, size_t datalen)
{
	uint32 c = initChecksum();
	return updateChecksum(c, data, datalen);
}


uint32 Checksum::initChecksum()
{
	return crc32(0, 0, 0);
}


uint32 Checksum::updateChecksum(uint32 running_checksum, const void* data, size_t datalen)
{
	for(size_t i = 0; i < datalen; i += std::numeric_limits<uint32>::max()) // For each chunk of 2^32 bytes
	{
		// Compute number of bytes to crc over in this step.
		const uint32 num = (uint32)myMin((size_t)std::numeric_limits<uint32>::max(), datalen - i);

		running_checksum = crc32(running_checksum, (const Bytef*)data + i, num);
	}

	return running_checksum;
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void Checksum::test()
{
	{
		std::string a = "";
		std::string b = "a";
		std::string c = "b";

		uint32 a_cs = Checksum::checksum((void*)a.c_str(), a.size());
		uint32 b_cs = Checksum::checksum((void*)b.c_str(), b.size());
		uint32 c_cs = Checksum::checksum((void*)c.c_str(), c.size());

		testAssert(a_cs != b_cs);
		testAssert(b_cs != c_cs);

		uint32 x = Checksum::initChecksum();
		x = Checksum::updateChecksum(x, (void*)b.c_str(), b.size());
		testAssert(b_cs == x);
	}

	// Check that checksum() gives the same result as updateChecksum()
	{
		std::string ab = "ab";
		std::string a = "a";
		std::string b = "b";

		uint32 x = Checksum::checksum((void*)ab.c_str(), ab.size());

		uint32 y = Checksum::initChecksum();
		y = Checksum::updateChecksum(y, (void*)a.c_str(), a.size()); 
		y = Checksum::updateChecksum(y, (void*)b.c_str(), b.size()); 
		testAssert(x == y);
	}
}


#endif
