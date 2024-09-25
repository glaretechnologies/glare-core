/*=====================================================================
TextureLoading.h
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../graphics/ImageMapSequence.h"
#include "../graphics/TextureData.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Mutex.h"
#include <map>


class OpenGLTexture;
class OpenGLEngine;
namespace glare { class TaskManager; }


struct OpenGLTextureLoadingProgress
{
	OpenGLTextureLoadingProgress() : num_mip_levels(0), next_mip_level(0), level_next_y(0), level_next_z(0) {}

	bool loadingInProgress() { return tex_data.nonNull(); }
	bool done() { return next_mip_level >= num_mip_levels; }

	std::string path;
	Reference<TextureData> tex_data;
	Reference<OpenGLTexture> opengl_tex;

	size_t num_mip_levels; // Num MIP levels to load into the texture
	size_t next_mip_level;
	size_t level_next_y;

	size_t level_next_z;
};


//struct BuildUInt8MapTextureDataScratchState
//{
//	BuildUInt8MapTextureDataScratchState() : task_manager(NULL) {}
//	~BuildUInt8MapTextureDataScratchState() { delete task_manager; }
//
//	//std::vector<Reference<glare::Task> > compress_tasks;
//	glare::TaskManager* task_manager;
//};


/*=====================================================================
TextureLoading
--------------
Code for loading a texture into OpenGL.
Tests are in TextureLoadingTests.cpp
=====================================================================*/
class TextureLoading
{
public:
	friend class TextureLoadingTests;

	// Load the texture data for frame_i into an existing OpenGL texture.  Used for animated images.
	static void loadIntoExistingOpenGLTexture(Reference<OpenGLTexture>& opengl_tex, const TextureData& texture_data, size_t frame_i);


	static void initialiseTextureLoadingProgress(const std::string& path, const Reference<OpenGLEngine>& opengl_engine, const OpenGLTextureKey& key, const TextureParams& texture_params, 
		const Reference<TextureData>& tex_data, OpenGLTextureLoadingProgress& loading_progress);

	static void partialLoadTextureIntoOpenGL(const Reference<OpenGLEngine>& opengl_engine, OpenGLTextureLoadingProgress& loading_progress, 
		size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes);

private:
	static Reference<OpenGLTexture> createUninitialisedOpenGLTexture(const TextureData& texture_data, const Reference<OpenGLEngine>& opengl_engine, const TextureParams& texture_params);

	static Reference<OpenGLTexture> createUninitialisedTextureForMap2D(const Map2D& map2d, const Reference<OpenGLEngine>& opengl_engine, const TextureParams& texture_params);
};
