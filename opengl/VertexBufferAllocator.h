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
#include "../utils/BestFitAllocator.h"
#include <map>
#include <limits>


class VertexBufferAllocator;


// A reference-counted stucture, than call call allocator->free() when all references to it are destroyed.
// Not the most efficient way of doing ref-counting on vertex blocks, but should be fine for our purposes.
struct BlockHandle : public RefCounted
{
	BlockHandle(glare::BestFitAllocator::BlockInfo* block_) : block(block_) {}
	~BlockHandle()
	{
		glare::BestFitAllocator* allocator = block->allocator;
		assert(allocator); // Should be non-null unless the BestFitAllocator has been destroyed (internal logic error)
		if(allocator)
			allocator->free(block);
	}

	glare::BestFitAllocator::BlockInfo* block;
};


struct VertBufAllocationHandle
{
	VertBufAllocationHandle() {}

	VBORef vbo;
	size_t per_spec_data_index;
	size_t offset; // offset in VBO, in bytes
	size_t size; // size of allocation
	int base_vertex;

	Reference<BlockHandle> block_handle;

	inline bool valid() const { return vbo.nonNull(); }
};


struct IndexBufAllocationHandle
{
	IndexBufAllocationHandle() : offset(std::numeric_limits<size_t>::max()) {}

	VBORef index_vbo;
	//size_t per_spec_data_index;
	size_t offset; // offset in VBO
	size_t size; // size of allocation

	Reference<BlockHandle> block_handle;

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
		VAORef vao;
		size_t next_offset;
	};


	struct VBOAndAllocator
	{
		VBORef vbo;
		Reference<glare::BestFitAllocator> allocator;
	};

	std::map<VertexSpec, int> per_spec_data_index; // Map from vertex spec to index into per_spec_data
	std::vector<PerSpecData> per_spec_data;

	std::vector<VBOAndAllocator> vert_vbos;

	std::vector<VBOAndAllocator> index_vbos;

	size_t use_VBO_size_B;
};


typedef Reference<VertexBufferAllocator> VertexBufferAllocatorRef;
