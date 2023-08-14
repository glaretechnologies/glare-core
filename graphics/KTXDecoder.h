/*=====================================================================
KTXDecoder.h
------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include <string>
class Map2D;


/*=====================================================================
KTXDecoder
----------
KTX is a simple file format, especially useful for block-compressed textures,
e.g. for OpenGL rendering.
See KTX File Format Specification:
https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html
https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html
=====================================================================*/
class KTXDecoder
{
public:
	// throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path);

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size);

	static Reference<Map2D> decodeKTX2(const std::string& path);

	static Reference<Map2D> decodeKTX2FromBuffer(const void* data, size_t size);



	static void supercompressKTX2File(const std::string& path_in, const std::string& path_out); // Only handles VK_FORMAT_BC6H_UFLOAT_BLOCK format currently.


	enum Format
	{
		Format_BC1, // Aka DXT1 (DXT without alpha)
		Format_BC3, // Aka DXT5 (DXT with alpha)
		Format_BC6H
	};

	static void writeKTX2File(Format format, bool supercompression, int w, int h, const std::vector<std::vector<uint8> >& level_image_data, const std::string& path_out);


	static void test();
};
