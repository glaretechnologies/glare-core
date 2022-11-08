/*=====================================================================
TextureProcessing.h
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../graphics/ImageMapSequence.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Mutex.h"
#include <map>


class OpenGLTexture;
class OpenGLEngine;
namespace DXTCompression { struct TempData; }
namespace glare { class TaskManager; }
namespace glare { class GeneralMemAllocator; }


/*=====================================================================
TextureLoading
--------------
Code for building compressed, mip-map level texture data.
Tests are in TextureProcessingTests.cpp
=====================================================================*/
class TextureProcessing
{
public:
	friend class TextureProcessingTests;

	// Init stb_compress_dxt lib.
	static void init();

	static int computeNumMIPLevels(size_t width, size_t height);

	// Builds compressed, mip-map level data, if applicable.
	// Uses task_manager for multi-threading if non-null.
	// May return a reference to imagemap in the returned TextureData.
	static Reference<TextureData> buildTextureData(const Map2D* map2d, const Reference<OpenGLEngine>& opengl_engine, glare::TaskManager* task_manager, bool allow_compression = true);

private:
	static Reference<TextureData> buildUInt8MapTextureData(const ImageMapUInt8* imagemap, glare::GeneralMemAllocator* general_mem_allocator, glare::TaskManager* task_manager, bool allow_compression);

	// Similar to above, but checks opengl_engine->GL_EXT_texture_compression_s3tc_support and opengl_engine->settings.compress_textures to see if compression should be done.
	static Reference<TextureData> buildUInt8MapTextureData(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine, glare::TaskManager* task_manager, bool allow_compression = true);

	// Builds compressed, mip-map level data for a sequence of images (e.g. animated gif)
	static Reference<TextureData> buildUInt8MapSequenceTextureData(const ImageMapSequenceUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine, glare::TaskManager* task_manager);

	static void downSampleToNextMipMapLevel(size_t prev_W, size_t prev_H, size_t N, const uint8* prev_level_image_data, float alpha_scale, size_t level_W, size_t level_H, uint8* data_out, float& alpha_coverage_out);

	// Uses task_manager for multi-threading if non-null.
	static void compressImageFrame(size_t total_compressed_size, js::Vector<uint8, 16>& temp_tex_buf_a, js::Vector<uint8, 16>& temp_tex_buf_b, 
		DXTCompression::TempData& compress_temp_data, TextureData* texture_data, size_t cur_frame_i, const ImageMapUInt8* source_image, glare::TaskManager* task_manager);
};
