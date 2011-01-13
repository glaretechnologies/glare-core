/*=====================================================================
Compression.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Jan 13 16:39:20 +1300 2011
=====================================================================*/
#pragma once


#include <vector>


/*=====================================================================
Compression
-------------------

=====================================================================*/
class Compression
{
public:
	Compression();
	~Compression();

	static void compress(const char* data, size_t size, std::vector<char>& data_out);


	static size_t decompressedSize(const char* data, size_t size);

	static void decompress(const char* data, size_t size, char* data_out, size_t decompressed_size);

	static void test();
private:

};
