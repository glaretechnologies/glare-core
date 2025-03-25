/*=====================================================================
VBO.cpp
-------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VBO.h"


#include "IncludeOpenGL.h"


VBO::VBO(const void* data, size_t size_, GLenum buffer_type_, GLenum usage)
:	buffer_name(0),
	buffer_type(buffer_type_),
	size(size_),
	mapped_ptr(nullptr)
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
	assert(!mapped_ptr);

	glDeleteBuffers(1, &buffer_name);
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
	assert(!mapped_ptr);

	bind();
	mapped_ptr = glMapBuffer(buffer_type, GL_WRITE_ONLY);
	unbind();
	return mapped_ptr;
}


void VBO::unmap()
{
	assert(mapped_ptr);

	bind();
	glUnmapBuffer(buffer_type);
	unbind();
	mapped_ptr = nullptr;
}


void VBO::bind() const
{
	// Make buffer active
#if defined(OSX) || defined(EMSCRIPTEN) // OSX doesn't define GL_SHADER_STORAGE_BUFFER
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
