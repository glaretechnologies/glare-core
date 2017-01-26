/*=====================================================================
FrameBuffer.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "FrameBuffer.h"


FrameBuffer::FrameBuffer()
:	buffer_name(0)
{
	// Create new frame buffer
	glGenFramebuffers(1, &buffer_name);
}


FrameBuffer::~FrameBuffer()
{
	glDeleteFramebuffers(1, &buffer_name);
}


void FrameBuffer::bind()
{
	// Make buffer active
	glBindFramebuffer(GL_FRAMEBUFFER, buffer_name);
}


void FrameBuffer::unbind()
{
	// Unbind buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
