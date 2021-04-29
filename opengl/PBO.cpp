/*=====================================================================
PBO.cpp
-------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "PBO.h"


PBO::PBO()
:	buffer_name(0),
	buffer_type(GL_PIXEL_UNPACK_BUFFER),
	size(0)
{
	// Create new buffer
	glGenBuffers(1, &buffer_name);

	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);

	// Upload vertex data to the video device
	//glBufferData(buffer_type, size, data, usage);

	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}


PBO::~PBO()
{
	glDeleteBuffers(1, &buffer_name);
}


void PBO::updateData(const void* data, size_t new_size)
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


void PBO::bind()
{
	glBindBuffer(buffer_type, buffer_name);
}


void PBO::unbind()
{
	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}
