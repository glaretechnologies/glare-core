/*=====================================================================
SSBO.cpp
--------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "SSBO.h"


#include "IncludeOpenGL.h"


// Just for Mac
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#endif


SSBO::SSBO()
:	handle(0),
	allocated_size(0)
{
	glGenBuffers(1, &handle);
}


SSBO::~SSBO()
{
	glDeleteBuffers(1, &handle);
}


void SSBO::bind()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
}


void SSBO::unbind()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}


void SSBO::allocate(size_t size_B)
{
	// "DYNAMIC: The data store contents will be modified repeatedly and used many times." (https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBufferData.xhtml)
	// "DRAW": The data store contents are modified by the application, and used as the source for GL drawing and image specification commands.
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, size_B, /*data=*/NULL, GL_DYNAMIC_DRAW); // allocate mem

	this->allocated_size = size_B;
}


void SSBO::updateData(size_t dest_offset, const void* src_data, size_t src_size)
{
	assert((dest_offset + src_size) <= this->allocated_size);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
	
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
}
