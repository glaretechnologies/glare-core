/*=====================================================================
TextureLoading.h
-----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


class OpenGLEngine;


class TextureLoading
{
public:
	friend class TextureLoadingTests;

	// Init stb_compress_dxt lib.
	static void init();

	static Reference<OpenGLTexture> loadUInt8Map(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine,
		OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping);

private:
	static Reference<ImageMapUInt8> downSampleToNextMipMapLevel(const ImageMapUInt8& prev_mip_level_image, size_t level_W, size_t level_H);
};
