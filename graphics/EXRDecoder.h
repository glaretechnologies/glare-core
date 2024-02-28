/*=====================================================================
EXRDecoder.h
------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include <string>
#include <vector>
#include <ImfCompression.h>
class Map2D;
class Image;
class Image4f;
template <class T> class Reference;


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

	// Uses task_manager as the thread pool for EXR tasks.
	static void init(glare::TaskManager* task_manager);

	static void shutdown();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);

	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size, const std::string& path);


	// Options for saving

	// Since the OpenEXR headers say DWAB is 'More efficient space wise and faster to decode full frames than DWAA_COMPRESSION',
	// we will skip support for saving with DWAA.
	enum CompressionMethod
	{
		CompressionMethod_None,
		CompressionMethod_RLE,
		CompressionMethod_ZIP,
		CompressionMethod_PIZ,
		CompressionMethod_PXR24,
		CompressionMethod_B44A,
		CompressionMethod_DWAB
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

		std::vector<std::string> channel_names; // Can leave empty, used if non-empty.
	};


	// throws glare::Exception
	static void saveImageToEXR(const float* pixel_data, size_t width, size_t height, size_t num_channels_in_buffer, bool save_alpha_channel, const std::string& pathname, 
		const std::string& layer_name, const SaveOptions& options);
	static void saveImageToEXR(const Image& image, const std::string& pathname, const std::string& layer_name, const SaveOptions& options);
	static void saveImageToEXR(const Image4f& image, bool save_alpha_channel, const std::string& pathname, const std::string& layer_name, const SaveOptions& options);
	static void saveImageToEXR(const ImageMapFloat& image, const std::string& pathname, const std::string& layer_name, const SaveOptions& options);
	

	static Imf::Compression EXRCompressionMethod(EXRDecoder::CompressionMethod m);

	static void test();
};
