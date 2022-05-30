/*=====================================================================
FrameBuffer.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "FrameBuffer.h"


#include "IncludeOpenGL.h"


FrameBuffer::FrameBuffer()
:	buffer_name(0),
	xres(0),
	yres(0),
	own_buffer(true)
{
	// Create new frame buffer
	glGenFramebuffers(1, &buffer_name);
}


FrameBuffer::FrameBuffer(GLuint buffer_name_)
:	buffer_name(buffer_name_),
	xres(0),
	yres(0),
	own_buffer(false)
{
}


FrameBuffer::~FrameBuffer()
{
	if(own_buffer)
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


void FrameBuffer::bindTextureAsTarget(OpenGLTexture& tex, GLenum attachment_point)
{
	xres = tex.xRes();
	yres = tex.yRes();

	bind(); // Bind this frame buffer

	// Bind the texture
	glFramebufferTexture(GL_FRAMEBUFFER, // target
		attachment_point,
		tex.texture_handle, // texture
		0); // mipmap level
}
