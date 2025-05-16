/*=====================================================================
FrameBuffer.cpp
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "FrameBuffer.h"


#include "IncludeOpenGL.h"
#include "RenderBuffer.h"
#if CHECK_GL_CONTEXT
#include <QtOpenGL/QGLWidget>
#endif
#if EMSCRIPTEN
#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl2ext.h>
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


void FrameBuffer::detachTexture(OpenGLTexture& tex, GLenum attachment_point)
{
	bindForDrawing(); // Bind this frame buffer

	glFramebufferTexture2D(GL_FRAMEBUFFER, // framebuffer target
		attachment_point,
		tex.getTextureTarget(),
		0, // texture
		0); // mipmap level

	unbind();
}


void FrameBuffer::attachRenderBuffer(RenderBuffer& render_buffer, GLenum attachment_point)
{
	xres = render_buffer.xRes();
	yres = render_buffer.yRes();

	bindForDrawing(); // Bind this frame buffer

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER, // target (NOTE: could be just GL_READ_FRAMEBUFFER or GL_DRAW_FRAMEBUFFER)
		attachment_point,
		GL_RENDERBUFFER, // render buffer target, must be GL_RENDERBUFFER
		render_buffer.buffer_name
	);
}


GLuint FrameBuffer::getAttachedRenderBufferName(GLenum attachment_point)
{
	// Check a renderbuffer is bound, as opposed to a texture.
	GLint ob_type = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment_point, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &ob_type);
	assert(ob_type == GL_RENDERBUFFER);

	GLint name = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment_point, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
	return (GLuint)name;
}


GLuint FrameBuffer::getAttachedTextureName(GLenum attachment_point)
{
	// Check a texture is bound, as opposed to a renderbuffer.
	GLint ob_type = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment_point, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &ob_type);
	assert(ob_type == GL_TEXTURE);

	GLint name = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment_point, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
	return (GLuint)name;
}


GLenum FrameBuffer::checkCompletenessStatus()
{
	bindForDrawing(); // Bind this frame buffer
	return glCheckFramebufferStatus(GL_FRAMEBUFFER);
}


bool FrameBuffer::isComplete()
{
	return checkCompletenessStatus() == GL_FRAMEBUFFER_COMPLETE;
}


void FrameBuffer::discardContents(ArrayRef<GLenum> attachments)
{
#if EMSCRIPTEN
	bindForDrawing(); // Bind this frame buffer

	glDiscardFramebufferEXT(GL_FRAMEBUFFER, /*num attachments=*/(GLsizei)attachments.size(), /*attachments=*/attachments.data());

	unbind();
#endif
}


void FrameBuffer::discardContents(GLenum attachment_a)
{
	const GLenum attachments[] = { attachment_a };
	discardContents(ArrayRef<GLenum>(attachments, staticArrayNumElems(attachments)));
}


void FrameBuffer::discardContents(GLenum attachment_a, GLenum attachment_b)
{
	const GLenum attachments[] = { attachment_a, attachment_b };
	discardContents(ArrayRef<GLenum>(attachments, staticArrayNumElems(attachments)));
}


void FrameBuffer::discardContents(GLenum attachment_a, GLenum attachment_b, GLenum attachment_c)
{
	const GLenum attachments[] = { attachment_a, attachment_b, attachment_c};
	discardContents(ArrayRef<GLenum>(attachments, staticArrayNumElems(attachments)));
}


void FrameBuffer::discardDefaultFrameBufferContents()
{
#if EMSCRIPTEN
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Bind the default framebuffer

	const GLenum attachments[2] = { GL_COLOR_EXT, GL_DEPTH_EXT };
	glDiscardFramebufferEXT(GL_FRAMEBUFFER, /*num attachments=*/staticArrayNumElems(attachments), /*attachments=*/attachments);
#endif
}
