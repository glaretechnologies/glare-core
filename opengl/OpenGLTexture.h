/*=====================================================================
OpenGLTexture.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/IncludeWindows.h" // This needs to go first for NOMINMAX.
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
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

	enum Format
	{
		Format_Greyscale_Uint8,
		Format_SRGB_Uint8,
		Format_SRGBA_Uint8,
		Format_RGB_Linear_Uint8,
		Format_RGBA_Linear_Uint8,
		Format_RGB_Linear_Float,
		Format_RGB_Linear_Half,
		Format_Depth_Float,
		Format_Compressed_SRGB_Uint8,
		Format_Compressed_SRGBA_Uint8,
		Format_Compressed_BC6 // BC6 half-float unsigned format: e.g. GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
	};

	void loadCubeMap(size_t tex_xres, size_t tex_yres, const std::vector<const void*>& tex_data, Format format);


	void makeGLTexture(Format format);
	void setMipMapLevelData(int mipmap_level, size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data);
	void setTexParams(const Reference<OpenGLEngine>& opengl_engine,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat
	);

	void load(size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data, const Reference<OpenGLEngine>& opengl_engine,
		Format format,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat
	);

	// Will return 0 if texture has not been loaded yet.
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }

	size_t getByteSize() const;

	GLuint texture_handle;

private:
	GLARE_DISABLE_COPY(OpenGLTexture);
	void getGLFormat(Format format, GLint& internal_format, GLenum& gl_format, GLenum& type);

	Format format;

	size_t xres, yres; // Will be set after load() etc.. is called, and 0 beforehand.

	size_t loaded_size;
};


typedef Reference<OpenGLTexture> OpenGLTextureRef;
