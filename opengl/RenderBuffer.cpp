/*=====================================================================
RenderBuffer.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "RenderBuffer.h"


#include "IncludeOpenGL.h"


RenderBuffer::RenderBuffer(size_t tex_xres, size_t tex_yres, int MSAA_samples_, OpenGLTextureFormat format)
:	buffer_name(0),
	xres(tex_xres),
	yres(tex_yres),
	MSAA_samples(MSAA_samples_)
{

	// Create new render buffer
	glGenRenderbuffers(1, &buffer_name);

	GLint gl_internal_format;
	GLenum gl_format, gl_type;
	OpenGLTexture::getGLFormat(format, gl_internal_format, gl_format, gl_type);

	bind();

	if(MSAA_samples <= 1)
	{
		glRenderbufferStorage(GL_RENDERBUFFER, /*internal format=*/gl_internal_format, (GLsizei)xres, (GLsizei)yres);
	}
	else
	{
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_samples, /*internal format=*/gl_internal_format, (GLsizei)xres, (GLsizei)yres);
	}

	unbind();
}


RenderBuffer::~RenderBuffer()
{
	glDeleteRenderbuffers(1, &buffer_name);
}


void RenderBuffer::bind()
{
	glBindRenderbuffer(GL_RENDERBUFFER, buffer_name);
}


void RenderBuffer::unbind()
{
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
