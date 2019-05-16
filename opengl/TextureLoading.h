/*=====================================================================
TextureLoading.h
-----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


//#include "IncludeWindows.h"
#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <vector>


class OpenGLEngine;


class TextureLoading
{
public:
	static Reference<OpenGLTexture> loadUInt8Map(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine,
		OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping);
};
