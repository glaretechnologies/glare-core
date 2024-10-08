/*=====================================================================
SSBO.h
------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <stdlib.h>


/*=====================================================================
SSBO
----
Shader Storage Buffer Object
=====================================================================*/
class SSBO : public RefCounted
{
public:
	SSBO();
	~SSBO();

	void bind();
	void unbind();

	void allocate(size_t size_B, bool map_memory);

	void allocateForMapping(size_t size_B);

	void updateData(size_t dest_offset, const void* src_data, size_t src_size, bool bind_needed = true);

	void readData(size_t src_offset, void* dst_data, size_t size);

	void* map();

	void flushRange(size_t offset, size_t size);

	// Invalidate the content of a buffer object's data store.
	void invalidateBufferData();

	size_t byteSize() const { return allocated_size; }

	GLuint handle;

private:
	GLARE_DISABLE_COPY(SSBO)

	size_t allocated_size;

	void* mapped_buffer;
};


typedef Reference<SSBO> SSBORef;
