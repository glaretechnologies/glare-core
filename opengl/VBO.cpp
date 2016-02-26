/*=====================================================================
VBO.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-02-13 22:44:00 +0000
=====================================================================*/
#include <GL/glew.h>


#include "VBO.h"


VBO::VBO(const void* data, size_t size_, GLenum buffer_type_)
:	buffer_type(buffer_type_),
	size(size_)
{
	// Create new VBO
	glGenBuffers(1, &buffer_name);

	// Make buffer active
	glBindBuffer(buffer_type, buffer_name);

	// Upload vertex data to the video device
	glBufferData(buffer_type, size, data, GL_STATIC_DRAW);

	// Unbind buffer
	glBindBuffer(buffer_type, 0);
}


VBO::~VBO()
{
	glDeleteBuffers(1, &buffer_name);
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
