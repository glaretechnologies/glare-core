/*=====================================================================
DrawIndirectBuffer.cpp
----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "DrawIndirectBuffer.h"


#include "IncludeOpenGL.h"


DrawIndirectBuffer::DrawIndirectBuffer()
:	handle(0),
	allocated_size(0)
{
	glGenBuffers(1, &handle);
}


DrawIndirectBuffer::~DrawIndirectBuffer()
{
	glDeleteBuffers(1, &handle);
}


void DrawIndirectBuffer::bind()
{
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, handle);
}


void DrawIndirectBuffer::unbind()
{
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}


void DrawIndirectBuffer::allocate(size_t size_B)
{
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, handle);

	glBufferData(GL_DRAW_INDIRECT_BUFFER, size_B, NULL, GL_DYNAMIC_DRAW); // allocate mem

//	glBufferStorage(GL_DRAW_INDIRECT_BUFFER, size_B, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_DYNAMIC_STORAGE_BIT);

	//glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

	this->allocated_size = size_B;

	unbind();
}


void DrawIndirectBuffer::updateData(size_t dest_offset, const void* src_data, size_t src_size)
{
	assert((dest_offset + src_size) <= this->allocated_size);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, handle);
	
	glBufferSubData(GL_DRAW_INDIRECT_BUFFER, (GLintptr)dest_offset, (GLsizeiptr)src_size, src_data);
}


// Invalidate the content of a buffer object's data store.
void DrawIndirectBuffer::invalidateBufferData()
{
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, handle);

	glInvalidateBufferData(handle);
}
