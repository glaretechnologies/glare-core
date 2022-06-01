/*=====================================================================
TextureLoading.h
-----------------
Copyright Glare Technologies Limited 2019 -
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


class TextureFrameData
{
public:
	js::Vector<uint8, 16> compressed_data; // Compressed data for all mip-map levels.
	Reference<const ImageMapUInt8> converted_image;
};

// Result of texture loading
class TextureData : public ThreadSafeRefCounted
{
public:
	size_t compressedSizeBytes() const;

	size_t W, H, bytes_pp;
	
	std::vector<size_t> level_offsets;

	std::vector<TextureFrameData> frames; // will have 1 element for non-animated images, more than 1 for animated gifs etc..

	std::vector<double> frame_durations; // For gifs basically
	std::vector<double> frame_start_times;
};


//struct BuildUInt8MapTextureDataScratchState
//{
//	BuildUInt8MapTextureDataScratchState() : task_manager(NULL) {}
//	~BuildUInt8MapTextureDataScratchState() { delete task_manager; }
//
//	//std::vector<Reference<glare::Task> > compress_tasks;
//	glare::TaskManager* task_manager;
//};


class TextureDataManager : public ThreadSafeRefCounted
{
public:
	// Thread-safe
	Reference<TextureData> getOrBuildTextureData(const std::string& key, const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine/*, BuildUInt8MapTextureDataScratchState& scratch_state*/, bool allow_compression = true);

	Reference<TextureData> getTextureData(const std::string& key); // returns null ref if not present.

	bool isTextureDataInserted(const std::string& key) const; // Thread-safe

	void insertBuiltTextureData(const std::string& key, Reference<TextureData> data);

	void removeTextureData(const std::string& key);

	void clear();

	size_t getTotalMemUsage() const;
private:
	std::map<std::string, Reference<TextureData> > loaded_textures;
	mutable Mutex mutex;
};


/*=====================================================================
TextureLoading
--------------
Code for building compressed, mip-map level texture data, and for loading it into OpenGL.
Tests are in TextureLoadingTests.cpp
=====================================================================*/
class TextureLoading
{
public:
	friend class TextureLoadingTests;

	// Init stb_compress_dxt lib.
	static void init();

	static int computeNumMIPLevels(size_t width, size_t height);

	// Builds compressed, mip-map level data.
	// Uses task_manager for multi-threading if non-null.
	// May return a reference to imagemap in the returned TextureData.
	static Reference<TextureData> buildUInt8MapTextureData(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine, glare::TaskManager* task_manager, bool allow_compression = true);

	static Reference<TextureData> buildUInt8MapTextureData(const ImageMapUInt8* imagemap, glare::TaskManager* task_manager, bool allow_compression);

	// Builds compressed, mip-map level data for a sequence of images (e.g. animated gif)
	static Reference<TextureData> buildUInt8MapSequenceTextureData(const ImageMapSequenceUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine, glare::TaskManager* task_manager);

	// Load the built texture data into OpenGL.
	static Reference<OpenGLTexture> loadTextureIntoOpenGL(const TextureData& texture_data, const Reference<OpenGLEngine>& opengl_engine,
		OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping, bool use_sRGB = true);

	// Load the texture data for frame_i into an existing OpenGL texture
	static void loadIntoExistingOpenGLTexture(Reference<OpenGLTexture>& opengl_tex, const TextureData& texture_data, size_t frame_i, const Reference<OpenGLEngine>& opengl_engine);

private:
	static void downSampleToNextMipMapLevel(size_t prev_W, size_t prev_H, size_t N, const uint8* prev_level_image_data, float alpha_scale, size_t level_W, size_t level_H, uint8* data_out, float& alpha_coverage_out);

	// Uses task_manager for multi-threading if non-null.
	static void compressImageFrame(size_t total_compressed_size, js::Vector<uint8, 16>& temp_tex_buf, const SmallVector<size_t, 20>& temp_tex_buf_offsets, 
		DXTCompression::TempData& compress_temp_data, TextureData* texture_data, size_t cur_frame_i, const ImageMapUInt8* source_image, glare::TaskManager* task_manager);
};
