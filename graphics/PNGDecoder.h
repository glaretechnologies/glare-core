/*=====================================================================
PNGDecoder.h
------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <map>
#include <string>
class Map2D;
class Bitmap;
class UInt8ComponentValueTraits;
class UInt16ComponentValueTraits;
namespace glare { class Allocator; }
template <class V, class VTraits> class ImageMap;


/*=====================================================================
PNGDecoder
----------
Loading and saving of PNG files using libpng.
=====================================================================*/
class PNGDecoder
{
public:
	// All methods throw ImFormatExcep on failure.

	static Reference<Map2D> decode(const std::string& path, glare::Allocator* mem_allocator = NULL);

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL);
	
	static void write(const Bitmap& bitmap, const std::map<std::string, std::string>& metadata, const std::string& path);
	static void write(const Bitmap& bitmap, const std::string& path); // Write with no metadata

	static void write(const ImageMap<uint8, UInt8ComponentValueTraits>& imagemap, const std::string& path); // Write with no metadata
	static void write(const ImageMap<uint16, UInt16ComponentValueTraits>& imagemap, const std::string& path); // Write with no metadata

	// Write tightly-packed data laid out in row-major order.
	static void write(const void* data, unsigned int W, unsigned int H, unsigned int N, unsigned int bits_per_channel, const std::map<std::string, std::string>& metadata, const std::string& path); // Write with metadata
	static void write(const void* data, unsigned int W, unsigned int H, unsigned int N, unsigned int bits_per_channel, const std::string& path); // Write with no metadata


	static const std::map<std::string, std::string> getMetaData(const std::string& image_path);

	static void test(const std::string& base_dir_path);
};
