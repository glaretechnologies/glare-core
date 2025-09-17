/*=====================================================================
VertexBufferAllocator.h
-----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "VAO.h"
#include "VBO.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include "../utils/BestFitAllocator.h"
#include "../utils/Mutex.h"
#include <map>
#include <limits>


class VertexBufferAllocator;
class OpenGLMeshRenderData;


// A reference-counted structure, that calls allocator->freeBlock() when all references to it are destroyed.
// Not the most efficient way of doing ref-counting on vertex blocks, but should be fine for our purposes.
struct BlockHandle : public ThreadSafeRefCounted
{
	BlockHandle(VertexBufferAllocator* vert_allocator_, glare::BestFitAllocator::BlockInfo* block_) : vert_allocator(vert_allocator_), block(block_) {}
	~BlockHandle();

	VertexBufferAllocator* vert_allocator;
	glare::BestFitAllocator::BlockInfo* block;
};


struct VertBufAllocationHandle
{
	VertBufAllocationHandle() : base_vertex(-1) {}

	VBORef vbo;
	size_t vbo_id; // Used for sorting batch draws by VBO used.
	size_t offset; // offset in VBO, in bytes
	size_t size; // size of allocation
	int base_vertex; // offset to be added to indices when getting vertex data

	Reference<BlockHandle> block_handle;

	inline bool valid() const { return vbo.nonNull(); }
};


struct IndexBufAllocationHandle
{
	IndexBufAllocationHandle() : offset(std::numeric_limits<size_t>::max()) {}

	VBORef index_vbo;
	size_t vbo_id; // Used for sorting batch draws by VBO used.
	size_t offset; // offset in VBO, in bytes
	size_t size; // size of allocation

	Reference<BlockHandle> block_handle;

	inline bool valid() const { return offset != std::numeric_limits<size_t>::max(); }
};


// This is for the Mac, that can't easily do VAO sharing among multiple meshes due to having to use glVertexAttribPointer().
// In this case we allocate, for each mesh, a unique VBO for vertex data, a unique VBO for index data, and a unique VAO.
//
// Also tried having a unique VAO per mesh, using shared VBOs (multiple mesh data in a single VBO), but was hitting severe slowdowns in WebGL on Chrome and Firefox.
#if defined(OSX) || defined(EMSCRIPTEN)
#define DO_INDIVIDUAL_VAO_ALLOC 1
#else
#define DO_INDIVIDUAL_VAO_ALLOC 0
#endif


/*=====================================================================
VertexBufferAllocator
---------------------
In charge of allocating memory for vertex and index data on the GPU.
if DO_INDIVIDUAL_VAO_ALLOC is 1, then just allocates individual VBOs for each mesh's vertex data.

For more modern OpenGL implementations that have glVertexAttribFormat, glVertexAttribBinding and glBindVertexBuffer,
multiple different mesh vertex datas can share the same VAO, with the buffer the VAO is bound to changed for rendering each 
batch with glBindVertexBuffer.  This is much more efficient because changing the current VAO is slow.

Allocates space for vertex data, out of one or more large shared buffers.

Also allocates space for index data, out of one or more large shared buffers.
=====================================================================*/
class VertexBufferAllocator : public ThreadSafeRefCounted
{
public:
	// If use_grouped_vbo_allocator is true, use the best-fit allocator, otherwise just make a new VBO for each allocation.
	VertexBufferAllocator(bool use_grouped_vbo_allocator);
	~VertexBufferAllocator();

	VertBufAllocationHandle allocateVertexDataSpace(size_t vert_stride, const void* vert_data, size_t vert_data_size_B);

	IndexBufAllocationHandle allocateIndexDataSpace(const void* index_data, size_t index_data_size_B);

	// Sets mesh_data_in_out.vao_data_index or mesh_data_in_out.individual_vao
	void getOrCreateAndAssignVAOForMesh(OpenGLMeshRenderData& mesh_data_in_out, const VertexSpec& vertex_spec);

	// Combines allocateVertexDataSpace(), allocateIndexDataSpace() and getOrCreateAndAssignVAOForMesh() calls.
	void allocateBufferSpaceAndVAO(OpenGLMeshRenderData& mesh_data_in_out, const VertexSpec& vertex_spec, const void* vert_data, size_t vert_data_size_B,
		const void* index_data, size_t index_data_size_B);

	void freeBlock(glare::BestFitAllocator::BlockInfo* block);

	std::string getDiagnostics() const;

private:
	GLARE_DISABLE_COPY(VertexBufferAllocator)
public:
	
	// Data per vertex specification
	struct VAOData
	{
		VAORef vao;
	};


	struct VBOAndAllocator
	{
		VBORef vbo;
		Reference<glare::BestFitAllocator> allocator;
	};

	struct VAOKey
	{
		VertexSpec vertex_spec;

		bool operator < (const VAOKey& b) const
		{
			return vertex_spec < b.vertex_spec;
		}
	};

	std::map<VAOKey, int> vao_map; // Map from VAOKey to index into vao_data;

	std::vector<VAOData> vao_data; // Only to be accessed from main thread

private:
	std::vector<VBOAndAllocator> vert_vbos;

	std::vector<VBOAndAllocator> index_vbos;
public:
	size_t use_VBO_size_B;
	bool use_grouped_vbo_allocator;

	Mutex mutex;
};


typedef Reference<VertexBufferAllocator> VertexBufferAllocatorRef;
