/*=====================================================================
VBO.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-02-13 22:44:00 +0000
=====================================================================*/
#include "IncludeOpenGL.h"
#include "VBO.h"


VBO::VBO(const void* data, size_t size_, GLenum buffer_type_, GLenum usage)
:	buffer_name(0),
	buffer_type(buffer_type_),
	size(size_)
{
	assert(buffer_type == GL_ARRAY_BUFFER || buffer_type == GL_ELEMENT_ARRAY_BUFFER);
	assert(usage == GL_STATIC_DRAW || usage == GL_STREAM_DRAW);

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


void VBO::updateData(const void* data, size_t new_size)
{
	// From http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/particles-instancing/
	// Update the buffers that OpenGL uses for rendering.
	// There are much more sophisticated means to stream data from the CPU to the GPU,
	// but this is outside the scope of this tutorial.
	// http://www.opengl.org/wiki/Buffer_Object_Streaming

	glBindBuffer(buffer_type, buffer_name);
	glBufferData(buffer_type, size, NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
	glBufferSubData(buffer_type, 0, new_size, data);
	this->size = new_size;
}


void VBO::bind()
{
	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);
}


void VBO::unbind()
{
	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}
