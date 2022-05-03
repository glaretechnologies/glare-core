/*=====================================================================
KTXDecoder.h
------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <string>
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
class Map2D;


/*=====================================================================
KTXDecoder
----------
KTX is a simple file format, especially useful for block-compressed textures,
e.g. for OpenGL rendering.
See KTX File Format Specification - https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
=====================================================================*/
class KTXDecoder
{
public:
	// throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path);

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size);

	static Reference<Map2D> decodeKTX2(const std::string& path);

	static Reference<Map2D> decodeKTX2FromBuffer(const void* data, size_t size);

	static void supercompressKTX2File(const std::string& path_in, const std::string& path_out);

	//static void writeSupercompressKTX2File(int w, int h, ArrayRef<uint8>& mipmap0_data, const std::string& path_out);
	static void writeSupercompressKTX2File(int w, int h, const std::vector<std::vector<uint8> >& level_image_data, const std::string& path_out);

	static void test();
};
