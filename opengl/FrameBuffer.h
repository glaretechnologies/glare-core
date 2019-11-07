/*=====================================================================
FrameBuffer.h
-------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
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

private:
	INDIGO_DISABLE_COPY(FrameBuffer);
public:
	
	GLuint buffer_name;
};
