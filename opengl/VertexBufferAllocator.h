/*=====================================================================
VertexBufferAllocator.h
-----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "VAO.h"
#include "VBO.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <map>
#include <limits>


struct VertBufAllocationHandle
{
	VBORef vbo;
	size_t per_spec_data_index;
	size_t offset; // offset in VBO, in bytes
	size_t size; // size of allocation
	int base_vertex;

	inline bool valid() const { return vbo.nonNull(); }
};


struct IndexBufAllocationHandle
{
	IndexBufAllocationHandle() : offset(std::numeric_limits<size_t>::max()) {}

	VBORef index_vbo;
	//size_t per_spec_data_index;
	size_t offset; // offset in VBO
	size_t size; // size of allocation

	inline bool valid() const { return offset != std::numeric_limits<size_t>::max(); }
};


// This is for the Mac, that can't easily do VAO sharing due to having to use glVertexAttribPointer().
#ifdef OSX
#define DO_INDIVIDUAL_VAO_ALLOC 1
#else
#define DO_INDIVIDUAL_VAO_ALLOC 0
#endif


/*=====================================================================
VertexBufferAllocator
---------------------

=====================================================================*/
class VertexBufferAllocator : public RefCounted
{
public:
	VertexBufferAllocator();
	~VertexBufferAllocator();


	VertBufAllocationHandle allocate(const VertexSpec& vertex_spec, const void* data, size_t size);

	IndexBufAllocationHandle allocateIndexData(const void* data, size_t size);

private:
	GLARE_DISABLE_COPY(VertexBufferAllocator)
public:
	
	struct PerSpecData
	{
		VBORef vbo;
		VAORef vao;
		size_t next_offset;
	};

	std::map<VertexSpec, int> per_spec_data_index; // Map from vertex spec to index into per_spec_data
	std::vector<PerSpecData> per_spec_data;

	VBORef main_vbo;
	size_t main_vbo_offset;

	VBORef indices_vbo;
	size_t next_indices_offset;
};


typedef Reference<VertexBufferAllocator> VertexBufferAllocatorRef;
