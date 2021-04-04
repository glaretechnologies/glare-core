/*=====================================================================
KTXDecoder.h
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include <string>
#include "../utils/Reference.h"
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

	static Reference<Map2D> decodeKTX2(const std::string& path);

	static void supercompressKTX2File(const std::string& path_in, const std::string& path_out);

	static void test();
};
