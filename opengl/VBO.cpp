/*=====================================================================
VBO.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-02-13 22:44:00 +0000
=====================================================================*/
#include <GL/glew.h>


#include "VBO.h"
#include <GL/GL.h>


VBO::VBO(const float* data, size_t data_num_floats)
{
	// Create new VBO
	glGenBuffers(1, &buffer_name);

	// Make buffer active
	glBindBuffer(GL_ARRAY_BUFFER, buffer_name);

	// Upload vertex data to the video device
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * data_num_floats, data, GL_STATIC_DRAW);

	// Unbind buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


VBO::~VBO()
{
	glDeleteBuffers(1, &buffer_name);
}


void VBO::bind()
{
	// Make buffer active
	glBindBuffer(GL_ARRAY_BUFFER, buffer_name);
}


void VBO::unbind()
{
	// Unbind buffer
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
