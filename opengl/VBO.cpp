/*=====================================================================
VBO.cpp
-------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VBO.h"


VBO::VBO(const void* data, size_t size_, GLenum buffer_type_, GLenum usage)
:	buffer_name(0),
	buffer_type(buffer_type_),
	size(size_)
{
	// Create new VBO
	glGenBuffers(1, &buffer_name);

	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);

	// Upload vertex data to the video device
	glBufferData(buffer_type, size, data, usage);

	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}


VBO::~VBO()
{
	glDeleteBuffers(1, &buffer_name);
}


void VBO::updateData(const void* data, size_t data_size)
{
	assert(data_size <= size);

	if(data_size == 0)
		return;

	// From http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/particles-instancing/
	// Update the buffers that OpenGL uses for rendering.
	// There are much more sophisticated means to stream data from the CPU to the GPU,
	// but this is outside the scope of this tutorial.
	// http://www.opengl.org/wiki/Buffer_Object_Streaming

	glBindBuffer(buffer_type, buffer_name);

	// NOTE: buffer orphaning seems slower.
	//glBufferData(buffer_type, size, NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
	
	glBufferSubData(buffer_type, /*offset=*/0, data_size, data);
}


void VBO::updateData(size_t offset, const void* data, size_t data_size)
{
	assert(offset + data_size <= size);

	if(data_size == 0)
		return;

	glBindBuffer(buffer_type, buffer_name);

	glBufferSubData(buffer_type, /*offset=*/offset, data_size, data);
}


void VBO::bind() const
{
	// Make buffer active
#ifdef OSX // OSX doesn't define GL_SHADER_STORAGE_BUFFER
	const GLenum use_buffer_type = buffer_type;
#else
	const GLenum use_buffer_type = (buffer_type == GL_SHADER_STORAGE_BUFFER) ? GL_ARRAY_BUFFER : buffer_type;
#endif
	glBindBuffer(use_buffer_type, buffer_name);
}


void VBO::unbind()
{
	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}
