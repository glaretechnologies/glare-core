/*=====================================================================
UniformBufOb.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "UniformBufOb.h"


#include "IncludeOpenGL.h"


// For emscripten
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER                 0x8A11
#endif


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
	
#if defined(__APPLE__)// || defined(EMSCRIPTEN)
	if(src_size == this->allocated_size) // If we are updating the whole buffer:
	{
		assert(dest_offset == 0);
		// Use buffer orphaning, a common way to improve streaming perf. See http://www.opengl.org/wiki/Buffer_Object_Streaming for details.
		// This seems to be essential on Mac, otherwise we get stalls when reusing buffers for different materials,
		// resulting in extremely slow rendering.
		glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)src_size, NULL, GL_DYNAMIC_DRAW);
		glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)src_size, src_data, GL_DYNAMIC_DRAW);
	}
	else
		glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
#else
	glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
#endif
	
}
