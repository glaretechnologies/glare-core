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
#include <vector>


class OpenGLEngine;


class OpenGLTexture : public RefCounted
{
public:
	OpenGLTexture();
	~OpenGLTexture();

	bool hasAlpha() const;

	enum Filtering
	{
		Filtering_Nearest,
		Filtering_Bilinear,
		Filtering_Fancy // Trilinear + anisotropic filtering if available
	};

	enum Wrapping
	{
		Wrapping_Repeat,
		Wrapping_Clamp
	};

	void loadCubeMap(size_t tex_xres, size_t tex_yres, const std::vector<const uint8*>& tex_data, const Reference<OpenGLEngine>& opengl_engine,
		GLint internal_format,
		GLenum format,
		GLenum type,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat
	);

	void load(size_t tex_xres, size_t tex_yres, const uint8* tex_data, const Reference<OpenGLEngine>& opengl_engine,
		GLint internal_format,
		GLenum format,
		GLenum type,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat
	);

	GLuint texture_handle;

private:
	INDIGO_DISABLE_COPY(OpenGLTexture);

	GLenum format;
};
