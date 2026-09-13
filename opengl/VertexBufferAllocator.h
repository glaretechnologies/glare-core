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
#include "../utils/CircularBuffer.h"
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


Deferred (quarantined) freeing of blocks
----------------------------------------
Data is written into these buffers from more than one OpenGL context: the main context writes with glBufferSubData()
(see allocateVertexDataSpace() below, TerrainSystem, MeshPrimitiveBuilding etc.), and OpenGLUploadThread writes with
glCopyBufferSubData() from its own context.  Commands issued in different contexts have no ordering with respect to
each other, and a command that has been issued has not necessarily executed on the GPU yet.

So if a block is returned to the free list as soon as the last BlockHandle referencing it is destroyed, the block can
be handed straight back out and written with new data while a previously-issued write to the same region (or a
previously-issued draw call reading from it) is still pending.  When the stale write finally executes it overwrites
the new mesh's data, which shows up as an intermittently garbled mesh.

To avoid this, freeBlock() doesn't free the block immediately: it records the block along with the current frame
number.  frameEnded() inserts a fence into the main context once per frame, and only returns blocks to the
BestFitAllocator once the fence for the frame in which they were freed has signalled, i.e. once the GPU has finished
executing everything that was issued in the main context up to that point.

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

	// Queues a block to be returned to its BestFitAllocator once the GPU has finished with it.  See comment above.
	// May be called from any thread.
	void freeBlock(glare::BestFitAllocator::BlockInfo* block);

	// Releases any quarantined blocks that the GPU has finished with, and inserts a fence covering all commands issued
	// in this context so far.  Must be called once per frame, on the thread that owns the main OpenGL context.
	void frameEnded();

	// Blocks until the GPU has finished with every currently-quarantined block, then returns them all to their
	// allocators, so that the buffers are as empty as the live allocations allow.
	// This is the one place we're willing to sync with the GPU, so only use it where a stall is acceptable and starting
	// from a clean slate is worth it - e.g. a force-refresh that tears down and reloads the world.
	// Must be called on the thread that owns the main OpenGL context.
	void waitForGPUAndReleaseAllPendingFrees();

	std::string getDiagnostics() const;

private:
	GLARE_DISABLE_COPY(VertexBufferAllocator)

	// All of these require mutex to be held:
	void releaseRetiredBlocks();  // Free blocks that were freed in a frame the GPU has finished.
	void pollFrameFences();       // Non-blocking: see which fences have signalled, and advance last_retired_frame_num.
	void waitForAndRemoveOldestFrameFence(); // Blocking.  Only used to bound the number of in-flight frames.
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

	// A block whose last BlockHandle has been destroyed, but which the GPU may not have finished with yet.
	struct PendingFree
	{
		glare::BestFitAllocator::BlockInfo* block;
		size_t block_size; // Copy of block->size, since block may be destroyed by BestFitAllocator::free() when we release it.
		uint64 freed_in_frame; // Value of current_frame_num when the block was freed by client code.
	};

	struct FrameFence
	{
		uint64 frame_num; // All commands issued in the main context during this frame, and earlier frames, are covered by sync_ob.
		void* sync_ob; // GLsync.  Stored as void* to avoid pulling the OpenGL headers into this header.
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

	// Both of these are FIFO queues: items are appended at the back and consumed from the front, in increasing
	// frame_num order.  CircularBuffer gives us O(1) pop_front() without shuffling the remaining items down.
	CircularBuffer<PendingFree> pending_free_blocks; // Blocks waiting for the GPU to finish with them.
	CircularBuffer<FrameFence> frame_fences; // Fences signal in the order they were created.
	size_t quarantined_size_B; // Sum of block_size over pending_free_blocks.  Maintained incrementally, for getDiagnostics().
	uint64 current_frame_num;
	uint64 last_retired_frame_num; // All commands issued in the main context during this frame, and earlier, have completed on the GPU.
public:
	size_t use_VBO_size_B;
	bool use_grouped_vbo_allocator;

	mutable Mutex mutex;
};


typedef Reference<VertexBufferAllocator> VertexBufferAllocatorRef;
