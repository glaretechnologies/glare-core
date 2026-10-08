/*=====================================================================
KTXDecoder.h
------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include <string>
namespace glare { class Allocator; }
class Map2D;
class OutStream;


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
	static Reference<Map2D> decode(const std::string& path, glare::Allocator* mem_allocator = NULL);

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL);

	static Reference<Map2D> decodeKTX2(const std::string& path, glare::Allocator* mem_allocator = NULL);

	// Handles animated textures (array textures with a KTXanimData entry), as written by writeKTX2File(), returning a multi-frame CompressedImage.
	static Reference<Map2D> decodeKTX2FromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL);



	static void supercompressKTX2File(const std::string& path_in, const std::string& path_out); // Only handles VK_FORMAT_BC6H_UFLOAT_BLOCK format currently.


	enum Format
	{
		//Format_SRGB_Uint8,
		Format_BC1, // Aka DXT1 (DXT without alpha)
		Format_BC3, // Aka DXT5 (DXT with alpha)
		Format_BC6H
	};

	// zstd_compression_level is used if supercompression is true.  3 is ZSTD_CLEVEL_DEFAULT.  Higher levels are slower to compress, but no slower to decompress.
	// num_frames is 1 for a non-animated texture.  If num_frames > 1, the texture is animated: it is written as an array texture with one layer per frame, with a KTXanimData
	// entry giving frame_duration_s.  level_image_data[k] holds the data for MIP level k of all frames: frame 0, then frame 1, etc.
	static void writeKTX2File(Format format, bool supercompression, int w, int h, int num_frames, double frame_duration_s, const std::vector<std::vector<uint8> >& level_image_data,
		const std::string& path_out, int zstd_compression_level);

	// As writeKTX2File(), but writes to a stream.
	static void writeKTX2ToStream(Format format, bool supercompression, int w, int h, int num_frames, double frame_duration_s, const std::vector<std::vector<uint8> >& level_image_data,
		OutStream& stream_out, int zstd_compression_level);


	static void test();
};
