/*=====================================================================
TextureProcessing.h
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "TextureData.h"
#include "ImageMap.h"
#include "ImageMapSequence.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Mutex.h"
#include "../utils/Vector.h"
#include <map>


namespace DXTCompression { struct TempData; }
namespace glare { class TaskManager; }
namespace glare { class Allocator; }


/*=====================================================================
TextureProcessing
-----------------
Code for building compressed, mip-map level texture data.

Tests are in TextureProcessingTests.cpp
=====================================================================*/
class TextureProcessing
{
public:
	friend class TextureProcessingTests;

	static int computeNumMIPLevels(size_t width, size_t height);

	// Builds compressed, mip-map level data, if applicable.
	// Uses task_manager for multi-threading if non-null.
	// May return a reference to imagemap in the returned TextureData.
	static Reference<TextureData> buildTextureData(const Map2D* map2d, glare::Allocator* general_mem_allocator, glare::TaskManager* task_manager, bool allow_compression, bool build_mipmaps);

private:
	static Reference<TextureData> buildUInt8MapTextureData(const ImageMapUInt8* imagemap, glare::Allocator* general_mem_allocator, glare::TaskManager* task_manager, bool allow_compression, bool build_mipmaps);

	// Builds compressed, mip-map level data for a sequence of images (e.g. animated gif)
	static Reference<TextureData> buildUInt8MapSequenceTextureData(const ImageMapSequenceUInt8* imagemap, glare::Allocator* general_mem_allocator, glare::TaskManager* task_manager, bool allow_compression, bool build_mipmaps);

	static void downSampleToNextMipMapLevel(size_t prev_W, size_t prev_H, size_t N, const uint8* prev_level_image_data, float alpha_scale, size_t level_W, size_t level_H, uint8* data_out, float& alpha_coverage_out);

	// Uses task_manager for multi-threading if non-null.
	static void buildMipMapDataForImageFrame(bool do_compression, js::Vector<uint8, 16>& temp_tex_buf_a, js::Vector<uint8, 16>& temp_tex_buf_b, 
		DXTCompression::TempData& compress_temp_data, TextureData* texture_data, size_t cur_frame_i, const ImageMapUInt8* source_image, glare::TaskManager* task_manager);
};
