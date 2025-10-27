/*=====================================================================
VBO.cpp
-------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VBO.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"


#if !EMSCRIPTEN && !defined(__APPLE__)
#define VBO_MEM_MAPPING_SUPPORT 1
#endif


VBO::VBO(const void* data, size_t size_, GLenum buffer_type_, GLenum usage, bool create_persistently_mapped_buffer)
:	buffer_name(0),
	buffer_type(buffer_type_),
	size(size_),
	mapped_ptr(nullptr),
	pool_index(std::numeric_limits<size_t>::max())
{
	// Create new VBO
	glGenBuffers(1, &buffer_name);

	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);

#if EMSCRIPTEN || defined(__APPLE__)
	// Mem-mapping is not supported in WebGL.
	// GL_MAP_PERSISTENT_BIT is not defined/supported on Mac.
	assert(!create_persistently_mapped_buffer);
#endif

	// Upload vertex data to the video device
#if VBO_MEM_MAPPING_SUPPORT
	if(create_persistently_mapped_buffer)
		glBufferStorage(buffer_type, size, data, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
	else
#endif
		glBufferData(buffer_type, size, data, usage);

	// Unbind buffer
	glBindBuffer(buffer_type, 0);

	OpenGLEngine::GPUMemAllocated(size);
}


VBO::~VBO()
{
	if(mapped_ptr)
		unmap();

	glDeleteBuffers(1, &buffer_name);

	OpenGLEngine::GPUMemFreed(size);
}


void VBO::updateData(const void* data, size_t data_size)
{
	assert(data_size <= size);

	if(data_size == 0)
		return;

	glBindBuffer(buffer_type, buffer_name);

#if 0 // If do buffer orphaning:
	// Buffer orphaning, a common way to improve streaming perf. See http://www.opengl.org/wiki/Buffer_Object_Streaming for details.
	if(data_size == this->size) // If we are updating the whole buffer:
	{
		glBufferData(buffer_type, size, NULL, GL_DYNAMIC_DRAW); 
		glBufferData(buffer_type, size, data, GL_DYNAMIC_DRAW);
	}
	else
		glBufferSubData(buffer_type, /*offset=*/0, data_size, data);
#else
	glBufferSubData(buffer_type, /*offset=*/0, data_size, data);
#endif
}


void VBO::updateData(size_t offset, const void* data, size_t data_size)
{
	assert(offset + data_size <= size);

	if(data_size == 0)
		return;

	glBindBuffer(buffer_type, buffer_name);

	glBufferSubData(buffer_type, /*offset=*/offset, data_size, data);
}


void* VBO::map()
{
#if !VBO_MEM_MAPPING_SUPPORT
	assert(!"VBO::map() not supported in emscripten.");
	return nullptr;
#else
	assert(!mapped_ptr);

	bind();
	Timer timer;
	//mapped_ptr = glMapBuffer(buffer_type, GL_WRITE_ONLY);
	mapped_ptr = glMapBufferRange(buffer_type, /*offset=*/0, /*length=*/this->size, /*access=*/
		GL_MAP_WRITE_BIT | 
		GL_MAP_INVALIDATE_RANGE_BIT | // previous contents of the specified range may be discarded
		GL_MAP_INVALIDATE_BUFFER_BIT | // the previous contents of the entire buffer may be discarded
		GL_MAP_PERSISTENT_BIT | // Needed to be able to use glCopyBufferSubData to copy data from this buffer while it's still mapped.
		GL_MAP_FLUSH_EXPLICIT_BIT); // We will be flushing with glFlushMappedBufferRange which required this bit.
	if(timer.elapsed() > 0.001)
		conPrint(doubleToStringNDecimalPlaces(Clock::getTimeSinceInit() * 1.0e3, 1) + " ms > =============== SLOW VBO MAP: " + timer.elapsedStringMSWIthNSigFigs() + " for VBO " + toHexString((uint64)this) + "===============");
	assert(mapped_ptr);
	unbind();
	return mapped_ptr;
#endif
}


void VBO::unmap()
{
#if !VBO_MEM_MAPPING_SUPPORT
	assert(!"VBO::unmap() not supported in emscripten.");
#else
	assert(mapped_ptr);

	bind();
	glUnmapBuffer(buffer_type);
	unbind();
	mapped_ptr = nullptr;
#endif
}


void VBO::flushWholeBuffer()
{
	flushRange(/*offset=*/0, /*range_size=*/size);
}


void VBO::flushRange(size_t offset, size_t range_size)
{
#if !VBO_MEM_MAPPING_SUPPORT
	assert(!"VBO::flushRange() not supported in emscripten.");
#else
	assert(mapped_ptr);

	bind();
	glFlushMappedBufferRange(buffer_type, offset, range_size);
	unbind();
#endif
}


void VBO::bind() const
{
	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);
}


void VBO::unbind()
{
	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}
