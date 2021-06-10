/*=====================================================================
UniformBufOb.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "UniformBufOb.h"


UniformBufOb::UniformBufOb()
:	handle(0)
{
	// Create new VAO
	//glGenBuffers(1, &handle);

	// Make buffer active
	//glBindVertexArray(handle);

	glGenBuffers(1, &handle);

}


UniformBufOb::~UniformBufOb()
{
	glDeleteBuffers(1, &handle);
}


void UniformBufOb::bind()
{
	// Make buffer active
	glBindBuffer(GL_UNIFORM_BUFFER, handle);
}


void UniformBufOb::unbind()
{
	// Unbind buffer
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


void UniformBufOb::allocate(size_t size_B)
{
	glBindBuffer(GL_UNIFORM_BUFFER, handle);
	glBufferData(GL_UNIFORM_BUFFER, size_B, NULL, GL_STATIC_DRAW); // allocate mem
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


void UniformBufOb::updateData(size_t dest_offset, const void* src_data, size_t src_size)
{
	glBindBuffer(GL_UNIFORM_BUFFER, handle);
	//glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
	glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)src_size, src_data, GL_STATIC_DRAW); // allocate mem
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
