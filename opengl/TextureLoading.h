/*=====================================================================
TextureLoading.h
-----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Mutex.h"
#include "../utils/TaskManager.h"
#include <unordered_set>


class OpenGLEngine;


// Result of texture loading
class TextureData : public ThreadSafeRefCounted
{
public:
	size_t W, H, bytes_pp;
	std::vector<js::Vector<uint8, 16> > compressed_data; // Compressed data at each mip-map level.
	Reference<const ImageMapUInt8> converted_image;
};


//struct BuildUInt8MapTextureDataScratchState
//{
//	BuildUInt8MapTextureDataScratchState() : task_manager(NULL) {}
//	~BuildUInt8MapTextureDataScratchState() { delete task_manager; }
//
//	//std::vector<Reference<Indigo::Task> > compress_tasks;
//	Indigo::TaskManager* task_manager;
//};


class TextureDataManager : public ThreadSafeRefCounted
{
public:
	// Thread-safe
	Reference<TextureData> getOrBuildTextureData(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine/*, BuildUInt8MapTextureDataScratchState& scratch_state*/);

	bool isTextureDataInserted(const ImageMapUInt8* imagemap) const; // Thread-safe

	void insertBuiltTextureData(const ImageMapUInt8* imagemap, Reference<TextureData> data);

	void removeTextureData(const ImageMapUInt8* imagemap);

	void clear();
//private:
	std::map<const ImageMapUInt8*, Reference<TextureData> > loaded_textures;
	mutable Mutex mutex;
};



class TextureLoading
{
public:
	friend class TextureLoadingTests;

	// Init stb_compress_dxt lib.
	static void init();

	// Builds compressed, mip-map level data.
	// Uses opengl_engine->getTaskManager() for multi-threading.
	static Reference<TextureData> buildUInt8MapTextureData(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine/*, BuildUInt8MapTextureDataScratchState& state*/);

	// Load the built texture data into OpenGL.
	static Reference<OpenGLTexture> loadTextureIntoOpenGL(const TextureData& texture_data, const Reference<OpenGLEngine>& opengl_engine,
		OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping);
private:
	static Reference<ImageMapUInt8> downSampleToNextMipMapLevel(const ImageMapUInt8& prev_mip_level_image, size_t level_W, size_t level_H);
};
