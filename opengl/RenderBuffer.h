/*=====================================================================
RenderBuffer.h
--------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "OpenGLTexture.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


/*=====================================================================
RenderBuffer
------------

=====================================================================*/
class RenderBuffer : public RefCounted
{
public:
	RenderBuffer(size_t tex_xres, size_t tex_yres, int MSAA_samples, OpenGLTextureFormat format);
	~RenderBuffer();

	void bind();

	static void unbind();

	
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }
	int MSAASamples() const { return MSAA_samples; }
private:
	GLARE_DISABLE_COPY(RenderBuffer);
public:
	
	GLuint buffer_name;
	size_t xres, yres;
	int MSAA_samples;
	size_t storage_size;
};


typedef Reference<RenderBuffer> RenderBufferRef;
