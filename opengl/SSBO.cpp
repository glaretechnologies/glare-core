/*=====================================================================
SSBO.cpp
--------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "SSBO.h"


#include "IncludeOpenGL.h"
#include <ConPrint.h>
#include <cstring> // For memcpy


// Just for Mac
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT             0x0040
#endif


SSBO::SSBO()
:	handle(0),
	allocated_size(0),
	mapped_buffer(0)
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


void SSBO::allocate(size_t size_B, bool map_memory)
{
	if(map_memory)
	{
		allocateForMapping(size_B);
		map();
	}
	else
	{
		// "DYNAMIC: The data store contents will be modified repeatedly and used many times." (https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBufferData.xhtml)
		// "DRAW": The data store contents are modified by the application, and used as the source for GL drawing and image specification commands.
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size_B, /*data=*/NULL, GL_DYNAMIC_DRAW); // allocate mem
		//glBufferData(GL_SHADER_STORAGE_BUFFER, size_B, /*data=*/NULL, GL_STATIC_DRAW); // allocate mem

		this->allocated_size = size_B;
	}
}


void SSBO::allocateForMapping(size_t size_B)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
#ifdef OSX
	assert(0);
#else
	glBufferStorage(GL_SHADER_STORAGE_BUFFER, size_B, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_DYNAMIC_STORAGE_BIT);
#endif

	this->allocated_size = size_B;
}


void SSBO::updateData(size_t dest_offset, const void* src_data, size_t src_size, bool bind_needed)
{
	assert((dest_offset + src_size) <= this->allocated_size);
	if(!((dest_offset + src_size) <= this->allocated_size))
		conPrint("ERROR: SSBO::updateData !((dest_offset + src_size) <= this->allocated_size)");

	if(mapped_buffer)
	{
		std::memcpy((uint8*)mapped_buffer + dest_offset, src_data, src_size);

		if(bind_needed)
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);

		glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, /*offset=*/dest_offset, src_size);
	}
	else
	{
		if(bind_needed)
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);

		glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
	}
}


void SSBO::readData(size_t src_offset, void* dst_data, size_t size)
{
	assert((src_offset + size) <= this->allocated_size);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);

	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, src_offset, size, dst_data);
}


void* SSBO::map()
{
	bind();

	this->mapped_buffer = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, /*offset=*/0, /*length=*/this->allocated_size, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT); // NOTE GL_MAP_FLUSH_EXPLICIT_BIT is new
	if(this->mapped_buffer == NULL)
	{
		conPrint("mapping failed!");
	}
	return mapped_buffer;

	//TODO: add unmapping
}


void SSBO::flushRange(size_t offset, size_t size)
{
	bind();

	glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, /*offset=*/offset, size);
}


// Invalidate the content of a buffer object's data store.
void SSBO::invalidateBufferData()
{
#ifdef OSX
	assert(0); // glInvalidateBufferData is OpenGL 4.3+
#else
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);

	glInvalidateBufferData(handle);
#endif
}
