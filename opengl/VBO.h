/*=====================================================================
VBO.h
-----
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <stdlib.h>


/*=====================================================================
VBO
---
Vertex buffer object
=====================================================================*/
class VBO : public ThreadSafeRefCounted
{
public:
	// data can be NULL
	VBO(const void* data, size_t size, GLenum buffer_type = /*GL_ARRAY_BUFFER=*/0x8892, GLenum usage = /*GL_STATIC_DRAW=*/0x88E4);
	~VBO();


	void updateData(const void* data, size_t data_size); // data_size must be <= size

	void updateData(size_t offset, const void* data, size_t data_size);

	void* map();
	void unmap();

	void flushRange(size_t offset, size_t range_size);

	void* getMappedPtr() { return mapped_ptr; }

	void bind() const;
	void unbind();

	GLuint bufferName() const { return buffer_name; }
	size_t getSize() const{ return size; }
private:
	GLARE_DISABLE_COPY(VBO)

	GLuint buffer_name;
	GLenum buffer_type;
	size_t size; // in bytes

	void* mapped_ptr;
};


typedef Reference<VBO> VBORef;
