/*=====================================================================
FrameBuffer.h
-------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
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
	~FrameBuffer();

	void bind();
	static void unbind();

	// attachment_point is GL_DEPTH_ATTACHMENT, GL_COLOR_ATTACHMENT0 etc..
	void bindTextureAsTarget(OpenGLTexture& tex, GLenum attachment_point);
private:
	INDIGO_DISABLE_COPY(FrameBuffer);
public:
	
	GLuint buffer_name;
};


typedef Reference<FrameBuffer> FrameBufferRef;
