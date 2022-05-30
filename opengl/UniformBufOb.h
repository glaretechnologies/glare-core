/*=====================================================================
UniformBufOb.h
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <vector>



/*=====================================================================
UniformBufOb
------------

=====================================================================*/
class UniformBufOb : public RefCounted
{
public:
	UniformBufOb();
	~UniformBufOb();

	void bind();
	void unbind();

	void allocate(size_t size_B);

	void updateData(size_t dest_offset, const void* src_data, size_t src_size);

	GLuint handle;

private:
	GLARE_DISABLE_COPY(UniformBufOb)

	size_t allocated_size;
};


typedef Reference<UniformBufOb> UniformBufObRef;
