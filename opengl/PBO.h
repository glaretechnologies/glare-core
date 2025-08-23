/*=====================================================================
PBO.h
-----
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include <stddef.h> // for size_t


/*=====================================================================
PBO
---
Pixel buffer object
=====================================================================*/
class PBO : public ThreadSafeRefCounted
{
public:
	// data can be NULL
	PBO(size_t size, bool for_upload = true);
	~PBO();

	void* map();
	void unmap();

	void flushWholeBuffer();
	void flushRange(size_t offset, size_t range_size);

	void* getMappedPtr() { return mapped_ptr; }

	void bind() const;
	void unbind();

	GLuint bufferName() const { return buffer_name; }
	size_t getSize() const{ return size; }
private:
	GLARE_DISABLE_COPY(PBO)

	GLuint buffer_name;
	GLenum buffer_type;
	size_t size; // in bytes

	void* mapped_ptr;
};


typedef Reference<PBO> PBORef;
