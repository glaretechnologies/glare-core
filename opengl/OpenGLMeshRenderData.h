/*=====================================================================
OpenGLMeshRenderData.h
----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "GLMemUsage.h"
#include "VertexBufferAllocator.h"
#include "../physics/jscol_aabbox.h"
#include "../graphics/AnimationData.h"
#include "../utils/Platform.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/MemAlloc.h"
#include "../utils/Vector.h"
#include "../utils/AllocatorVector.h"
#include <stdlib.h>
class BatchedMesh;


// Data for a bunch of triangles from a given mesh, that all share the same material.
class OpenGLBatch
{
public:
	uint32 material_index;
	uint32 prim_start_offset; // Offset in bytes from the start of the index buffer.
	uint32 num_indices; // Number of vertex indices (= num triangles/3).
};


// OpenGL data for a given mesh.
class OpenGLMeshRenderData // : public ThreadSafeRefCounted
{
public:
	OpenGLMeshRenderData() : has_vert_colours(false), has_vert_tangents(false), num_materials_referenced(0), refcount(0), index_type_bits(0) {}

	GLARE_ALIGNED_16_NEW_DELETE

	GLMemUsage getTotalMemUsage() const;
	size_t GPUVertMemUsage() const;
	size_t GPUIndicesMemUsage() const;
	size_t getNumVerts() const; // Just for testing/debugging.
	size_t getVertStrideB() const;
	size_t getNumTris() const; // Just for testing/debugging.

	bool usesSkinning() const { return !animation_data.joint_nodes.empty(); } // TEMP HACK

	void setIndexType(GLenum the_index_type) { index_type = the_index_type; updateIndexTypeBits(); }
	GLenum getIndexType() const { return index_type; }
	uint32 computeIndexTypeBits(GLenum the_index_type) const { return (the_index_type == /*GL_UNSIGNED_BYTE=*/0x1401) ? 0 : ((the_index_type == /*GL_UNSIGNED_SHORT=*/0x1403) ? 1 : 2); }
	void updateIndexTypeBits() { index_type_bits = computeIndexTypeBits(index_type); }

	js::AABBox aabb_os; // Should go first as is aligned.

	glare::AllocatorVector<uint8, 16> vert_data;

	VertBufAllocationHandle vbo_handle;

	js::Vector<uint32, 16> vert_index_buffer;
	js::Vector<uint16, 16> vert_index_buffer_uint16;
	js::Vector<uint8, 16> vert_index_buffer_uint8;

	IndexBufAllocationHandle indices_vbo_handle;

#if DO_INDIVIDUAL_VAO_ALLOC
	VAORef individual_vao; // Used when DO_INDIVIDUAL_VAO_ALLOC is defined, e.g. on Mac and Emscripten/WebGL.
#endif

	std::vector<OpenGLBatch> batches;
	bool has_uvs;
	bool has_shading_normals;
	bool has_vert_colours;
	bool has_vert_tangents;
private:
	GLenum index_type; // One of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT.
public:
	uint32 index_type_bits; // GL_UNSIGNED_BYTE = 0, GL_UNSIGNED_SHORT = 1, GL_UNSIGNED_INT = 2

	VertexSpec vertex_spec;

	// If this is non-null, load vertex and index data directly from batched_mesh instead of from vert_data and vert_index_buffer etc..
	// We take this approach to avoid copying the data from batched_mesh to vert_data etc..
	Reference<BatchedMesh> batched_mesh;

	AnimationData animation_data;

	size_t num_materials_referenced;

	// From ThreadSafeRefCounted.  Manually define this stuff here, so refcount can be defined not at the start of the structure, which wastes space due to alignment issues.
	inline glare::atomic_int decRefCount() const { return refcount.decrement(); }
	inline void incRefCount() const { refcount.increment(); }
	inline glare::atomic_int getRefCount() const { return refcount; }
	mutable glare::AtomicInt refcount;

	bool processed_in_mem_count;
};

typedef Reference<OpenGLMeshRenderData> OpenGLMeshRenderDataRef;
