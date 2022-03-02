/*=====================================================================
DrawIndirectBuffer.h
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <vector>


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

	size_t byteSize() const { return allocated_size;  }

	GLuint handle;

private:
	GLARE_DISABLE_COPY(DrawIndirectBuffer)

	size_t allocated_size;
};


typedef Reference<DrawIndirectBuffer> DrawIndirectBufferRef;
