/*=====================================================================
FrameBuffer.h
-------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "OpenGLTexture.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
class OpenGLShader;


/*=====================================================================
FrameBuffer
---------

=====================================================================*/
class FrameBuffer : public RefCounted
{
public:
	FrameBuffer();
	explicit FrameBuffer(GLuint buffer_name); // Does not take ownership of framebuffer.
	~FrameBuffer();

	void bind();
	static void unbind();

	// attachment_point is GL_DEPTH_ATTACHMENT, GL_COLOR_ATTACHMENT0 etc..
	void bindTextureAsTarget(OpenGLTexture& tex, GLenum attachment_point);

	// Will return 0 if texture has not been bound yet.
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }
private:
	GLARE_DISABLE_COPY(FrameBuffer);
public:
	
	GLuint buffer_name;
	size_t xres, yres; // Will be set after bindTextureAsTarget() is called, and 0 beforehand.
	bool own_buffer; // If true, will call glDeleteFramebuffers on destruction.
};


typedef Reference<FrameBuffer> FrameBufferRef;
