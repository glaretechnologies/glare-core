/*=====================================================================
PBO.cpp
-------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "PBO.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"


PBO::PBO(size_t size_, bool for_upload)
:	buffer_name(0),
	buffer_type(for_upload ? GL_PIXEL_UNPACK_BUFFER : GL_PIXEL_PACK_BUFFER),
	size(size_),
	mapped_ptr(nullptr)
{
	// Create new buffer
	glGenBuffers(1, &buffer_name);

	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);

	// Upload vertex data to the video device
	glBufferData(buffer_type, size, nullptr, for_upload ? GL_STREAM_DRAW : /*GL_STREAM_READ*/GL_STREAM_READ);

	// Unbind buffer
	glBindBuffer(buffer_type, 0);

	OpenGLEngine::GPUMemAllocated(size);
}


PBO::~PBO()
{
	assert(!mapped_ptr);

	glDeleteBuffers(1, &buffer_name);

	OpenGLEngine::GPUMemFreed(size);
}


void* PBO::map()
{
#if EMSCRIPTEN
	assert(!"PBO::map() not supported in emscripten.");
	return nullptr;
#else
	assert(!mapped_ptr);

	bind();
	mapped_ptr = glMapBuffer(buffer_type, (buffer_type == GL_PIXEL_UNPACK_BUFFER) ? GL_WRITE_ONLY : GL_READ_ONLY);
	unbind();
	return mapped_ptr;
#endif
}


void PBO::unmap()
{
#if EMSCRIPTEN
	assert(!"PBO::unmap() not supported in emscripten.");
#else
	assert(mapped_ptr);

	bind();
	glUnmapBuffer(buffer_type);
	mapped_ptr = nullptr;
	unbind();
#endif
}


void PBO::bind() const
{
	glBindBuffer(buffer_type, buffer_name);
}


void PBO::unbind()
{
	glBindBuffer(buffer_type, 0);
}
