/*=====================================================================
PBO.cpp
-------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "PBO.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include <tracy/Tracy.hpp>


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

	// Upload data to the GPU
	//glBufferData(buffer_type, size, nullptr, for_upload ? GL_STREAM_DRAW : GL_STREAM_READ);
	glBufferStorage(buffer_type, size, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

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
	ZoneScoped; // Tracy profiler
#if EMSCRIPTEN
	assert(!"PBO::map() not supported in emscripten.");
	return nullptr;
#else
	assert(!mapped_ptr);

	bind();
	//mapped_ptr = glMapBuffer(buffer_type, (buffer_type == GL_PIXEL_UNPACK_BUFFER) ? GL_WRITE_ONLY : GL_READ_ONLY);
	mapped_ptr = glMapBufferRange(buffer_type, /*offset=*/0, /*length=*/this->size, /*access=*/
		GL_MAP_WRITE_BIT | 
		GL_MAP_INVALIDATE_RANGE_BIT | // previous contents of the specified range may be discarded
		GL_MAP_INVALIDATE_BUFFER_BIT | // the previous contents of the entire buffer may be discarded
		GL_MAP_PERSISTENT_BIT | // Needed to be able to use glCopyBufferSubData to copy data from this buffer while it's still mapped.
		GL_MAP_FLUSH_EXPLICIT_BIT); // We will be flushing with glFlushMappedBufferRange which required this bit.
	unbind();
	return mapped_ptr;
#endif
}


void PBO::unmap()
{
	ZoneScoped; // Tracy profiler
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


void PBO::flushWholeBuffer()
{
	flushRange(/*offset=*/0, /*range_size=*/size);
}


void PBO::flushRange(size_t offset, size_t range_size)
{
#if EMSCRIPTEN
	assert(!"PBO::flushRange() not supported in emscripten.");
#else
	assert(mapped_ptr);

	bind();
	glFlushMappedBufferRange(buffer_type, offset, range_size);
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
