/*=====================================================================
UniformBufOb.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "UniformBufOb.h"


UniformBufOb::UniformBufOb()
:	handle(0),
	allocated_size(0)
{
	glGenBuffers(1, &handle);
}


UniformBufOb::~UniformBufOb()
{
	glDeleteBuffers(1, &handle);
}


void UniformBufOb::bind()
{
	glBindBuffer(GL_UNIFORM_BUFFER, handle);
}


void UniformBufOb::unbind()
{
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


void UniformBufOb::allocate(size_t size_B)
{
	// Buffer orphaning seems significantly slower than just using GL_DYNAMIC_DRAW and glBufferSubData() for uniform buffer obs.

	// When not doing buffer orphaning, we will use GL_DYNAMIC_DRAW:
	// "DYNAMIC: The data store contents will be modified repeatedly and used many times." (https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBufferData.xhtml)
	// "DRAW": The data store contents are modified by the application, and used as the source for GL drawing and image specification commands.
	glBindBuffer(GL_UNIFORM_BUFFER, handle);
	glBufferData(GL_UNIFORM_BUFFER, size_B, NULL, GL_DYNAMIC_DRAW); // allocate mem

	this->allocated_size = size_B;
}


void UniformBufOb::updateData(size_t dest_offset, const void* src_data, size_t src_size)
{
	assert((dest_offset + src_size) <= this->allocated_size);

	glBindBuffer(GL_UNIFORM_BUFFER, handle);
	
	glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
	
	//glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)src_size, NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See http://www.opengl.org/wiki/Buffer_Object_Streaming for details.
	//glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)src_size, src_data, GL_STREAM_DRAW); // allocate mem
}
