/*=====================================================================
DrawIndirectBuffer.h
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <stdlib.h>


/*=====================================================================
DrawIndirectBuffer
------------------

=====================================================================*/
class DrawIndirectBuffer : public RefCounted
{
public:
	DrawIndirectBuffer();
	~DrawIndirectBuffer();

	void bind();
	void unbind();

	void allocate(size_t size_B);

	void updateData(size_t dest_offset, const void* src_data, size_t src_size);

	size_t byteSize() const { return allocated_size; }

	// Invalidate the content of a buffer object's data store.
	void invalidateBufferData();

	GLuint handle;

private:
	GLARE_DISABLE_COPY(DrawIndirectBuffer)

	size_t allocated_size;
};


typedef Reference<DrawIndirectBuffer> DrawIndirectBufferRef;
