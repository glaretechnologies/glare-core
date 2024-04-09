/*=====================================================================
jpegdecoder.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "../utils/Reference.h"
#include <string>
class Map2D;
namespace glare { class Allocator; }


/*=====================================================================
JPEGDecoder
-----------

=====================================================================*/
class JPEGDecoder
{
public:
	~JPEGDecoder();


	struct SaveOptions
	{
		SaveOptions() : quality(95) {}
		SaveOptions(int quality_) : quality(quality_) {}

		int quality;
	};


	static Reference<Map2D> decode(const std::string& base_dir_path, const std::string& path, glare::Allocator* mem_allocator = NULL);

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size, const std::string& base_dir_path, glare::Allocator* mem_allocator = NULL);

	static void save(const Reference<ImageMapUInt8>& image, const std::string& path, const SaveOptions& options);


	static void test(const std::string& base_dir_path);
private:
	JPEGDecoder();
};
