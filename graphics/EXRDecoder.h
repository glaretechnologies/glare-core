/*=====================================================================
EXRDecoder.h
------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "../utils/Reference.h"
#include <string>
class Map2D;
class Image;
class Image4f;


/*=====================================================================
EXRDecoder
----------
Reading and writing of images in EXR format.
=====================================================================*/
class EXRDecoder
{
public:
	EXRDecoder();
	~EXRDecoder();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);


	// Options for saving
	enum CompressionMethod
	{
		CompressionMethod_None,
		CompressionMethod_RLE,
		CompressionMethod_ZIP,
		CompressionMethod_PIZ,
		CompressionMethod_PXR24,
		CompressionMethod_B44A
	};

	enum BitDepth
	{
		BitDepth_16,
		BitDepth_32
	};

	struct SaveOptions
	{
		SaveOptions() : compression_method(CompressionMethod_PIZ), bit_depth(BitDepth_32) {}
		SaveOptions(CompressionMethod compression_method_, BitDepth bit_depth_) : compression_method(compression_method_), bit_depth(bit_depth_) {}

		CompressionMethod compression_method;
		BitDepth bit_depth;
	};


	// throws Indigo::Exception
	static void saveImageToEXR(const Image& image, const std::string& pathname, const SaveOptions& options);
	static void saveImageToEXR(const Image4f& image, bool save_alpha_channel, const std::string& pathname, const SaveOptions& options);
	static void saveImageToEXR(const ImageMapFloat& image, const std::string& pathname, const SaveOptions& options);
	
	static void test();

private:
	static void setEXRThreadPoolSize();
};
