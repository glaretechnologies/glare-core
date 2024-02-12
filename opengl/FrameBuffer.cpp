/*=====================================================================
FrameBuffer.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "FrameBuffer.h"


#include "IncludeOpenGL.h"
#include "graphics/Map2D.h"
#if CHECK_GL_CONTEXT
#include <QtOpenGL/QGLWidget>
#endif


FrameBuffer::FrameBuffer()
:	buffer_name(0),
	xres(0),
	yres(0),
	own_buffer(true)
{
#if CHECK_GL_CONTEXT
	context = QGLContext::currentContext();
#endif

	// Create new frame buffer
	glGenFramebuffers(1, &buffer_name);
}


FrameBuffer::FrameBuffer(GLuint buffer_name_)
:	buffer_name(buffer_name_),
	xres(0),
	yres(0),
	own_buffer(false)
{
#if CHECK_GL_CONTEXT
	context = NULL;
#endif
}


FrameBuffer::~FrameBuffer()
{
#if CHECK_GL_CONTEXT
	//conPrint("~FrameBuffer() currentContext(): " + toHexString((uint64)QGLContext::currentContext()));
	assert(QGLContext::currentContext() == context);
#endif

	if(own_buffer)
		glDeleteFramebuffers(1, &buffer_name);
}


void FrameBuffer::bindForReading()
{
#if CHECK_GL_CONTEXT
	assert(QGLContext::currentContext() == context);
#endif

	// Make buffer active
	glBindFramebuffer(GL_READ_FRAMEBUFFER, buffer_name);
}


void FrameBuffer::bindForDrawing()
{
#if CHECK_GL_CONTEXT
	assert(QGLContext::currentContext() == context);
#endif

	// Make buffer active
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer_name);
}


void FrameBuffer::unbind()
{
	// Unbind buffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void FrameBuffer::attachTexture(OpenGLTexture& tex, GLenum attachment_point)
{
#if CHECK_GL_CONTEXT
	assert(QGLContext::currentContext() == context);
#endif

	xres = tex.xRes();
	yres = tex.yRes();

	bindForDrawing(); // Bind this frame buffer

	// Attach the texture.  glFramebufferTexture is only in OpenGL ES 3.2+, so use glFramebufferTexture2D.
	glFramebufferTexture2D(GL_FRAMEBUFFER, // framebuffer target
		attachment_point,
		tex.getTextureTarget(),
		tex.texture_handle, // texture
		0); // mipmap level
}
