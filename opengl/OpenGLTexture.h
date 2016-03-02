/*=====================================================================
OpenGLTexture.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <QtOpenGL/QGLWidget>
#include "../utils/IncludeWindows.h" // This needs to go first for NOMINMAX.
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


class OpenGLEngine;


class OpenGLTexture : public RefCounted
{
public:
	OpenGLTexture();
	~OpenGLTexture();

	void load(size_t tex_xres, size_t tex_yres, const uint8* tex_data, Reference<OpenGLEngine>& opengl_engine/*bool anisotropic_filtering_supported, float max_anisotropy*/);

	GLuint texture_handle;

private:
	INDIGO_DISABLE_COPY(OpenGLTexture);
};
