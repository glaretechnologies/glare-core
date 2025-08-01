/*=====================================================================
TextureData.h
-------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../graphics/Map2D.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/AllocatorVector.h"
#include <vector>


/*=====================================================================
TextureData
-----------
Processed texture data, produced by TextureProcessing.
Contains Mipmap data, that is possibly compressed.
=====================================================================*/


/*
	Layout of mipmap_data:
	im 0, im 1 are the different array images ('layers').

	Mip level 0                                               Mip level 1                              Mip level 2
	----------------------------------------------------------------------------------------------------
	|im 0         |  im 1        |  im 2        |   im 3      |im 0    |  im 1   |  im 2  |   im 3  |  ...
	----------------------------------------------------------------------------------------------------
	^<------------------------------------------------------->^
	|                     level_size[0]                       |
	offset[0]                                             offset[1]
		 
*/


enum OpenGLTextureFormat
{
	Format_Greyscale_Uint8,
	Format_Greyscale_Float,
	Format_Greyscale_Half,
	Format_SRGB_Uint8,
	Format_SRGBA_Uint8,
	Format_RGB_Linear_Uint8,
	Format_RGB_Integer_Uint8,
	Format_RGBA_Linear_Uint8,
	Format_RGBA_Integer_Uint8,
	Format_RGB_Linear_Float,
	Format_RGB_Linear_Half,
	Format_RGBA_Linear_Float,
	Format_RGBA_Linear_Half,
	Format_Depth_Float,
	Format_Depth_Uint16,
	Format_Compressed_DXT_RGB_Uint8,   // BC1 / DXT1, linear sRGB colour space
	Format_Compressed_DXT_RGBA_Uint8,  // BC3 / DXT5, linear sRGB colour space
	Format_Compressed_DXT_SRGB_Uint8,  // BC1 / DXT1, non-linear sRGB colour space
	Format_Compressed_DXT_SRGBA_Uint8, // BC3 / DXT5, non-linear sRGB colour space
	Format_Compressed_BC6H, // BC6H half-float unsigned format: i.e. GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
	Format_Compressed_ETC2_RGB_Uint8,   // i.e. GL_COMPRESSED_RGB8_ETC2
	Format_Compressed_ETC2_RGBA_Uint8,  // i.e. GL_COMPRESSED_RGBA8_ETC2_EAC 
	Format_Compressed_ETC2_SRGB_Uint8,  // i.e. GL_COMPRESSED_SRGB8_ETC2
	Format_Compressed_ETC2_SRGBA_Uint8  // i.e. GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
};


bool isCompressed(OpenGLTextureFormat format);
size_t bytesPerPixel(OpenGLTextureFormat format); // For non block-compressed formats
size_t bytesPerBlock(OpenGLTextureFormat format); // For block-compressed formats
size_t numChannels(OpenGLTextureFormat format);
const char* textureFormatString(OpenGLTextureFormat format);


class TextureData : public ThreadSafeRefCounted
{
public:
	TextureData() : frame_durations_equal(false), num_array_images(0), D(1) {}

	size_t totalCPUMemUsage() const;

	void setAllocator(const Reference<glare::Allocator>& al) { mipmap_data.setAllocator(al); }

	size_t numMipLevels() const { return level_offsets.empty() ? 1 : level_offsets.size(); }

	bool isCompressed() const { return ::isCompressed(format); };

	bool isArrayTexture() const { return num_array_images > 0; }

	bool isMultiFrame() const { return num_frames > 1; } // e.g. is animated

	size_t numFrames() const { return num_frames; }

	size_t numChannels() const;

	double uncompressedBitsPerChannel() const;

	static size_t computeNumMipLevels(size_t W, size_t H);
	static size_t computeNum4PixelBlocksForLevel(size_t base_W, size_t base_H, size_t level);
	static size_t computeStorageSizeB(size_t W, size_t H, OpenGLTextureFormat format, bool include_mip_levels);
	
	OpenGLTextureFormat format;
	size_t W, H;
	size_t D; // Depth.  May be > 1 for array images.
	size_t num_array_images; // 0 if not array image, >= 1 if array image.

	struct LevelOffsetData
	{
		LevelOffsetData() {}
		LevelOffsetData(size_t offset_, size_t level_size_) : offset(offset_), level_size(level_size_) {}
		size_t offset; // Offset (in bytes) from the start of the current frame data to the data for the given MIP level. Same for all frames.  In bytes.
		size_t level_size; // Size (in bytes) of the given MIP level data for the current frame.
	};

	std::vector<LevelOffsetData> level_offsets;

	glare::AllocatorVector<uint8, 16> mipmap_data;
	
	Reference<const Map2D> converted_image; // May reference an image directly if we are not computing mipmaps for it.

	std::vector<double> frame_end_times;

	bool frame_durations_equal;
	double recip_frame_duration; // Set if frame_durations_equal is true.
	double last_frame_end_time;
	size_t num_frames; // will have 1 frame for non-animated images, more than 1 for animated gifs etc..
	size_t frame_size_B;
};


typedef Reference<TextureData> TextureDataRef;
